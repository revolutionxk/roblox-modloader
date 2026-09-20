#include "internal_pointers.hpp"

#include "RobloxModLoader/internal/platform.hpp"
#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/util/compile_time_helpers.hpp"

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
			    "E8 ? ? ? ? 48 8D 15 ? ? ? ? 48 8D 0D ? ? ? ? 84 C0 48 0F 45 CA 48 8D 05 ? ? ? ? 48 89 44 24 50 48 C7 44 24 58 11 00 00 00 48 8D 44 24 38 48 89 44 24 40",
			    [](const rml::memory::handle ptr) {
				    const auto is_internal = ptr.add(1).rip();
				    const auto* const bytes = is_internal.as<const std::uint8_t*>();

				    const bool valid_shape =
				        bytes[0] == 0x80 && bytes[1] == 0x3D && bytes[6] == 0x00 &&
				        bytes[7] == 0x74 && bytes[8] == 0x0C &&
				        bytes[9] == 0x80 && bytes[10] == 0x3D && bytes[15] == 0x00 &&
				        bytes[16] == 0x74 && bytes[17] == 0x03 &&
				        bytes[18] == 0xB0 && bytes[19] == 0x01 && bytes[20] == 0xC3 &&
				        bytes[21] == 0x32 && bytes[22] == 0xC0 && bytes[23] == 0xC3;

				    if (!valid_shape)
					    return;

				    auto* const channel_flag = is_internal.add(2).rip().add(1).as<bool*>();
				    auto* const internal_flag = is_internal.add(11).rip().add(1).as<bool*>();
				    if (channel_flag == internal_flag)
					    return;

				    g_engine_pointers.is_internal = is_internal.as<void*>();
				    g_engine_pointers.channel_flag = channel_flag;
				    g_engine_pointers.internal_flag = internal_flag;
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
