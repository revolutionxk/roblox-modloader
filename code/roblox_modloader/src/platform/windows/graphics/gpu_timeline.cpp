#include "RobloxModLoader/platform/graphics/gpu_timeline.hpp"

namespace rml::platform
{
	bool start_gpu_timeline(GpuTimelineSink, void*)
	{
		return false;
	}

	void stop_gpu_timeline()
	{
	}

	std::uint64_t gpu_timeline_serial() noexcept
	{
		return 0;
	}

	std::uint64_t gpu_timeline_now() noexcept
	{
		return 0;
	}

	double gpu_timeline_period() noexcept
	{
		return 1.0;
	}

	std::string_view gpu_timeline_api() noexcept
	{
		return "unsupported";
	}
}
