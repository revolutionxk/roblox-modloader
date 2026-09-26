#include "mod_descriptors.hpp"

#include "RobloxModLoader/memory/i_rtti_provider.hpp"
#include "RobloxModLoader/memory/pattern.hpp"
#include "RobloxModLoader/memory/range.hpp"
#include "RobloxModLoader/memory/string_anchor.hpp"
#include "pointers.hpp"

#include <cstring>
#include <mutex>
#include <regex>
#include <unordered_map>

RML_LOG_SCOPE("ModDescriptors");

namespace rml::reflection
{
	static constexpr std::uint32_t k_protection_none = 0;

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

	// `mangled` spells T as it appears in an EventDesc RTTI name: Itanium on macOS, undecorated on Windows.
	const PropertyTypeInfo& property_type_info(const PropertyType type)
	{
#if defined(RML_WINDOWS)
		static constexpr PropertyTypeInfo infos[] = {
		    {"bool", "RBX::Reflection::TypedPropertyDescriptor<bool>", "bool", RBX::Reflection::TypeId::Bool, true, false},
		    {"int", "RBX::Reflection::TypedPropertyDescriptor<int>", "int", RBX::Reflection::TypeId::Int, true, false},
		    {"float", "RBX::Reflection::TypedPropertyDescriptor<float>", "float", RBX::Reflection::TypeId::Float, true, true},
		    {"double", "RBX::Reflection::TypedPropertyDescriptor<double>", "double", RBX::Reflection::TypeId::Double, true, true},
		    {"string", "RBX::Reflection::TypedPropertyDescriptor<std::basic_string<char,std::char_traits<char>,std::allocator<char> > >", "std::basic_string<char,std::char_traits<char>,std::allocator<char> >", RBX::Reflection::TypeId::String, false, false},
		};
#else
		static constexpr PropertyTypeInfo infos[] = {
		    {"bool", "RBX::Reflection::TypedPropertyDescriptor<bool>", "b", RBX::Reflection::TypeId::Bool, true, false},
		    {"int", "RBX::Reflection::TypedPropertyDescriptor<int>", "i", RBX::Reflection::TypeId::Int, true, false},
		    {"float", "RBX::Reflection::TypedPropertyDescriptor<float>", "f", RBX::Reflection::TypeId::Float, true, true},
		    {"double", "RBX::Reflection::TypedPropertyDescriptor<double>", "d", RBX::Reflection::TypeId::Double, true, true},
		    {"string", "RBX::Reflection::TypedPropertyDescriptor<std::string>", "NSt3__112basic_stringIcNS3_11char_traitsIcEENS3_9allocatorIcEEEE", RBX::Reflection::TypeId::String, false, false},
		};
#endif
		return infos[static_cast<std::size_t>(type)];
	}

	static constexpr PropertyTypeInfo k_void_type_info{"null", nullptr, "v", 0, false, false};

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

#if defined(RML_WINDOWS)
	// Every Type registers itself in the engine's type vector; exactly one must match.
	static const RBX::Reflection::Type* find_type_singleton(const PropertyTypeInfo& info)
	{
		const auto* registry = g_pointers ? g_pointers->m_roblox_pointers.type_registry : nullptr;
		if (!registry)
			return nullptr;

		const RBX::Reflection::Type* found = nullptr;
		std::size_t matches = 0;
		for (const auto* type : *registry)
		{
			if (!type || type->name != info.engine_name || type->type_id != info.type_id || type->is_number != info.is_number
			    || type->is_float != info.is_float)
				continue;
			found = type;
			++matches;
		}

		if (matches != 1)
		{
			RML_ERROR("Expected one Type named '{}' (id {}) in the type registry, found {}", info.engine_name, info.type_id, matches);
			return nullptr;
		}

		return found;
	}
#else
	static const RBX::Reflection::Type* find_type_singleton(const PropertyTypeInfo& info)
	{
		const auto mov_w = [](const unsigned reg, const unsigned value) { return std::format("{:02X} {:02X} 80 52", ((value << 5) | reg) & 0xFF, (value << 5) >> 8); };
		const memory::pattern shape(
		    "F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 ? ? ? ? ? ? ? ? 08 C1 BF 38 ? ? ? ? ? ? ? ? ? ? ? ? FD 7B 41 A9 F4 4F C2 A8 C0 03 5F D6 "
		    "? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? "
		    + mov_w(2, info.type_id) + " " + mov_w(3, info.is_number) + " " + mov_w(4, info.is_float) + " 05 00 80 52 06 00 80 52");

		for (const auto& function : memory::functions_referencing_string(info.engine_name))
		{
			const memory::range body(memory::handle(function.start), function.size);
			const auto hit = body.scan(shape);
			if (!hit || hit->as<void*>() != function.start)
				continue;

			RML_DEBUG("Type::getSingleton<{}> at 0x{:X}", info.engine_name, reinterpret_cast<std::uintptr_t>(function.start));
			return reinterpret_cast<const RBX::Reflection::Type* (*)()>(function.start)();
		}

		return nullptr;
	}
#endif

