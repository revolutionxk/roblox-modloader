#pragma once

#include "RobloxModLoader/hooking/i_hook.hpp"
#include "RobloxModLoader/rml_export.hpp"

#include <expected>
#include <string>

namespace rml
{
	class RML_EXPORT DetourHook : public IHook
	{
	public:
		explicit DetourHook();
		explicit DetourHook(const std::string& name, void* detour);
		explicit DetourHook(const std::string& name, void* target, void* detour);

		~DetourHook() noexcept override;
		DetourHook(DetourHook&& that) = delete;
		DetourHook& operator=(DetourHook&& that) = delete;
		DetourHook(DetourHook const&) = delete;
		DetourHook& operator=(DetourHook const&) = delete;

		void set_instance(const std::string& name, void* detour);
		void set_instance(const std::string& name, void* target, void* detour);
		void set_target_and_create_hook(void* target);
		void destroy();

		[[nodiscard]] std::expected<void, rml::HookError> enable() override;
		[[nodiscard]] std::expected<void, rml::HookError> disable() override;
		[[nodiscard]] bool is_enabled() const override;

		template<typename T>
		T get_original()
		{
			return reinterpret_cast<T>(m_original);
		}

		void** get_original_ptr()
		{
			return &m_original;
		}

		[[nodiscard]] const std::string& name() const
		{
			return m_name;
		}

		void fix_hook_address();

	private:
		void create_hook();

		std::string m_name;
		void* m_original{};
		void* m_target{};
		void* m_detour{};
		bool m_enabled{};
	};
}
