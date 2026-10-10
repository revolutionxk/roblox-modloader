#pragma once

#include "RobloxModLoader/render/frame_resources.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace RBX::Graphics
{
	class Device;
	class Framebuffer;
	class Geometry;
	class Texture;
	class VertexLayout;
}

namespace rml::render::detail
{
	class RenderTargetImpl final : public RenderTarget
	{
	public:
		RenderTargetImpl(TargetDesc desc, std::vector<std::shared_ptr<RBX::Graphics::Texture>> colors, std::shared_ptr<RBX::Graphics::Texture> depth, std::shared_ptr<RBX::Graphics::Framebuffer> framebuffer);

		[[nodiscard]] const TargetDesc& desc() const override;
		[[nodiscard]] RBX::Graphics::Texture* color(std::size_t index) const override;
		[[nodiscard]] RBX::Graphics::Texture* depth() const override;
		[[nodiscard]] RBX::Graphics::Framebuffer* framebuffer() const;

	private:
		TargetDesc m_desc;
		std::vector<std::shared_ptr<RBX::Graphics::Texture>> m_colors;
		std::shared_ptr<RBX::Graphics::Texture> m_depth;
		std::shared_ptr<RBX::Graphics::Framebuffer> m_framebuffer;
	};

	class FrameResourcesImpl final : public FrameResources
	{
	public:
		static constexpr std::uint64_t k_eviction_frames = 60;

		using Provider = std::function<RBX::Graphics::Texture*()>;

		explicit FrameResourcesImpl(RBX::Graphics::Device& device);

		RenderTarget* target(std::string_view id, const TargetDesc& desc) override;
		[[nodiscard]] RenderTarget* find(std::string_view id) const override;
		void publish(std::string_view id, RBX::Graphics::Texture* texture) override;
		[[nodiscard]] RBX::Graphics::Texture* texture(std::string_view id) const override;

		void begin_frame(std::uint64_t frame_index);
		void provide(std::string_view id, Provider provider);
		void withdraw(std::string_view id);
		[[nodiscard]] RBX::Graphics::Device& device() const;

	private:
		struct Entry
		{
			std::unique_ptr<RenderTargetImpl> target;
			std::uint64_t last_used{};
		};

		[[nodiscard]] std::unique_ptr<RenderTargetImpl> create(const TargetDesc& desc, const std::string& name) const;

		RBX::Graphics::Device& m_device;
		std::map<std::string, Entry, std::less<>> m_targets;
		mutable std::map<std::string, RBX::Graphics::Texture*, std::less<>> m_published;
		mutable std::map<std::string, Provider, std::less<>> m_providers;
		std::uint64_t m_frame{};
	};

	struct FullscreenGeometry
	{
		std::shared_ptr<RBX::Graphics::VertexLayout> layout;
		std::shared_ptr<RBX::Graphics::Geometry> geometry;
	};

	[[nodiscard]] FullscreenGeometry create_fullscreen_geometry(RBX::Graphics::Device& device);
}
