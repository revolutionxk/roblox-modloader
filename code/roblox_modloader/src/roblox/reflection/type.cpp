#include "RobloxModLoader/roblox/reflection/type.hpp"

#include "pointers.hpp"
#include "type_index.hpp"

namespace RBX::Reflection
{
	using rml::reflection::TypeIndex;

	std::span<const Type* const> Type::get_all_types()
	{
		const auto* registry = g_pointers ? g_pointers->m_roblox_pointers.type_registry : nullptr;
		return registry ? std::span<const Type* const>(*registry) : std::span<const Type* const>{};
	}

	template<>
	const Type& Type::get_singleton<void>()
	{
		static const Type& type = TypeIndex::require("null");
		return type;
	}

	template<>
	const Type& Type::get_singleton<bool>()
	{
		static const Type& type = TypeIndex::require("bool");
		return type;
	}

	template<>
	const Type& Type::get_singleton<int>()
	{
		static const Type& type = TypeIndex::require("int");
		return type;
	}

	template<>
	const Type& Type::get_singleton<float>()
	{
		static const Type& type = TypeIndex::require("float");
		return type;
	}

	template<>
	const Type& Type::get_singleton<double>()
	{
		static const Type& type = TypeIndex::require("double");
		return type;
	}

	template<>
	const Type& Type::get_singleton<std::string>()
	{
		static const Type& type = TypeIndex::require("string");
		return type;
	}
}
