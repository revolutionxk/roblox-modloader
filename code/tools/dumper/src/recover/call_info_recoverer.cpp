#include "recover/call_info_recoverer.hpp"

#include "recover/closure_recoverer.hpp"
#include "recover/proto_recoverer.hpp"

#include <algorithm>
#include <format>
#include <map>

namespace rml::dumper::recover
{
	std::vector<std::int64_t> CallInfoRecoverer::relocated_slots(const disasm::Trace& trace,
	                                                             const disasm::Object state)
	{
		std::map<disasm::Object, std::vector<std::int64_t>> by_object;

		for (const auto& access : trace.accesses)
		{
			if (!access.is_write || access.width != 8 || access.object == state ||
			    access.object == disasm::no_object || access.index != disasm::Register::none ||
			    access.displacement < 0 || access.displacement >= 0x40)
				continue;

			auto& slots = by_object[access.object];
			if (std::ranges::find(slots, access.displacement) == slots.end())
				slots.push_back(access.displacement);
		}

		for (const auto& [object, slots] : by_object)
			if (slots.size() == relocated_count)
				return slots;

		return {};
	}

	const disasm::MemoryAccess* CallInfoRecoverer::taken_from_argument(const disasm::Trace& trace,
	                                                                   const disasm::Register argument)
	{
		const auto found = std::ranges::find_if(trace.accesses, [argument](const disasm::MemoryAccess& access) {
			return access.is_write && access.width == 8 && access.value_object == disasm::entry_object(argument) &&
			       access.displacement >= 0 && access.displacement < 0x40;
		});

		return found != trace.accesses.end() ? &*found : nullptr;
	}

	std::optional<std::int64_t> CallInfoRecoverer::handed_to_state(const disasm::Trace& trace,
	                                                               const disasm::Object frame,
	                                                               const disasm::Object state,
	                                                               const std::size_t slot)
	{
		for (const auto& read : trace.accesses)
		{
			if (read.is_write || read.object != frame || read.width != 8 || read.displacement < 0)
				continue;

			for (const auto& write : trace.accesses)
			{
				if (!write.is_write || write.object != state || write.width != 8)
					continue;
				if (static_cast<std::size_t>(write.displacement) != slot ||
				    write.value_register != read.value_register || write.sequence <= read.sequence)
					continue;

				return read.displacement;
			}
		}

		return std::nullopt;
	}

	std::optional<std::int64_t> CallInfoRecoverer::stride(const disasm::Trace& trace, const disasm::Register frame,
	                                                      const std::size_t before, const std::int64_t smallest)
	{
		for (const auto& constant : trace.constants)
		{
			if (constant.kind != disasm::ConstantKind::step || constant.destination != frame)
				continue;
			if (constant.sequence >= before || constant.value < smallest || constant.value % 8 != 0)
				continue;

			return constant.value;
		}

		return std::nullopt;
	}

	void CallInfoRecoverer::recover_proto_slots(const RecoveryContext& context, const disasm::Trace& trace,
	                                            const disasm::Object frame, schema::StructLayout& layout)
	{
		const auto proto_probe = "the qword luau_precall stores into the new call info, carrying the closure's proto";
		const auto code_probe = "the qword luau_precall stores into the new call info, carrying the code of the "
		                        "closure's proto";

		const auto* closure = context.layout("Closure");
		const auto* proto = context.layout("Proto");
		const auto* is_c = closure != nullptr ? closure->find("isC") : nullptr;
		const auto* p = closure != nullptr ? closure->find("p") : nullptr;
		const auto* code = proto != nullptr ? proto->find("code") : nullptr;

		if (is_c == nullptr || p == nullptr || code == nullptr)
		{
			context.report().record_failure("CallInfo", "proto", proto_probe);
			context.report().record_failure("CallInfo", "savedpc", code_probe);
			return;
		}

		const disasm::TraceQuery query(trace);
		const auto closures = ClosureRecoverer::closures_in(trace, is_c->offset);
		const auto protos = ProtoRecoverer::protos_in(trace, closures, p->offset);

		const auto carrying = [&](const auto& loaded_from) -> const disasm::MemoryAccess* {
			for (const auto& store : trace.accesses)
			{
				if (!store.is_write || store.object != frame || store.width != 8 || store.displacement < 0 ||
				    layout.overlaps(static_cast<std::size_t>(store.displacement), 8))
					continue;

				for (const auto& load : trace.accesses)
					if (!load.is_write && load.width == 8 && loaded_from(load) && query.stored_step(load, store) == 0)
						return &store;
			}

			return nullptr;
		};

		const auto* proto_slot = carrying([&](const disasm::MemoryAccess& load) {
			return closures.contains(load.object) && static_cast<std::size_t>(load.displacement) == p->offset;
		});

		if (proto_slot == nullptr)
		{
			context.report().record_failure("CallInfo", "proto", proto_probe);
		}
		else
		{
			context.report().record_recovered("CallInfo", "proto", proto_probe);
			layout.add({.name = "proto",
			            .type = "Proto*",
			            .size = 8,
			            .offset = static_cast<std::size_t>(proto_slot->displacement),
			            .provenance = schema::Provenance::recovered("luau_precall", proto_probe)});
		}

		const auto* code_slot = carrying([&](const disasm::MemoryAccess& load) {
			return protos.contains(load.object) && static_cast<std::size_t>(load.displacement) == code->offset;
		});

		if (code_slot == nullptr)
		{
			context.report().record_failure("CallInfo", "savedpc", code_probe);
			return;
		}

		context.report().record_recovered("CallInfo", "savedpc", code_probe);
		layout.add({.name = "savedpc",
		            .type = "const Instruction*",
		            .size = 8,
		            .offset = static_cast<std::size_t>(code_slot->displacement),
		            .provenance = schema::Provenance::recovered("luau_precall", code_probe)});
	}

