#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/roblox/graphics/render_camera.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "render/engine_stages.hpp"
#include "render/injection_dispatch.hpp"
#include "roblox/graphics/graphics_registry.hpp"

void rml::Hooks::scene_manager_render_scene(RBX::Graphics::SceneManager* scene_manager, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* target, const RBX::Graphics::RenderCamera* camera, RBX::ArrayView<RBX::Graphics::Framebuffer*> extra, std::uint32_t capture_mode)
{
	graphics::GraphicsRegistry::instance().set_scene_manager(scene_manager);

	auto& dispatch = render::detail::InjectionDispatch::instance();
	const bool active = dispatch.begin_view(*scene_manager, context, target, camera, capture_mode);
	const bool engine_clouds = scene_manager->clouds_enabled;
	const bool force = active && !engine_clouds && dispatch.plans_clouds_path();
	if (force)
	{
		render::detail::EngineStages::instance().force_skip(render::EngineStage::Clouds);
		scene_manager->clouds_enabled = true;
	}

	Hooking::get_original<&Hooks::scene_manager_render_scene>()(scene_manager, context, target, camera, extra, capture_mode);

	if (force)
		scene_manager->clouds_enabled = engine_clouds;
	dispatch.end_view();
}
