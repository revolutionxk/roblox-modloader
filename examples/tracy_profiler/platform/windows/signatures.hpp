#pragma once

#include <RobloxModLoader/memory/handle.hpp>

namespace tracy_profiler::signatures
{
	inline constexpr char enter[] = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 05 ? ? ? ? 48 8B F1 48 C1";
	inline constexpr char state[] = "48 8D 0D ? ? ? ? 45 8B 94 0B D0 36 00 00 4C";
	inline constexpr char leave[] = "48 83 FA FF 0F 84 ? ? ? ? 48 89 5C 24 10 56";
	inline constexpr char flip_cpu[] = "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55 41 54 41 55 41 56 41 57 48 8D A8 18 FD FF FF 48";
	inline constexpr char put_label[] = "48 89 5C 24 08 55 56 57 48 83 EC 20 48 8B EA 48 8B F9 8B 0D";
	inline constexpr char label_literal[] = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 48 8B 05 ? ? ? ? 48 8B F9 48 C1";
	inline constexpr char get_token[] = "48 89 5C 24 08 57 48 83 EC 30 41 0F B6 D9 48 8D 3D ? ? ? ? 45 33 C9 41 8B C1";
	inline constexpr char on_thread_create[] = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 40 44 8B FA 48 8B F1 E8 ? ? ? ? E8";
	inline constexpr char allocator_image[] = "mimalloc.dll";
	inline constexpr char lua_newstate[] = "48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 55 41 56 41 57 48 8D AC 24 C0 FE FF FF";
	inline constexpr char lua_close[] = "40 53 48 83 EC 20 48 8B 41 18 48 8B 98 E8 02 00";
	inline constexpr char memory_category_index[] = "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 70 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 60 45";
	inline constexpr char track_external_allocate[] = "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 60 49 8B F9 49";
	inline constexpr char track_external_deallocate[] = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 49 8B F1 49 8B E8 0F B6";
	inline constexpr char track_external_deallocate_deferred[] = "48 89 5C 24 10 48 89 6C 24 18 48 89 74 24 20 57 48 83 EC 50 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 48 8B D9 81 F9 00";
	inline constexpr char flush_deferred_category[] = "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 60 48 8B 1D ? ? ? ? 48";
	inline constexpr char flush_deferred_all[] = "48 89 5C 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 60 48 8B 1D ? ? ? ? 48";
	inline constexpr char category_count[] = "8B 05 ? ? ? ? FF C0 C3 CC CC CC CC CC CC CC";
	inline constexpr char category_name[] = "48 83 EC 28 81 F9 00 04 00 00 72 ? E8 ? ? ? ? 48";
	inline constexpr char category_total[] = "48 83 EC 28 80 3D ? ? ? ? 00 74 ? 84 D2 74";

	[[nodiscard]] inline rml::memory::handle state_reference(const rml::memory::handle match)
	{
		return match.add(3).rip();
	}
}
