#pragma once
#include "RobloxModLoader/util/layout_assert.hpp"
#include "task_scheduler.job.hpp"

#include <cstdint>

namespace RBX
{
	class DataModel;

	enum class DMTaskType : std::int32_t
	{
	};

	class DataModelJob : public TaskSchedulerJob
	{
	public:
		virtual StepResult step_data_model_job(const Stats& stats) = 0;

		DMTaskType task_type;
		double cyclic_executive_steps;

	private:
		[[maybe_unused]] bool reserved_170;

	public:
		std::uint64_t profiler_token;

	private:
		[[maybe_unused]] bool reserved_180;

	public:
		DataModel* data_model;
		std::uint32_t data_model_id;

	private:
		[[maybe_unused]] std::uint32_t reserved_194;

	public:
		bool profile_telemetry;

		RML_LAYOUT_GUARD_BEGIN()
#if defined(RML_WINDOWS)
		RML_ASSERT_OFFSET(DataModelJob, task_type, 0x170);
		RML_ASSERT_OFFSET(DataModelJob, profiler_token, 0x188);
		RML_ASSERT_OFFSET(DataModelJob, data_model, 0x198);
		RML_ASSERT_OFFSET(DataModelJob, data_model_id, 0x1A0);
		RML_ASSERT_OFFSET(DataModelJob, profile_telemetry, 0x1A8);
		RML_ASSERT_SIZE(DataModelJob, 0x1B0);
#else
		RML_ASSERT_OFFSET(DataModelJob, task_type, 0x164);
		RML_ASSERT_OFFSET(DataModelJob, profiler_token, 0x178);
		RML_ASSERT_OFFSET(DataModelJob, data_model, 0x188);
		RML_ASSERT_OFFSET(DataModelJob, data_model_id, 0x190);
		RML_ASSERT_OFFSET(DataModelJob, profile_telemetry, 0x198);
		RML_ASSERT_SIZE(DataModelJob, 0x1A0);
#endif
		RML_LAYOUT_GUARD_END()
	};
}
