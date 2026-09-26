#include "mod_descriptors.hpp"

#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/util/string.hpp"
#include "pointers.hpp"

#include <cstring>
#include <mutex>
#include <ranges>
#include <unordered_map>

RML_LOG_SCOPE("ModDescriptors");

namespace rml::reflection
{

	static std::mutex s_functions_mutex;
	static std::unordered_map<const void*, const FunctionInvoker*> s_functions;

	static const FunctionInvoker* invoker_for(const void* descriptor)
	{
		std::lock_guard lock(s_functions_mutex);
		const auto it = s_functions.find(descriptor);
		return it == s_functions.end() ? nullptr : it->second;
	}

	class FunctionCarrier
	{
	public:
		virtual ~FunctionCarrier() = default;

		virtual int execute_custom(void*, lua_State*) const
		{
			return 0;
		}

		virtual int execute_lua(void* instance, lua_State* L, int) const
		{
			const auto invoker = invoker_for(this);
			if (!invoker)
				return 0;

			try
			{
				return invoker->invoke(static_cast<RBX::Instance*>(instance), L);
			}
			catch (const std::exception& e)
			{
				g_pointers->m_roblox_pointers.luaL_errorL(L, "%s", e.what());
			}
			catch (...)
			{
				g_pointers->m_roblox_pointers.luaL_errorL(L, "unknown error in mod function");
			}
			return 0;
		}
	};

	static void* function_carrier_vtable()
	{
		static const FunctionCarrier carrier;
		return *reinterpret_cast<void* const*>(&carrier);
	}

	const PropertyTypeInfo& property_type_info(const PropertyType type)
	{
		static constexpr PropertyTypeInfo infos[] = {
		    {"RBX::Reflection::TypedPropertyDescriptor<bool>", "bool", RBX::Reflection::TypeId::Bool, true, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<int>", "int", RBX::Reflection::TypeId::Int, true, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<float>", "float", RBX::Reflection::TypeId::Float, true, true},
		    {"RBX::Reflection::TypedPropertyDescriptor<double>", "double", RBX::Reflection::TypeId::Double, true, true},
		    {"RBX::Reflection::TypedPropertyDescriptor<std::string>", "std::string", RBX::Reflection::TypeId::String, false, false},
		};
		return infos[static_cast<std::size_t>(type)];
	}

	const void* variant_ops(const PropertyType type)
	{
		switch (type)
		{
		case PropertyType::Bool: return VariantOps<bool>::table;
		case PropertyType::Int: return VariantOps<int>::table;
		case PropertyType::Float: return VariantOps<float>::table;
		case PropertyType::Double: return VariantOps<double>::table;
		case PropertyType::String: return VariantOps<std::string>::table;
		}
		return nullptr;
	}

	const RBX::Reflection::Type* type_singleton(const PropertyType type)
	{
		using RBX::Reflection::Type;
		switch (type)
		{
		case PropertyType::Bool: return Type::try_singleton<bool>();
		case PropertyType::Int: return Type::try_singleton<int>();
		case PropertyType::Float: return Type::try_singleton<float>();
		case PropertyType::Double: return Type::try_singleton<double>();
		case PropertyType::String: return Type::try_singleton<std::string>();
		}
		return nullptr;
	}

	const RBX::Reflection::Type* void_type_singleton()
	{
		return RBX::Reflection::Type::try_singleton<void>();
	}

	static void* event_desc_vtable(const std::vector<EventArgument>& arguments)
	{
		auto* const index = memory::rtti();
		if (!index)
			return nullptr;

		const auto names = arguments | std::views::transform([](const EventArgument& argument) { return std::string_view(property_type_info(argument.type).cpp_name); });
		const auto found = index->find_matching(std::format("RBX::Reflection::EventDesc<*,void({}),*", utils::join(names, ",")));
		return found ? *found : nullptr;
	}

	static void* typed_property_vtable(const PropertyType type)
	{
		auto* const index = memory::rtti();
		if (!index)
			return nullptr;

		const auto found = index->find(property_type_info(type).descriptor_class);
		return found ? *found : nullptr;
	}

