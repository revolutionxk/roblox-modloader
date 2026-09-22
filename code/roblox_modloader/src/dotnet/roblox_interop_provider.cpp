
#include "RobloxModLoader/memory/foreign_call.hpp"
#include "roblox_interop_provider.hpp"

#include "pointers.hpp"

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"
#include "RobloxModLoader/roblox/reflection/function_descriptor.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "RobloxModLoader/util/memory.hpp"
#include "dotnet_arguments.hpp"
#include "dotnet_event_descriptor.hpp"
#include "dotnet_variant.hpp"
#include "dotnet_yield.hpp"
#include "type_marshaler.hpp"

#include <RobloxModLoader/qt/mods_menu.hpp>
#include <RobloxModLoader/qt/qt_integration.hpp>
#include <RobloxModLoader/roblox/instance.hpp>
#include <RobloxModLoader/roblox/reflection/object.hpp>
#include <RobloxModLoader/roblox/reflection/property_descriptor.hpp>
#include <cassert>
#include <format>
#include <stdexcept>
#include <string_view>
#include <utility>

#if RML_ENABLE_LUAU
	#include <RobloxModLoader/luau/dispatch/dispatcher.hpp>
	#include <RobloxModLoader/luau/dispatch/work.hpp>
	#include <RobloxModLoader/luau/modules/bytecode_cache.hpp>
	#include <RobloxModLoader/luau/script_host.hpp>
	#include <RobloxModLoader/luau/script_runtime.hpp>
#endif

RML_LOG_SCOPE("Interop");

namespace rml::dotnet
{
	void RobloxInteropProvider::verify_populated(const InteropTable& table)
	{
		const std::pair<const void*, std::string_view> members[]{
		    {reinterpret_cast<const void*>(table.reflection_invoke), "reflection_invoke"},
		    {reinterpret_cast<const void*>(table.reflection_invoke_async), "reflection_invoke_async"},
		    {reinterpret_cast<const void*>(table.reflection_get_property), "reflection_get_property"},
		    {reinterpret_cast<const void*>(table.reflection_set_property), "reflection_set_property"},
		    {reinterpret_cast<const void*>(table.reflection_event_connect), "reflection_event_connect"},
		    {reinterpret_cast<const void*>(table.reflection_event_disconnect), "reflection_event_disconnect"},
		    {reinterpret_cast<const void*>(table.object_create_by_name), "object_create_by_name"},
		    {reinterpret_cast<const void*>(table.managed_log), "managed_log"},
		    {reinterpret_cast<const void*>(table.free_string), "free_string"},
		    {reinterpret_cast<const void*>(table.free_native_ptr), "free_native_ptr"},
		    {reinterpret_cast<const void*>(table.mods_menu_add_action), "mods_menu_add_action"},
		    {reinterpret_cast<const void*>(table.mods_menu_add_submenu), "mods_menu_add_submenu"},
		    {reinterpret_cast<const void*>(table.mods_menu_add_separator), "mods_menu_add_separator"},
		    {reinterpret_cast<const void*>(table.mods_menu_add_checkable), "mods_menu_add_checkable"},
		    {reinterpret_cast<const void*>(table.mods_menu_set_item_icon), "mods_menu_set_item_icon"},
		    {reinterpret_cast<const void*>(table.mods_menu_remove), "mods_menu_remove"},
		    {reinterpret_cast<const void*>(table.reflection_event_fire), "reflection_event_fire"},
		    {reinterpret_cast<const void*>(table.reflection_event_disconnect_all), "reflection_event_disconnect_all"},
		    {reinterpret_cast<const void*>(table.reflection_event_slots), "reflection_event_slots"},
		    {reinterpret_cast<const void*>(table.event_slot_fire), "event_slot_fire"},
		    {reinterpret_cast<const void*>(table.event_slot_disconnect), "event_slot_disconnect"},
		    {reinterpret_cast<const void*>(table.event_slot_release), "event_slot_release"},
		    {reinterpret_cast<const void*>(table.luau_host_ready), "luau_host_ready"},
		    {reinterpret_cast<const void*>(table.luau_schedule), "luau_schedule"},
		    {reinterpret_cast<const void*>(table.luau_evaluate), "luau_evaluate"},
		    {reinterpret_cast<const void*>(table.luau_ref_call), "luau_ref_call"},
		    {reinterpret_cast<const void*>(table.luau_ref_index), "luau_ref_index"},
		    {reinterpret_cast<const void*>(table.luau_ref_release), "luau_ref_release"},
		};

		for (const auto& [pointer, name] : members)
		{
			if (!pointer)
				RML_ERROR("InteropTable::{} was never populated", name);

			assert(pointer && "InteropTable member left unpopulated after populate");
		}
	}

