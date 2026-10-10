#include "RobloxModLoader/hooking/hooking.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/roblox/job_vtable.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "pointers.hpp"
#include "RobloxModLoader/qt/qt_module.hpp"
#include "render/engine_stages.hpp"
#include "render/injection_table.hpp"
#include "roblox/graphics/graphics_registry.hpp"

#include <utility>

RML_LOG_SCOPE("Hooking");

namespace rml
{
	Hooking::Hooking() :
	    m_hook_engine(g_hook_engine ? nullptr : create_hook_engine()),
	    m_owns_engine(m_hook_engine != nullptr)
	{
		if (m_owns_engine)
			g_hook_engine = m_hook_engine.get();

		RML_INFO("Initializing hooking");

		for (const auto kind : {rml::JobKind::Heartbeat, rml::JobKind::Physics, rml::JobKind::WaitingHybridScripts, rml::JobKind::Render})
		{
			const auto vtable = rml::task_scheduler().get_vtable_for_job_kind(kind);

			if (!vtable.has_value())
			{
				RML_WARN("Failed to get vtable for job kind {}", std::to_underlying(kind));
				continue;
			}

			const auto step_slot = rml::job_step_slot();

			auto job_hook = std::make_unique<vtable_hook>(*vtable, step_slot + 1);
			job_hook->hook(step_slot, reinterpret_cast<void*>(&Hooks::on_job_step));
			m_jobs_hook[kind] = std::move(job_hook);
			RML_DEBUG("Hooked job kind {} with vtable 0x{:X}", std::to_underlying(kind), reinterpret_cast<std::uintptr_t>(*vtable));
		}

		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			if (detour_hook_helper.m_on_hooking_available)
				detour_hook_helper.m_detour_hook->set_target_and_create_hook(detour_hook_helper.m_on_hooking_available());
		}

#if RML_ENABLE_LUAU
		DetourHookHelper::add<Hooks::luau_load>("LUAU_LOAD", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.luau_load));
