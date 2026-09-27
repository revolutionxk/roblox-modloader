#include "RobloxModLoader/internal/common.hpp"
#include "luau_waiting_script_job.hpp"

#include "RobloxModLoader/luau/script_host.hpp"
#include "RobloxModLoader/luau/script_runtime.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "RobloxModLoader/roblox/waiting_hybrid_scripts_job.hpp"

RML_LOG_SCOPE("LuauWaitingScriptJob");

namespace rml::jobs
{
	LuauWaitingScriptJob::LuauWaitingScriptJob() noexcept
		: JobBase(JOB_NAME, JobPriority::High, JobKind::WaitingHybridScripts, true)
	{
	}

	static luau::ScriptHost* host_for_job(const JobExecutionContext& context)
	{
		auto* runtime = luau::script_runtime();
		if (!runtime)
		{
			return nullptr;
		}

		const auto data_model = RBX::DataModel::from_job(context.job_as<RBX::DataModelJob>());
		if (!data_model)
		{
			return nullptr;
		}

		// A host serves the DataModel the watcher last bound for its type. A new DataModel steps
		// before the watcher rebinds, and pumping the old host there would run its work against
		// the Lua VM that died with the previous DataModel.
		if (!has_task_scheduler() || task_scheduler().get_data_model_by_type(data_model->type) != data_model)
		{
			return nullptr;
		}

		return runtime->host(data_model->type);
	}

	bool LuauWaitingScriptJob::should_execute_impl(const JobExecutionContext& context) noexcept
	{
		const auto* host = host_for_job(context);
		return host != nullptr && host->has_pending();
	}

	void LuauWaitingScriptJob::execute_impl(const JobExecutionContext& context)
	{
		if (auto* host = host_for_job(context))
		{
			host->pump(luau::Budget{});
		}
	}

	void LuauWaitingScriptJob::destroy_impl() noexcept
	{
	}
}
