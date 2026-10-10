#pragma once

#include "rml/dumper/disasm/trace.hpp"

namespace rml::dumper::disasm
{
	class TraceQuery
	{
	public:
		explicit TraceQuery(const Trace& trace) :
		    m_trace(trace)
		{
		}

		TraceQuery(Trace&&) = delete;

		[[nodiscard]] const Trace& trace() const { return m_trace; }

		[[nodiscard]] const MemoryAccess* nth_write(Object object, std::size_t n, std::uint8_t width = 0,
		                                            std::optional<std::size_t> after = std::nullopt,
		                                            Register value = Register::none) const;
		[[nodiscard]] const MemoryAccess* nth_distinct_write(Object object, std::size_t n, std::uint8_t width = 0,
		                                                     std::optional<std::size_t> after = std::nullopt) const;
		[[nodiscard]] const MemoryAccess* nth_read(Object object, std::size_t n, std::uint8_t width = 0,
		                                           std::optional<std::size_t> after = std::nullopt,
		                                           Register value = Register::none) const;
		[[nodiscard]] const MemoryAccess* first_immediate_write(Object object, std::uint64_t value,
		                                                        std::optional<std::size_t> after = std::nullopt) const;
		[[nodiscard]] const MemoryAccess* first_indexed(Object object, bool is_write,
		                                                std::optional<std::size_t> after = std::nullopt) const;
		[[nodiscard]] Object dominant_object() const;
		[[nodiscard]] std::optional<std::size_t> call_sequence(Rva target, std::size_t skip = 0) const;
		[[nodiscard]] std::optional<std::size_t> first_call_sequence() const;
		[[nodiscard]] Object loaded_by(const MemoryAccess& read) const;
		[[nodiscard]] std::optional<std::int64_t> stored_step(const MemoryAccess& load,
		                                                      const MemoryAccess& store) const;

	private:
		[[nodiscard]] const MemoryAccess* nth(bool is_write, Object object, std::size_t n, std::uint8_t width,
		                                      std::optional<std::size_t> after, Register value) const;

		const Trace& m_trace;
	};
}
