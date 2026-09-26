#pragma once

#include "RobloxModLoader/memory/all.hpp"
#include "pointers.hpp"

namespace rml
{
	constexpr auto Pointers::get_roblox_batch()
	{
		// clang-format off
		constexpr auto batch_and_hash = memory::make_batch<

			{
				"RBX_GLOBAL_INIT",
				memory::referencing("[FLog::Error] Unable to initialize libsodium."),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.global_init = ptr.as<functions::global_init>();
				},
			},
			{
				"CLASS_DESCRIPTOR_CTOR",
				"FF C3 01 D1 FC 6F 01 A9 FA 67 02 A9 F8 5F 03 A9 F6 57 04 A9 F4 4F 05 A9 FD 7B 06 A9 FD 83 01 91 F8 03 07 AA F6 03 06 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.class_descriptor_ctor = ptr.as<functions::class_descriptor_ctor>();
				},
			},
			{
				"CLASS_DESCRIPTOR_ALL_CLASSES",
				"FF C3 00 D1 FD 7B 02 A9 FD 83 00 91 ? ? ? ? ? ? ? ? E8 07 00 F9 ? ? ? ? ? ? ? ? 08 C1 BF F8 1F 05 00 B1 ? ? ? ? E8 23 00 91 A8 83 1F F8 A8 23 00 D1 E8 0B 00 F9 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E1 43 00 91 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? FD 7B 42 A9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.class_descriptor_all_classes = ptr.as<functions::class_descriptor_all_classes>();
				},
			},
			{
				"TYPE_REGISTRY",
				"FF 83 00 D1 FD 7B 01 A9 FD 43 00 91 ? ? ? ? ? ? ? ? 08 C1 BF 38 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E0 07 00 F9 E1 23 00 91 E0 03 08 AA ? ? ? ? FD 7B 41 A9 FF 83 00 91 C0 03 5F D6",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.type_registry = ptr.add(0x1C).adrp().as<const std::vector<const RBX::Reflection::Type*>*>();
				},
			},
			{
				"CREATABLE_GET_CREATOR",
				"FF C3 00 D1 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F3 03 00 AA ? ? ? ? F3 07 00 F9 ? ? ? ? ? ? ? ? E1 23 00 91",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.creatable_get_creator = ptr.as<functions::creatable_get_creator>();
				},
			},
			{
				"INSTANCE_CTOR",
				"FF 83 01 D1 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 F6 03 02 AA F3 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? 38 00 80 52",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.instance_ctor = ptr.as<functions::instance_ctor>();
				},
			},
			{
				"CREATABLE_CREATE_INSTANCE_IMPL",
				"FF 83 02 D1 FC 6F 04 A9 FA 67 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD 7B 09 A9 FD 43 02 91 F7 03 05 AA F6 03 04 AA FB 03 02 AA FA 03 01 AA F9 03 00 AA F5 03 08 AA E0 03 03 AA ? ? ? ? F3 03 00 AA ? ? ? ? F4 03 00 AA ? ? ? ? 80 0E 40 F9 ? ? ? ? F8 03 00 AA 80 0E 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.create_instance_impl = ptr.as<functions::create_instance_impl>();
				},
			},
			{
				"VISUAL_ENGINE_BEGIN_RENDER",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 14 01 09 0A",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.visual_engine_begin_render = ptr.as<functions::visual_engine_begin_render>();
				},
			},
			{
				"SCENE_MANAGER_RENDER_SCENE",
				"FF 43 06 D1 E9 23 12 6D FC 6F 13 A9 FA 67 14 A9 F8 5F 15 A9 F6 57 16 A9 F4 4F 17 A9 FD 7B 18 A9 FD 03 06 91 F4 03 06 AA F5 03 05 AA F6 03 04 AA F7 03 03 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.scene_manager_render_scene = ptr.as<functions::scene_manager_render_scene>();
				},
			},
			{
				"PROPERTY_DESCRIPTOR_CTOR",
				"F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F4 03 07 AA F3 03 05 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.property_descriptor_ctor = ptr.as<functions::property_descriptor_ctor>();
				},
			},
			{
				"FUNCTION_DESCRIPTOR_CTOR",
				"FF 83 00 D1 FD 7B 01 A9 FD 43 00 91 E8 03 03 AA E4 17 00 A9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.function_descriptor_ctor = ptr.as<functions::function_descriptor_ctor>();
				},
			},
			{
				"EVENT_DESCRIPTOR_CTOR",
				"FD 7B BF A9 FD 03 00 91 E5 03 03 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 08 00 00 F9 00 E4 00 6F 00 80 84 3C 00 80 85 3C 00 80 86 3C FD 7B C1 A8 C0 03 5F D6 1F 09 00 B9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.event_descriptor_ctor = ptr.as<functions::event_descriptor_ctor>();
				},
			},
			{
				"MENU_BUILD_FROM_DOM",
				"FF C3 02 D1 FC 6F 05 A9 FA 67 06 A9 F8 5F 07 A9 F6 57 08 A9 F4 4F 09 A9 FD 7B 0A A9 FD 83 02 91 F4 03 01 AA F3 03 00 AA E2 17 00 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.build_menu_bar_from_dom = ptr.as<functions::build_menu_bar_from_dom>();
				},
			},
			{
				"GET_STRING_ATOM",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E1 03 13 AA FD 7B 41 A9 F4 4F C2 A8 ? ? ? ? FF C3 00 D1 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F3 03 00 AA ? ? ? ? F4 03 01 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.get_string_atom = ptr.as<functions::get_string_atom>();
				},
			},
			{
				"DESCRIPTOR_LOOKUP",
				"FF 43 01 D1 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 F4 03 01 AA F3 03 00 AA 21 10 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.descriptor_lookup = ptr.add(0x78).bl().as<functions::descriptor_lookup>();
				},
			},
			{
				"MEMBER_TABLE_OFFSET",
				"FF 43 01 D1 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 F4 03 01 AA F3 03 00 AA 21 10 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.member_table_offset = (*ptr.add(0x70).as<const std::uint32_t*>() >> 10) & 0xFFF;
				},
			},
			{
				"OBJECT_CREATE_BY_NAME",
				"FF 03 01 D1 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F4 03 02 AA F5 03 00 AA F3 03 08 AA E0 03 01 AA ? ? ? ? ? ? ? ? F6 03 00 AA 9F 06 00 71 ? ? ? ? ? ? ? ? 9F 06 00 71 ? ? ? ? C8 02 40 F9 08 05 40 F9 E0 03 16 AA 00 01 3F D6 ? ? ? ? ? ? ? ? 9F 0E 00 71 ? ? ? ? 9F 0A 00 71 ? ? ? ? C8 02 40 F9 08 09 40 F9 E0 03 16 AA 00 01 3F D6 ? ? ? ? C8 02 40 F9 09 01 40 F9 E8 03 00 91 E0 03 16 AA E1 03 15 AA E2 03 14 AA 20 01 3F D6 E0 03 00 91",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.object_create_by_name = ptr.as<functions::object_create_by_name>();
				},
			},
			{
				"FREEBLOCK",
				"E8 03 01 AA 09 0C 40 F9 41 8C 5F F8 2A 14 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.freeblock = ptr.as<functions::freeblock>();
				},
			},
			{
				"LUAA_PSEUDO2ADDR",
				memory::from("LUA_TOBOOLEAN").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaA_pseudo2addr = ptr.as<functions::luaA_pseudo2addr>();
				},
			},
			{
				"LUAC_BARRIERBACK",
				memory::from("LUA_INSERT").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaC_barrierback = ptr.as<functions::luaC_barrierback>();
				},
			},
			{
				"LUAC_BARRIERF",
				memory::from("LUA_SETMETATABLE").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaC_barrierf = ptr.as<functions::luaC_barrierf>();
				},
			},
			{
				"LUAC_BARRIERTABLE",
				memory::from("LUA_RAWSET").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaC_barriertable = ptr.as<functions::luaC_barriertable>();
				},
			},
			{
				"LUAC_ENUMHEAP",
				memory::referencing("[type name]"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaC_enumheap = ptr.as<functions::luaC_enumheap>();
				},
			},
			{
				"LUAD_RAWRUNPROTECTED",
				"F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 E8 03 01 AA F5 03 00 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaD_rawrunprotected = ptr.as<functions::luaD_rawrunprotected>();
				},
			},
			{
				"LUAD_THROW",
				memory::from("LUA_ERROR").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaD_throw = ptr.as<functions::luaD_throw>();
				},
			},
			{
				"LUAE_NEWTHREAD",
				memory::from("LUA_NEWTHREAD").call_from_end(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaE_newthread = ptr.as<functions::luaE_newthread>();
				},
			},
			{
				"LUAF_NEWCCLOSURE",
				memory::from("LUA_PUSHCCLOSUREK").call(3),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaF_newCclosure = ptr.as<functions::luaF_newCclosure>();
				},
			},
			{
				"LUAF_NEWLCLOSURE",
				"F8 5F BC A9 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F4 03 03 AA F5 03 02 AA F3 03 01 AA F6 03 00 AA 68 7E 7C 93",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaF_newLclosure = ptr.as<functions::luaF_newLclosure>();
				},
			},
			{
				"LUAH_NEW",
				memory::from("LUA_CREATETABLE").call_from_end(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaH_new = ptr.as<functions::luaH_new>();
				},
			},
			{
				"LUAH_SETNUM",
				memory::from("LUA_RAWSETI").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaH_setnum = ptr.as<functions::luaH_setnum>();
				},
			},
			{
				"LUAL_ARGERRORL",
				memory::referencing("invalid argument #%d (%s)"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_argerrorL = ptr.as<functions::luaL_argerrorL>();
				},
			},
			{
				"LUAL_CHECKANY",
				memory::referencing("assertion failed!").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_checkany = ptr.as<functions::luaL_checkany>();
				},
			},
			{
				"LUAL_CHECKLSTRING",
				memory::referencing("attempt to index vector with '%s'").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_checklstring = ptr.as<functions::luaL_checklstring>();
				},
			},
			{
				"LUAL_CHECKTYPE",
				memory::referencing("table is already frozen").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_checktype = ptr.as<functions::luaL_checktype>();
				},
			},
			{
				"LUAL_ERRORL",
				memory::referencing("%d-byte integer does not fit into Lua Integer").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_errorL = ptr.as<functions::luaL_errorL>();
				},
			},
			{
				"LUAL_REGISTER",
				memory::referencing("name conflict for module '%s'"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_register = ptr.as<functions::luaL_register>();
				},
			},
			{
				"LUAL_SANDBOXTHREAD",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 00 AA 01 00 80 52 02 00 80 52 ? ? ? ? E0 03 13 AA 01 00 80 52 02 00 80 52 ? ? ? ? E0 03 13 AA 21 E2 84 12",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_sandboxthread = ptr.as<functions::luaL_sandboxthread>();
				},
			},
			{
				"LUAL_TYPEERRORL",
				memory::referencing("missing argument #%d (%s expected)"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_typeerrorL = ptr.as<functions::luaL_typeerrorL>();
				},
			},
			{
				"LUAL_WHERE",
				memory::referencing("%s:%d: "),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_where = ptr.as<functions::luaL_where>();
				},
			},
			{
				"LUAM_VISITGCO",
				memory::referencing("},\"roots\":{\n").call_from_end(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaM_visitgco = ptr.as<functions::luaM_visitgco>();
				},
			},
			{
				"LUA_BREAK",
				memory::referencing("attempt to break across metamethod/C-call boundary"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_break = ptr.as<functions::lua_break>();
				},
			},
			{
				"LUA_CALL",
				memory::referencing("randomseed").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_call = ptr.as<functions::lua_call>();
				},
			},
			{
				"LUA_CREATETABLE",
				memory::referencing("size out of range").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_createtable = ptr.as<functions::lua_createtable>();
				},
			},
			{
				"LUA_GETFIELD",
				memory::referencing("randomseed").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_getfield = ptr.as<functions::lua_getfield>();
				},
			},
			{
				"LUA_GETINFO",
				memory::referencing("%s:%d: ").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_getinfo = ptr.as<functions::lua_getinfo>();
				},
			},
			{
				"LUA_GETMETATABLE",
				memory::referencing("table is already frozen").call(2).call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_getmetatable = ptr.as<functions::lua_getmetatable>();
				},
			},
			{
				"LUA_GETREADONLY",
				memory::referencing("table is already frozen").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_getreadonly = ptr.as<functions::lua_getreadonly>();
				},
			},
			{
				"LUA_GETTABLE",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F4 03 01 AA F3 03 00 AA 08 04 40 39 ? ? ? ? 62 42 01 91 E0 03 13 AA E1 03 13 AA ? ? ? ? 9F 06 00 71 ? ? ? ? 68 0A 40 F9 08 51 34 8B 09 41 00 D1 68 16 40 F9 ? ? ? ? ? ? ? ? 3F 01 08 EB 21 31 8A 9A",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_gettable = ptr.as<functions::lua_gettable>();
				},
			},
			{
				"LUA_GETTOP",
				memory::referencing("assertion failed!").call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_gettop = ptr.as<functions::lua_gettop>();
				},
			},
			{
				"LUA_GETUPVALUE",
				"FF 03 01 D1 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F4 03 02 AA F5 03 01 AA F3 03 00 AA 08 04 40 39 ? ? ? ? 62 42 01 91 E0 03 13 AA E1 03 13 AA ? ? ? ? 68 16 40 F9 08 41 00 91 69 06 40 F9 29 01 40 F9 1F 01 09 EB ? ? ? ? E0 03 13 AA 21 00 80 52 ? ? ? ? ? ? ? ? BF 06 00 71 ? ? ? ? 68 0A 40 F9 08 51 35 8B 08 41 00 D1 69 16 40 F9 ? ? ? ? ? ? ? ? 1F 01 09 EB 00 31 8A 9A",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_getupvalue = ptr.as<functions::lua_getupvalue>();
				},
			},
			{
				"LUA_INSERT",
				memory::referencing("buffer too large").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_insert = ptr.as<functions::lua_insert>();
				},
			},
			{
				"LUA_ISNUMBER",
				memory::referencing("attempt to multiply a Vector2 with an incompatible value type or nil").call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_isnumber = ptr.as<functions::lua_isnumber>();
				},
			},
			{
				"LUA_ISSTRING",
				memory::referencing("Error occurred").call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_isstring = ptr.as<functions::lua_isstring>();
				},
			},
			{
				"LUA_ISUSERDATA",
				"FD 7B BF A9 FD 03 00 91 3F 04 00 71 ? ? ? ? 08 08 40 F9 08 51 21 8B 08 41 00 D1 09 14 40 F9 ? ? ? ? ? ? ? ? 1F 01 09 EB 00 31 8A 9A ? ? ? ? C8 E1 84 12 3F 00 08 6B ? ? ? ? 08 14 40 F9 00 D1 21 8B ? ? ? ? ? ? ? ? 08 0C 40 B9 1F 25 00 71 04 19 42 7A",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_isuserdata = ptr.as<functions::lua_isuserdata>();
				},
			},
			{
				"LUA_NEWTHREAD",
				memory::referencing("Not enough resources to create a thread for callback execution").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_newthread = ptr.as<functions::lua_newthread>();
				},
			},
			{
				"LUA_NEXT",
				memory::referencing("invalid key to 'next'").caller(),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_next = ptr.as<functions::lua_next>();
				},
			},
			{
				"LUA_OBJLEN",
				memory::referencing("table or string expected").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_objlen = ptr.as<functions::lua_objlen>();
				},
			},
			{
				"LUA_PCALL",
				"FF 03 01 D1 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F6 03 03 AA F4 03 02 AA F5 03 01 AA F3 03 00 AA 28 04 00 11",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pcall = ptr.as<functions::lua_pcall>();
				},
			},
			{
				"LUA_PUSHBOOLEAN",
				memory::referencing("Invalid number of arguments: %d, Vector2.FuzzyEq expects 2 or 3.").call(5),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushboolean = ptr.as<functions::lua_pushboolean>();
				},
			},
			{
				"LUA_PUSHCCLOSUREK",
				memory::referencing("_LOADED").call_from_end(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushcclosurek = ptr.as<functions::lua_pushcclosurek>();
				},
			},
			{
				"LUA_PUSHINTEGER",
				memory::referencing("table or string expected").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushinteger = ptr.as<functions::lua_pushinteger>();
				},
			},
			{
				"LUA_PUSHLSTRING",
				memory::referencing("charpattern").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushlstring = ptr.as<functions::lua_pushlstring>();
				},
			},
			{
				"LUA_PUSHNIL",
				memory::referencing("final position out of string").call_from_end(3),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushnil = ptr.as<functions::lua_pushnil>();
				},
			},
			{
				"LUA_PUSHNUMBER",
				memory::referencing("attempt to index vector with '%s'").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushnumber = ptr.as<functions::lua_pushnumber>();
				},
			},
			{
				"LUA_PUSHSTRING",
				memory::referencing("Error occurred").call_from_end(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushstring = ptr.as<functions::lua_pushstring>();
				},
			},
			{
				"LUA_PUSHVALUE",
				memory::referencing("table is already frozen").call(4),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_pushvalue = ptr.as<functions::lua_pushvalue>();
				},
			},
			{
				"LUA_RAWGET",
				memory::referencing("table is already frozen").call(2).call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawget = ptr.as<functions::lua_rawget>();
				},
			},
			{
				"LUA_RAWGETFIELD",
				memory::referencing("Error occurred").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawgetfield = ptr.as<functions::lua_rawgetfield>();
				},
			},
			{
				"LUA_RAWGETI",
				memory::referencing("Attempt to migrate WeakObjectRef across VM boundary").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawgeti = ptr.as<functions::lua_rawgeti>();
				},
			},
			{
				"LUA_RAWITER",
				"F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F4 03 02 AA F5 03 01 AA F3 03 00 AA 08 04 40 39 ? ? ? ? 62 42 01 91 E0 03 13 AA E1 03 13 AA ? ? ? ? 68 16 40 F9 08 81 00 91",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawiter = ptr.as<functions::lua_rawiter>();
				},
			},
			{
				"LUA_RAWSET",
				memory::referencing("Debugger is out of Luau stack space in a table key update").call_from_end(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawset = ptr.as<functions::lua_rawset>();
				},
			},
			{
				"LUA_RAWSETFIELD",
				"F8 5F BC A9 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F4 03 02 AA F3 03 00 AA 3F 04 00 71",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawsetfield = ptr.as<functions::lua_rawsetfield>();
				},
			},
			{
				"LUA_RAWSETI",
				memory::referencing("wrong number of arguments to 'insert'").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_rawseti = ptr.as<functions::lua_rawseti>();
				},
			},
			{
				"LUA_REF",
				"F8 5F BC A9 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F3 03 00 AA 17 0C 40 F9 3F 04 00 71",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_ref = ptr.as<functions::lua_ref>();
				},
			},
			{
				"LUA_RESUME",
				"F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F4 03 02 AA F3 03 00 AA ? ? ? ? F5 03 00 AA ? ? ? ? 68 0E 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_resume = ptr.as<functions::lua_resume>();
				},
			},
			{
				"LUA_SETFIELD",
				memory::referencing("charpattern").call_from_end(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_setfield = ptr.as<functions::lua_setfield>();
				},
			},
			{
				"LUA_SETMETATABLE",
				memory::referencing("nil or boolean").call_from_end(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_setmetatable = ptr.as<functions::lua_setmetatable>();
				},
			},
			{
				"LUA_SETREADONLY",
				memory::referencing("table is already frozen").call(3),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_setreadonly = ptr.as<functions::lua_setreadonly>();
				},
			},
			{
				"LUA_SETSAFEENV",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 02 AA 3F 04 00 71 ? ? ? ? 08 08 40 F9 08 51 21 8B 08 41 00 D1 09 14 40 F9 ? ? ? ? ? ? ? ? 1F 01 09 EB 00 31 8A 9A ? ? ? ? C8 E1 84 12 3F 00 08 6B ? ? ? ? 08 14 40 F9 00 D1 21 8B ? ? ? ? ? ? ? ? 08 00 40 F9 7F 02 00 71",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_setsafeenv = ptr.as<functions::lua_setsafeenv>();
				},
			},
			{
				"LUA_SETTABLE",
				memory::referencing("Debugger is out of Luau stack space in a global update").call_from_end(4),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_settable = ptr.as<functions::lua_settable>();
				},
			},
			{
				"LUA_SETTOP",
				memory::referencing("randomseed").call_from_end(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_settop = ptr.as<functions::lua_settop>();
				},
			},
			{
				"LUA_SETUPVALUE",
				"FF 03 01 D1 F6 57 01 A9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F4 03 02 AA F3 03 00 AA 3F 04 00 71 ? ? ? ? 68 0A 40 F9 08 51 21 8B 08 41 00 D1 69 16 40 F9 ? ? ? ? ? ? ? ? 1F 01 09 EB 15 31 8A 9A ? ? ? ? C8 E1 84 12 3F 00 08 6B ? ? ? ? 68 16 40 F9 15 D1 21 8B ? ? ? ? E0 03 13 AA ? ? ? ? F5 03 00 AA E2 23 00 91",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_setupvalue = ptr.as<functions::lua_setupvalue>();
				},
			},
			{
				"LUA_TOBOOLEAN",
				memory::referencing("assertion failed!").call(1),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_toboolean = ptr.as<functions::lua_toboolean>();
				},
			},
			{
				"LUA_TOINTEGERX",
				memory::from("LUAL_CHECKINTEGER").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_tointegerx = ptr.as<functions::lua_tointegerx>();
				},
			},
			{
				"LUA_TOLSTRING",
				memory::referencing("lua_exception: unexpected exception status").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_tolstring = ptr.as<functions::lua_tolstring>();
				},
			},
			{
				"LUA_TONUMBERX",
				memory::from("LUAL_CHECKNUMBER").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_tonumberx = ptr.as<functions::lua_tonumberx>();
				},
			},
			{
				"LUA_TOPOINTER",
				memory::referencing("%s: 0x%016llx").call(9),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_topointer = ptr.as<functions::lua_topointer>();
				},
			},
			{
				"LUA_TYPE",
				memory::from("LUAL_CHECKANY").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_type = ptr.as<functions::lua_type>();
				},
			},
			{
				"LUA_UNREF",
				"3F 04 00 71 ? ? ? ? F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 01 AA 14 0C 40 F9 80 12 43 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_unref = ptr.as<functions::lua_unref>();
				},
			},
			{
				"LUA_YIELD",
				memory::referencing("attempt to yield across metamethod/C-call boundary"),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_yield = ptr.as<functions::lua_yield>();
				},
			},
			{
				"LUAU_LOAD",
				"FF 83 02 D1 FA 67 05 A9 F8 5F 06 A9 F6 57 07 A9 F4 4F 08 A9 FD 7B 09 A9 FD 43 02 91 F4 03 04 AA F5 03 03 AA F6 03 02 AA F7 03 01 AA F3 03 00 AA 18 0C 40 F9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luau_load = ptr.as<functions::luau_load>();
				},
			},
			{
				"SIGNAL_DISCONNECT",
				"FF 03 01 D1 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F3 03 00 AA FF 0B 00 F9 ? ? ? ? F4 03 00 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.signal_disconnect = ptr.as<functions::signal_disconnect>();
				},
			},
			{
				"SLOTS_HOLDER_RELEASE",
				"FF 03 01 D1 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F3 03 00 AA FF 0B 00 F9 ? ? ? ? F4 03 00 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.slots_holder_release = ptr.add(0x6C).bl().add(0x18).bl().as<functions::slots_holder_release>();
				},
			},
			{
				"NAME_DECLARE",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? E1 03 13 AA FD 7B 41 A9 F4 4F C2 A8 ? ? ? ? FF 03 03 D1",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.name_declare = ptr.as<functions::name_declare>();
				},
			},
			{
				"SIGNAL_MUTEX_GET",
				"FF 03 01 D1 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F3 03 00 AA FF 0B 00 F9 ? ? ? ? F4 03 00 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.signal_mutex_get = ptr.add(0x18).bl().as<functions::signal_mutex_get>();
				},
			},
			{
				"LUAL_CHECKINTEGER",
				memory::referencing("size out of range").call(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_checkinteger = ptr.as<functions::luaL_checkinteger>();
				},
			},
			{
				"LUAL_CHECKNUMBER",
				memory::referencing("invalid conversion specifier").call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_checknumber = ptr.as<functions::luaL_checknumber>();
				},
			},
			{
				"LUAL_OPTBOOLEAN",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 01 AA F4 03 00 AA ? ? ? ? 1F 04 00 71 ? ? ? ? E0 03 14 AA E1 03 13 AA FD 7B 41 A9",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_optboolean = ptr.add(0x44).as<functions::luaL_optboolean>();
				},
			},
			{
				"LUAL_OPTINTEGER",
				memory::referencing("position out of range").call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaL_optinteger = ptr.as<functions::luaL_optinteger>();
				},
			},
			{
				"LUAO_NILOBJECT",
				memory::from("LUA_TYPE").data_reference(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luaO_nilobject = ptr.as<void*>();
				},
			},
			{
				"LUA_ERROR",
				memory::from("LUA_SETTOP").call_from_end(0),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_error = ptr.as<functions::lua_error>();
				},
			},
			{
				"LUAU_EXECUTE",
				memory::referencing("cannot resume dead coroutine").call(2).call(2),
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.luau_execute = ptr.as<functions::luau_execute>();
				},
			},
			{
				"RBX_GET_GLOBAL_STATE",
				"F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 F3 03 01 AA ? ? ? ? F4 03 00 AA E0 03 13 AA ? ? ? ? E1 03 00 AA",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.get_global_state = ptr.as<functions::get_global_state>();
				},
			},
			{
				"LUA_ISCFUNCTION",
				"FD 7B BF A9 FD 03 00 91 3F 04 00 71 ? ? ? ? 08 08 40 F9 08 51 21 8B 08 41 00 D1 09 14 40 F9 ? ? ? ? ? ? ? ? 1F 01 09 EB 00 31 8A 9A ? ? ? ? C8 E1 84 12 3F 00 08 6B ? ? ? ? 08 14 40 F9 00 D1 21 8B ? ? ? ? ? ? ? ? 08 0C 40 B9 1F 21 00 71 ? ? ? ? 08 00 40 F9 08 0D 40 39 1F 01 00 71 E0 07 9F 1A",
				[](const memory::handle ptr) {
					g_pointers->m_roblox_pointers.lua_iscfunction = ptr.as<functions::lua_iscfunction>();
				},
			}
		>();
		// clang-format on

		return batch_and_hash;
	}
}
