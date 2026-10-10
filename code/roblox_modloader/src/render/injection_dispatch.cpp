#include "render/injection_dispatch.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/graphics/output_readback.hpp"
#include "RobloxModLoader/roblox/graphics/device.hpp"
#include "RobloxModLoader/roblox/graphics/device_context.hpp"
#include "RobloxModLoader/roblox/graphics/global_shader_data.hpp"
#include "RobloxModLoader/roblox/graphics/main_render_targets.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "RobloxModLoader/roblox/graphics/texture.hpp"
#include "render/injection_table.hpp"
#include "render/injection_traits.hpp"
#include "roblox/graphics/graphics_registry.hpp"

RML_LOG_SCOPE("Render");

namespace rml::render::detail
{
	static thread_local int t_depth = 0;

	InjectionDispatch& InjectionDispatch::instance()
	{
		static InjectionDispatch dispatch;
		return dispatch;
	}

	RenderGraphImpl& InjectionDispatch::graph()
	{
		return m_graph;
	}

	PassSuspension& InjectionDispatch::suspension()
	{
		return m_suspension;
	}

	std::uint64_t InjectionDispatch::frame_index() const
	{
		return m_frame_index;
	}

	void InjectionDispatch::ensure_device(RBX::Graphics::Device& device)
	{
		if (m_device == &device && m_resources && m_fullscreen.geometry)
			return;
		m_resources.reset();
		m_output_copy.reset();
		m_fullscreen = {};
		m_device = &device;
		m_resources = std::make_unique<FrameResourcesImpl>(device);
		m_fullscreen = create_fullscreen_geometry(device);
		if (!m_fullscreen.geometry)
			RML_ERROR("fullscreen geometry could not be created; injection points are disabled");
	}

	bool InjectionDispatch::begin_view(RBX::Graphics::SceneManager& scene, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* output, const RBX::Graphics::RenderCamera* camera, const std::uint32_t capture_mode)
	{
		if (t_depth++ > 0 || !context)
			return false;
		auto unowned = std::thread::id{};
		if (!m_owner.compare_exchange_strong(unowned, std::this_thread::get_id(), std::memory_order_acq_rel))
			return false;

		auto& registry = graphics::GraphicsRegistry::instance();
		auto* device = registry.validate() ? registry.device() : nullptr;
		if (device)
			ensure_device(*device);
		if (!device || !m_fullscreen.geometry)
		{
			m_owner.store({}, std::memory_order_release);
			return false;
		}

		const auto now = std::chrono::steady_clock::now();
		const float delta = m_last_time.time_since_epoch().count() == 0 ? 0.f : std::chrono::duration<float>(now - m_last_time).count();
		m_last_time = now;
		if (std::exchange(m_main_view_now, false))
			m_main_view_seen = true;

		const auto* targets = scene.get_main_render_targets();
		m_scene = &scene;
		m_context = context;
		m_output = output;
		m_occurrences.clear();
		m_main_done = false;
		++m_frame_index;
		m_resources->begin_frame(m_frame_index);

		auto& frame = m_frame.emplace(FrameContext{*m_resources});
		frame.device = device;
		frame.globals = &scene.read_global_shader_data();
		frame.camera = camera;
		frame.view_width = scene.view_width;
		frame.view_height = scene.view_height;
		frame.render_width = scene.drs_width ? scene.drs_width : scene.view_width;
		frame.render_height = scene.drs_height ? scene.drs_height : scene.view_height;
		frame.output_width = output ? output->width : 0;
		frame.output_height = output ? output->height : 0;
		frame.samples = targets && targets->sample_count > 1 ? targets->sample_count : 1;
		frame.pipeline = m_main_view_seen ? Pipeline::MainView : Pipeline::Classic;
		frame.capture = capture_mode != 0;
		frame.delta_time = delta;
		frame.frame_index = m_frame_index;

		auto& table = InjectionTable::instance();
		auto& stages = EngineStages::instance();
		m_frame_lock = m_graph.begin_frame(frame, [&table](const InjectionPoint point) { return table.resolved(point); }, stages.tokens().snapshot());
		stages.set_frame(m_graph.skip_set());
		m_suspension.set_widening(m_graph.plan().has_offscreen_in_pass());

		if (!m_announced && !m_graph.plan().points.empty())
		{
			m_announced = true;
			std::size_t passes = 0;
			for (const auto& point : m_graph.plan().points)
				passes += point.offscreen.size() + point.scene.size();
			RML_INFO("render graph active: {} pass(es) at {} injection point(s)", passes, m_graph.plan().points.size());
		}

		reach(InjectionPoint::at(FramePoint::FrameBegin));
		return true;
	}

