#include "render/injection_table.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/roblox/graphics/render_queue.hpp"
#include "RobloxModLoader/roblox/graphics/scene_manager.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"
#include "render/injection_dispatch.hpp"

#include <algorithm>
#include <cstring>
#include <string_view>
#include <type_traits>
#include <vector>

RML_LOG_SCOPE("Render");

namespace rml::render::detail
{
	InjectionTable& InjectionTable::instance()
	{
		static InjectionTable table;
		return table;
	}

	void InjectionTable::resolve(const InjectionPoint point)
	{
		std::lock_guard lock(m_mutex);
		m_resolved.insert(point.key());
		RML_DEBUG("injection point {} resolved", to_string(point));
	}

	void InjectionTable::resolve_stage(const EngineStage stage)
	{
		for (const auto side : {Side::Before, Side::After, Side::Instead})
			resolve(InjectionPoint::stage(stage, side));
		RML_INFO("stage injection points resolved for {}", k_engine_stage_names[static_cast<std::size_t>(stage)]);
	}

	void InjectionTable::resolve_queues()
	{
		m_queues = true;
		RML_INFO("queue injection points resolved for {} group(s)", m_present.count());
	}

	bool InjectionTable::resolved(const InjectionPoint point) const
	{
		if (point.kind() == InjectionKind::Queue)
			return m_queues && has_group(point.queue_group());
		std::lock_guard lock(m_mutex);
		return m_resolved.contains(point.key());
	}

	void InjectionTable::fired(const InjectionPoint point, const std::uint64_t frame)
	{
		std::lock_guard lock(m_mutex);
		m_fired[point.key()] = frame;
	}

	InjectionStatus InjectionTable::status(const InjectionPoint point, const std::uint64_t frame) const
	{
		if (!resolved(point))
			return InjectionStatus::Unresolved;
		std::lock_guard lock(m_mutex);
		const auto it = m_fired.find(point.key());
		return it != m_fired.end() && it->second + 1 >= frame ? InjectionStatus::Available : InjectionStatus::Inactive;
	}

	void InjectionTable::load_queue_names()
	{
		const memory::module image(platform::studio_image_name());
		const auto base = image.begin().as<std::uintptr_t>();
		const auto size = image.size();
		const std::string_view bytes(reinterpret_cast<const char*>(base), size);
		static constexpr std::string_view k_first{"\0Id_Opaque\0", 11};

		std::vector<std::string_view> names;
		if (const auto at = bytes.find(k_first); at != std::string_view::npos)
		{
			const auto first = base + at + 1;
			const auto* words = reinterpret_cast<const std::uintptr_t*>(base);
			const auto count = size / sizeof(std::uintptr_t);
			for (std::size_t i = 0; i + k_max_engine_groups < count && names.empty(); ++i)
			{
				if (words[i] != first)
					continue;
				for (std::size_t j = 0; j < k_max_engine_groups; ++j)
				{
					const auto entry = words[i + j];
					if (entry < base || entry >= base + size)
						break;
					const auto* text = reinterpret_cast<const char*>(entry);
					const std::string_view name(text, strnlen(text, 64));
					if (!name.starts_with("Id_"))
						break;
					names.push_back(name);
				}
				if (names.size() < 4)
					names.clear();
			}
		}

		if (names.empty())
		{
			RML_WARN("queue name table not found; assuming the 0.739+ group order");
			for (std::size_t i = 0; i < static_cast<std::size_t>(QueueGroup::Count); ++i)
			{
				m_groups[i] = static_cast<QueueGroup>(i);
				m_present.set(i);
			}
			return;
		}

		for (std::size_t i = 0; i < names.size(); ++i)
		{
			const auto name = names[i].substr(3);
			const auto it = std::ranges::find(k_queue_group_names, name);
			if (it == k_queue_group_names.end())
			{
				RML_INFO("engine queue group '{}' has no injection points", names[i]);
				continue;
			}
			const auto group = static_cast<QueueGroup>(std::distance(k_queue_group_names.begin(), it));
			m_groups[i] = group;
			m_present.set(static_cast<std::size_t>(group));
		}
		RML_INFO("queue name table: {} engine groups, {} mapped", names.size(), m_present.count());
	}

	std::optional<QueueGroup> InjectionTable::group_of(const std::uint32_t engine_index) const
	{
		return engine_index < m_groups.size() ? m_groups[engine_index] : std::nullopt;
	}

	std::optional<QueueGroup> InjectionTable::group_of(const RBX::Graphics::SceneManager& scene, const void* group) const
	{
		const auto in_queue = [&](const RBX::Graphics::RenderQueue* queue) -> std::optional<QueueGroup> {
			if (!queue)
				return std::nullopt;
			for (std::uint32_t index = 0; index < std::size(queue->groups); ++index)
			{
				if (&queue->groups[index] == group)
					return group_of(index);
			}
			return std::nullopt;
		};

		const RBX::Graphics::RenderQueue* queues[] = {
		    scene.render_queue.get(),
		    scene.player_gui_render_queue.get(),
		    scene.capture_render_queue.get(),
		    scene.shadow_render_queue.get(),
		    scene.gui_render_queue.get(),
		    scene.gui_prepass_render_queue.get(),
		    scene.env_map_render_queue.get(),
		    scene.terrain_feedback_render_queue.get(),
		    scene.highlight_render_queues[0].get(),
		    scene.highlight_render_queues[1].get(),
		    scene.performance_overlay_render_queue.get(),
		};
		for (const auto* queue : queues)
		{
			if (const auto found = in_queue(queue))
				return found;
		}
		for (const auto& queue : scene.studio_selection_render_queues)
		{
			if (const auto found = in_queue(queue.get()))
				return found;
		}
		return std::nullopt;
	}

	bool InjectionTable::has_group(const QueueGroup group) const
	{
		return m_present.test(static_cast<std::size_t>(group));
	}
}

namespace rml::render
{
	InjectionStatus status(const InjectionPoint point)
	{
		return detail::InjectionTable::instance().status(point, detail::InjectionDispatch::instance().frame_index());
	}
}
