#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tracy_profiler
{
	class Callstacks
	{
	public:
		static constexpr int capacity = 64;

		explicit Callstacks(const void* enter);

		[[nodiscard]] int capture(std::uint64_t* frames, int depth) const;
		[[nodiscard]] int capture_from_studio(std::uint64_t* frames, int depth, int skip) const;
		[[nodiscard]] std::size_t shim_count() const;
		[[nodiscard]] std::uintptr_t studio_caller() const;

	private:
		[[nodiscard]] bool is_engine_caller(std::uintptr_t return_address) const;

		std::vector<std::uintptr_t> m_shims;
	};
}
