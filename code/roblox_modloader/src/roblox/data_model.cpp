#include "RobloxModLoader/roblox/data_model.hpp"

#include "RobloxModLoader/roblox/data_model_job.hpp"

namespace RBX
{
	DataModel* DataModel::from_job(const DataModelJob* job)
	{
		if (job == nullptr)
		{
			return nullptr;
		}

		return job->arbiter ? static_cast<DataModel*>(job->arbiter.get()) : nullptr;
	}
}
