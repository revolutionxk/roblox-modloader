#include "recover/closure_recoverer.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <set>

namespace rml::dumper::recover
{
	disasm::Object ClosureRecoverer::allocated_object(const disasm::Trace& trace)
	{
		std::map<disasm::Object, std::size_t> writes;

		for (const auto& access : trace.accesses)
			if (access.is_write && access.object != disasm::no_object &&
			    access.index == disasm::Register::none && access.displacement >= 0 &&
			    access.displacement < 0x40)
				++writes[access.object];

		disasm::Object best = disasm::no_object;
		std::size_t most = 0;

		for (const auto& [object, count] : writes)
			if (count > most)
			{
				most = count;
				best = object;
			}

		return best;
	}

	const disasm::MemoryAccess* ClosureRecoverer::write_from(const disasm::Trace& trace, const disasm::Object object,
	                                                         const disasm::Register value, const std::uint8_t width)
	{
		const auto found = std::ranges::find_if(trace.accesses, [&](const disasm::MemoryAccess& access) {
			return access.is_write && access.object == object && access.value_object == disasm::entry_object(value) &&
			       access.width == width && access.displacement >= 0;
		});

		return found != trace.accesses.end() ? &*found : nullptr;
	}

	static std::map<std::int64_t, std::uint8_t> constant_bytes(const disasm::Trace& trace,
	                                                           const disasm::Object object)
	{
		std::map<std::int64_t, std::uint8_t> bytes;

		for (const auto& access : trace.accesses)
		{
			if (!access.is_write || access.object != object || !access.immediate || access.displacement < 0)
				continue;
			if (access.width == 0 || access.width > 8)
				continue;

			for (std::uint8_t i = 0; i < access.width; ++i)
				bytes[access.displacement + i] = static_cast<std::uint8_t>(*access.immediate >> i * 8 & 0xFF);
		}

		return bytes;
	}

	std::optional<std::int64_t> ClosureRecoverer::differing_constant(const disasm::Trace& first,
	                                                                 const disasm::Trace& second,
	                                                                 const disasm::Object first_object,
	                                                                 const disasm::Object second_object)
	{
		const auto left = constant_bytes(first, first_object);
		const auto right = constant_bytes(second, second_object);

		for (const auto& [offset, value] : left)
		{
			const auto twin = right.find(offset);
			if (twin != right.end() && twin->second != value)
				return offset;
		}

		return std::nullopt;
	}

	std::optional<std::int64_t> ClosureRecoverer::matching_constant(const disasm::Trace& first,
	                                                                const disasm::Trace& second,
	                                                                const disasm::Object first_object,
	                                                                const disasm::Object second_object,
	                                                                const std::int64_t below,
	                                                                const std::int64_t skip)
	{
		const auto left = constant_bytes(first, first_object);
		const auto right = constant_bytes(second, second_object);

		for (const auto& [offset, value] : left)
		{
			if (offset < below || offset == skip)
				continue;

			if (const auto twin = right.find(offset); twin != right.end() && twin->second == value)
				return offset;
		}

		return std::nullopt;
	}

	std::optional<std::int64_t> ClosureRecoverer::taken_from(const disasm::Trace& trace, const disasm::Object source,
	                                                         const disasm::Object destination,
	                                                         const std::uint8_t width)
	{
		for (const auto& read : trace.accesses)
		{
			if (read.is_write || read.object != source || read.width != width || read.displacement < 0)
				continue;

			for (const auto& write : trace.accesses)
			{
				if (!write.is_write || write.object != destination || write.width != width)
					continue;
				if (write.value_register != read.value_register || write.sequence <= read.sequence)
					continue;

				return write.displacement;
			}
		}

		return std::nullopt;
	}

