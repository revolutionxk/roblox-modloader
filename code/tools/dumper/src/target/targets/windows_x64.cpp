#include "target/targets/profiles.hpp"

namespace rml::dumper::target
{
	static constexpr std::array<AnchorSpec, anchor_count> windows_x64_anchors{{
	    {.id = Anchor::luaD_reallocstack, .text = "cannot resume non-suspended coroutine", .path = {{{Step::call_from_end, 1}}}},
	    {.id = Anchor::luaD_reallocCI, .origin = Anchor::luau_precall, .path = {{{Step::call, 1}, {Step::call, 0}}}},
	    {Anchor::luaE_newthread,
	     "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC ? 44 0F B6 41 ? 33 F6 48 8B F9 89 74 24 ? 44 8D 4E ? 41 8D 51 ? E8"},
	    {.id = Anchor::lua_settop, .text = "randomseed", .path = {{{Step::call_from_end, 0}}}},
	    {Anchor::lua_resume, "40 53 48 83 EC 20 41 B8 01 00 00 00 48 8B D9 E8 ? ? ? ? 85 C0"},
	    {Anchor::luaM_free,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 48 8B 59 ? 49 8B F8 45 0F B6 F1 48 8B F2 48 8B E9"},
	    {Anchor::luaF_findupval,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B 41 ? 48 8D 59 ? 48 8B 69 ? 48 8B FA 48 8B F1 48 85 C0"},
	    {Anchor::luaH_new,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 45 33 F6 41 8B F0 44 0F B6 41 ? 8B EA 48 8B F9"},
	    {Anchor::luaF_newLclosure,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 8B EA 45 33 F6 48 63 D2 49 8B F1 48 83 C2 02"},
	    {Anchor::luaF_newCclosure,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 63 F2 33 ED 49 8B F8 89 6C 24 ? 44 0F B6 41"},
	    {Anchor::luaF_freeproto, "48 89 5C 24 ? 48 89 74 24 ? 57 48 83 EC 30 44 0F B6 4A ? 49 8B F0"},
	    {Anchor::luaU_load,
	     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC 80 00 00 00 49 8B E9"},
	    {.id = Anchor::lua_pushnumber, .text = "attempt to index vector with '%s'", .path = {{{Step::call_from_end, 1}}}},
	    {.id = Anchor::lua_toboolean, .text = "assertion failed!", .path = {{{Step::call, 1}}}},
	    {.id = Anchor::luau_precall, .text = "cannot resume dead coroutine", .path = {{{Step::call, 0}}}},
	    {.id = Anchor::lua_getinfo, .text = "%s:%d: ", .path = {{{Step::call, 0}}}},
	    {.id = Anchor::lua_pushcclosurek, .text = "_LOADED", .path = {{{Step::call_from_end, 2}}}},
		{Anchor::luaC_enumheap, "", "weakregistry"},
		{Anchor::rbx_derive_thread_capabilities,
		"48 89 5C 24 ? 57 48 83 EC 20 48 8B DA 48 8B F9 E8 ? ? ? ? 4C 8B C8 4C 8B 40 ? 48 85 DB 74 ? 48 8B 43"},
	}};

	const TargetProfile& windows_x64_profile()
	{
		static const TargetProfile profile{
		    .name = "windows-x64",
		    .format = ImageFormat::pe,
		    .architecture = Architecture::x86_64,
		    .abi = &disasm::Abi::windows_x64(),
		    .anchors = windows_x64_anchors,
		};

		return profile;
	}
}
