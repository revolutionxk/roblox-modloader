#pragma once

#include "RobloxModLoader/internal/engine_abi.hpp"
#include "RobloxModLoader/roblox/reflection/array_view.hpp"
#include "RobloxModLoader/roblox/reflection/creatable.hpp"
#include "RobloxModLoader/roblox/reflection/descriptor.hpp"
#include "RobloxModLoader/roblox/util/standard_out.hpp"
#include "lua.h"
#include "lualib.h"

#include <cstdarg>
#include <cstdint>
#include <vector>

namespace RBX::Security
{
	enum class Identity : std::uint64_t;
}

namespace RBX::Signals
{
	struct Signal;
}

namespace RBX::Graphics
{
	class DeviceContext;
	class Framebuffer;
	class VisualEngine;
}

namespace RBX
{
	class Name;
}

namespace RBX::Reflection
{
	class ClassDescriptor;
	class PropertyDescriptor;
	class EventDescriptor;
	class FunctionDescriptor;
	class YieldFunctionDescriptor;
	class CallbackDescriptor;
}

namespace functions
{
	using get_string_atom = uintptr_t (*)(const char* name);
	using name_declare = const RBX::Name* (*)(const char* name);
	using slots_holder_release = void (*)(RBX::Signals::Signal* holder);
	using descriptor_lookup = uintptr_t* (*)(uintptr_t class_descriptor_hash, uintptr_t* member_descriptor_hash);
	using get_scheduler = uintptr_t (*)();
	using print = void(RML_ENGINE_CALL*)(RBX::MessageType level, const char* fmt, ...);
	using luaH_new = void*(RML_ENGINE_CALL*)(void* L, int32_t narray, int32_t nhash);
	using freeblock = void(RML_ENGINE_CALL*)(lua_State* L, int32_t sizeClass, void* block);
	using lua_pushvalue = void*(RML_ENGINE_CALL*)(lua_State * L, int idx);
	using luaE_newthread = lua_State*(RML_ENGINE_CALL*)(lua_State * L);
	using rbx_thread_identity_context = void*(RML_ENGINE_CALL*)(lua_State* L);
	using luau_execute = void(RML_ENGINE_CALL*)(lua_State* L);
	using luau_load = lua_Status(RML_ENGINE_CALL*)(lua_State* L, const char* chunkname, const char* data, size_t size, int env);
	using lua_setfield = void(RML_ENGINE_CALL*)(lua_State* L, int idx, const char* k);
	using luaD_rawrunprotected = int(RML_ENGINE_CALL*)(lua_State* L, void (*PFunc)(lua_State*, void*), void* ud);
	using lua_newthread = lua_State*(RML_ENGINE_CALL*)(lua_State * L);
	using luaD_throw = void(RML_ENGINE_CALL*)(lua_State* L, int errcode);
	using get_global_state = lua_State*(RML_ENGINE_CALL*)(void* script_context, const RBX::Security::Identity* identity, const uint64_t* script);
	using object_create_by_name = uintptr_t (*)(uintptr_t* out, uintptr_t engine_context, uintptr_t name, uint32_t creator_role);
	using instance_bridge_push = void(RML_ENGINE_CALL*)(lua_State* L, uintptr_t instance);
	using task_defer = int(RML_ENGINE_CALL*)(lua_State* L);
	using build_menu_bar_from_dom = void*(RML_ENGINE_CALL*)(void* out_menu_bar, void* dom, void* context);
	using signal_disconnect = void(RML_ENGINE_CALL*)(void* slot);
	using signal_slot_free = void(RML_ENGINE_CALL*)(void* slot);
	using signal_mutex_get = void*(RML_ENGINE_CALL*)();
	using global_init = void (*)();
	using class_descriptor_ctor = void (*)(void* self, void* base, const char* name, std::uint32_t instance_id, std::uint64_t stable_id, bool a6, bool a7, const void* attributes, std::uint32_t protection, const std::uint32_t* memory_category, RBX::ArrayView<const RBX::Reflection::PropertyDescriptor*> properties, RBX::ArrayView<const RBX::Reflection::EventDescriptor*> events, RBX::ArrayView<const RBX::Reflection::FunctionDescriptor*> functions, RBX::ArrayView<const RBX::Reflection::YieldFunctionDescriptor*> yield_functions, RBX::ArrayView<const RBX::Reflection::CallbackDescriptor*> callbacks);
	using class_descriptor_all_classes = std::vector<RBX::Reflection::ClassDescriptor*>* (*)();
	using creatable_get_creator = const RBX::ICreator* (*)(const RBX::Name* name);
	using creatable_register_creator = void (*)(const RBX::Reflection::ClassDescriptor* descriptor, const RBX::ICreator* creator);
	using instance_ctor = void (*)(void* self, const RBX::ForceConstructionInCreatable* force, const char* name);
	using create_instance_impl = void* (*)(std::uint32_t stable_id, std::size_t size, std::size_t align, std::uint32_t memory_category, void* (*construct)(void* memory, const void* args), const void* args);
	using property_descriptor_ctor = void (*)(void* self, void* class_descriptor, const void* type, const char* name, const char* category, const void* attributes, std::uint32_t protection_get, std::uint32_t protection_set, bool a9);
	// Attributes by value: x4/x5 on arm64, a pointer to a caller copy on Win64.
	using function_descriptor_ctor = void (*)(void* self, void* class_descriptor, const char* name, std::uint32_t protection, RBX::Reflection::Descriptor::Attributes attributes);
	using event_descriptor_ctor = void (*)(void* self, void* class_descriptor, const char* name, std::uint32_t protection, const void* attributes);
	using visual_engine_begin_render = RBX::Graphics::DeviceContext* (*)(RBX::Graphics::VisualEngine* self);
	using scene_manager_render_scene = void (*)(void* self, RBX::Graphics::DeviceContext* context, RBX::Graphics::Framebuffer* target, const void* camera, RBX::ArrayView<RBX::Graphics::Framebuffer*> extra, std::uint32_t capture_mode);

