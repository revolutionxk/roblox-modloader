#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <memory>
#include <string>

namespace RBX
{
	class TaskSchedulerArbiter;

	struct Stats
	{
		double now;
		double last_step;
		double delta_time;
	};

	struct Error
	{
		double error;
	};

	enum StepResult : int
	{
		Done,
		Stepped,
	};

	class TaskSchedulerJob
	{
	public:
		virtual bool try_job_again()
		{
			return false;
		}

		virtual StepResult step(const Stats& stats) = 0;

		virtual void on_pre_step()
		{
		}

		virtual void on_post_step()
		{
		}

		virtual void on_added()
		{
		}

		virtual int get_desired_worker_count() const
		{
			return 1;
		}

		virtual std::ptrdiff_t get_desired_stack_size() const
		{
			return 0;
		}

		virtual ~TaskSchedulerJob() = default;

		std::shared_ptr<TaskSchedulerJob> self;
		std::string name;
		std::shared_ptr<TaskSchedulerArbiter> arbiter;

	private:
		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, self, 0x8);
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, name, 0x18);
#if defined(RML_WINDOWS)
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, arbiter, 0x38);
		RML_ASSERT_LAYOUT_SIZE(TaskSchedulerJob, 0x48);
#else
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, arbiter, 0x30);
		RML_ASSERT_LAYOUT_SIZE(TaskSchedulerJob, 0x40);
#endif
		RML_LAYOUT_GUARD_END()
	};
}
