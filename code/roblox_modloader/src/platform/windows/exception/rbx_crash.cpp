#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/memory/module_utils.hpp"
#include "stack_trace.hpp"

void try_capture_cpp_exception()
{
	try
	{
		std::rethrow_exception(std::current_exception());
	}
	catch (const std::exception& ex)
	{
		LOG_ERROR("Roblox Studio invoked RBXCRASH with C++ exception!");
		LOG_ERROR("C++ Exception details: '{}'", ex.what());
	}
	catch (...)
	{
		LOG_ERROR("Roblox Studio invoked RBXCRASH with unknown C++ exception type");
	}
}

void rml::Hooks::rbx_crash(const char* type, const char* message)
{
	const auto function_name = "hooks::rbx_crash";

	const char* crash_type = (type != nullptr) ? type : "Unknown";
	const char* crash_description = (message != nullptr) ? message : "Not described";

	LOG_ERROR("=== ROBLOX CRASH TRIGGERED ===");
	LOG_ERROR("CRASH TYPE: {}", crash_type);
	LOG_ERROR("CRASH DESCRIPTION: {}", crash_description);

	const auto roblox_base = memory::module_utils::get_roblox_studio_base();
	const auto our_base = reinterpret_cast<uintptr_t>(g_hinstance);

	LOG_ERROR("RobloxStudioBeta.exe Base: 0x{:016X}", roblox_base);
	LOG_ERROR("roblox_modloader.dll Base: 0x{:016X}", our_base);

	try_capture_cpp_exception();

	rml::exception_filter::log_stack_trace({.heading = "ROBLOX CRASH STACK TRACE", .frame = "Crash Frame", .chain = "Crash Stack Chain"});

	LOG_ERROR("=== END ROBLOX CRASH INFORMATION ===");
	LOG_ERROR("Crash handling completed. Please send the log file to the developer.");

	global_logger()->flush();

	const std::string message_text = std::format("Roblox Studio has invoked RBXCRASH!\n\n"
	                                             "Crash Type: {}\n"
	                                             "Description: {}\n\n"
	                                             "Detailed crash information has been logged.\n"
	                                             "The application will continue after a brief pause.",
	    crash_type,
	    crash_description);

	MessageBoxA(nullptr, message_text.c_str(), "RobloxModLoader - Roblox Crash Detected", MB_OK | MB_ICONERROR);

	LOG_ERROR("Waiting 5 seconds before allowing Roblox to continue...");
	std::this_thread::sleep_for(std::chrono::seconds(5));

	LOG_INFO("Roblox crash handler completed, allowing execution to continue");
}
