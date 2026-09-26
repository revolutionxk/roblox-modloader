#include "RobloxModLoader/hooking/vtable_index.hpp"
#include "RobloxModLoader/roblox/adorn.hpp"
#include "RobloxModLoader/roblox/graphics/adorn_render.hpp"
#include "RobloxModLoader/roblox/graphics/device.hpp"
#include "RobloxModLoader/roblox/graphics/device_context.hpp"
#include "RobloxModLoader/roblox/graphics/global_shader_data.hpp"
#include "RobloxModLoader/roblox/graphics/material.hpp"
#include "RobloxModLoader/roblox/graphics/render_camera.hpp"
#include "RobloxModLoader/roblox/graphics/render_queue.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "RobloxModLoader/roblox/graphics/texture_ref.hpp"

#include <cstddef>
#include <doctest/doctest.h>
#include <string>
#include <type_traits>
#include <vector>

TEST_CASE("graphics interfaces are pure and keep the dumped slot order")
{
	using namespace RBX::Graphics;
	static_assert(std::is_abstract_v<Device>);
	static_assert(std::is_abstract_v<DeviceContext>);
	static_assert(sizeof(Device) == sizeof(void*));
	static_assert(sizeof(DeviceContext) == sizeof(void*));
	static_assert(static_cast<int>(Texture::Format::RGBA8) == 4);
	static_assert(static_cast<int>(Texture::Format::BGRA8) == 5);
	static_assert(static_cast<int>(Texture::Format::D24S8) == 30);
	static_assert(static_cast<int>(Texture::Format::Count) == 40);

#if !defined(_MSC_VER)
	const std::vector<char> bytecode;
	const std::string name;
	const RasterizerState rasterizer{};
	const BlendState blend{};
	const DepthState depth{};
	CHECK(rml::vtable_index_of(&Device::create_shader, Shader::Type::Vertex, bytecode, name) == 24);
	CHECK(rml::vtable_index_of(&Device::create_texture_impl, Texture::Type::Type_2D, Texture::Format::RGBA8, 0u, 0u, 0u, 0u, 0u, 0u, Texture::Usage::Static, name) == 49);
	CHECK(rml::vtable_index_of(&DeviceContext::set_render_state, rasterizer, blend, depth) == 26);
	CHECK(rml::vtable_index_of(&DeviceContext::end_sync_profiler_scope_unmapped, std::uint64_t{}, nullptr, nullptr) == 39);
	CHECK(rml::vtable_index_of(&Texture::reduce_mip_levels, 0u) == 13);
	CHECK(rml::vtable_index_of(&Buffer::download_debug, 0u, nullptr, 0u) == 7);
	CHECK(rml::vtable_index_of(&Shader::reload, bytecode) == 4);
#endif
}

TEST_CASE("scene mirrors keep the measured layout")
{
	using namespace RBX::Graphics;
	static_assert(sizeof(RenderCamera) == 832);
	static_assert(alignof(RenderCamera) == 16);
	static_assert(sizeof(RBX::Frustum_SIMD) == 320);
	static_assert(sizeof(GlobalShaderData) == 976);
	static_assert(sizeof(MainRenderTargets) == 448);
	static_assert(sizeof(SceneManager) == 2064);
	static_assert(offsetof(SceneManager, global_shader_data) == 480);
	static_assert(offsetof(SceneManager, main_render_targets) == 1624);
	CHECK(static_cast<int>(ScenePhase::Render) == 2);
	CHECK(static_cast<int>(PreRotate::Rotate270) == 3);
}

