#include "mod_descriptors.hpp"

#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/memory/vtable.hpp"
#include "RobloxModLoader/roblox/reflection/type.hpp"
#include "RobloxModLoader/util/string.hpp"
#include "pointers.hpp"

#include <array>
#include <memory>
#include <optional>
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

	static void* const* function_carrier_vtable()
	{
		static const FunctionCarrier carrier;
		return memory::vtable_of(&carrier);
	}

	const PropertyTypeInfo& property_type_info(const PropertyType type)
	{
		static constexpr PropertyTypeInfo infos[] = {
		    {"RBX::Reflection::TypedPropertyDescriptor<bool>", "bool", RBX::Reflection::TypeId::Bool, true, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<int>", "int", RBX::Reflection::TypeId::Int, true, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<float>", "float", RBX::Reflection::TypeId::Float, true, true},
		    {"RBX::Reflection::TypedPropertyDescriptor<double>", "double", RBX::Reflection::TypeId::Double, true, true},
		    {"RBX::Reflection::TypedPropertyDescriptor<std::string>", "std::string", RBX::Reflection::TypeId::String, false, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<RBX::Color3>", "RBX::Color3", RBX::Reflection::TypeId::Color3, false, false},
		    {"RBX::Reflection::TypedPropertyDescriptor<RBX::Vector3>", "RBX::Vector3", RBX::Reflection::TypeId::Vector3, false, false},
		    {"RBX::Reflection::EnumPropDescriptor<*", "int", RBX::Reflection::TypeId::Int, false, false},
		};
		return infos[static_cast<std::size_t>(type)];
	}

	const RBX::Reflection::detail::holder* variant_ops(const PropertyType type)
	{
		switch (type)
		{
		case PropertyType::Bool: return RBX::Reflection::detail::typed_holder<bool>::singleton();
		case PropertyType::Int: return RBX::Reflection::detail::typed_holder<int>::singleton();
		case PropertyType::Float: return RBX::Reflection::detail::typed_holder<float>::singleton();
		case PropertyType::Double: return RBX::Reflection::detail::typed_holder<double>::singleton();
		case PropertyType::String: return RBX::Reflection::detail::typed_holder<std::string>::singleton();
		case PropertyType::Color3: return RBX::Reflection::detail::typed_holder<G3D::Color3>::singleton();
		case PropertyType::Vector3: return RBX::Reflection::detail::typed_holder<G3D::Vector3>::singleton();
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
		case PropertyType::Color3: return Type::try_singleton<G3D::Color3>();
		case PropertyType::Vector3: return Type::try_singleton<G3D::Vector3>();
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

	template<typename Descriptor>
	static ModMember allocate_member()
	{
		return {std::make_unique<std::byte[]>(sizeof(Descriptor))};
	}

	static const RBX::Reflection::PropertyDescriptor::Attributes& property_attributes()
	{
		static const auto attributes = [] {
			RBX::Reflection::PropertyDescriptor::Attributes result{};
			result.functionality = RBX::Reflection::PropertyDescriptor::STANDARD_NO_REPLICATE;
			return result;
		}();
		return attributes;
	}

	template<typename V>
	static ModMember build_typed_property(void* owner_storage, const std::string& name, const std::string& category, const RBX::Reflection::Type* engine_type, void* const* vtable, void* accessor)
	{
		using Descriptor = RBX::Reflection::TypedPropertyDescriptor<V>;
		auto member = allocate_member<Descriptor>();
		auto* storage = member.storage.get();
		g_pointers->m_roblox_pointers.property_descriptor_ctor(storage, owner_storage, engine_type, name.c_str(), category.c_str(), &property_attributes(), RBX::Security::Protection{}, RBX::Security::Protection{}, false);
		memory::set_vtable(storage, vtable);
		reinterpret_cast<Descriptor*>(storage)->get_set.reset(static_cast<typename Descriptor::GetSet*>(accessor));
		return member;
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

		const auto vtable = static_cast<void* const*>(typed_property_vtable(type));
		if (!vtable)
			return std::unexpected(std::format("no vtable for {}; property '{}' skipped", info.descriptor_class, name));

		switch (type)
		{
		case PropertyType::Bool: return build_typed_property<bool>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Int: return build_typed_property<int>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Float: return build_typed_property<float>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Double: return build_typed_property<double>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::String: return build_typed_property<std::string>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Color3: return build_typed_property<G3D::Color3>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Vector3: return build_typed_property<G3D::Vector3>(owner_storage, name, category, engine_type, vtable, accessor);
		case PropertyType::Enum: break;
		}
		return std::unexpected(std::format("property '{}' is an enum; use make_enum_property", name));
	}

	using ModEnumProperty = RBX::Reflection::EnumPropDescriptor<int>;

	class ValidEnumGetSet final : public RBX::Reflection::TypedPropertyDescriptor<int>::GetSet
	{
	public:
		ValidEnumGetSet(const rml::reflection::GetSet<int>* inner, const RBX::Reflection::EnumDescriptor& enumeration) :
		    m_inner(inner),
		    m_enum(enumeration)
		{
		}

		bool is_read_only() const override
		{
			return m_inner->is_read_only();
		}

		bool is_write_only() const override
		{
			return m_inner->is_write_only();
		}

		int get_value(const RBX::Reflection::DescribedBase* instance) const override
		{
			const auto value = m_inner->get_value(instance);
			return m_enum.find_item_by_value(value) ? value : m_enum.items[0].value;
		}

		void set_value(RBX::Reflection::DescribedBase* instance, const int& value) const override
		{
			m_inner->set_value(instance, value);
		}

		bool equal_values(const RBX::Reflection::DescribedBase* a, const RBX::Reflection::DescribedBase* b) const override
		{
			return get_value(a) == get_value(b);
		}

		bool is_value_equal_to(const RBX::Reflection::DescribedBase* instance, const int& value) const override
		{
			return get_value(instance) == value;
		}

	private:
		const rml::reflection::GetSet<int>* m_inner;
		const RBX::Reflection::EnumDescriptor& m_enum;
	};

	static void get_enum_variant(const ModEnumProperty* self, const RBX::Reflection::DescribedBase* instance, RBX::Reflection::Variant& out)
	{
		self->value_enum_descriptor->convert_int_value_to_typed_variant_if_valid_value(self->get_set->get_value(instance), out);
	}

	static void set_enum_variant(const ModEnumProperty* self, RBX::Reflection::DescribedBase* instance, const RBX::Reflection::Variant& value)
	{
		if (const auto* item = self->value_enum_descriptor->lookup_by_enum_value_in_variant(value))
			self->get_set->set_value(instance, item->value);
	}

	static void* const* enum_property_vtable()
	{
		static const auto vtable = []() -> std::optional<memory::VtableCopy> {
			auto* const index = memory::rtti();
			const auto donor = index ? index->find_matching("RBX::Reflection::EnumPropDescriptor<*") : std::nullopt;
			if (!donor)
				return std::nullopt;
			memory::VtableCopy copy(static_cast<void* const*>(*donor), memory::virtual_index(&RBX::Reflection::EnumPropertyDescriptor::set_enum_item) + 1);
			copy.replace(&RBX::Reflection::PropertyDescriptor::get_variant_with_same_type_as_property, &get_enum_variant);
			copy.replace(&RBX::Reflection::PropertyDescriptor::set_variant_with_same_type_as_property, &set_enum_variant);
			return copy;
		}();
		return vtable ? vtable->address_point() : nullptr;
	}

	std::expected<ModMember, std::string> make_enum_property(void* owner_storage, const std::string& name, const std::string& category, const RBX::Reflection::EnumDescriptor& enumeration, void* accessor)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.property_descriptor_ctor)
			return std::unexpected("PROPERTY_DESCRIPTOR_CTOR is unavailable");
		const auto vtable = enum_property_vtable();
		if (!vtable)
			return std::unexpected(std::format("no engine EnumPropDescriptor vtable; property '{}' skipped", name));
		if (enumeration.item_count == 0)
			return std::unexpected(std::format("enum {} has no items; property '{}' skipped", enumeration.name.to_string(), name));

		auto member = allocate_member<ModEnumProperty>();
		auto* storage = member.storage.get();
		p.property_descriptor_ctor(storage, owner_storage, &enumeration, name.c_str(), category.c_str(), &property_attributes(), RBX::Security::Protection{}, RBX::Security::Protection{}, true);
		memory::set_vtable(storage, vtable);

		auto& property = *reinterpret_cast<ModEnumProperty*>(storage);
		property.enum_descriptor = &enumeration;
		property.get_set = std::make_unique<ValidEnumGetSet>(static_cast<const rml::reflection::GetSet<int>*>(accessor), enumeration);
		property.value_enum_descriptor = &enumeration;
		return member;
	}

	std::expected<ModMember, std::string> make_function(void* owner_storage, const std::string& name, const FunctionInvoker* invoker)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.function_descriptor_ctor)
			return std::unexpected("FUNCTION_DESCRIPTOR_CTOR is unavailable");

		auto member = allocate_member<RBX::Reflection::FunctionDescriptor>();
		auto* storage = member.storage.get();
		p.function_descriptor_ctor(storage, owner_storage, name.c_str(), RBX::Security::Protection{}, RBX::Reflection::FunctionDescriptor::Attributes{});
		memory::set_vtable(storage, function_carrier_vtable());

		{
			std::lock_guard lock(s_functions_mutex);
			s_functions[storage] = invoker;
		}

		return member;
	}

	std::expected<ModMember, std::string> make_event(void* owner_storage, const std::string& name, const std::ptrdiff_t member_offset, const std::vector<EventArgument>& arguments)
	{
		using RBX::Reflection::SignatureDescriptor;

		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.event_descriptor_ctor || !p.name_declare)
			return std::unexpected("EVENT_DESCRIPTOR_CTOR or NAME_DECLARE is unavailable");

		const auto vtable = event_desc_vtable(arguments);
		if (!vtable)
			return std::unexpected(std::format("no engine EventDesc vtable for the signature of event '{}'", name));

		const auto void_type = void_type_singleton();
		if (!void_type)
			return std::unexpected("Type::singleton<void> was not found");

		std::vector<SignatureDescriptor::Argument> items;
		items.reserve(arguments.size());
		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const auto type = type_singleton(arguments[i].type);
			if (!type)
				return std::unexpected(std::format("Type::singleton<{}> was not found; event '{}' skipped", property_type_info(arguments[i].type).cpp_name, name));

			const auto argument_name = arguments[i].name.empty() ? std::format("arg{}", i + 1) : arguments[i].name;
			items.push_back({p.name_declare(argument_name.c_str()), type, nullptr, nullptr, RBX::Reflection::Variant{}});
		}

		auto member = allocate_member<RBX::Reflection::EventDesc>();
		auto* storage = member.storage.get();
		static const RBX::Reflection::Descriptor::Attributes attributes;
		p.event_descriptor_ctor(storage, owner_storage, name.c_str(), RBX::Security::Protection{}, &attributes);
		memory::set_vtable(storage, static_cast<void* const*>(vtable));

		auto& event = *reinterpret_cast<RBX::Reflection::EventDesc*>(storage);
		event.signal = static_cast<decltype(event.signal)>(member_offset);
		std::construct_at(&event.signature.arguments, std::move(items));
		std::construct_at(&event.signature.result_types, std::vector<SignatureDescriptor::Result>{{void_type, nullptr, nullptr}});

		return member;
	}

	RBX::Reflection::Variant make_variant(const PropertyType type, const void* value)
	{
		RBX::Reflection::Variant variant;
		const auto engine_type = type_singleton(type);
		const auto* ops = variant_ops(type);
		if (!engine_type || !ops)
			return variant;

		variant.set_type_and_ops(engine_type, ops);
		ops->construct_func(static_cast<const char*>(value), static_cast<char*>(variant.storage()));
		return variant;
	}

	void destroy_variant(RBX::Reflection::Variant& variant)
	{
		if (variant.is_void())
			return;

		if (const auto* ops = variant.value_ops(); ops && ops->destruct_func)
			ops->destruct_func(static_cast<char*>(variant.storage()));
		variant.set_type_and_ops(nullptr, nullptr);
	}
}