	const RBX::Reflection::Type* type_singleton(const PropertyType type)
	{
		static std::mutex mutex;
		static std::unordered_map<PropertyType, const RBX::Reflection::Type*> cache;

		std::lock_guard lock(mutex);
		if (const auto it = cache.find(type); it != cache.end())
			return it->second;

		const auto singleton = find_type_singleton(property_type_info(type));
		cache[type] = singleton;
		return singleton;
	}

	const RBX::Reflection::Type* void_type_singleton()
	{
		static const auto singleton = find_type_singleton(k_void_type_info);
		return singleton;
	}

	static void* event_desc_vtable(const std::vector<EventArgument>& arguments)
	{
#if defined(RML_WINDOWS)
		// Undecorated spelling: "void (float,int)", "void (void)" for none.
		std::string key;
		for (const auto& argument : arguments)
		{
			if (!key.empty())
				key += ',';
			key += property_type_info(argument.type).mangled;
		}
		if (arguments.empty())
			key = "void";

		const std::string prefix = "RBX::Reflection::EventDesc<";
		const std::string pattern = "^RBX::Reflection::EventDesc<([A-Za-z0-9_:]+),void \\(" + key + "\\),rbx::signal<void \\(" + key + "\\)>,rbx::signal<void \\(" + key + "\\)> \\1::\\*>$";
#else
		std::string mangled_arguments;
		std::size_t strings = 0;
		for (const auto& argument : arguments)
		{
			if (argument.type == PropertyType::String && strings++)
				return nullptr;
			mangled_arguments += property_type_info(argument.type).mangled;
		}
		if (arguments.empty())
			mangled_arguments = "v";

		const auto key = mangled_arguments;
		const std::string prefix = "N3RBX10Reflection9EventDescINS_";
		const std::string pattern = "^N3RBX10Reflection9EventDescINS_[0-9]+[A-Za-z0-9_]+?EFv" + mangled_arguments + "EN3rbx6signalIS[0-9A-Z]*_EEMS2_S[0-9A-Z]*_EE$";
#endif

		static std::mutex mutex;
		static std::unordered_map<std::string, void*> cache;
		std::lock_guard lock(mutex);
		if (const auto it = cache.find(key); it != cache.end())
			return it->second;

		void* vtable = nullptr;
		if (g_rtti_provider)
		{
			const std::regex shape(pattern);
			const auto found = g_rtti_provider->find_class_vtable_matching(prefix, [&](const std::string_view name) {
				return std::regex_match(name.begin(), name.end(), shape);
			});
			if (found)
				vtable = *found;
		}
		cache[key] = vtable;
		return vtable;
	}

	static void* typed_property_vtable(const PropertyType type)
	{
		static std::mutex mutex;
		static std::unordered_map<PropertyType, void*> cache;

		std::lock_guard lock(mutex);
		if (const auto it = cache.find(type); it != cache.end())
			return it->second;

		void* vtable = nullptr;
		if (g_rtti_provider)
		{
			if (const auto found = g_rtti_provider->find_class_vtable(property_type_info(type).descriptor_class))
				vtable = *found;
		}
		cache[type] = vtable;
		return vtable;
	}

	std::expected<ModMember, std::string> make_property(void* owner_storage, const std::string& name, const std::string& category, const PropertyType type, void* accessor)
	{
		const auto& p = g_pointers->m_roblox_pointers;
		if (!p.property_descriptor_ctor)
			return std::unexpected("PROPERTY_DESCRIPTOR_CTOR is unavailable");

		const auto& info = property_type_info(type);
		const auto engine_type = type_singleton(type);
		if (!engine_type)
			return std::unexpected(std::format("Type::getSingleton<{}> was not found; property '{}' skipped", info.engine_name, name));

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
		p.function_descriptor_ctor(bytes, owner_storage, name.c_str(), k_protection_none, RBX::Reflection::Descriptor::Attributes{});
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
			return std::unexpected("Type::getSingleton<void> was not found");

		std::vector<RBX::Reflection::SignatureDescriptor::Argument> items;
		items.reserve(arguments.size());
		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			const auto type = type_singleton(arguments[i].type);
			if (!type)
				return std::unexpected(std::format("Type::getSingleton<{}> was not found; event '{}' skipped", property_type_info(arguments[i].type).engine_name, name));

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
