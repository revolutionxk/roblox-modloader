#include "external_memory.hpp"

#include "callstack.hpp"
#include "capture.hpp"
#include "detours.hpp"
#include "settings.hpp"

#include <RobloxModLoader/hooking/hooking.hpp>
#include <RobloxModLoader/memory/string_anchor.hpp>
#include <algorithm>
#include <array>
#include <thread>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	static InFlight g_in_flight;
	static std::atomic<ExternalMemory*> g_external{nullptr};

	static void track_allocate(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::uintptr_t id, const std::size_t size)
	{
		const InFlight::Call call{g_in_flight};
		original<&track_allocate>()(category, type, id, size);
		if (auto* self = g_external.load(std::memory_order_acquire); self && self->recording())
		{
			try
			{
				self->allocate(category, type, id, size);
			}
			catch (...)
			{
				self->count_failure();
			}
		}
	}

	static void track_deallocate(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::uintptr_t id, const std::size_t size)
	{
		const InFlight::Call call{g_in_flight};
		if (auto* self = g_external.load(std::memory_order_acquire); self && self->recording())
		{
			try
			{
				if (___tracy_connected() && !self->from_flush())
					self->release(category, type, id, size);
			}
			catch (...)
			{
				self->count_failure();
			}
		}
		original<&track_deallocate>()(category, type, id, size);
	}

	static void track_deallocate_deferred(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::uintptr_t id, const std::size_t size)
	{
		const InFlight::Call call{g_in_flight};
		if (auto* self = g_external.load(std::memory_order_acquire); self && self->recording())
		{
			try
			{
				self->release(category, type, id, size);
			}
			catch (...)
			{
				self->count_failure();
			}
		}
		original<&track_deallocate_deferred>()(category, type, id, size);
	}

	std::size_t ExternalMemory::ResourceHash::operator()(const Resource& resource) const noexcept
	{
		auto hash = std::hash<std::uintptr_t>{}(resource.id);
		hash ^= (static_cast<std::size_t>(resource.category) << 1) ^ (static_cast<std::size_t>(resource.type) << 12);
		return hash;
	}

	ExternalMemory::ExternalMemory(const MemoryEngine& engine, const Callstacks& callstacks) :
	    m_engine(engine),
	    m_callstacks(callstacks)
	{
		const auto count = std::min(m_engine.category_count(), RBX::Memory::max_categories);
		m_gpu_pools.reserve(count);
		m_heap_pools.reserve(count);
		for (std::uint32_t index = 0; index < count; ++index)
		{
			const auto* name = m_engine.category_name(index);
			const std::string text = name && *name ? name : "default";
			m_gpu_pools.push_back("GPU/" + text);
			m_heap_pools.push_back("External/" + text);
		}
	}

	ExternalMemory::~ExternalMemory()
	{
		remove();
	}

	bool ExternalMemory::install()
	{
		if (m_installed || !m_engine.has_external() || m_gpu_pools.empty())
			return m_installed;
		g_external.store(this, std::memory_order_release);
		rml::Hooking::DetourHookHelper::add<&track_deallocate>("RBX::Memory::trackExternalDeallocate",
		    reinterpret_cast<void*>(m_engine.track_external_deallocate));
		rml::Hooking::DetourHookHelper::add<&track_deallocate_deferred>("RBX::Memory::trackExternalDeallocateDeferred",
		    reinterpret_cast<void*>(m_engine.track_external_deallocate_deferred));
		rml::Hooking::DetourHookHelper::add<&track_allocate>("RBX::Memory::trackExternalAllocate",
		    reinterpret_cast<void*>(m_engine.track_external_allocate));
		m_installed = true;
		if (!rml::Hooking::get_original<&track_allocate>() || !rml::Hooking::get_original<&track_deallocate>() || !rml::Hooking::get_original<&track_deallocate_deferred>())
			remove();
		return m_installed;
	}

	void ExternalMemory::apply(const Settings& settings)
	{
		m_depth.store(settings.memory_callstack_depth, std::memory_order_relaxed);
		const bool wanted = settings.memory_events_external;
		if (m_events.exchange(wanted, std::memory_order_relaxed) && !wanted)
			forget_all();
	}

	void ExternalMemory::remove()
	{
		if (!m_installed)
			return;
		m_events.store(false, std::memory_order_relaxed);
		g_external.store(nullptr, std::memory_order_release);
		rml::Hooking::DetourHookHelper::disable<&track_allocate>();
		rml::Hooking::DetourHookHelper::disable<&track_deallocate>();
		rml::Hooking::DetourHookHelper::disable<&track_deallocate_deferred>();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		g_in_flight.wait_idle(std::chrono::seconds(2));
		rml::Hooking::DetourHookHelper::remove<&track_allocate>();
		rml::Hooking::DetourHookHelper::remove<&track_deallocate>();
		rml::Hooking::DetourHookHelper::remove<&track_deallocate_deferred>();
		m_installed = false;
	}

	std::uint64_t ExternalMemory::dropped() const
	{
		return m_failures.load(std::memory_order_relaxed);
	}

	void ExternalMemory::count_failure()
	{
		m_failures.fetch_add(1, std::memory_order_relaxed);
	}

	bool ExternalMemory::recording() const
	{
		return m_events.load(std::memory_order_relaxed);
	}

	bool ExternalMemory::from_flush() const
	{
		const auto caller = m_callstacks.studio_caller();
		if (!caller)
			return false;
		const auto function = rml::memory::function_containing(reinterpret_cast<const void*>(caller - 1));
		if (!function)
			return false;
		const auto* start = reinterpret_cast<const void*>(function->start);
		return start == m_engine.flush_deferred_category || start == m_engine.flush_deferred_all;
	}

	const char* ExternalMemory::pool(const std::uint32_t category, const RBX::Memory::MemoryType type) const
	{
		const auto& pools = type == RBX::Memory::MemoryType::Gpu ? m_gpu_pools : m_heap_pools;
		return pools[category < pools.size() ? category : 0].c_str();
	}

	ExternalMemory::LiveMap::iterator ExternalMemory::newest_with_size(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::size_t size)
	{
		auto newest = m_live.end();
		std::uint64_t newest_address = 0;
		for (auto it = m_live.begin(); it != m_live.end(); ++it)
		{
			if (it->first.category != category || it->first.type != type)
				continue;
			for (const auto& live : it->second)
			{
				if (live.size == size && live.address > newest_address)
				{
					newest = it;
					newest_address = live.address;
				}
			}
		}
		return newest;
	}

	void ExternalMemory::observe_epoch()
	{
		const auto epoch = connection_epoch();
		if (epoch == m_epoch)
			return;
		m_epoch = epoch;
		m_live.clear();
		m_next = address_base;
	}

	void ExternalMemory::forget_all()
	{
		std::scoped_lock lock(m_mutex);
		observe_epoch();
		if (___tracy_connected())
		{
			for (const auto& [resource, entries] : m_live)
			{
				for (const auto& live : entries)
					___tracy_emit_memory_free_named(reinterpret_cast<const void*>(live.address),
					    pool(resource.category, resource.type));
			}
		}
		m_live.clear();
		m_next = address_base;
	}

	void ExternalMemory::allocate(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::uintptr_t id, const std::size_t size)
	{
		if (!___tracy_connected())
			return;
		std::array<std::uint64_t, Callstacks::capacity> frames{};
		const auto depth = m_callstacks.capture_from_studio(frames.data(), std::min(m_depth.load(std::memory_order_relaxed), Callstacks::capacity), 0);
		std::scoped_lock lock(m_mutex);
		if (!m_events.load(std::memory_order_relaxed))
			return;
		observe_epoch();
		const auto address = m_next;
		m_next += (std::max<std::uint64_t>(size, 1) + 0xFFF) & ~std::uint64_t{0xFFF};
		m_live[{category, type, id}].push_back({address, size});
		___tracy_emit_memory_alloc_frames_named(reinterpret_cast<const void*>(address), size, frames.data(), depth, pool(category, type));
	}

	void ExternalMemory::release(const std::uint32_t category, const RBX::Memory::MemoryType type, const std::uintptr_t id, const std::size_t size)
	{
		if (!___tracy_connected())
			return;
		std::scoped_lock lock(m_mutex);
		observe_epoch();
		auto found = m_live.find({category, type, id});
		if (found == m_live.end() && id == 0)
			found = newest_with_size(category, type, size);
		if (found == m_live.end() || found->second.empty())
			return;
		auto& entries = found->second;
		const auto newest = std::ranges::find(entries.rbegin(), entries.rend(), size, &Live::size);
		const auto match = newest == entries.rend() ? std::prev(entries.end()) : std::prev(newest.base());
		const auto address = match->address;
		entries.erase(match);
		if (entries.empty())
			m_live.erase(found);
		___tracy_emit_memory_free_named(reinterpret_cast<const void*>(address), pool(category, type));
	}
}
