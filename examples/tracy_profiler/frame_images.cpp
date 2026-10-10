#include "frame_images.hpp"

#include "capture.hpp"

#include <RobloxModLoader/render/render.hpp>
#include <RobloxModLoader/roblox/graphics/types.hpp>
#include <algorithm>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	static constexpr const char* metal_fragment = R"(
#include <metal_stdlib>
using namespace metal;

struct Params
{
	float2 source_size;
	float2 target_size;
};

fragment float4 rml_tracy_thumbnail_fs(float4 position [[position]], constant Params& p [[buffer(0)]], texture2d<float> source [[texture(0)]])
{
	constexpr sampler linear_clamp(filter::linear, address::clamp_to_edge);
	float2 uv = position.xy / p.target_size;
	float2 step = 1.0 / (p.target_size * 4.0);
	float3 sum = 0.0;
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x)
			sum += source.sample(linear_clamp, uv + (float2(x, y) - 1.5) * step).rgb;
	return float4(sum / 16.0, 1.0);
}
)";

	static constexpr const char* hlsl_fragment = R"(
cbuffer Params : register(b0)
{
	float2 source_size;
	float2 target_size;
};

Texture2D<float4> Source : register(t0);
SamplerState SourceSampler : register(s0);

float4 rml_tracy_thumbnail_fs(float4 position : SV_Position) : SV_Target
{
	float2 uv = position.xy / target_size;
	float2 step = 1.0 / (target_size * 4.0);
	float3 sum = 0.0;
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 4; ++x)
			sum += Source.Sample(SourceSampler, uv + (float2(x, y) - 1.5) * step).rgb;
	return float4(sum / 16.0, 1.0);
}
)";

	static constexpr std::string_view target_id = "tracy_profiler.frame_image";
	static constexpr std::uint64_t read_delay = 2;

	FrameImages::FrameImages() :
	    FullscreenPass({metal_fragment, hlsl_fragment, "rml_tracy_thumbnail_fs", 0x1, 0x1}, "rml_tracy_thumbnail")
	{
	}

	std::string FrameImages::get_name() const
	{
		return std::string(pass_name);
	}

	rml::render::InjectionPoint FrameImages::get_injection_point() const
	{
		return rml::render::InjectionPoint::at(rml::render::FramePoint::FrameEnd);
	}

	void FrameImages::apply(const Settings& settings)
	{
		m_enabled.store(settings.frame_images);
		m_max_width.store(static_cast<std::uint32_t>(settings.frame_image_width));
		m_max_height.store(static_cast<std::uint32_t>(settings.frame_image_height));
		m_interval.store(static_cast<std::uint32_t>(settings.frame_image_interval));
	}

	void FrameImages::shutdown()
	{
		m_stopping.store(true);
	}

	void FrameImages::reset()
	{
		for (auto& slot : m_slots)
			slot = {};
		m_drawn = false;
		m_width = 0;
		m_height = 0;
	}

	void FrameImages::on_device_lost()
	{
		FullscreenPass::on_device_lost();
		reset();
		m_readable = false;
	}

	void FrameImages::collect()
	{
		for (auto& slot : m_slots)
		{
			if (!slot.pending || m_frames < slot.issued + read_delay)
				continue;

			slot.pending = false;
			const auto bytes = static_cast<std::size_t>(slot.width) * slot.height * 4;
			if (!slot.buffer || slot.buffer->size < bytes)
				continue;

			m_pixels.resize(slot.buffer->size);
			if (!slot.buffer->copy_data(m_pixels.data()))
				continue;

			const auto offset = std::min<std::uint64_t>(frame_marks() - slot.mark, 255);
			___tracy_emit_frame_image(m_pixels.data(),
			    static_cast<std::uint16_t>(slot.width),
			    static_cast<std::uint16_t>(slot.height),
			    static_cast<std::uint8_t>(offset),
			    0);
		}
	}

	void FrameImages::issue(const rml::render::FrameContext& frame)
	{
		if (!m_drawn)
			return;
		m_drawn = false;

		auto* target = frame.resources.find(target_id);
		auto* texture = target ? target->color(0) : nullptr;
		if (!texture)
			return;

		auto& slot = m_slots[m_frames % m_slots.size()];
		if (slot.pending)
			slot.pending = false;
		if (!slot.buffer || slot.width != m_width || slot.height != m_height)
			slot.buffer = texture->create_async_download_buffer(0, 0);
		if (!slot.buffer || !texture->async_download(slot.buffer))
			return;

		slot.pending = true;
		slot.issued = m_frames;
		slot.mark = m_drawn_mark;
		slot.width = m_width;
		slot.height = m_height;
	}

	void FrameImages::pre_render(const rml::render::FrameContext& frame)
	{
		try
		{
			const bool wanted = m_enabled.load() && !m_stopping.load() && ___tracy_connected();
			if (!wanted)
			{
				if (m_readable)
					m_readable = !rml::render::enable_output_readback(false);
				reset();
				return;
			}

			if (const auto epoch = connection_epoch(); epoch != m_epoch)
			{
				m_epoch = epoch;
				reset();
			}

			if (!m_readable)
			{
				m_readable = rml::render::enable_output_readback(true);
				return;
			}

			++m_frames;
			collect();
			issue(frame);
		}
		catch (...)
		{
			reset();
		}
	}

	void FrameImages::render(const rml::render::RenderContext& ctx)
	{
		using namespace RBX::Graphics;
		if (!m_readable || !m_enabled.load() || m_stopping.load() || m_frames % std::max<std::uint32_t>(m_interval.load(), 1) != 0)
			return;

		auto* source = ctx.frame.resources.texture(rml::render::resource_names::OUTPUT_COLOR);
		if (!source || source->width < 4 || source->height < 4 || !ensure_program(ctx.frame))
			return;

		const auto scale = std::min({1.0,
		    static_cast<double>(m_max_width.load()) / source->width,
		    static_cast<double>(m_max_height.load()) / source->height});
		const auto width = std::max<std::uint32_t>(static_cast<std::uint32_t>(source->width * scale) / 4 * 4, 4);
		const auto height = std::max<std::uint32_t>(static_cast<std::uint32_t>(source->height * scale) / 4 * 4, 4);

		rml::render::TargetDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.colors[0] = Texture::Format::RGBA8;
		desc.color_count = 1;
		auto* target = ctx.frame.resources.target(target_id, desc);
		if (!target)
			return;

		const Params params{{static_cast<float>(source->width), static_cast<float>(source->height)}, {static_cast<float>(width), static_cast<float>(height)}};
		ctx.commands.begin(*target, rml::render::LoadOp::DontCare);
		ctx.commands.set_state(RasterizerState::make(RasterizerState::Cull_None), BlendState::opaque(), DepthState::make(DepthState::Function_Always, false));
		ctx.commands.bind_texture(0, source, SamplerState::make(SamplerState::Filter_Linear, SamplerState::Address_Clamp));
		ctx.commands.bind_constants(0, params);
		draw_fullscreen(ctx);
		ctx.commands.end();

		m_width = width;
		m_height = height;
		m_drawn = true;
		m_drawn_mark = frame_marks();
	}
}
