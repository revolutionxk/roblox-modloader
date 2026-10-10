#pragma once

#include "RobloxModLoader/roblox/graphics/texture.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rml::render
{
	namespace resource_names
	{
		inline constexpr std::string_view SCENE_DEPTH = "scene_depth";
		inline constexpr std::string_view SCENE_COLOR = "scene_color";
		inline constexpr std::string_view OUTPUT_COLOR = "output_color";
	}

	struct TargetDesc
	{
		std::uint32_t width{};
		std::uint32_t height{};
		std::array<RBX::Graphics::Texture::Format, 4> colors{};
		std::uint8_t color_count{1};
		std::optional<RBX::Graphics::Texture::Format> depth;

		friend bool operator==(const TargetDesc&, const TargetDesc&) = default;
	};

	class RenderTarget
	{
	public:
		virtual ~RenderTarget() = default;

		[[nodiscard]] virtual const TargetDesc& desc() const = 0;
		[[nodiscard]] virtual RBX::Graphics::Texture* color(std::size_t index) const = 0;
		[[nodiscard]] virtual RBX::Graphics::Texture* depth() const = 0;
	};

	class FrameResources
	{
	public:
		virtual ~FrameResources() = default;

		virtual RenderTarget* target(std::string_view id, const TargetDesc& desc) = 0;
		[[nodiscard]] virtual RenderTarget* find(std::string_view id) const = 0;
		virtual void publish(std::string_view id, RBX::Graphics::Texture* texture) = 0;
		[[nodiscard]] virtual RBX::Graphics::Texture* texture(std::string_view id) const = 0;
	};
}
