#pragma once

#include "RobloxModLoader/internal/platform.hpp"

#ifndef NOMINMAX
	#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#if defined(RML_WINDOWS)
	#include <Windows.h>
#endif

#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/util/hash.hpp"
#include "pattern.hpp"
#include "range.hpp"
#include "signature.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <vector>

namespace rml::memory
{
	template<size_t N>
	struct batch
	{
		std::array<signature, N> m_entries;

		constexpr batch(std::array<signature, N> entries)
		{
			m_entries = entries;
		}
	};

	template<size_t N>
	struct batch_and_hash
	{
		batch<N> m_batch;
		uint32_t m_hash;
	};

	struct signature_hasher
	{
		template<signature sig>
		static inline constexpr uint32_t compute_hash(uint32_t hash)
		{
			hash = utils::fnv1a_32(sig.m_ida.view(), hash);
			hash = utils::fnv1a_32(sig.m_anchor.m_text.view(), hash);
			hash = utils::fnv1a_32(sig.m_anchor.m_origin.view(), hash);
			for (std::uint8_t i = 0; i < sig.m_anchor.m_step_count; ++i)
				hash = (hash ^ ((static_cast<uint32_t>(sig.m_anchor.m_steps[i]) << 8) | sig.m_anchor.m_indices[i])) * utils::fnv32_prime;

			return hash;
		}

		template<signature... sigs>
		static inline constexpr uint32_t add(uint32_t hash = utils::fnv32_offset)
		{
			((hash = compute_hash<sigs>(hash)), ...);

			return hash;
		}
	};

	template<signature... args>
	static inline constexpr auto make_batch(uint32_t hash = utils::fnv32_offset)
	{
		constexpr std::array<signature, sizeof...(args)> a1 = {args...};

		constexpr memory::batch<a1.size()> h(a1);

		return batch_and_hash<a1.size()>{h, signature_hasher::add<args...>()};
	}

	struct batch_runner
	{
		inline static std::mutex s_entry_mutex;
		inline static std::vector<std::future<bool>> g_futures;

		template<size_t N>
		inline static bool run(const memory::batch<N> batch, range region)
		{
			for (auto& entry : batch.m_entries)
			{
				g_futures.emplace_back(std::async(&scan_pattern_and_execute_callback, region, entry));
			}

			bool found_all_patterns = true;
			for (auto& future : g_futures)
			{
				future.wait();

				if (!future.get())
					found_all_patterns = false;
			}

			g_futures.clear();

			return found_all_patterns;
		}

		inline static bool scan_pattern_and_execute_callback(range region, signature entry)
		{
			if (auto result = region.scan(entry.m_ida.c_str()); result.has_value())
			{
				if (entry.m_on_signature_found)
				{
					std::lock_guard<std::mutex> lock(s_entry_mutex); // Acquire a lock on the mutex to synchronize access.

					std::invoke(std::move(entry.m_on_signature_found), result.value());

					LOG_INFO("Found '{}' RobloxStudioBeta.exe+0x{:X}",
					    entry.m_name.c_str(),
					    result.value().as<std::uintptr_t>() - region.begin().as<std::uintptr_t>());

					return true;
				}
			}

			LOG_INFO("Failed to find '{}'.", entry.m_name.c_str());

			return false;
		}
	};
}
