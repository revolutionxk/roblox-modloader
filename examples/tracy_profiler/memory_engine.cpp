#include "memory_engine.hpp"

#include "signatures.hpp"

#include <RobloxModLoader/memory/batch.hpp>
#include <RobloxModLoader/memory/module.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>

namespace tracy_profiler
{
	static MemoryEngine g_memory{};

	static void found_lua_newstate(const rml::memory::handle ptr)
	{
		g_memory.lua_newstate = ptr.as<decltype(g_memory.lua_newstate)>();
	}

	static void found_lua_close(const rml::memory::handle ptr)
	{
		g_memory.lua_close = ptr.as<decltype(g_memory.lua_close)>();
	}

	static void found_memory_category_index(const rml::memory::handle ptr)
	{
		g_memory.memory_category_index = ptr.as<decltype(g_memory.memory_category_index)>();
	}

	static void found_track_external_allocate(const rml::memory::handle ptr)
	{
		g_memory.track_external_allocate = ptr.as<RBX::Memory::TrackExternal>();
	}

	static void found_track_external_deallocate(const rml::memory::handle ptr)
	{
		g_memory.track_external_deallocate = ptr.as<RBX::Memory::TrackExternal>();
	}

	static void found_track_external_deallocate_deferred(const rml::memory::handle ptr)
	{
		g_memory.track_external_deallocate_deferred = ptr.as<RBX::Memory::TrackExternal>();
	}

	static void found_flush_deferred_category(const rml::memory::handle ptr)
	{
		g_memory.flush_deferred_category = ptr.as<const void*>();
	}

	static void found_flush_deferred_all(const rml::memory::handle ptr)
	{
		g_memory.flush_deferred_all = ptr.as<const void*>();
	}

	static void found_category_count(const rml::memory::handle ptr)
	{
		g_memory.category_count = ptr.as<RBX::Memory::GetCategoryCount>();
	}

	static void found_category_name(const rml::memory::handle ptr)
	{
		g_memory.category_name = ptr.as<RBX::Memory::GetCategoryName>();
	}

	static void found_category_total(const rml::memory::handle ptr)
	{
		g_memory.category_total = ptr.as<RBX::Memory::GetCategoryTotal>();
	}

	static constexpr rml::memory::signature lua_newstate_signature{"LUA_NEWSTATE", signatures::lua_newstate, &found_lua_newstate};
	static constexpr rml::memory::signature lua_close_signature{"LUA_CLOSE", signatures::lua_close, &found_lua_close};
	static constexpr rml::memory::signature memory_category_index_signature{"MEMORY_CATEGORIES_GET_INDEX", signatures::memory_category_index, &found_memory_category_index};
	static constexpr rml::memory::signature track_external_allocate_signature{"RBX_MEMORY_TRACK_EXTERNAL_ALLOCATE", signatures::track_external_allocate, &found_track_external_allocate};
	static constexpr rml::memory::signature track_external_deallocate_signature{"RBX_MEMORY_TRACK_EXTERNAL_DEALLOCATE", signatures::track_external_deallocate, &found_track_external_deallocate};
	static constexpr rml::memory::signature track_external_deallocate_deferred_signature{"RBX_MEMORY_TRACK_EXTERNAL_DEALLOCATE_DEFERRED", signatures::track_external_deallocate_deferred, &found_track_external_deallocate_deferred};
	static constexpr rml::memory::signature flush_deferred_category_signature{"RBX_MEMORY_FLUSH_DEFERRED_CATEGORY", signatures::flush_deferred_category, &found_flush_deferred_category};
	static constexpr rml::memory::signature flush_deferred_all_signature{"RBX_MEMORY_FLUSH_DEFERRED_ALL", signatures::flush_deferred_all, &found_flush_deferred_all};
	static constexpr rml::memory::signature category_count_signature{"RBX_MEMORY_GET_CATEGORY_COUNT", signatures::category_count, &found_category_count};
	static constexpr rml::memory::signature category_name_signature{"RBX_MEMORY_GET_CATEGORY_NAME", signatures::category_name, &found_category_name};
	static constexpr rml::memory::signature category_total_signature{"RBX_MEMORY_GET_CATEGORY_TOTAL", signatures::category_total, &found_category_total};

