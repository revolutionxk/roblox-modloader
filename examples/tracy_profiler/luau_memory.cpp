#include "luau_memory.hpp"

#include "callstack.hpp"
#include "detours.hpp"
#include "luau_frames.hpp"
#include "settings.hpp"

#include <RobloxModLoader/hooking/hooking.hpp>
#include <algorithm>
#include <bit>
#include <format>
#include <spdlog/spdlog.h>
#include <string_view>
#include <thread>
#include <tracy/TracyC.h>
#include <utility>

namespace tracy_profiler
{
	using rml::luau::mirror::AllocationHook;
	using rml::luau::mirror::FreeHook;
	using rml::luau::mirror::GlobalState;
	using rml::luau::mirror::LuaState;

	static InFlight g_in_flight;
	static std::atomic<LuauMemory*> g_luau{nullptr};
	static std::atomic<std::int32_t> g_closing{0};
	static thread_local LuaState* t_stack_moving = nullptr;

	struct CloseScope
	{
		CloseScope()
		{
			g_closing.fetch_add(1);
		}

		~CloseScope()
		{
			g_closing.fetch_sub(1);
		}

		CloseScope(const CloseScope&) = delete;
		CloseScope& operator=(const CloseScope&) = delete;
	};

	static bool has_name(const std::string_view names, const std::string_view name)
	{
		std::size_t begin = 0;
		while (begin <= names.size())
		{
			const auto end = std::min(names.find(", ", begin), names.size());
			if (names.substr(begin, end - begin) == name)
				return true;
			begin = end + 2;
		}
		return false;
	}

	static void discard_pool(const char* pool)
	{
		if (___tracy_connected())
			___tracy_emit_memory_discard(pool);
	}

	static void on_allocate(LuaState* state, void* block, std::size_t, const std::size_t size, std::uint8_t, std::int32_t, std::int32_t)
	{
		const InFlight::Call call{g_in_flight};
		const auto moving = std::exchange(t_stack_moving, nullptr) == state;
		if (auto* self = g_luau.load(std::memory_order_acquire))
		{
			try
			{
				self->emit_allocation(state, block, size, !moving);
			}
			catch (...)
			{
			}
		}
	}

	static void on_free(LuaState* state, void* block)
	{
		const InFlight::Call call{g_in_flight};
		if (state && block && (block == state->stack || block == state->base_ci))
			t_stack_moving = state;
		if (auto* self = g_luau.load(std::memory_order_acquire))
		{
			try
			{
				self->emit_free(state, block);
			}
			catch (...)
			{
			}
		}
	}

	static LuaState* lua_newstate_detour(void* allocator, void* ud)
	{
		const InFlight::Call call{g_in_flight};
		auto* state = original<&lua_newstate_detour>()(allocator, ud);
		if (auto* self = g_luau.load(std::memory_order_acquire); self && state)
		{
			try
			{
				self->add(state->global);
			}
			catch (...)
			{
			}
		}
		return state;
	}

	static void lua_close_detour(LuaState* state)
	{
		const InFlight::Call call{g_in_flight};
		const CloseScope scope;
		auto* self = g_luau.load(std::memory_order_seq_cst);
		auto* global = state ? state->global : nullptr;
		if (self && global)
		{
			try
			{
				self->begin_close(global);
			}
			catch (...)
			{
			}
		}
		original<&lua_close_detour>()(state);
		if (self && global && g_luau.load(std::memory_order_seq_cst) == self)
		{
			try
			{
				self->end_close(global);
			}
			catch (...)
			{
			}
		}
	}

	static std::uint8_t memory_category_index_detour(void* facet, LuaState* state, const std::string& name, const bool sequential)
	{
		const InFlight::Call call{g_in_flight};
		const auto index = original<&memory_category_index_detour>()(facet, state, name, sequential);
		if (auto* self = g_luau.load(std::memory_order_acquire); self && state)
		{
			try
			{
				self->name_category(state->global, index, name);
			}
			catch (...)
			{
			}
		}
		return index;
	}

	LuauMemory::LuauMemory(const MemoryEngine& engine, const Callstacks& callstacks, LuauFrames& frames, std::shared_ptr<spdlog::logger> log) :
	    m_engine(engine),
	    m_callstacks(callstacks),
	    m_frames(frames),
	    m_log(std::move(log))
	{
	}

	LuauMemory::~LuauMemory()
	{
		remove();
	}

	void LuauMemory::install()
	{
		if (m_installed || !m_engine.has_luau())
			return;
		g_luau.store(this, std::memory_order_release);
		rml::Hooking::DetourHookHelper::add<&lua_close_detour>("lua_close", reinterpret_cast<void*>(m_engine.lua_close));
		if (m_engine.memory_category_index)
		{
			rml::Hooking::DetourHookHelper::add<&memory_category_index_detour>("MemoryCategoriesFacet::getMemoryCategoryIndex",
			    reinterpret_cast<void*>(m_engine.memory_category_index));
			m_names_hooked = true;
		}
		rml::Hooking::DetourHookHelper::add<&lua_newstate_detour>("lua_newstate", reinterpret_cast<void*>(m_engine.lua_newstate));
		m_installed = true;
	}

