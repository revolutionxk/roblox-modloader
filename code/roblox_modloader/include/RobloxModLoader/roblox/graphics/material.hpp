#pragma once

#include "RobloxModLoader/roblox/graphics/device_context.hpp"
#include "RobloxModLoader/roblox/graphics/render_queue.hpp"
#include "RobloxModLoader/roblox/graphics/shader.hpp"
#include "RobloxModLoader/roblox/graphics/texture_ref.hpp"
#include "RobloxModLoader/roblox/graphics/types.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace RBX::Graphics
{
	using TechniqueCaps = unsigned;

	class Technique
	{
	public:
		struct TextureUnit
		{
			TextureRef texture;
			SamplerState state;
		};

		struct BufferSlot
		{
			std::uint32_t constant_offset;
			std::uint32_t constant_size;
			std::shared_ptr<Buffer> buffer;
		};

		static constexpr unsigned lod_lq_threshold = 3;

		unsigned lod_index;
		RenderQueue::Pass pass;
		TechniqueCaps caps;
		float texel_density_scale;
		RasterizerState rasterizer_state;
		DepthState depth_state;
		BlendState blend_state;
		std::shared_ptr<ShaderProgram> program;
		std::uint32_t buffer_mask;
		std::uint32_t texture_mask;
		std::vector<TextureUnit> textures;
		std::vector<BufferSlot> buffers;
		std::vector<char> constants;

		Technique(std::shared_ptr<ShaderProgram> program, const unsigned lod_index, const RenderQueue::Pass pass = RenderQueue::Pass_Default) :
		    lod_index(lod_index),
		    pass(pass),
		    caps(0),
		    texel_density_scale(1.0f),
		    rasterizer_state(RasterizerState::make(RasterizerState::Cull_Back)),
		    depth_state(DepthState::make(DepthState::Function_LessEqual, true)),
		    blend_state(BlendState::opaque()),
		    program(std::move(program)),
		    buffer_mask(0),
		    texture_mask(0)
		{
		}

		void set_rasterizer_state(const RasterizerState& state)
		{
			rasterizer_state = state;
		}

		const RasterizerState& get_rasterizer_state() const
		{
			return rasterizer_state;
		}

		void set_depth_state(const DepthState& state)
		{
			depth_state = state;
		}

		const DepthState& get_depth_state() const
		{
			return depth_state;
		}

		void set_blend_state(const BlendState& state)
		{
			blend_state = state;
		}

		const BlendState& get_blend_state() const
		{
			return blend_state;
		}

		void set_texture(const unsigned stage, const TextureRef& texture, const SamplerState& state)
		{
			if (textures.size() <= stage)
				textures.resize(stage + 1, TextureUnit{TextureRef(), SamplerState::make(SamplerState::Filter_Point)});
			texture_mask |= 1u << stage;
			textures[stage].texture = texture;
			textures[stage].state = state;
		}

		const TextureRef& get_texture(const unsigned stage) const
		{
			static const TextureRef dummy;
			return stage < textures.size() ? textures[stage].texture : dummy;
		}

		void set_buffer(const unsigned slot, const std::shared_ptr<Buffer>& buffer)
		{
			if (buffers.size() <= slot)
				buffers.resize(slot + 1);
			buffer_mask |= 1u << slot;
			buffers[slot].buffer = buffer;
		}

		void set_constants(const unsigned slot, const void* data, const unsigned size)
		{
			if (buffers.size() <= slot)
				buffers.resize(slot + 1);
			buffer_mask |= 1u << slot;
			auto& target = buffers[slot];
			if (target.constant_size == 0)
			{
				target.constant_offset = static_cast<std::uint32_t>(constants.size());
				target.constant_size = size;
				constants.resize(constants.size() + size);
			}
			std::memcpy(constants.data() + target.constant_offset, data, std::min(size, target.constant_size));
		}

		void set_caps(const TechniqueCaps value)
		{
			caps = value;
		}

		bool matches_caps(const TechniqueCaps available) const
		{
			return (~available & caps) == 0;
		}

		unsigned get_lod_index() const
		{
			return lod_index;
		}

		RenderQueue::Pass get_pass() const
		{
			return pass;
		}

		const ShaderProgram* get_program() const
		{
			return program.get();
		}

		bool is_lq() const
		{
			return lod_index >= lod_lq_threshold;
		}

		bool is_shq() const
		{
			return lod_index == 0;
		}

		void bind_resources(DeviceContext* context) const
		{
			if (program)
			{
				for (unsigned pending = buffer_mask & program->buffer_mask; pending != 0; pending &= pending - 1)
				{
					const unsigned slot = std::countr_zero(pending);
					const auto& entry = buffers[slot];
					if (entry.constant_size != 0)
						context->bind_buffer_data(slot, constants.data() + entry.constant_offset, entry.constant_size);
					else if (entry.buffer)
						context->bind_buffer(slot, entry.buffer.get());
				}

				for (unsigned pending = texture_mask & program->texture_mask; pending != 0; pending &= pending - 1)
				{
					const unsigned stage = std::countr_zero(pending);
					textures[stage].texture.bind_to_device(context, stage, textures[stage].state);
				}
			}
		}

		void apply(DeviceContext* context) const
		{
			context->set_render_state(rasterizer_state, blend_state, depth_state);
			context->bind_program(program.get());
			bind_resources(context);
		}

		bool ready() const
		{
			for (const auto& unit : textures)
			{
				if (unit.texture.get_status() == TextureLoadStatus::Waiting)
					return false;
			}
			return true;
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(Technique::TextureUnit, 32);
	RML_ASSERT_SIZE(Technique::BufferSlot, 24);
	RML_ASSERT_OFFSET(Technique, caps, 8);
	RML_ASSERT_OFFSET(Technique, rasterizer_state, 16);
	RML_ASSERT_OFFSET(Technique, depth_state, 24);
	RML_ASSERT_OFFSET(Technique, blend_state, 28);
	RML_ASSERT_OFFSET(Technique, program, 40);
	RML_ASSERT_OFFSET(Technique, buffer_mask, 56);
	RML_ASSERT_OFFSET(Technique, textures, 64);
	RML_ASSERT_OFFSET(Technique, buffers, 88);
	RML_ASSERT_OFFSET(Technique, constants, 112);
	RML_ASSERT_SIZE(Technique, 136);
	RML_LAYOUT_DIAGNOSTIC_POP()

	class Material
	{
	public:
		enum Flags : std::uint32_t
		{
			Flag_Backfaces = 1 << 0
		};

		std::vector<Technique> techniques;

	private:
		[[maybe_unused]] std::uint32_t reserved_18{};

	public:
		std::uint32_t flags{};
		std::uint32_t pass_mask{};

		Material() = default;

		void add_technique(const Technique& technique)
		{
			techniques.push_back(technique);
			pass_mask |= 1u << technique.pass;
		}

		void remove_all_techniques()
		{
			techniques.clear();
			pass_mask = 0;
		}

		const Technique* get_best_technique(const unsigned lod_index, const RenderQueue::Pass pass, const TechniqueCaps caps) const
		{
			for (const auto& technique : techniques)
			{
				if (technique.pass == pass && technique.lod_index >= lod_index && technique.matches_caps(caps))
					return &technique;
			}
			return nullptr;
		}

		const std::vector<Technique>& get_techniques() const
		{
			return techniques;
		}

		std::vector<Technique>& get_techniques()
		{
			return techniques;
		}

		bool has_backfaces() const
		{
			return (flags & Flag_Backfaces) != 0;
		}

		bool ready() const
		{
			for (const auto& technique : techniques)
			{
				if (!technique.ready())
					return false;
			}
			return true;
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Material, flags, 28);
	RML_ASSERT_OFFSET(Material, pass_mask, 32);
	RML_ASSERT_SIZE(Material, 40);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
