#pragma once

#include "RobloxModLoader/roblox/graphics/render_pass.hpp"
#include "RobloxModLoader/roblox/graphics/visual_engine.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace rml::graphics
{
	void* adorn_render_pre_submit_pass_target();

	class GraphicsRegistry
	{
	public:
		static GraphicsRegistry& instance();

		[[nodiscard]] RBX::Graphics::VisualEngine* visual_engine() const;
		[[nodiscard]] RBX::Graphics::Device* device() const;
		[[nodiscard]] RBX::Graphics::SceneManager* scene_manager() const;
		void set_visual_engine(RBX::Graphics::VisualEngine* engine);
		void set_scene_manager(RBX::Graphics::SceneManager* scene_manager);
		void advance_frame();
		void add_render_callback(RenderCallback callback);
		void run_render_callbacks(RenderPassContext& context);
		void add_adorn_callback(AdornCallback callback);
		void run_adorn_callbacks(RBX::Graphics::AdornRender& adorn);
		[[nodiscard]] RBX::Graphics::AdornRender* adorn_render();
		[[nodiscard]] std::vector<RBX::Graphics::AdornRender*> adorn_renders();
		[[nodiscard]] bool validate();

	private:
		struct Entry
		{
			RenderCallback callback;
			unsigned failures;
		};

		struct AdornEntry
		{
			RBX::Graphics::AdornRender* adorn;
			std::uint64_t last_frame;
			float area;
		};

		void track_adorn_render(RBX::Graphics::AdornRender& adorn);

		std::atomic<RBX::Graphics::VisualEngine*> m_visual_engine{nullptr};
		std::atomic<RBX::Graphics::SceneManager*> m_scene_manager{nullptr};
		std::mutex m_callbacks_mutex;
		std::vector<Entry> m_callbacks;
		std::vector<std::pair<AdornCallback, unsigned>> m_adorn_callbacks;
		std::atomic<std::uint64_t> m_frame{0};
		std::mutex m_adorn_mutex;
		std::vector<AdornEntry> m_adorn_renders;
		std::atomic<int> m_validation{0};
	};
}
