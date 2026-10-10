#pragma once

#include "rml/dumper/recover/recoverer.hpp"

namespace rml::dumper::recover
{
	class TStringRecoverer final : public Recoverer
	{
	public:
		[[nodiscard]] std::string_view struct_name() const override { return "TString"; }
		[[nodiscard]] std::span<const std::string_view> depends_on() const override { return m_dependencies; }

		[[nodiscard]] std::expected<schema::StructLayout, Error> recover(
		    const RecoveryContext& context) const override;

	private:
		static constexpr std::array<std::string_view, 3> m_dependencies{"Closure", "Proto", "lua_Debug"};
	};
}
