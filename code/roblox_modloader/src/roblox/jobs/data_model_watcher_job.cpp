#include "data_model_watcher_job.hpp"

#include "../../mod/mod_manager.hpp"
#include "RobloxModLoader/internal/common.hpp"
#if RML_ENABLE_LUAU
	#include "RobloxModLoader/luau/script_host.hpp"
	#include "RobloxModLoader/luau/script_runtime.hpp"
#endif
#include "RobloxModLoader/roblox/data_model.hpp"
#include "RobloxModLoader/roblox/script_context.hpp"
#include "RobloxModLoader/roblox/security/script_permissions.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "RobloxModLoader/roblox/waiting_hybrid_scripts_job.hpp"
#include "dotnet/dotnet_mod_loader.hpp"
#include "pointers.hpp"

namespace rml::jobs
{
	DataModelWatcherJob::DataModelWatcherJob() noexcept :
	    JobBase(JOB_NAME, JobPriority::High, JobKind::WaitingHybridScripts, true)
	{
	}

	bool DataModelWatcherJob::should_execute_impl(const JobExecutionContext& context) noexcept
	{
		const auto data_model = RBX::DataModel::from_job(context.job_as<RBX::DataModelJob>());
		if (!data_model)
		{
			return false;
		}

		const auto type = data_model->type;

		m_data_models[type] = data_model;
		m_data_model_last_time_stepped[type] = std::chrono::high_resolution_clock::now();

		return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_last_check)
		           .count()
		    > 16;
	}

	void DataModelWatcherJob::execute_impl(const JobExecutionContext& context)
	{
		m_last_check = std::chrono::high_resolution_clock::now();

		const auto job = context.job_as<RBX::ScriptContextFacets::WaitingHybridScriptsJob>();
		const auto new_data_model = RBX::DataModel::from_job(job);
		if (!new_data_model)
		{
			return;
		}

		const auto old_data_model = task_scheduler().get_data_model_by_type(new_data_model->type);
		if (old_data_model == new_data_model)
		{
			return;
		}

		check_and_cleanup_stale_data_models();

		on_data_model_changed(old_data_model, new_data_model, job->script_context);
	}

	void DataModelWatcherJob::destroy_impl() noexcept
	{
	}

	void DataModelWatcherJob::on_data_model_changed(const RBX::DataModel* old_data_model, RBX::DataModel* new_data_model, RBX::ScriptContext* script_context)
	{
		if (!has_task_scheduler())
		{
			LOG_ERROR("TaskScheduler is null, cannot set new DataModel.");
			return;
		}

		if (old_data_model == new_data_model)
		{
			return;
		}

		LOG_INFO("DataModel changed from 0x{:X} to 0x{:X} by {}",
		    old_data_model ? reinterpret_cast<uintptr_t>(old_data_model) : 0,
		    new_data_model ? reinterpret_cast<uintptr_t>(new_data_model) : 0,
		    new_data_model ? std::to_underlying(new_data_model->type) : 0);

		task_scheduler().set_data_model(new_data_model->type, new_data_model, script_context);

		const auto data_model_type = new_data_model->type;

		LOG_INFO("New DataModel type: {}, notifying mods and scripts", std::to_underlying(data_model_type));

		events::DataModelChangedEvent ev(reinterpret_cast<u64>(old_data_model), reinterpret_cast<u64>(new_data_model), static_cast<int>(data_model_type));
		events::event_manager().emit(ev);

#if RML_ENABLE_LUAU
		if (auto* runtime = luau::script_runtime())
		{
			if (script_context)
			{
				if (const auto lua_state = script_context->get_global_state(RBX::Security::Identity::RobloxEngine))
				{
					runtime->bind_host(data_model_type, lua_state);
					runtime->activate(data_model_type);
				}
				else
				{
					LOG_WARN("No global Lua state for DataModel type {}; scripts will not run", std::to_underlying(data_model_type));
				}
			}
		}
#endif

		// Notify managed (.NET) mods about the change if the bridge is initialized
		if (dotnet::g_dotnet_mod_loader)
		{
			try
			{
				dotnet::g_dotnet_mod_loader->notify_data_model_changed(
				    reinterpret_cast<u64>(static_cast<const RBX::Instance*>(old_data_model)),
				    reinterpret_cast<u64>(static_cast<RBX::Instance*>(new_data_model)), static_cast<int>(data_model_type));
			}
			catch (const std::exception& e)
			{
				LOG_WARN("Failed to notify managed mods of DataModel change: {}", e.what());
			}
		}

		// if (g_mod_manager)
		// {
		// 	try
		// 	{
		// 		g_mod_manager->notify_managed_datamodel_changed(old_data_model, new_data_model, data_model_type);
		// 	}
		// 	catch (const std::exception& e)
		// 	{
		// 		LOG_ERROR("Failed to notify managed mods of DataModel change: {}", e.what());
		// 	}
		// }

#if RML_ENABLE_LUAU
		if (auto* runtime = luau::script_runtime())
		{
			if (auto* host = runtime->host(data_model_type))
			{
				host->pump(luau::Budget{.max_items = 256, .max_time = std::chrono::milliseconds{250}});
			}
		}
#endif
	}

	void DataModelWatcherJob::check_and_cleanup_stale_data_models()
	{
		const auto now = std::chrono::high_resolution_clock::now();
		constexpr auto stale_threshold = std::chrono::seconds{5};

		std::vector<RBX::DataModelType> stale_types;
		for (auto it = m_data_model_last_time_stepped.begin(); it != m_data_model_last_time_stepped.end();)
		{
			const auto& [data_model_type, last_time] = *it;

			if (const auto time_since_last_step = now - last_time; time_since_last_step <= stale_threshold)
			{
				++it;
				continue;
			}

			const auto current_data_model = task_scheduler().get_data_model_by_type(data_model_type);
			const auto tracked_data_model = m_data_models.find(data_model_type);

			bool should_cleanup = false;

			if (current_data_model)
			{
				if (tracked_data_model != m_data_models.end() && tracked_data_model->second != current_data_model)
				{
					should_cleanup = true;
				}
			}
			else
			{
				should_cleanup = true;
			}

			if (!should_cleanup)
			{
				++it;
				continue;
			}

			LOG_INFO("Detected stale DataModel type: {}, cleaning up", std::to_underlying(data_model_type));

			stale_types.push_back(data_model_type);
			m_data_models.erase(data_model_type);
			it = m_data_model_last_time_stepped.erase(it);
		}

		for (const auto& stale_type : stale_types)
		{
			task_scheduler().cleanup_data_model(stale_type);
		}
	}
}