	[[nodiscard]] RBX::Instance* as_instance(const uintptr_t handle)
	{
		if (!utils::memory::is_valid_pointer(handle))
			return nullptr;
		return reinterpret_cast<RBX::Instance*>(handle);
	}

	void invoke_reflection_function(RBX::Reflection::DescribedBase* instance, const RBX::Reflection::FunctionDescriptor& descriptor, const InteropVariant* args, const uint32_t arg_count, InteropVariant& out)
	{
		// find_function() resolves a name against one shared member table and hands the hit back as
		// a FunctionDescriptor whatever it really is; a YieldFunctionDescriptor is 0x78 bytes, so
		// kind (+0x78) and invoke_func_ptr (+0x80) would read past the object. Prove the descriptor
		// is a member of its owner's function container before reading anything past MemberDescriptor.
		if (descriptor.owner.find_function_descriptor(descriptor.name.c_str()) != &descriptor)
			throw std::runtime_error(std::format("'{}' is not a function of '{}'; it is not callable as a plain function",
			    descriptor.name.c_str(), descriptor.owner.name.c_str()));

		// A member that is a function is still not necessarily a bound one: a custom invoker keeps
		// no native member pointer, and calling that slot jumps to address zero and takes Studio
		// down with it.
		if (descriptor.get_kind() != RBX::Reflection::FunctionDescriptor::Default
		    || !static_cast<const RBX::Reflection::BoundFunctionDescriptor&>(descriptor).invoke_func_ptr)
			throw std::runtime_error(std::format("'{}' has no native function pointer; it is not callable as a plain function",
			    descriptor.name.c_str()));

		DotNetArguments arguments{args, arg_count, &descriptor.get_signature()};

		const auto function = RBX::Function(descriptor, instance);

		const auto type = descriptor.get_signature().first_result_type();
		const bool indirect_result = TypeMarshaler::returns_indirectly(type);

		const auto ret = function.invoke(arguments, indirect_result);

		TypeMarshaler::encode_return_value(type, ret, reinterpret_cast<uintptr_t>(&arguments.return_value), out);
	}

	RBX::Reflection::EventArguments build_event_fire_args(const RBX::Reflection::EventDescriptor* descriptor, const InteropVariant* args, const uint32_t arg_count)
	{
		RBX::Reflection::EventArguments event_args;
		const auto& signature = descriptor->get_signature();
		const auto sig_args = signature.arguments();

		if (sig_args.size() == 1 && sig_args[0].type && sig_args[0].type->type_id == RBX::Reflection::TypeId::Tuple)
		{
			RBX::Reflection::Variant tuple_variant;
			if (TypeMarshaler::build_tuple_variant(args, arg_count, sig_args[0].type, tuple_variant))
				event_args.push_back(std::move(tuple_variant));
			return event_args;
		}

		const DotNetArguments arguments{args, arg_count, &signature};
		event_args.reserve(arg_count);
		for (uint32_t i = 0; i < arg_count; ++i)
		{
			RBX::Reflection::Variant value;
			if (arguments.get_varint(static_cast<int>(i) + 1, value))
				event_args.push_back(std::move(value));
		}
		return event_args;
	}

