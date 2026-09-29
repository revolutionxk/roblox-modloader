#pragma once

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/reflection/class_builder.hpp"
#include "RobloxModLoader/roblox/reflection/enum_descriptor.hpp"
#include "RobloxModLoader/roblox/reflection/property_descriptor.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"

#include <lua.h>

#include <memory>
#include <string>

namespace rml::reflection
{
	struct PropertyTypeInfo
	{
		const char* descriptor_class;
		const char* cpp_name;
		std::uint8_t type_id;
		bool is_number;
		bool is_float;
	};

	const PropertyTypeInfo& property_type_info(PropertyType type);
	const RBX::Reflection::Type* type_singleton(PropertyType type);
	const RBX::Reflection::Type* void_type_singleton();
	const RBX::Reflection::detail::holder* variant_ops(PropertyType type);

	struct ModMember
	{
		std::unique_ptr<std::byte[]> storage;
	};

	std::expected<ModMember, std::string> make_property(void* owner_storage, const std::string& name, const std::string& category, PropertyType type, void* accessor);

	std::expected<ModMember, std::string> make_enum_property(void* owner_storage, const std::string& name, const std::string& category, const RBX::Reflection::EnumDescriptor& enumeration, void* accessor);

	std::expected<ModMember, std::string> make_function(void* owner_storage, const std::string& name, const FunctionInvoker* invoker);

	std::expected<ModMember, std::string> make_event(void* owner_storage, const std::string& name, std::ptrdiff_t member_offset, const std::vector<EventArgument>& arguments);
}
