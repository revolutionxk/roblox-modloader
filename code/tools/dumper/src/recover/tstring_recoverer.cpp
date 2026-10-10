#include "recover/tstring_recoverer.hpp"

#include "recover/closure_recoverer.hpp"
#include "recover/proto_recoverer.hpp"

#include <algorithm>
#include <format>
#include <set>

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

		std::set<disasm::Object> strings;
		for (const auto& read : (*trace)->accesses)
			if (!read.is_write && read.width == 8 && protos.contains(read.object) &&
			    static_cast<std::size_t>(read.displacement) == source->offset)
				if (const auto loaded = query.loaded_by(read); loaded >= disasm::first_derived_object)
					strings.insert(loaded);

		const disasm::MemoryAccess* data = nullptr;
		for (const auto& store : (*trace)->accesses)
		{
			if (!store.is_write || store.object != record || store.width != 8 ||
			    static_cast<std::size_t>(store.displacement) != slot->offset)
				continue;

			const auto* load = query.preceding(store);
			if (load != nullptr && !load->is_write && load->width == 8 && load->displacement >= 0 &&
			    load->value_register == store.value_register && strings.contains(load->object))
			{
				data = load;
				break;
			}
		}

		if (data == nullptr)
		{
			context.report().record_failure("TString", "data", data_probe);
			context.report().record_failure("TString", "len", len_probe);
			return layout;
		}

		const auto length = std::ranges::find_if((*trace)->accesses, [data](const disasm::MemoryAccess& read) {
			return !read.is_write && read.object == data->object && read.width == 4 && read.displacement >= 0 &&
			       read.index == disasm::Register::none && read.displacement != data->displacement;
		});

		if (length == (*trace)->accesses.end() || length->displacement >= data->displacement)
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

		context.report().record_recovered("TString", "data", data_probe);
		layout.add({.name = "data",
		            .type = "char",
		            .size = 1,
		            .offset = static_cast<std::size_t>(data->displacement),
		            .provenance = schema::Provenance::recovered("lua_getinfo", data_probe),
		            .count = 1});

		if (const auto* name = debug->find("name"), *debugname = proto->find("debugname");
		    name != nullptr && debugname != nullptr)
		{
			for (const auto& store : (*trace)->accesses)
			{
				if (!store.is_write || store.object != record ||
				    static_cast<std::size_t>(store.displacement) != name->offset)
					continue;

				const auto* load = query.preceding(store);
				if (load == nullptr || load->is_write || !protos.contains(load->object) ||
				    static_cast<std::size_t>(load->displacement) != debugname->offset)
					continue;

				const auto header = query.step_between(store.value_register, load->sequence, store.sequence);
				if (header && *header != data->displacement)
					context.report().record_failure(
					    "TString", "data",
					    std::format("lua_getinfo steps the proto's debug name 0x{:X} bytes in before ar->name, not "
					                "0x{:X}",
					                *header, data->displacement));
			}
		}

		layout.size = layout.fields.back().end();

		return layout;
	}
}