	std::expected<ModMember, std::string> make_property(void* owner_storage, const std::string& name, const std::string& category, const PropertyType type, void* accessor)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.property_descriptor_ctor)
			return std::unexpected("PROPERTY_DESCRIPTOR_CTOR is unavailable");

		const auto& info = property_type_info(type);
		const auto engine_type = type_singleton(type);
		if (!engine_type)
			return std::unexpected(std::format("Type::singleton<{}> was not found; property '{}' skipped", info.cpp_name, name));

		const auto vtable = typed_property_vtable(type);
		if (!vtable)
			return std::unexpected(std::format("no vtable for {}; property '{}' skipped", info.descriptor_class, name));

		ModMember member;
		member.storage = std::make_unique<std::byte[]>(k_member_storage);
		std::memset(member.storage.get(), 0, k_member_storage);

		static PropertyAttributes attributes;
		auto* bytes = member.storage.get();
		p.property_descriptor_ctor(bytes, owner_storage, engine_type, name.c_str(), category.c_str(), &attributes, k_protection_none, k_protection_none, false);
		*reinterpret_cast<void**>(bytes) = vtable;
		*reinterpret_cast<void**>(bytes + k_property_accessor_offset) = accessor;
		*reinterpret_cast<void**>(bytes + k_property_accessor_offset + 8) = nullptr;
		*reinterpret_cast<void**>(bytes + k_property_accessor_offset + 16) = nullptr;

		return member;
	}

	std::expected<ModMember, std::string> make_function(void* owner_storage, const std::string& name, const FunctionInvoker* invoker)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.function_descriptor_ctor)
			return std::unexpected("FUNCTION_DESCRIPTOR_CTOR is unavailable");

		ModMember member;
		member.storage = std::make_unique<std::byte[]>(k_member_storage);
		std::memset(member.storage.get(), 0, k_member_storage);

		auto* bytes = member.storage.get();
		p.function_descriptor_ctor(bytes, owner_storage, name.c_str(), k_protection_none, 0, 0);
		*reinterpret_cast<void**>(bytes) = function_carrier_vtable();

		{
			std::lock_guard lock(s_functions_mutex);
			s_functions[bytes] = invoker;
		}

		return member;
	}

	std::expected<ModMember, std::string> make_event(void* owner_storage, const std::string& name, const std::ptrdiff_t member_offset, const std::vector<EventArgument>& arguments)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.event_descriptor_ctor || !p.name_declare)
			return std::unexpected("EVENT_DESCRIPTOR_CTOR or NAME_DECLARE is unavailable");

		const auto vtable = event_desc_vtable(arguments);
		if (!vtable)
			return std::unexpected(std::format("no engine EventDesc vtable for the signature of event '{}'", name));

		const auto void_type = void_type_singleton();
		if (!void_type)
			return std::unexpected("Type::singleton<void> was not found");

		std::vector<RBX::Reflection::SignatureDescriptor::Argument> items;
		items.reserve(arguments.size());
		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const auto type = type_singleton(arguments[i].type);
			if (!type)
				return std::unexpected(std::format("Type::singleton<{}> was not found; event '{}' skipped", property_type_info(arguments[i].type).cpp_name, name));

			const auto argument_name = arguments[i].name.empty() ? std::format("arg{}", i + 1) : arguments[i].name;
			items.push_back({p.name_declare(argument_name.c_str()), type, nullptr, nullptr, RBX::Reflection::Variant{}});
		}

		ModMember member;
		member.storage = std::make_unique<std::byte[]>(k_member_storage);
		std::memset(member.storage.get(), 0, k_member_storage);

		static RBX::Reflection::Descriptor::Attributes attributes;
		auto* bytes = member.storage.get();
		p.event_descriptor_ctor(bytes, owner_storage, name.c_str(), k_protection_none, &attributes);
		*reinterpret_cast<void**>(bytes) = vtable;
		*reinterpret_cast<std::int64_t*>(bytes + k_event_member_offset) = member_offset;
		::new (bytes + k_event_signature_offset) std::vector<RBX::Reflection::SignatureDescriptor::Argument>(std::move(items));
		::new (bytes + k_event_signature_offset + sizeof(std::vector<RBX::Reflection::SignatureDescriptor::Argument>)) std::vector<RBX::Reflection::SignatureDescriptor::Result>{{void_type, nullptr, nullptr}};

		return member;
	}

	RBX::Reflection::Variant make_variant(const PropertyType type, const void* value)
	{
		RBX::Reflection::Variant variant;
		const auto engine_type = type_singleton(type);
		const auto ops = static_cast<const void* const*>(variant_ops(type));
		if (!engine_type || !ops)
			return variant;

		variant.set_type_and_ops(engine_type, ops);
		reinterpret_cast<void (*)(const char*, char*)>(const_cast<void*>(ops[0]))(static_cast<const char*>(value), static_cast<char*>(variant.storage()));
		return variant;
	}

	void destroy_variant(RBX::Reflection::Variant& variant)
	{
		if (variant.is_void())
			return;

		const auto ops = static_cast<const void* const*>(variant.value_ops());
		if (ops && ops[2])
			reinterpret_cast<void (*)(char*)>(const_cast<void*>(ops[2]))(static_cast<char*>(variant.storage()));
		variant.set_type_and_ops(nullptr, nullptr);
	}
}
