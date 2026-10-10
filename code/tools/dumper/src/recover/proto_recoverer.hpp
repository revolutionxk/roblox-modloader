#pragma once

#include "rml/dumper/recover/recoverer.hpp"

#include <set>

namespace rml::dumper::recover
{
	class ProtoRecoverer final : public Recoverer
	{
	public:
		[[nodiscard]] std::string_view struct_name() const override { return "Proto"; }
		[[nodiscard]] std::span<const std::string_view> depends_on() const override { return m_dependencies; }

		[[nodiscard]] std::expected<schema::StructLayout, Error> recover(
		    const RecoveryContext& context) const override;

		struct FreedArray
		{
			std::int64_t pointer{};
			std::int64_t size{};
			std::int64_t element{};
		};

		[[nodiscard]] static std::vector<FreedArray> freed_arrays(const disasm::Trace& trace, Rva free_function,
		                                                          disasm::Object proto);

		static void recover_userdata(const RecoveryContext& context, schema::StructLayout& layout);

		[[nodiscard]] static std::set<disasm::Object> protos_in(const disasm::Trace& trace,
		                                                        const std::set<disasm::Object>& closures,
		                                                        std::size_t p);

	private:
		static constexpr std::array<std::string_view, 3> m_dependencies{"CommonHeader", "Closure", "lua_Debug"};

		static void recover_line(const RecoveryContext& context, schema::StructLayout& layout);
		static void recover_debug(const RecoveryContext& context, schema::StructLayout& layout);
	};
}
