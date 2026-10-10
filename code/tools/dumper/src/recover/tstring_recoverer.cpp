#include "recover/tstring_recoverer.hpp"

#include "recover/closure_recoverer.hpp"
#include "recover/proto_recoverer.hpp"

#include <algorithm>
#include <format>
#include <utility>

namespace rml::dumper::recover
{
	std::expected<schema::StructLayout, Error> TStringRecoverer::recover(const RecoveryContext& context) const
	{
		const auto trace = context.trace(target::Anchor::lua_getinfo);
		if (!trace)
			return std::unexpected(trace.error());

		schema::StructLayout layout{.name = "TString"};

		const auto data_probe = "how far into the proto's source string lua_getinfo points ar->source";
		const auto len_probe = "the dword lua_getinfo reads off that source string as the length to shorten";

		const auto* debug = context.layout("lua_Debug");
		const auto* closure = context.layout("Closure");
		const auto* proto = context.layout("Proto");
		const auto* slot = debug != nullptr ? debug->find("source") : nullptr;
		const auto* is_c = closure != nullptr ? closure->find("isC") : nullptr;
		const auto* p = closure != nullptr ? closure->find("p") : nullptr;
		const auto* source = proto != nullptr ? proto->find("source") : nullptr;

		if (slot == nullptr || is_c == nullptr || p == nullptr || source == nullptr)
		{
			context.report().record_failure("TString", "data", data_probe);
			context.report().record_failure("TString", "len", len_probe);
			return layout;
		}

		const disasm::TraceQuery query(**trace);
		const auto record = query.dominant_object();
		const auto protos =
		    ProtoRecoverer::protos_in(**trace, ClosureRecoverer::closures_in(**trace, is_c->offset), p->offset);

		const auto stepped = [&](const schema::Field& slot, const schema::Field& field)
		    -> std::pair<const disasm::MemoryAccess*, std::int64_t> {
			for (const auto& store : (*trace)->accesses)
			{
				if (!store.is_write || store.object != record || store.width != 8 ||
				    static_cast<std::size_t>(store.displacement) != slot.offset)
					continue;

				for (const auto& load : (*trace)->accesses)
				{
					if (load.is_write || load.width != 8 || !protos.contains(load.object) ||
					    static_cast<std::size_t>(load.displacement) != field.offset)
						continue;

					if (const auto step = query.stored_step(load, store); step && *step > 0)
						return {&load, *step};
				}
			}

			return {nullptr, 0};
		};

		const auto [pointer, header] = stepped(*slot, *source);

		if (pointer == nullptr)
		{
			context.report().record_failure("TString", "data", data_probe);
			context.report().record_failure("TString", "len", len_probe);
			return layout;
		}

		const auto string = query.loaded_by(*pointer);
		const auto length = std::ranges::find_if((*trace)->accesses, [&](const disasm::MemoryAccess& read) {
			return !read.is_write && read.object == string && read.width == 4 && read.displacement >= 0 &&
			       read.index == disasm::Register::none && read.displacement < header;
		});

		if (length == (*trace)->accesses.end())
		{
			context.report().record_failure("TString", "len", len_probe);
		}
		else
		{
			context.report().record_recovered("TString", "len", len_probe);
			layout.add({.name = "len",
			            .type = "unsigned",
			            .size = 4,
			            .offset = static_cast<std::size_t>(length->displacement),
			            .provenance = schema::Provenance::recovered("lua_getinfo", len_probe)});
		}

		const auto* name = debug->find("name");
		const auto* debugname = proto->find("debugname");

		if (name != nullptr && debugname != nullptr)
		{
			if (const auto [named, step] = stepped(*name, *debugname); named != nullptr && step != header)
			{
				context.report().record_failure(
				    "TString", "data",
				    std::format("lua_getinfo steps the proto's debug name 0x{:X} bytes in before ar->name, not the "
				                "0x{:X} it steps the source",
				                step, header));
				layout.size = layout.fields.empty() ? 0 : layout.fields.back().end();
				return layout;
			}
		}

		context.report().record_recovered("TString", "data", data_probe);
		layout.add({.name = "data",
		            .type = "char",
		            .size = 1,
		            .offset = static_cast<std::size_t>(header),
		            .provenance = schema::Provenance::recovered("lua_getinfo", data_probe),
		            .count = 1});

		layout.size = layout.fields.back().end();

		return layout;
	}
}
