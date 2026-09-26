#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <string>

namespace RBX::Graphics
{
	class Device;

	class Resource
	{
	public:
		virtual ~Resource()
		{
		}

		virtual void set_debug_name(const std::string& name) = 0;
		virtual bool is_container_resource() const = 0;

		Device* device;
		std::size_t index;
		std::uint32_t memory_category;
		std::uint32_t reserved_1c;
		union
		{
			std::string debug_name;
		};

	protected:
		Resource()
		{
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(Resource, device, 0x8);
	RML_ASSERT_OFFSET(Resource, index, 0x10);
	RML_ASSERT_OFFSET(Resource, memory_category, 0x18);
	RML_ASSERT_OFFSET(Resource, debug_name, 0x20);
#if defined(RML_WINDOWS)
	RML_ASSERT_SIZE(Resource, 0x40);
#else
	RML_ASSERT_SIZE(Resource, 0x38);
#endif
	RML_LAYOUT_DIAGNOSTIC_POP()
}
