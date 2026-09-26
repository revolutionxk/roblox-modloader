#include "application.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/version.hpp"
#include "RobloxModLoader/qt/qt_integration.hpp"
#include "subsystems/config_subsystem.hpp"
#include "subsystems/init_gate_subsystem.hpp"
#include "subsystems/logger_subsystem.hpp"
#include "subsystems/crash_dumper_subsystem.hpp"
#include "subsystems/engine_signatures_subsystem.hpp"
#include "subsystems/event_manager_subsystem.hpp"
#include "subsystems/qt_integration_subsystem.hpp"
#include "subsystems/rtti_manager_subsystem.hpp"
#include "subsystems/pointers_subsystem.hpp"
#include "subsystems/task_scheduler_subsystem.hpp"
#include "subsystems/job_manager_subsystem.hpp"
#include "subsystems/hooking_subsystem.hpp"
#include "subsystems/script_subsystem_adapter.hpp"
#include "subsystems/mod_manager_subsystem.hpp"

RML_LOG_SCOPE("Application");

namespace rml
{
	using namespace std::chrono_literals;

	Application::Application() = default;

	Application::~Application()
	{
		if (!m_shutdown_complete)
		{
			shutdown();
		}
	}

	std::expected<void, SubsystemError> Application::initialize()
	{
		RML_INFO("Initializing Roblox Mod Loader {}...", version::string());

		auto event_manager_subsystem = std::make_unique<EventManagerSubsystem>();
		auto& event_manager_subsystem_ref = *event_manager_subsystem;

		auto task_scheduler_subsystem = std::make_unique<TaskSchedulerSubsystem>();
		auto& task_scheduler_subsystem_ref = *task_scheduler_subsystem;

		m_subsystems.push_back(std::make_unique<ConfigSubsystem>());
		m_subsystems.push_back(std::make_unique<LoggerSubsystem>());
		m_subsystems.push_back(std::make_unique<PointersSubsystem>());
		m_subsystems.push_back(std::make_unique<RttiManagerSubsystem>());
		m_subsystems.push_back(std::make_unique<InitGateSubsystem>());
		m_subsystems.push_back(std::make_unique<EngineSignaturesSubsystem>());
		m_subsystems.push_back(std::make_unique<CrashDumperSubsystem>());
		m_subsystems.push_back(std::move(event_manager_subsystem));
		m_subsystems.push_back(std::make_unique<QtIntegrationSubsystem>());
		m_subsystems.push_back(std::move(task_scheduler_subsystem));
		m_subsystems.push_back(std::make_unique<JobManagerSubsystem>(task_scheduler_subsystem_ref));
		m_subsystems.push_back(std::make_unique<HookingSubsystem>());
		m_subsystems.push_back(std::make_unique<ScriptSubsystemAdapter>());
		m_subsystems.push_back(std::make_unique<ModManagerSubsystem>(event_manager_subsystem_ref));

		m_shutdown_complete = false;

		for (const auto& subsystem : m_subsystems)
		{
			RML_INFO("Initializing subsystem: {}", subsystem->name());

			if (auto result = subsystem->initialize(); !result)
			{
				RML_ERROR("Failed to initialize subsystem {}: {}", subsystem->name(), result.error().message);
				if (const auto gate = InitGate::instance())
					gate->abort();
				return std::unexpected(result.error());
			}

			RML_INFO("Subsystem initialized: {}", subsystem->name());
		}

		g_hooking->enable();
		RML_INFO("Hooking enabled.");

		if (const auto gate = InitGate::instance())
			gate->mark_mods_loaded();

		if (qt::QtIntegration::instance()->ensure_action_hook())
		{
			RML_INFO("Qt action hook installed.");
		}
		else
		{
			RML_WARN("Qt action hook not installed yet (Qt not resolvable); will retry on demand.");
		}

		return {};
	}

	void Application::run()
	{
		g_running = true;

		while (g_running)
		{
			if (const auto qt_integration = qt::QtIntegration::instance(); qt_integration && !qt_integration->is_action_hook_ready())
			{
				qt_integration->ensure_action_hook();
			}

			std::this_thread::sleep_for(1s);
		}
	}

	void Application::shutdown()
	{
		for (auto& subsystem : m_subsystems | std::views::reverse)
		{
			RML_INFO("Shutting down subsystem: {}", subsystem->name());
			subsystem->shutdown();
		}

		m_subsystems.clear();
		m_shutdown_complete = true;
	}
}
