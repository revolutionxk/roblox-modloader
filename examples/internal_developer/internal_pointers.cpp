#include "internal_pointers.hpp"

#include "RobloxModLoader/internal/platform.hpp"
#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/util/compile_time.hpp"

#include <iterator>

RML_LOG_SCOPE("InternalDeveloper")

namespace internal_developer
{
	static EnginePointers g_engine_pointers{};

	EnginePointers& engine_pointers() noexcept
	{
		return g_engine_pointers;
	}

	static constexpr auto engine_batch()
	{
		// clang-format off
#if defined(RML_WINDOWS)
		return rml::memory::make_batch<
			{
			    "IS_INTERNAL",
			    "E8 ? ? ? ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? 84 C0 48 0F 45 CA 48 8D 05 ? ? ? ? 48 89 44 24 ? 48 C7 44 24 ? 11 00 00 00",
			    [](const rml::memory::handle ptr) {
				    const auto is_internal = ptr.add(1).rip();
				    g_engine_pointers.is_internal = is_internal.as<void*>();

				    bool** flags[] = {&g_engine_pointers.channel_flag, &g_engine_pointers.internal_flag};
				    std::size_t found = 0;

				    for (int offset = 0; offset < 0x40 && found < std::size(flags); ++offset)
				    {
					    const auto instruction = is_internal.add(offset);
					    const auto* const bytes = instruction.as<std::uint8_t*>();

					    if (bytes[0] == 0x80 && bytes[1] == 0x3D && bytes[6] == 0x00)
					    {
						    *flags[found++] = instruction.add(2).rip().add(1).as<bool*>();
						    offset += 6;
					    }
				    }

				    if (found == 1)
					    g_engine_pointers.internal_flag = g_engine_pointers.channel_flag;
			    },
			}
		>();
#else
		return rml::memory::make_batch<
			{
			    "IS_INTERNAL",
			    "F4 03 00 AA ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 9F 02 00 71 0A 5C 80 39 0B 30 40 A9 28 11 88 9A 5F 01 00 F1 69 B1 80 9A",
			    [](const rml::memory::handle ptr) {
				    const auto is_internal = ptr.sub(4).bl();

				    g_engine_pointers.is_internal = is_internal.as<void*>();
				    g_engine_pointers.channel_flag = is_internal.adrp().as<bool*>();
				    g_engine_pointers.internal_flag = is_internal.add(8).adrp().as<bool*>();
			    },
			}
		>();
#endif
		// clang-format on
	}

	bool resolve_engine_pointers()
	{
		const rml::memory::module studio{rml::platform::studio_image_name()};

		const auto [batch, hash] = engine_batch();

		if (!rml::memory::batch_runner::run(batch, studio))
		{
			RML_ERROR("Signature scan failed; Roblox Studio was probably updated");
			return false;
		}

		if (!g_engine_pointers.complete())
		{
			RML_ERROR("Signature matched but the engine flags were not resolved");
			return false;
		}

		return true;
	}
}
