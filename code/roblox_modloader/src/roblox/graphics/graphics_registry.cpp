#include "graphics_registry.hpp"

#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/instruction.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/memory/string_anchor.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"
#include "RobloxModLoader/roblox/graphics/adorn_render.hpp"
#include "RobloxModLoader/roblox/graphics/device.hpp"
#include "RobloxModLoader/roblox/graphics/device_context.hpp"
#include "RobloxModLoader/roblox/graphics/shader_manager.hpp"
#include "RobloxModLoader/util/string.hpp"

#include <algorithm>

RML_LOG_SCOPE("Graphics");

namespace rml::graphics
{
	static constexpr unsigned k_max_callback_failures = 2;
	static constexpr std::size_t k_min_detour_target_size = 32;
	static constexpr std::uint64_t k_adorn_stale_frames = 8;

	GraphicsRegistry& GraphicsRegistry::instance()
	{
		static GraphicsRegistry registry;
		return registry;
	}

	RBX::Graphics::VisualEngine* GraphicsRegistry::visual_engine() const
	{
		return m_visual_engine.load(std::memory_order_acquire);
	}

	RBX::Graphics::Device* GraphicsRegistry::device() const
	{
		const auto engine = visual_engine();
		return engine ? engine->device : nullptr;
	}

	RBX::Graphics::SceneManager* GraphicsRegistry::scene_manager() const
	{
		return m_scene_manager.load(std::memory_order_acquire);
	}

	void GraphicsRegistry::set_scene_manager(RBX::Graphics::SceneManager* scene_manager)
	{
		if (m_scene_manager.exchange(scene_manager, std::memory_order_acq_rel) != scene_manager)
			RML_INFO("SceneManager captured at 0x{:X}", reinterpret_cast<std::uintptr_t>(scene_manager));
	}

	void GraphicsRegistry::set_visual_engine(RBX::Graphics::VisualEngine* engine)
	{
		m_device.store(engine ? engine->device : nullptr, std::memory_order_release);
		if (m_visual_engine.exchange(engine, std::memory_order_acq_rel) != engine)
			RML_INFO("VisualEngine captured at 0x{:X} (device 0x{:X})",
			    reinterpret_cast<std::uintptr_t>(engine),
			    reinterpret_cast<std::uintptr_t>(engine ? engine->device : nullptr));
	}

	void GraphicsRegistry::advance_frame()
	{
		m_frame.fetch_add(1, std::memory_order_acq_rel);
	}