	void InjectionDispatch::end_view()
	{
		if (owns_frame())
		{
			if (m_frame)
			{
				reach(InjectionPoint::at(FramePoint::FrameEnd));
				m_graph.end_frame(*m_frame, std::move(m_frame_lock));
				m_frame.reset();
				m_scene = nullptr;
				m_context = nullptr;
				m_output = nullptr;
			}
			m_owner.store({}, std::memory_order_release);
		}
		--t_depth;
	}

	bool InjectionDispatch::owns_frame() const
	{
		return t_depth == 1 && m_owner.load(std::memory_order_acquire) == std::this_thread::get_id();
	}

	static bool after_main(const InjectionPoint point)
	{
		return point == InjectionPoint::at(FramePoint::MainAfterOpaque) || point == InjectionPoint::at(FramePoint::UIBefore) || point == InjectionPoint::at(FramePoint::FrameEnd) || (point.kind() == InjectionKind::Stage && point.engine_stage() == EngineStage::UI) || point == InjectionPoint::queue(QueueGroup::Opaque, Side::After);
	}

	void InjectionDispatch::publish_builtins(const InjectionPoint point, const bool offscreen)
	{
		m_resources->withdraw(resource_names::SCENE_DEPTH);
		m_resources->withdraw(resource_names::SCENE_COLOR);
		m_resources->withdraw(resource_names::OUTPUT_COLOR);
		if (point == InjectionPoint::at(FramePoint::FrameEnd) && m_output)
			m_resources->provide(resource_names::OUTPUT_COLOR, [this] {
				return copy_output();
			});

		const bool readable = traits(point).state == PassState::InPass ? offscreen && m_main_done : m_main_done;
		const auto* targets = m_scene ? m_scene->get_main_render_targets() : nullptr;
		if (!readable || !targets)
			return;
		if (const auto* scene_fb = targets->scene_fb.get(); scene_fb && scene_fb->depth.texture)
			m_resources->publish(resource_names::SCENE_DEPTH, scene_fb->depth.texture.get());
		if (auto* color = targets->main_color())
			m_resources->publish(resource_names::SCENE_COLOR, color);
	}

	RBX::Graphics::Texture* InjectionDispatch::copy_output()
	{
		using RBX::Graphics::Texture;
		if (!m_readback.load() || !m_output || !m_context || !m_device || m_output->samples > 1 || m_output->color.empty()
		    || !m_output->color[0].texture)
			return nullptr;
		if (!rml::platform::output_readable(m_output->width, m_output->height))
			return nullptr;

		const auto& source = *m_output->color[0].texture;
		if (!m_output_copy || m_output_copy->width != m_output->width || m_output_copy->height != m_output->height
		    || m_output_copy->format != source.format)
		{
			m_output_copy.reset();
			try
			{
				const auto usage = static_cast<Texture::Usage>(static_cast<std::uint32_t>(Texture::Usage::ShaderRead) | static_cast<std::uint32_t>(Texture::Usage::RenderTarget));
				m_output_copy = m_device->create_texture_impl(Texture::Type::Type_2D,
				    static_cast<Texture::Format>(source.format),
				    m_output->width,
				    m_output->height,
				    1,
				    1,
				    1,
				    1,
				    usage,
				    "rml:output_color");
			}
			catch (const std::exception& e)
			{
				RML_ERROR("output_color could not be created: {}", e.what());
				return nullptr;
			}
		}
		if (!m_output_copy)
			return nullptr;

		m_context->copy_framebuffer(m_output, m_output_copy.get(), 0, 0);
		return m_output_copy.get();
	}