	void release_fire_args(RBX::Reflection::EventArguments& event_args)
	{
		for (auto& value : event_args)
		{
			if (!value.is_void() && value.type().type_id == RBX::Reflection::TypeId::Tuple)
				std::destroy_at(static_cast<std::shared_ptr<const RBX::Reflection::Tuple>*>(value.storage()));
		}
	}

#if RML_ENABLE_LUAU
	static luau::Value to_luau_value(const InteropVariant& value)
	{
		switch (value.tag)
		{
		case InteropValueTag::Bool: return value.as_bool;
		case InteropValueTag::Int64: return static_cast<double>(value.as_int64);
		case InteropValueTag::Double: return value.as_double;
		case InteropValueTag::Float: return static_cast<double>(value.as_float);
		case InteropValueTag::String: return std::string{value.as_string ? value.as_string : ""};
		case InteropValueTag::Instance: return luau::InstanceHandle{value.as_instance};
		case InteropValueTag::LuauRef: return luau::LuauRefHandle{static_cast<luau::RefId>(value.as_uint64)};
		default: return luau::Value{};
		}
	}

	static void dispatch_luau_chunk(const int32_t data_model_type, const char* chunk_name, const char* source, ManagedYieldCallback callback, void* state, const bool want_result)
	{
		auto* const runtime = luau::script_runtime();
		if (!runtime)
		{
			callback(state, nullptr, "luau runtime is not available");
			return;
		}

		auto* const host = runtime->host(static_cast<RBX::DataModelType>(data_model_type));
		if (!host)
		{
			callback(state, nullptr, "no luau host is bound to the requested data model");
			return;
		}

		std::string name = chunk_name && *chunk_name ? chunk_name : "@managed";
		auto bytecode = luau::BytecodeCache::compile(source ? source : "", name);
		if (!bytecode)
		{
			callback(state, nullptr, bytecode.error().message.c_str());
			return;
		}

		host->dispatcher().post_managed(
		    luau::RunChunk{.chunk_name = std::move(name), .bytecode = std::move(*bytecode), .owner = {}, .want_result = want_result},
		    luau::ManagedCompletion{callback, state});
	}

	static luau::ScriptHost* luau_host_for_ref(const luau::RefId id)
	{
		auto* const runtime = luau::script_runtime();
		return runtime ? runtime->host_for_ref(id) : nullptr;
	}
#endif

