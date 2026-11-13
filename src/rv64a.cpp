#include "../include/structs.hpp"

MemoryReturn AMO_SC(Hart& hart, uint64_t addr, MemorySize size, uint64_t val, void* out_val)
{
	if(hart.reservation.valid && hart.reservation.addr == addr && hart.reservation.size == size)
	{
		MemoryReturn out = hart.mmio->store(&hart, addr, size, val);
		if(!out.is_success) return out;
		hart.reservation.valid = false;
		*(uint8_t*)out_val	   = 0;
		return out;
	}
	else
	{
		hart.reservation.valid = false;
		*(uint8_t*)out_val	   = 1;
		return { true, 0, 0 };
	}
}
MemoryReturn AMO_LR(Hart& hart, uint64_t addr, MemorySize size, void* val)
{
	uint64_t value;
	MemoryReturn p = hart.mmio->load(&hart, addr, size, &value);
	if(!p.is_success) return p;
	hart.reservation.valid = true;
	hart.reservation.size  = size;
	hart.reservation.addr  = addr;
	switch(size)
	{
		case MemorySize::Byte:
			*(uint8_t*)val = value;
			break;
		case MemorySize::Halfword:
			*(uint16_t*)val = value;
			break;
		case MemorySize::Word:
			*(uint32_t*)val = value;
			break;
		case MemorySize::Doubleword:
			*(uint64_t*)val = value;
			break;
	}
	return p;
}

ExecReturn exec_LR_D(Hart& hart, InstructionData& inst)
{
	uint64_t val	 = 0;
	MemoryReturn out = AMO_LR(hart, hart.regs[inst.rs1], MemorySize::Doubleword, &val);
	if(!out.is_success) return { false, false, 4, out.exc_code, out.tval };
	hart.regs[inst.rd] = val;
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_SC_D(Hart& hart, InstructionData& inst)
{
	uint64_t val	 = 0;
	MemoryReturn out = AMO_SC(hart, hart.regs[inst.rs1], MemorySize::Doubleword, hart.regs[inst.rs2], &val);
	if(!out.is_success) return { false, false, 4, out.exc_code, out.tval };
	hart.regs[inst.rd] = val;
	return { true, false, 4, 0, 0 };
}

MemoryReturn AMO64(Hart& hart, uint64_t addr, uint64_t rs2, uint64_t (*func)(uint64_t a, uint64_t b), uint64_t* out_val)
{
	uint64_t val;
	MemoryReturn out = hart.mmio->load(&hart, addr, MemorySize::Doubleword, &val);
	if(!out.is_success) return out;
	uint64_t new_val = func(val, rs2);

	MemoryReturn s = hart.mmio->store(&hart, addr, MemorySize::Doubleword, new_val);
	if(!s.is_success) return s;
	*(uint64_t*)out_val = val;
	return { true, 0, 0 };
}

ExecReturn exec_AMOSWAP_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOADD_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return a + b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOXOR_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return a ^ b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOAND_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return a & b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOOR_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return a | b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMIN_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return (uint64_t)std::min((int64_t)a, (int64_t)b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMAX_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return (uint64_t)std::max((int64_t)a, (int64_t)b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMINU_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return std::min(a, b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMAXU_D(Hart& hart, InstructionData& inst)
{
	uint64_t val;
	MemoryReturn out = AMO64(hart, hart.regs[inst.rs1], hart.regs[inst.rs2], [](uint64_t a, uint64_t b)
	{ return std::max(a, b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}

// RV32A

ExecReturn exec_LR_W(Hart& hart, InstructionData& inst)
{
	uint32_t val	 = 0;
	MemoryReturn out = AMO_LR(hart, hart.regs[inst.rs1], MemorySize::Word, &val);
	if(!out.is_success) return { false, false, 0, out.exc_code, out.tval };
	hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_SC_W(Hart& hart, InstructionData& inst)
{
	uint32_t val	 = 0;
	MemoryReturn out = AMO_SC(hart, hart.regs[inst.rs1], MemorySize::Word, (uint32_t)hart.regs[inst.rs2], &val);
	if(!out.is_success) return { false, false, 0, out.exc_code, out.tval };
	hart.regs[inst.rd] = val;
	return { true, false, 4, 0, 0 };
}

MemoryReturn AMO32(Hart& hart, uint64_t addr, uint32_t rs2, uint32_t (*func)(uint32_t a, uint32_t b), uint32_t* out_val)
{
	uint32_t val;
	MemoryReturn out = hart.mmio->load(&hart, addr, MemorySize::Word, &val);
	if(!out.is_success) return out;
	uint64_t new_val = func(val, rs2);

	MemoryReturn s = hart.mmio->store(&hart, addr, MemorySize::Word, new_val);
	if(!s.is_success) return s;
	*(uint32_t*)out_val = val;
	return { true, 0, 0 };
}

ExecReturn exec_AMOSWAP_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOADD_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return a + b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOXOR_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return a ^ b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOAND_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return a & b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOOR_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return a | b; }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMIN_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return (uint32_t)std::min((int32_t)a, (int32_t)b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMAX_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return (uint32_t)std::max((int32_t)a, (int32_t)b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMINU_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return std::min(a, b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}
ExecReturn exec_AMOMAXU_W(Hart& hart, InstructionData& inst)
{
	uint32_t val;
	MemoryReturn out = AMO32(hart, hart.regs[inst.rs1], (uint32_t)hart.regs[inst.rs2], [](uint32_t a, uint32_t b)
	{ return std::max(a, b); }, &val);
	if(out.is_success) hart.regs[inst.rd] = (int64_t)(int32_t)val;
	return { out.is_success, false, 4, out.exc_code, out.tval };
}

void InstructionDecoder::init_rv64a()
{
	add_instruction("00001************010*****0101111", exec_AMOSWAP_W);
	add_instruction("00000************010*****0101111", exec_AMOADD_W);
	add_instruction("01100************010*****0101111", exec_AMOAND_W);
	add_instruction("00100************010*****0101111", exec_AMOXOR_W);
	add_instruction("01000************010*****0101111", exec_AMOOR_W);
	add_instruction("10000************010*****0101111", exec_AMOMIN_W);
	add_instruction("11000************010*****0101111", exec_AMOMINU_W);
	add_instruction("10100************010*****0101111", exec_AMOMAX_W);
	add_instruction("11100************010*****0101111", exec_AMOMAXU_W);
	add_instruction("00010************010*****0101111", exec_LR_W);
	add_instruction("00011************010*****0101111", exec_SC_W);

	add_instruction("00001************011*****0101111", exec_AMOSWAP_D);
	add_instruction("00000************011*****0101111", exec_AMOADD_D);
	add_instruction("01100************011*****0101111", exec_AMOAND_D);
	add_instruction("00100************011*****0101111", exec_AMOXOR_D);
	add_instruction("01000************011*****0101111", exec_AMOOR_D);
	add_instruction("10000************011*****0101111", exec_AMOMIN_D);
	add_instruction("11000************011*****0101111", exec_AMOMINU_D);
	add_instruction("10100************011*****0101111", exec_AMOMAX_D);
	add_instruction("11100************011*****0101111", exec_AMOMAXU_D);
	add_instruction("00010************011*****0101111", exec_LR_D);
	add_instruction("00011************011*****0101111", exec_SC_D);
}
