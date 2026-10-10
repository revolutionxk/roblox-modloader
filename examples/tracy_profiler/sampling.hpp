#pragma once

#include <cstdint>

namespace tracy_profiler
{
	int32_t sample_hook(uint32_t thread, uint64_t* frames, int32_t depth, int32_t capacity, uint64_t link, void* user);
}
