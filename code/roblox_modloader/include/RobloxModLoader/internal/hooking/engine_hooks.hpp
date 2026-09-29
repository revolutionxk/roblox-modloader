#pragma once

#include "RobloxModLoader/roblox/job.hpp"
#include "RobloxModLoader/roblox/util/array_view.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"

#include <cstdint>
#include <lua.h>

namespace RBX
{
	class TaskSchedulerJob;
	class ICreator;
	class Name;

	namespace Graphics
	{
		class AdornRender;
		class SceneManager;
		class Device;
		class DeviceContext;
		class Framebuffer;
		class RenderCamera;
		struct PassClear;
		struct PassResolve;
		class VisualEngine;
		struct GlobalShaderData;
	}
}

namespace rml::qt
{
	class QString;
}

namespace rml
{
	struct Hooks
	{
		static void rbx_crash(const char* type, const char* message);
		static uint64_t* on_authentication(uint64_t* _this, uint64_t doc_panel_provider, uint64_t q_image_provider);
		static RBX::TaskScheduler::StepResult on_job_step(RBX::DataModelJob* job, const RBX::Stats& time_metrics);
		static lua_Status luau_load(lua_State* L, const char* chunkname, const char* data, size_t size, int env);
		static void* build_menu_bar_from_dom(void* out_menu_bar, void* dom, void* context);
		static void qt_action_activate(void* self, int event);
		static void global_init();
		static const RBX::ICreator* creatable_get_creator(const RBX::Name* name);
		static RBX::Graphics::DeviceContext* visual_engine_begin_render(RBX::Graphics::VisualEngine* self);
		static void adorn_render_pre_submit_pass(RBX::Graphics::AdornRender* self);
		static void scene_manager_render_scene(RBX::Graphics::SceneManager* self, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* target, const RBX::Graphics::RenderCamera* camera, RBX::ArrayView<RBX::Graphics::Framebuffer*> extra, std::uint32_t capture_mode);
		static void clouds_update(void* clouds, RBX::Graphics::DeviceContext* context, void* view_info, const RBX::Graphics::RenderCamera* camera, RBX::Graphics::Framebuffer* main_framebuffer, RBX::Graphics::GlobalShaderData* globals, const void* camera_change, void* stats);
		static void clouds_composite(void* clouds, RBX::Graphics::DeviceContext* context, const void* camera, RBX::Graphics::GlobalShaderData* globals, void* stats);
		static void clouds_composite_clouds(void* clouds, RBX::Graphics::DeviceContext* context, RBX::Graphics::GlobalShaderData* globals, void* stats);
		static void* device_destroy(RBX::Graphics::Device* self, unsigned int flags);
		static void device_context_begin_pass(RBX::Graphics::DeviceContext* self, RBX::Graphics::Framebuffer* framebuffer, unsigned load_mask, unsigned store_mask, const RBX::Graphics::PassClear* clear, const RBX::Graphics::PassResolve* resolve, unsigned flags);
		static void render_objects_clipped(RBX::Graphics::DeviceContext* context, void* instance_glob, const void* view, void* group, void* stats, std::uint32_t tracker, const void* tokens, void* clip, bool first, bool second);
		static void dispatch_scene_dispatch(void* self, const void* context, const void* view, std::uint32_t pass, std::uint32_t id, const void* query);
		static void* reflection_metadata_load(void* self, const void* path);
		static qt::QString qt_translate(const char* context, const char* key, const char* disambiguation, int n);
		static void scene_manager_render_sky(RBX::Graphics::SceneManager* self, RBX::Graphics::DeviceContext* context, const RBX::Graphics::RenderCamera* camera, bool first, bool second, int face);
		static void scene_manager_render_ui(RBX::Graphics::SceneManager* self, RBX::Graphics::DeviceContext* context, const RBX::Graphics::RenderCamera* camera, void* stats, bool rotate, int debug_mode, bool capture);
	};
}