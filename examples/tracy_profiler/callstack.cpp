#include "callstack.hpp"

#include <RobloxModLoader/memory/string_anchor.hpp>
#include <RobloxModLoader/platform/debug/call_stack.hpp>
#include <algorithm>
#include <array>

namespace tracy_profiler
{
	static constexpr std::size_t leading_frame_budget = 16;

	Callstacks::Callstacks(const void* enter)
	{
		for (const auto& function : rml::memory::functions_calling(enter))
			m_shims.push_back(reinterpret_cast<std::uintptr_t>(function.start));
		std::ranges::sort(m_shims);
	}

	std::size_t Callstacks::shim_count() const
	{
		return m_shims.size();
	}

	std::uintptr_t Callstacks::studio_caller() const
	{
		std::array<std::uintptr_t, leading_frame_budget> raw{};
		const auto captured = rml::platform::capture_return_addresses(raw);
		for (std::size_t index = 0; index < captured; ++index)
		{
			if (rml::memory::function_containing(reinterpret_cast<const void*>(raw[index] - 1)))
				return raw[index];
		}
		return 0;
	}

	bool Callstacks::is_engine_caller(const std::uintptr_t return_address) const
	{
		const auto function = rml::memory::function_containing(reinterpret_cast<const void*>(return_address - 1));
		return function && !std::ranges::binary_search(m_shims, reinterpret_cast<std::uintptr_t>(function->start));
	}

	int Callstacks::capture(std::uint64_t* frames, const int depth) const
	{
		std::array<std::uintptr_t, capacity + leading_frame_budget> raw{};
		const auto captured = rml::platform::capture_return_addresses(raw);

		std::size_t first = 0;
		while (first < captured && !is_engine_caller(raw[first]))
			++first;

		const auto count = std::min<std::size_t>(captured - first, static_cast<std::size_t>(depth));
		std::copy_n(raw.begin() + static_cast<std::ptrdiff_t>(first), count, frames);
		return static_cast<int>(count);
	}
}
