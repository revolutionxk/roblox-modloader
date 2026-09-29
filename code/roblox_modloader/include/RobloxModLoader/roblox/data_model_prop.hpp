#pragma once

#include "signals.hpp"
#include "workspace.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace RBX
{
	class DataModelProp
	{
	public:
		virtual ~DataModelProp() = default;

		std::string place_id_string;
		std::string universe_id_string;
		std::string job_id;
		std::shared_ptr<Workspace> workspace;
		rbx::signal<void()> loaded_signal;
		rbx::signal<void(bool)> graphics_quality_signal;

	private:
		std::string reserved_string;
		std::byte reserved_tail[0x18];
		std::uint64_t reserved_counter;
		std::uint16_t reserved_flags;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(DataModelProp, place_id_string, 0x08);
#if defined(RML_WINDOWS)
	RML_ASSERT_OFFSET(DataModelProp, universe_id_string, 0x28);
	RML_ASSERT_OFFSET(DataModelProp, job_id, 0x48);
	RML_ASSERT_OFFSET(DataModelProp, workspace, 0x68);
	RML_ASSERT_SIZE(DataModelProp, 0xD0);
#else
	RML_ASSERT_OFFSET(DataModelProp, universe_id_string, 0x20);
	RML_ASSERT_OFFSET(DataModelProp, job_id, 0x38);
	RML_ASSERT_OFFSET(DataModelProp, workspace, 0x50);
	RML_ASSERT_SIZE(DataModelProp, 0xB0);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
