#pragma once

#include "RobloxModLoader/roblox/adorn.hpp"
#include "RobloxModLoader/roblox/graphics/buffer.hpp"
#include "RobloxModLoader/roblox/graphics/material.hpp"
#include "RobloxModLoader/roblox/graphics/render_queue.hpp"
#include "RobloxModLoader/roblox/graphics/shader.hpp"
#include "RobloxModLoader/roblox/graphics/texture.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace RBX
{
	class DataModel;
}

namespace RBX::Graphics
{
	class VisualEngine;
	class VertexStreamerMigrationLayer;

	struct AdornVertex
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;
	};

	struct AdornMesh : Renderable
	{
		Adorn::Material material;
		std::uint32_t reserved_12;
		const GeometryBatch* batch;
		float view_depth;
		float radius;
		Vector4 extra;
		std::int32_t z_index;
		Color4 color;
		Matrix3 rotation;
		Vector3 translation;
		float reserved_116;
		std::shared_ptr<Texture> texture;

	private:
		AdornMesh() = delete;
	};

	class AdornRender : public Adorn
	{
	public:
		static constexpr std::size_t k_unit_batches = 58;

		VisualEngine* visual_engine;
		const DataModel* data_model;
		union {
			std::string context;
		};
		VertexStreamerMigrationLayer* vertex_streamer;
		union {
			rbx::signal<void()> unbind_resources;
		};
		float viewport_height_at_submit;
		CoordinateFrame object_to_world;
		std::uint32_t reserved_252;
		std::shared_ptr<Texture> current_texture;
		std::shared_ptr<VertexLayout> vertex_layout;
		bool texture_dirty;
		std::uint8_t texture_flags;
		std::uint16_t reserved_290;
		float viewport_width;
		float viewport_height;
		std::byte reserved_300[20];
		union {
			std::vector<AdornMesh> meshes[Adorn::Pass_Count];
		};
		std::unique_ptr<GeometryBatch> unit_batches[k_unit_batches];
		std::byte custom_batch_cache[80];
		std::shared_ptr<Technique> techniques[Adorn::Pass_Count][Adorn::Material_Count];
		std::shared_ptr<ShaderProgram> programs[Adorn::Material_Count];
		void* font_subsystem;
		std::byte texture_cache[40];

		VertexStreamerMigrationLayer* get_vertex_streamer() const
		{
			return vertex_streamer;
		}

		const GeometryBatch* unit_box() const
		{
			return unit_batches[2].get();
		}

		const GeometryBatch* unit_cylinder() const
		{
			return unit_batches[3].get();
		}

		const GeometryBatch* unit_sphere() const
		{
			return unit_batches[4].get();
		}

		const GeometryBatch* unit_cone(const bool cap) const
		{
			return unit_batches[cap ? 7 : 6].get();
		}

		Technique* get_technique(const Adorn::Pass pass, const Adorn::Material material) const
		{
			return techniques[pass][material].get();
		}

		static RenderQueue::Id queue_id_for(const Adorn::Pass pass, const bool always_on_top_adorns = true)
		{
			switch (pass)
			{
				case Adorn::Pass_Opaque:
					return RenderQueue::Id_Opaque;
				case Adorn::Pass_Default:
					return RenderQueue::Id_OpaqueAdorns;
				case Adorn::Pass_Transparent:
					return RenderQueue::Id_Transparent;
				case Adorn::Pass_DepthWrite:
					return RenderQueue::Id_OnTopWithDepth;
				case Adorn::Pass_Blend:
					return RenderQueue::Id_OnTopReadOnlyDepth;
				case Adorn::Pass_AlwaysOnTop:
					return always_on_top_adorns ? RenderQueue::Id_AlwaysOnTopAdorns : RenderQueue::Id_AlwaysOnTop;
				default:
					return RenderQueue::Id_Count;
			}
		}

		~AdornRender() override
		{
		}

	private:
		AdornRender() = delete;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(AdornVertex, 32);
	RML_ASSERT_SIZE(AdornMesh, 136);
	RML_ASSERT_OFFSET(AdornMesh, batch, 16);
	RML_ASSERT_OFFSET(AdornMesh, z_index, 48);
	RML_ASSERT_OFFSET(AdornMesh, rotation, 68);
	RML_ASSERT_OFFSET(AdornMesh, texture, 120);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(AdornRender, visual_engine, 0xa8);
	RML_ASSERT_OFFSET(AdornRender, context, 0xb8);
	RML_ASSERT_OFFSET(AdornRender, vertex_streamer, 0xd8);
	RML_ASSERT_OFFSET(AdornRender, unbind_resources, 0xe0);
	RML_ASSERT_OFFSET(AdornRender, object_to_world, 0xec);
	RML_ASSERT_OFFSET(AdornRender, current_texture, 0x120);
	RML_ASSERT_OFFSET(AdornRender, vertex_layout, 0x130);
	RML_ASSERT_OFFSET(AdornRender, viewport_width, 0x144);
	RML_ASSERT_OFFSET(AdornRender, meshes, 0x160);
	RML_ASSERT_OFFSET(AdornRender, unit_batches, 0x208);
	RML_ASSERT_OFFSET(AdornRender, custom_batch_cache, 0x3d8);
	RML_ASSERT_OFFSET(AdornRender, techniques, 0x428);
	RML_ASSERT_OFFSET(AdornRender, programs, 0x9d8);
	RML_ASSERT_OFFSET(AdornRender, font_subsystem, 0xaa8);
	RML_ASSERT_SIZE(AdornRender, 0xad8);
#else
	RML_ASSERT_OFFSET(AdornRender, visual_engine, 144);
	RML_ASSERT_OFFSET(AdornRender, context, 160);
	RML_ASSERT_OFFSET(AdornRender, vertex_streamer, 184);
	RML_ASSERT_OFFSET(AdornRender, unbind_resources, 192);
	RML_ASSERT_OFFSET(AdornRender, object_to_world, 204);
	RML_ASSERT_OFFSET(AdornRender, current_texture, 256);
	RML_ASSERT_OFFSET(AdornRender, vertex_layout, 272);
	RML_ASSERT_OFFSET(AdornRender, viewport_width, 292);
	RML_ASSERT_OFFSET(AdornRender, meshes, 320);
	RML_ASSERT_OFFSET(AdornRender, unit_batches, 488);
	RML_ASSERT_OFFSET(AdornRender, custom_batch_cache, 952);
	RML_ASSERT_OFFSET(AdornRender, techniques, 1032);
	RML_ASSERT_OFFSET(AdornRender, programs, 2488);
	RML_ASSERT_OFFSET(AdornRender, font_subsystem, 2696);
	RML_ASSERT_SIZE(AdornRender, 2744);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
