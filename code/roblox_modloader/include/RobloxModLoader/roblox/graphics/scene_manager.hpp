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
#include <vector>


namespace RBX
{
	class RuntimeConfigMain;
	class Quaternion;

	namespace DataModelBinding
	{
		class FetcherFrontend;
	}
}

namespace RBX::Graphics
{
	class VisualEngine;
	class CullableScene;
	class CullableSceneNode;
	class RenderQueue;
	class GeometryBatch;
	class MainView;
	class EnvMapView;
	class DispatchScene;
	class Sky;
	class AdvSky;
	class Clouds;
	class SSAO;
	class SunRays;
	class EnvMapPBR;
	class MotionBuffer;
	class ShadowMap;
	class WatermarkRenderer;

	enum class ScenePhase : std::uint8_t
	{
		None,
		Update,
		Render
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
		std::uint32_t reserved_28;
		VisualEngine* visual_engine;
		std::byte reserved_40[24];
	};

	struct Blur
	{
		class SceneManager* scene_manager;
		bool enabled;
		float size;
	};

	struct DepthOfField
	{
		Matrix4 reserved_0;
		Matrix4 reserved_64;
		class SceneManager* scene_manager;
		float far_focus_distance;
		float near_focus_distance;
		float reserved_144;
		float near_intensity;
		float far_intensity;
		bool enabled;
		std::byte reserved_157[3];
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
		float reserved_8[4];
		float reserved_24;
		float eye_offset;
	};

	struct ColorGrading
	{
		bool enabled;
	};

