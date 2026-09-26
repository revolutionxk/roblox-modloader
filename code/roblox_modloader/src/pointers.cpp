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

		const auto [batch, hash] = get_early_batch();
		run_batch<"early">(batch, hash, memory::module(platform::studio_image_name()));
	}

	void Pointers::resolve_engine()
	{
		const auto [batch, hash] = get_roblox_batch();
		run_batch<"roblox">(batch, hash, memory::module(platform::studio_image_name()));

		m_main_window = platform::acquire_main_window();
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
