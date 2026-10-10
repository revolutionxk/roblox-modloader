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

	[[nodiscard]] inline rml::memory::handle state_reference(const rml::memory::handle match)
	{
		return match.add(3).rip();
	}
}
