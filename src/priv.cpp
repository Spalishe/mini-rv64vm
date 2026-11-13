#include "../include/fields.hpp"
#include "../include/structs.hpp"

ExecReturn exec_MRET(Hart& hart, InstructionData& inst)
{
	hart.pc		= hart.csr_read(CSR_MEPC);
	uint64_t st = hart.csrs[CSR_MSTATUS];
	switch((st >> 11) & 3)
	{
		case 0b00:
			// If MPP != M-mode, MRET also sets MPRV=0.
			st &= ~(1 << 17);
			hart.mode = PrivilegeMode::User;
			break;
		case 0b01:
			st &= ~(1 << 17);
			hart.mode = PrivilegeMode::Supervisor;
			break;
		case 0b11:
			hart.mode = PrivilegeMode::Machine;
			break;
	}
	// Read a previous interrupt-enable bit for machine mode (MPIE, 7), and set a global interrupt-enable bit for machine mode (MIE, 3) to it.
	st &= ~(1 << 3);
	st |= ((st >> 7) & 1) << 3;
	st |= (1 << 7);
	st &= ~((1 << 11) | (1 << 12));
	st |= ((char)PrivilegeMode::User << 11);
	hart.csrs[CSR_MSTATUS] = st;
	return { true, true, 0, 0, 0 };
}
ExecReturn exec_SRET(Hart& hart, InstructionData& inst)
{
	uint64_t st = hart.csrs[CSR_MSTATUS];
	if(hart.mode == PrivilegeMode::User || (hart.mode == PrivilegeMode::Supervisor && ((st >> 22) & 1)))
	{
		return { false, false, 0, 2, inst.inst };
	}
	hart.pc = hart.csr_read(CSR_SEPC);
	st &= ~(1 << 17);
	switch((st >> 8) & 1)
	{
		case 0b0:
			hart.mode = PrivilegeMode::User;
			break;
		case 0b1:
			hart.mode = PrivilegeMode::Supervisor;
			break;
	}
	// Read a previous interrupt-enable bit for supervisor mode (SPIE,5), and set a global interrupt-enable bit for supervisor mode (SIE, 1) to it.
	st &= ~2;
	st |= ((st >> 5) & 1) << 1;
	st |= 1 << 5;
	st &= ~(1 << 8);
	st |= ((char)PrivilegeMode::User << 8);
	hart.csrs[CSR_MSTATUS] = st;
	return { true, true, 0, 0, 0 };
}
ExecReturn exec_SFENCE_VMA(Hart& hart, InstructionData& inst)
{
	// Clear TLB Cache, but theres no mmu so we dont need this.

	bool tvm = (hart.csrs[CSR_MSTATUS] >> 20) & 1;

	// Only legal in S or M mode
	if(hart.mode == PrivilegeMode::User)
	{
		return { false, false, 0, 2, inst.inst };
	}
	if(hart.mode == PrivilegeMode::Supervisor && tvm)
	{
		return { false, false, 0, 2, inst.inst };
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_WFI(Hart& hart, InstructionData& inst)
{
	bool tw = (hart.csrs[CSR_MSTATUS] >> 21) & 1;

	if(tw)
	{
		return { false, false, 0, 2, inst.inst };
	}
	hart.WFI = true;
	return { true, false, 4, 0, 0 };
}

void InstructionDecoder::init_priv()
{
	add_instruction("00110000001000000000000001110011", exec_MRET);
	add_instruction("00010000001000000000000001110011", exec_SRET);
	add_instruction("00010000010100000000000001110011", exec_WFI);
	add_instruction("0001001**********000000001110011", exec_SFENCE_VMA);
}
