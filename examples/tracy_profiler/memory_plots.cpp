#include "memory_plots.hpp"

#include "capture.hpp"
#include "settings.hpp"

#include <bit>
#include <format>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	static constexpr std::uint32_t datamodel_tags = 32;

	struct HeapWalk
	{
		std::uint64_t* bytes{};
		std::uint64_t* by_datamodel{};
	};

	static void visit(const std::uint32_t word, const std::size_t live, void* arg)
	{
		const auto& walk = *static_cast<HeapWalk*>(arg);
		const auto parts = std::bit_cast<RBX::Memory::CategoryWord>(word);
		walk.bytes[parts.category] += live;
		if (walk.by_datamodel)
			walk.by_datamodel[parts.data_model_tag * RBX::Memory::max_categories + parts.category] += live;
	}

	MemoryPlots::MemoryPlots(const MemoryEngine& engine, LuauMemory* luau, std::function<std::uint64_t()> dropped, std::function<void()> tick) :
	    m_engine(engine),
	    m_luau(luau),
	    m_dropped(std::move(dropped)),
	    m_tick(std::move(tick)),
	    m_heap_bytes(RBX::Memory::max_categories)
	{
	}

	MemoryPlots::~MemoryPlots()
	{
		stop();
	}

	void MemoryPlots::apply(const Settings& settings)
	{
		m_enabled.store(settings.memory_plots, std::memory_order_relaxed);
		m_by_datamodel.store(settings.memory_plots_by_datamodel, std::memory_order_relaxed);
	}

	void MemoryPlots::start()
	{
		if (m_engine.has_categories())
		{
			const auto count = std::min(m_engine.category_count(), RBX::Memory::max_categories);
			m_categories.reserve(count);
			m_heap_plots.reserve(count);
			m_gpu_plots.reserve(count);
			for (std::uint32_t index = 0; index < count; ++index)
			{
				const auto* name = m_engine.category_name(index);
				m_categories.emplace_back(name && *name ? name : "default");
				m_heap_plots.push_back("Memory/Heap/" + m_categories.back());
				m_gpu_plots.push_back("Memory/GPU/" + m_categories.back());
			}
		}
		m_thread = std::jthread([this](const std::stop_token stop) {
			run(stop);
		});
	}

	void MemoryPlots::stop()
	{
		if (!m_thread.joinable())
			return;
		m_thread.request_stop();
		m_thread.join();
	}

	void MemoryPlots::run(const std::stop_token stop)
	{
		while (!stop.stop_requested())
		{
			try
			{
				sample();
			}
			catch (...)
			{
			}
			std::unique_lock lock(m_wait_mutex);
			m_wait.wait_for(lock, stop, sample_period, [] {
				return false;
			});
		}
	}

	void MemoryPlots::plot(const char* name, const std::uint64_t value)
	{
		const auto found = m_last.find(name);
		if (found == m_last.end())
		{
			if (value == 0)
				return;
			m_last.emplace(name, value);
			___tracy_emit_plot_config(name, TracyPlotFormatMemory, 0, 1, 0);
		}
		else if (found->second == value)
			return;
		else
			found->second = value;
		___tracy_emit_plot_int(name, static_cast<std::int64_t>(value));
	}

	const char* MemoryPlots::datamodel_plot(const std::uint32_t tag, const std::uint32_t category)
	{
		const auto key = tag * RBX::Memory::max_categories + category;
		if (const auto found = m_datamodel_plots.find(key); found != m_datamodel_plots.end())
			return found->second;
		const auto& text = m_names.emplace_back(std::format("Memory/Heap/DataModel {}/{}", tag, m_categories[category]));
		return m_datamodel_plots.emplace(key, text.c_str()).first->second;
	}

	void MemoryPlots::sample()
	{
		if (m_luau)
			m_luau->maintain();
		if (m_tick)
			m_tick();
		if (!___tracy_connected())
			return;
		if (const auto epoch = connection_epoch(); epoch != m_epoch)
		{
			m_epoch = epoch;
			m_last.clear();
		}
		if (m_enabled.load(std::memory_order_relaxed))
		{
			sample_heap();
			sample_gpu();
			sample_luau();
		}
		sample_health();
	}

	void MemoryPlots::sample_heap()
	{
		if (!m_engine.visit_live_bytes || m_categories.empty())
			return;
		std::ranges::fill(m_heap_bytes, 0);
		const auto by_datamodel = m_by_datamodel.load(std::memory_order_relaxed);
		if (by_datamodel)
			m_datamodel_bytes.assign(static_cast<std::size_t>(datamodel_tags) * RBX::Memory::max_categories, 0);
		HeapWalk walk{m_heap_bytes.data(), by_datamodel ? m_datamodel_bytes.data() : nullptr};
		m_engine.visit_live_bytes(&visit, &walk);
		for (std::size_t index = 0; index < m_categories.size(); ++index)
			plot(m_heap_plots[index].c_str(), m_heap_bytes[index]);
		if (!by_datamodel)
			return;
		for (std::uint32_t tag = 0; tag < datamodel_tags; ++tag)
		{
			for (std::uint32_t category = 0; category < m_categories.size(); ++category)
			{
				const auto bytes = m_datamodel_bytes[tag * RBX::Memory::max_categories + category];
				if (bytes != 0 || m_datamodel_plots.contains(tag * RBX::Memory::max_categories + category))
					plot(datamodel_plot(tag, category), bytes);
			}
		}
	}

	void MemoryPlots::sample_gpu()
	{
		if (!m_engine.category_total)
			return;
		for (std::uint32_t index = 0; index < m_categories.size(); ++index)
			plot(m_gpu_plots[index].c_str(), m_engine.category_total(index, RBX::Memory::MemoryType::Gpu));
	}

	void MemoryPlots::sample_luau()
	{
		if (!m_luau)
			return;
		m_luau->sample(m_luau_samples);
		for (const auto& vm : m_luau_samples)
		{
			plot(vm.total, vm.total_bytes);
			for (const auto& [name, bytes] : vm.categories)
				plot(name, bytes);
		}
	}

	void MemoryPlots::sample_health()
	{
		___tracy_emit_plot_int("RML/Memory events dropped", static_cast<std::int64_t>(m_dropped ? m_dropped() : 0));
		___tracy_emit_plot_int("RML/Luau hooks", static_cast<std::int64_t>(m_luau ? m_luau->hooked() : 0));
	}
}
