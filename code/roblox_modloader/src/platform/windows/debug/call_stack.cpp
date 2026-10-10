#include "RobloxModLoader/platform/debug/call_stack.hpp"

#include "RobloxModLoader/internal/common.hpp"

namespace rml::platform
{
	static constexpr std::size_t max_captured_frames = 62;

	std::size_t capture_return_addresses(const std::span<std::uintptr_t> frames) noexcept
	{
		const auto count = static_cast<DWORD>(std::min(frames.size(), max_captured_frames));
		return RtlCaptureStackBackTrace(1, count, reinterpret_cast<void**>(frames.data()), nullptr);
	}

	bool frame_established(const void*) noexcept
	{
		return true;
	}

	std::uint64_t current_thread_id() noexcept
	{
		return GetCurrentThreadId();
	}
}