	static constexpr auto memory_batch()
	{
		return rml::memory::make_batch<lua_newstate_signature, lua_close_signature, memory_category_index_signature, track_external_allocate_signature, track_external_deallocate_signature, track_external_deallocate_deferred_signature, flush_deferred_category_signature, flush_deferred_all_signature, category_count_signature, category_name_signature, category_total_signature>();
	}

	template<typename Function>
	static void exported(const rml::memory::module& allocator, const std::string_view name, Function& target)
	{
		target = allocator.get_export(name).as<Function>();
	}

	bool MemoryEngine::has_categories() const
	{
		return category_count && category_name;
	}

	bool MemoryEngine::has_luau() const
	{
		return lua_newstate && lua_close;
	}

	bool MemoryEngine::has_external() const
	{
		return has_categories() && track_external_allocate && track_external_deallocate && track_external_deallocate_deferred;
	}

	bool MemoryEngine::has_heap() const
	{
		return has_categories() && get_category && resolve_category && usable_size && malloc && malloc_aligned && realloc && realloc_aligned && free_prepared && free;
	}

	std::vector<std::string> MemoryEngine::missing() const
	{
		std::vector<std::string> names;
		const std::pair<bool, const char*> checks[] = {
		    {lua_newstate != nullptr, "LUA_NEWSTATE"},
		    {lua_close != nullptr, "LUA_CLOSE"},
		    {memory_category_index != nullptr, "MEMORY_CATEGORIES_GET_INDEX"},
		    {track_external_allocate != nullptr, "RBX_MEMORY_TRACK_EXTERNAL_ALLOCATE"},
		    {track_external_deallocate != nullptr, "RBX_MEMORY_TRACK_EXTERNAL_DEALLOCATE"},
		    {track_external_deallocate_deferred != nullptr, "RBX_MEMORY_TRACK_EXTERNAL_DEALLOCATE_DEFERRED"},
		    {flush_deferred_category != nullptr, "RBX_MEMORY_FLUSH_DEFERRED_CATEGORY"},
		    {flush_deferred_all != nullptr, "RBX_MEMORY_FLUSH_DEFERRED_ALL"},
		    {category_count != nullptr, "RBX_MEMORY_GET_CATEGORY_COUNT"},
		    {category_name != nullptr, "RBX_MEMORY_GET_CATEGORY_NAME"},
		    {category_total != nullptr, "RBX_MEMORY_GET_CATEGORY_TOTAL"},
		    {visit_live_bytes != nullptr, "mi_category_visit_live_bytes_fast"},
		    {get_category != nullptr, "mi_get_category"},
		    {resolve_category != nullptr, "mi_resolve_category"},
		    {usable_size != nullptr, "mi_usable_size"},
		    {malloc != nullptr, "mi_malloc"},
		    {malloc_aligned != nullptr, "mi_malloc_aligned"},
		    {realloc != nullptr, "mi_realloc"},
		    {realloc_aligned != nullptr, "mi_realloc_aligned"},
		    {free_prepared != nullptr, "mi_free_prepared"},
		    {free != nullptr, "mi_free"},
		};
		for (const auto& [present, name] : checks)
		{
			if (!present)
				names.emplace_back(name);
		}
		return names;
	}

	MemoryEngine resolve_memory_engine()
	{
		g_memory = {};
		const rml::memory::module studio{rml::platform::studio_image_name()};
		const auto batch = memory_batch().m_batch;
		rml::memory::batch_runner::run(batch, studio);

		const rml::memory::module allocator{std::string_view{signatures::allocator_image}};
		exported(allocator, "mi_category_visit_live_bytes_fast", g_memory.visit_live_bytes);
		exported(allocator, "mi_get_category", g_memory.get_category);
		exported(allocator, "mi_resolve_category", g_memory.resolve_category);
		exported(allocator, "mi_usable_size", g_memory.usable_size);
		exported(allocator, "mi_malloc", g_memory.malloc);
		exported(allocator, "mi_malloc_aligned", g_memory.malloc_aligned);
		exported(allocator, "mi_realloc", g_memory.realloc);
		exported(allocator, "mi_realloc_aligned", g_memory.realloc_aligned);
		exported(allocator, "mi_free_prepared", g_memory.free_prepared);
		exported(allocator, "mi_free", g_memory.free);
		return g_memory;
	}
}
