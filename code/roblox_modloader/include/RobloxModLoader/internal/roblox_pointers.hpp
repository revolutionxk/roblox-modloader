#pragma once

#include "RobloxModLoader/internal/platform.hpp"
#include "function_types.hpp"

#include <vector>

#if defined(RML_WINDOWS)
	#ifndef NOMINMAX
		#define NOMINMAX
	#endif

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif

#include "RobloxModLoader/rml_export.hpp"

namespace RBX::Reflection
{
	class Type;
}
template<typename T>
class HashTable;

// needed for serialization of the pointers cache
#pragma pack(push, 1)
struct RobloxPointers
{
	void* m_rbx_crash;
	functions::print print;

	void* m_profile_log;

	functions::get_scheduler get_scheduler;

	// Script Context
	void* resume_waiting_scripts;

	functions::get_string_atom get_string_atom;
	functions::name_declare name_declare;
	functions::descriptor_lookup descriptor_lookup;
	
	std::uint64_t member_table_offset;

	// Lua Functions
	functions::luau_execute luau_execute;
	functions::luau_load luau_load;
	functions::luaE_newthread luaE_newthread;
	functions::rbx_thread_identity_context rbx_thread_identity_context;
	functions::lua_pushvalue lua_pushvalue;
	functions::luaH_new luaH_new;
	functions::freeblock freeblock;
	functions::lua_newthread lua_newthread;
	functions::luaD_rawrunprotected luaD_rawrunprotected;
	functions::luaD_throw luaD_throw;
	functions::lua_setfield lua_setfield;
	struct LuaNode* luaH_dummynode;

#undef luaO_nilobject // ¯\_(ツ)_/¯
	void* luaO_nilobject;

	functions::task_defer task_defer;
	functions::get_global_state get_global_state;
	
	functions::lua_gettop lua_gettop;
	functions::lua_settop lua_settop;
	functions::lua_resume lua_resume;
	functions::lua_pcall lua_pcall;
	functions::lua_call lua_call;
	functions::lua_yield lua_yield;
	functions::lua_type lua_type;
	functions::lua_pushnil lua_pushnil;
	functions::lua_pushnumber lua_pushnumber;
	functions::lua_pushinteger lua_pushinteger;
	functions::lua_pushboolean lua_pushboolean;
	functions::lua_pushlstring lua_pushlstring;
	functions::lua_pushstring lua_pushstring;
	functions::lua_pushcclosurek lua_pushcclosurek;
	functions::lua_tolstring lua_tolstring;
	functions::lua_tonumberx lua_tonumberx;
	functions::lua_tointegerx lua_tointegerx;
	functions::lua_toboolean lua_toboolean;
	functions::lua_topointer lua_topointer;
	functions::lua_objlen lua_objlen;
	functions::lua_createtable lua_createtable;
	functions::lua_getfield lua_getfield;
	functions::lua_gettable lua_gettable;
	functions::lua_settable lua_settable;
	functions::lua_rawget lua_rawget;
	functions::lua_rawgeti lua_rawgeti;
	functions::lua_rawset lua_rawset;
	functions::lua_rawseti lua_rawseti;
	functions::lua_rawgetfield lua_rawgetfield;
	functions::lua_rawsetfield lua_rawsetfield;
	functions::lua_rawiter lua_rawiter;
	functions::lua_next lua_next;
	functions::lua_ref lua_ref;
	functions::lua_unref lua_unref;
	functions::lua_getmetatable lua_getmetatable;
	functions::lua_setmetatable lua_setmetatable;
	functions::lua_getreadonly lua_getreadonly;
	functions::lua_setreadonly lua_setreadonly;
	functions::lua_setsafeenv lua_setsafeenv;
	functions::lua_getupvalue lua_getupvalue;
	functions::lua_setupvalue lua_setupvalue;
	functions::lua_getinfo lua_getinfo;
	functions::lua_error lua_error;
	functions::lua_break lua_break;
	functions::lua_isnumber lua_isnumber;
	functions::lua_isstring lua_isstring;
	functions::lua_iscfunction lua_iscfunction;
	functions::lua_isuserdata lua_isuserdata;
	functions::lua_insert lua_insert;

	functions::luaL_register luaL_register;
	functions::luaL_errorL luaL_errorL;
	functions::luaL_typeerrorL luaL_typeerrorL;
	functions::luaL_argerrorL luaL_argerrorL;
	functions::luaL_where luaL_where;
	functions::luaL_checkinteger luaL_checkinteger;
	functions::luaL_checknumber luaL_checknumber;
	functions::luaL_checklstring luaL_checklstring;
	functions::luaL_checktype luaL_checktype;
	functions::luaL_checkany luaL_checkany;
	functions::luaL_optinteger luaL_optinteger;
	functions::luaL_optboolean luaL_optboolean;
	functions::luaL_sandboxthread luaL_sandboxthread;
	
	functions::luaF_newLclosure luaF_newLclosure;
	functions::luaF_newCclosure luaF_newCclosure;
	functions::luaC_barrierf luaC_barrierf;
	functions::luaC_barrierback luaC_barrierback;
	functions::luaC_enumheap luaC_enumheap;
	functions::luaM_visitgco luaM_visitgco;
	functions::luaC_barriertable luaC_barriertable;
	functions::luaH_setnum luaH_setnum;
	functions::luaA_pseudo2addr luaA_pseudo2addr;

	functions::object_create_by_name object_create_by_name;
	functions::instance_bridge_push instance_bridge_push;
	functions::build_menu_bar_from_dom build_menu_bar_from_dom;

	functions::signal_disconnect signal_disconnect;
	functions::signal_slot_free signal_slot_free;
	functions::signal_mutex_get signal_mutex_get;
	functions::slots_holder_release slots_holder_release;

	functions::global_init global_init;
	functions::class_descriptor_ctor class_descriptor_ctor;
	functions::class_descriptor_all_classes class_descriptor_all_classes;
	functions::creatable_get_creator creatable_get_creator;
	functions::instance_ctor instance_ctor;
	functions::create_instance_impl create_instance_impl;
	functions::property_descriptor_ctor property_descriptor_ctor;
	functions::function_descriptor_ctor function_descriptor_ctor;
	functions::event_descriptor_ctor event_descriptor_ctor;
	functions::visual_engine_begin_render visual_engine_begin_render;
	functions::scene_manager_render_scene scene_manager_render_scene;

	const std::vector<const RBX::Reflection::Type*>* type_registry;
	// Windows files mod creators through it; macOS keeps the getCreator hook and leaves it null.
	functions::creatable_register_creator creatable_register_creator;
};
#pragma pack(pop)
static_assert(sizeof(RobloxPointers) % 8 == 0, "Pointers are not properly aligned");

RML_EXPORT RobloxPointers* get_roblox_pointers();
