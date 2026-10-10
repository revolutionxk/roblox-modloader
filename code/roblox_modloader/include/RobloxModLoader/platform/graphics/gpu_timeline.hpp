#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace rml::platform
{
	struct GpuEncoderTiming
	{
		std::uint64_t serial;
		std::uint64_t thread;
		std::uint64_t begin;
		std::uint64_t end;
	};

	struct GpuCommandBufferTiming
	{
		std::uint64_t begin;
		std::uint64_t end;
		std::span<const GpuEncoderTiming> encoders;
	};

	using GpuTimelineSink = void (*)(const GpuCommandBufferTiming& timing, void* user);

	[[nodiscard]] RML_EXPORT bool start_gpu_timeline(GpuTimelineSink sink, void* user);
	RML_EXPORT void stop_gpu_timeline();
	[[nodiscard]] RML_EXPORT std::uint64_t gpu_timeline_serial() noexcept;
	[[nodiscard]] RML_EXPORT std::uint64_t gpu_timeline_now() noexcept;
	[[nodiscard]] RML_EXPORT double gpu_timeline_period() noexcept;
	[[nodiscard]] RML_EXPORT std::string_view gpu_timeline_api() noexcept;
}
