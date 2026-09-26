#pragma once
// clang-format off
#include "RobloxModLoader/memory/batch.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/memory/signature_cache.hpp"
#include "RobloxModLoader/util/compile_time.hpp"
#include "RobloxModLoader/internal/roblox_pointers.hpp"
// clang-format on

#include <span>

namespace rml
{
	class Pointers
	{
	public:
		static constexpr auto get_early_batch();
		static constexpr auto get_roblox_batch();

	private:
		template<utils::fixed_string batch_name, size_t N>
		void run_batch(const memory::batch<N>& batch, const std::uint32_t sigset_hash, const memory::module& mem_region)
		{
			const std::span<const memory::signature> entries{batch.m_entries.data(), batch.m_entries.size()};
			memory::run_batch_cached(entries, mem_region, sigset_hash, batch_name.view());
		}

	public:
		explicit Pointers();

		~Pointers();

		void resolve_engine();

	public:
		void* m_main_window{};

	public:
		RobloxPointers m_roblox_pointers{};
	};
}

inline rml::Pointers* g_pointers{};