	bool InjectionDispatch::enable_output_readback(const bool enabled)
	{
		if (m_readback.load() == enabled)
			return true;
		if (!rml::platform::set_output_readable(enabled))
			return false;
		m_readback.store(enabled);
		return true;
	}

	void InjectionDispatch::run(const std::vector<std::size_t>& passes, const InjectionPoint point, const std::uint32_t occurrence, const CommandsImpl::Mode mode)
	{
		for (const auto index : passes)
		{
			CommandsImpl commands(*m_context, *m_fullscreen.geometry, mode);
			const RenderContext ctx{*m_frame, commands, point, occurrence};
			m_context->begin_group(m_graph.pass_name(index).c_str(), 0);
			m_graph.render(index, ctx);
			commands.finish();
			m_context->end_group();
		}
	}

	void InjectionDispatch::reach(const InjectionPoint point)
	{
		if (!owns_frame() || !m_frame || !m_context)
			return;

		InjectionTable::instance().fired(point, m_frame_index);
		if (after_main(point))
			m_main_done = true;

		const auto* plan = m_graph.plan().find(point);
		if (!plan)
			return;

		const auto occurrence = m_occurrences[point.key()]++;
		const auto info = traits(point);
		bool ran = false;

		if (!plan->offscreen.empty())
		{
			publish_builtins(point, true);
			auto* target = info.state == PassState::InPass ? m_context->get_framebuffer() : nullptr;
			if (target)
			{
				const auto suspended = m_suspension.suspend(*m_context, target);
				run(plan->offscreen, point, occurrence, CommandsImpl::Mode::Offscreen);
				m_suspension.resume(*m_context, target, suspended);
			}
			else
			{
				run(plan->offscreen, point, occurrence, CommandsImpl::Mode::Offscreen);
			}
			ran = true;
		}

		if (!plan->scene.empty())
		{
			publish_builtins(point, false);
			if (info.state == PassState::Between && info.scene_target == SceneTarget::ViewOutput)
			{
				if (m_output)
				{
					m_context->begin_pass(m_output, m_output->mask, m_output->mask, nullptr, nullptr, 0);
					run(plan->scene, point, occurrence, CommandsImpl::Mode::Scene);
					m_context->end_pass();
					ran = true;
				}
			}
			else
			{
				run(plan->scene, point, occurrence, CommandsImpl::Mode::Scene);
				ran = true;
			}
		}

		if (ran && m_frame->globals)
			m_context->bind_buffer_data(0, m_frame->globals, static_cast<unsigned>(sizeof(RBX::Graphics::GlobalShaderData)));
	}

	bool InjectionDispatch::on_scene_target() const
	{
		if (!owns_frame() || !m_frame || !m_context || !m_scene)
			return false;
		const auto* targets = m_scene->get_main_render_targets();
		return targets && targets->scene_fb && m_context->get_framebuffer() == targets->scene_fb.get();
	}

	bool InjectionDispatch::on_output_target() const
	{
		return owns_frame() && m_frame && m_context && m_output && m_context->get_framebuffer() == m_output;
	}

	bool InjectionDispatch::plans_clouds_path() const
	{
		return std::ranges::any_of(m_graph.plan().points, [](const PointPlan& plan) {
			const auto point = plan.point;
			return point == InjectionPoint::at(FramePoint::MainAfterOpaque) || point == InjectionPoint::at(FramePoint::CloudsPrepare) || (point.kind() == InjectionKind::Stage && point.engine_stage() == EngineStage::Clouds);
		});
	}

	void InjectionDispatch::note_main_view()
	{
		m_main_view_now = true;
	}

	void InjectionDispatch::device_lost(RBX::Graphics::Device& device)
	{
		if (m_device != &device)
			return;
		m_graph.device_lost();
		m_resources.reset();
		m_output_copy.reset();
		m_fullscreen = {};
		m_device = nullptr;
		RML_INFO("render resources released with the device");
	}
}

namespace rml::render
{
	RenderGraph& graph()
	{
		return detail::InjectionDispatch::instance().graph();
	}

	bool enable_output_readback(const bool enabled)
	{
		return detail::InjectionDispatch::instance().enable_output_readback(enabled);
	}
}
