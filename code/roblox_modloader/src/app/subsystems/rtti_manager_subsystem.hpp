#pragma once

#include "../isubsystem.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "filesystem/directory.hpp"

namespace rml
{
	class RttiManagerSubsystem final : public ISubsystem
	{
	public:
		std::expected<void, SubsystemError> initialize() override
		{
			try
			{
				m_index = std::make_unique<memory::RttiIndex>(memory::create_rtti_backend(), filesystem::directory::get_mod_loader_directory() / "cache" / "rtti.bin");
			}
			catch (const std::exception& e)
			{
				return std::unexpected(SubsystemError{std::string(name()), e.what()});
			}

			memory::install_rtti(m_index.get());
			return {};
		}

		void shutdown() override
		{
			memory::install_rtti(nullptr);
			m_index.reset();
		}

		[[nodiscard]] std::string_view name() const noexcept override
		{
			return "RttiManager";
		}

	private:
		std::unique_ptr<memory::RttiIndex> m_index;
	};
}
