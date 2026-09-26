#pragma once

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/reflection/class_builder.hpp"
#include "RobloxModLoader/roblox/reflection/creatable.hpp"
#include "RobloxModLoader/roblox/reflection/object.hpp"

#include <array>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rml::reflection
{
	struct PropertySpec
	{
		std::string name;
		std::string category;
		PropertyType type;
		std::shared_ptr<void> accessor;
	};

	struct FunctionSpec
	{
		std::string name;
		std::shared_ptr<FunctionInvoker> invoker;
	};

	struct EventSpec
	{
		std::string name;
		std::ptrdiff_t member_offset;
		std::vector<EventArgument> arguments;
	};

	struct ClassSpec
	{
		std::string name;
		std::string base;
		ClassLayout layout;
		std::vector<PropertySpec> properties;
		std::vector<FunctionSpec> functions;
		std::vector<EventSpec> events;
	};

	struct ExtensionSpec
	{
		std::string name;
		std::vector<PropertySpec> properties;
		std::vector<FunctionSpec> functions;
	};

	struct RegisteredExtension
	{
		RBX::Reflection::ClassDescriptor* descriptor{};
		std::vector<std::unique_ptr<std::byte[]>> member_storage;
		std::vector<std::shared_ptr<void>> accessors;
		std::vector<std::shared_ptr<FunctionInvoker>> invokers;
		std::vector<const RBX::Reflection::PropertyDescriptor*> property_table;
		std::vector<const RBX::Reflection::FunctionDescriptor*> function_table;
	};

	struct RegisteredClass;

	class ModInstanceCreator final : public RBX::ICreator
	{
	public:
		explicit ModInstanceCreator(RegisteredClass& entry) :
		    m_entry(entry)
		{
		}

		std::shared_ptr<void> create(RBX::EngineContext* context, RBX::CreatorRole role) const override;
		bool is_serializable() const override;
		bool is_script_creatable() const override;
		const RBX::Reflection::ClassDescriptor* descriptor() const override;

	private:
		RegisteredClass& m_entry;
	};

	inline constexpr std::size_t k_vtable_prefix_slots = 2;
	inline constexpr std::size_t k_cloned_vtable_slots = 160;
	using ClonedVtable = std::array<void*, k_vtable_prefix_slots + k_cloned_vtable_slots>;

	struct RegisteredClass
	{
		std::string name;
		ClassLayout layout{};
		RBX::Reflection::ClassDescriptor* descriptor{};
		RBX::Reflection::ClassDescriptor* base{};
		void** engine_vtable{};
		std::unique_ptr<ModInstanceCreator> creator;
		std::unique_ptr<std::byte[]> storage;
		std::unique_ptr<ClonedVtable> vtable;
		std::once_flag vtable_once;
		std::vector<std::unique_ptr<std::byte[]>> member_storage;
		std::vector<std::shared_ptr<void>> accessors;
		std::vector<std::shared_ptr<FunctionInvoker>> invokers;
		std::vector<const RBX::Reflection::PropertyDescriptor*> property_table;
		std::vector<const RBX::Reflection::FunctionDescriptor*> function_table;
		std::vector<const RBX::Reflection::EventDescriptor*> event_table;
		std::unordered_map<std::ptrdiff_t, const RBX::Reflection::EventDescriptor*> events_by_offset;
	};

	class ClassRegistry
	{
	public:
		static ClassRegistry& instance();

		[[nodiscard]] static bool available();
		[[nodiscard]] std::expected<void, std::string> validate();
		[[nodiscard]] std::expected<RBX::Reflection::ClassDescriptor*, std::string> define(const ClassSpec& spec);
		[[nodiscard]] std::expected<RBX::Reflection::ClassDescriptor*, std::string> extend(const ExtensionSpec& spec);
		[[nodiscard]] const RBX::ICreator* creator_for(const RBX::Name* name) const;
		[[nodiscard]] RBX::Reflection::ClassDescriptor* find_engine_class(std::string_view name) const;
		[[nodiscard]] RegisteredClass* class_of(const void* instance);
		[[nodiscard]] void** vtable_for(RegisteredClass& entry, void** derived_vtable);

	private:
		std::deque<RegisteredClass> m_classes;
		std::deque<RegisteredExtension> m_extensions;
		std::optional<std::expected<void, std::string>> m_validation;
		std::unordered_map<const RBX::Name*, const RBX::ICreator*> m_creators;
		std::unordered_map<const RBX::Reflection::ClassDescriptor*, RegisteredClass*> m_by_descriptor;
	};
}
