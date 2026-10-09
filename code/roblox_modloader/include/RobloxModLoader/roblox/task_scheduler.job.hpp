#pragma once

#include "RobloxModLoader/roblox/rsl/condition.hpp"
#include "RobloxModLoader/roblox/rsl/mutex.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>
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

		std::weak_ptr<TaskSchedulerJob> self;
		std::string name;
		std::shared_ptr<TaskSchedulerArbiter> arbiter;
		std::int32_t priority;

	private:
		[[maybe_unused]] std::uint64_t reserved_48[0x13];

	public:
		std::uint32_t sync_workers;
		bool completion_waiting;
		RSL::Condition completion;

	private:
		[[maybe_unused]] std::uint64_t reserved_f0[0xC];
		RSL::Mutex reserved_mutex;
		RSL::Condition reserved_condition;
		[[maybe_unused]] std::uint16_t reserved_160;
		[[maybe_unused]] std::uint8_t reserved_162;

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, self, 0x8);
		RML_ASSERT_LAYOUT_OFFSET(TaskSchedulerJob, name, 0x18);
#if defined(RML_WINDOWS)
		RML_ASSERT_OFFSET(TaskSchedulerJob, arbiter, 0x38);
		RML_ASSERT_OFFSET(TaskSchedulerJob, sync_workers, 0xE8);
		RML_ASSERT_OFFSET(TaskSchedulerJob, reserved_160, 0x168);
		RML_ASSERT_SIZE(TaskSchedulerJob, 0x170);
#else
		RML_ASSERT_OFFSET(TaskSchedulerJob, arbiter, 0x30);
		RML_ASSERT_OFFSET(TaskSchedulerJob, sync_workers, 0xE0);
		RML_ASSERT_OFFSET(TaskSchedulerJob, completion, 0xE8);
		RML_ASSERT_OFFSET(TaskSchedulerJob, reserved_mutex, 0x150);
		RML_ASSERT_OFFSET(TaskSchedulerJob, reserved_160, 0x160);
		RML_ASSERT_SIZE(TaskSchedulerJob, 0x168);
#endif
		RML_LAYOUT_GUARD_END()
	};
}
