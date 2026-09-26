#pragma once

#include "../util/name.hpp"
#include "RobloxModLoader/roblox/memory/noncopyable.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace RBX::Reflection
{
	class Descriptor : public rml::memory::Noncopyable
	{
	public:
		struct Attributes
		{
			bool is_deprecated{false};
			const Descriptor* preferred{nullptr};

			static Attributes deprecated()
			{
				Attributes result;
				result.is_deprecated = true;
				return result;
			}
		};

		static bool locked_down;

		const Name& name;
		Attributes attributes;
		std::uint32_t id;
		std::uint32_t stable_id;

		Descriptor() = delete;

		virtual ~Descriptor()
		{
		}
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(Descriptor::Attributes, 0x10);
	RML_ASSERT_SIZE(Descriptor, 0x28);
	RML_ASSERT_REF_OFFSET(Descriptor, name, 0x8);
	RML_ASSERT_OFFSET(Descriptor, attributes, 0x10);
	RML_ASSERT_OFFSET(Descriptor, id, 0x20);
	RML_ASSERT_OFFSET(Descriptor, stable_id, 0x24);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