	void GraphicsRegistry::on_device_destroyed(RBX::Graphics::Device* device)
	{
		auto* expected = device;
		if (m_device.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel))
		{
			m_visual_engine.store(nullptr, std::memory_order_release);
			m_scene_manager.store(nullptr, std::memory_order_release);
			RML_INFO("Device 0x{:X} destroyed", reinterpret_cast<std::uintptr_t>(device));
		}
	}

	void GraphicsRegistry::add_adorn_callback(AdornCallback callback)
	{
		std::lock_guard lock(m_callbacks_mutex);
		m_adorn_callbacks.emplace_back(std::move(callback), 0u);
	}

	void GraphicsRegistry::run_adorn_callbacks(RBX::Graphics::AdornRender& adorn)
	{
		track_adorn_render(adorn);

		std::lock_guard lock(m_callbacks_mutex);

		for (auto it = m_adorn_callbacks.begin(); it != m_adorn_callbacks.end();)
		{
			try
			{
				it->first(adorn);
				++it;
				continue;
			}
			catch (const std::exception& e)
			{
				RML_ERROR("adorn callback threw: {}", e.what());
			}
			catch (...)
			{
				RML_ERROR("adorn callback threw an unknown exception");
			}

			if (++it->second >= k_max_callback_failures)
			{
				RML_ERROR("adorn callback removed after {} failures", it->second);
				it = m_adorn_callbacks.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void GraphicsRegistry::track_adorn_render(RBX::Graphics::AdornRender& adorn)
	{
		const auto frame = m_frame.load(std::memory_order_acquire);
		const auto area = adorn.viewport_width * adorn.viewport_height;
		std::lock_guard lock(m_adorn_mutex);
		const auto it = std::ranges::find(m_adorn_renders, &adorn, &AdornEntry::adorn);
		if (it != m_adorn_renders.end())
		{
			it->last_frame = frame;
			it->area = area;
			return;
		}

		m_adorn_renders.push_back({&adorn, frame, area});
		RML_INFO("AdornRender captured at 0x{:X} ({}x{}), {} active",
		    reinterpret_cast<std::uintptr_t>(&adorn),
		    adorn.viewport_width,
		    adorn.viewport_height,
		    m_adorn_renders.size());
	}

	std::vector<RBX::Graphics::AdornRender*> GraphicsRegistry::adorn_renders()
	{
		const auto frame = m_frame.load(std::memory_order_acquire);
		std::lock_guard lock(m_adorn_mutex);
		std::erase_if(m_adorn_renders, [frame](const AdornEntry& entry) {
			return entry.last_frame + k_adorn_stale_frames < frame;
		});

		auto entries = m_adorn_renders;
		std::ranges::stable_sort(entries, std::greater{}, &AdornEntry::area);

		std::vector<RBX::Graphics::AdornRender*> result;
		result.reserve(entries.size());
		for (const auto& entry : entries)
			result.push_back(entry.adorn);
		return result;
	}

	RBX::Graphics::AdornRender* GraphicsRegistry::adorn_render()
	{
		const auto renders = adorn_renders();
		return renders.empty() ? nullptr : renders.front();
	}

	static void* follow_thunk(void* target)
	{
#if defined(RML_MACOS)
		const auto word = *static_cast<const std::uint32_t*>(target);
		if (memory::instruction::arm64_is_branch(word) && !memory::instruction::arm64_is_linked_branch(word))
			return reinterpret_cast<void*>(memory::instruction::arm64_branch_target(reinterpret_cast<std::uintptr_t>(target), word));
#endif
		return target;
	}

	void* device_destructor_target()
	{
		auto* const index = memory::rtti();
		if (!index)
			return nullptr;

		const memory::module image(platform::studio_image_name());
		for (const auto* name : {"RBX::Graphics::DeviceD3D11", "RBX::Graphics::DeviceMtl"})
		{
			const auto vtable = index->find(name);
			if (!vtable)
				continue;

			auto* target = (*vtable)[platform::abi::destructor_body_slot];
			if (image.contains(memory::handle(target)))
				target = follow_thunk(target);
			const auto function = image.contains(memory::handle(target)) ? memory::function_containing(target) : std::nullopt;
			if (!function || function->start != target || function->size < k_min_detour_target_size)
			{
				RML_ERROR("{} destructor is not detourable", name);
				return nullptr;
			}
			return target;
		}
		return nullptr;
	}

	void* device_context_begin_pass_target()
	{
		auto* const index = memory::rtti();
		if (!index)
			return nullptr;

		const memory::module image(platform::studio_image_name());
		const auto slot = memory::virtual_index(&RBX::Graphics::DeviceContext::begin_pass);
		for (const auto* name : {"RBX::Graphics::DeviceContextD3D11", "RBX::Graphics::DeviceContextMtl"})
		{
			const auto vtable = index->find(name);
			if (!vtable)
				continue;

			auto* target = (*vtable)[slot];
			const auto function = image.contains(memory::handle(target)) ? memory::function_containing(target) : std::nullopt;
			if (!function || function->start != target || function->size < k_min_detour_target_size)
			{
				RML_ERROR("{} vtable slot {} is not detourable", name, slot);
				return nullptr;
			}
			return target;
		}
		return nullptr;
	}

	void* adorn_render_pre_submit_pass_target()
	{
		auto* const index = memory::rtti();
		if (!index)
			return nullptr;

		const auto vtable = index->find("RBX::Graphics::AdornRender");
		if (!vtable)
			return nullptr;

		const auto slot = memory::virtual_index(&RBX::Adorn::pre_submit_pass);
		const memory::module image(platform::studio_image_name());
		auto* target = (*vtable)[slot];
		if (!image.contains(memory::handle(target)))
		{
			RML_ERROR("AdornRender vtable slot {} is not code", slot);
			return nullptr;
		}

		const auto function = memory::function_containing(target);
		if (!function || function->start != target || function->size < k_min_detour_target_size)
		{
			RML_ERROR("AdornRender vtable slot {} is too small to detour ({} bytes)", slot, function ? function->size : 0);
			return nullptr;
		}
		return target;
	}

	bool GraphicsRegistry::validate()
	{
		if (const auto state = m_validation.load(std::memory_order_acquire); state != 0)
			return state > 0;

		const auto check = [&]() -> bool {
			auto* device = this->device();
			if (!device)
			{
				RML_ERROR("graphics surface disabled: VisualEngine has no device");
				return false;
			}

			const memory::module image(platform::studio_image_name());
			auto** vtable = memory::vtable_of(device);
			const auto slots = memory::virtual_index(&RBX::Graphics::Device::create_texture_with_hardware_buffer_impl) + 1;
			for (std::size_t slot = 0; slot < slots; ++slot)
			{
				if (!image.contains(memory::handle(vtable[slot])) || !platform::is_executable(vtable[slot]))
				{
					RML_ERROR("graphics surface disabled: Device vtable slot {} is not code", slot);
					return false;
				}
			}
			if (platform::is_executable(vtable[slots]))
			{
				RML_ERROR("graphics surface disabled: Device vtable has more than {} slots", slots);
				return false;
			}

			const auto language = device->get_shading_language();
			const auto level = device->get_feature_level();
			if (!utils::is_printable(language, 64) || !utils::is_printable(level, 64))
			{
				RML_ERROR("graphics surface disabled: Device strings are not printable");
				return false;
			}

			RML_INFO("Device: shading language '{}', feature level '{}', {} vtable slots", language, level, slots);
			return true;
		};

		const bool ok = check();
		m_validation.store(ok ? 1 : -1, std::memory_order_release);
		return ok;
	}

	RBX::Graphics::VisualEngine* visual_engine()
	{
		return GraphicsRegistry::instance().visual_engine();
	}

	RBX::Graphics::Device* device()
	{
		return GraphicsRegistry::instance().device();
	}

	RBX::Graphics::SceneManager* scene_manager()
	{
		return GraphicsRegistry::instance().scene_manager();
	}

	RBX::Graphics::IShaderManager* shader_manager()
	{
		const auto engine = GraphicsRegistry::instance().visual_engine();
		return engine ? engine->get_shader_manager() : nullptr;
	}

	std::shared_ptr<RBX::Graphics::ShaderProgram> engine_program(const std::string_view vertex, const std::string_view fragment)
	{
		auto* manager = shader_manager();
		if (!manager)
			return nullptr;
		return manager->get_program(RBX::StringView(vertex), RBX::StringView(fragment));
	}

	void add_adorn_callback(AdornCallback callback)
	{
		GraphicsRegistry::instance().add_adorn_callback(std::move(callback));
	}

	RBX::Graphics::AdornRender* adorn_render()
	{
		return GraphicsRegistry::instance().adorn_render();
	}

	std::vector<RBX::Graphics::AdornRender*> adorn_renders()
	{
		return GraphicsRegistry::instance().adorn_renders();
	}
}
