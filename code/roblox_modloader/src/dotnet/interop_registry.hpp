#pragma once

#include "RobloxModLoader/internal/platform.hpp"
#include <cstddef>
#include <cstdint>

#if defined(RML_WINDOWS)
	#define RML_INTEROP_CALL __cdecl
#else
	#define RML_INTEROP_CALL
#endif

namespace rml::dotnet
{
	enum class InteropValueTag : uint8_t
	{
		Null = 0,
		Bool = 1,
		Int64 = 2,
		Double = 3,
		Float = 4,
		String = 5,
		Instance = 6,
		InstanceArray = 7,
		Blittable = 8,
		Tuple = 9,
		LuauRef = 10,
	};

	struct alignas(8) InteropVariant
	{
		InteropValueTag tag;
		union {
			bool as_bool;
			int64_t as_int64;
			uint64_t as_uint64;
			double as_double;
			float as_float;
			uintptr_t as_instance;
			char* as_string;
		};
	};

	static_assert(sizeof(InteropVariant) == 16, "InteropVariant must be 16 bytes (managed mirror is Size = 16).");
	static_assert(alignof(InteropVariant) == 8, "InteropVariant must be 8-byte aligned.");
	static_assert(offsetof(InteropVariant, tag) == 0, "InteropVariant.tag must be at offset 0.");
	static_assert(offsetof(InteropVariant, as_uint64) == 8, "InteropVariant union must start at offset 8.");

	using ManagedEventCallback = void(RML_INTEROP_CALL*)(void* state, const InteropVariant* args, uint32_t arg_count);
	using ManagedYieldCallback = void(RML_INTEROP_CALL*)(void* state, const InteropVariant* result, const char* error_message);

	struct alignas(8) InteropTable
	{
		uint32_t version;
		uint32_t size;

		void(RML_INTEROP_CALL* reflection_invoke)(uintptr_t instance, const char* function_name, const InteropVariant* args, uint32_t arg_count, InteropVariant* out_result);
		void(RML_INTEROP_CALL* reflection_get_property)(uintptr_t instance, const char* property_name, InteropVariant* out_value);
		void(RML_INTEROP_CALL* reflection_set_property)(uintptr_t instance, const char* property_name, const InteropVariant* value);

		uintptr_t(RML_INTEROP_CALL* reflection_event_connect)(uintptr_t instance, const char* event_name, ManagedEventCallback callback, void* state);
		void(RML_INTEROP_CALL* reflection_event_disconnect)(uintptr_t connection_handle);

		uintptr_t(RML_INTEROP_CALL* object_create_by_name)(const char* class_name, int creator_role);

		void(RML_INTEROP_CALL* managed_log)(int32_t level, const char* utf8, int32_t len);

		void(RML_INTEROP_CALL* free_string)(const char* str);
		void(RML_INTEROP_CALL* free_native_ptr)(const void* ptr);
		
		uintptr_t(RML_INTEROP_CALL* mods_menu_add_action)(uintptr_t parent_id, const char* text, ManagedEventCallback callback, void* state);
		uintptr_t(RML_INTEROP_CALL* mods_menu_add_submenu)(uintptr_t parent_id, const char* text);
		uintptr_t(RML_INTEROP_CALL* mods_menu_add_separator)(uintptr_t parent_id);
		uintptr_t(RML_INTEROP_CALL* mods_menu_add_checkable)(uintptr_t parent_id, const char* text, int initial, ManagedEventCallback callback, void* state);
		void(RML_INTEROP_CALL* mods_menu_set_item_icon)(uintptr_t id, const char* utf8_path);
		void(RML_INTEROP_CALL* mods_menu_remove)(uintptr_t id);

		void(RML_INTEROP_CALL* reflection_invoke_async)(uintptr_t instance, const char* function_name, const InteropVariant* args, uint32_t arg_count, ManagedYieldCallback callback, void* state);

		void(RML_INTEROP_CALL* reflection_event_fire)(uintptr_t instance, const char* event_name, const InteropVariant* args, uint32_t arg_count);
		void(RML_INTEROP_CALL* reflection_event_disconnect_all)(uintptr_t instance, const char* event_name);

		uintptr_t*(RML_INTEROP_CALL* reflection_event_slots)(uintptr_t instance, const char* event_name, uint32_t* out_count);
		void(RML_INTEROP_CALL* event_slot_fire)(uintptr_t instance, const char* event_name, uintptr_t slot_handle, const InteropVariant* args, uint32_t arg_count);
		void(RML_INTEROP_CALL* event_slot_disconnect)(uintptr_t slot_handle);
		void(RML_INTEROP_CALL* event_slot_release)(uintptr_t slot_handle);

		int32_t(RML_INTEROP_CALL* luau_host_ready)(int32_t data_model_type);
		void(RML_INTEROP_CALL* luau_schedule)(int32_t data_model_type, const char* chunk_name, const char* source, ManagedYieldCallback callback, void* state);
		void(RML_INTEROP_CALL* luau_evaluate)(int32_t data_model_type, const char* chunk_name, const char* source, ManagedYieldCallback callback, void* state);
		void(RML_INTEROP_CALL* luau_ref_call)(uintptr_t ref_handle, const InteropVariant* args, uint32_t arg_count, ManagedYieldCallback callback, void* state);
		void(RML_INTEROP_CALL* luau_ref_index)(uintptr_t ref_handle, const char* key, ManagedYieldCallback callback, void* state);
		void(RML_INTEROP_CALL* luau_ref_release)(uintptr_t ref_handle);

		uint8_t(RML_INTEROP_CALL* instance_retain)(uintptr_t instance);
		void(RML_INTEROP_CALL* instance_release)(uintptr_t instance);
	};

	inline constexpr uint32_t RML_INTEROP_VERSION = 11;

	class InteropRegistry
	{
	public:
		InteropRegistry();

		[[nodiscard]] InteropTable* table() noexcept
		{
			return &m_table;
		}
		[[nodiscard]] const InteropTable* table() const noexcept
		{
			return &m_table;
		}

	private:
		InteropTable m_table{};
	};

} // namespace rml::dotnet
