#include "stack_trace.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module_utils.hpp"
#include "RobloxModLoader/util/string.hpp"

#include <array>
#include <format>
#include <vector>

#include <dbghelp.h>

#pragma comment(lib, "dbghelp.lib")

RML_LOG_SCOPE("StackTrace");

namespace rml::exception_filter
{
	static bool resolve_symbol(const HANDLE process, const DWORD64 address, char* name, const std::size_t name_size, DWORD64* displacement)
	{
		__try
		{
			char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME];
			auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = MAX_SYM_NAME;

			if (!SymFromAddr(process, address, displacement, symbol))
				return false;

			strncpy_s(name, name_size, symbol->Name, _TRUNCATE);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	static void log_frame(const HANDLE process, const StackTraceLabels& labels, const WORD index, const DWORD64 address)
	{
		const auto module_name = memory::module_utils::get_module_name_from_address(address);
		const auto rebased = memory::module_utils::get_roblox_studio_rebased_address(address, memory::module_utils::get_roblox_studio_base());

		char name[MAX_SYM_NAME] = {};
		DWORD64 displacement = 0;
		if (resolve_symbol(process, address, name, sizeof(name), &displacement))
			RML_ERROR("[{} {}] Inside {} @ 0x{:016X} ({}) | Studio Rebase: 0x{:016X} | Displacement: +0x{:X}", labels.frame, index, name, address, module_name, rebased, displacement);
		else
			RML_ERROR("[{} {}] Unknown Subroutine @ 0x{:016X} ({}) | Studio Rebase: 0x{:016X}", labels.frame, index, address, module_name, rebased);
	}

	void log_stack_trace(const StackTraceLabels& labels)
	{
		try
		{
			RML_ERROR("=== {} ===", labels.heading);

			HANDLE process = nullptr;
			if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &process, PROCESS_ALL_ACCESS, FALSE, 0))
			{
				RML_ERROR("Failed to duplicate process handle: {}", GetLastError());
				process = GetCurrentProcess();
			}

			if (!SymInitialize(process, nullptr, TRUE))
			{
				RML_ERROR("Failed to initialize symbol handler: {}", GetLastError());
				if (process != GetCurrentProcess())
					CloseHandle(process);
				return;
			}

			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);

			std::array<void*, 256> stack{};
			const auto frame_count = RtlCaptureStackBackTrace(1, static_cast<DWORD>(stack.size() - 1), stack.data(), nullptr);
			RML_ERROR("Captured {} stack frames:", frame_count);

			std::vector<std::string> chain;
			chain.reserve(frame_count);
			for (WORD i = 0; i < frame_count; ++i)
			{
				const auto address = reinterpret_cast<DWORD64>(stack[i]);
				log_frame(process, labels, i, address);
				chain.push_back(std::format("{:#x}", address));
			}

			RML_ERROR("{}: {}", labels.chain, chain.empty() ? std::string{"No stack trace available"} : utils::join(chain, " -> "));

			SymCleanup(process);
			if (process != GetCurrentProcess())
				CloseHandle(process);
		}
		catch (...)
		{
			RML_ERROR("Failed to log stack trace safely");
		}
	}
}
