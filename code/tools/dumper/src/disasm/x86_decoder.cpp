#include "disasm/x86_decoder.hpp"

#include <Zydis/Zydis.h>

#include <algorithm>
#include <array>

namespace rml::dumper::disasm
{
	static Register to_register(const ZydisRegister value)
	{
		switch (ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, value))
		{
		case ZYDIS_REGISTER_RAX: return Register::rax;
		case ZYDIS_REGISTER_RCX: return Register::rcx;
		case ZYDIS_REGISTER_RDX: return Register::rdx;
		case ZYDIS_REGISTER_RBX: return Register::rbx;
		case ZYDIS_REGISTER_RSP: return Register::rsp;
		case ZYDIS_REGISTER_RBP: return Register::rbp;
		case ZYDIS_REGISTER_RSI: return Register::rsi;
		case ZYDIS_REGISTER_RDI: return Register::rdi;
		case ZYDIS_REGISTER_R8: return Register::r8;
		case ZYDIS_REGISTER_R9: return Register::r9;
		case ZYDIS_REGISTER_R10: return Register::r10;
		case ZYDIS_REGISTER_R11: return Register::r11;
		case ZYDIS_REGISTER_R12: return Register::r12;
		case ZYDIS_REGISTER_R13: return Register::r13;
		case ZYDIS_REGISTER_R14: return Register::r14;
		case ZYDIS_REGISTER_R15: return Register::r15;
		case ZYDIS_REGISTER_RIP: return Register::rip;
		default: return Register::none;
		}
	}

	class ValueOrigins
	{
	public:
		ValueOrigins()
		{
			for (std::size_t i = 0; i < m_origins.size(); ++i)
			{
				m_origins[i] = static_cast<Register>(i);
				m_objects[i] = entry_object(static_cast<Register>(i));
			}
		}

		[[nodiscard]] Register of(const Register value) const
		{
			const auto index = static_cast<std::size_t>(value);
			return index < m_origins.size() ? m_origins[index] : value;
		}

		[[nodiscard]] Object object_of(const Register value) const
		{
			const auto index = static_cast<std::size_t>(value);
			return index < m_objects.size() ? m_objects[index] : no_object;
		}

		void alias(const Register destination, const Register source)
		{
			if (const auto index = static_cast<std::size_t>(destination); index < m_origins.size())
			{
				m_origins[index] = of(source);
				m_objects[index] = object_of(source);
			}
		}

		void redefine(const Register destination)
		{
			if (const auto index = static_cast<std::size_t>(destination); index < m_origins.size())
			{
				m_origins[index] = destination;
				m_objects[index] = m_next++;
			}
		}

		void set_zero(const Register destination)
		{
			if (const auto index = static_cast<std::size_t>(destination); index < m_origins.size())
			{
				m_origins[index] = destination;
				m_objects[index] = zero_object;
			}
		}

		void capture(std::array<Object, register_slots>& objects) const
		{
			std::ranges::copy(m_objects, objects.begin());
		}

		void redefine_volatiles()
		{
			for (const auto scratch : {Register::rax, Register::rcx, Register::rdx, Register::r8, Register::r9,
			                           Register::r10, Register::r11})
				redefine(scratch);
		}

	private:
		static constexpr std::size_t slots = static_cast<std::size_t>(Register::rip) + 1;

		std::array<Register, slots> m_origins{};
		std::array<Object, slots> m_objects{};
		Object m_next{first_derived_object};
	};

	static bool stops_the_trace(const ZydisMnemonic mnemonic)
	{
		return mnemonic == ZYDIS_MNEMONIC_UD2 || mnemonic == ZYDIS_MNEMONIC_INT3;
	}

	static std::optional<std::uint64_t> first_immediate(const ZydisDecodedInstruction& instruction,
	                                                    const ZydisDecodedOperand* operands)
	{
		for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
			if (operands[i].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
				return operands[i].imm.value.u;

		return std::nullopt;
	}

	static Register companion_register(const ZydisDecodedInstruction& instruction,
	                                   const ZydisDecodedOperand* operands, const std::uint8_t memory_operand)
	{
		for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
		{
			if (i == memory_operand || operands[i].type != ZYDIS_OPERAND_TYPE_REGISTER)
				continue;

			if (const auto mapped = to_register(operands[i].reg.value); mapped != Register::none)
				return mapped;
		}

		return Register::none;
	}

	static bool writes(const ZydisDecodedInstruction& instruction, const ZydisDecodedOperand* operands,
	                   const Register value)
	{
		for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
			if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER && to_register(operands[i].reg.value) == value &&
			    (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0)
				return true;

		return false;
	}

	static void record_constant(Trace& trace, const ZydisDecodedInstruction& instruction,
	                            const ZydisDecodedOperand* operands, const Rva address, std::size_t& sequence,
	                            const ValueOrigins& origins)
	{
		const auto destination = operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
		                             ? to_register(operands[0].reg.value)
		                             : Register::none;

		const auto push = [&](const std::int64_t value, const ConstantKind kind) {
			if (value <= 1 || value > 0x10000)
				return;

			trace.constants.push_back({.sequence = sequence++,
			                           .address = address,
			                           .destination = destination,
			                           .value = value,
			                           .kind = kind,
			                           .operand = origins.object_of(destination)});
		};

		switch (instruction.mnemonic)
		{
		case ZYDIS_MNEMONIC_IMUL:
			for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
				if (operands[i].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
					push(operands[i].imm.value.s, ConstantKind::scale);
			break;

		case ZYDIS_MNEMONIC_SHL:
			if (instruction.operand_count_visible >= 2 && operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
			    operands[1].imm.value.u < 16)
				push(static_cast<std::int64_t>(1) << operands[1].imm.value.u, ConstantKind::scale);
			break;

		case ZYDIS_MNEMONIC_ADD:
		case ZYDIS_MNEMONIC_SUB:
			if (instruction.operand_count_visible >= 2 && operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
			    destination != Register::rsp)
				push(operands[1].imm.value.s, ConstantKind::step);
			break;

		case ZYDIS_MNEMONIC_MOV:
			if (instruction.operand_count_visible >= 2 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
			    operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
				push(operands[1].imm.value.s, ConstantKind::literal);
			break;

		case ZYDIS_MNEMONIC_LEA:
			if (instruction.operand_count_visible >= 2 && operands[1].type == ZYDIS_OPERAND_TYPE_MEMORY &&
			    operands[1].mem.index != ZYDIS_REGISTER_NONE && operands[1].mem.scale > 1)
				push(operands[1].mem.scale, ConstantKind::scale);
			break;

		default:
			break;
		}
	}

	std::expected<Trace, Error> X86Decoder::trace(const std::span<const std::byte> code, const Rva begin) const
	{
		ZydisDecoder decoder;
		if (ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
			return std::unexpected(Error::make(ErrorCode::recovery, "cannot initialise the x86 decoder"));

		Trace trace;
		trace.begin = begin;
		trace.end = begin;

		ValueOrigins origins;
		std::size_t sequence = 0;
		std::size_t offset = 0;

		while (offset < code.size())
		{
			ZydisDecodedInstruction instruction;
			ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

			if (ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, code.data() + offset, code.size() - offset,
			                                       &instruction, operands)))
				break;

			if (stops_the_trace(instruction.mnemonic))
				break;

			const auto address = static_cast<Rva>(begin + offset);

			if (instruction.mnemonic == ZYDIS_MNEMONIC_CALL)
			{
				CallSite call;
				call.sequence = sequence++;
				call.address = address;

				for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
				{
					if (operands[i].type != ZYDIS_OPERAND_TYPE_IMMEDIATE || !operands[i].imm.is_relative)
						continue;

					ZyanU64 absolute = 0;
					if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operands[i], address, &absolute)))
						call.target = static_cast<Rva>(absolute);
				}

				for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
					if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER)
						call.callee = origins.object_of(to_register(operands[i].reg.value));

				origins.capture(call.registers);
				trace.calls.push_back(call);
				origins.redefine_volatiles();
			}

			const auto first_access = trace.accesses.size();
			const auto first_constant = trace.constants.size();
			std::array<Register, ZYDIS_MAX_OPERAND_COUNT> filled{};

			for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
			{
				const auto& operand = operands[i];
				if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY)
					continue;

				const auto raw_base = to_register(operand.mem.base);
				const auto base = origins.of(raw_base);
				if (base == Register::none || base == Register::rsp)
					continue;

				MemoryAccess access;
				access.sequence = sequence++;
				access.address = address;
				access.object = origins.object_of(raw_base);
				access.base = base;
				access.index = origins.of(to_register(operand.mem.index));
				access.scale = operand.mem.scale != 0 ? operand.mem.scale : 1;
				access.width = static_cast<std::uint8_t>(operand.size / 8);
				access.displacement = operand.mem.disp.has_displacement ? operand.mem.disp.value : 0;
				access.is_write = (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0;
				const auto raw_value = companion_register(instruction, operands, i);
				access.value_register = origins.of(raw_value);
				access.value_object = origins.object_of(raw_value);
				access.immediate = first_immediate(instruction, operands);

				if (access.is_write && !access.immediate && origins.object_of(raw_value) == zero_object)
					access.immediate = 0;

				if (!access.is_write && writes(instruction, operands, raw_value))
					filled[trace.accesses.size() - first_access] = raw_value;

				trace.accesses.push_back(access);
			}

			record_constant(trace, instruction, operands, address, sequence, origins);

			if (instruction.mnemonic == ZYDIS_MNEMONIC_MOV && instruction.operand_count_visible == 2 &&
			    operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER)
			{
				origins.alias(to_register(operands[0].reg.value), to_register(operands[1].reg.value));
			}
			else
			{
				for (std::uint8_t i = 0; i < instruction.operand_count_visible; ++i)
					if (operands[i].type == ZYDIS_OPERAND_TYPE_REGISTER &&
					    (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0)
						origins.redefine(to_register(operands[i].reg.value));
			}

			if ((instruction.mnemonic == ZYDIS_MNEMONIC_XOR || instruction.mnemonic == ZYDIS_MNEMONIC_SUB) &&
			    instruction.operand_count_visible == 2 &&
			    operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
			    operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER &&
			    operands[0].reg.value == operands[1].reg.value)
				origins.set_zero(to_register(operands[0].reg.value));

			for (auto index = first_access; index < trace.accesses.size(); ++index)
				if (const auto value = filled[index - first_access]; value != Register::none)
					trace.accesses[index].loaded = origins.object_of(value);

			for (auto index = first_constant; index < trace.constants.size(); ++index)
				trace.constants[index].result = origins.object_of(trace.constants[index].destination);

			offset += instruction.length;
			trace.end = static_cast<Rva>(begin + offset);
		}

		return trace;
	}
}