#endif
		DetourHookHelper::add<Hooks::build_menu_bar_from_dom>("MENU_BUILD_FROM_DOM",
		    reinterpret_cast<void*>(g_pointers->m_roblox_pointers.build_menu_bar_from_dom));

		if (g_pointers->m_roblox_pointers.creatable_get_creator)
			DetourHookHelper::add<Hooks::creatable_get_creator>("CREATABLE_GET_CREATOR",
			    reinterpret_cast<void*>(g_pointers->m_roblox_pointers.creatable_get_creator));

		if (g_pointers->m_roblox_pointers.visual_engine_begin_render)
			DetourHookHelper::add<Hooks::visual_engine_begin_render>("VISUAL_ENGINE_BEGIN_RENDER",
			    reinterpret_cast<void*>(g_pointers->m_roblox_pointers.visual_engine_begin_render));

		auto& injection_table = render::detail::InjectionTable::instance();
		auto& engine_stages = render::detail::EngineStages::instance();

		if (g_pointers->m_roblox_pointers.scene_manager_render_scene)
		{
			DetourHookHelper::add<Hooks::scene_manager_render_scene>("SCENE_MANAGER_RENDER_SCENE",
			    reinterpret_cast<void*>(g_pointers->m_roblox_pointers.scene_manager_render_scene));
			injection_table.resolve(render::InjectionPoint::at(render::FramePoint::FrameBegin));
			injection_table.resolve(render::InjectionPoint::at(render::FramePoint::FrameEnd));
		}

		if (const auto& pointers = g_pointers->m_roblox_pointers; pointers.clouds_update && (pointers.clouds_composite || pointers.clouds_composite_clouds))
		{
			DetourHookHelper::add<Hooks::clouds_update>("CLOUDS_UPDATE", reinterpret_cast<void*>(pointers.clouds_update));
			if (pointers.clouds_composite)
				DetourHookHelper::add<Hooks::clouds_composite>("CLOUDS_COMPOSITE", reinterpret_cast<void*>(pointers.clouds_composite));
			else
				DetourHookHelper::add<Hooks::clouds_composite_clouds>("CLOUDS_COMPOSITE_CLOUDS", reinterpret_cast<void*>(pointers.clouds_composite_clouds));
			injection_table.resolve(render::InjectionPoint::at(render::FramePoint::CloudsPrepare));
			injection_table.resolve(render::InjectionPoint::at(render::FramePoint::MainAfterOpaque));
			injection_table.resolve_stage(render::EngineStage::Clouds);
			engine_stages.mark_hooked(render::EngineStage::Clouds);
		}

		if (const auto begin_pass = graphics::device_context_begin_pass_target())
			DetourHookHelper::add<Hooks::device_context_begin_pass>("DEVICE_CONTEXT_BEGIN_PASS", begin_pass);

		const bool classic_queues = g_pointers->m_roblox_pointers.render_objects_clipped != nullptr;
		const bool main_view_queues = g_pointers->m_roblox_pointers.dispatch_scene_dispatch != nullptr;
		if (classic_queues)
			DetourHookHelper::add<Hooks::render_objects_clipped>("RENDER_OBJECTS_CLIPPED", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.render_objects_clipped));
		if (main_view_queues)
			DetourHookHelper::add<Hooks::dispatch_scene_dispatch>("DISPATCH_SCENE_DISPATCH", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.dispatch_scene_dispatch));
		if (classic_queues || main_view_queues)
		{
			injection_table.load_queue_names();
			injection_table.resolve_queues();
			engine_stages.mark_queues_hooked();
		}
		if (classic_queues != main_view_queues)
			RML_WARN("queue injection points cover only the {} pipeline", classic_queues ? "classic" : "MainView");

		if (g_pointers->m_roblox_pointers.scene_manager_render_sky)
		{
			DetourHookHelper::add<Hooks::scene_manager_render_sky>("SCENE_MANAGER_RENDER_SKY", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.scene_manager_render_sky));
			injection_table.resolve_stage(render::EngineStage::Sky);
			engine_stages.mark_hooked(render::EngineStage::Sky);
		}

		if (g_pointers->m_roblox_pointers.scene_manager_render_ui)
		{
			DetourHookHelper::add<Hooks::scene_manager_render_ui>("SCENE_MANAGER_RENDER_UI", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.scene_manager_render_ui));
			injection_table.resolve(render::InjectionPoint::at(render::FramePoint::UIBefore));
			injection_table.resolve_stage(render::EngineStage::UI);
			engine_stages.mark_hooked(render::EngineStage::UI);
		}

		if (const auto device_destructor = graphics::device_destructor_target())
			DetourHookHelper::add<Hooks::device_destroy>("DEVICE_DESTROY", device_destructor);

		if (g_pointers->m_roblox_pointers.reflection_metadata_load)
			DetourHookHelper::add<Hooks::reflection_metadata_load>("REFLECTION_METADATA_LOAD", reinterpret_cast<void*>(g_pointers->m_roblox_pointers.reflection_metadata_load));

		if (const auto translate = qt::detail::core_export_optional("QCoreApplication::translate(char const*, char const*, char const*, int)"))
			DetourHookHelper::add<Hooks::qt_translate>("QT_TRANSLATE", translate);

		if (const auto pre_submit_pass = graphics::adorn_render_pre_submit_pass_target())
			DetourHookHelper::add<Hooks::adorn_render_pre_submit_pass>("ADORN_RENDER_PRE_SUBMIT_PASS", pre_submit_pass);

		g_hooking = this;
	}

	Hooking::~Hooking()
	{
		if (m_enabled)
		{
			disable();
		}

		g_hooking = nullptr;
		if (m_owns_engine)
			g_hook_engine = nullptr;
	}

	void Hooking::enable()
	{
		for (auto& job_hook : m_jobs_hook | std::views::values)
		{
			if (!job_hook)
				continue;

			if (const auto result = job_hook->enable(); !result)
				RML_ERROR("Failed to enable job vtable hook: {}", result.error().describe());
		}

		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			if (const auto result = detour_hook_helper.m_detour_hook->enable(); !result)
				RML_ERROR("Failed to enable detour hook: {}", result.error().describe());
		}

		if (g_hook_engine)
			g_hook_engine->apply_queued();

		m_enabled = true;
	}

	void Hooking::disable()
	{
		m_enabled = false;

		for (auto& job_hook : m_jobs_hook | std::views::values)
		{
			if (job_hook)
			{
				if (const auto result = job_hook->disable(); !result)
					RML_WARN("Failed to disable job vtable hook: {}", result.error().describe());
			}
		}

		for (auto& detour_hook_helper : m_detour_hook_helpers)
		{
			if (const auto result = detour_hook_helper.m_detour_hook->disable(); !result)
				RML_WARN("Failed to disable detour hook: {}", result.error().describe());
		}

		if (g_hook_engine)
			g_hook_engine->apply_queued();

		m_detour_hook_helpers.clear();
	}

	Hooking::DetourHookHelper::~DetourHookHelper()
	{
	}

	void Hooking::DetourHookHelper::register_helper(const DetourHookHelper& helper)
	{
		helper.enable_hook_if_hooking_is_already_running();
		m_detour_hook_helpers.push_back(helper);
	}

	void Hooking::DetourHookHelper::disable_hook(DetourHook& hook)
	{
		if (const auto result = hook.disable(); !result)
			RML_WARN("Failed to disable detour hook '{}': {}", hook.name(), result.error().describe());

		if (g_hook_engine)
			g_hook_engine->apply_queued();
	}

	void Hooking::DetourHookHelper::unregister_helper(DetourHook& hook)
	{
		std::erase_if(m_detour_hook_helpers, [&hook](const DetourHookHelper& helper) {
			return helper.m_detour_hook == &hook;
		});

		hook.destroy();
	}

	void Hooking::DetourHookHelper::enable_hook_if_hooking_is_already_running() const
	{
		if (g_hooking && g_hooking->m_enabled)
			enable_now();
	}

	void Hooking::DetourHookHelper::enable_now() const
	{
		if (m_on_hooking_available)
		{
			m_detour_hook->set_target_and_create_hook(m_on_hooking_available());
		}

		if (const auto result = m_detour_hook->enable(); !result)
			RML_ERROR("Failed to enable detour hook '{}' immediately: {}", m_detour_hook->name(), result.error().describe());

		if (g_hook_engine)
			g_hook_engine->apply_queued();
	}

}