	void LuauMemory::apply(const Settings& settings)
	{
		m_native_depth.store(settings.memory_callstack_depth, std::memory_order_relaxed);
		m_luau_depth.store(settings.memory_luau_depth, std::memory_order_relaxed);
		m_events.store(settings.memory_events_luau, std::memory_order_relaxed);
		maintain();
	}

	void LuauMemory::remove()
	{
		if (!m_installed)
			return;
		m_events.store(false, std::memory_order_relaxed);
		{
			std::scoped_lock lock(m_mutex);
			for (auto& vm : m_vms)
			{
				if (vm.hooked && !vm.closing)
					unhook(vm);
				vm.hooked = false;
			}
		}
		g_luau.store(nullptr, std::memory_order_seq_cst);
		rml::Hooking::DetourHookHelper::disable<&lua_newstate_detour>();
		rml::Hooking::DetourHookHelper::disable<&lua_close_detour>();
		if (m_names_hooked)
			rml::Hooking::DetourHookHelper::disable<&memory_category_index_detour>();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		while (g_closing.load() != 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		if (!g_in_flight.wait_idle(std::chrono::seconds(2)))
		{
			m_log->warn("Luau memory: detours still busy; left installed");
			return;
		}
		while (g_closing.load() != 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		rml::Hooking::DetourHookHelper::remove<&lua_newstate_detour>();
		rml::Hooking::DetourHookHelper::remove<&lua_close_detour>();
		if (m_names_hooked)
			rml::Hooking::DetourHookHelper::remove<&memory_category_index_detour>();
		m_names_hooked = false;
		m_installed = false;
	}

	bool LuauMemory::hook(Vm& vm) const
	{
		std::atomic_ref allocation(vm.global->onallocate);
		std::atomic_ref release(vm.global->onfree);
		FreeHook expected_release = nullptr;
		if (!release.compare_exchange_strong(expected_release, &on_free))
			return false;
		AllocationHook expected_allocation = nullptr;
		if (!allocation.compare_exchange_strong(expected_allocation, &on_allocate))
		{
			FreeHook ours = &on_free;
			release.compare_exchange_strong(ours, nullptr);
			return false;
		}
		return true;
	}

	void LuauMemory::unhook(Vm& vm) const
	{
		AllocationHook ours = &on_allocate;
		FreeHook ours_release = &on_free;
		std::atomic_ref(vm.global->onallocate).compare_exchange_strong(ours, nullptr);
		std::atomic_ref(vm.global->onfree).compare_exchange_strong(ours_release, nullptr);
	}

	void LuauMemory::maintain()
	{
		std::scoped_lock lock(m_mutex);
		const auto events = m_events.load(std::memory_order_relaxed);
		std::size_t hooked = 0;
		for (auto& vm : m_vms)
		{
			if (vm.closing)
				continue;
			if (vm.hooked && std::atomic_ref(vm.global->onallocate).load() != &on_allocate)
			{
				unhook(vm);
				vm.hooked = false;
				discard_pool(vm.pool);
				if (!vm.yielded)
					m_log->info("Luau {} hook taken by the engine; its Luau events pause until the engine releases it", vm.label);
				vm.yielded = true;
			}
			if (events && !vm.hooked)
				vm.hooked = hook(vm);
			else if (!events && vm.hooked)
			{
				unhook(vm);
				vm.hooked = false;
				discard_pool(vm.pool);
			}
			if (vm.hooked)
				++hooked;
		}
		m_hooked.store(hooked, std::memory_order_relaxed);
	}

	std::size_t LuauMemory::hooked() const
	{
		return m_hooked.load(std::memory_order_relaxed);
	}

	const char* LuauMemory::intern(std::string text)
	{
		return m_names.emplace_back(std::move(text)).c_str();
	}

	LuauMemory::Vm* LuauMemory::find(const GlobalState* global)
	{
		const auto found = std::ranges::find_if(m_vms, [global](const Vm& vm) {
			return vm.global == global && !vm.closing;
		});
		return found == m_vms.end() ? nullptr : &*found;
	}

	LuauMemory::Vm& LuauMemory::ensure(GlobalState* global)
	{
		if (auto* existing = find(global))
			return *existing;
		const auto tag = m_engine.get_category ? std::bit_cast<RBX::Memory::CategoryWord>(m_engine.get_category()).data_model_tag : 0u;
		auto& vm = m_vms.emplace_back();
		vm.global = global;
		const auto id = m_next_id++;
		vm.label = tag ? std::format("VM {} (DataModel {})", id, tag) : std::format("VM {}", id);
		vm.pool = intern("Luau/" + vm.label);
		vm.total = intern("Memory/Luau/" + vm.label + "/total");
		for (auto& route : m_routes)
		{
			if (route.global.load(std::memory_order_acquire))
				continue;
			const auto slot = static_cast<std::size_t>(&route - m_routes.data());
			route.pool.store(vm.pool, std::memory_order_release);
			route.global.store(global, std::memory_order_release);
			vm.route = slot;
			if (slot + 1 > m_route_limit.load(std::memory_order_relaxed))
				m_route_limit.store(slot + 1, std::memory_order_release);
			break;
		}
		if (!vm.route)
			m_log->warn("Luau {} has no free route slot; its Luau events are dropped", vm.label);
		m_log->info("Luau {} registered", vm.label);
		return vm;
	}

	const char* LuauMemory::pool_of(const GlobalState* global) const
	{
		const auto limit = m_route_limit.load(std::memory_order_acquire);
		for (std::size_t index = 0; index < limit; ++index)
		{
			if (m_routes[index].global.load(std::memory_order_acquire) == global)
				return m_routes[index].pool.load(std::memory_order_acquire);
		}
		return nullptr;
	}

	void LuauMemory::add(GlobalState* global)
	{
		std::scoped_lock lock(m_mutex);
		auto& vm = ensure(global);
		if (m_events.load(std::memory_order_relaxed) && !vm.hooked)
			vm.hooked = hook(vm);
	}

	void LuauMemory::begin_close(GlobalState* global)
	{
		std::scoped_lock lock(m_mutex);
		if (auto* vm = find(global))
			vm->closing = true;
	}

	void LuauMemory::end_close(GlobalState* global)
	{
		std::scoped_lock lock(m_mutex);
		const auto found = std::ranges::find_if(m_vms, [global](const Vm& vm) {
			return vm.global == global && vm.closing;
		});
		if (found == m_vms.end())
			return;
		if (found->route)
			m_routes[*found->route].global.store(nullptr, std::memory_order_release);
		m_vms.erase(found);
	}

	void LuauMemory::name_category(GlobalState* global, const std::uint8_t index, const std::string& name)
	{
		std::scoped_lock lock(m_mutex);
		const auto closing = std::ranges::any_of(m_vms, [global](const Vm& vm) {
			return vm.global == global && vm.closing;
		});
		if (closing && !find(global))
			return;
		auto& vm = ensure(global);
		auto& names = vm.names[index];
		if (has_name(names, name))
			return;
		if (names.size() + name.size() > 200)
			return;
		if (!names.empty())
			names += ", ";
		names += name;
		vm.plots[index] = intern("Memory/Luau/" + vm.label + "/" + names);
	}

	void LuauMemory::emit_allocation(LuaState* state, void* block, const std::size_t size, const bool walk)
	{
		if (!m_events.load(std::memory_order_relaxed) || !state || !block || !___tracy_connected())
			return;
		const auto* pool = pool_of(state->global);
		if (!pool)
			return;
		std::array<std::uint64_t, LuauFrames::max_depth + Callstacks::capacity> frames{};
		auto count = walk ? m_frames.capture(*state, frames.data(), std::min(m_luau_depth.load(std::memory_order_relaxed), LuauFrames::max_depth)) : 0;
		count += m_callstacks.capture_from_studio(frames.data() + count, std::min(m_native_depth.load(std::memory_order_relaxed), Callstacks::capacity), 1);
		___tracy_emit_memory_alloc_frames_named(block, size, frames.data(), count, pool);
	}

	void LuauMemory::emit_free(LuaState* state, void* block)
	{
		if (!m_events.load(std::memory_order_relaxed) || !state || !block || !___tracy_connected())
			return;
		if (const auto* pool = pool_of(state->global))
			___tracy_emit_memory_free_named(block, pool);
	}

	void LuauMemory::sample(std::vector<Sample>& samples)
	{
		samples.clear();
		std::scoped_lock lock(m_mutex);
		for (auto& vm : m_vms)
		{
			if (vm.closing)
				continue;
			auto& sample = samples.emplace_back();
			sample.total = vm.total;
			sample.total_bytes = std::atomic_ref(vm.global->totalbytes).load(std::memory_order_relaxed);
			for (std::size_t index = 0; index < vm.plots.size(); ++index)
			{
				const auto bytes = std::atomic_ref(vm.global->memcatbytes[index]).load(std::memory_order_relaxed);
				if (bytes == 0 && !vm.plots[index])
					continue;
				if (!vm.plots[index])
					vm.plots[index] = intern(std::format("Memory/Luau/{}/memcat {}", vm.label, index));
				sample.categories.emplace_back(vm.plots[index], bytes);
			}
		}
	}
}