	std::expected<schema::StructLayout, Error> CallInfoRecoverer::recover(const RecoveryContext& context) const
	{
		const auto precall = context.trace(target::Anchor::luau_precall);
		const auto stack = context.trace(target::Anchor::luaD_reallocstack);
		if (!precall)
			return std::unexpected(precall.error());
		if (!stack)
			return std::unexpected(stack.error());

		schema::StructLayout layout{.name = "CallInfo"};

		const auto* func = taken_from_argument(**precall, context.abi().argument(1));
		if (func == nullptr)
		{
			context.report().record_failure("CallInfo", "func",
			                                "luau_precall stores no qword straight from its function argument");
			return layout;
		}

		const auto probe = "the qword luau_precall stores straight from the function it is about to call";
		context.report().record_recovered("CallInfo", "func", probe);
		layout.add({.name = "func",
		            .type = "StkId",
		            .size = 8,
		            .offset = static_cast<std::size_t>(func->displacement),
		            .provenance = schema::Provenance::recovered("luau_precall", probe)});

		const auto* state = context.layout("lua_State");
		const auto* state_top = state != nullptr ? state->find("top") : nullptr;

		if (state_top != nullptr)
		{
			const auto top = handed_to_state(**precall, func->object,
			                                 disasm::entry_object(context.abi().argument(0)), state_top->offset);

			const auto reason = "the qword luau_precall hands over to the top of the state";

			if (top)
			{
				context.report().record_recovered("CallInfo", "top", reason);
				layout.add({.name = "top",
				            .type = "StkId",
				            .size = 8,
				            .offset = static_cast<std::size_t>(*top),
				            .provenance = schema::Provenance::recovered("luau_precall", reason)});
			}
			else
			{
				context.report().record_failure("CallInfo", "top", reason);
			}
		}

		const disasm::TraceQuery query(**stack);
		const auto slots = relocated_slots(**stack, query.dominant_object());

		if (slots.size() != relocated_count)
		{
			context.report().record_failure(
			    "CallInfo", "base",
			    std::format("correctstack relocates {} slots through a call info, expected {}", slots.size(),
			                relocated_count));
		}
		else
		{
			const auto left = std::ranges::find_if(slots, [&layout](const std::int64_t slot) {
				return !layout.covers(static_cast<std::size_t>(slot));
			});

			const auto reason = "the one of the three slots correctstack relocates that is neither the function "
			                    "nor the top";

			if (left != slots.end() && layout.fields.size() == relocated_count - 1)
			{
				context.report().record_recovered("CallInfo", "base", reason);
				layout.add({.name = "base",
				            .type = "StkId",
				            .size = 8,
				            .offset = static_cast<std::size_t>(*left),
				            .provenance = schema::Provenance::recovered("luaD_reallocstack", reason)});
			}
			else
			{
				context.report().record_failure("CallInfo", "base", reason);
			}
		}

		recover_proto_slots(context, **precall, func->object, layout);

		const auto smallest = layout.fields.empty() ? 0 : static_cast<std::int64_t>(layout.fields.back().end());
		const auto size = stride(**precall, func->base, func->sequence, smallest);

		if (!size)
		{
			context.report().record_failure(
			    "CallInfo", "sizeof",
			    std::format("luau_precall walks the call info array by nothing that reaches 0x{:X}", smallest));
			layout.size = static_cast<std::size_t>(smallest);
			return layout;
		}

		context.report().record_recovered(
		    "CallInfo", "sizeof", std::format("0x{:X}, the step luau_precall takes to reach the next frame", *size));
		layout.size = static_cast<std::size_t>(*size);

		return layout;
	}
}
