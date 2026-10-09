#pragma once
#include "data_model_job.hpp"
#include "script_context.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <memory>

namespace RBX
{
	class Random
	{
	public:
		std::uint64_t state;
	};
}

namespace RBX::ScriptContextFacets
{
	class WaitingHybridScriptsJob : public DataModelJob
	{
	public:
		double waiting_threads_budget;
		Random random;
		std::weak_ptr<ScriptContext> script_context;

	private:
		RML_LAYOUT_GUARD_BEGIN()
#if defined(RML_WINDOWS)
		RML_ASSERT_OFFSET(WaitingHybridScriptsJob, waiting_threads_budget, 0x1B0);
		RML_ASSERT_OFFSET(WaitingHybridScriptsJob, script_context, 0x1C0);
		RML_ASSERT_SIZE(WaitingHybridScriptsJob, 0x1D0);
#else
		RML_ASSERT_OFFSET(WaitingHybridScriptsJob, waiting_threads_budget, 0x1A0);
		RML_ASSERT_OFFSET(WaitingHybridScriptsJob, script_context, 0x1B0);
		RML_ASSERT_SIZE(WaitingHybridScriptsJob, 0x1C0);
#endif
		RML_LAYOUT_GUARD_END()
	};
}
