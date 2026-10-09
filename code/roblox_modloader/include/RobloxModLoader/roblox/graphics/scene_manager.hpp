#pragma once

#include "RobloxModLoader/roblox/rsl/condition.hpp"
#include "RobloxModLoader/roblox/rsl/thread.hpp"
#include "RobloxModLoader/roblox/util/dynamic_bitset.hpp"
#include "RobloxModLoader/roblox/graphics/global_shader_data.hpp"
#include "RobloxModLoader/roblox/graphics/main_render_targets.hpp"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>


namespace RBX
{
	class Quaternion;

	namespace DataModelBinding
	{
		class FetcherFrontend;
	}

	struct RuntimeConfigMain
	{
		bool gbuffer;
		bool post_fx_bloom;
		std::int32_t ssao_level;
		std::int32_t post_fx_level;
		std::uint32_t msaa_level;
		std::int32_t texture_anisotropy;
		std::int32_t ui_texture_perf_tier;
		std::int32_t quality_level;
		std::int32_t shadow_map_cascade_update_level;
		std::int32_t shadow_map_updates_per_frame;
		std::uint32_t legacy_shadow_level;
		bool constant_fog_low_quality;
		float view_cull_sq_distance;
		float render_cull_sq_distance;
		float cull_pixels_over_target_height_main_view;
		float cull_pixels_over_target_height_shadow_map;
		std::int32_t viewport_frame_render_count;
		float max_megapixel_count;

	private:
		bool reserved_44;

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(RuntimeConfigMain, reserved_44, 0x44);
		RML_LAYOUT_GUARD_END()
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(RuntimeConfigMain, msaa_level, 0xC);
	RML_ASSERT_OFFSET(RuntimeConfigMain, legacy_shadow_level, 0x24);
	RML_ASSERT_OFFSET(RuntimeConfigMain, constant_fog_low_quality, 0x28);
	RML_ASSERT_OFFSET(RuntimeConfigMain, view_cull_sq_distance, 0x2C);
	RML_ASSERT_OFFSET(RuntimeConfigMain, max_megapixel_count, 0x40);
	RML_ASSERT_SIZE(RuntimeConfigMain, 0x48);
	RML_LAYOUT_DIAGNOSTIC_POP()
}

namespace RBX::Graphics
{
	class VisualEngine;
	class CullableScene;
	class CullableSceneNode;
	class RenderQueue;
	class VertexStreamerMigrationLayer;
	class GeometryBatch;
	class MainView;
	class EnvMapView;
	class SoftwareOcclusionTarget;
	class DispatchScene;
	class Sky;
	class AdvSky;
	class Clouds;
	class SSAO;
	class SunRays;
	class EnvMapPBR;
	class MotionBuffer;
	class MLPostProcess;
	class ShadowMap;
	class WatermarkRenderer;
	class LightObject;

	enum class ScenePhase : std::uint8_t
	{
		None,
		Update,
		Render
	};

	struct VisibleQuery
	{
		dynamic_bitset visible;

	private:
		std::uint32_t reserved_18;
		std::uint32_t reserved_1c;
		std::uint32_t reserved_20;

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(VisibleQuery, reserved_18, 0x18);
		RML_ASSERT_OFFSET(VisibleQuery, reserved_20, 0x20);
		RML_LAYOUT_GUARD_END()
	};

	struct Glow
	{
		bool enabled;
		float intensity;
		float size;
		bool bloom_enabled;
		float bloom_intensity;
		float bloom_size;
		float bloom_threshold;
		VisualEngine* visual_engine;
		std::unique_ptr<RTPool::RT> history_buffers[2];
		std::uint32_t history_index;
	};

	struct Blur
	{
		class SceneManager* scene_manager;
		bool enabled;
		float size;
	};

	struct DepthOfField
	{
		Matrix4 inverse_projection;
		Matrix4 projection;
		class SceneManager* scene_manager;
		float far_focus_distance;
		float near_blur_distance;
		float far_blur_distance;
		float near_intensity;
		float far_intensity;
		bool enabled;
	};

	struct ColorCorrection
	{
		bool enabled;
		Color3 tint_color;
		float brightness;
		float contrast;
		float saturation;
	};

	struct Highlight
	{
		VisualEngine* visual_engine;
	};

	struct Vignette
	{
		VisualEngine* visual_engine;
		Vector3 last_camera_position;
		float intensity;
		float camera_speed;
		float eye_offset;
	};

