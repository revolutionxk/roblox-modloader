#include "RobloxModLoader/platform/graphics/gpu_timeline.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <objc/runtime.h>
#include <pthread.h>
#include <time.h>

#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rml::platform
{
	static constexpr NSUInteger sample_capacity = 2048;

	struct CommandBufferState
	{
		id<MTLCounterSampleBuffer> samples;
		NSUInteger used{};
		std::vector<std::pair<std::uint64_t, std::uint64_t>> encoders;
	};

	struct Delivery
	{
		GpuTimelineSink sink;
		void* user;
	};

	static auto& g_mutex = *new std::mutex();
	static auto& g_delivery = *new std::mutex();
	static auto& g_states = *new std::unordered_map<const void*, std::unique_ptr<CommandBufferState>>();
	static auto& g_pool = *new std::vector<id<MTLCounterSampleBuffer>>();
	static Delivery g_target{};
	static id<MTLDevice> g_device;
	static id<MTLCounterSet> g_counter_set;
	static std::atomic<bool> g_active{false};
	static std::atomic<std::uint64_t> g_serial{0};
	static std::atomic<IMP> g_create_buffer{nullptr};
	static std::atomic<IMP> g_render_encoder{nullptr};
	static std::atomic<IMP> g_blit_encoder{nullptr};
	static std::atomic<IMP> g_compute_encoder{nullptr};
	static std::once_flag g_install;
	static bool g_installed{};
	static double g_gpu_scale{1.0};
	static std::int64_t g_gpu_offset{};

	static std::uint64_t current_thread()
	{
		std::uint64_t thread = 0;
		pthread_threadid_np(nullptr, &thread);
		return thread;
	}

	static std::uint64_t host_ticks(const CFTimeInterval seconds)
	{
		return static_cast<std::uint64_t>(std::llround(seconds * 1e9));
	}

	static std::uint64_t gpu_to_host(const std::uint64_t gpu)
	{
		return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(static_cast<double>(gpu) * g_gpu_scale)) + g_gpu_offset);
	}

	static void calibrate()
	{
		MTLTimestamp cpu0 = 0;
		MTLTimestamp gpu0 = 0;
		[g_device sampleTimestamps:&cpu0 gpuTimestamp:&gpu0];
		[NSThread sleepForTimeInterval:0.02];
		MTLTimestamp cpu1 = 0;
		MTLTimestamp gpu1 = 0;
		[g_device sampleTimestamps:&cpu1 gpuTimestamp:&gpu1];
		g_gpu_scale = gpu1 > gpu0 ? static_cast<double>(cpu1 - cpu0) / static_cast<double>(gpu1 - gpu0) : 1.0;
		g_gpu_offset = static_cast<std::int64_t>(cpu1) - static_cast<std::int64_t>(std::llround(static_cast<double>(gpu1) * g_gpu_scale));
	}

	static id<MTLCounterSampleBuffer> acquire_samples()
	{
		{
			std::scoped_lock lock(g_mutex);
			if (!g_pool.empty())
			{
				id<MTLCounterSampleBuffer> buffer = g_pool.back();
				g_pool.pop_back();
				return buffer;
			}
		}
		MTLCounterSampleBufferDescriptor* descriptor = [MTLCounterSampleBufferDescriptor new];
		descriptor.counterSet = g_counter_set;
		descriptor.storageMode = MTLStorageModeShared;
		descriptor.sampleCount = sample_capacity;
		return [g_device newCounterSampleBufferWithDescriptor:descriptor error:nil];
	}

	static CommandBufferState* state_of(id buffer)
	{
		std::scoped_lock lock(g_mutex);
		const auto it = g_states.find((__bridge const void*)buffer);
		return it == g_states.end() ? nullptr : it->second.get();
	}

	static NSUInteger reserve(CommandBufferState& state)
	{
		if (!state.samples || state.used + 2 > sample_capacity)
			return NSNotFound;
		const auto index = state.used;
		state.encoders.emplace_back(g_serial.fetch_add(1) + 1, current_thread());
		state.used += 2;
		return index;
	}

	static void unreserve(CommandBufferState& state)
	{
		state.encoders.pop_back();
		state.used -= 2;
	}

	static void complete(id<MTLCommandBuffer> buffer)
	{
		std::unique_ptr<CommandBufferState> state;
		{
			std::scoped_lock lock(g_mutex);
			const auto it = g_states.find((__bridge const void*)buffer);
			if (it == g_states.end())
				return;
			state = std::move(it->second);
			g_states.erase(it);
		}

		if (g_active.load() && state->used > 0 && buffer.status == MTLCommandBufferStatusCompleted)
		{
			@autoreleasepool
			{
				NSData* data = [state->samples resolveCounterRange:NSMakeRange(0, state->used)];
				const auto* values = data ? static_cast<const MTLCounterResultTimestamp*>(data.bytes) : nullptr;
				const auto count = data ? data.length / sizeof(MTLCounterResultTimestamp) : 0;
				std::vector<GpuEncoderTiming> encoders;
				encoders.reserve(state->encoders.size());
				for (std::size_t i = 0; i < state->encoders.size() && i * 2 + 1 < count; ++i)
				{
					const auto begin = values[i * 2].timestamp;
					const auto end = values[i * 2 + 1].timestamp;
					if (begin == MTLCounterErrorValue || end == MTLCounterErrorValue || end < begin)
						continue;
					encoders.push_back({state->encoders[i].first, state->encoders[i].second, gpu_to_host(begin), gpu_to_host(end)});
				}
				const GpuCommandBufferTiming timing{host_ticks(buffer.GPUStartTime), host_ticks(buffer.GPUEndTime), encoders};
				std::scoped_lock lock(g_delivery);
				if (g_target.sink)
				{
					try
					{
						g_target.sink(timing, g_target.user);
					}
					catch (...)
					{
					}
				}
			}
		}

		if (state->samples)
		{
			std::scoped_lock lock(g_mutex);
			g_pool.push_back(state->samples);
		}
	}

	static id swizzled_create_buffer(id self, SEL command)
	{
		const IMP original = g_create_buffer.load();
		id buffer = reinterpret_cast<id (*)(id, SEL)>(original)(self, command);
		if (!buffer || !g_active.load())
			return buffer;

		try
		{
			if ([(id<MTLCommandBuffer>)buffer device] != g_device)
				return buffer;
			auto state = std::make_unique<CommandBufferState>();
			state->samples = acquire_samples();
			if (!state->samples)
				return buffer;
			{
				std::scoped_lock lock(g_mutex);
				g_states.insert_or_assign((__bridge const void*)buffer, std::move(state));
			}
			[(id<MTLCommandBuffer>)buffer addCompletedHandler:^(id<MTLCommandBuffer> done) {
				try
				{
					complete(done);
				}
				catch (...)
				{
				}
			}];
		}
		catch (...)
		{
		}
		return buffer;
	}

	static void clear_sample_buffer(MTLRenderPassSampleBufferAttachmentDescriptor* attachment)
	{
		try
		{
			attachment.sampleBuffer = nil;
		}
		catch (...)
		{
		}
	}

	static id swizzled_render_encoder(id self, SEL command, MTLRenderPassDescriptor* descriptor)
	{
		const IMP original = g_render_encoder.load();
		CommandBufferState* reserved = nullptr;
		MTLRenderPassSampleBufferAttachmentDescriptor* attachment = nil;
		try
		{
			auto* state = g_active.load() && descriptor ? state_of(self) : nullptr;
			const auto index = state ? reserve(*state) : NSNotFound;
			if (index != NSNotFound)
			{
				reserved = state;
				attachment = descriptor.sampleBufferAttachments[0];
				attachment.sampleBuffer = state->samples;
				attachment.startOfVertexSampleIndex = index;
				attachment.endOfVertexSampleIndex = MTLCounterDontSample;
				attachment.startOfFragmentSampleIndex = MTLCounterDontSample;
				attachment.endOfFragmentSampleIndex = index + 1;
			}
		}
		catch (...)
		{
			if (reserved)
				unreserve(*reserved);
			if (attachment)
				clear_sample_buffer(attachment);
			attachment = nil;
		}

		id encoder = reinterpret_cast<id (*)(id, SEL, MTLRenderPassDescriptor*)>(original)(self, command, descriptor);
		if (attachment)
			clear_sample_buffer(attachment);
		return encoder;
	}

	static id swizzled_blit_encoder(id self, SEL command)
	{
		const IMP original = g_blit_encoder.load();
		CommandBufferState* reserved = nullptr;
		try
		{
			auto* state = g_active.load() ? state_of(self) : nullptr;
			const auto index = state ? reserve(*state) : NSNotFound;
			if (index != NSNotFound)
			{
				reserved = state;
				MTLBlitPassDescriptor* descriptor = [MTLBlitPassDescriptor blitPassDescriptor];
				descriptor.sampleBufferAttachments[0].sampleBuffer = state->samples;
				descriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = index;
				descriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = index + 1;
				return [(id<MTLCommandBuffer>)self blitCommandEncoderWithDescriptor:descriptor];
			}
		}
		catch (...)
		{
			if (reserved)
				unreserve(*reserved);
		}
		return reinterpret_cast<id (*)(id, SEL)>(original)(self, command);
	}

	static id swizzled_compute_encoder(id self, SEL command)
	{
		const IMP original = g_compute_encoder.load();
		CommandBufferState* reserved = nullptr;
		try
		{
			auto* state = g_active.load() ? state_of(self) : nullptr;
			const auto index = state ? reserve(*state) : NSNotFound;
			if (index != NSNotFound)
			{
				reserved = state;
				MTLComputePassDescriptor* descriptor = [MTLComputePassDescriptor computePassDescriptor];
				descriptor.dispatchType = MTLDispatchTypeSerial;
				descriptor.sampleBufferAttachments[0].sampleBuffer = state->samples;
				descriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = index;
				descriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = index + 1;
				return [(id<MTLCommandBuffer>)self computeCommandEncoderWithDescriptor:descriptor];
			}
		}
		catch (...)
		{
			if (reserved)
				unreserve(*reserved);
		}
		return reinterpret_cast<id (*)(id, SEL)>(original)(self, command);
	}

	static void swizzle(Method method, IMP replacement, std::atomic<IMP>& original)
	{
		original.store(method_getImplementation(method));
		method_setImplementation(method, replacement);
	}

	static bool install()
	{
		g_device = MTLCreateSystemDefaultDevice();
		if (!g_device || ![g_device supportsCounterSampling:MTLCounterSamplingPointAtStageBoundary])
			return false;

		for (id<MTLCounterSet> set in g_device.counterSets)
		{
			if ([set.name isEqualToString:MTLCommonCounterSetTimestamp])
				g_counter_set = set;
		}
		if (!g_counter_set)
			return false;

		calibrate();

		id<MTLCommandQueue> queue = [g_device newCommandQueue];
		id<MTLCommandBuffer> probe = [queue commandBufferWithUnretainedReferences];
		if (!queue || !probe)
			return false;
		const Class queue_class = object_getClass(queue);
		const Class buffer_class = object_getClass(probe);
		Method create_buffer = class_getInstanceMethod(queue_class, @selector(commandBufferWithUnretainedReferences));
		Method render_encoder = class_getInstanceMethod(buffer_class, @selector(renderCommandEncoderWithDescriptor:));
		Method blit_encoder = class_getInstanceMethod(buffer_class, @selector(blitCommandEncoder));
		Method compute_encoder = class_getInstanceMethod(buffer_class, @selector(computeCommandEncoder));
		if (!create_buffer || !render_encoder || !blit_encoder || !compute_encoder)
			return false;

		swizzle(create_buffer, reinterpret_cast<IMP>(&swizzled_create_buffer), g_create_buffer);
		swizzle(render_encoder, reinterpret_cast<IMP>(&swizzled_render_encoder), g_render_encoder);
		swizzle(blit_encoder, reinterpret_cast<IMP>(&swizzled_blit_encoder), g_blit_encoder);
		swizzle(compute_encoder, reinterpret_cast<IMP>(&swizzled_compute_encoder), g_compute_encoder);
		return true;
	}

	bool start_gpu_timeline(const GpuTimelineSink sink, void* user)
	{
		@autoreleasepool
		{
			std::call_once(g_install, [] { g_installed = install(); });
		}
		if (!g_installed)
			return false;

		{
			std::scoped_lock lock(g_delivery);
			g_target = {sink, user};
		}
		g_active.store(true);
		return true;
	}

	void stop_gpu_timeline()
	{
		g_active.store(false);
		std::scoped_lock lock(g_delivery);
		g_target = {};
	}

	std::uint64_t gpu_timeline_serial() noexcept
	{
		return g_serial.load();
	}

	std::uint64_t gpu_timeline_now() noexcept
	{
		return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
	}

	double gpu_timeline_period() noexcept
	{
		return 1.0;
	}

	std::string_view gpu_timeline_api() noexcept
	{
		return "Metal";
	}
}
