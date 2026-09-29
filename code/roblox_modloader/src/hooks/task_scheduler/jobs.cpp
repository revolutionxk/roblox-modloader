#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/internal/hooking/engine_hooks.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "RobloxModLoader/roblox/job_vtable.hpp"
#include "RobloxModLoader/roblox/task_scheduler.job.hpp"

#include <unordered_map>
#include <utility>

RBX::TaskScheduler::StepResult rml::Hooks::on_job_step(RBX::DataModelJob* job, const RBX::Stats& time_metrics)
{
	if (!job)
		return RBX::TaskScheduler::StepResult::Stepped;

	const auto vtable = memory::vtable_of(job);
	const auto detected_kind = [&]() -> rml::JobKind {
		if (!rml::has_task_scheduler())
		{
			LOG_ERROR("[hooks::on_job_step] Task scheduler is not initialized, cannot determine job kind.");
			return rml::JobKind::Heartbeat;
		}

		const auto kind = rml::task_scheduler().get_job_kind_from_vtable(vtable);
		if (!kind.has_value())
			return rml::JobKind::Heartbeat;

		return *kind;
	}();

	if (rml::has_task_scheduler() && !rml::task_scheduler().is_shutdown())
	{
		try
		{
			const rml::JobExecutionContext context{.kind = detected_kind,
			    .job = job,
			    .stats = &time_metrics,
			    .delta_time = 0.0};

			rml::task_scheduler().execute_jobs_for_kind(context);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("[hooks::on_job_step] Exception executing custom jobs for kind {}: {}", std::to_underlying(detected_kind), e.what());
		}
		catch (...)
		{
			LOG_ERROR("[hooks::on_job_step] Unknown exception executing custom jobs for kind {}", std::to_underlying(detected_kind));
		}
	}

	if (const auto it = g_hooking->m_jobs_hook.find(detected_kind); it != g_hooking->m_jobs_hook.end() && it->second)
	{
		return it->second->get_original<decltype(&on_job_step)>(rml::job_step_slot())(job, time_metrics);
	}

	LOG_WARN("[hooks::on_job_step] No hook found for job kind {}, returning Stepped", std::to_underlying(detected_kind));
	return RBX::TaskScheduler::StepResult::Stepped;
}

