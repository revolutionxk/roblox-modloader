#pragma once

#include "RobloxModLoader/memory/batch.hpp"
#include "RobloxModLoader/memory/module.hpp"

#include <cstdint>

namespace internal_developer
{
	struct EnginePointers
	{
		void* is_internal{nullptr};
		bool* channel_flag{nullptr};
		bool* internal_flag{nullptr};

		[[nodiscard]] bool complete() const noexcept
		{
			return is_internal != nullptr && channel_flag != nullptr && internal_flag != nullptr &&
			       channel_flag != internal_flag;
		}
	};

	[[nodiscard]] EnginePointers& engine_pointers() noexcept;

	[[nodiscard]] bool resolve_engine_pointers();
}
