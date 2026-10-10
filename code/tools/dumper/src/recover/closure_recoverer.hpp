#pragma once

#include "rml/dumper/recover/recoverer.hpp"

#include <set>

namespace rml::dumper::recover
{
	class ClosureRecoverer final : public Recoverer
	{
	public:
		[[nodiscard]] std::string_view struct_name() const override { return "Closure"; }
		[[nodiscard]] std::span<const std::string_view> depends_on() const override { return m_dependencies; }

		[[nodiscard]] std::expected<schema::StructLayout, Error> recover(
		    const RecoveryContext& context) const override;

		[[nodiscard]] static const disasm::MemoryAccess* write_from(const disasm::Trace& trace,
		                                                            disasm::Object object, disasm::Register value,
		                                                            std::uint8_t width);
		[[nodiscard]] static std::optional<std::int64_t> differing_constant(const disasm::Trace& first,
		                                                                    const disasm::Trace& second,
		                                                                    disasm::Object first_object,
		                                                                    disasm::Object second_object);
		[[nodiscard]] static std::optional<std::int64_t> matching_constant(const disasm::Trace& first,
		                                                                   const disasm::Trace& second,
		                                                                   disasm::Object first_object,
		                                                                   disasm::Object second_object,
		                                                                   std::int64_t below, std::int64_t skip);
		[[nodiscard]] static std::optional<std::int64_t> taken_from(const disasm::Trace& trace, disasm::Object source,
		                                                            disasm::Object destination, std::uint8_t width);

		[[nodiscard]] static std::set<disasm::Object> closures_in(const disasm::Trace& trace, std::size_t is_c);

		static constexpr std::int64_t collectable_header_size = 3;

	private:
		static constexpr std::array<std::string_view, 2> m_dependencies{"CommonHeader", "lua_Debug"};

		[[nodiscard]] static disasm::Object allocated_object(const disasm::Trace& trace);

		static void recover_debugname(const RecoveryContext& context, schema::StructLayout& layout);
	};
}
