#pragma once

#include <cstdlib>
#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/roblox/reflection/event_descriptor.hpp"
#include "RobloxModLoader/roblox/signals.hpp"
#include "dotnet_variant.hpp"
#include "interop_registry.hpp"
#include "spdlog/spdlog.h"
#include "type_marshaler.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace rml::dotnet
{
	class ManagedEventSlot final : public RBX::Reflection::GenericSlotWrapper
	{
	public:
		ManagedEventSlot(const ManagedEventCallback callback, void* state) noexcept :
		    m_callback(callback),
		    m_state(state)
		{
		}

		void deliver(const RBX::EventArguments& args) override
		{
			if (!m_callback)
				return;

			try
			{
				std::vector<char*> owned_strings;
				std::vector<InteropVariant> interop_args;
				interop_args.reserve(args.size());

				for (const auto& arg : args)
				{
					if (!arg.is_void() && arg.type().type_id == RBX::Reflection::TypeId::Tuple)
					{
						if (const auto* tuple = arg.try_cast<std::shared_ptr<const RBX::Reflection::Tuple>>()->get())
						{
							for (const auto& value : tuple->values)
								interop_args.push_back(TypeMarshaler::encode_variant(value, &owned_strings));
						}
						continue;
					}

					interop_args.push_back(TypeMarshaler::encode_variant(arg, &owned_strings));
				}

				m_callback(m_state, interop_args.data(), static_cast<uint32_t>(interop_args.size()));

				for (auto* str : owned_strings)
				{
					free(str);
				}
			}
			catch (const std::exception& ex)
			{
				RML_ERROR_AT("ManagedEventSlot", "Exception while dispatching event: {}", ex.what());
			}
			catch (...)
			{
				RML_ERROR_AT("ManagedEventSlot", "Unknown exception while dispatching event");
			}
		}

	private:
		ManagedEventCallback m_callback;
		void* m_state;
	};

	struct ManagedEventConnection
	{
		std::shared_ptr<RBX::Reflection::GenericSlotWrapper> slot;
		rbx::signals::connection connection;
	};
}
