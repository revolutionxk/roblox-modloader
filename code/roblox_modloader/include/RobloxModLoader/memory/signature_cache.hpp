#pragma once

#include "range.hpp"
#include "signature.hpp"

#include <cstdint>
#include <span>
#include <string_view>

namespace rml::memory
{
	bool run_batch_cached(std::span<const signature> entries, range region, std::uint32_t sigset_hash, std::string_view cache_name);
}
