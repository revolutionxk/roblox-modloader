#pragma once

#include "RobloxModLoader/roblox/signals.hpp"
#include "RobloxModLoader/roblox/util/Extents.h"
#include "RobloxModLoader/roblox/util/G3DCore.h"
#include "RobloxModLoader/roblox/util/Rotation2d.h"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace RSL
{
	template<typename T>
	struct ArrayView
	{
		const T* data;
		std::size_t size;
	};
}

namespace RBX
{
	class Camera;
	class ContentId;
	class Content;
	class InstanceIdentifier;
	class TextureProxyBase;
	class GfxGui;
	class ViewportFrame;
	class EditableImage;
	class VideoSinkBase;
	class RenderTargetConfiguration;
	class GlyphData;
	class GuiEffect;
	class TextLayout;
	class I3DLinearFunc;

	namespace DefinitiveText
	{
		class FontSubsystemHandle;
	}

	namespace Graphics
	{
		class GeometryBatch;
	}

	using TextureProxyBaseRef = std::shared_ptr<TextureProxyBase>;

	struct Vector2Value
	{
		float x;
		float y;

		Vector2 to_vector2() const
		{
			return Vector2(x, y);
		}

		static Vector2Value from(const Vector2& v)
		{
			return {v.x, v.y};
		}
	};

	struct Rect2DValue
	{
		Vector2Value min;
		Vector2Value max;

		float width() const
		{
			return max.x - min.x;
		}

		float height() const
		{
			return max.y - min.y;
		}

		Rect2D to_rect2d() const
		{
			return Rect2D::xyxy(min.x, min.y, max.x, max.y);
		}
	};

	static_assert(std::is_trivially_copyable_v<Vector2Value> && sizeof(Vector2Value) == 8);
	static_assert(std::is_trivially_copyable_v<Rect2DValue> && sizeof(Rect2DValue) == 16);

	class Adorn
	{
	public:
		enum Material : int
		{
			Material_Default,
			Material_NoLighting,
			Material_SelfLit,
			Material_SelfLitHighlight,
			Material_FixedWidthAALine,
			Material_AALine,
			Material_Outline,
			Material_OnTop,
			Material_Object,
			Material_ObjectRope,
			Material_ObjectSpring,
			Material_ObjectParabola,
			Material_CvxDebug,
			Material_Count
		};

		enum Pass : int
		{
			Pass_Opaque,
			Pass_Default,
			Pass_Transparent,
			Pass_DepthWrite,
			Pass_Blend,
			Pass_AlwaysOnTop,
			Pass_Composite,
			Pass_Count
		};

		enum Mesh : int
		{
			Mesh_Box,
			Mesh_Sphere,
			Mesh_Cylinder,
			Mesh_Segmented
		};

		static constexpr int maximum_z_index = 10;

