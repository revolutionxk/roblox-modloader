#include "recover/proto_recoverer.hpp"

#include "recover/closure_recoverer.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <map>

namespace rml::dumper::recover
{
	std::vector<ProtoRecoverer::FreedArray> ProtoRecoverer::freed_arrays(const disasm::Trace& trace,
	                                                                     const Rva free_function,
	                                                                     const disasm::Object proto)
	{
		std::vector<FreedArray> arrays;
		std::size_t previous = 0;

		for (const auto& call : trace.calls)
		{
			if (call.target != free_function)
				continue;

			FreedArray array{.pointer = -1, .size = -1, .element = 1};

			for (const auto& access : trace.accesses)
			{
				if (access.is_write || access.object != proto || access.sequence >= call.sequence ||
				    access.sequence < previous || access.displacement < 0)
					continue;

				if (access.width == 8)
					array.pointer = access.displacement;
				else if (access.width == 4)
					array.size = access.displacement;
			}

			for (const auto& constant : trace.constants)
				if (constant.kind == disasm::ConstantKind::scale && constant.sequence < call.sequence &&
				    constant.sequence >= previous)
					array.element *= constant.value;

			previous = call.sequence;

			if (array.pointer >= 0 && array.size >= 0)
				arrays.push_back(array);
		}

		return arrays;
	}

	std::expected<schema::StructLayout, Error> ProtoRecoverer::recover(const RecoveryContext& context) const
	{
		const auto trace = context.trace(target::Anchor::luaF_freeproto);
		if (!trace)
			return std::unexpected(trace.error());

		const auto free_function = context.anchors().at(target::Anchor::luaM_free);

		disasm::Object proto = disasm::no_object;
		std::size_t most = 0;
		std::map<disasm::Object, std::size_t> reads;

		for (const auto& access : (*trace)->accesses)
			if (!access.is_write && access.object != disasm::no_object && access.displacement >= 0)
				if (const auto count = ++reads[access.object]; count > most)
				{
					most = count;
					proto = access.object;
				}

		const auto arrays = freed_arrays(**trace, free_function, proto);

		struct FreedField
		{
			std::string_view pointer;
			std::string_view type;
			std::string_view size;
		};

		static constexpr std::array<FreedField, 8> freed_in_order{{
		    {"code", "Instruction*", "sizecode"},
		    {"p", "Proto**", "sizep"},
		    {"k", "TValue*", "sizek"},
		    {"lineinfo", "uint8_t*", "sizelineinfo"},
		    {"locvars", "LocVar*", "sizelocvars"},
		    {"upvalues", "TString**", "sizeupvalues"},
		    {"debuginsn", "uint8_t*", "sizecode"},
		    {"typeinfo", "uint8_t*", "sizetypeinfo"},
		}};

		schema::StructLayout layout{.name = "Proto"};

		if (arrays.size() < freed_in_order.size())
		{
			context.report().record_failure(
			    "Proto", "code",
			    std::format("luaF_freeproto releases {} arrays through luaM_free, expected at least {}",
			                arrays.size(), freed_in_order.size()));
			return layout;
		}

		if (arrays.size() > freed_in_order.size())
			context.report().record_note(
			    "Proto", std::format("luaF_freeproto releases {} arrays, the {} beyond the ones luau declares stay "
			                         "unnamed",
			                         arrays.size(), arrays.size() - freed_in_order.size()));

		for (std::size_t i = 0; i < freed_in_order.size(); ++i)
		{
			const auto& freed = freed_in_order[i];
			const auto probe = std::format("array {} released by luaF_freeproto", i);

			context.report().record_recovered("Proto", std::string(freed.pointer), probe);
			layout.add({.name = std::string(freed.pointer),
			            .type = std::string(freed.type),
			            .size = 8,
			            .offset = static_cast<std::size_t>(arrays[i].pointer),
			            .provenance = schema::Provenance::recovered("luaF_freeproto", probe)});

			auto size_name = std::string(freed.size);
			auto size_probe = probe;

			if (const auto* already = layout.find(size_name); already != nullptr)
			{
				if (already->offset == static_cast<std::size_t>(arrays[i].size))
					continue;

				size_name = std::format("size{}", freed.pointer);
				size_probe = std::format(
				    "array {} counts through its own field, not the {} at 0x{:X} that was expected", i, freed.size,
				    already->offset);
			}

			context.report().record_recovered("Proto", size_name, size_probe);
			layout.add({.name = size_name,
			            .type = "int",
			            .size = 4,
			            .offset = static_cast<std::size_t>(arrays[i].size),
			            .provenance = schema::Provenance::recovered("luaF_freeproto", std::move(size_probe))});
		}

		static constexpr std::array<std::pair<std::string_view, std::int64_t>, 3> element_sizes{
		    {{"code", 4}, {"p", 8}, {"k", 16}}};

		for (const auto& [name, expected] : element_sizes)
		{
			const auto* field = layout.find(name);
			if (field == nullptr)
				continue;

			const auto freed = std::ranges::find(arrays, static_cast<std::int64_t>(field->offset),
			                                     &FreedArray::pointer);
			if (freed == arrays.end() || freed->element == expected)
				continue;

			context.report().record_failure(
			    "Proto", std::string(name),
			    std::format("luaF_freeproto walks it in steps of {} bytes, not the {} an element takes",
			                freed->element, expected));
		}

		recover_userdata(context, layout);
		recover_line(context, layout);
		recover_debug(context, layout);

		layout.size = layout.fields.empty() ? 0 : layout.fields.back().end();

		return layout;
	}

