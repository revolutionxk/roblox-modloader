#include "RobloxModLoader/hooking/detour_hook.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/hooking/i_hook_engine.hpp"

RML_LOG_SCOPE("DetourHook");

namespace rml
{
	DetourHook::DetourHook()
	{
	}

	DetourHook::DetourHook(const std::string& name, void* detour)
	{
		set_instance(name, detour);
	}

	DetourHook::DetourHook(const std::string& name, void* target, void* detour)
	{
		set_instance(name, target, detour);
	}

	void DetourHook::set_instance(const std::string& name, void* detour)
	{
		m_name = name;
		m_detour = detour;
	}

	void DetourHook::set_instance(const std::string& name, void* target, void* detour)
	{
		m_name = name;
		m_target = target;
		m_detour = detour;

		create_hook();
	}

	void DetourHook::set_target_and_create_hook(void* target)
	{
		m_target = target;
		create_hook();
	}

	void DetourHook::create_hook()
	{
		if (!m_target || !g_hook_engine)
			return;

		fix_hook_address();

		if (const auto result = g_hook_engine->create(m_name, m_target, m_detour, &m_original); !result)
			RML_ERROR("Failed to create hook: {}", result.error().describe());
	}

	DetourHook::~DetourHook() noexcept
	{
		destroy();
	}

	void DetourHook::destroy()
	{
		if (!m_target || !g_hook_engine)
			return;

		if (const auto result = g_hook_engine->remove(m_name, m_target); !result)
			RML_ERROR("Failed to remove hook: {}", result.error().describe());

		m_target = nullptr;
		m_original = nullptr;
		m_enabled = false;
	}

	std::expected<void, rml::HookError> DetourHook::enable()
	{
		if (m_enabled)
			return {};

		if (!g_hook_engine)
			return std::unexpected(HookError::from_status(m_name, 0, "hook engine unavailable"));

		auto result = g_hook_engine->queue_enable(m_name, m_target);
		if (!result)
		{
			RML_ERROR("Failed to enable hook '{}': {}", m_name, result.error().describe());
			return result;
		}

		m_enabled = true;
		return {};
	}

	std::expected<void, rml::HookError> DetourHook::disable()
	{
		if (!g_hook_engine)
			return std::unexpected(HookError::from_status(m_name, 0, "hook engine unavailable"));

		auto result = g_hook_engine->queue_disable(m_name, m_target);
		if (!result)
		{
			RML_WARN("Failed to disable hook '{}': {}", m_name, result.error().describe());
			return result;
		}

		m_enabled = false;
		return {};
	}

	bool DetourHook::is_enabled() const
	{
		return m_enabled;
	}

	void DetourHook::fix_hook_address()
	{
		if (g_hook_engine)
			m_target = g_hook_engine->resolve_thunk(m_target);
	}
}
