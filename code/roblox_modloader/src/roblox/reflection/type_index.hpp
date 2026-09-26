#pragma once

#include <string_view>

namespace RBX::Reflection
{
	class Type;
}

namespace rml::reflection
{
	class TypeIndex
	{
	public:
		[[nodiscard]] static const RBX::Reflection::Type* find(std::string_view name);
		[[nodiscard]] static const RBX::Reflection::Type* find_by_id(int type_id);
		[[nodiscard]] static const RBX::Reflection::Type& require(std::string_view name);
	};
}