	std::set<disasm::Object> ProtoRecoverer::protos_in(const disasm::Trace& trace,
	                                                   const std::set<disasm::Object>& closures, const std::size_t p)
	{
		const disasm::TraceQuery query(trace);
		std::set<disasm::Object> protos;

		for (const auto& read : trace.accesses)
		{
			if (read.is_write || read.width != 8 || read.index != disasm::Register::none ||
			    static_cast<std::size_t>(read.displacement) != p || !closures.contains(read.object))
				continue;

			if (const auto loaded = query.loaded_by(read); loaded >= disasm::first_derived_object)
				protos.insert(loaded);
		}

		return protos;
	}

	void ProtoRecoverer::recover_line(const RecoveryContext& context, schema::StructLayout& layout)
	{
		const auto array_probe = "the qword pusherror reads off the proto whose lineinfo it indexes, and indexes in "
		                         "four byte steps";
		const auto shift_probe = "the field narrower than a pointer pusherror reads off the proto whose lineinfo it "
		                         "indexes that loadsafe also stores into the proto it fills lineinfo into, the shift "
		                         "that picks the absolute line";

		const auto trace = context.trace(target::Anchor::pusherror);
		const auto* lineinfo = layout.find("lineinfo");

		if (!trace || lineinfo == nullptr)
		{
			context.report().record_failure("Proto", "abslineinfo", array_probe);
			context.report().record_failure("Proto", "linegaplog2", shift_probe);
			return;
		}

		const disasm::TraceQuery query(**trace);

		const auto indexed = [&](const disasm::MemoryAccess& read, const std::uint8_t width) {
			const auto loaded = query.loaded_by(read);
			return std::ranges::any_of((*trace)->accesses, [&](const disasm::MemoryAccess& use) {
				return !use.is_write && use.object == loaded && use.index != disasm::Register::none &&
				       use.scale == width && use.width == width;
			});
		};

		const auto lines = std::ranges::find_if((*trace)->accesses, [&](const disasm::MemoryAccess& read) {
			return !read.is_write && read.width == 8 && read.index == disasm::Register::none &&
			       static_cast<std::size_t>(read.displacement) == lineinfo->offset && indexed(read, 1);
		});

		const auto proto = lines != (*trace)->accesses.end() ? lines->object : disasm::no_object;

		const disasm::MemoryAccess* array = nullptr;
		std::map<std::int64_t, const disasm::MemoryAccess*> narrow;

		for (const auto& read : (*trace)->accesses)
		{
			if (read.is_write || read.object != proto || read.displacement < 0 ||
			    read.index != disasm::Register::none)
				continue;

			if (read.width != 8)
			{
				narrow.try_emplace(read.displacement, &read);
				continue;
			}

			if (array == nullptr && static_cast<std::size_t>(read.displacement) != lineinfo->offset &&
			    indexed(read, 4))
				array = &read;
		}

		if (array == nullptr || layout.overlaps(static_cast<std::size_t>(array->displacement), 8))
		{
			context.report().record_failure("Proto", "abslineinfo", array_probe);
		}
		else
		{
			context.report().record_recovered("Proto", "abslineinfo", array_probe);
			layout.add({.name = "abslineinfo",
			            .type = "int*",
			            .size = 8,
			            .offset = static_cast<std::size_t>(array->displacement),
			            .provenance = schema::Provenance::recovered("pusherror", array_probe)});
		}

		const auto* shift = stored_by_loader(context, layout, narrow);
		const auto* type = shift == nullptr ? nullptr
		                   : shift->width == 1 ? "uint8_t"
		                   : shift->width == 4 ? "int"
		                                       : nullptr;

		if (type == nullptr || layout.overlaps(static_cast<std::size_t>(shift->displacement), shift->width))
		{
			context.report().record_failure("Proto", "linegaplog2", shift_probe);
			return;
		}

		const auto detail = std::format("{}, stored {} byte wide", shift_probe, shift->width);

		context.report().record_recovered("Proto", "linegaplog2", detail);
		layout.add({.name = "linegaplog2",
		            .type = type,
		            .size = shift->width,
		            .offset = static_cast<std::size_t>(shift->displacement),
		            .provenance = schema::Provenance::recovered("loadsafe", detail)});
	}

