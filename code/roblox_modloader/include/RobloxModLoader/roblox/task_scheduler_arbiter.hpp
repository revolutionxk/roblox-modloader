#pragma once

#include "RobloxModLoader/roblox/rsl/mutex.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>

namespace RBX
{
	class TaskSchedulerArbiter
	{
	public:
		virtual bool is_exclusive() const = 0;

		virtual void on_pre_acquire() = 0;

		virtual void on_acquire() = 0;

		virtual void* on_borrow() = 0;

		virtual void on_return(std::size_t token) = 0;

		virtual void on_release() = 0;

		virtual ~TaskSchedulerArbiter() = default;

		virtual const char* arbiter_name() const = 0;

		RSL::Mutex mutex;
		std::int32_t job_count;
		std::uint64_t current_job_index;
		void* job_slots;
		std::int32_t job_slot_capacity;

	private:
		[[maybe_unused]] std::uint64_t reserved_30[0x28];
		[[maybe_unused]] bool reserved_170;
		[[maybe_unused]] std::uint64_t reserved_178;
		[[maybe_unused]] double reserved_180[2];
		[[maybe_unused]] double reserved_190;
		[[maybe_unused]] std::uint64_t reserved_198[3];
		[[maybe_unused]] bool reserved_1b0;

	public:
		double last_step_time;

	private:
		[[maybe_unused]] std::uint64_t reserved_1c0[5];

		RML_LAYOUT_GUARD_BEGIN()
		RML_ASSERT_OFFSET(TaskSchedulerArbiter, reserved_30, 0x30);
		RML_ASSERT_OFFSET(TaskSchedulerArbiter, reserved_178, 0x178);
		RML_ASSERT_OFFSET(TaskSchedulerArbiter, reserved_1b0, 0x1B0);
		RML_ASSERT_OFFSET(TaskSchedulerArbiter, reserved_1c0, 0x1C0);
		RML_LAYOUT_GUARD_END()
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, mutex, 0x08);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_count, 0x10);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, current_job_index, 0x18);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_slots, 0x20);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_slot_capacity, 0x28);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, last_step_time, 0x1B8);
	RML_ASSERT_SIZE(TaskSchedulerArbiter, 0x1E8);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
