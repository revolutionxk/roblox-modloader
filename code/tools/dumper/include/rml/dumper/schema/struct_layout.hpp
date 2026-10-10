#pragma once

#include "rml/dumper/core/error.hpp"
#include "rml/dumper/schema/field.hpp"

#include <expected>
#include <string_view>
#include <vector>

namespace rml::dumper::schema
{
	struct StructLayout
	{
		std::string name;
		std::size_t size{};
		std::vector<Field> fields;

		void add(Field field);

		[[nodiscard]] const Field* find(std::string_view field_name) const;
		[[nodiscard]] bool covers(std::size_t offset) const;
		[[nodiscard]] bool overlaps(std::size_t offset, std::size_t size) const;
		[[nodiscard]] std::size_t recovered_count() const;
	};

	[[nodiscard]] std::expected<void, Error> validate(const StructLayout& layout);
}
