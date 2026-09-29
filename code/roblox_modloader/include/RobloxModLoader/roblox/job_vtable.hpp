#pragma once

#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/roblox/data_model_job.hpp"

#include <cstddef>

namespace rml
{
	inline std::size_t job_step_slot()
	{
		static const std::size_t slot = memory::virtual_index(&RBX::DataModelJob::step_data_model_job);
		return slot;
	}
}