		virtual const Camera* get_camera() const = 0;
		virtual ~Adorn() = default;
		virtual TextureProxyBaseRef create_texture_proxy(const ContentId& id, bool& waiting, const InstanceIdentifier& owner, bool blocking) = 0;
		virtual TextureProxyBaseRef create_texture_proxy(ViewportFrame* frame) = 0;
		virtual TextureProxyBaseRef create_texture_proxy(std::shared_ptr<EditableImage> image) = 0;
		virtual TextureProxyBaseRef create_texture_proxy(const Content& content) = 0;
		virtual TextureProxyBaseRef create_texture_proxy(std::weak_ptr<VideoSinkBase> sink) = 0;
		virtual TextureProxyBaseRef create_future_texture_proxy(const ContentId& id, bool& waiting, const InstanceIdentifier& owner, bool blocking) = 0;
		virtual void garbage_collect_incremental() = 0;
		virtual void request_or_update_ui_rt(const std::shared_ptr<GfxGui>& gui, const RenderTargetConfiguration& configuration) = 0;
		virtual rbx::signal<void()>& get_unbind_resources_signal() = 0;
		virtual void prepare_render_pass() = 0;
		virtual void finish_render_pass() = 0;
		virtual void pre_submit_pass() = 0;
		virtual void post_submit_pass() = 0;
		virtual void begin_screen_space_batching(bool enabled) = 0;
		virtual void end_screen_space_batching() = 0;
		virtual CoordinateFrame get_coordinate_frame() const = 0;
		virtual Rect2DValue get_viewport() const = 0;
		virtual void set_texture_impl(const TextureProxyBaseRef& texture, std::uint8_t flags) = 0;
		virtual void set_render_target_impl(void* target, std::uint8_t flags) = 0;
		virtual Rect2DValue get_texture_size(const TextureProxyBaseRef& texture) const = 0;
		virtual void line2d(const Vector2& p0, const Vector2& p1, const Color4& color) = 0;
		virtual void glyph2d_impl(Adorn* adorn, Vector2Value position, const Color4& color, const Rotation2D& rotation, const Rect2D& clip, const GlyphData& glyph, GuiEffect* effect) = 0;
		virtual void glyph2d_impl(Adorn* adorn, Vector2Value position, const Color4& color, const Rotation2D& rotation, const Rect2D& clip, const GlyphData& glyph, GuiEffect* effect, DefinitiveText::FontSubsystemHandle& fonts) = 0;
		virtual void rect2d_impl(const Rect2D& rect, const Rect2D& clip, const Vector2& tex0, const Vector2& tex1, const Color4& color, const Rotation2D& rotation) = 0;
		virtual void line3d(const Vector3& p0, const Vector3& p1, const Color4& color, int z_index, bool always_on_top) = 0;
		virtual void line3d_aa(const Vector3& p0, const Vector3& p1, const Color4& color, float thickness, int z_index, bool always_on_top, bool fixed_width) = 0;
		virtual void polyline3d(const Vector3* points, const Color4* colors, int count) = 0;
		virtual void set_object_to_world_matrix(const CoordinateFrame& cframe) = 0;
		virtual void box(const AABox& box, const Color4& color) = 0;
		virtual void box(const CoordinateFrame& cframe, const Vector3& size, const Color4& color, int z_index, Pass pass) = 0;
		virtual void sphere(const Sphere& sphere, const Color4& color) = 0;
		virtual void sphere(const CoordinateFrame& cframe, float radius, const Color4& color, int z_index, Pass pass) = 0;
		virtual void spherical_annulus(const CoordinateFrame& cframe, float radius, const Color4& color, int z_index, Pass pass) = 0;
		virtual void explosion(const Sphere& sphere) = 0;
		virtual void cylinder(const CoordinateFrame& cframe, float radius, float length, const Color4& color, int z_index, Pass pass) = 0;
		virtual void complex_cylinder(const CoordinateFrame& cframe, float radius, float length, float inner_radius, float angle, const Color4& color, int z_index, Pass pass) = 0;
		virtual void cylinder_along_x(float radius, float length, const Color4& color, bool cap) = 0;
		virtual void tapered_capsule(const CoordinateFrame& cframe, float radius0, float radius1, float length, const Color4& color, int z_index, Pass pass) = 0;
		virtual void cone(const CoordinateFrame& cframe, float radius, float height, bool cap, const Color4& color, int z_index, Pass pass) = 0;
		virtual void pyramid(const CoordinateFrame& cframe, float width, float height, int sides, const Color4& color, int z_index, Pass pass) = 0;
		virtual void ray(const Ray& ray, const Color4& color) = 0;
		virtual void axes(const Color4& x_color, const Color4& y_color, const Color4& z_color, float scale) = 0;
		virtual void quad(bool textured, const CoordinateFrame& cframe, const Vector2& size, const Color4& color, int z_index, bool always_on_top) = 0;
		virtual void convex_polygon(const Vector3* vertices, int count, const Color4& color) = 0;
		virtual void convex_polyhedron(const Vector3* vertices, int count, const Color4& color) = 0;
		virtual void extrusion(I3DLinearFunc* trajectory, int trajectory_segments, I3DLinearFunc* profile, int profile_segments, const Color4& color, bool close_trajectory, bool close_profile, Pass pass) = 0;
		virtual void triangle_list(const Color4& color, const Vector3* vertices, int vertex_count, const short* indices, int index_count, bool always_on_top, int z_index, bool wireframe) = 0;
		virtual std::unique_ptr<Graphics::GeometryBatch, void (*)(Graphics::GeometryBatch*)> make_custom_batch(RSL::ArrayView<Vector3> vertices, RSL::ArrayView<std::uint16_t> indices, std::optional<RSL::ArrayView<Vector3>> normals, std::optional<RSL::ArrayView<Vector2>> uvs) = 0;
		virtual void draw_custom_batch(const Graphics::GeometryBatch& batch, Material material, const Vector3& translation, const Matrix3& rotation, const Vector3& scale, const Color4& color, const Sphere& bounds, const Vector4& extra, int z_index, Pass pass) = 0;
		virtual void object(const CoordinateFrame& cframe, const Vector3& scale, const Sphere& bounds, const Color4& color, const Vector4& extra, Mesh mesh, int segments, Material material, Pass pass, int z_index) = 0;
		virtual void curved_surface(const CoordinateFrame& cframe, float radius, const Color4& color, float angle, float length, bool always_on_top) = 0;
		virtual bool is_visible(const Extents& extents, const CoordinateFrame& cframe) const = 0;
		virtual Vector2Value draw_font2d_impl(Adorn* adorn, TextLayout* layout) = 0;

		Vector4 user_gui_inset;
		bool vr;
		bool ignore_texture;
		std::uint16_t reserved_26;
		Material current_material;
		std::int32_t no_distance_culling;
		std::uint32_t reserved_36;
		std::shared_ptr<GfxGui> current_gui;
		std::vector<std::shared_ptr<GfxGui>> gui_list;
		std::unordered_map<void*, std::shared_ptr<GfxGui>> gui_map;
		void* scratch_begin;
		void* scratch_end;
		void* scratch_capacity;

		void set_material(const Material material)
		{
			current_material = material;
		}

		Material get_material() const
		{
			return current_material;
		}

		void set_user_gui_inset(const Vector4& value)
		{
			user_gui_inset = value;
		}

		bool is_vr() const
		{
			return vr;
		}

		void set_ignore_texture(const bool ignore)
		{
			ignore_texture = ignore;
		}

		bool get_ignore_texture() const
		{
			return ignore_texture;
		}

		void set_texture(const TextureProxyBaseRef& texture)
		{
			set_texture_impl(texture, 0);
		}

		void box(const Extents& extents, const Color4& color)
		{
			box(AABox(extents.min(), extents.max()), color);
		}

	protected:
		Adorn() = default;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Adorn, user_gui_inset, 8);
	RML_ASSERT_OFFSET(Adorn, vr, 24);
	RML_ASSERT_OFFSET(Adorn, current_material, 28);
	RML_ASSERT_OFFSET(Adorn, current_gui, 40);
	RML_ASSERT_OFFSET(Adorn, gui_list, 56);
	RML_ASSERT_OFFSET(Adorn, gui_map, 80);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(Adorn, scratch_begin, 0x90);
	RML_ASSERT_SIZE(Adorn, 0xa8);
#else
	RML_ASSERT_OFFSET(Adorn, scratch_begin, 120);
	RML_ASSERT_SIZE(Adorn, 144);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
