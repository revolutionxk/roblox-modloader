#include "crash_dumper.hpp"
#include "stack_trace.hpp"

#include "RobloxModLoader/internal/platform.hpp"

#include "RobloxModLoader/memory/module_utils.hpp"

#include <chrono>
#include <dbghelp.h>
#include <filesystem>
#include <polyhook2/PE/IatHook.hpp>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

namespace fs = std::filesystem;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::time_point_cast;

RML_LOG_SCOPE("CrashDumper");

namespace rml::exception_filter
{
	CrashDumper::CrashDumper()
	{
		s_instance = this;
		g_crash_dumper = this;
	}

	CrashDumper::~CrashDumper()
	{
		disable();
		g_crash_dumper = nullptr;
		s_instance = nullptr;
	}

	void CrashDumper::enable()
	{
		if (m_enabled)
		{
			RML_WARN("Crash dumper already enabled");
			return;
		}

		try
		{
			SetErrorMode(SEM_FAILCRITICALERRORS);

			m_veh_handle = AddVectoredExceptionHandler(1, vectored_exception_handler);
			if (!m_veh_handle)
			{
				RML_WARN("Failed to register Vectored Exception Handler");
			}

			m_previous_exception_filter = SetUnhandledExceptionFilter(exception_handler);
			m_set_unhandled_exception_filter_hook = std::make_unique<PLH::IatHook>("kernel32.dll", "SetUnhandledExceptionFilter", std::bit_cast<uint64_t>(&hooked_set_unhandled_exception_filter), &m_hook_trampoline, L"");

			if (!m_set_unhandled_exception_filter_hook->hook())
			{
				RML_ERROR("Failed to hook SetUnhandledExceptionFilter");
				SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(m_previous_exception_filter));
				return;
			}

			m_enabled = true;
			RML_INFO("Crash dumper enabled");
		}
		catch (const std::exception& e)
		{
			RML_ERROR("Failed to enable crash dumper: {}", e.what());
		}
		catch (...)
		{
			RML_ERROR("Failed to enable crash dumper: unknown exception");
		}
	}

	void CrashDumper::disable()
	{
		if (!m_enabled)
		{
			return;
		}

		try
		{
			if (m_veh_handle)
			{
				RemoveVectoredExceptionHandler(m_veh_handle);
				m_veh_handle = nullptr;
			}

			if (m_set_unhandled_exception_filter_hook)
			{
				m_set_unhandled_exception_filter_hook->unHook();
				m_set_unhandled_exception_filter_hook.reset();
			}

			SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(m_previous_exception_filter));
			m_enabled = false;

			RML_INFO("Crash dumper disabled successfully");
		}
		catch (...)
		{
			RML_ERROR("Exception occurred while disabling crash dumper");
		}
	}

	void CrashDumper::set_full_memory_dump(bool enabled)
	{
		m_full_memory_dump = enabled;
		RML_INFO("Full memory dump {}", enabled ? "enabled" : "disabled");
	}

	LPTOP_LEVEL_EXCEPTION_FILTER WINAPI CrashDumper::hooked_set_unhandled_exception_filter(LPTOP_LEVEL_EXCEPTION_FILTER filter)
	{
		RML_WARN("Attempt to override exception handler blocked");
		return nullptr;
	}

	std::wstring CrashDumper::generate_dump_filename()
	{
		try
		{
			std::wstring dump_dir;
			if (auto current_path = fs::current_path(); !current_path.empty())
			{
				dump_dir = current_path.wstring();
			}
			else
			{
				wchar_t temp_path[MAX_PATH];
				if (GetTempPathW(MAX_PATH, temp_path) > 0)
				{
					dump_dir = temp_path;
				}
				else
				{
					dump_dir = L"C:\\";
				}
			}

			const auto crash_dir = fs::path(dump_dir) / "RobloxModLoader" / "crashes";
			std::error_code ec;
			fs::create_directories(crash_dir, ec);

			bool use_local_time = true;
#if defined(RML_WINDOWS)
			if (const auto module = GetModuleHandleW(L"ntdll.dll"); module && GetProcAddress(module, "wine_get_version"))
			{
				use_local_time = false;
			}
#endif

			const auto now = time_point_cast<seconds>(system_clock::now());
			const auto time_t = system_clock::to_time_t(now);
			std::tm tm{};
			const bool converted = (use_local_time ? localtime_s(&tm, &time_t) : gmtime_s(&tm, &time_t)) == 0;
			const auto filename = converted
			    ? std::format(L"crash_{:04d}_{:02d}_{:02d}_{:02d}_{:02d}_{:02d}.dmp", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec)
			    : std::format(L"crash_{}.dmp", now.time_since_epoch().count());

			return (crash_dir / filename).wstring();
		}
		catch (...)
		{
			const auto timestamp = std::chrono::duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
			return std::format(L"crash_{}.dmp", timestamp);
		}
	}

	bool CrashDumper::create_minidump(PEXCEPTION_POINTERS exception_pointers, const std::wstring& dump_path) const
	{
		try
		{
			const HANDLE file = CreateFileW(dump_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				const auto error = GetLastError();
				RML_ERROR("Failed to create crash dump file {}: {}", std::filesystem::path(dump_path).string(), error);
				return false;
			}

			MINIDUMP_EXCEPTION_INFORMATION exception_info{};
			exception_info.ThreadId = GetCurrentThreadId();
			exception_info.ExceptionPointers = exception_pointers;
			exception_info.ClientPointers = FALSE;

			const int additional_flags = m_full_memory_dump ? MiniDumpWithFullMemory | MiniDumpIgnoreInaccessibleMemory : 0;
			const auto dump_type = static_cast<MINIDUMP_TYPE>(DEFAULT_DUMP_TYPE | additional_flags);

			const BOOL result = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, dump_type, &exception_info, nullptr, nullptr);

			CloseHandle(file);

			if (!result)
			{
				const auto error = GetLastError();
				RML_ERROR("Failed to write crash dump: {}", error);
				return false;
			}

			RML_INFO("Crash dump successfully written to: {}", std::filesystem::path(dump_path).string());
			return true;
		}
		catch (...)
		{
			RML_ERROR("Exception occurred while creating crash dump");
			return false;
		}
	}

	LONG WINAPI CrashDumper::vectored_exception_handler(PEXCEPTION_POINTERS exception_pointers)
	{
		const DWORD code = exception_pointers->ExceptionRecord->ExceptionCode;
		constexpr DWORD NON_FATAL_CODES[] = {0xE06D7363, 0xE0434352, 0x04242420, EXCEPTION_BREAKPOINT, EXCEPTION_SINGLE_STEP, DBG_PRINTEXCEPTION_C, DBG_PRINTEXCEPTION_WIDE_C, 0x406D1388, 0x000006BA, 0xC0000135, 0xC0000138, 0xC0000139};
		for (const DWORD non_fatal : NON_FATAL_CODES)
		{
			if (code == non_fatal)
			{
				return EXCEPTION_CONTINUE_SEARCH;
			}
		}

		thread_local bool in_veh = false;
		if (in_veh)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
		in_veh = true;

		try
		{
			RML_ERROR("=== [VEH] CRITICAL EXCEPTION DETECTED (before Roblox handler) ===");
			RML_ERROR("Exception Code: 0x{:08X}", code);
			RML_ERROR("Exception Address: 0x{:016X}", reinterpret_cast<uintptr_t>(exception_pointers->ExceptionRecord->ExceptionAddress));

			const auto dump_path = generate_dump_filename();
			if (s_instance)
			{
				s_instance->create_minidump(exception_pointers, dump_path);
			}
			log_exception_info(exception_pointers);
			log_register_state(exception_pointers->ContextRecord);
			log_stack_trace();

			MessageBoxW(nullptr,
			    std::format(L"Roblox ModLoader has encountered a critical error before the main exception handler could process it.\n\n"
			                L"A crash dump has been created at:\n{}\n\n"
			                L"Please send this file along with the log files to the developer for analysis.\n\n"
			                L"The application will now exit.",
			        dump_path)
			        .c_str(),
			    L"RobloxModLoader - Critical Error",
			    MB_OK | MB_ICONERROR);

			try
			{
				global_logger()->flush();
			}
			catch (...)
			{
			}
		}
		catch (...)
		{
		}

		in_veh = false;

		return EXCEPTION_CONTINUE_SEARCH;
	}

	LONG WINAPI CrashDumper::exception_handler(PEXCEPTION_POINTERS exception_pointers)
	{
		thread_local bool in_exception_handler = false;
		if (in_exception_handler)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
		in_exception_handler = true;

		try
		{
			RML_ERROR("=== CRITICAL EXCEPTION DETECTED ===");
			RML_ERROR("Exception Code: 0x{:08X}", exception_pointers->ExceptionRecord->ExceptionCode);
			RML_ERROR("Exception Address: 0x{:016X}", reinterpret_cast<uintptr_t>(exception_pointers->ExceptionRecord->ExceptionAddress));

			const auto dump_path = generate_dump_filename();
			const bool dump_created = s_instance && s_instance->create_minidump(exception_pointers, dump_path);

			log_exception_info(exception_pointers);
			log_register_state(exception_pointers->ContextRecord);
			log_stack_trace();

			try
			{
				global_logger()->flush();
			}
			catch (...)
			{
			}

			std::wstring message;
			if (dump_created)
			{
				message = std::format(L"Roblox ModLoader has encountered a critical error!\n\n"
				                      L"A crash dump has been created at:\n{}\n\n"
				                      L"Please send this file along with the log files to the developer for analysis.\n\n"
				                      L"The application will now exit.",
				    dump_path);
			}
			else
			{
				message = L"Roblox ModLoader has encountered a critical error!\n\n"
				          L"Failed to create crash dump, but error details have been logged.\n"
				          L"Please check the log files and send them to the developer.\n\n"
				          L"The application will now exit.";
			}

			MessageBoxW(nullptr, message.c_str(), L"RobloxModLoader - Critical Error", MB_OK | MB_ICONERROR);
		}
		catch (...)
		{
			try
			{
				MessageBoxW(nullptr,
				    L"RobloxModLoader encountered a critical error during exception handling.\n"
				    L"Please check log files for details.",
				    L"RobloxModLoader - Fatal Error",
				    MB_OK | MB_ICONERROR);
			}
			catch (...)
			{
			}
		}

		in_exception_handler = false;
		return EXCEPTION_EXECUTE_HANDLER;
	}

	void CrashDumper::log_exception_info(PEXCEPTION_POINTERS exception_pointers)
	{
		try
		{
			const auto* context = exception_pointers->ContextRecord;
			const auto* exception_record = exception_pointers->ExceptionRecord;

			RML_ERROR("=== EXCEPTION INFORMATION ===");
			RML_ERROR("Thread RIP: 0x{:016X}", context->Rip);

			const auto our_module_base = reinterpret_cast<uintptr_t>(g_hinstance);
			const auto rebased_our_rip = context->Rip - our_module_base;
			RML_ERROR("roblox_modloader.dll Base: 0x{:016X}", our_module_base);
			RML_ERROR("Rebased ModLoader: 0x{:016X}", rebased_our_rip);

			if (const auto roblox_base = memory::module_utils::get_roblox_studio_base(); roblox_base != 0)
			{
				const auto rebased_rip = context->Rip - roblox_base;
				RML_ERROR("RobloxStudioBeta.exe Base: 0x{:016X}", roblox_base);
				RML_ERROR("Rebased Studio: 0x{:016X}", rebased_rip);
			}
			const auto exception_addr = reinterpret_cast<uintptr_t>(exception_record->ExceptionAddress);
			if (const auto module_name = memory::module_utils::get_module_name_from_address(exception_addr);
			    !module_name.empty())
			{
				const auto module_base = memory::module_utils::get_module_base_address(module_name);
				const auto rebased_addr = exception_addr - module_base;

				RML_ERROR("Exception Module: {} @ 0x{:016X}", module_name, module_base);
				RML_ERROR("Exception Rebased Address: 0x{:016X}", rebased_addr);
			}

			RML_ERROR("Exception Flags: 0x{:08X}", exception_record->ExceptionFlags);
			RML_ERROR("Number of Parameters: {}", exception_record->NumberParameters);
			for (DWORD i = 0; i < exception_record->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
			{
				RML_ERROR("Parameter {}: 0x{:016X}", i, exception_record->ExceptionInformation[i]);
			}
		}
		catch (...)
		{
			RML_ERROR("Failed to log exception info safely");
		}
	}

	void CrashDumper::log_register_state(PCONTEXT context)
	{
		try
		{
			RML_ERROR("=== REGISTER STATE ===");

			RML_ERROR("-- GENERAL PURPOSE REGISTERS --");
			RML_ERROR("RAX: 0x{:016X}", context->Rax);
			RML_ERROR("RBX: 0x{:016X}", context->Rbx);
			RML_ERROR("RCX: 0x{:016X}", context->Rcx);
			RML_ERROR("RDX: 0x{:016X}", context->Rdx);
			RML_ERROR("RDI: 0x{:016X}", context->Rdi);
			RML_ERROR("RSI: 0x{:016X}", context->Rsi);
			RML_ERROR("R08: 0x{:016X}", context->R8);
			RML_ERROR("R09: 0x{:016X}", context->R9);
			RML_ERROR("R10: 0x{:016X}", context->R10);
			RML_ERROR("R11: 0x{:016X}", context->R11);
			RML_ERROR("R12: 0x{:016X}", context->R12);
			RML_ERROR("R13: 0x{:016X}", context->R13);
			RML_ERROR("R14: 0x{:016X}", context->R14);
			RML_ERROR("R15: 0x{:016X}", context->R15);

			RML_ERROR("-- STACK POINTERS --");
			RML_ERROR("RBP: 0x{:016X}", context->Rbp);
			RML_ERROR("RSP: 0x{:016X}", context->Rsp);
			RML_ERROR("RIP: 0x{:016X}", context->Rip);

			RML_ERROR("-- FLAGS --");
			RML_ERROR("RFLAGS: 0x{:016X}", context->EFlags);
		}
		catch (...)
		{
			RML_ERROR("Failed to log register state safely");
		}
	}

	void CrashDumper::log_stack_trace()
	{
		exception_filter::log_stack_trace({.heading = "STACK TRACE", .frame = "Stack Frame", .chain = "Stack Chain"});
	}

	std::unique_ptr<ICrashHandler> create_crash_handler()
	{
		return std::make_unique<CrashDumper>();
	}
}
