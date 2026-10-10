#include "rml/dumper/disasm/trace_query.hpp"

#include <algorithm>
#include <map>
#include <vector>

namespace rml::dumper::disasm
{
	const MemoryAccess* TraceQuery::nth(const bool is_write, const Object object, const std::size_t n,
	                                    const std::uint8_t width, const std::optional<std::size_t> after,
	                                    const Register value) const
	{
		std::size_t seen = 0;

		for (const auto& access : m_trace.accesses)
		{
			if (access.is_write != is_write || access.object != object)
				continue;
			if (width != 0 && access.width != width)
				continue;
			if (value != Register::none && access.value_register != value)
				continue;
			if (after && access.sequence <= *after)
				continue;

			if (seen++ == n)
				return &access;
		}

		return nullptr;
	}

	const MemoryAccess* TraceQuery::nth_write(const Object object, const std::size_t n, const std::uint8_t width,
	                                          const std::optional<std::size_t> after, const Register value) const
	{
		return nth(true, object, n, width, after, value);
	}

	const MemoryAccess* TraceQuery::nth_distinct_write(const Object object, const std::size_t n,
	                                                   const std::uint8_t width,
	                                                   const std::optional<std::size_t> after) const
	{
		std::vector<std::int64_t> seen;

		for (const auto& access : m_trace.accesses)
		{
			if (!access.is_write || access.object != object)
				continue;
			if (width != 0 && access.width != width)
				continue;
			if (after && access.sequence <= *after)
				continue;
			if (std::ranges::find(seen, access.displacement) != seen.end())
				continue;

			if (seen.size() == n)
				return &access;

			seen.push_back(access.displacement);
		}

		return nullptr;
	}

	const MemoryAccess* TraceQuery::nth_read(const Object object, const std::size_t n, const std::uint8_t width,
	                                         const std::optional<std::size_t> after, const Register value) const
	{
		return nth(false, object, n, width, after, value);
	}

	const MemoryAccess* TraceQuery::first_immediate_write(const Object object, const std::uint64_t value,
	                                                      const std::optional<std::size_t> after) const
	{
		for (const auto& access : m_trace.accesses)
		{
			if (!access.is_write || access.object != object || access.immediate != value)
				continue;
			if (after && access.sequence <= *after)
				continue;

			return &access;
		}

		return nullptr;
	}

	const MemoryAccess* TraceQuery::first_indexed(const Object object, const bool is_write,
	                                              const std::optional<std::size_t> after) const
	{
		for (const auto& access : m_trace.accesses)
		{
			if (access.is_write != is_write || access.object != object || access.index == Register::none)
				continue;
			if (after && access.sequence <= *after)
				continue;

			return &access;
		}

		return nullptr;
	}

	Object TraceQuery::dominant_object() const
	{
		std::map<Object, std::size_t> counts;

		for (const auto& access : m_trace.accesses)
			if (access.is_write && access.object != no_object)
				++counts[access.object];

		std::size_t best = 0;
		Object winner = no_object;

		for (const auto& [object, count] : counts)
			if (count > best)
			{
				best = count;
				winner = object;
			}

		return winner;
	}

	std::optional<std::size_t> TraceQuery::call_sequence(const Rva target, const std::size_t skip) const
	{
		std::size_t seen = 0;

		for (const auto& call : m_trace.calls)
		{
			if (call.target != target)
				continue;

			if (seen++ == skip)
				return call.sequence;
		}

		return std::nullopt;
	}

	std::optional<std::size_t> TraceQuery::first_call_sequence() const
	{
		if (m_trace.calls.empty())
			return std::nullopt;

		return m_trace.calls.front().sequence;
	}

	Object TraceQuery::loaded_by(const MemoryAccess& read) const
	{
		if (read.loaded != no_object)
			return read.loaded;

		for (const auto& access : m_trace.accesses)
			if (access.sequence > read.sequence && access.base == read.value_register)
				return access.object;

		return no_object;
	}

	std::optional<std::int64_t> TraceQuery::stored_step(const MemoryAccess& load, const MemoryAccess& store) const
	{
		const auto loaded = loaded_by(load);
		if (loaded == no_object || !store.is_write || store.value_object == no_object)
			return std::nullopt;

		if (store.value_object == loaded)
			return 0;

		for (const auto& address : m_trace.accesses)
			if (!address.is_write && address.object == loaded && address.index == Register::none &&
			    address.loaded == store.value_object)
				return address.displacement;

		for (const auto& constant : m_trace.constants)
			if (constant.kind == ConstantKind::step && constant.operand == loaded &&
			    constant.result == store.value_object)
				return constant.value;

		return std::nullopt;
	}
}
