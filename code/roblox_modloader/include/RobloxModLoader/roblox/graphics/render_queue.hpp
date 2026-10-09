#pragma once

#include "RobloxModLoader/roblox/graphics/buffer.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace RBX::Graphics
{
	class DeviceContext;
	class Technique;
	class ViewContext;
	struct RenderableStats;
	struct RenderOperation;

	class Renderable
	{
	public:
		virtual ~Renderable() = default;
		virtual void upload_buffer_data(DeviceContext* context, const ViewContext& view, const RenderOperation* operations, std::size_t count, RenderableStats& stats) const
		{
		}
		virtual void render(DeviceContext* context, const ViewContext& view, const RenderOperation* operations, std::size_t count, RenderableStats& stats) const = 0;
		virtual bool get_instance_index_buffer(const ViewContext& view, const RenderOperation* operations, std::size_t count, std::vector<unsigned>& indices) const
		{
			return false;
		}
		virtual void set_instance_index_buffer_offset_count(unsigned offset, unsigned count) const
		{
		}
	};

	struct RenderOperation
	{
		const Renderable* renderable;
		const Technique* technique;
		const GeometryBatch* geometry;
		float distance_key;
		float radius;
		union
		{
			std::int32_t z_index;
			std::uint32_t distance_bucket;
		};

	private:
		std::uint8_t reserved_24{0};
		std::uint8_t reserved_25{3};
		std::uint8_t reserved_26{5};
		std::uint8_t reserved_27{4};

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(RenderOperation, reserved_24, 0x24);
		RML_LAYOUT_GUARD_END()

	public:
		static RenderOperation make(const Renderable* renderable, const Technique* technique, const GeometryBatch* geometry, const float distance_key = 0.0f, const float radius = 0.0f, const std::int32_t z_index = 0)
		{
			RenderOperation op;
			op.renderable = renderable;
			op.technique = technique;
			op.geometry = geometry;
			op.distance_key = distance_key;
			op.radius = radius;
			op.z_index = z_index;
			return op;
		}

		bool clip_at_distance(const float distance, const bool near_side) const
		{
			if (near_side)
				return distance > std::max(radius + radius, distance_key) + radius;
			return distance_key - radius > distance;
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(RenderOperation, geometry, 16);
	RML_ASSERT_OFFSET(RenderOperation, distance_key, 24);
	RML_ASSERT_OFFSET(RenderOperation, z_index, 32);
	RML_ASSERT_SIZE(RenderOperation, 40);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class RenderQueueGroup
	{
	public:
		enum SortMode : int
		{
			Sort_None,
			Sort_Material,
			Sort_Distance,
			Sort_DistanceAndZIndex,
			Sort_ZIndexDistance,
			Sort_ReverseDistance,
			Sort_MaterialAndReverseDistance,
			Sort_DistanceBucketAndMaterial,
			Sort_MaterialAndIB,
			Sort_ZIndexMaterialAndIB
		};

		std::vector<RenderOperation> operations;

		void clear()
		{
			operations.clear();
		}

		void push(const RenderOperation& op)
		{
			operations.push_back(op);
		}

		const RenderOperation& operator[](const std::size_t index) const
		{
			return operations[index];
		}

		std::size_t size() const
		{
			return operations.size();
		}

		bool empty() const
		{
			return operations.empty();
		}

		const RenderOperation* begin() const
		{
			return operations.data();
		}

		const RenderOperation* end() const
		{
			return operations.data() + operations.size();
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(RenderQueueGroup, 24);
	RML_LAYOUT_DIAGNOSTIC_POP()

	struct RenderQueueStats
	{
		std::uint32_t cullable_scene_node;
		std::uint32_t beam_node;
		std::uint32_t custom_emitter;
		std::uint32_t explosion_emitter;
		std::uint32_t particle_emitter;
		std::uint32_t fast_cluster;
		std::uint32_t gui_cluster;
		std::uint32_t trail_node;
		std::uint32_t render_node;
		std::uint32_t smooth_cluster;
	};

	struct MipLodConsts
	{
	private:
		std::uint32_t reserved_0;
		float reserved_4[8];
		float reserved_24;
		float reserved_28[4];

	public:
		std::uint8_t marker_view;

		void clear()
		{
			reserved_0 = 0;
			reserved_24 = 0.0f;
		}

	private:
		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(MipLodConsts, reserved_24, 0x24);
		RML_ASSERT_OFFSET(MipLodConsts, reserved_28, 0x28);
		RML_LAYOUT_GUARD_END()
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(RenderQueueStats, 0x28);
	RML_ASSERT_OFFSET(MipLodConsts, marker_view, 0x38);
	RML_ASSERT_SIZE(MipLodConsts, 0x3C);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class RenderQueue
	{
	public:
		enum Pass : int
		{
			Pass_Default,
			Pass_GlassTint,
			Pass_Shadows,
			Pass_Depth,
			Pass_DepthVT,
			Pass_CastShadowsPancaked,
			Pass_CastShadows,
			Pass_Backfaces,
			Pass_EnvMap,
			Pass_HighlightPostProcess,
			Pass_HighlightInPlace,
			Pass_HighlightMobileDepthTestMarker,
			Pass_HighlightMobileIds,
			Pass_PerformanceOverlay,
			Pass_StudioSelection,
			Pass_StudioSelectionHover,
			Pass_StudioSelectionActive,
			Pass_StudioSelectionTeamCreate,
			Pass_StudioSelectionNegated,
			Pass_TerrainWithVT,
			Pass_TerrainVTFeedback,
			Pass_Count
		};

		enum Id : int
		{
			Id_Opaque,
			Id_Terrain,
			Id_Decals,
			Id_OpaqueCasters,
			Id_OpaqueAdorns,
			Id_OpaqueWithAlpha,
			Id_Water,
			Id_GlassTint,
			Id_Glass,
			Id_Transparent,
			Id_TransparentCasters,
			Id_OnTopWithDepth,
			Id_OnTopReadOnlyDepth,
			Id_AlwaysOnTop,
			Id_AlwaysOnTopRobloxGui,
			Id_AlwaysOnTopAdorns,
			Id_Screen,
			Id_ScreenOnTopOfBlur,
			Id_Count
		};

		enum Features : unsigned
		{
			Features_Glow = 1 << 0
		};

		static constexpr const char* pass_names[Pass_Count] = {
		    "Pass_Default",
		    "Pass_GlassTint",
		    "Pass_Shadows",
		    "Pass_Depth",
		    "Pass_DepthVT",
		    "Pass_CastShadowsPancaked",
		    "Pass_CastShadows",
		    "Pass_Backfaces",
		    "Pass_EnvMap",
		    "Pass_HighlightPostProcess",
		    "Pass_HighlightInPlace",
		    "Pass_HighlightMobileDepthTestMarker",
		    "Pass_HighlightMobileIds",
		    "Pass_PerformanceOverlay",
		    "Pass_StudioSelection",
		    "Pass_StudioSelectionHover",
		    "Pass_StudioSelectionActive",
		    "Pass_StudioSelectionTeamCreate",
		    "Pass_StudioSelectionNegated",
		    "Pass_TerrainWithVT",
		    "Pass_TerrainVTFeedback",
		};

		static constexpr const char* id_names[Id_Count] = {
		    "Id_Opaque",
		    "Id_Terrain",
		    "Id_Decals",
		    "Id_OpaqueCasters",
		    "Id_OpaqueAdorns",
		    "Id_OpaqueWithAlpha",
		    "Id_Water",
		    "Id_GlassTint",
		    "Id_Glass",
		    "Id_Transparent",
		    "Id_TransparentCasters",
		    "Id_OnTopWithDepth",
		    "Id_OnTopReadOnlyDepth",
		    "Id_AlwaysOnTop",
		    "Id_AlwaysOnTopRobloxGui",
		    "Id_AlwaysOnTopAdorns",
		    "Id_Screen",
		    "Id_ScreenOnTopOfBlur",
		};

		RenderQueueStats stats;
		MipLodConsts mip_lod_consts;

	private:
		const void* reserved_68;

	public:
		RenderQueueGroup groups[Id_Count];
		unsigned features;

	private:
		std::uint32_t reserved_224;

	public:
		RenderQueueGroup& get_group(const Id id)
		{
			return groups[id];
		}

		const RenderQueueGroup& get_group(const Id id) const
		{
			return groups[id];
		}

		unsigned get_features() const
		{
			return features;
		}

		void set_feature(const unsigned feature)
		{
			features |= feature;
		}

		void clear()
		{
			for (auto& group : groups)
				group.clear();
			reserved_68 = nullptr;
			features = 0;
			reserved_224 = 0xFFFFFFFFu;
			mip_lod_consts.clear();
		}

	private:
		RenderQueue() = delete;

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(RenderQueue, reserved_68, 0x68);
		RML_ASSERT_OFFSET(RenderQueue, reserved_224, 0x224);
		RML_LAYOUT_GUARD_END()
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(RenderQueue, mip_lod_consts, 0x28);
	RML_ASSERT_OFFSET(RenderQueue, groups, 0x70);
	RML_ASSERT_OFFSET(RenderQueue, features, 0x220);
	RML_ASSERT_SIZE(RenderQueue, 0x228);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