	struct RuntimeConfigMain
	{
		std::byte reserved_0[12];
		std::uint32_t msaa_level;
		std::byte reserved_16[20];
		std::uint32_t legacy_shadow_level;
		std::byte reserved_40[4];
		float view_cull_sq_distance;
		std::byte reserved_48[24];
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(Glow, 64);
	RML_ASSERT_SIZE(Blur, 16);
	RML_ASSERT_OFFSET(DepthOfField, near_intensity, 148);
	RML_ASSERT_OFFSET(DepthOfField, enabled, 156);
	RML_ASSERT_SIZE(DepthOfField, 160);
	RML_ASSERT_SIZE(ColorCorrection, 28);
	RML_ASSERT_OFFSET(Vignette, eye_offset, 28);
	RML_ASSERT_SIZE(Vignette, 32);
	RML_ASSERT_OFFSET(RuntimeConfigMain, legacy_shadow_level, 36);
	RML_ASSERT_OFFSET(RuntimeConfigMain, view_cull_sq_distance, 44);
	RML_ASSERT_SIZE(RuntimeConfigMain, 72);
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
		float reserved_56;
		std::uint32_t reserved_60;
		std::unique_ptr<CullableScene> cullable_scene;
		RSL::Thread cull_job_thread;
		RSL::Condition cull_job_fence;
		std::atomic<std::uint32_t> cull_job_state;
		std::byte reserved_92[28];
		std::vector<CullableSceneNode*> render_nodes;
		std::unique_ptr<RenderQueue> render_queue;
		std::unique_ptr<RenderQueue> capture_render_queue;
		std::byte reserved_160[16];
		std::unique_ptr<RenderQueue> render_queues_176[8];
		std::unique_ptr<RenderQueue> performance_overlay_render_queue;
		std::byte reserved_248[32];
		std::unique_ptr<RenderQueue> render_queues_280[5];
		dynamic_bitset cull_visibility_320;
		std::uint64_t reserved_344;
		std::uint32_t reserved_352;
		std::uint32_t reserved_356;
		dynamic_bitset cull_visibility_360;
		std::uint64_t reserved_384;
		std::uint32_t reserved_392;
		std::uint32_t reserved_396;
		std::unique_ptr<MainView> main_view;
		std::unique_ptr<EnvMapView> env_map_view;
		std::byte reserved_416[16];
		bool forced_bloom;
		bool wireframe_rendering;
		bool clouds_enabled;
		bool sky_enabled;
		bool high_dpi_framebuffer_halved;
		bool deterministic_capture_full_resolution;
		bool reserved_438;
		bool reserved_439;
		std::int32_t sky_mode;
		Color4 clear_color;
		Color4 clear_color_2d;
		bool reduced_motion_enabled;
		std::byte reserved_477[3];
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
		std::byte reserved_1584[8];
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
		std::byte reserved_1720[8];
		std::unique_ptr<DispatchScene> dispatch_scene;
		std::unique_ptr<DataModelBinding::FetcherFrontend> fetcher_frontend;
		std::unique_ptr<ShadowMap> shadow_maps[3];
		std::byte reserved_1768[24];
		TextureRef shadow_mask;
		TextureRef shadow_map_texture;
		TextureRef ssao_texture;
		std::unique_ptr<WatermarkRenderer> watermark_renderer;
		std::byte reserved_1848[16];
		RuntimeConfigMain runtime_config;
		std::vector<std::shared_ptr<GuiClusterRT>> gui_cluster_rts;
		std::uint32_t supported_color_write_mask;
		ScenePhase phase;
		bool reserved_1965;
		std::byte reserved_1966[2];
		std::int32_t multiview_stereo;
		std::byte reserved_1972[52];
		std::int64_t reserved_2024;
		std::byte reserved_2032[32];

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
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(SceneManager, view_width, 24);
	RML_ASSERT_OFFSET(SceneManager, drs_width, 32);
	RML_ASSERT_OFFSET(SceneManager, point_of_interest, 40);
	RML_ASSERT_OFFSET(SceneManager, sq_min_part_distance, 52);
	RML_ASSERT_OFFSET(SceneManager, cullable_scene, 64);
	RML_ASSERT_OFFSET(SceneManager, cull_job_state, 88);
	RML_ASSERT_OFFSET(SceneManager, render_nodes, 120);
	RML_ASSERT_OFFSET(SceneManager, render_queue, 144);
	RML_ASSERT_OFFSET(SceneManager, performance_overlay_render_queue, 240);
	RML_ASSERT_OFFSET(SceneManager, render_queues_280, 280);
	RML_ASSERT_OFFSET(SceneManager, cull_visibility_320, 320);
	RML_ASSERT_OFFSET(SceneManager, cull_visibility_360, 360);
	RML_ASSERT_OFFSET(SceneManager, main_view, 400);
	RML_ASSERT_OFFSET(SceneManager, forced_bloom, 432);
	RML_ASSERT_OFFSET(SceneManager, sky_enabled, 435);
	RML_ASSERT_OFFSET(SceneManager, sky_mode, 440);
	RML_ASSERT_OFFSET(SceneManager, clear_color, 444);
	RML_ASSERT_OFFSET(SceneManager, clear_color_2d, 460);
	RML_ASSERT_OFFSET(SceneManager, reduced_motion_enabled, 476);
	RML_ASSERT_OFFSET(SceneManager, global_shader_data, 480);
	RML_ASSERT_OFFSET(SceneManager, fog_end, 1456);
	RML_ASSERT_OFFSET(SceneManager, exposure_compensation, 1464);
	RML_ASSERT_OFFSET(SceneManager, camera_rotation_smoothed, 1468);
	RML_ASSERT_OFFSET(SceneManager, camera_change_matrix, 1484);
	RML_ASSERT_OFFSET(SceneManager, last_camera_position, 1548);
	RML_ASSERT_OFFSET(SceneManager, last_camera_direction, 1560);
	RML_ASSERT_OFFSET(SceneManager, camera_change, 1572);
	RML_ASSERT_OFFSET(SceneManager, fullscreen_triangle, 1576);
	RML_ASSERT_OFFSET(SceneManager, sky, 1592);
	RML_ASSERT_OFFSET(SceneManager, main_render_targets, 1624);
	RML_ASSERT_OFFSET(SceneManager, glow, 1640);
	RML_ASSERT_OFFSET(SceneManager, depth_of_field, 1656);
	RML_ASSERT_OFFSET(SceneManager, env_map, 1672);
	RML_ASSERT_OFFSET(SceneManager, vignette, 1704);
	RML_ASSERT_OFFSET(SceneManager, color_grading, 1712);
	RML_ASSERT_OFFSET(SceneManager, dispatch_scene, 1728);
	RML_ASSERT_OFFSET(SceneManager, shadow_maps, 1744);
	RML_ASSERT_OFFSET(SceneManager, shadow_mask, 1792);
	RML_ASSERT_OFFSET(SceneManager, watermark_renderer, 1840);
	RML_ASSERT_OFFSET(SceneManager, runtime_config, 1864);
	RML_ASSERT_OFFSET(SceneManager, gui_cluster_rts, 1936);
	RML_ASSERT_OFFSET(SceneManager, supported_color_write_mask, 1960);
	RML_ASSERT_OFFSET(SceneManager, phase, 1964);
	RML_ASSERT_OFFSET(SceneManager, multiview_stereo, 1968);
	RML_ASSERT_OFFSET(SceneManager, reserved_2024, 2024);
	RML_ASSERT_SIZE(SceneManager, 2064);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