	std::expected<schema::StructLayout, Error> ClosureRecoverer::recover(const RecoveryContext& context) const
	{
		const auto lua_trace = context.trace(target::Anchor::luaF_newLclosure);
		const auto c_trace = context.trace(target::Anchor::luaF_newCclosure);
		if (!lua_trace)
			return std::unexpected(lua_trace.error());
		if (!c_trace)
			return std::unexpected(c_trace.error());

		const auto lua_object = allocated_object(**lua_trace);
		const auto c_object = allocated_object(**c_trace);

		schema::StructLayout layout{.name = "Closure"};

		const auto record = [&](std::string name, std::string type, const std::size_t size,
		                        const std::optional<std::int64_t> offset, std::string description) {
			if (!offset || *offset < 0)
			{
				context.report().record_failure("Closure", name, std::move(description));
				return;
			}

			context.report().record_recovered("Closure", name, description);
			layout.add({.name = std::move(name),
			            .type = std::move(type),
			            .size = size,
			            .offset = static_cast<std::size_t>(*offset),
			            .provenance = schema::Provenance::recovered("luaF_newLclosure", std::move(description))});
		};

		const auto offset_of = [](const disasm::MemoryAccess* access) -> std::optional<std::int64_t> {
			return access != nullptr ? std::optional{access->displacement} : std::nullopt;
		};

		const auto tag = differing_constant(**lua_trace, **c_trace, lua_object, c_object);

		record("isC", "uint8_t", 1, tag,
		       "byte constant that differs between the lua and the c closure constructor");

		record("preload", "uint8_t", 1,
		       matching_constant(**lua_trace, **c_trace, lua_object, c_object, collectable_header_size,
		                         tag.value_or(-1)),
		       "the other byte constant both closure constructors agree on");

		record("stacksize", "uint8_t", 1,
		       taken_from(**lua_trace, disasm::entry_object(context.abi().argument(3)), lua_object, 1),
		       "byte the lua closure takes from the proto it is built for");

		record("nupvalues", "uint8_t", 1,
		       offset_of(write_from(**lua_trace, lua_object, context.abi().argument(1), 1)),
		       "byte taken from the element count argument");

		record("env", "LuaTable*", 8,
		       offset_of(write_from(**lua_trace, lua_object, context.abi().argument(2), 8)),
		       "qword taken from the environment argument");

		record("p", "Proto*", 8,
		       offset_of(write_from(**lua_trace, lua_object, context.abi().argument(3), 8)),
		       "qword taken from the proto argument");

		if (const auto* proto = layout.find("p"); proto != nullptr)
		{
			const auto probe = "the value array of a lua closure starts right after its proto";

			context.report().record_recovered("Closure", "uprefs", probe);
			layout.add({.name = "uprefs",
			            .type = "TValue*",
			            .size = 8,
			            .offset = proto->end(),
			            .provenance = schema::Provenance::recovered("luaF_newLclosure", probe)});
		}

		std::vector<std::int64_t> c_slots;
		for (const auto& access : (*c_trace)->accesses)
		{
			if (!access.is_write || access.object != c_object || access.width != 8 || access.displacement < 0)
				continue;
			if (std::ranges::find(c_slots, access.displacement) == c_slots.end())
				c_slots.push_back(access.displacement);
		}

		std::ranges::sort(c_slots);

		if (const auto* env = layout.find("env"); env != nullptr && c_slots.size() >= 2)
		{
			const auto after_env = std::ranges::find_if(c_slots, [env](const std::int64_t offset) {
				return static_cast<std::size_t>(offset) > env->offset;
			});

			if (after_env != c_slots.end())
			{
				const auto probe = std::format(
				    "the value array of a c closure starts after the {} slots it initialises from 0x{:X}",
				    std::distance(after_env, c_slots.end()), *after_env);

				context.report().record_recovered("Closure", "upvals", probe);
				layout.add({.name = "upvals",
				            .type = "TValue*",
				            .size = 8,
				            .offset = static_cast<std::size_t>(c_slots.back()) + 8,
				            .provenance = schema::Provenance::recovered("luaF_newCclosure", probe)});
			}
		}

		recover_debugname(context, layout);

		layout.size = layout.fields.empty() ? 0 : layout.fields.back().end();

		return layout;
	}

	std::set<disasm::Object> ClosureRecoverer::closures_in(const disasm::Trace& trace, const std::size_t is_c)
	{
		std::set<disasm::Object> closures;

		for (const auto& access : trace.accesses)
			if (!access.is_write && access.width == 1 && access.object != disasm::no_object &&
			    access.index == disasm::Register::none && static_cast<std::size_t>(access.displacement) == is_c)
				closures.insert(access.object);

		return closures;
	}

	void ClosureRecoverer::recover_debugname(const RecoveryContext& context, schema::StructLayout& layout)
	{
		const auto probe = "the qword lua_getinfo reads off a c closure and hands to ar->name";

		const auto trace = context.trace(target::Anchor::lua_getinfo);
		const auto* debug = context.layout("lua_Debug");
		const auto* name = debug != nullptr ? debug->find("name") : nullptr;
		const auto* is_c = layout.find("isC");
		const auto* proto = layout.find("p");

		if (!trace || name == nullptr || is_c == nullptr || proto == nullptr)
		{
			context.report().record_failure("Closure", "debugname", probe);
			return;
		}

		const disasm::TraceQuery query(**trace);
		const auto record = query.dominant_object();
		const auto closures = closures_in(**trace, is_c->offset);

		for (const auto& store : (*trace)->accesses)
		{
			if (!store.is_write || store.object != record || store.width != 8 ||
			    static_cast<std::size_t>(store.displacement) != name->offset)
				continue;

			for (const auto& load : (*trace)->accesses)
			{
				if (load.is_write || load.width != 8 || load.displacement < 0 || !closures.contains(load.object))
					continue;

				const auto offset = static_cast<std::size_t>(load.displacement);
				if (offset == proto->offset || layout.overlaps(offset, 8))
					continue;

				const auto header = query.stored_step(load, store);
				if (!header)
					continue;

				const auto detail = *header != 0
				                        ? std::format("{}, past a 0x{:X} byte string header", probe, *header)
				                        : std::format("{}, as it is", probe);

				context.report().record_recovered("Closure", "debugname", detail);
				layout.add({.name = "debugname",
				            .type = *header != 0 ? "TString*" : "const char*",
				            .size = 8,
				            .offset = offset,
				            .provenance = schema::Provenance::recovered("lua_getinfo", detail)});
				return;
			}
		}

		context.report().record_failure("Closure", "debugname", probe);
	}
}
