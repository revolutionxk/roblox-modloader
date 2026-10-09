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
		[[maybe_unused]] std::int64_t reserved_70[3];
		[[maybe_unused]] std::uint64_t reserved_88[3];
		[[maybe_unused]] std::uint32_t reserved_a0;
		[[maybe_unused]] std::uint32_t reserved_a4;
		[[maybe_unused]] std::uint16_t reserved_a8;

		RML_LAYOUT_GUARD_BEGIN()
#if !defined(RML_WINDOWS)
		RML_ASSERT_OFFSET(DataModelProp, reserved_70, 0x70);
		RML_ASSERT_OFFSET(DataModelProp, reserved_a8, 0xA8);
#endif
		RML_LAYOUT_GUARD_END()
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
