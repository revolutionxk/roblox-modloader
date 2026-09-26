#pragma once

#include "../isubsystem.hpp"
#include "pointers.hpp"

namespace rml
{
	class EngineSignaturesSubsystem final : public ISubsystem
	{
	public:
		std::expected<void, SubsystemError> initialize() override
		{
			if (!g_pointers)
				return std::unexpected(SubsystemError{std::string(name()), "Pointers are not initialized"});

			g_pointers->resolve_engine();
			return {};
		}

		void shutdown() override
		{
		}

		[[nodiscard]] std::string_view name() const noexcept override
		{
			return "EngineSignatures";
		}
	};
}
