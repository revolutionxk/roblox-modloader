#pragma once

#include "RobloxModLoader/roblox/job_base.hpp"

namespace rml::dotnet
{
	class InstanceCollectorJob final : public jobs::JobBase
	{
	public:
		InstanceCollectorJob() noexcept;

	private:
		bool should_execute_impl(const JobExecutionContext& context) noexcept override;
		void execute_impl(const JobExecutionContext& context) override;
		void destroy_impl() noexcept override;

		static constexpr std::string_view JOB_NAME = "DotnetInstanceCollector";
	};
}
