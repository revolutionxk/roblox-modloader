#include "instance_collector_job.hpp"

#include "instance_handles.hpp"

namespace rml::dotnet
{
	InstanceCollectorJob::InstanceCollectorJob() noexcept :
	    JobBase(JOB_NAME, JobPriority::Low, JobKind::WaitingHybridScripts, true)
	{
	}

	bool InstanceCollectorJob::should_execute_impl(const JobExecutionContext&) noexcept
	{
		return instance_handles().has_released();
	}

	void InstanceCollectorJob::execute_impl(const JobExecutionContext&)
	{
		instance_handles().collect();
	}

	void InstanceCollectorJob::destroy_impl() noexcept
	{
	}
}
