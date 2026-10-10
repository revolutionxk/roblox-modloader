#include "callstack.hpp"
#include "capture.hpp"
#include "engine.hpp"
#include "frame_images.hpp"
#include "gpu_zones.hpp"
#include "memory.hpp"
#include "sampling.hpp"
#include "settings.hpp"
#include "symbols.hpp"
#include "timers.hpp"

#include <RobloxModLoader/config/mod_settings.hpp>
#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/memory/module.hpp>
#include <RobloxModLoader/mod/mod_base.hpp>
#include <RobloxModLoader/platform/graphics/gpu_timeline.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>
#include <RobloxModLoader/render/render.hpp>
#include <exception>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <tracy/TracyC.h>
#include <vector>

static std::string join(const std::vector<std::string>& names)
{
	std::string joined;
	for (const auto& name : names)
	{
		if (!joined.empty())
			joined += ", ";
		joined += name;
	}
	return joined;
}

class TracyProfiler final : public ModBase
{
public:
	TracyProfiler()
	{
		name = "Tracy Profiler";
		version = "0.1.0";
		author = "RML";
		description = "Streams Roblox Studio's MicroProfile scopes, frames, labels, counters and CPU samples into the Tracy profiler.";
		m_log = rml::Logger::get_logger("TracyProfiler");
	}

	void on_load() override
	{
		try
		{
			start();
		}
		catch (const std::exception& error)
		{
			m_log->error("start failed: {}", error.what());
			stop();
		}
		catch (...)
		{
			m_log->error("start failed");
			stop();
		}
	}

	void on_unload() override
	{
		try
		{
			stop();
		}
		catch (const std::exception& error)
		{
			m_log->error("stop failed: {}", error.what());
		}
		catch (...)
		{
			m_log->error("stop failed");
		}
	}

private:
	void start()
	{
		m_store = std::make_unique<rml::config::ModSettings>(paths().config_file("config.toml"));
		m_store->write_default_if_missing(tracy_profiler::default_config_template());
		m_store->load();
		m_settings = tracy_profiler::read(*m_store);

		if (!m_settings.enabled)
		{
			m_log->info("disabled in config.toml");
			return;
		}

		const auto engine = tracy_profiler::resolve_engine();
		if (const auto missing = engine.missing_required(); !missing.empty())
		{
			m_log->error("MicroProfile signatures missing ({}); Roblox Studio was probably updated, the profiler stays off", join(missing));
			return;
		}

		if (const auto missing = engine.missing_optional(); !missing.empty())
			m_log->warn("optional MicroProfile signatures missing ({}); those features stay off", join(missing));

		___tracy_set_sampling_frequency(m_settings.sampling_hz);
		___tracy_startup_profiler();
		m_started = true;

		m_callstacks = std::make_unique<tracy_profiler::Callstacks>(reinterpret_cast<const void*>(engine.enter));
		m_timers = std::make_unique<tracy_profiler::Timers>(*engine.state);
		const rml::memory::module studio{rml::platform::studio_image_name()};
		m_symbols = std::make_unique<tracy_profiler::Symbols>(*m_timers, studio.begin().as<std::uintptr_t>());
		m_symbols->load_file(paths().file("symbols.txt"));
		m_symbols->add(reinterpret_cast<const void*>(engine.enter), "MicroProfileEnter");
		m_symbols->add(reinterpret_cast<const void*>(engine.leave), "MicroProfileLeave");
		m_symbols->add(reinterpret_cast<const void*>(engine.flip_cpu), "MicroProfileFlipCpu");
		m_symbols->add(reinterpret_cast<const void*>(engine.put_label), "MicroProfilePutLabel");
		m_symbols->add(reinterpret_cast<const void*>(engine.label_literal), "MicroProfileLabelLiteral");
		m_symbols->add(reinterpret_cast<const void*>(engine.on_thread_create), "MicroProfileOnThreadCreate");
		m_symbols->add(reinterpret_cast<const void*>(engine.get_token), "RBX::Profiler::getToken");
		m_symbols->index_rtti();
		m_symbols->index_scope_owners(reinterpret_cast<const void*>(engine.get_token));
		m_timers->attach(*m_symbols);
		___tracy_set_symbol_resolver(&tracy_profiler::Symbols::tracy_resolver, m_symbols.get());
		___tracy_set_sample_hook(&tracy_profiler::sample_hook, nullptr);

		tracy_profiler::apply_settings(m_settings);
		tracy_profiler::install_capture(engine, *m_timers, *m_callstacks);
		m_installed = true;

		auto frame_images = std::make_unique<tracy_profiler::FrameImages>();
		frame_images->apply(m_settings);
		m_frame_images = frame_images.get();
		rml::render::graph().add_pass(std::move(frame_images));

		m_memory = std::make_unique<tracy_profiler::Memory>(*m_callstacks, m_log);
		m_memory->start(m_settings);

		m_store->watch([this] {
			try
			{
				std::scoped_lock lock(m_mutex);
				m_settings = tracy_profiler::read(*m_store);
				tracy_profiler::apply_settings(m_settings);
				if (m_frame_images)
					m_frame_images->apply(m_settings);
				if (m_gpu_zones)
					m_gpu_zones->set_enabled(m_settings.gpu_zones);
				if (m_memory)
					m_memory->apply(m_settings);
				m_log->info("config.toml reloaded (pass_through={}, zone_callstacks={}, callstack_depth={})",
				    m_settings.pass_through,
				    m_settings.zone_callstacks,
				    m_settings.callstack_depth);
			}
			catch (const std::exception& error)
			{
				m_log->error("config.toml reload failed: {}", error.what());
			}
			catch (...)
			{
				m_log->error("config.toml reload failed");
			}
		});

		m_log->info("MicroProfile capture installed ({} scope shims, {} scope owners)",
		    m_callstacks->shim_count(),
		    m_symbols->scope_owner_count());
		m_log->info("Tracy started on demand (sampling {} Hz)", m_settings.sampling_hz);

		m_gpu_zones = std::make_unique<tracy_profiler::GpuZones>(*m_timers, *m_callstacks);
		tracy_profiler::attach_gpu_zones(m_gpu_zones.get());
		m_gpu_zones->set_enabled(m_settings.gpu_zones);
		if (m_gpu_zones->running())
			m_log->info("GPU timeline started ({})", rml::platform::gpu_timeline_api());
		else if (m_settings.gpu_zones)
			m_log->warn("GPU timeline unsupported on this platform; GPU zones stay off");
		else
			m_log->info("GPU zones disabled in config.toml");
	}

