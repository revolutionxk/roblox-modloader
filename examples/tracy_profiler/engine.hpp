#pragma once

#include <RobloxModLoader/roblox/profiler/micro_profile.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace tracy_profiler
{
	struct Engine
	{
		MicroProfile* state{};
		std::uint64_t (*enter)(MicroProfileTimerToken token, std::uint64_t tick){};
		void (*leave)(MicroProfileTimerToken token, std::uint64_t enter_tick, std::uint64_t leave_tick){};
		void (*flip_cpu)(){};
		void (*put_label)(MicroProfileLabelToken label, const char* text){};
		void (*label_literal)(MicroProfileLabelToken label, const char* text){};
		void (*on_thread_create)(const char* name, RBX::ThreadBufferSizeType buffer_size){};
		MicroProfileTimerToken (*get_token)(const char* group, const char* name, int color, std::uint8_t flags){};

		[[nodiscard]] std::vector<std::string> missing_required() const;
		[[nodiscard]] std::vector<std::string> missing_optional() const;
	};

	[[nodiscard]] Engine resolve_engine();
}
