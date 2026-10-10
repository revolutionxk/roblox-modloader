#include "recover/global_state_recoverer.hpp"

#include <algorithm>

namespace rml::dumper::recover
{
	disasm::Object GlobalStateRecoverer::global_of(const RecoveryContext& context, const disasm::Trace& trace)
	{
		const auto* state = context.layout("lua_State");
		const auto* global = state != nullptr ? state->find("global") : nullptr;
		if (global == nullptr)
			return disasm::no_object;

		const auto reached = std::ranges::find_if(trace.accesses, [&](const disasm::MemoryAccess& access) {
			return !access.is_write && access.object == disasm::entry_object(context.abi().argument(0)) &&
			       access.width == 8 && static_cast<std::size_t>(access.displacement) == global->offset;
		});

		return reached != trace.accesses.end() ? disasm::TraceQuery(trace).loaded_by(*reached) : disasm::no_object;
	}

	const disasm::MemoryAccess* GlobalStateRecoverer::called_hook(const disasm::Trace& trace,
	                                                              const disasm::Object global,
	                                                              const disasm::Register state)
	{
		for (const auto& load : trace.accesses)
		{
			if (load.is_write || load.object != global || load.width != 8 || load.displacement < 0 ||
			    load.index != disasm::Register::none || load.loaded == disasm::no_object)
				continue;

			const auto called = std::ranges::any_of(trace.calls, [&](const disasm::CallSite& call) {
				return call.callee == load.loaded && call.object_of(state) == disasm::entry_object(state);
			});

			if (called)
				return &load;
		}

		return nullptr;
	}

	void GlobalStateRecoverer::take(const RecoveryContext& context, schema::StructLayout& layout,
	                                const disasm::MemoryAccess* access, schema::Field field)
	{
		const auto probe = field.provenance.as_recovered()->probe;

		if (access == nullptr || access->displacement < 0 ||
		    layout.overlaps(static_cast<std::size_t>(access->displacement), field.size))
		{
			context.report().record_failure("global_State", field.name, probe);
			return;
		}

		field.offset = static_cast<std::size_t>(access->displacement);
		context.report().record_recovered("global_State", field.name, probe);
		layout.add(std::move(field));
	}

	void GlobalStateRecoverer::recover_currentwhite(const RecoveryContext& context, const disasm::Trace& trace,
	                                                schema::StructLayout& layout)
	{
		const auto* header = context.layout("CommonHeader");
		const auto* marked = header != nullptr ? header->find("marked") : nullptr;

		const auto probe = "the byte the collector paints a freshly built object with";

		const auto owner = global_of(context, trace);

		if (owner == disasm::no_object || marked == nullptr)
		{
			context.report().record_failure("global_State", "currentwhite", probe);
			return;
		}

		std::optional<std::int64_t> currentwhite;

		for (const auto& read : trace.accesses)
		{
			if (read.is_write || read.object != owner || read.width != 1 || read.displacement < 0)
				continue;

			for (const auto& write : trace.accesses)
			{
				if (!write.is_write || write.width != 1 || write.object == owner)
					continue;
				if (static_cast<std::size_t>(write.displacement) != marked->offset ||
				    write.value_register != read.value_register || write.sequence <= read.sequence)
					continue;

				currentwhite = read.displacement;
				break;
			}

			if (currentwhite)
				break;
		}

		if (!currentwhite)
		{
			context.report().record_failure("global_State", "currentwhite", probe);
			return;
		}

		context.report().record_recovered("global_State", "currentwhite", probe);
		layout.add({.name = "currentwhite",
		            .type = "uint8_t",
		            .size = 1,
		            .offset = static_cast<std::size_t>(*currentwhite),
		            .provenance = schema::Provenance::recovered("luaE_newthread", probe)});
	}

