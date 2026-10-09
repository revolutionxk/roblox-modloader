#pragma once

#include "buffer.hpp"
#include "device_context.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "types.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace RBX::Graphics
{
	class Device
	{
	public:
		struct Statistics
		{
		private:
			[[maybe_unused]] float reserved_0;
			[[maybe_unused]] bool reserved_4;

		public:
			double display_interval_ms;
			double vsync_gap_ms;
			double frame_target_ms;
			double screen_max_fps;

		private:
			[[maybe_unused]] double reserved_28;

		public:
			std::uint32_t frames_within_target;
			std::uint32_t frames_missed;
			bool vsync_stats_valid;

		private:
			RML_LAYOUT_GUARD_BEGIN()
			RML_ASSERT_OFFSET(Statistics, reserved_4, 0x4);
			RML_ASSERT_OFFSET(Statistics, reserved_28, 0x28);
			RML_LAYOUT_GUARD_END()
		};

		struct FrameTimingDetails
		{
			double target_fps;
		};

		struct VideoMemoryInfo
		{
			std::uint64_t allocated_mb;
			std::uint64_t budget_mb;
			std::uint64_t available_mb;
		};

	public:
		virtual ~Device() = default;
		virtual void add_window(void* handle) = 0;
		virtual void add_transparent_window(void* handle) = 0;
		virtual void add_video_capture_window(void* handle) = 0;
		virtual void remove_window(void* handle) = 0;
		virtual Framebuffer* get_framebuffer_for_window(void* handle) = 0;
		virtual unsigned get_video_capture_window_image_index(void* handle) = 0;
		virtual void set_video_capture_window_present_index(void* handle, unsigned index) = 0;
		virtual bool validate() = 0;
		virtual bool is_device_lost() const = 0;
		virtual int get_device_loss_reason() const = 0;
		virtual void fill_recovery_telemetry_params(DeviceRecoveryTelemetryParams& params) = 0;
		virtual DeviceContext* begin_frame() = 0;
		virtual void end_frame() = 0;
		virtual Framebuffer* get_main_framebuffer() = 0;
		virtual Framebuffer* get_main_framebuffer_pre_rotate() const = 0;
		virtual DeviceVR* get_vr() = 0;
		virtual void set_vr(bool enabled) = 0;
		virtual bool is_vr_connected() = 0;
		virtual bool is_vsync_enabled() const = 0;
		virtual std::string get_feature_level() = 0;
		virtual std::string get_shading_language() = 0;
		virtual bool get_gpu_driver_version(int* out) const = 0;
		virtual std::shared_ptr<Shader> create_shader(Shader::Type type, const std::vector<char>& bytecode, const std::string& name) = 0;
		virtual std::shared_ptr<ShaderProgram> create_shader_program(const std::shared_ptr<Shader>* shaders, std::size_t count, const std::string& name) = 0;
		virtual std::shared_ptr<Buffer> create_buffer(Buffer::Type type, unsigned size, unsigned element_size, Buffer::Usage usage, const std::string& name) = 0;
		virtual const DeviceCaps& get_caps() const = 0;
		virtual void set_display_maximum_refresh_rate(double rate) = 0;
		virtual Statistics get_statistics() const = 0;

	private:
		virtual void reserved_30() = 0;

	public:
		virtual void set_frame_timing_details(FrameTimingDetails details) = 0;
		virtual double consume_drawable_wait_stats() = 0;
		virtual void suspend() = 0;
		virtual void resume() = 0;
		virtual void set_thread_context() = 0;
		virtual void* create_shared_context() = 0;
		virtual void delete_shared_context(void* context) = 0;
		virtual void update_window_handle(void* handle) = 0;
		virtual void reset_pipeline_cache() = 0;
		virtual std::size_t get_device_memory_alloc_size(std::size_t* out) const = 0;
		virtual VideoMemoryInfo get_video_memory_info() const = 0;
		virtual void* get_physical_device_pointer() const = 0;
		virtual void* get_main_window_handle() const = 0;
		virtual void query_hardware_information(ClientSessionMapFieldContainer& out) = 0;
		virtual void hint_shader_compilation_finished() const = 0;
		virtual void* get_debug_trace() = 0;
		virtual std::shared_ptr<VertexLayout> create_vertex_layout_impl(const std::vector<VertexLayout::Element>& elements, const std::vector<std::size_t>& strides, const std::string& name) = 0;
		virtual std::shared_ptr<Geometry> create_geometry_impl(const std::shared_ptr<VertexLayout>& layout, const std::shared_ptr<Buffer>* vertex_buffers, std::size_t vertex_buffer_count, const std::shared_ptr<Buffer>& index_buffer, unsigned base_vertex_index, const std::string& name) = 0;
		virtual std::shared_ptr<Framebuffer> create_framebuffer_impl(const std::vector<Renderbuffer>& color, const Renderbuffer& depth, const std::string& name) = 0;
		virtual std::shared_ptr<Texture> create_texture_impl(Texture::Type type, Texture::Format format, unsigned width, unsigned height, unsigned depth, unsigned mip_levels, unsigned array_length, unsigned samples, Texture::Usage usage, const std::string& name) = 0;
		virtual std::shared_ptr<Texture> create_texture_no_crash_impl(Texture::Type type, Texture::Format format, unsigned width, unsigned height, unsigned depth, unsigned mip_levels, unsigned array_length, unsigned samples, Texture::Usage usage, const std::string& name) = 0;
		virtual std::shared_ptr<Texture> create_texture_with_hardware_buffer_impl(Texture::Type type, Texture::Format format, unsigned width, unsigned height, unsigned depth, unsigned mip_levels, unsigned array_length, unsigned samples, Texture::Usage usage, const std::string& name, void* buffer) = 0;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Device::Statistics, display_interval_ms, 0x8);
	RML_ASSERT_OFFSET(Device::Statistics, vsync_stats_valid, 0x38);
	RML_ASSERT_SIZE(Device::Statistics, 0x40);
	RML_ASSERT_SIZE(Device::VideoMemoryInfo, 24);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
