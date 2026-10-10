#include "RobloxModLoader/platform/graphics/gpu_timeline.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <mach/mach_time.h>
#include <objc/runtime.h>
#include <pthread.h>

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

	static std::mutex g_mutex;
	static std::unordered_map<const void*, std::unique_ptr<CommandBufferState>> g_states;
	static std::vector<id<MTLCounterSampleBuffer>> g_pool;
	static id<MTLDevice> g_device;
	static id<MTLCounterSet> g_counter_set;
	static std::atomic<bool> g_active{false};
	static std::atomic<GpuTimelineSink> g_sink{nullptr};
	static std::atomic<void*> g_user{nullptr};
	static std::atomic<std::uint64_t> g_serial{0};
	static IMP g_create_buffer;
	static IMP g_render_encoder;
	static IMP g_blit_encoder;
	static IMP g_compute_encoder;
	static bool g_swizzled{};
	static double g_period{1.0};
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
		return static_cast<std::uint64_t>(std::llround(seconds * 1e9 / g_period));
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
		state.used += 2;
		state.encoders.emplace_back(g_serial.fetch_add(1) + 1, current_thread());
		return index;
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

		const auto sink = g_sink.load();
		if (sink && state->used > 0 && buffer.status == MTLCommandBufferStatusCompleted)
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
				try
				{
					sink(timing, g_user.load());
				}
				catch (...)
				{
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
		id buffer = reinterpret_cast<id (*)(id, SEL)>(g_create_buffer)(self, command);
		if (!buffer || !g_active.load() || [(id<MTLCommandBuffer>)buffer device] != g_device)
			return buffer;

		try
		{
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

	static id swizzled_render_encoder(id self, SEL command, MTLRenderPassDescriptor* descriptor)
	{
		const auto original = reinterpret_cast<id (*)(id, SEL, MTLRenderPassDescriptor*)>(g_render_encoder);
		auto* state = g_active.load() && descriptor ? state_of(self) : nullptr;
		const auto index = state ? reserve(*state) : NSNotFound;
		if (index == NSNotFound)
			return original(self, command, descriptor);

		MTLRenderPassSampleBufferAttachmentDescriptor* attachment = descriptor.sampleBufferAttachments[0];
		attachment.sampleBuffer = state->samples;
		attachment.startOfVertexSampleIndex = index;
		attachment.endOfVertexSampleIndex = MTLCounterDontSample;
		attachment.startOfFragmentSampleIndex = MTLCounterDontSample;
		attachment.endOfFragmentSampleIndex = index + 1;
		id encoder = original(self, command, descriptor);
		attachment.sampleBuffer = nil;
		return encoder;
	}

	static id swizzled_blit_encoder(id self, SEL command)
	{
		auto* state = g_active.load() ? state_of(self) : nullptr;
		const auto index = state ? reserve(*state) : NSNotFound;
		if (index == NSNotFound)
			return reinterpret_cast<id (*)(id, SEL)>(g_blit_encoder)(self, command);

		MTLBlitPassDescriptor* descriptor = [MTLBlitPassDescriptor blitPassDescriptor];
		descriptor.sampleBufferAttachments[0].sampleBuffer = state->samples;
		descriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = index;
		descriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = index + 1;
		return [(id<MTLCommandBuffer>)self blitCommandEncoderWithDescriptor:descriptor];
	}

	static id swizzled_compute_encoder(id self, SEL command)
	{
		auto* state = g_active.load() ? state_of(self) : nullptr;
		const auto index = state ? reserve(*state) : NSNotFound;
		if (index == NSNotFound)
			return reinterpret_cast<id (*)(id, SEL)>(g_compute_encoder)(self, command);

		MTLComputePassDescriptor* descriptor = [MTLComputePassDescriptor computePassDescriptor];
		descriptor.dispatchType = MTLDispatchTypeSerial;
		descriptor.sampleBufferAttachments[0].sampleBuffer = state->samples;
		descriptor.sampleBufferAttachments[0].startOfEncoderSampleIndex = index;
		descriptor.sampleBufferAttachments[0].endOfEncoderSampleIndex = index + 1;
		return [(id<MTLCommandBuffer>)self computeCommandEncoderWithDescriptor:descriptor];
	}

	static bool swizzle(Class owner, SEL selector, IMP replacement, IMP& original)
	{
		Method method = class_getInstanceMethod(owner, selector);
		if (!method)
			return false;
		original = method_setImplementation(method, replacement);
		return original != nullptr;
	}

	bool start_gpu_timeline(const GpuTimelineSink sink, void* user)
	{
		if (g_active.load())
			return true;

		@autoreleasepool
		{
			if (!g_device)
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

			mach_timebase_info_data_t timebase{};
			mach_timebase_info(&timebase);
			g_period = static_cast<double>(timebase.numer) / static_cast<double>(timebase.denom);
			calibrate();

			if (!g_swizzled)
			{
				id<MTLCommandQueue> queue = [g_device newCommandQueue];
				id<MTLCommandBuffer> probe = [queue commandBufferWithUnretainedReferences];
				if (!queue || !probe)
					return false;
				const Class queue_class = object_getClass(queue);
				const Class buffer_class = object_getClass(probe);
				if (!swizzle(queue_class, @selector(commandBufferWithUnretainedReferences), reinterpret_cast<IMP>(&swizzled_create_buffer), g_create_buffer) ||
				    !swizzle(buffer_class, @selector(renderCommandEncoderWithDescriptor:), reinterpret_cast<IMP>(&swizzled_render_encoder), g_render_encoder) ||
				    !swizzle(buffer_class, @selector(blitCommandEncoder), reinterpret_cast<IMP>(&swizzled_blit_encoder), g_blit_encoder) ||
				    !swizzle(buffer_class, @selector(computeCommandEncoder), reinterpret_cast<IMP>(&swizzled_compute_encoder), g_compute_encoder))
					return false;
				g_swizzled = true;
			}
		}

		g_user.store(user);
		g_sink.store(sink);
		g_active.store(true);
		return true;
	}

	void stop_gpu_timeline()
	{
		g_active.store(false);
		g_sink.store(nullptr);
		g_user.store(nullptr);
	}

	std::uint64_t gpu_timeline_serial() noexcept
	{
		return g_serial.load();
	}

	std::uint64_t gpu_timeline_now() noexcept
	{
		return mach_absolute_time();
	}

	double gpu_timeline_period() noexcept
	{
		return g_period;
	}

	std::string_view gpu_timeline_api() noexcept
	{
		return "Metal";
	}
}
