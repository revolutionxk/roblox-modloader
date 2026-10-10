#pragma once

#include "rml/dumper/recover/recoverer.hpp"

namespace rml::dumper::recover
{
	class GlobalStateRecoverer final : public Recoverer
	{
	public:
		[[nodiscard]] std::string_view struct_name() const override { return "global_State"; }
		[[nodiscard]] std::span<const std::string_view> depends_on() const override { return m_dependencies; }

		[[nodiscard]] std::expected<schema::StructLayout, Error> recover(
		    const RecoveryContext& context) const override;

	private:
		static constexpr std::array<std::string_view, 2> m_dependencies{"lua_State", "CommonHeader"};

		[[nodiscard]] static disasm::Object global_of(const RecoveryContext& context, const disasm::Trace& trace);
		[[nodiscard]] static const disasm::MemoryAccess* called_hook(const disasm::Trace& trace,
		                                                             disasm::Object global,
		                                                             std::optional<std::size_t> after);

		static void take(const RecoveryContext& context, schema::StructLayout& layout,
		                 const disasm::MemoryAccess* access, schema::Field field);

		static void recover_currentwhite(const RecoveryContext& context, const disasm::Trace& trace,
		                                 schema::StructLayout& layout);
		static void recover_accounting(const RecoveryContext& context, schema::StructLayout& layout);
		static void recover_allocation_hook(const RecoveryContext& context, schema::StructLayout& layout);
	};
}