	void RobloxInteropProvider::populate(InteropTable& table)
	{
		table.reflection_invoke = [](const uintptr_t instance_ptr, const char* function_name, const InteropVariant* args, const uint32_t arg_count, InteropVariant* out_result) {
			if (out_result)
			{
				out_result->tag = InteropValueTag::Null;
				out_result->as_uint64 = 0;
			}

			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !function_name)
				{
					return;
				}

				const auto* descriptor = instance->get_descriptor().find_function(function_name);

				if (!descriptor)
					return;

				InteropVariant local_result{};
				auto& result = out_result ? *out_result : local_result;
				invoke_reflection_function(instance, *descriptor, args, arg_count, result);

			}
			catch (const std::exception& e)
			{
				RML_ERROR("invoke('{}') failed: {}", function_name ? function_name : "?", e.what());
			}
			catch (...)
			{
				RML_ERROR("invoke('{}') failed: unknown exception", function_name ? function_name : "?");
			}
		};

		table.reflection_invoke_async = [](const uintptr_t instance_ptr, const char* function_name, const InteropVariant* args, const uint32_t arg_count, ManagedYieldCallback callback, void* state) {
			if (!callback)
				return;

			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !function_name)
				{
					callback(state, nullptr, "invalid instance or function name");
					return;
				}

				auto& class_descriptor = instance->get_descriptor();

				if (const auto* yield_descriptor = class_descriptor.find_yield_function_descriptor(function_name))
				{
					DotNetArguments arguments{args, arg_count, &yield_descriptor->get_signature()};
					YieldInvocation::dispatch(*yield_descriptor, *instance, arguments, callback, state);
					return;
				}

				const auto* descriptor = class_descriptor.find_function(function_name);
				if (!descriptor)
				{
					callback(state, nullptr, "function not found");
					return;
				}

				InteropVariant result{};
				invoke_reflection_function(instance, *descriptor, args, arg_count, result);
				callback(state, &result, nullptr);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("invoke_async('{}') failed: {}", function_name ? function_name : "?", e.what());
				callback(state, nullptr, e.what());
			}
			catch (...)
			{
				RML_ERROR("invoke_async('{}') failed: unknown exception", function_name ? function_name : "?");
				callback(state, nullptr, "unknown exception");
			}
		};

		table.reflection_get_property = [](const uintptr_t instance_ptr, const char* property_name, InteropVariant* out_value) {
			if (!out_value)
				return;
			*out_value = null_value();

			try
			{
				const auto* instance = as_instance(instance_ptr);
				if (!instance || !property_name)
					return;

				const auto property_descriptor = instance->get_descriptor().find_property(property_name);
				if (!property_descriptor)
					return;

				*out_value = TypeMarshaler::encode_property(property_descriptor, instance);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("get_property('{}') failed: {}", property_name ? property_name : "?", e.what());
			}
			catch (...)
			{
				RML_ERROR("get_property('{}') failed: unknown exception", property_name ? property_name : "?");
			}
		};

		table.reflection_set_property = [](const uintptr_t instance_ptr, const char* property_name, const InteropVariant* value) {
			if (!value)
				return;

			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !property_name)
					return;

				const auto property_descriptor = instance->get_descriptor().find_property(property_name);

				if (!property_descriptor)
					return;

				(void)TypeMarshaler::decode_property(property_descriptor, instance, *value);

			}
			catch (const std::exception& e)
			{
				RML_ERROR("set_property('{}') failed: {}", property_name ? property_name : "?", e.what());
			}
			catch (...)
			{
				RML_ERROR("set_property('{}') failed: unknown exception", property_name ? property_name : "?");
			}
		};

		table.reflection_event_connect = [](const uintptr_t instance_ptr, const char* event_name, const ManagedEventCallback callback, void* state) -> uintptr_t {
			try
			{

				if (!callback)
					return 0;

				auto* instance = as_instance(instance_ptr);
				if (!instance || !event_name)
					return 0;

				const auto* event_descriptor = instance->get_descriptor().find_event(event_name);
				if (!event_descriptor)
					return 0;

				const auto slot = std::make_shared<ManagedEventSlot>(callback, state);

				auto holder = std::make_unique<ManagedEventConnection>();
				holder->slot = slot;
				holder->connection = event_descriptor->connect_generic(instance, slot);

				return reinterpret_cast<uintptr_t>(holder.release());
			}
			catch (const std::exception& e)
			{
				RML_ERROR("event_connect('{}') failed: {}", event_name ? event_name : "?", e.what());
				return 0;
			}
			catch (...)
			{
				RML_ERROR("event_connect('{}') failed: unknown exception", event_name ? event_name : "?");
				return 0;
			}
		};

		table.reflection_event_disconnect = [](const uintptr_t connection_handle) {
			const auto* holder = reinterpret_cast<ManagedEventConnection*>(connection_handle);
			if (!holder)
				return;

			try
			{
				holder->connection.disconnect();
			}
			catch (const std::exception& e)
			{
				RML_ERROR("event_disconnect failed: {}", e.what());
			}
			catch (...)
			{
				RML_ERROR("event_disconnect failed: unknown exception");
			}

			delete holder;
		};

		table.object_create_by_name = [](const char* class_name, const int creator_role) -> uintptr_t {
			if (!class_name)
				return 0;

			try
			{
				const auto atom = g_pointers->m_roblox_pointers.get_string_atom(class_name);

				struct CreatedInstance
				{
					uintptr_t instance;
					uintptr_t control_block;
				};

				CreatedInstance created{};
				memory::call_returning<CreatedInstance>(
				    reinterpret_cast<void*>(g_pointers->m_roblox_pointers.object_create_by_name),
				    created,
				    uintptr_t{0},
				    static_cast<uintptr_t>(atom),
				    static_cast<uint32_t>(creator_role));

				const auto out = created.instance;
				if (!out)
				{
					RML_ERROR("creator_create_by_name('{}') failed: null instance", class_name);
					return 0;
				}

				return out;
			}
			catch (const std::exception& e)
			{
				RML_ERROR("create_by_name('{}') failed: {}", class_name, e.what());
				return 0;
			}
			catch (...)
			{
				RML_ERROR("create_by_name('{}') failed: unknown exception", class_name);
				return 0;
			}
		};

		table.managed_log = [](const int32_t level, const char* utf8, const int32_t len) {
			if (!utf8 || len < 0)
				return;

			const std::string_view message{utf8, static_cast<size_t>(len)};

			switch (level)
			{
			case 0: rml_scoped_logger()->log(spdlog::level::trace, message); break;
			case 1: rml_scoped_logger()->log(spdlog::level::debug, message); break;
			case 2: rml_scoped_logger()->log(spdlog::level::info, message); break;
			case 3: rml_scoped_logger()->log(spdlog::level::warn, message); break;
			case 4: rml_scoped_logger()->log(spdlog::level::err, message); break;
			case 5: rml_scoped_logger()->log(spdlog::level::critical, message); break;
			default: rml_scoped_logger()->log(spdlog::level::info, message); break;
			}
		};

		table.free_string = [](const char* str) {
			free(const_cast<char*>(str));
		};

		table.free_native_ptr = [](const void* ptr) {
			free(const_cast<void*>(ptr));
		};

		table.mods_menu_add_action = [](const uintptr_t parent_id, const char* text, const ManagedEventCallback callback, void* state) -> uintptr_t {
			if (!text || !callback)
				return 0;

			auto* const integration = rml::qt::QtIntegration::instance();
			if (!integration)
				return 0;

			return integration->menu().add_action(parent_id, text, [callback, state] {
				callback(state, nullptr, 0);
			});
		};

		table.mods_menu_add_submenu = [](const uintptr_t parent_id, const char* text) -> uintptr_t {
			if (!text)
				return 0;

			auto* const integration = rml::qt::QtIntegration::instance();
			if (!integration)
				return 0;

			return integration->menu().add_submenu(parent_id, text);
		};

		table.mods_menu_add_separator = [](const uintptr_t parent_id) -> uintptr_t {
			auto* const integration = rml::qt::QtIntegration::instance();
			if (!integration)
				return 0;

			return integration->menu().add_separator(parent_id);
		};

		table.mods_menu_add_checkable = [](const uintptr_t parent_id, const char* text, const int initial, const ManagedEventCallback callback, void* state) -> uintptr_t {
			if (!text || !callback)
				return 0;

			auto* const integration = rml::qt::QtIntegration::instance();
			if (!integration)
				return 0;

			return integration->menu().add_checkable(parent_id, text, initial != 0, [callback, state](const bool value) {
				const InteropVariant arg = bool_value(value);
				callback(state, &arg, 1);
			});
		};

		table.mods_menu_set_item_icon = [](const uintptr_t id, const char* utf8_path) {
			if (!utf8_path)
				return;

			if (auto* const integration = rml::qt::QtIntegration::instance())
				integration->menu().set_item_icon(id, utf8_path);
		};

		table.mods_menu_remove = [](const uintptr_t id) {
			if (auto* const integration = rml::qt::QtIntegration::instance())
				integration->menu().remove(id);
		};

		table.reflection_event_fire = [](const uintptr_t instance_ptr, const char* event_name, const InteropVariant* args, const uint32_t arg_count) {
			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !event_name)
					return;

				const auto* descriptor = instance->get_descriptor().find_event(event_name);
				if (!descriptor)
					return;

				auto event_args = build_event_fire_args(descriptor, args, arg_count);
				descriptor->fire_event_generic(instance, event_args);
				release_fire_args(event_args);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("event_fire('{}') failed: {}", event_name ? event_name : "?", e.what());
			}
			catch (...)
			{
				RML_ERROR("event_fire('{}') failed: unknown exception", event_name ? event_name : "?");
			}
		};

		table.reflection_event_disconnect_all = [](const uintptr_t instance_ptr, const char* event_name) {
			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !event_name)
					return;

				if (const auto* descriptor = instance->get_descriptor().find_event(event_name))
					descriptor->disconnect_all(instance);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("event_disconnect_all('{}') failed: {}", event_name ? event_name : "?", e.what());
			}
			catch (...)
			{
				RML_ERROR("event_disconnect_all('{}') failed: unknown exception", event_name ? event_name : "?");
			}
		};

		table.reflection_event_slots = [](const uintptr_t instance_ptr, const char* event_name, uint32_t* out_count) -> uintptr_t* {
			if (out_count)
				*out_count = 0;

			try
			{
				auto* instance = as_instance(instance_ptr);
				if (!instance || !event_name)
					return nullptr;

				const auto* descriptor = instance->get_descriptor().find_event(event_name);
				if (!descriptor)
					return nullptr;

				auto snapshot = descriptor->snapshot_connections(instance);
				if (snapshot.empty())
					return nullptr;

				auto* result = static_cast<uintptr_t*>(malloc(snapshot.size() * sizeof(uintptr_t)));
				if (!result)
					return nullptr;

				for (size_t i = 0; i < snapshot.size(); ++i)
					result[i] = reinterpret_cast<uintptr_t>(new RBX::Signals::Connection(std::move(snapshot[i])));

				if (out_count)
					*out_count = static_cast<uint32_t>(snapshot.size());
				return result;
			}
			catch (...)
			{
				return nullptr;
			}
		};

		table.event_slot_fire = [](const uintptr_t instance_ptr, const char* event_name, const uintptr_t slot_handle, const InteropVariant* args, const uint32_t arg_count) {
			try
			{
				const auto* connection = reinterpret_cast<RBX::Signals::Connection*>(slot_handle);
				if (!connection)
					return;

				const auto* slot = connection->raw_slot();
				if (!slot || !slot->source)
					return;

				auto* wrapper = static_cast<RBX::Reflection::GenericSlotWrapper*>(slot->wrapper_ptr);
				if (!wrapper)
					return;

				RBX::Reflection::EventArguments event_args;
				if (const auto* instance = as_instance(instance_ptr); instance && event_name)
				{
					if (const auto* descriptor = instance->get_descriptor().find_event(event_name))
						event_args = build_event_fire_args(descriptor, args, arg_count);
				}

				wrapper->deliver(event_args);
				release_fire_args(event_args);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("event_slot_fire failed: {}", e.what());
			}
			catch (...)
			{
				RML_ERROR("event_slot_fire failed: unknown exception");
			}
		};

		table.event_slot_disconnect = [](const uintptr_t slot_handle) {
			if (const auto* connection = reinterpret_cast<RBX::Signals::Connection*>(slot_handle))
				connection->disconnect();
		};

		table.event_slot_release = [](const uintptr_t slot_handle) {
			delete reinterpret_cast<RBX::Signals::Connection*>(slot_handle);
		};

		table.luau_host_ready = [](const int32_t data_model_type) -> int32_t {
#if RML_ENABLE_LUAU
			try
			{
				auto* const runtime = luau::script_runtime();
				if (!runtime)
					return 0;

				return runtime->host(static_cast<RBX::DataModelType>(data_model_type)) ? 1 : 0;
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_host_ready failed: {}", e.what());
				return 0;
			}
			catch (...)
			{
				RML_ERROR("luau_host_ready failed: unknown exception");
				return 0;
			}
#else
			(void)data_model_type;
			return 0;
#endif
		};

		table.luau_schedule = [](const int32_t data_model_type, const char* chunk_name, const char* source, ManagedYieldCallback callback, void* state) {
			if (!callback)
				return;

#if RML_ENABLE_LUAU
			try
			{
				dispatch_luau_chunk(data_model_type, chunk_name, source, callback, state, false);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_schedule failed: {}", e.what());
				callback(state, nullptr, e.what());
			}
			catch (...)
			{
				RML_ERROR("luau_schedule failed: unknown exception");
				callback(state, nullptr, "unknown exception");
			}
#else
			(void)data_model_type;
			(void)chunk_name;
			(void)source;
			callback(state, nullptr, "luau support is disabled in this build");
#endif
		};

		table.luau_evaluate = [](const int32_t data_model_type, const char* chunk_name, const char* source, ManagedYieldCallback callback, void* state) {
			if (!callback)
				return;

#if RML_ENABLE_LUAU
			try
			{
				dispatch_luau_chunk(data_model_type, chunk_name, source, callback, state, true);
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_evaluate failed: {}", e.what());
				callback(state, nullptr, e.what());
			}
			catch (...)
			{
				RML_ERROR("luau_evaluate failed: unknown exception");
				callback(state, nullptr, "unknown exception");
			}
#else
			(void)data_model_type;
			(void)chunk_name;
			(void)source;
			callback(state, nullptr, "luau support is disabled in this build");
#endif
		};

		table.luau_ref_call = [](const uintptr_t ref_handle, const InteropVariant* args, const uint32_t arg_count, ManagedYieldCallback callback, void* state) {
			if (!callback)
				return;

#if RML_ENABLE_LUAU
			try
			{
				const auto id = static_cast<luau::RefId>(ref_handle);
				auto* const host = luau_host_for_ref(id);
				if (!host)
				{
					callback(state, nullptr, "luau reference is not owned by any live host");
					return;
				}

				luau::CallRef work{.target = id};
				if (args)
				{
					work.args.reserve(arg_count);
					for (uint32_t i = 0; i < arg_count; ++i)
						work.args.push_back(to_luau_value(args[i]));
				}

				host->dispatcher().post_managed(std::move(work), luau::ManagedCompletion{callback, state});
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_ref_call failed: {}", e.what());
				callback(state, nullptr, e.what());
			}
			catch (...)
			{
				RML_ERROR("luau_ref_call failed: unknown exception");
				callback(state, nullptr, "unknown exception");
			}
#else
			(void)ref_handle;
			(void)args;
			(void)arg_count;
			callback(state, nullptr, "luau support is disabled in this build");
#endif
		};

		table.luau_ref_index = [](const uintptr_t ref_handle, const char* key, ManagedYieldCallback callback, void* state) {
			if (!callback)
				return;

#if RML_ENABLE_LUAU
			try
			{
				if (!key)
				{
					callback(state, nullptr, "index key is null");
					return;
				}

				const auto id = static_cast<luau::RefId>(ref_handle);
				auto* const host = luau_host_for_ref(id);
				if (!host)
				{
					callback(state, nullptr, "luau reference is not owned by any live host");
					return;
				}

				host->dispatcher().post_managed(
				    luau::IndexRef{.target = id, .key = key},
				    luau::ManagedCompletion{callback, state});
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_ref_index failed: {}", e.what());
				callback(state, nullptr, e.what());
			}
			catch (...)
			{
				RML_ERROR("luau_ref_index failed: unknown exception");
				callback(state, nullptr, "unknown exception");
			}
#else
			(void)ref_handle;
			(void)key;
			callback(state, nullptr, "luau support is disabled in this build");
#endif
		};

		table.luau_ref_release = [](const uintptr_t ref_handle) {
#if RML_ENABLE_LUAU
			try
			{
				const auto id = static_cast<luau::RefId>(ref_handle);
				if (auto* const host = luau_host_for_ref(id))
					host->dispatcher().post(luau::ReleaseRef{.target = id});
			}
			catch (const std::exception& e)
			{
				RML_ERROR("luau_ref_release failed: {}", e.what());
			}
			catch (...)
			{
				RML_ERROR("luau_ref_release failed: unknown exception");
			}
#else
			(void)ref_handle;
#endif
		};

		verify_populated(table);
	}
} // namespace rml::dotnet