	const disasm::MemoryAccess* ProtoRecoverer::stored_by_loader(
	    const RecoveryContext& context, const schema::StructLayout& layout,
	    const std::map<std::int64_t, const disasm::MemoryAccess*>& candidates)
	{
		const auto trace = context.trace(target::Anchor::loadsafe);
		const auto* lineinfo = layout.find("lineinfo");
		if (!trace || lineinfo == nullptr)
			return nullptr;

		std::set<disasm::Object> filled;
		for (const auto& write : (*trace)->accesses)
			if (write.is_write && write.width == 8 && write.index == disasm::Register::none &&
			    static_cast<std::size_t>(write.displacement) == lineinfo->offset)
				filled.insert(write.object);

		std::map<std::int64_t, const disasm::MemoryAccess*> stored;
		for (const auto& write : (*trace)->accesses)
			if (write.is_write && filled.contains(write.object) && write.index == disasm::Register::none &&
			    write.width < 8 && candidates.contains(write.displacement))
				stored.try_emplace(write.displacement, &write);

		return stored.size() == 1 ? stored.begin()->second : nullptr;
	}

	void ProtoRecoverer::recover_debug(const RecoveryContext& context, schema::StructLayout& layout)
	{
		struct Handed
		{
			std::string_view field;
			std::string_view type;
			std::uint8_t width;
			std::string_view slot;
			bool past_header;
			std::string_view probe;
		};

		static constexpr std::array<Handed, 3> handed{{
		    {"source", "TString*", 8, "source", true,
		     "the qword lua_getinfo reads off the closure's proto and hands, past the string header, to ar->source"},
		    {"debugname", "TString*", 8, "name", true,
		     "the qword lua_getinfo reads off the closure's proto and hands, past the string header, to ar->name"},
		    {"linedefined", "int", 4, "linedefined", false,
		     "the dword lua_getinfo copies from the closure's proto into ar->linedefined"},
		}};

		const auto trace = context.trace(target::Anchor::lua_getinfo);
		const auto* debug = context.layout("lua_Debug");
		const auto* closure = context.layout("Closure");
		const auto* is_c = closure != nullptr ? closure->find("isC") : nullptr;
		const auto* p = closure != nullptr ? closure->find("p") : nullptr;

		if (!trace || debug == nullptr || is_c == nullptr || p == nullptr)
		{
			for (const auto& entry : handed)
				context.report().record_failure("Proto", std::string(entry.field), std::string(entry.probe));
			return;
		}

		const disasm::TraceQuery query(**trace);
		const auto record = query.dominant_object();
		const auto protos = protos_in(**trace, ClosureRecoverer::closures_in(**trace, is_c->offset), p->offset);

		const auto handed_from = [&](const Handed& entry, const schema::Field& slot) -> const disasm::MemoryAccess* {
			for (const auto& store : (*trace)->accesses)
			{
				if (!store.is_write || store.object != record || store.width != slot.size ||
				    static_cast<std::size_t>(store.displacement) != slot.offset)
					continue;

				for (const auto& load : (*trace)->accesses)
				{
					if (load.is_write || load.width != entry.width || load.displacement < 0 ||
					    !protos.contains(load.object))
						continue;

					if (const auto step = query.stored_step(load, store); step && (*step != 0) == entry.past_header)
						return &load;
				}
			}

			return nullptr;
		};

		for (const auto& entry : handed)
		{
			const auto* slot = debug->find(entry.slot);
			const auto* found = slot != nullptr ? handed_from(entry, *slot) : nullptr;

			if (found == nullptr || layout.overlaps(static_cast<std::size_t>(found->displacement), entry.width))
			{
				context.report().record_failure("Proto", std::string(entry.field), std::string(entry.probe));
				continue;
			}

			context.report().record_recovered("Proto", std::string(entry.field), std::string(entry.probe));
			layout.add({.name = std::string(entry.field),
			            .type = std::string(entry.type),
			            .size = entry.width,
			            .offset = static_cast<std::size_t>(found->displacement),
			            .provenance = schema::Provenance::recovered("lua_getinfo", std::string(entry.probe))});
		}
	}

