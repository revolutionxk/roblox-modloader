#pragma once

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/reflection/class_builder.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"

#include <lua.h>

#include <memory>
#include <string>

namespace rml::reflection
{
	inline constexpr std::uint32_t k_protection_none = 0;
	inline constexpr std::size_t k_member_storage = 512;
	inline constexpr std::size_t k_property_accessor_offset = 144;
	inline constexpr std::size_t k_event_signature_offset = 0x48;
	inline constexpr std::size_t k_event_member_offset = 0x78;
	inline constexpr std::uint8_t k_property_functionality_standard_no_replicate = 1 | 4 | 8 | 16;

	struct PropertyAttributes
	{
		std::uint64_t descriptor_attributes[2]{};
		std::uint64_t reserved_16{};
		std::uint32_t reserved_24{};
		std::uint8_t reserved_28{};
		std::uint8_t functionality{k_property_functionality_standard_no_replicate};
		std::uint8_t reserved_30{};
		std::uint8_t reserved_31{};
	};

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
	const void* variant_ops(PropertyType type);

	struct ModMember
	{
		std::unique_ptr<std::byte[]> storage;
	};

	std::expected<ModMember, std::string> make_property(void* owner_storage, const std::string& name, const std::string& category, PropertyType type, void* accessor);

	std::expected<ModMember, std::string> make_function(void* owner_storage, const std::string& name, const FunctionInvoker* invoker);

	std::expected<ModMember, std::string> make_event(void* owner_storage, const std::string& name, std::ptrdiff_t member_offset, const std::vector<EventArgument>& arguments);
}
