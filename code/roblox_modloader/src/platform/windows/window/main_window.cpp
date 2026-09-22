#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/logger/logger.hpp"

RML_LOG_SCOPE("MainWindow");

namespace rml::platform
{
	namespace
	{
		// The window is resolved lazily, on first use, never during loader init: while the loader
		// initialises, Studio's main thread is parked inside the init gate (src/app/init_gate.cpp:105),
		// so no top-level window can be created or pumped and any poll there is guaranteed to fail.
		// The retry budget below therefore only covers the first lazy lookup; later lookups take a
		// single enumeration pass and never sleep.
		constexpr int main_window_search_attempts = 20;
		constexpr std::chrono::milliseconds main_window_search_interval{100};

		// Qt names its top-level window classes "Qt<version>QWindow<suffix>"; a regular decorated
		// main window gets the "Icon" suffix, while tool windows, popups and the splash screen get
		// the other suffixes. This is the anchor that identifies Studio's main window by identity
		// rather than by Z-order.
		constexpr std::wstring_view qt_window_class_prefix = L"Qt";
		constexpr std::wstring_view qt_main_window_class_suffix = L"QWindowIcon";

		struct WindowCandidate
		{
			HWND window;
			std::wstring class_name;
		};

		struct WindowSearch
		{
			DWORD process_id;
			std::vector<WindowCandidate> anchored;
			std::vector<WindowCandidate> shaped;
		};

		std::wstring window_class_name(HWND window)
		{
			wchar_t buffer[256]{};

			const int length = GetClassNameW(window, buffer, static_cast<int>(std::size(buffer)));
			if (length <= 0)
				return {};

			return std::wstring{buffer, static_cast<std::size_t>(length)};
		}

		std::string narrow(const std::wstring& text)
		{
			if (text.empty())
				return {};

			const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0,
			        nullptr, nullptr);
			if (size <= 0)
				return {};

			std::string narrowed(static_cast<std::size_t>(size), '\0');
			WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrowed.data(), size, nullptr,
			        nullptr);

			return narrowed;
		}

		bool has_client_area(HWND window)
		{
			RECT client{};

			if (!GetClientRect(window, &client))
				return false;

			return client.right > client.left && client.bottom > client.top;
		}

		// Everything a Studio main window must be, checked on the window itself. Used both to filter
		// the enumeration and to validate the winner independently of the rung that produced it.
		bool has_main_window_shape(HWND window, const DWORD process_id)
		{
			DWORD window_process_id = 0;

			if (GetWindowThreadProcessId(window, &window_process_id) == 0 || window_process_id != process_id)
				return false;

			if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER) != nullptr)
				return false;

			if ((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) != 0)
				return false;

			return has_client_area(window);
		}

		bool is_qt_main_window_class(const std::wstring_view class_name)
		{
			return class_name.starts_with(qt_window_class_prefix) && class_name.ends_with(qt_main_window_class_suffix);
		}

		bool is_valid_main_window(HWND window, const DWORD process_id)
		{
			return window != nullptr && IsWindow(window) && has_main_window_shape(window, process_id)
			    && !window_class_name(window).empty();
		}

		BOOL CALLBACK collect_process_window(HWND window, const LPARAM parameter)
		{
			auto* search = reinterpret_cast<WindowSearch*>(parameter);

			if (!has_main_window_shape(window, search->process_id))
				return TRUE;

			auto class_name = window_class_name(window);
			if (class_name.empty())
				return TRUE;

			if (is_qt_main_window_class(class_name))
				search->anchored.push_back({window, std::move(class_name)});
			else
				search->shaped.push_back({window, std::move(class_name)});

			return TRUE;
		}

		void log_candidates(const std::string_view rung, const std::vector<WindowCandidate>& candidates)
		{
			for (const auto& candidate : candidates)
				RML_ERROR("  {} candidate: hwnd={} class='{}'", rung, static_cast<const void*>(candidate.window),
				        narrow(candidate.class_name));
		}

		HWND resolve_main_window(const int attempts)
		{
			const DWORD process_id = GetCurrentProcessId();

			for (int attempt = 0; attempt < attempts; ++attempt)
			{
				WindowSearch search{process_id, {}, {}};

				SetLastError(ERROR_SUCCESS);
				if (!EnumWindows(&collect_process_window, reinterpret_cast<LPARAM>(&search)))
				{
					RML_ERROR("EnumWindows failed while looking for Studio's main window (GetLastError={})",
					        GetLastError());
					return nullptr;
				}

				const bool anchored = !search.anchored.empty();
				const std::vector<WindowCandidate>& candidates = anchored ? search.anchored : search.shaped;
				const std::string_view rung = anchored ? "class-name" : "window-shape";

				if (candidates.size() > 1)
				{
					RML_ERROR("Refusing to guess Studio's main window: {} windows matched the {} anchor",
					        candidates.size(), rung);
					log_candidates(rung, candidates);
					return nullptr;
				}

				if (candidates.size() == 1)
				{
					HWND window = candidates.front().window;

					if (is_valid_main_window(window, process_id))
					{
						if (!anchored)
							RML_WARN("No window matched the Qt main window class anchor; falling back to the "
							         "single window-shape match. Verify this is Studio's main window.");

						RML_INFO("Resolved Studio's main window by {} anchor: hwnd={} class='{}'", rung,
						        static_cast<const void*>(window), narrow(candidates.front().class_name));
						return window;
					}

					RML_WARN("Rejected Studio main window candidate hwnd={} class='{}': failed validation",
					        static_cast<const void*>(window), narrow(candidates.front().class_name));
				}
				else
				{
					RML_WARN("No Studio main window found yet (attempt {}/{}): no visible unowned top-level window "
					         "with a client area belongs to this process",
					        attempt + 1, attempts);
				}

				if (attempt + 1 < attempts)
					std::this_thread::sleep_for(main_window_search_interval);
			}

			RML_ERROR("Failed to find Studio's main window after {} attempt(s) over {}ms (GetLastError={})", attempts,
			        main_window_search_interval.count() * (attempts - 1), GetLastError());

			return nullptr;
		}
	}

	void* acquire_main_window()
	{
		static std::mutex cache_mutex;
		static HWND cached_window = nullptr;
		static bool searched_once = false;

		std::lock_guard lock(cache_mutex);

		const DWORD process_id = GetCurrentProcessId();

		if (is_valid_main_window(cached_window, process_id))
			return cached_window;

		if (cached_window != nullptr)
		{
			RML_WARN("Cached Studio main window hwnd={} is no longer valid; re-resolving",
			        static_cast<const void*>(cached_window));
			cached_window = nullptr;
		}

		// Only the first lookup is allowed to wait: after that the window either exists or the caller
		// gets an immediate answer instead of a stall.
		cached_window = resolve_main_window(searched_once ? 1 : main_window_search_attempts);
		searched_once = true;

		return cached_window;
	}
}
