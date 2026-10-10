#pragma once

#include "settings.hpp"

#include <RobloxModLoader/render/passes/fullscreen_pass.hpp>
#include <RobloxModLoader/roblox/graphics/texture_download_buffer.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace tracy_profiler
{
	class FrameImages final : public rml::render::FullscreenPass
	{
	public:
		static constexpr std::string_view pass_name = "TracyProfiler.FrameImages";

		FrameImages();

		void apply(const Settings& settings);
		void shutdown();

		[[nodiscard]] std::string get_name() const override;
		[[nodiscard]] rml::render::InjectionPoint get_injection_point() const override;
		void pre_render(const rml::render::FrameContext& frame) override;
		void render(const rml::render::RenderContext& ctx) override;
		void on_device_lost() override;

	private:
		struct Slot
		{
			std::shared_ptr<RBX::Graphics::TextureDownloadBuffer> buffer;
			std::uint64_t issued{};
			std::uint64_t mark{};
			std::uint32_t width{};
			std::uint32_t height{};
			bool pending{};
		};

		struct Params
		{
			float target_size[2];
			float sample_step[2];
		};

		void reset();
		void collect();
		void issue(const rml::render::FrameContext& frame);

		std::atomic<bool> m_enabled{true};
		std::atomic<std::uint32_t> m_max_width{320};
		std::atomic<std::uint32_t> m_max_height{180};
		std::atomic<std::uint32_t> m_interval{1};
		std::atomic<bool> m_stopping{false};
		bool m_readable{};
		std::uint64_t m_epoch{};
		std::uint64_t m_frames{};
		std::uint32_t m_width{};
		std::uint32_t m_height{};
		std::uint64_t m_drawn_mark{};
		std::uint64_t m_last_emitted{};
		bool m_drawn{};
		std::array<Slot, 3> m_slots{};
		std::vector<std::uint8_t> m_pixels;
	};
}
