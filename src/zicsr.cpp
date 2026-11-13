#include "../include/fields.hpp"
#include "../include/structs.hpp"

bool compatible_mode(PrivilegeMode csr_level, PrivilegeMode cur_mode)
{
	if(csr_level == PrivilegeMode::Machine)
	{
		return cur_mode == PrivilegeMode::Machine;
	}
	else if(csr_level == PrivilegeMode::Supervisor)
	{
		return (cur_mode == PrivilegeMode::Machine) || (cur_mode == PrivilegeMode::Supervisor);
	}
	else if(csr_level == PrivilegeMode::User)
	{
		return (cur_mode == PrivilegeMode::Machine) || (cur_mode == PrivilegeMode::Supervisor) || (cur_mode == PrivilegeMode::User);
	}
	return true;
}

bool csr_accessible(uint16_t csr_addr, PrivilegeMode current_priv, bool write)
{
	PrivilegeMode csr_level;

	if(csr_addr >= 0x000 && csr_addr <= 0x0FF)
		csr_level = PrivilegeMode::User; // User
	else if(csr_addr >= 0x100 && csr_addr <= 0x1FF)
		csr_level = PrivilegeMode::Supervisor; // Supervisor
	else if(csr_addr >= 0x300 && csr_addr <= 0x3FF)
		csr_level = PrivilegeMode::Machine; // Machine
	else if(csr_addr >= 0xB00 && csr_addr <= 0xBFF)
		csr_level = PrivilegeMode::Machine; // Machine counters
	else if(csr_addr >= 0xC00 && csr_addr <= 0xCFF)
		csr_level = PrivilegeMode::User; // User counters (cycle/time)
	else if(csr_addr >= 0xF00 && csr_addr <= 0xFFF)
		csr_level = PrivilegeMode::Machine; // Machine info
	else
		csr_level = PrivilegeMode::User;

	if(!compatible_mode(csr_level, current_priv))
	{
		return false;
	}

	if(csr_addr == 0xDA0
	   || (csr_addr >= 0x10C && csr_addr <= 0x10F)
	   || (csr_addr >= 0x30C && csr_addr <= 0x30F)
	   || csr_addr == 0xFB0
	   || csr_addr == 0x7A0
	   || (csr_addr >= 0x7A1 && csr_addr <= 0x7A3)
	   || csr_addr == 0x321
	   || csr_addr == 0xB02)
	{
		// Unimplemented
		return false;
	}

	if(csr_addr == 0xF11
	   || csr_addr == 0xF12
	   || csr_addr == 0xF13
	   || csr_addr == 0xF14
	   || csr_addr == 0xF15f
	   || (csr_addr >= 0xC00 && csr_addr <= 0xC1F))
	{
		return !write; // RO
	}

	return true;
}

ExecReturn exec_CSRRW(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, true))
	{
		return { false, false, 0, 2, inst.inst };
	}

	uint64_t init_val = hart.csr_read(inst.imm);
	hart.csr_write(inst.imm, hart.regs[inst.rs1]);
	if(inst.rd != 0)
	{
		hart.regs[inst.rd] = init_val;
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_CSRRS(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, (inst.rs1 != 0)))
	{
		return { false, false, 0, 2, inst.inst };
	}

	uint64_t init_val = hart.csr_read(inst.imm);
	if(inst.rs1 != 0)
	{
		uint64_t mask = hart.regs[inst.rs1];
		hart.csr_write(inst.imm, init_val | mask);
	}
	hart.regs[inst.rd] = init_val;
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_CSRRC(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, (inst.rs1 != 0)))
	{
		return { false, false, 0, 2, inst.inst };
	}

	uint64_t init_val = hart.csr_read(inst.imm);
	if(inst.rs1 != 0)
	{
		uint64_t mask = hart.regs[inst.rs1];
		hart.csr_write(inst.imm, init_val & ~mask);
	}
	hart.regs[inst.rd] = init_val;
	return { true, false, 4, 0, 0 };
}

ExecReturn exec_CSRRWI(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, inst.rs1 != 0))
	{
		return { false, false, 0, 2, inst.inst };
	}

	if(inst.rd != 0)
	{
		hart.regs[inst.rd] = hart.csr_read(inst.imm);
	}
	hart.csr_write(inst.imm, inst.rs1);
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_CSRRSI(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, (inst.rs1 != 0)))
	{
		return { false, false, 0, 2, inst.inst };
	}

	uint64_t init_val = hart.csr_read(inst.imm);
	if(inst.rs1 != 0)
	{
		uint64_t mask = inst.rs1;
		hart.csr_write(inst.imm, init_val | mask);
	}
	hart.regs[inst.rd] = init_val;
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_CSRRCI(Hart& hart, InstructionData& inst)
{
	if(!csr_accessible(inst.imm, hart.mode, (inst.rs1 != 0)))
	{
		return { false, false, 0, 2, inst.inst };
	}

	uint64_t init_val = hart.csr_read(inst.imm);
	if(inst.rs1 != 0)
	{
		uint64_t mask = inst.rs1;
		hart.csr_write(inst.imm, init_val & ~mask);
	}
	hart.regs[inst.rd] = init_val;
	return { true, false, 4, 0, 0 };
}

void InstructionDecoder::init_zicsr()
{
	add_instruction("*****************001*****1110011", exec_CSRRW, imm_Zicsr);
	add_instruction("*****************010*****1110011", exec_CSRRS, imm_Zicsr);
	add_instruction("*****************011*****1110011", exec_CSRRC, imm_Zicsr);
	add_instruction("*****************101*****1110011", exec_CSRRWI, imm_Zicsr);
	add_instruction("*****************110*****1110011", exec_CSRRSI, imm_Zicsr);
	add_instruction("*****************111*****1110011", exec_CSRRCI, imm_Zicsr);
}
