#pragma once

#include "RobloxModLoader/roblox/graphics/texture.hpp"
#include "RobloxModLoader/roblox/graphics/texture_ref.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <memory>

namespace RBX::Graphics
{
	class VisualEngine;

	class RTPool
	{
	public:
		struct RT;

	private:
		RTPool() = delete;
	};

	class MainRenderTargets
	{
	public:
		enum class Mode : std::uint32_t
		{
			NoDepth = 0,
			WithDepth = 1,
			Reset = 2,
		};

		struct RTMSAA
		{
			std::shared_ptr<Texture> main_color;
			std::shared_ptr<Texture> main_color_msaa;
			std::shared_ptr<Framebuffer> main_fb;
			std::shared_ptr<Framebuffer> main_fb_msaa;
		};

		std::unique_ptr<RTMSAA> main;
		std::shared_ptr<Texture> main_depth;
		std::shared_ptr<Texture> main_depth_resolved;
		std::shared_ptr<Texture> ui_stencil;
		std::shared_ptr<Framebuffer> ui_fb;
		std::shared_ptr<Texture> xr_motion_vectors;
		std::shared_ptr<Framebuffer> xr_motion_vectors_fb;
		std::unique_ptr<RTPool::RT> color_opaque_rt;
		std::unique_ptr<RTPool::RT> depth_opaque_rt;
		std::shared_ptr<Framebuffer> scene_fb;
		std::shared_ptr<Framebuffer> main_depth_resolve_fb;
		std::shared_ptr<Texture> highlight_outline;
		std::shared_ptr<Texture> highlight_outline_depth;
		std::shared_ptr<Framebuffer> highlight_outline_fb;
		std::shared_ptr<Texture> studio_selection_depth_source;
		std::shared_ptr<Framebuffer> studio_selection_depth_source_fb;
		std::shared_ptr<Texture> studio_selection;
		std::shared_ptr<Texture> studio_selection_depth;
		std::shared_ptr<Framebuffer> studio_selection_fb;
		std::shared_ptr<Texture> studio_selection_team_create;
		std::shared_ptr<Texture> studio_selection_hover_depth;
		std::shared_ptr<Framebuffer> studio_selection_hover_fb;
		std::shared_ptr<Texture> studio_selection_composite;
		std::shared_ptr<Framebuffer> studio_selection_composite_fb;
		std::shared_ptr<Texture> performance_overlay;
		std::shared_ptr<Framebuffer> performance_overlay_fb;
		TextureRef color_opaque;
		TextureRef depth_opaque;
		VisualEngine* visual_engine;
		Mode mode;
		std::uint32_t sample_count;
		bool memoryless_supported;
		bool reserved_441;
		bool reserved_442;

		Texture* main_color() const
		{
			return main ? main->main_color.get() : nullptr;
		}

		Framebuffer* main_framebuffer() const
		{
			if (!main)
				return nullptr;
			return sample_count > 1 ? main->main_fb_msaa.get() : main->main_fb.get();
		}

	private:
		MainRenderTargets() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(MainRenderTargets::RTMSAA, 64);
	RML_ASSERT_OFFSET(MainRenderTargets, main_depth, 8);
	RML_ASSERT_OFFSET(MainRenderTargets, main_depth_resolved, 24);
	RML_ASSERT_OFFSET(MainRenderTargets, ui_stencil, 40);
	RML_ASSERT_OFFSET(MainRenderTargets, xr_motion_vectors, 72);
	RML_ASSERT_OFFSET(MainRenderTargets, color_opaque_rt, 104);
	RML_ASSERT_OFFSET(MainRenderTargets, scene_fb, 120);
	RML_ASSERT_OFFSET(MainRenderTargets, main_depth_resolve_fb, 136);
	RML_ASSERT_OFFSET(MainRenderTargets, highlight_outline, 152);
	RML_ASSERT_OFFSET(MainRenderTargets, studio_selection_depth_source, 200);
	RML_ASSERT_OFFSET(MainRenderTargets, studio_selection, 232);
	RML_ASSERT_OFFSET(MainRenderTargets, studio_selection_composite, 328);
	RML_ASSERT_OFFSET(MainRenderTargets, performance_overlay, 360);
	RML_ASSERT_OFFSET(MainRenderTargets, color_opaque, 392);
	RML_ASSERT_OFFSET(MainRenderTargets, visual_engine, 424);
	RML_ASSERT_OFFSET(MainRenderTargets, sample_count, 436);
	RML_ASSERT_OFFSET(MainRenderTargets, memoryless_supported, 440);
	RML_ASSERT_SIZE(MainRenderTargets, 448);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
