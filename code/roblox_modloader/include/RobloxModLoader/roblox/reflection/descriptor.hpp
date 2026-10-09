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
			const Descriptor* preferred{nullptr};
			bool is_deprecated{false};
			std::uint8_t thread_safety{};
			std::uint8_t aurora_access{};
			bool simulation_access{false};

			static Attributes deprecated()
			{
				Attributes result;
				result.is_deprecated = true;
				return result;
			}
		};

		static bool locked_down;

		const Name& name;
		const Descriptor* preferred;
		bool is_deprecated;
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
	RML_ASSERT_REFERENCE_OFFSET(Descriptor, name, 0x8);
	RML_ASSERT_OFFSET(Descriptor::Attributes, is_deprecated, 0x8);
	RML_ASSERT_OFFSET(Descriptor::Attributes, thread_safety, 0x9);
	RML_ASSERT_OFFSET(Descriptor::Attributes, simulation_access, 0xB);
	RML_ASSERT_OFFSET(Descriptor, preferred, 0x10);
	RML_ASSERT_OFFSET(Descriptor, is_deprecated, 0x18);
	RML_ASSERT_OFFSET(Descriptor, id, 0x1C);
	RML_ASSERT_OFFSET(Descriptor, stable_id, 0x20);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
