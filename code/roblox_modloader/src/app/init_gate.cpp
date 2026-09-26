#include "init_gate.hpp"

#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/memory/all.hpp"
#include "RobloxModLoader/memory/signature_cache.hpp"
#include "RobloxModLoader/mod/init_context.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "config/config_manager.hpp"
#include "memory/engine_signatures.hpp"
#include "pointers.hpp"

RML_LOG_SCOPE("InitGate");

namespace rml
{
	InitGate::InitGate()
	{
		g_init_gate = this;
	}

	InitGate::~InitGate()
	{
		m_latch.release();
		if (m_owned_engine && g_hook_engine == m_owned_engine.get())
			g_hook_engine = nullptr;
		g_init_gate = nullptr;
	}

	InitGate* InitGate::instance()
	{
		return g_init_gate;
	}

	static void* find_global_init()
	{
		if (g_pointers && g_pointers->m_roblox_pointers.global_init)
			return reinterpret_cast<void*>(g_pointers->m_roblox_pointers.global_init);

		const auto [batch, hash] = Pointers::get_roblox_batch();
		for (const auto& entry : batch.m_entries)
		{
			if (std::string_view(entry.m_name.str) != "RBX_GLOBAL_INIT")
				continue;

			const memory::module image(platform::studio_image_name());
			if (const auto cached = memory::find_cached_signature(entry, image, hash))
			{
				RML_INFO("RBX_GLOBAL_INIT taken from the signature cache");
				return cached->as<void*>();
			}

			const auto started = std::chrono::steady_clock::now();
			const auto hit = memory::scan_signature(entry, image);
			RML_INFO("RBX_GLOBAL_INIT scanned alone in {} ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count());
			return hit ? hit->as<void*>() : nullptr;
		}
		return nullptr;
	}

	std::expected<void, std::string> InitGate::install()
	{
		const auto target = find_global_init();
		if (!target)
		{
			m_latch.mark_missed();
			return std::unexpected("RBX_GLOBAL_INIT was not resolved; class registration is unavailable this session");
		}

		if (!g_hook_engine)
		{
			m_owned_engine = create_hook_engine();
			g_hook_engine = m_owned_engine.get();
		}

		Hooking::DetourHookHelper::add_now<Hooks::global_init>("RBX_GLOBAL_INIT", reinterpret_cast<void*>(target));

		RML_INFO("RBX::globalInit gate installed at 0x{:X}", reinterpret_cast<std::uintptr_t>(target));
		return {};
	}

	void InitGate::register_callback(std::string mod_name, Callback callback)
	{
		std::lock_guard lock(m_callbacks_mutex);
		m_callbacks.emplace_back(std::move(mod_name), std::move(callback));
	}

	void InitGate::mark_mods_loaded()
	{
		m_latch.release();
	}

	void InitGate::abort()
	{
		m_latch.release();
	}

	InitGateState InitGate::state() const
	{
		return m_latch.state();
	}

	bool InitGate::is_open() const
	{
		return m_open.load(std::memory_order_acquire);
	}

	void InitGate::on_global_init_reached()
	{
		m_latch.mark_reached();
		const auto timeout = std::chrono::seconds(config::get_config_manager().get_core_config().developer.init_gate_timeout_seconds);
		RML_INFO("RBX::globalInit reached; waiting up to {}s for mods", timeout.count());

		if (!m_latch.wait(timeout))
			RML_WARN("Init gate timed out after {}s; running on_init for the mods loaded so far", timeout.count());

		run_callbacks();
	}

	void InitGate::run_callbacks()
	{
		std::vector<std::pair<std::string, Callback>> callbacks;
		{
			std::lock_guard lock(m_callbacks_mutex);
			callbacks = m_callbacks;
		}

		InitContext context(*this);
		m_open.store(true, std::memory_order_release);

		for (auto& [name, callback] : callbacks)
		{
			try
			{
				callback(context);
				RML_INFO("on_init done: {}", name);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("on_init threw for {}: {}", name, e.what());
			}
			catch (...)
			{
				RML_ERROR("on_init threw an unknown exception for {}", name);
			}
		}

		m_open.store(false, std::memory_order_release);
		RML_INFO("Init gate closed after {} callbacks", callbacks.size());
	}

	bool InitContext::is_open() const
	{
		return m_gate.is_open();
	}
}
