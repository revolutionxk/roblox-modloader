#include "heap_memory.hpp"

#include "callstack.hpp"
#include "detours.hpp"
#include "settings.hpp"

#include <RobloxModLoader/memory/module.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	static InFlight g_in_flight;
	static std::atomic<HeapMemory*> g_heap{nullptr};
	static MemoryEngine g_original{};

	static void* heap_malloc(const std::size_t size)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		auto* block = g_original.malloc(size);
		if (self && block)
			self->record_allocation(block, size);
		return block;
	}

	static void* heap_malloc_aligned(const std::size_t size, const std::size_t alignment)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		auto* block = g_original.malloc_aligned(size, alignment);
		if (self && block)
			self->record_allocation(block, size);
		return block;
	}

	static void* heap_realloc(void* block, const std::size_t size)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		if (self && block)
			self->record_free(block);
		auto* moved = g_original.realloc(block, size);
		if (self && moved)
			self->record_allocation(moved, size);
		return moved;
	}

	static void* heap_realloc_aligned(void* block, const std::size_t size, const std::size_t alignment)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		if (self && block)
			self->record_free(block);
		auto* moved = g_original.realloc_aligned(block, size, alignment);
		if (self && moved)
			self->record_allocation(moved, size);
		return moved;
	}

	static void heap_free_prepared(void* block, void* state)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		if (self && block)
			self->record_free(block);
		g_original.free_prepared(block, state);
	}

	static void heap_free(void* block)
	{
		const InFlight::Call call{g_in_flight};
		auto* self = g_heap.load(std::memory_order_acquire);
		if (self && block)
			self->record_free(block);
		g_original.free(block);
	}

	HeapMemory::HeapMemory(const MemoryEngine& engine, const Callstacks& callstacks, std::shared_ptr<spdlog::logger> log) :
	    m_engine(engine),
	    m_callstacks(callstacks),
	    m_log(std::move(log)),
	    m_pools(RBX::Memory::max_categories),
	    m_luau(RBX::Memory::max_categories)
	{
		g_original = engine;
		const rml::memory::module image{rml::platform::module_path_containing(reinterpret_cast<const void*>(&heap_malloc))};
		m_image_begin = image.begin().as<std::uintptr_t>();
		m_image_size = image.size();
		if (m_image_size == 0)
			m_log->warn("heap memory events: own image range unresolved; profiler allocations will not be filtered");
		const auto count = std::min(m_engine.category_count(), RBX::Memory::max_categories);
		const auto& invalid = m_names.emplace_back("Heap/invalid");
		std::ranges::fill(m_pools, invalid.c_str());
		for (std::uint32_t index = 0; index < count; ++index)
		{
			const auto* raw = m_engine.category_name(index);
			const std::string_view name = raw && *raw ? raw : "default";
			const auto prefix = "Heap/" + std::string(name.substr(0, name.find('/')));
			const auto existing = std::ranges::find(m_names, prefix);
			m_pools[index] = existing != m_names.end() ? existing->c_str() : m_names.emplace_back(prefix).c_str();
			m_luau[index] = name.starts_with("Luau/") ? 1 : 0;
		}
	}

	HeapMemory::~HeapMemory()
	{
		stop();
	}

	bool HeapMemory::active() const
	{
		return m_events.load(std::memory_order_relaxed);
	}

	std::uint64_t HeapMemory::take_events()
	{
		return m_seen.exchange(0, std::memory_order_relaxed);
	}

	std::uint64_t HeapMemory::dropped() const
	{
		return m_failures.load(std::memory_order_relaxed);
	}

	bool HeapMemory::bind()
	{
		const std::array<std::pair<std::string_view, void*>, 6> hooks{{
		    {"mi_free_prepared", reinterpret_cast<void*>(&heap_free_prepared)},
		    {"mi_free", reinterpret_cast<void*>(&heap_free)},
		    {"mi_realloc", reinterpret_cast<void*>(&heap_realloc)},
		    {"mi_realloc_aligned", reinterpret_cast<void*>(&heap_realloc_aligned)},
		    {"mi_malloc", reinterpret_cast<void*>(&heap_malloc)},
		    {"mi_malloc_aligned", reinterpret_cast<void*>(&heap_malloc_aligned)},
		}};
		m_bindings.clear();
		const auto image = rml::platform::studio_image_name();
		for (const auto& [symbol, hook] : hooks)
		{
			auto slots = rml::platform::find_import_slots(image, symbol);
			if (slots.empty())
			{
				m_log->warn("heap memory events off: no import slot for {}", symbol);
				m_bindings.clear();
				return false;
			}
			m_bindings.push_back({symbol, std::move(slots), hook});
		}
		g_heap.store(this, std::memory_order_release);
		for (const auto& binding : m_bindings)
		{
			if (!rml::platform::rebind_import_slots(binding.slots, binding.hook))
			{
				m_log->warn("heap memory events off: could not rebind {}", binding.symbol);
				unbind();
				return false;
			}
		}
		return true;
	}

	void HeapMemory::unbind()
	{
		for (const auto& binding : m_bindings)
			rml::platform::restore_import_slots(binding.slots);
		m_bindings.clear();
		discard_all();
	}

	void HeapMemory::discard_all() const
	{
		for (const auto& name : m_names)
			discard(name.c_str());
	}

	void HeapMemory::discard(const char* pool) const
	{
		if (___tracy_connected())
			___tracy_emit_memory_discard(pool);
	}

	void HeapMemory::apply(const Settings& settings)
	{
		m_depth.store(settings.memory_callstack_depth, std::memory_order_relaxed);
		const auto min_size = static_cast<std::size_t>(settings.memory_events_min_size);
		if (m_min_size.exchange(min_size, std::memory_order_relaxed) < min_size && m_bound)
			discard_all();
		const auto was_skipping = m_skip_luau.exchange(settings.memory_events_luau, std::memory_order_relaxed);
		if (m_bound && settings.memory_events_luau && !was_skipping)
		{
			if (const auto luau = std::ranges::find(m_luau, std::uint8_t{1}); luau != m_luau.end())
				discard(m_pools[static_cast<std::size_t>(luau - m_luau.begin())]);
		}
		if (settings.memory_events_heap && !m_bound)
		{
			m_events.store(true, std::memory_order_relaxed);
			m_bound = bind();
			if (!m_bound)
				m_events.store(false, std::memory_order_relaxed);
		}
		else if (!settings.memory_events_heap && m_bound)
		{
			m_events.store(false, std::memory_order_relaxed);
			unbind();
			m_bound = false;
		}
	}

	void HeapMemory::stop()
	{
		if (!m_bound && !g_heap.load(std::memory_order_acquire))
			return;
		m_events.store(false, std::memory_order_relaxed);
		if (m_bound)
		{
			unbind();
			m_bound = false;
		}
		g_heap.store(nullptr, std::memory_order_release);
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		g_in_flight.wait_idle(std::chrono::seconds(2));
	}

	void HeapMemory::record_allocation(void* block, const std::size_t size)
	{
		if (!m_events.load(std::memory_order_relaxed) || size < m_min_size.load(std::memory_order_relaxed) || !___tracy_connected())
			return;
		try
		{
			const auto category = std::bit_cast<RBX::Memory::CategoryWord>(m_engine.resolve_category(block)).category;
			if (m_skip_luau.load(std::memory_order_relaxed) && m_luau[category])
				return;
			const auto depth = std::min(m_depth.load(std::memory_order_relaxed), Callstacks::capacity);
			const auto scan = std::min(std::max(depth, 8), Callstacks::capacity);
			std::array<std::uint64_t, Callstacks::capacity> frames{};
			const auto captured = m_callstacks.capture_from_studio(frames.data(), scan, 1);
			for (int index = 0; index < captured; ++index)
			{
				if (frames[index] - 1 - m_image_begin < m_image_size)
					return;
			}
			___tracy_emit_memory_alloc_frames_named(block, size, frames.data(), std::min(captured, depth), m_pools[category]);
			m_seen.fetch_add(1, std::memory_order_relaxed);
		}
		catch (...)
		{
			m_failures.fetch_add(1, std::memory_order_relaxed);
		}
	}

	void HeapMemory::record_free(void* block)
	{
		if (!m_events.load(std::memory_order_relaxed) || !___tracy_connected())
			return;
		try
		{
			if (const auto minimum = m_min_size.load(std::memory_order_relaxed); minimum > 0 && m_engine.usable_size(block) < minimum)
				return;
			const auto category = std::bit_cast<RBX::Memory::CategoryWord>(m_engine.resolve_category(block)).category;
			if (m_skip_luau.load(std::memory_order_relaxed) && m_luau[category])
				return;
			___tracy_emit_memory_free_named(block, m_pools[category]);
			m_seen.fetch_add(1, std::memory_order_relaxed);
		}
		catch (...)
		{
			m_failures.fetch_add(1, std::memory_order_relaxed);
		}
	}
}