	void GlobalStateRecoverer::recover_accounting(const RecoveryContext& context, schema::StructLayout& layout)
	{
		const auto total_probe = "the qword luaM_free takes the freed size off";
		const auto categories_probe = "the qword array luaM_free indexes by memory category and takes the freed "
		                              "size off, one slot for every value the memcat byte holds";
		const auto hook_probe = "the function pointer luaM_free loads off the global state and calls with the state "
		                        "as its first argument";

		const auto trace = context.trace(target::Anchor::luaM_free);
		const auto global = trace ? global_of(context, **trace) : disasm::no_object;
		const auto freed = disasm::entry_object(context.abi().argument(2));

		const auto taken_off = [&](const bool indexed) -> const disasm::MemoryAccess* {
			if (global == disasm::no_object)
				return nullptr;

			const auto found = std::ranges::find_if((*trace)->accesses, [&](const disasm::MemoryAccess& access) {
				return access.is_write && access.object == global && access.width == 8 &&
				       access.value_object == freed && access.displacement >= 0 &&
				       (access.index != disasm::Register::none) == indexed;
			});

			return found != (*trace)->accesses.end() ? &*found : nullptr;
		};

		take(context, layout, taken_off(false),
		     {.name = "totalbytes",
		      .type = "size_t",
		      .size = 8,
		      .provenance = schema::Provenance::recovered("luaM_free", total_probe)});

		const auto* header = context.layout("CommonHeader");
		const auto* memcat = header != nullptr ? header->find("memcat") : nullptr;
		const auto count = memcat != nullptr ? std::size_t{1} << memcat->size * 8 : 0;
		const auto* categories = taken_off(true);

		take(context, layout, categories != nullptr && categories->scale == 8 && count != 0 ? categories : nullptr,
		     {.name = "memcatbytes",
		      .type = "size_t",
		      .size = count * 8,
		      .provenance = schema::Provenance::recovered("luaM_free", categories_probe),
		      .count = count});

		take(context, layout,
		     global != disasm::no_object ? called_hook(**trace, global, context.abi().argument(0)) : nullptr,
		     {.name = "onfree",
		      .type = "FreeHook",
		      .size = 8,
		      .provenance = schema::Provenance::recovered("luaM_free", hook_probe),
		      .signature = schema::Signature{.result = "void",
		                                     .parameters = {{"lua_State*", "state"}, {"void*", "block"}}}});
	}

	void GlobalStateRecoverer::recover_allocation_hook(const RecoveryContext& context, schema::StructLayout& layout)
	{
		const auto probe = "the function pointer luaM_new loads off the global state and calls with the state as its "
		                   "first argument";

		const auto trace = context.trace(target::Anchor::luaM_new);
		const auto global = trace ? global_of(context, **trace) : disasm::no_object;
		const auto* hook =
		    global != disasm::no_object ? called_hook(**trace, global, context.abi().argument(0)) : nullptr;

		take(context, layout, hook,
		     {.name = "onallocate",
		      .type = "AllocationHook",
		      .size = 8,
		      .provenance = schema::Provenance::recovered("luaM_new", probe),
		      .signature = schema::Signature{.result = "void",
		                                     .parameters = {{"lua_State*", "state"},
		                                                    {"void*", "block"},
		                                                    {"size_t", "osize"},
		                                                    {"size_t", "nsize"},
		                                                    {"uint8_t", "memcat"},
		                                                    {"int", "tag"},
		                                                    {"int", "extra"}}}});
	}

	std::expected<schema::StructLayout, Error> GlobalStateRecoverer::recover(const RecoveryContext& context) const
	{
		const auto trace = context.trace(target::Anchor::luaE_newthread);
		if (!trace)
			return std::unexpected(trace.error());

		schema::StructLayout layout{.name = "global_State"};

		recover_currentwhite(context, **trace, layout);
		recover_accounting(context, layout);
		recover_allocation_hook(context, layout);

		layout.size = layout.fields.empty() ? 0 : layout.fields.back().end();

		return layout;
	}
}
