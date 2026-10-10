#pragma once

#include "rml/dumper/emit/emitter.hpp"

namespace rml::dumper::emit
{
	class MirrorEmitter final : public TextEmitter
	{
	public:
		[[nodiscard]] std::string_view id() const override { return "mirror"; }
		[[nodiscard]] std::string_view default_filename() const override { return "luau_layout.hpp"; }
		[[nodiscard]] std::expected<void, Error> emit(const schema::LayoutSet& layouts,
		                                              std::ostream& out) const override;

		struct Slot
		{
			std::string name;
			std::string type;
			std::size_t offset{};
			std::size_t size{};
			bool reserved{};
			std::size_t count{};
		};

		[[nodiscard]] static std::vector<Slot> pack(const schema::StructLayout& layout,
		                                            const schema::LayoutSet& layouts);
		[[nodiscard]] static std::string mirror_name(std::string_view struct_name);
	};
}
