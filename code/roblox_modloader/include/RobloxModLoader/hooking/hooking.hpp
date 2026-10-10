#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/roblox/job_types.hpp"
#include "detour_hook.hpp"
#include "i_hook_engine.hpp"
#include "vtable_hook.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rml
{
	struct Hooks;

	class Hooking
	{
		friend Hooks;

	public:
		explicit Hooking();

		~Hooking();

		void enable();

		void disable();

		class RML_EXPORT DetourHookHelper
		{
			friend Hooking;

			using ret_ptr_fn = std::function<void*()>;

			ret_ptr_fn m_on_hooking_available = nullptr;

			DetourHook* m_detour_hook;

			void enable_hook_if_hooking_is_already_running() const;
			void enable_now() const;

			static void register_helper(const DetourHookHelper& helper);
			static void disable_hook(DetourHook& hook);
			static void unregister_helper(DetourHook& hook);

			template<auto detour_function>
			struct hook_to_detour_hook_helper
			{
				static inline DetourHook m_detour_hook;
			};

		public:
			template<auto detour_function>
			static void add(const std::string& name, void* target)
			{
				hook_to_detour_hook_helper<detour_function>::m_detour_hook.set_instance(name, target, reinterpret_cast<void*>(detour_function));

				DetourHookHelper d{};
				d.m_detour_hook = &hook_to_detour_hook_helper<detour_function>::m_detour_hook;

				register_helper(d);
			}

			template<auto detour_function>
			static void add_now(const std::string& name, void* target)
			{
				hook_to_detour_hook_helper<detour_function>::m_detour_hook.set_instance(name, target, reinterpret_cast<void*>(detour_function));
				DetourHookHelper d{};
				d.m_detour_hook = &hook_to_detour_hook_helper<detour_function>::m_detour_hook;
				d.enable_now();
				m_detour_hook_helpers.push_back(d);
			}

			template<auto detour_function>
			static void* add_lazy(const std::string& name, DetourHookHelper::ret_ptr_fn on_hooking_available)
			{
				hook_to_detour_hook_helper<detour_function>::m_detour_hook.set_instance(name, reinterpret_cast<void*>(detour_function));

				DetourHookHelper d{};
				d.m_detour_hook = &hook_to_detour_hook_helper<detour_function>::m_detour_hook;
				d.m_on_hooking_available = on_hooking_available;

				register_helper(d);

				return nullptr;
			}

			template<auto detour_function>
			static void disable()
			{
				disable_hook(hook_to_detour_hook_helper<detour_function>::m_detour_hook);
			}

			template<auto detour_function>
			static void remove()
			{
				unregister_helper(hook_to_detour_hook_helper<detour_function>::m_detour_hook);
			}

			~DetourHookHelper();
		};

		template<auto detour_function>
		static auto get_original()
		{
			return DetourHookHelper::hook_to_detour_hook_helper<detour_function>::m_detour_hook.template get_original<decltype(detour_function)>();
		}

	private:
		bool m_enabled{};
		std::unique_ptr<IHookEngine> m_hook_engine;
		bool m_owns_engine{};
		std::unordered_map<rml::JobKind, std::unique_ptr<vtable_hook> > m_jobs_hook;

		static inline std::vector<DetourHookHelper> m_detour_hook_helpers;
	};
}

inline rml::Hooking* g_hooking{};
