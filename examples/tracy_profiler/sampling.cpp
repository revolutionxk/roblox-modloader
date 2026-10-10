#include "sampling.hpp"

#include <RobloxModLoader/platform/debug/call_stack.hpp>
#include <cstddef>
#include <cstring>

namespace tracy_profiler
{
	int32_t sample_hook(uint32_t, uint64_t* frames, const int32_t depth, const int32_t capacity, const uint64_t link, void*)
	{
		if (link == 0 || depth < 1 || depth >= capacity)
			return depth;

		if (rml::platform::frame_established(reinterpret_cast<const void*>(frames[0])))
			return depth;

		if (depth > 1 && frames[1] == link)
			return depth;

		std::memmove(frames + 2, frames + 1, static_cast<std::size_t>(depth - 1) * sizeof(std::uint64_t));
		frames[1] = link;
		return depth + 1;
	}
}
