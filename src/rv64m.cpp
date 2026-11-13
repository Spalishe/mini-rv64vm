#include "../include/structs.hpp"

ExecReturn exec_MULW(Hart& hart, InstructionData& inst)
{
	hart.regs[inst.rd] = (uint64_t)((int32_t)hart.regs[inst.rs1] * (int32_t)hart.regs[inst.rs2]);
	return { true, false, 4, 0, 0 };
}

ExecReturn exec_DIVW(Hart& hart, InstructionData& inst)
{
	if((int32_t)hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)-1;
	}
	else if((int32_t)hart.regs[inst.rs1] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)0;
	}
	else if((int32_t)hart.regs[inst.rs1] == std::numeric_limits<int32_t>::min() && (int32_t)hart.regs[inst.rs2] == -1)
	{
		hart.regs[inst.rd] = (int32_t)hart.regs[inst.rs1];
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)((int32_t)hart.regs[inst.rs1] / (int32_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_DIVUW(Hart& hart, InstructionData& inst)
{
	if((uint32_t)hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)-1;
	}
	else if((uint32_t)hart.regs[inst.rs1] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)0;
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)(int32_t)((uint32_t)hart.regs[inst.rs1] / (uint32_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_REMW(Hart& hart, InstructionData& inst)
{
	if((int32_t)hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)(int32_t)hart.regs[inst.rs1];
	}
	else if((int32_t)hart.regs[inst.rs1] == std::numeric_limits<int32_t>::min() && (int32_t)hart.regs[inst.rs2] == -1)
	{
		hart.regs[inst.rd] = 0;
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)(int64_t)((int32_t)hart.regs[inst.rs1] % (int32_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_REMUW(Hart& hart, InstructionData& inst)
{
	if((uint32_t)hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1];
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)(int64_t)(int32_t)((uint32_t)hart.regs[inst.rs1] % (uint32_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}

ExecReturn exec_MUL(Hart& hart, InstructionData& inst)
{
	hart.regs[inst.rd] = hart.regs[inst.rs1] * hart.regs[inst.rs2];
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_MULH(Hart& hart, InstructionData& inst)
{
	__int128_t res	   = (__int128_t)(int64_t)hart.regs[inst.rs1] * (__int128_t)(int64_t)hart.regs[inst.rs2];
	hart.regs[inst.rd] = (uint64_t)(res >> 64);
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_MULHSU(Hart& hart, InstructionData& inst)
{
	__uint128_t res	   = (__uint128_t)(__int128_t)(int64_t)hart.regs[inst.rs1] * (__uint128_t)hart.regs[inst.rs2];
	hart.regs[inst.rd] = (uint64_t)(res >> 64);
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_MULHU(Hart& hart, InstructionData& inst)
{
	__uint128_t res	   = (__uint128_t)hart.regs[inst.rs1] * (__uint128_t)hart.regs[inst.rs2];
	hart.regs[inst.rd] = (uint64_t)(res >> 64);
	return { true, false, 4, 0, 0 };
}

ExecReturn exec_DIV(Hart& hart, InstructionData& inst)
{
	if(hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)-1;
	}
	else if(hart.regs[inst.rs1] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)0;
	}
	else if(hart.regs[inst.rs1] == std::numeric_limits<int64_t>::min() && hart.regs[inst.rs2] == -1)
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1];
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)((int64_t)hart.regs[inst.rs1] / (int64_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_DIVU(Hart& hart, InstructionData& inst)
{
	if(hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)-1;
	}
	else if(hart.regs[inst.rs1] == 0)
	{
		hart.regs[inst.rd] = (uint64_t)0;
	}
	else
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1] / hart.regs[inst.rs2];
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_REM(Hart& hart, InstructionData& inst)
{
	if(hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1];
	}
	else if(hart.regs[inst.rs1] == std::numeric_limits<int64_t>::min() && hart.regs[inst.rs2] == -1)
	{
		hart.regs[inst.rd] = 0;
	}
	else
	{
		hart.regs[inst.rd] = (uint64_t)((int64_t)hart.regs[inst.rs1] % (int64_t)hart.regs[inst.rs2]);
	}
	return { true, false, 4, 0, 0 };
}
ExecReturn exec_REMU(Hart& hart, InstructionData& inst)
{
	if(hart.regs[inst.rs2] == 0)
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1];
	}
	else
	{
		hart.regs[inst.rd] = hart.regs[inst.rs1] % (int64_t)hart.regs[inst.rs2];
	}
	return { true, false, 4, 0, 0 };
}

void InstructionDecoder::init_rv64m()
{
	add_instruction("0000001**********000*****0110011", exec_MUL);
	add_instruction("0000001**********000*****0111011", exec_MULW);
	add_instruction("0000001**********001*****0110011", exec_MULH);
	add_instruction("0000001**********010*****0110011", exec_MULHSU);
	add_instruction("0000001**********011*****0110011", exec_MULHU);
	add_instruction("0000001**********100*****0110011", exec_DIV);
	add_instruction("0000001**********101*****0110011", exec_DIVU);
	add_instruction("0000001**********101*****0111011", exec_DIVUW);
	add_instruction("0000001**********100*****0111011", exec_DIVW);
	add_instruction("0000001**********110*****0110011", exec_REM);
	add_instruction("0000001**********111*****0110011", exec_REMU);
	add_instruction("0000001**********111*****0111011", exec_REMUW);
	add_instruction("0000001**********110*****0111011", exec_REMW);
}