	void stop()
	{
		if (m_store)
			m_store->stop_watching();

		if (m_installed)
		{
			if (m_frame_images)
			{
				m_frame_images->shutdown();
				rml::render::graph().remove_pass(tracy_profiler::FrameImages::pass_name);
				m_frame_images = nullptr;
				rml::render::enable_output_readback(false);
			}
			if (m_memory)
				m_memory->stop();
			tracy_profiler::attach_gpu_zones(nullptr);
			tracy_profiler::remove_capture();
			if (m_gpu_zones)
				m_gpu_zones->stop();
			m_installed = false;
		}

		if (m_started)
		{
			___tracy_set_sample_hook(nullptr, nullptr);
			___tracy_shutdown_profiler();
			___tracy_set_symbol_resolver(nullptr, nullptr);
			m_started = false;
		}

		m_symbols.reset();
		m_memory.reset();
		m_gpu_zones.reset();
		m_callstacks.reset();
		m_timers.reset();
	}

	std::shared_ptr<spdlog::logger> m_log;
	std::unique_ptr<rml::config::ModSettings> m_store;
	std::mutex m_mutex;
	tracy_profiler::Settings m_settings;
	std::unique_ptr<tracy_profiler::Timers> m_timers;
	std::unique_ptr<tracy_profiler::Symbols> m_symbols;
	std::unique_ptr<tracy_profiler::Callstacks> m_callstacks;
	std::unique_ptr<tracy_profiler::GpuZones> m_gpu_zones;
	std::unique_ptr<tracy_profiler::Memory> m_memory;
	tracy_profiler::FrameImages* m_frame_images{};
	bool m_installed{};
	bool m_started{};
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new TracyProfiler();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()
