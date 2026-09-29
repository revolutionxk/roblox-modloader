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

	private:
		std::byte reserved_14[0x4];

	public:
		std::uint64_t current_job_index;
		void* job_slots;
		std::int32_t job_slot_capacity;

	private:
		std::byte reserved_2c[0x14C];

	public:
		std::uint64_t frame_budget;

	private:
		std::byte reserved_180[0x68];
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, mutex, 0x08);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_count, 0x10);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, current_job_index, 0x18);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_slots, 0x20);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, job_slot_capacity, 0x28);
	RML_ASSERT_OFFSET(TaskSchedulerArbiter, frame_budget, 0x178);
	RML_ASSERT_SIZE(TaskSchedulerArbiter, 0x1E8);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
