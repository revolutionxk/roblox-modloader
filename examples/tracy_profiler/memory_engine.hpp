#pragma once

#include <RobloxModLoader/luau/generated/luau_layout.hpp>
#include <RobloxModLoader/roblox/memory/memory.hpp>
#include <RobloxModLoader/roblox/memory/mimalloc.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace tracy_profiler
{
	struct MemoryEngine
	{
		RBX::Memory::GetCategoryCount category_count{};
		RBX::Memory::GetCategoryName category_name{};
		RBX::Memory::GetCategoryTotal category_total{};
		RBX::Memory::TrackExternal track_external_allocate{};
		RBX::Memory::TrackExternal track_external_deallocate{};
		RBX::Memory::TrackExternal track_external_deallocate_deferred{};
		const void* flush_deferred_category{};
		const void* flush_deferred_all{};
		rml::luau::mirror::LuaState* (*lua_newstate)(void* allocator, void* ud){};
		void (*lua_close)(rml::luau::mirror::LuaState* state){};
		std::uint8_t (*memory_category_index)(void* facet, rml::luau::mirror::LuaState* state, const std::string& name, bool sequential){};
		mi_category_visit_live_bytes_fast_fn visit_live_bytes{};
		mi_get_category_fn get_category{};
		mi_resolve_category_fn resolve_category{};
		mi_usable_size_fn usable_size{};
		mi_is_in_heap_region_fn is_in_heap_region{};
		mi_malloc_fn malloc{};
		mi_malloc_aligned_fn malloc_aligned{};
		mi_realloc_fn realloc{};
		mi_realloc_aligned_fn realloc_aligned{};
		mi_free_prepared_fn free_prepared{};
		mi_free_fn free{};

		[[nodiscard]] bool has_categories() const;
		[[nodiscard]] bool has_luau() const;
		[[nodiscard]] bool has_external() const;
		[[nodiscard]] bool has_heap() const;
		[[nodiscard]] std::vector<std::string> missing() const;
	};

	[[nodiscard]] MemoryEngine resolve_memory_engine();
}
