#pragma once

#include "buffer.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>

namespace RBX::Graphics
{
	class DeviceContext
	{
	public:
		struct CopyTextureParams;

		virtual ~DeviceContext() = default;
		virtual void set_default_anisotropy(unsigned value) = 0;
		virtual Framebuffer* get_framebuffer() = 0;
		virtual void copy_framebuffer(Framebuffer* buffer, Texture* texture, unsigned index, unsigned mip) = 0;
		virtual void resolve_framebuffer(Framebuffer* msaa_buffer, Framebuffer* buffer, unsigned mask) = 0;
		virtual void generate_mipmaps(Texture* texture) = 0;
		virtual void begin_pass(Framebuffer* buffer, unsigned load_mask, unsigned store_mask, const PassClear* clear, const PassResolve* resolve, unsigned flags) = 0;
		virtual void end_pass() = 0;
		virtual void set_viewport(unsigned x, unsigned y, unsigned width, unsigned height) = 0;
		virtual void set_scissor(unsigned x, unsigned y, unsigned width, unsigned height) = 0;
		virtual void bind_program(ShaderProgram* program) = 0;
		virtual void bind_buffer(unsigned slot, Buffer* buffer) = 0;
		virtual void bind_buffer_rw(unsigned slot, Buffer* buffer) = 0;
		virtual void begin_buffer_data() = 0;
		virtual void upload_buffer_data(const void* data, unsigned size) = 0;
		virtual void upload_buffer_data(std::size_t count, const void** data, std::size_t size) = 0;
		virtual void end_buffer_data() = 0;
		virtual void bind_buffer_data(unsigned slot, const void* data, unsigned size) = 0;
		virtual void bind_buffer_data(unsigned slot, const ConstantBuffer& buffer) = 0;
		virtual void bind_texture(unsigned stage, Texture* texture, const SamplerState& state) = 0;
		virtual void bind_texture_rw(unsigned stage, Texture* texture, unsigned mip) = 0;
		virtual void bind_buffer_inst(unsigned slot, Buffer* buffer, unsigned offset) = 0;
		virtual void upload_instance_index_buffer(unsigned* indices, std::size_t count) = 0;
		virtual void bind_instance_data_buffer(unsigned slot, const void* data, unsigned size, const unsigned* indices, std::size_t count) = 0;
		virtual void copy_texture(const CopyTextureParams& params) = 0;
		virtual void set_render_state(const RasterizerState& rasterizer, const BlendState& blend, const DepthState& depth) = 0;
		virtual void set_stencil_reference(std::uint8_t reference) = 0;
		virtual void draw(Geometry* geometry, Geometry::Primitive primitive, unsigned offset, unsigned base_vertex, unsigned count, unsigned instances, unsigned base_instance) = 0;
		virtual void draw(const GeometryBatch& batch, unsigned offset, unsigned count) = 0;
		virtual void begin_compute() = 0;
		virtual void end_compute() = 0;
		virtual int begin_occlusion_query() = 0;
		virtual void end_occlusion_query(int query) = 0;
		virtual bool poll_occlusion_query(int query, std::uint64_t* samples) = 0;
		virtual void dispatch(unsigned x, unsigned y, unsigned z) = 0;
		virtual void begin_group(const char* name, int color) = 0;
		virtual void end_group() = 0;
		virtual void begin_sync_profiler_scope(std::uint64_t token, Profiler::ActiveRegion* region, const char* name) = 0;
		virtual void end_sync_profiler_scope(std::uint64_t token, Profiler::ActiveRegion* region, const char* name) = 0;
	};
}
