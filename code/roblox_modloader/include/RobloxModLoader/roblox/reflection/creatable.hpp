#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>
#include <memory>

namespace RBX
{
	class EngineContext;

	namespace Reflection
	{
		class ClassDescriptor;
	}

	enum class CreatorRole : std::int32_t
	{
		Replication = 0,
		Serialization = 1,
		Scripting = 2,
		Engine = 3
	};

	class ForceConstructionInCreatable
	{
	public:
		EngineContext* context;
		std::int32_t tag;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_SIZE(ForceConstructionInCreatable, 16);
	RML_LAYOUT_DIAGNOSTIC_POP()

	// Windows 0.739 calls slot 3 for the class descriptor right after getCreator.
	class ICreator
	{
	public:
		virtual std::shared_ptr<void> create(EngineContext* context, CreatorRole role) const = 0;
		virtual bool is_serializable() const = 0;
		virtual bool is_script_creatable() const = 0;
		virtual const Reflection::ClassDescriptor* descriptor() const = 0;
	};
}
