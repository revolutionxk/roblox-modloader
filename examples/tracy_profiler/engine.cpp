#include "engine.hpp"

#include "signatures.hpp"

#include <RobloxModLoader/memory/batch.hpp>
#include <RobloxModLoader/memory/module.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>

namespace tracy_profiler
{
	static Engine g_engine{};

	static void found_enter(const rml::memory::handle ptr)
	{
		g_engine.enter = ptr.as<decltype(g_engine.enter)>();
	}

	static void found_state(const rml::memory::handle ptr)
	{
		g_engine.state = signatures::state_reference(ptr).as<MicroProfile*>();
	}

	static void found_leave(const rml::memory::handle ptr)
	{
		g_engine.leave = ptr.as<decltype(g_engine.leave)>();
	}

	static void found_flip_cpu(const rml::memory::handle ptr)
	{
		g_engine.flip_cpu = ptr.as<decltype(g_engine.flip_cpu)>();
	}

	static void found_put_label(const rml::memory::handle ptr)
	{
		g_engine.put_label = ptr.as<decltype(g_engine.put_label)>();
	}

	static void found_label_literal(const rml::memory::handle ptr)
	{
		g_engine.label_literal = ptr.as<decltype(g_engine.label_literal)>();
	}

	static void found_on_thread_create(const rml::memory::handle ptr)
	{
		g_engine.on_thread_create = ptr.as<decltype(g_engine.on_thread_create)>();
	}

	static void found_get_token(const rml::memory::handle ptr)
	{
		g_engine.get_token = ptr.as<decltype(g_engine.get_token)>();
	}

	static constexpr rml::memory::signature enter_signature{"MICROPROFILE_ENTER", signatures::enter, &found_enter};
	static constexpr rml::memory::signature state_signature{"MICROPROFILE_STATE", signatures::state, &found_state};
	static constexpr rml::memory::signature leave_signature{"MICROPROFILE_LEAVE", signatures::leave, &found_leave};
	static constexpr rml::memory::signature flip_cpu_signature{"MICROPROFILE_FLIP_CPU", signatures::flip_cpu, &found_flip_cpu};
	static constexpr rml::memory::signature put_label_signature{"MICROPROFILE_PUT_LABEL", signatures::put_label, &found_put_label};
	static constexpr rml::memory::signature label_literal_signature{"MICROPROFILE_LABEL_LITERAL", signatures::label_literal, &found_label_literal};
	static constexpr rml::memory::signature on_thread_create_signature{"MICROPROFILE_ON_THREAD_CREATE", signatures::on_thread_create, &found_on_thread_create};
	static constexpr rml::memory::signature get_token_signature{"PROFILER_GET_TOKEN", signatures::get_token, &found_get_token};

	static constexpr auto engine_batch()
	{
		return rml::memory::make_batch<enter_signature, state_signature, leave_signature, flip_cpu_signature, put_label_signature, label_literal_signature, on_thread_create_signature, get_token_signature>();
	}

	std::vector<std::string> Engine::missing_required() const
	{
		std::vector<std::string> missing;
		if (!enter)
			missing.emplace_back("MICROPROFILE_ENTER");
		if (!state)
			missing.emplace_back("MICROPROFILE_STATE");
		if (!leave)
			missing.emplace_back("MICROPROFILE_LEAVE");
		return missing;
	}

	std::vector<std::string> Engine::missing_optional() const
	{
		std::vector<std::string> missing;
		if (!flip_cpu)
			missing.emplace_back("MICROPROFILE_FLIP_CPU");
		if (!put_label)
			missing.emplace_back("MICROPROFILE_PUT_LABEL");
		if (!label_literal)
			missing.emplace_back("MICROPROFILE_LABEL_LITERAL");
		if (!on_thread_create)
			missing.emplace_back("MICROPROFILE_ON_THREAD_CREATE");
		if (!get_token)
			missing.emplace_back("PROFILER_GET_TOKEN");
		return missing;
	}

	Engine resolve_engine()
	{
		g_engine = {};
		const rml::memory::module studio{rml::platform::studio_image_name()};
		const auto batch = engine_batch().m_batch;
		rml::memory::batch_runner::run(batch, studio);
		return g_engine;
	}
}
