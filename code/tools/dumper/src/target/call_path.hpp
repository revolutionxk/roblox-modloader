#pragma once

#include "rml/dumper/core/types.hpp"
#include "rml/dumper/image/image.hpp"
#include "rml/dumper/target/anchor.hpp"

#include <expected>
#include <span>
#include <string>
#include <vector>

namespace rml::dumper::target
{
	[[nodiscard]] std::vector<Rva> calls_from(const image::Image& image, const index::FunctionBounds& function);
	[[nodiscard]] std::expected<Rva, std::string> follow_path(const image::Image& image, Rva start, std::span<const PathStep> path);
}
