#pragma once

#include "rml/dumper/recover/recoverer.hpp"

namespace rml::dumper::recover
{
	class CallInfoRecoverer final : public Recoverer
	{
	public:
		[[nodiscard]] std::string_view struct_name() const override { return "CallInfo"; }
		[[nodiscard]] std::span<const std::string_view> depends_on() const override { return m_dependencies; }

		[[nodiscard]] std::expected<schema::StructLayout, Error> recover(
		    const RecoveryContext& context) const override;

		[[nodiscard]] static std::vector<std::int64_t> relocated_slots(const disasm::Trace& trace,
		                                                               disasm::Object state);
		[[nodiscard]] static const disasm::MemoryAccess* taken_from_argument(const disasm::Trace& trace,
		                                                                     disasm::Register argument);
		[[nodiscard]] static std::optional<std::int64_t> handed_to_state(const disasm::Trace& trace,
		                                                                 disasm::Object frame, disasm::Object state,
		                                                                 std::size_t slot);
		[[nodiscard]] static std::optional<std::int64_t> stride(const disasm::Trace& trace, disasm::Register frame,
		                                                        std::size_t before, std::int64_t smallest);

	private:
		static constexpr std::size_t relocated_count = 3;
		static constexpr std::array<std::string_view, 3> m_dependencies{"lua_State", "Closure", "Proto"};

		static void recover_proto_slots(const RecoveryContext& context, const disasm::Trace& trace,
		                                disasm::Object frame, schema::StructLayout& layout);
	};
}