	struct ColorGrading
	{
		bool enabled;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(VisibleQuery, 0x28);
	RML_ASSERT_OFFSET(Glow, visual_engine, 0x20);
	RML_ASSERT_OFFSET(Glow, history_buffers, 0x28);
	RML_ASSERT_OFFSET(Glow, history_index, 0x38);
	RML_ASSERT_SIZE(Glow, 0x40);
	RML_ASSERT_SIZE(Blur, 0x10);
	RML_ASSERT_OFFSET(DepthOfField, scene_manager, 0x80);
	RML_ASSERT_OFFSET(DepthOfField, far_blur_distance, 0x90);
	RML_ASSERT_OFFSET(DepthOfField, near_intensity, 0x94);
	RML_ASSERT_OFFSET(DepthOfField, enabled, 0x9C);
	RML_ASSERT_SIZE(DepthOfField, 0xA0);
	RML_ASSERT_SIZE(ColorCorrection, 0x1C);
	RML_ASSERT_OFFSET(Vignette, intensity, 0x14);
	RML_ASSERT_OFFSET(Vignette, eye_offset, 0x1C);
	RML_ASSERT_SIZE(Vignette, 0x20);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class SceneManager
	{
	public:
		struct GuiClusterRT;

		VisualEngine* visual_engine;
		std::int32_t output_rect_x;
		std::int32_t output_rect_y;
		std::int32_t output_rect_width;
		std::int32_t output_rect_height;
		std::uint32_t view_width;
		std::uint32_t view_height;
		std::uint32_t drs_width;
		std::uint32_t drs_height;
		Vector3 point_of_interest;
		float sq_min_part_distance;
		float view_cull_sq_distance;
		std::unique_ptr<CullableScene> cullable_scene;

	private:
		std::uint64_t reserved_48;

	public:
		RSL::Thread cull_job_thread;
		RSL::Condition cull_job_fence;
		std::atomic<std::uint32_t> cull_job_state;
		std::vector<CullableSceneNode*> query_nodes;
		std::vector<CullableSceneNode*> render_nodes;
		std::unique_ptr<RenderQueue> render_queue;
		std::unique_ptr<RenderQueue> player_gui_render_queue;
		std::unique_ptr<RenderQueue> capture_render_queue;
		std::unique_ptr<VertexStreamerMigrationLayer> capture_vertex_streamer;
		std::unique_ptr<RenderQueue> shadow_render_queue;
		std::unique_ptr<RenderQueue> gui_render_queue;
		std::unique_ptr<RenderQueue> gui_prepass_render_queue;
		std::unique_ptr<RenderQueue> env_map_render_queue;
		std::unique_ptr<RenderQueue> terrain_feedback_render_queue;
		std::unique_ptr<RenderQueue> highlight_render_queues[2];

	private:
		std::unique_ptr<RenderQueue> reserved_f0;

	public:
		std::unique_ptr<RenderQueue> performance_overlay_render_queue;
		std::vector<std::pair<std::uint64_t, CullableSceneNode*>> env_map_nodes;
		std::size_t env_map_node_cursor;
		std::unique_ptr<RenderQueue> studio_selection_render_queues[5];
		VisibleQuery main_query;
		VisibleQuery terrain_query;
		std::unique_ptr<MainView> main_view;
		std::unique_ptr<EnvMapView> env_map_view;
		std::shared_ptr<SoftwareOcclusionTarget> software_occlusion_target;
		bool forced_bloom;
		bool wireframe_rendering;
		bool clouds_enabled;
		bool sky_enabled;
		bool high_dpi_framebuffer_halved;
		bool deterministic_capture_full_resolution;
		std::int32_t sky_mode;
		Color4 clear_color;
		Color4 clear_color_2d;
		bool reduced_motion_enabled;
		GlobalShaderData global_shader_data;
		float fog_end;
		float fog_inv_range;
		float exposure_compensation;
		float camera_rotation_smoothed[4];
		Matrix4 camera_change_matrix;
		Vector3 last_camera_position;
		Vector3 last_camera_direction;
		float camera_change;
		std::unique_ptr<GeometryBatch> fullscreen_triangle;
		std::unique_ptr<GeometryBatch> shadow_box;
		std::unique_ptr<Sky> sky;
		std::unique_ptr<AdvSky> adv_sky;
		std::unique_ptr<Clouds> clouds;
		std::unique_ptr<RTPool> rt_pool;
		std::unique_ptr<MainRenderTargets> main_render_targets;
		std::unique_ptr<SSAO> ssao;
		std::unique_ptr<Glow> glow;
		std::unique_ptr<Blur> blur;
		std::unique_ptr<DepthOfField> depth_of_field;
		std::unique_ptr<SunRays> sun_rays;
		std::unique_ptr<EnvMapPBR> env_map;
		std::unique_ptr<ColorCorrection> color_correction;
		std::unique_ptr<Highlight> highlight;
		std::unique_ptr<MotionBuffer> motion_buffer;
		std::unique_ptr<Vignette> vignette;
		std::unique_ptr<ColorGrading> color_grading;
		std::unique_ptr<MLPostProcess> ml_post_process;
		std::unique_ptr<DispatchScene> dispatch_scene;
		std::unique_ptr<DataModelBinding::FetcherFrontend> fetcher_frontend;
		std::unique_ptr<ShadowMap> shadow_maps[3];

	private:
		float reserved_6f0;

	public:
		TextureRef shadow_blur_mask;
		TextureRef shadow_mask;
		TextureRef shadow_map_texture;
		TextureRef ssao_texture;
		std::unique_ptr<WatermarkRenderer> watermark_renderer;

	private:
		double reserved_740;

	public:
		double elapsed_time;
		RuntimeConfigMain runtime_config;
		std::vector<std::shared_ptr<GuiClusterRT>> gui_cluster_rts;
		std::uint32_t supported_color_write_mask;
		ScenePhase phase;

	private:
		bool reserved_7b5;

	public:
		std::int32_t multiview_stereo;
		std::vector<LightObject*> visible_lights;

	private:
		std::vector<void*> reserved_7d8;

	public:
		std::int32_t overdraw_queries[2];
		std::uint64_t overdraw_query_results[2];
		std::int32_t overdraw_query_pending;
		double overdraw;

		CullableScene* get_cullable_scene() const
		{
			return cullable_scene.get();
		}

		Sky* get_sky() const
		{
			return sky.get();
		}

		EnvMapPBR* get_env_map() const
		{
			return env_map.get();
		}

		SSAO* get_ssao() const
		{
			return ssao.get();
		}

		MainRenderTargets* get_main_render_targets() const
		{
			return main_render_targets.get();
		}

		const GlobalShaderData& read_global_shader_data() const
		{
			return global_shader_data;
		}

		GlobalShaderData& write_global_shader_data()
		{
			return global_shader_data;
		}

		const Vector3& get_point_of_interest() const
		{
			return point_of_interest;
		}

		float get_minimum_sq_part_distance() const
		{
			return sq_min_part_distance;
		}

		const GeometryBatch& get_fullscreen_triangle() const
		{
			return *fullscreen_triangle;
		}

		std::uint32_t get_drawport_width() const
		{
			return drs_width;
		}

		std::uint32_t get_drawport_height() const
		{
			return drs_height;
		}

		std::uint32_t get_view_width() const
		{
			return view_width;
		}

		std::uint32_t get_view_height() const
		{
			return view_height;
		}

		const RuntimeConfigMain& get_runtime_config() const
		{
			return runtime_config;
		}

		const TextureRef& get_color_opaque() const
		{
			return main_render_targets->color_opaque;
		}

		const TextureRef& get_depth_opaque() const
		{
			return main_render_targets->depth_opaque;
		}

		bool can_read_main_rt_depth() const
		{
			return main_render_targets && main_render_targets->mode == MainRenderTargets::Mode::WithDepth;
		}

		Texture* get_main_depth() const
		{
			return main_render_targets ? main_render_targets->main_depth.get() : nullptr;
		}

		Texture* get_main_color() const
		{
			return main_render_targets ? main_render_targets->main_color() : nullptr;
		}

		std::uint32_t get_sample_count() const
		{
			return main_render_targets ? main_render_targets->sample_count : 1;
		}

		ShadowMap* pick_shadow_map() const
		{
			const auto level = runtime_config.legacy_shadow_level;
			if (shadow_maps[2] && level == 3)
				return shadow_maps[2].get();
			if (shadow_maps[1] && (level == 2 || level == 3))
				return shadow_maps[1].get();
			return level == 1 ? shadow_maps[0].get() : nullptr;
		}

	private:
		SceneManager() = delete;

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(SceneManager, reserved_48, 0x48);
		RML_ASSERT_OFFSET(SceneManager, reserved_f0, 0xF0);
		RML_ASSERT_OFFSET(SceneManager, reserved_6f0, 0x6F0);
		RML_ASSERT_OFFSET(SceneManager, reserved_740, 0x740);
		RML_ASSERT_OFFSET(SceneManager, reserved_7b5, 0x7B5);
		RML_ASSERT_OFFSET(SceneManager, reserved_7d8, 0x7D8);
		RML_LAYOUT_GUARD_END()
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(SceneManager, view_width, 0x18);
	RML_ASSERT_OFFSET(SceneManager, point_of_interest, 0x28);
	RML_ASSERT_OFFSET(SceneManager, view_cull_sq_distance, 0x38);
	RML_ASSERT_OFFSET(SceneManager, cullable_scene, 0x40);
	RML_ASSERT_OFFSET(SceneManager, cull_job_state, 0x60);
	RML_ASSERT_OFFSET(SceneManager, query_nodes, 0x68);
	RML_ASSERT_OFFSET(SceneManager, render_nodes, 0x80);
	RML_ASSERT_OFFSET(SceneManager, render_queue, 0x98);
	RML_ASSERT_OFFSET(SceneManager, capture_vertex_streamer, 0xB0);
	RML_ASSERT_OFFSET(SceneManager, shadow_render_queue, 0xB8);
	RML_ASSERT_OFFSET(SceneManager, highlight_render_queues, 0xE0);
	RML_ASSERT_OFFSET(SceneManager, performance_overlay_render_queue, 0xF8);
	RML_ASSERT_OFFSET(SceneManager, env_map_nodes, 0x100);
	RML_ASSERT_OFFSET(SceneManager, studio_selection_render_queues, 0x120);
	RML_ASSERT_OFFSET(SceneManager, main_query, 0x148);
	RML_ASSERT_OFFSET(SceneManager, terrain_query, 0x170);
	RML_ASSERT_OFFSET(SceneManager, main_view, 0x198);
	RML_ASSERT_OFFSET(SceneManager, software_occlusion_target, 0x1A8);
	RML_ASSERT_OFFSET(SceneManager, forced_bloom, 0x1B8);
	RML_ASSERT_OFFSET(SceneManager, sky_mode, 0x1C0);
	RML_ASSERT_OFFSET(SceneManager, clear_color, 0x1C4);
	RML_ASSERT_OFFSET(SceneManager, reduced_motion_enabled, 0x1E4);
	RML_ASSERT_OFFSET(SceneManager, global_shader_data, 0x1E8);
	RML_ASSERT_OFFSET(SceneManager, fog_end, 0x5B8);
	RML_ASSERT_OFFSET(SceneManager, camera_change_matrix, 0x5D4);
	RML_ASSERT_OFFSET(SceneManager, camera_change, 0x62C);
	RML_ASSERT_OFFSET(SceneManager, fullscreen_triangle, 0x630);
	RML_ASSERT_OFFSET(SceneManager, shadow_box, 0x638);
	RML_ASSERT_OFFSET(SceneManager, sky, 0x640);
	RML_ASSERT_OFFSET(SceneManager, main_render_targets, 0x660);
	RML_ASSERT_OFFSET(SceneManager, env_map, 0x690);
	RML_ASSERT_OFFSET(SceneManager, color_grading, 0x6B8);
	RML_ASSERT_OFFSET(SceneManager, ml_post_process, 0x6C0);
	RML_ASSERT_OFFSET(SceneManager, shadow_maps, 0x6D8);
	RML_ASSERT_OFFSET(SceneManager, shadow_blur_mask, 0x6F8);
	RML_ASSERT_OFFSET(SceneManager, shadow_mask, 0x708);
	RML_ASSERT_OFFSET(SceneManager, watermark_renderer, 0x738);
	RML_ASSERT_OFFSET(SceneManager, elapsed_time, 0x748);
	RML_ASSERT_OFFSET(SceneManager, runtime_config, 0x750);
	RML_ASSERT_OFFSET(SceneManager, gui_cluster_rts, 0x798);
	RML_ASSERT_OFFSET(SceneManager, supported_color_write_mask, 0x7B0);
	RML_ASSERT_OFFSET(SceneManager, phase, 0x7B4);
	RML_ASSERT_OFFSET(SceneManager, multiview_stereo, 0x7B8);
	RML_ASSERT_OFFSET(SceneManager, visible_lights, 0x7C0);
	RML_ASSERT_OFFSET(SceneManager, overdraw_queries, 0x7F0);
	RML_ASSERT_OFFSET(SceneManager, overdraw_query_results, 0x7F8);
	RML_ASSERT_OFFSET(SceneManager, overdraw_query_pending, 0x808);
	RML_ASSERT_OFFSET(SceneManager, overdraw, 0x810);
	RML_ASSERT_SIZE(SceneManager, 0x818);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