TEST_CASE("adorn interface keeps the dumped slot order")
{
	using namespace RBX;
#if defined(_WIN32)
	// Adorn carries a std::unordered_map (adorn.hpp:205), which is 64 bytes on MSVC against
	// libc++'s 40; AdornRender grows for the same reason.
	static_assert(sizeof(Adorn) == 168);
	static_assert(sizeof(Graphics::AdornRender) == 2776);
#else
	static_assert(sizeof(Adorn) == 144);
	static_assert(sizeof(Graphics::AdornRender) == 2744);
#endif
	static_assert(sizeof(Graphics::GeometryBatch) == 40);
	static_assert(Adorn::Material_Count == 13);
	static_assert(Adorn::Pass_Count == 7);

#if !defined(_MSC_VER)
	const Color4* color = nullptr;
	const CoordinateFrame* cframe = nullptr;
	const Vector3* vector = nullptr;
	CHECK(rml::vtable_index_of(&Adorn::get_camera) == 0);
	CHECK(rml::vtable_index_of(&Adorn::prepare_render_pass) == 12);
	CHECK(rml::vtable_index_of(&Adorn::pre_submit_pass) == 14);
	CHECK(rml::vtable_index_of(&Adorn::post_submit_pass) == 15);
	CHECK(rml::vtable_index_of(&Adorn::get_viewport) == 19);
	CHECK(rml::vtable_index_of(&Adorn::line3d, *vector, *vector, *color, 0, false) == 27);
	CHECK(rml::vtable_index_of(&Adorn::set_object_to_world_matrix, *cframe) == 30);
	CHECK(rml::vtable_index_of(&Adorn::explosion, *static_cast<const Sphere*>(nullptr)) == 36);
	CHECK(rml::vtable_index_of(&Adorn::ray, *static_cast<const Ray*>(nullptr), *color) == 43);
	CHECK(rml::vtable_index_of(&Adorn::draw_font2d_impl, nullptr, nullptr) == 55);
#endif
}

TEST_CASE("render queue, technique and texture ref mirrors keep the measured layout")
{
	using namespace RBX::Graphics;
	static_assert(sizeof(RenderOperation) == 40);
	static_assert(sizeof(RenderQueueGroup) == 24);
	static_assert(sizeof(RenderQueue) == 552);
	static_assert(offsetof(RenderQueue, groups) == 112);
	static_assert(offsetof(RenderQueue, features) == 544);
	static_assert(RenderQueue::Id_Count == 18);
	static_assert(RenderQueue::Pass_Count == 21);
	static_assert(RenderQueue::Id_AlwaysOnTopAdorns == 15);
	static_assert(RenderQueue::Id_ScreenOnTopOfBlur == 17);
	static_assert(RenderQueue::Pass_Backfaces == 7);
	static_assert(sizeof(Technique) == 136);
	static_assert(offsetof(Technique, program) == 40);
	static_assert(offsetof(Technique, textures) == 64);
	static_assert(sizeof(Material) == 40);
#if defined(_WIN32)
	static_assert(sizeof(ShaderProgram) == 120);
#else
	static_assert(sizeof(ShaderProgram) == 112);
#endif
	static_assert(sizeof(ImageInfo) == 88);
	static_assert(sizeof(TextureRefData) == 152);
	static_assert(offsetof(TextureRefData, status) == 144);
	static_assert(sizeof(TextureRef) == 16);
	static_assert(std::is_abstract_v<Renderable>);
	static_assert(sizeof(AdornMesh) == 136);
	static_assert(std::is_base_of_v<Renderable, AdornMesh>);

	const RenderOperation op = RenderOperation::make(nullptr, nullptr, nullptr, 10.0f, 2.0f, 3);
	CHECK(op.reserved_37 == 3);
	CHECK(op.reserved_38 == 5);
	CHECK(op.reserved_39 == 4);
	CHECK(op.clip_at_distance(7.0f, false));
	CHECK_FALSE(op.clip_at_distance(9.0f, false));
	CHECK(op.clip_at_distance(13.0f, true));
	CHECK_FALSE(op.clip_at_distance(11.0f, true));

	const BlendState opaque = BlendState::opaque();
	CHECK_FALSE(opaque.blending_needed());
	CHECK(BlendState::alpha_blend().blending_needed());
	CHECK(BlendState::make(BlendState::Factor_Zero, BlendState::Factor_One).blending_needed());
	CHECK(AdornRender::queue_id_for(RBX::Adorn::Pass_Default) == RenderQueue::Id_OpaqueAdorns);
	CHECK(AdornRender::queue_id_for(RBX::Adorn::Pass_Composite) == RenderQueue::Id_Count);

#if !defined(_MSC_VER)
	const ViewContext* view = nullptr;
	RenderableStats* stats = nullptr;
	CHECK(rml::vtable_index_of(&Renderable::render, nullptr, *view, nullptr, std::size_t{}, *stats) == 3);
	CHECK(rml::vtable_index_of(&Renderable::set_instance_index_buffer_offset_count, 0u, 0u) == 5);
#endif
}
