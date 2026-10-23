#pragma once

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include "RobloxModLoader/exception/i_crash_handler.hpp"

#include <Windows.h>
#include <bit>
#include <cstdint>
#include <dbghelp.h>
#include <memory>
#include <string>

namespace PLH
{
	class IatHook;
}

namespace rml::exception_filter
{
	class CrashDumper final : public ICrashHandler
	{
		static constexpr int DEFAULT_DUMP_TYPE = MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithModuleHeaders | MiniDumpWithAvxXStateContext;

		bool m_enabled = false;
		bool m_full_memory_dump = false;
		void* m_previous_exception_filter = nullptr;
		void* m_veh_handle = nullptr;
		std::unique_ptr<PLH::IatHook> m_set_unhandled_exception_filter_hook;
		uint64_t m_hook_trampoline = 0;

	public:
		CrashDumper();

		~CrashDumper() override;

		CrashDumper(const CrashDumper&) = delete;

		CrashDumper& operator=(const CrashDumper&) = delete;

		CrashDumper(CrashDumper&&) = delete;

		CrashDumper& operator=(CrashDumper&&) = delete;

		void enable() override;

		void disable() override;

		void set_full_memory_dump(bool enabled) override;

		[[nodiscard]] bool is_enabled() const override
		{
			return m_enabled;
		}

	private:
		// Self-pointer for the static SEH/VEH callbacks, which need the concrete instance (the portable
		// g_crash_dumper is typed as the interface).
		static inline CrashDumper* s_instance = nullptr;

		static LONG WINAPI exception_handler(PEXCEPTION_POINTERS exception_pointers);

		static LONG WINAPI vectored_exception_handler(PEXCEPTION_POINTERS exception_pointers);

		static LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hooked_set_unhandled_exception_filter(LPTOP_LEVEL_EXCEPTION_FILTER filter);

		static HMODULE module_at(const void* address);

		static bool raised_in_owned_code(const void* address);

		bool create_minidump(PEXCEPTION_POINTERS exception_pointers, const std::wstring& dump_path) const;

		static std::wstring generate_dump_filename();

		static void log_exception_info(PEXCEPTION_POINTERS exception_pointers);

		static void log_register_state(PCONTEXT context);

		static void log_stack_trace();
	};
}
