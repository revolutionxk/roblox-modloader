#pragma once

#include "RobloxModLoader/render/render_pass.hpp"
#include "render/commands_impl.hpp"
#include "render/engine_stages.hpp"
#include "render/frame_resources_impl.hpp"
#include "render/pass_suspension.hpp"
#include "render/render_graph_impl.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RBX::Graphics
{
	class Device;
	class DeviceContext;
	class Framebuffer;
	class RenderCamera;
	class SceneManager;
}

namespace rml::render::detail
{
	class InjectionDispatch
	{
	public:
		static InjectionDispatch& instance();

		[[nodiscard]] RenderGraphImpl& graph();
		[[nodiscard]] PassSuspension& suspension();
		[[nodiscard]] std::uint64_t frame_index() const;

		bool begin_view(RBX::Graphics::SceneManager& scene, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* output, const RBX::Graphics::RenderCamera* camera, std::uint32_t capture_mode);
		void end_view();
		void reach(InjectionPoint point);
		[[nodiscard]] bool on_scene_target() const;
		[[nodiscard]] bool on_output_target() const;
		[[nodiscard]] bool plans_clouds_path() const;
		void note_main_view();
		void device_lost(RBX::Graphics::Device& device);
		bool enable_output_readback(bool enabled);

	private:
		InjectionDispatch() = default;

		[[nodiscard]] bool owns_frame() const;
		void ensure_device(RBX::Graphics::Device& device);
		void publish_builtins(InjectionPoint point, bool offscreen);
		RBX::Graphics::Texture* copy_output();
		void run(const std::vector<std::size_t>& passes, InjectionPoint point, std::uint32_t occurrence, CommandsImpl::Mode mode);

		RenderGraphImpl m_graph;
		PassSuspension m_suspension;
		RBX::Graphics::Device* m_device{};
		std::unique_ptr<FrameResourcesImpl> m_resources;
		FullscreenGeometry m_fullscreen;
		std::optional<FrameContext> m_frame;
		std::unique_lock<std::mutex> m_frame_lock;
		RBX::Graphics::SceneManager* m_scene{};
		RBX::Graphics::DeviceContext* m_context{};
		RBX::Graphics::Framebuffer* m_output{};
		std::shared_ptr<RBX::Graphics::Texture> m_output_copy;
		std::unordered_map<std::uint32_t, std::uint32_t> m_occurrences;
		std::chrono::steady_clock::time_point m_last_time{};
		std::uint64_t m_frame_index{};
		std::atomic<std::thread::id> m_owner{};
		std::atomic<bool> m_readback{false};
		bool m_main_view_seen{};
		bool m_main_view_now{};
		bool m_main_done{};
		bool m_announced{};
	};

	template<typename Original>
	void run_stage(const EngineStage stage, const bool inject, Original&& original)
	{
		auto& dispatch = InjectionDispatch::instance();
		if (inject)
			dispatch.reach(InjectionPoint::stage(stage, Side::Before));
		if (EngineStages::instance().skips(stage))
		{
			if (inject)
				dispatch.reach(InjectionPoint::stage(stage, Side::Instead));
		}
		else
		{
			std::forward<Original>(original)();
		}
		if (inject)
			dispatch.reach(InjectionPoint::stage(stage, Side::After));
	}

	template<typename Original>
	void run_queue(const QueueGroup group, const bool inject, Original&& original)
	{
		auto& dispatch = InjectionDispatch::instance();
		if (inject)
			dispatch.reach(InjectionPoint::queue(group, Side::Before));
		if (EngineStages::instance().skips(group))
		{
			if (inject)
				dispatch.reach(InjectionPoint::queue(group, Side::Instead));
		}
		else
		{
			std::forward<Original>(original)();
		}
		if (inject)
			dispatch.reach(InjectionPoint::queue(group, Side::After));
	}
}