	void ProtoRecoverer::recover_userdata(const RecoveryContext& context, schema::StructLayout& layout)
	{
		if (!context.anchors().has(target::Anchor::rbx_derive_thread_capabilities))
		{
			context.report().record_failure("Proto", "userdata",
			                                 "rbx_derive_thread_capabilities is not resolved on this build");
			return;
		}

		const auto* closure = context.layout("Closure");
		const auto* proto_pointer = closure == nullptr ? nullptr : closure->find("p");
		if (proto_pointer == nullptr)
		{
			context.report().record_failure("Proto", "userdata",
			                                 "cannot follow the mask pointer without Closure.p recovered");
			return;
		}

		const auto trace = context.trace(target::Anchor::rbx_derive_thread_capabilities);
		if (!trace)
		{
			context.report().record_failure("Proto", "userdata", trace.error().message());
			return;
		}

		const disasm::MemoryAccess* found = nullptr;
		for (const auto& load : (*trace)->accesses)
		{
			if (load.is_write || load.width != 8 || load.value_register == disasm::Register::none ||
			    load.displacement != static_cast<std::int64_t>(proto_pointer->offset))
				continue;

			for (const auto& access : (*trace)->accesses)
			{
				if (access.is_write || access.width != 8 || access.displacement <= 0 ||
				    access.sequence <= load.sequence || access.base != load.value_register)
					continue;

				if (found != nullptr && found->displacement != access.displacement)
				{
					context.report().record_failure(
					    "Proto", "userdata",
					    "rbx_derive_thread_capabilities reads more than one qword off the proto");
					return;
				}

				found = &access;
			}
		}

		if (found == nullptr)
		{
			context.report().record_failure("Proto", "userdata",
			                                 "rbx_derive_thread_capabilities reads no qword off the proto");
			return;
		}

		const auto probe = "the qword rbx_derive_thread_capabilities reads off the proto to gate capabilities";

		context.report().record_recovered("Proto", "userdata", probe);
		layout.add({.name = "userdata",
		            .type = "void*",
		            .size = 8,
		            .offset = static_cast<std::size_t>(found->displacement),
		            .provenance = schema::Provenance::recovered("rbx_derive_thread_capabilities", probe)});
	}
}
