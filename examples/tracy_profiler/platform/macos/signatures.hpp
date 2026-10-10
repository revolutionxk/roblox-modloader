#pragma once

#include <RobloxModLoader/memory/handle.hpp>

namespace tracy_profiler::signatures
{
	inline constexpr char enter[] = "FF C3 01 D1 FC 6F 01 A9 FA 67 02 A9 F8 5F 03 A9 F6 57 04 A9 F4 4F 05 A9 FD 7B 06 A9 FD 83 01 91 ? ? ? ? ? ? ? ? 1F 39 40 EA";
	inline constexpr char state[] = "? ? ? ? ? ? ? ? 08 35 9B 52 C8 04 A0 72 E8 6A 68 F8";
	inline constexpr char leave[] = "3F 04 00 B1 ? ? ? ? FC 6F BA A9 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 F3 03 02 AA";
	inline constexpr char flip_cpu[] = "FC 6F BA A9 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 FF C3 08 D1 ? ? ? ? ? ? ? ? 08 01 40 F9 A8 03 1A F8 ? ? ? ? ? ? ? ? E8 B6 49 91";
	inline constexpr char put_label[] = "FC 6F BA A9 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 F5 03 01 AA F4 03 00 AA ? ? ? ? ? ? ? ? 00 01 40 B9";
	inline constexpr char label_literal[] = "FA 67 BB A9 F8 5F 01 A9 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 ? ? ? ? ? ? ? ? 1F 39 40 EA";
	inline constexpr char get_token[] = "F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F3 03 03 AA F4 03 02 AA F5 03 01 AA F6 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? 1F 00 00 71 E3 17 9F 1A E0 03 16 AA";
	inline constexpr char on_thread_create[] = "FF 03 02 D1 FC 6F 02 A9 FA 67 03 A9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 7B 07 A9 FD C3 01 91 F3 03 01 AA F4 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 08 01 40 39";
	inline constexpr char allocator_image[] = "libmimalloc.3.dylib";
	inline constexpr char lua_newstate[] = "F6 57 BD A9 F4 4F 01 A9 FD 7B 02 A9 FD 83 00 91 F4 03 01 AA F5 03 00 AA ? ? ? ? ? ? ? ? E0 03 01 AA";
	inline constexpr char lua_close[] = "F4 4F BE A9 FD 7B 01 A9 FD 43 00 91 08 0C 40 F9 13 75 41 F9";
	inline constexpr char memory_category_index[] = "FF 03 02 D1 FA 67 03 A9 F8 5F 04 A9 F6 57 05 A9 F4 4F 06 A9 FD 7B 07 A9 FD C3 01 91 F6 03 03 AA F4 03 02 AA F3 03 00 AA E0 03 01 AA";
	inline constexpr char track_external_allocate[] = "FF C3 01 D1 F8 5F 03 A9 F6 57 04 A9 F4 4F 05 A9 FD 7B 06 A9 FD 83 01 91 1F 00 10 71";
	inline constexpr char track_external_deallocate[] = "1F 00 10 71 ? ? ? ? ? ? ? ? FF 03 02 D1 F6 57 05 A9 F4 4F 06 A9 FD 7B 07 A9 FD C3 01 91";
	inline constexpr char track_external_deallocate_deferred[] = "1F 00 10 71 ? ? ? ? ? ? ? ? E4 03 03 AA E3 03 02 AA E2 03 01 AA E1 03 00 AA";
	inline constexpr char flush_deferred_category[] = "FF 43 01 D1 F8 5F 01 A9 F6 57 02 A9 F4 4F 03 A9 FD 7B 04 A9 FD 03 01 91 3F 00 10 71";
	inline constexpr char flush_deferred_all[] = "FF 83 01 D1 FA 67 01 A9 F8 5F 02 A9 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91 F4 03 00 AA 13 60 00 91";
	inline constexpr char category_count[] = "? ? ? ? ? ? ? ? 00 05 00 11 C0 03 5F D6 1F 00 10 71";
	inline constexpr char category_name[] = "1F 00 10 71 ? ? ? ? FD 7B BF A9 FD 03 00 91 ? ? ? ? ? ? ? ? ? ? ? ? FD 7B C1 A8";
	inline constexpr char category_total[] = "? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 00 00 80 D2 C0 03 5F D6 1F 00 10 71 ? ? ? ? FD 7B BF A9 FD 03 00 91";

	[[nodiscard]] inline rml::memory::handle state_reference(const rml::memory::handle match)
	{
		return match.adrp();
	}
}
