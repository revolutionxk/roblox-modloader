#include "rml/dumper/schema/struct_layout.hpp"

#include "rml/dumper/schema/layout_set.hpp"

#include <algorithm>

namespace rml::dumper::schema
{
	void StructLayout::add(Field field)
	{
		const auto position = std::ranges::upper_bound(fields, field.offset, {}, &Field::offset);
		fields.insert(position, std::move(field));
	}

	const Field* StructLayout::find(const std::string_view field_name) const
	{
		const auto found = std::ranges::find(fields, field_name, &Field::name);
		return found != fields.end() ? &*found : nullptr;
	}

	bool StructLayout::covers(const std::size_t offset) const
	{
		return std::ranges::any_of(fields, [offset](const Field& field) {
			return offset >= field.offset && offset < field.end();
		});
	}

	bool StructLayout::overlaps(const std::size_t offset, const std::size_t size) const
	{
		return std::ranges::any_of(fields, [offset, size](const Field& field) {
			return offset < field.end() && field.offset < offset + size;
		});
	}

	std::size_t StructLayout::recovered_count() const
	{
		return static_cast<std::size_t>(
		    std::ranges::count_if(fields, [](const Field& field) { return field.provenance.is_recovered(); }));
	}

	std::expected<void, Error> validate(const StructLayout& layout)
	{
		for (const auto& field : layout.fields)
		{
			if (field.size == 0)
				return std::unexpected(
				    Error::make(ErrorCode::validation, "{}.{} has no size", layout.name, field.name));

			if (field.end() > layout.size)
				return std::unexpected(Error::make(ErrorCode::validation,
				                                   "{}.{} ends at 0x{:X} past the 0x{:X} byte struct", layout.name,
				                                   field.name, field.end(), layout.size));

			if (field.element_size() > 1 && field.offset % field.element_size() != 0)
				return std::unexpected(Error::make(ErrorCode::validation,
				                                   "{}.{} at 0x{:X} breaks {} byte alignment", layout.name,
				                                   field.name, field.offset, field.element_size()));
		}

		for (std::size_t i = 1; i < layout.fields.size(); ++i)
		{
			const auto& previous = layout.fields[i - 1];
			const auto& current = layout.fields[i];

			if (current.offset < previous.end())
				return std::unexpected(Error::make(ErrorCode::validation,
				                                   "{}.{} at 0x{:X} overlaps {}.{} at 0x{:X}", layout.name,
				                                   current.name, current.offset, layout.name, previous.name,
				                                   previous.offset));
		}

		return {};
	}

	const StructLayout* LayoutSet::find(const std::string_view name) const
	{
		const auto found = structs.find(std::string(name));
		return found != structs.end() ? &found->second : nullptr;
	}
}