	using lua_gettop = int(RML_ENGINE_CALL*)(lua_State* L);
	using lua_settop = void(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_resume = int(RML_ENGINE_CALL*)(lua_State* L, lua_State* from, int narg);
	using lua_pcall = int(RML_ENGINE_CALL*)(lua_State* L, int nargs, int nresults, int errfunc);
	using lua_call = void(RML_ENGINE_CALL*)(lua_State* L, int nargs, int nresults);
	using lua_yield = int(RML_ENGINE_CALL*)(lua_State* L, int nresults);
	using lua_type = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_pushnil = void(RML_ENGINE_CALL*)(lua_State* L);
	using lua_pushnumber = void(RML_ENGINE_CALL*)(lua_State* L, double n);
	using lua_pushinteger = void(RML_ENGINE_CALL*)(lua_State* L, int n);
	using lua_pushboolean = void(RML_ENGINE_CALL*)(lua_State* L, int b);
	using lua_pushlstring = void(RML_ENGINE_CALL*)(lua_State* L, const char* s, size_t len);
	using lua_pushstring = void(RML_ENGINE_CALL*)(lua_State* L, const char* s);
	using lua_pushcclosurek = void(RML_ENGINE_CALL*)(lua_State* L, lua_CFunction fn, const char* debugname, int nup, lua_Continuation cont);
	using lua_tolstring = const char*(RML_ENGINE_CALL*)(lua_State* L, int idx, size_t* len);
	using lua_tonumberx = double(RML_ENGINE_CALL*)(lua_State* L, int idx, int* isnum);
	using lua_tointegerx = int(RML_ENGINE_CALL*)(lua_State* L, int idx, int* isnum);
	using lua_toboolean = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_topointer = const void*(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_objlen = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_createtable = void(RML_ENGINE_CALL*)(lua_State* L, int narr, int nrec);
	using lua_getfield = int(RML_ENGINE_CALL*)(lua_State* L, int idx, const char* k);
	using lua_gettable = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_settable = void(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_rawget = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_rawgeti = int(RML_ENGINE_CALL*)(lua_State* L, int idx, int n);
	using lua_rawset = void(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_rawseti = void(RML_ENGINE_CALL*)(lua_State* L, int idx, int n);
	using lua_rawgetfield = int(RML_ENGINE_CALL*)(lua_State* L, int idx, const char* k);
	using lua_rawsetfield = void(RML_ENGINE_CALL*)(lua_State* L, int idx, const char* k);
	using lua_rawiter = int(RML_ENGINE_CALL*)(lua_State* L, int idx, int iter);
	using lua_next = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_ref = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_unref = void(RML_ENGINE_CALL*)(lua_State* L, int ref);
	using lua_getmetatable = int(RML_ENGINE_CALL*)(lua_State* L, int objindex);
	using lua_setmetatable = int(RML_ENGINE_CALL*)(lua_State* L, int objindex);
	using lua_getreadonly = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_setreadonly = void(RML_ENGINE_CALL*)(lua_State* L, int idx, int enabled);
	using lua_setsafeenv = void(RML_ENGINE_CALL*)(lua_State* L, int idx, int enabled);
	using lua_getupvalue = const char*(RML_ENGINE_CALL*)(lua_State* L, int funcindex, int n);
	using lua_setupvalue = const char*(RML_ENGINE_CALL*)(lua_State* L, int funcindex, int n);
	using lua_getinfo = int(RML_ENGINE_CALL*)(lua_State* L, int level, const char* what, lua_Debug* ar);
	using lua_error = void(RML_ENGINE_CALL*)(lua_State* L);
	using lua_break = int(RML_ENGINE_CALL*)(lua_State* L);
	using lua_isnumber = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_isstring = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_iscfunction = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_isuserdata = int(RML_ENGINE_CALL*)(lua_State* L, int idx);
	using lua_insert = void(RML_ENGINE_CALL*)(lua_State* L, int idx);

	using luaL_register = void(RML_ENGINE_CALL*)(lua_State* L, const char* libname, const luaL_Reg* l);
	using luaL_errorL = void(RML_ENGINE_CALL*)(lua_State* L, const char* fmt, ...);
	using luaL_typeerrorL = void(RML_ENGINE_CALL*)(lua_State* L, int narg, const char* tname);
	using luaL_argerrorL = void(RML_ENGINE_CALL*)(lua_State* L, int narg, const char* extramsg);
	using luaL_where = void(RML_ENGINE_CALL*)(lua_State* L, int lvl);
	using luaL_checkinteger = int(RML_ENGINE_CALL*)(lua_State* L, int narg);
	using luaL_checknumber = double(RML_ENGINE_CALL*)(lua_State* L, int narg);
	using luaL_checklstring = const char*(RML_ENGINE_CALL*)(lua_State* L, int narg, size_t* len);
	using luaL_checktype = void(RML_ENGINE_CALL*)(lua_State* L, int narg, int t);
	using luaL_checkany = void(RML_ENGINE_CALL*)(lua_State* L, int narg);
	using luaL_optinteger = int(RML_ENGINE_CALL*)(lua_State* L, int narg, int def);
	using luaL_optboolean = int(RML_ENGINE_CALL*)(lua_State* L, int narg, int def);
	using luaL_sandboxthread = void(RML_ENGINE_CALL*)(lua_State* L);
	
	using luaF_newLclosure = void*(RML_ENGINE_CALL*)(lua_State* L, int nelems, void* e, void* p);
	using luaF_newCclosure = void*(RML_ENGINE_CALL*)(lua_State* L, int nelems, void* e);
	using luaC_barrierf = void(RML_ENGINE_CALL*)(lua_State* L, void* o, void* v);
	using luaC_barrierback = void(RML_ENGINE_CALL*)(lua_State* L, void* o, void** gclist);
	using luaC_enumheap = void(RML_ENGINE_CALL*)(lua_State* L, void* context,
	                                             void (*node)(void* ctx, void* ptr, uint8_t tt, uint8_t memcat, size_t size, const char* name),
	                                             void (*edge)(void* ctx, void* from, void* to, const char* name));
	using luaM_visitgco = void(RML_ENGINE_CALL*)(lua_State* L, void* context, bool (*visitor)(void* ctx, void* page, void* gco));
	using luaC_barriertable = void(RML_ENGINE_CALL*)(lua_State* L, void* t, void* v);
	using luaH_setnum = void*(RML_ENGINE_CALL*)(lua_State* L, void* t, int key);
	using luaA_pseudo2addr = void*(RML_ENGINE_CALL*)(lua_State* L, int idx);
}
