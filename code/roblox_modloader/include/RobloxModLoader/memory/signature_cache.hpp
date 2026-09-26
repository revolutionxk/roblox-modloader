#pragma once

#include "range.hpp"
#include "signature.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace rml::memory
{
	bool run_batch_cached(std::span<const signature> entries, range region, std::uint32_t sigset_hash);

	std::optional<handle> find_cached_signature(const signature& entry, range region, std::uint32_t sigset_hash);

	std::optional<handle> scan_signature(const signature& entry, range region);
}
