#include "pointers.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/all.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "memory/engine_signatures.hpp"

namespace rml
{
	Pointers::Pointers()
	{
		g_pointers = this;

		const auto roblox_region = memory::module(platform::studio_image_name());
		const auto [m_roblox_batch, m_hash] = get_roblox_batch();

		constexpr cstxpr_str roblox_batch_name{"roblox"};

		run_batch<roblox_batch_name>(m_roblox_batch, m_hash, roblox_region);
	}

	void* Pointers::main_window() const
	{
		return platform::acquire_main_window();
	}

	Pointers::~Pointers()
	{
		g_pointers = nullptr;
	}
}

RobloxPointers* get_roblox_pointers()
{
	if (!g_pointers)
	{
		throw std::runtime_error("Pointers not initialized");
	}

	return &g_pointers->m_roblox_pointers;
}
