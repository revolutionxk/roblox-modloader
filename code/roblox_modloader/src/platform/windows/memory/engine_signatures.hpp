#pragma once

#include "RobloxModLoader/memory/all.hpp"
#include "pointers.hpp"

namespace rml
{
	constexpr auto Pointers::get_early_batch()
	{
		// clang-format off
		return memory::make_batch<
		    {
		        "RBX_GLOBAL_INIT",
		        memory::referencing("[FLog::Error] Unable to initialize libsodium."),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.global_init = ptr.as<functions::global_init>();
		        },
		    }
		>();
		// clang-format on
	}

	constexpr auto Pointers::get_roblox_batch()
	{
		// clang-format off
		constexpr auto batch_and_hash = memory::make_batch<
		    // Lua Functions
		    {
		        "LUA_LOAD",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC 80 00 00 00 49 8B E9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luau_load = ptr.as<functions::luau_load>();
		        },
		    },
		    {
		        "LUAU_EXECUTE",
		        memory::referencing("cannot resume dead coroutine").call(2).call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luau_execute = ptr.as<functions::luau_execute>();
		        },
		    },
		    {
		        "LUAE_NEWTHREAD",
		        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 44 0F B6 41 ? 33 F6 48 8B F9 89 74 24 ? 44 8D 4E ? 41 8D 51 ? E8",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaE_newthread = ptr.as<functions::luaE_newthread>();
		        },
		    },
		    {
		        "RBX_THREAD_IDENTITY_CONTEXT",
		        "40 53 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 8D 0D ? ? ? ? 48 89 48 ? 48 89 58 ? 48 83 C4 20 5B C3",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.rbx_thread_identity_context = ptr.as<functions::rbx_thread_identity_context>();
		        },
		    },
		    {
		        "LUAH_NEW",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 45 33 F6 41 8B F0 44 0F B6 41 ? 8B EA 48 8B F9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaH_new = ptr.as<functions::luaH_new>();
		        },
		    },
		    {
		        "FREEBLOCK",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.freeblock = ptr.as<functions::freeblock>();
		        },
		    },
		    {"LUAD_RAWRUNPROTECTED",
		        "48 89 4C 24 ? 48 83 EC ? 48 8B C2 49 8B D0 FF D0 33 C0 EB 04 8B 44 24 48 48 83 C4 ? C3",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaD_rawrunprotected = ptr.as<functions::luaD_rawrunprotected>();
		        }},
		    {"LUAD_THROW",
		        memory::from("LUA_ERROR").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaD_throw = ptr.as<functions::luaD_throw>();
		        }},
		    {"LUA_SETFIELD",
		        memory::referencing("charpattern").call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_setfield = ptr.as<functions::lua_setfield>();
		        }},
		    {
		        "PROFILE_LOG",
		        "40 55 56 57 41 56 48 83 EC ? 48 8B 05",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.m_profile_log = ptr.as<void*>();
		        },
		    },
		    {
		        "OBJECT_CREATE_BY_NAME",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 41 8B F9 48 8B EA",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.object_create_by_name = ptr.as<functions::object_create_by_name>();
		        },
		    },
		    {
		        "INSTANCE_BRIDGE_PUSH",
		        "48 89 5C 24 ? 57 48 83 EC ? 48 8B FA 48 8B D9 E8 ? ? ? ? 48 8B CB 84 C0 74 ? 48 8B D7",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.instance_bridge_push = ptr.as<functions::instance_bridge_push>();
		        },
		    },
		    {"DESCRIPTOR_LOOKUP",
		        "48 83 EC 18 ? ? ? 4C 8B D9 75",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.descriptor_lookup = ptr.as<functions::descriptor_lookup>();
		        }
		    },
		    {"MEMBER_TABLE_OFFSET",
	        "48 8B 4B ? E8 ? ? ? ? 48 8D 8F ? ? ? ? 48 89 44 24 ? 48 8D 54 24 ? E8",
	        [](const memory::handle ptr) {
		        g_pointers->m_roblox_pointers.member_table_offset = *ptr.add(12).as<std::uint32_t*>();
	        }
	    },
	    {"GET_STRING_ATOM",
		        "48 89 5C 24 ? 57 48 83 EC 20 48 8B 1D ? ? ? ? 48 8B F9 48 85 DB",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.get_string_atom = ptr.as<functions::get_string_atom>();
		        }
		    },
		    {"MENU_BUILD_FROM_DOM",
		        "48 89 5C 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC 50 01 00 00 49 8B D8",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.build_menu_bar_from_dom = ptr.as<functions::build_menu_bar_from_dom>();
		        }
		    },
		    {"SIGNAL_DISCONNECT",
		        "48 89 5C 24 ? 57 48 83 EC 30 48 8B F9 33 DB 48 89 5C 24 ? E8 ? ? ? ? 48 89 44 24 ? 88 5C 24 ? 48 8B C8 E8 ? ? ? ? 85 C0 0F 85",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.signal_disconnect = ptr.as<functions::signal_disconnect>();
			        g_pointers->m_roblox_pointers.signal_mutex_get = ptr.add(21).rip().as<functions::signal_mutex_get>();
		        }
		    },
		    {"SIGNAL_SLOT_FREE",
		        "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B D9 E8 ? ? ? ? 48 8D 50 10 BF FF FF FF FF 48 3B DA 0F 82",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.signal_slot_free = ptr.as<functions::signal_slot_free>();
		        }
		    },
		    {"TYPE_REGISTRY",
		        "48 8B 15 ? ? ? ? 48 8D 0D ? ? ? ? 48 3B 15 ? ? ? ? 48 89 0D ? ? ? ? 48 89 74 24",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.type_registry = ptr.add(10).rip().as<const std::vector<const RBX::Reflection::Type*>*>();
		        }
		    },
		    {"LUA_GETTOP",
		        memory::referencing("assertion failed!").call(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_gettop = ptr.as<functions::lua_gettop>();
		        }
		    },
		    {"LUA_SETTOP",
		        memory::referencing("randomseed").call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_settop = ptr.as<functions::lua_settop>();
		        }
		    },
		    {"LUA_NEWTHREAD",
		        "40 53 48 83 EC 20 48 8B 51 ? 48 8B D9 48 8B 42",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_newthread = ptr.as<functions::lua_newthread>();
		        }
		    },
		    {"LUA_RESUME",
		        "48 89 74 24 ? 57 48 83 EC 20 49 63 F0 48 8B F9 44 8B C6",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_resume = ptr.as<functions::lua_resume>();
		        }
		    },
		    {"LUA_PCALL",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 40 8D 6A",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pcall = ptr.as<functions::lua_pcall>();
		        }
		    },
		    {"LUA_CALL",
		        memory::referencing("randomseed").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_call = ptr.as<functions::lua_call>();
		        }
		    },
		    {"LUA_PUSHVALUE",
		        memory::referencing("table is already frozen").call(4),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushvalue = ptr.as<functions::lua_pushvalue>();
		        }
		    },
		    {"LUA_TYPE",
		        memory::from("LUAL_CHECKANY").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_type = ptr.as<functions::lua_type>();
		        }
		    },
		    {"LUA_PUSHNIL",
		        memory::referencing("final position out of string").call_from_end(3),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushnil = ptr.as<functions::lua_pushnil>();
		        }
		    },
		    {"LUA_PUSHNUMBER",
		        memory::referencing("attempt to index vector with '%s'").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushnumber = ptr.as<functions::lua_pushnumber>();
		        }
		    },
		    {"LUA_PUSHINTEGER",
		        memory::referencing("table or string expected").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushinteger = ptr.as<functions::lua_pushinteger>();
		        }
		    },
		    {"LUA_PUSHBOOLEAN",
		        memory::referencing("Invalid number of arguments: %d, Vector2.FuzzyEq expects 2 or 3.").call(5),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushboolean = ptr.as<functions::lua_pushboolean>();
		        }
		    },
		    {"LUA_PUSHLSTRING",
		        memory::referencing("charpattern").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushlstring = ptr.as<functions::lua_pushlstring>();
		        }
		    },
		    {"LUA_PUSHSTRING",
		        memory::referencing("Error occurred").call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushstring = ptr.as<functions::lua_pushstring>();
		        }
		    },
		    {"LUA_PUSHCCLOSUREK",
		        memory::referencing("_LOADED").call_from_end(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_pushcclosurek = ptr.as<functions::lua_pushcclosurek>();
		        }
		    },
		    {"LUA_TOLSTRING",
		        memory::referencing("lua_exception: unexpected exception status").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_tolstring = ptr.as<functions::lua_tolstring>();
		        }
		    },
		    {"LUA_TONUMBERX",
		        memory::from("LUAL_CHECKNUMBER").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_tonumberx = ptr.as<functions::lua_tonumberx>();
		        }
		    },
		    {"LUA_TOINTEGERX",
		        memory::from("LUAL_CHECKINTEGER").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_tointegerx = ptr.as<functions::lua_tointegerx>();
		        }
		    },
		    {"LUA_TOBOOLEAN",
		        memory::referencing("assertion failed!").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_toboolean = ptr.as<functions::lua_toboolean>();
		        }
		    },
		    {"LUA_TOPOINTER",
		        memory::referencing("Unable to make room on stack to freeze table").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_topointer = ptr.as<functions::lua_topointer>();
		        }
		    },
		    {"LUA_OBJLEN",
		        memory::referencing("table or string expected").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_objlen = ptr.as<functions::lua_objlen>();
		        }
		    },
		    {"LUA_CREATETABLE",
		        memory::referencing("size out of range").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_createtable = ptr.as<functions::lua_createtable>();
		        }
		    },
		    {"LUA_GETFIELD",
		        memory::referencing("randomseed").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_getfield = ptr.as<functions::lua_getfield>();
		        }
		    },
		    {"LUA_GETTABLE",
		        memory::referencing("invalid replacement value (a %s)").call(5),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_gettable = ptr.as<functions::lua_gettable>();
		        }
		    },
		    {"LUA_SETTABLE",
		        memory::referencing("Debugger is out of Luau stack space in a global update").call_from_end(4),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_settable = ptr.as<functions::lua_settable>();
		        }
		    },
		    {"LUA_RAWGET",
		        memory::referencing("InvalidInstance").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawget = ptr.as<functions::lua_rawget>();
		        }
		    },
		    {"LUA_RAWGETI",
		        memory::referencing("Attempt to migrate WeakObjectRef across VM boundary").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawgeti = ptr.as<functions::lua_rawgeti>();
		        }
		    },
		    {"LUA_RAWSET",
		        memory::referencing("Debugger is out of Luau stack space in a table key update").call_from_end(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawset = ptr.as<functions::lua_rawset>();
		        }
		    },
		    {"LUA_RAWSETI",
		        memory::referencing("wrong number of arguments to 'insert'").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawseti = ptr.as<functions::lua_rawseti>();
		        }
		    },
		    {"LUA_RAWGETFIELD",
		        memory::referencing("Error occurred").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawgetfield = ptr.as<functions::lua_rawgetfield>();
		        }
		    },
		    {"LUA_RAWSETFIELD",
		        "48 89 5C 24 ? 57 48 83 EC 20 4D 8B D0 48 8B F9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawsetfield = ptr.as<functions::lua_rawsetfield>();
		        }
		    },
		    {"LUA_RAWITER",
		        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 F6 41 ? ? 48 8B F9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_rawiter = ptr.as<functions::lua_rawiter>();
		        }
		    },
		    {"LUA_NEXT",
		        memory::referencing("invalid key to 'next'").caller(),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_next = ptr.as<functions::lua_next>();
		        }
		    },
		    {"LUA_REF",
		        "48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC 20 4C 8B 71 ? 48 8B F1",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_ref = ptr.as<functions::lua_ref>();
		        }
		    },
		    {"LUA_UNREF",
		        "40 57 48 83 EC 20 8B FA 85 D2 7E ? 48 89 5C 24 ?",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_unref = ptr.as<functions::lua_unref>();
		        }
		    },
		    {"LUA_GETMETATABLE",
		        "48 89 5C 24 ? 57 48 83 EC 20 F6 41 ? 04 48 8B D9 48 63 FA 74 ? 4C 8D 41 ? 48 8B D1 E8 ? ? ? ? 48 8B 53 ? 48 8B 43 ? 48 8D 4A ? 48 3B 08 76 ? 48 8B C2 48 2B 43 ? 48 C1 F8 04 48 FF C0 48 3D 40 1F 00 00 0F 8F ? ? ? ? 48 8B 43 ? 48 2B C2 48 83 F8 10 7F ? 4C 8D 44 24 ? C7 44 24 ? 01 00 00 00 48 8D 15 ? ? ? ? 48 8B CB E8 ? ? ? ? 85 C0 0F 85 ? ? ? ? 48 8B 4B ? 48 8B 43 ? 48 83 C0 10 48 39 01 73 ? 48 89 01 85 FF 7E ? 48 8B 53 ? 48 8D 05 ? ? ? ? 48 83 C2 F0 48 8B CF 48 C1 E1 04 48 03 D1 48 3B 53 ? 48 0F 42 C2 EB ? 81 FF F0 D8 FF FF 7E ? 48 8B C7 48 C1 E0 04",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_getmetatable = ptr.as<functions::lua_getmetatable>();
		        }
		    },
		    {"LUA_SETMETATABLE",
		        memory::referencing("nil or boolean").call_from_end(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_setmetatable = ptr.as<functions::lua_setmetatable>();
		        }
		    },
		    {"LUA_GETREADONLY",
		        memory::referencing("table is already frozen").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_getreadonly = ptr.as<functions::lua_getreadonly>();
		        }
		    },
		    {"LUA_GETUPVALUE",
		        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 F6 41 ? ? 48 8B D9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_getupvalue = ptr.as<functions::lua_getupvalue>();
		        }
		    },
		    {"LUA_SETUPVALUE",
		        "48 83 EC 28 4D 63 D0",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_setupvalue = ptr.as<functions::lua_setupvalue>();
		        }
		    },
		    {"LUA_GETINFO",
		        memory::referencing("%s:%d: ").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_getinfo = ptr.as<functions::lua_getinfo>();
		        }
		    },
		    {"LUA_ERROR",
		        memory::from("LUA_SETTOP").call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_error = ptr.as<functions::lua_error>();
		        }
		    },
		    {"LUA_BREAK",
		        memory::referencing("attempt to break across metamethod/C-call boundary"),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_break = ptr.as<functions::lua_break>();
		        }
		    },
		    {"LUA_YIELD",
		        memory::referencing("attempt to yield across metamethod/C-call boundary"),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_yield = ptr.as<functions::lua_yield>();
		        }
		    },
		    {"LUA_ISNUMBER",
		        memory::referencing("attempt to multiply a Vector2 with an incompatible value type or nil").call(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_isnumber = ptr.as<functions::lua_isnumber>();
		        }
		    },
		    {"LUA_ISSTRING",
		        memory::referencing("Error occurred").call(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_isstring = ptr.as<functions::lua_isstring>();
		        }
		    },
		    {"LUA_ISCFUNCTION",
		        "48 83 EC 28 85 D2 7E ? 4C 8B 41 ? 48 8D 05 ? ? ? ? 49 83 C0 F0 48 63 D2 48 C1 E2 04 4C 03 C2 4C 3B 41 ? 49 0F 42 C0 EB ? 81 FA F0 D8 FF FF 7E ? 48 63 C2 48 C1 E0 04 48 03 41 ? EB ? E8 ? ? ? ? 83 78 ? ? 75 ? 48 8B 00 80 78 ? ? 74 ? B8 01 00 00 00",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_iscfunction = ptr.as<functions::lua_iscfunction>();
		        }
		    },
		    {"LUA_ISUSERDATA",
		        "48 83 EC 28 85 D2 7E ? 4C 8B 41 ? 48 8D 05 ? ? ? ? 49 83 C0 F0 48 63 D2 48 C1 E2 04 4C 03 C2 4C 3B 41 ? 49 0F 42 C0 EB ? 81 FA F0 D8 FF FF 7E ? 48 63 C2 48 C1 E0 04 48 03 41 ? EB ? E8 ? ? ? ? 8B 48 ? 83 F9 09 74",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_isuserdata = ptr.as<functions::lua_isuserdata>();
		        }
		    },
		    {"LUA_INSERT",
		        "48 89 5C 24 ? 57 48 83 EC 20 F6 41 ? ? 48 8B D9 48 63 FA 74 ? 4C 8D 41 ? 48 8B D1 E8 ? ? ? ? 85 FF 7E ? 48 8B 53 ? 48 8B CF 48 8B 43",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_insert = ptr.as<functions::lua_insert>();
		        }
		    },
		    {"LUA_SETREADONLY",
		        memory::referencing("table is already frozen").call(3),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.lua_setreadonly = ptr.as<functions::lua_setreadonly>();
		        }
		    },
		    {"LUAF_NEWLCLOSURE",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 8B EA 45 33 F6 48 63 D2 49 8B F1 48 83 C2 02",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaF_newLclosure = ptr.as<functions::luaF_newLclosure>();
		        }
		    },
		    {"LUAF_NEWCCLOSURE",
		        "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 63 F2 33 ED 49 8B F8 89 6C 24 ? 44 0F B6 41",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaF_newCclosure = ptr.as<functions::luaF_newCclosure>();
		        }
		    },
		    {"LUAC_BARRIERF",
		        memory::from("LUA_SETMETATABLE").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaC_barrierf = ptr.as<functions::luaC_barrierf>();
		        }
		    },
		    {"LUAC_BARRIERBACK",
		        memory::referencing("cannot resume non-suspended coroutine").call_from_end(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaC_barrierback = ptr.as<functions::luaC_barrierback>();
		        }
		    },
		    {"LUAC_BARRIERTABLE",
		        memory::referencing("'__newindex' chain too long; possible loop").call(4),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaC_barriertable = ptr.as<functions::luaC_barriertable>();
		        }
		    },
		    {"LUAC_ENUMHEAP",
		        "40 55 57 41 57 48 8B EC 48 83 EC 40 48 8B 79 ? 4C 8B F9",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaC_enumheap = ptr.as<functions::luaC_enumheap>();
		        }
		    },
		    {"LUAM_VISITGCO",
		        memory::referencing("\"%d\":{\"size\":%d},\n").call(3),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaM_visitgco = ptr.as<functions::luaM_visitgco>();
		        }
		    },
		    {"LUAH_SETNUM",
		        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 40 41 8D 40 ?",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaH_setnum = ptr.as<functions::luaH_setnum>();
		        }
		    },
		    {"LUAA_PSEUDO2ADDR",
		        memory::from("LUA_TOBOOLEAN").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaA_pseudo2addr = ptr.as<functions::luaA_pseudo2addr>();
		        }
		    },
		    {"LUAO_NILOBJECT",
		        memory::from("LUA_TYPE").data_reference(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaO_nilobject = ptr.as<void*>();
		        }
		    },
		    {"LUAL_REGISTER",
		        memory::referencing("name conflict for module '%s'"),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_register = ptr.as<functions::luaL_register>();
		        }
		    },
		    {"LUAL_ERRORL",
		        memory::referencing("%d-byte integer does not fit into Lua Integer").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_errorL = ptr.as<functions::luaL_errorL>();
		        }
		    },
		    {"LUAL_TYPEERRORL",
		        memory::referencing("missing argument #%d (%s expected)"),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_typeerrorL = ptr.as<functions::luaL_typeerrorL>();
		        }
		    },
		    {"LUAL_ARGERRORL",
		        memory::referencing("invalid argument #%d (%s)"),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_argerrorL = ptr.as<functions::luaL_argerrorL>();
		        }
		    },
		    {"LUAL_WHERE",
		        memory::referencing("%s:%d: "),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_where = ptr.as<functions::luaL_where>();
		        }
		    },
		    {"LUAL_CHECKINTEGER",
		        memory::referencing("size out of range").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_checkinteger = ptr.as<functions::luaL_checkinteger>();
		        }
		    },
		    {"LUAL_CHECKNUMBER",
		        memory::referencing("invalid conversion specifier").call(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_checknumber = ptr.as<functions::luaL_checknumber>();
		        }
		    },
		    {"LUAL_CHECKLSTRING",
		        memory::referencing("attempt to index vector with '%s'").call(1),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_checklstring = ptr.as<functions::luaL_checklstring>();
		        }
		    },
		    {"LUAL_CHECKTYPE",
		        memory::referencing("table is already frozen").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_checktype = ptr.as<functions::luaL_checktype>();
		        }
		    },
		    {"LUAL_CHECKANY",
		        memory::referencing("assertion failed!").call(0),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_checkany = ptr.as<functions::luaL_checkany>();
		        }
		    },
		    {"LUAL_OPTINTEGER",
		        memory::referencing("position out of range").call(2),
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_optinteger = ptr.as<functions::luaL_optinteger>();
		        }
		    },
		    {"LUAL_OPTBOOLEAN",
		        "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 20 41 8B F0 8B DA 48 8B F9 E8 ? ? ? ? 85 C0",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_optboolean = ptr.as<functions::luaL_optboolean>();
		        }
		    },
		    {"LUAL_SANDBOXTHREAD",
		        "40 53 48 83 EC 20 45 33 C0 33 D2 48 8B D9 E8 ? ? ? ? 45 33 C0 33 D2 48 8B CB E8 ? ? ? ? BA EE D8 FF FF",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.luaL_sandboxthread = ptr.as<functions::luaL_sandboxthread>();
		        }
		    },
		    {"RBX_GET_GLOBAL_STATE",
		        "48 83 EC ? 8B 81 ? ? ? ? 90 83 F8 ? 7D ? 48 81 C1 ? ? ? ? E8 ? ? ? ? 48",
		        [](const memory::handle ptr) {
			        g_pointers->m_roblox_pointers.get_global_state = ptr.as<functions::get_global_state>();
		        }
		    },
		    {"RBX_TASK_DEFER", "48 89 5C 24 ? 44 89 4C 24 ? 4C 89 44 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC B0 00 00 00", [](const memory::handle ptr) {
		    		g_pointers->m_roblox_pointers.task_defer = ptr.as<functions::task_defer>();
				}
		    }
		>();

		// clang-format on

		return batch_and_hash;
	}
}
