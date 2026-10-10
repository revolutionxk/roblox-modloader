#include "render/frame_resources_impl.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/graphics/buffer.hpp"
#include "RobloxModLoader/roblox/graphics/device.hpp"
#include "RobloxModLoader/roblox/graphics/texture.hpp"

RML_LOG_SCOPE("Render");

namespace rml::render::detail
{
	static constexpr auto k_target_usage = static_cast<RBX::Graphics::Texture::Usage>(static_cast<std::uint32_t>(RBX::Graphics::Texture::Usage::ShaderRead) | static_cast<std::uint32_t>(RBX::Graphics::Texture::Usage::RenderTarget));

	RenderTargetImpl::RenderTargetImpl(TargetDesc desc, std::vector<std::shared_ptr<RBX::Graphics::Texture>> colors, std::shared_ptr<RBX::Graphics::Texture> depth, std::shared_ptr<RBX::Graphics::Framebuffer> framebuffer) :
	    m_desc(desc),
	    m_colors(std::move(colors)),
	    m_depth(std::move(depth)),
	    m_framebuffer(std::move(framebuffer))
	{
	}

	const TargetDesc& RenderTargetImpl::desc() const
	{
		return m_desc;
	}

	RBX::Graphics::Texture* RenderTargetImpl::color(const std::size_t index) const
	{
		return index < m_colors.size() ? m_colors[index].get() : nullptr;
	}

	RBX::Graphics::Texture* RenderTargetImpl::depth() const
	{
		return m_depth.get();
	}

	RBX::Graphics::Framebuffer* RenderTargetImpl::framebuffer() const
	{
		return m_framebuffer.get();
	}

	FrameResourcesImpl::FrameResourcesImpl(RBX::Graphics::Device& device) :
	    m_device(device)
	{
	}

	static bool valid(const TargetDesc& desc)
	{
		if (desc.width == 0 || desc.height == 0 || desc.color_count > desc.colors.size())
			return false;
		return desc.color_count > 0 || desc.depth.has_value();
	}

	std::unique_ptr<RenderTargetImpl> FrameResourcesImpl::create(const TargetDesc& desc, const std::string& name) const
	{
		using namespace RBX::Graphics;
		try
		{
			std::vector<std::shared_ptr<Texture>> colors;
			std::vector<Renderbuffer> attachments;
			for (std::uint8_t i = 0; i < desc.color_count; ++i)
			{
				auto texture = m_device.create_texture_impl(Texture::Type::Type_2D, desc.colors[i], desc.width, desc.height, 1, 1, 1, 1, k_target_usage, std::format("{}.color{}", name, i));
				if (!texture)
					return nullptr;
				attachments.push_back(Renderbuffer{texture});
				colors.push_back(std::move(texture));
			}

			std::shared_ptr<Texture> depth;
			Renderbuffer depth_attachment{};
			if (desc.depth)
			{
				depth = m_device.create_texture_impl(Texture::Type::Type_2D, *desc.depth, desc.width, desc.height, 1, 1, 1, 1, k_target_usage, name + ".depth");
				if (!depth)
					return nullptr;
				depth_attachment = Renderbuffer{depth};
			}

			auto framebuffer = m_device.create_framebuffer_impl(attachments, depth_attachment, name);
			if (!framebuffer)
				return nullptr;
			RML_DEBUG("render target '{}' {}x{} with {} colour attachment(s)", name, desc.width, desc.height, desc.color_count);
			return std::make_unique<RenderTargetImpl>(desc, std::move(colors), std::move(depth), std::move(framebuffer));
		}
		catch (const std::exception& e)
		{
			RML_ERROR("render target '{}' could not be created: {}", name, e.what());
			return nullptr;
		}
	}

	RenderTarget* FrameResourcesImpl::target(const std::string_view id, const TargetDesc& desc)
	{
		if (!valid(desc))
			return nullptr;

		const auto it = m_targets.find(id);
		if (it != m_targets.end() && it->second.target->desc() == desc)
		{
			it->second.last_used = m_frame;
			return it->second.target.get();
		}

		auto created = create(desc, std::format("rml:{}", id));
		if (!created)
		{
			if (it != m_targets.end())
				m_targets.erase(it);
			return nullptr;
		}

		auto* raw = created.get();
		m_targets.insert_or_assign(std::string(id), Entry{std::move(created), m_frame});
		return raw;
	}

	RenderTarget* FrameResourcesImpl::find(const std::string_view id) const
	{
		const auto it = m_targets.find(id);
		return it == m_targets.end() ? nullptr : it->second.target.get();
	}

	void FrameResourcesImpl::publish(const std::string_view id, RBX::Graphics::Texture* texture)
	{
		if (texture)
			m_published.insert_or_assign(std::string(id), texture);
		else
			withdraw(id);
	}

	void FrameResourcesImpl::provide(const std::string_view id, Provider provider)
	{
		m_published.erase(std::string(id));
		m_providers.insert_or_assign(std::string(id), std::move(provider));
	}

	RBX::Graphics::Texture* FrameResourcesImpl::texture(const std::string_view id) const
	{
		if (const auto it = m_published.find(id); it != m_published.end())
			return it->second;

		const auto provider = m_providers.find(id);
		if (provider == m_providers.end())
			return nullptr;

		auto produce = std::move(provider->second);
		m_providers.erase(provider);
		auto* texture = produce ? produce() : nullptr;
		if (texture)
			m_published.insert_or_assign(std::string(id), texture);
		return texture;
	}

	void FrameResourcesImpl::withdraw(const std::string_view id)
	{
		if (const auto it = m_published.find(id); it != m_published.end())
			m_published.erase(it);
		if (const auto it = m_providers.find(id); it != m_providers.end())
			m_providers.erase(it);
	}

	void FrameResourcesImpl::begin_frame(const std::uint64_t frame_index)
	{
		m_frame = frame_index;
		m_published.clear();
		m_providers.clear();
		std::erase_if(m_targets, [frame_index](const auto& entry) { return entry.second.last_used + k_eviction_frames < frame_index; });
	}

	RBX::Graphics::Device& FrameResourcesImpl::device() const
	{
		return m_device;
	}

	FullscreenGeometry create_fullscreen_geometry(RBX::Graphics::Device& device)
	{
		FullscreenGeometry result;
		result.layout = device.create_vertex_layout_impl({}, {}, "rml_fullscreen");
		if (result.layout)
			result.geometry = device.create_geometry_impl(result.layout, nullptr, 0, nullptr, 0, "rml_fullscreen");
		return result;
	}
}
