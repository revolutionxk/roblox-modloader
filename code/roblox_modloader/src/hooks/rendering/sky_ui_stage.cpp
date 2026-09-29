#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "render/injection_dispatch.hpp"

void rml::Hooks::scene_manager_render_sky(RBX::Graphics::SceneManager* self, RBX::Graphics::DeviceContext* context, const RBX::Graphics::RenderCamera* camera, const bool first, const bool second, const int face)
{
	auto& dispatch = render::detail::InjectionDispatch::instance();
	render::detail::run_stage(render::EngineStage::Sky, dispatch.on_scene_target(), [&] { Hooking::get_original<&Hooks::scene_manager_render_sky>()(self, context, camera, first, second, face); });
}

void rml::Hooks::scene_manager_render_ui(RBX::Graphics::SceneManager* self, RBX::Graphics::DeviceContext* context, const RBX::Graphics::RenderCamera* camera, void* stats, const bool rotate, const int debug_mode, const bool capture)
{
	auto& dispatch = render::detail::InjectionDispatch::instance();
	const bool inject = dispatch.on_output_target();
	if (inject)
		dispatch.reach(render::InjectionPoint::at(render::FramePoint::UIBefore));
	render::detail::run_stage(render::EngineStage::UI, inject, [&] { Hooking::get_original<&Hooks::scene_manager_render_ui>()(self, context, camera, stats, rotate, debug_mode, capture); });
}
