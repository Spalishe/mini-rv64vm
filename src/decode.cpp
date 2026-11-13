#include "../include/structs.hpp"
#include <cassert>

InstructionDecoder::InstructionDecoder()
{
	cache = new InstructionCache[CACHE_SIZE];

	init_rv64i();
	init_rv64m();
	init_rv64a();
	init_zicsr();
	init_zifencei();
	init_priv();
}

void InstructionDecoder::add_instruction(std::string instruction_mask, ExecReturn (*func)(Hart& h, InstructionData& data), uint64_t (*immfunc)(uint32_t inst))
{
	assert((instruction_mask.size() == 32) && "Instruction instruction_mask size isn't 32 bits.");
	uint32_t inst_mask	= 0;
	uint32_t inst_match = 0;
	for(int i = 0; i < instruction_mask.size(); i++)
	{
		char c = instruction_mask[(instruction_mask.size() - 1) - i];
		if(c == '1')
		{
			inst_match |= (1u << i);
			inst_mask |= (1u << i);
		}
		else if(c == '0')
		{
			inst_mask |= (1u << i);
		}
		else if(c == '*')
		{
			// dont care
		}
	}
	insts.emplace_back();
	Instruction& inst = insts.back();
	inst.func		  = func;
	inst.mask		  = inst_mask;
	inst.match		  = inst_match;
	inst.immfunc	  = immfunc;
}

static inline uint8_t d_rd(uint32_t inst)
{
	return (inst >> 7) & 0x1f;
}

static inline uint8_t d_rs1(uint32_t inst)
{
	return (inst >> 15) & 0x1f;
}

static inline uint8_t d_rs2(uint32_t inst)
{
	return (inst >> 20) & 0x1f;
}

InstructionCache& InstructionDecoder::decode(uint64_t pc, uint32_t inst)
{
	InstructionCache& c = cache[inst & (CACHE_SIZE - 1)];
	if(c.valid && c.pc == pc)
	{
		return c;
	}

	Instruction* i = nullptr;
	for(auto& in : insts)
	{
		if((inst & in.mask) == in.match)
		{
			i = &in;
			break;
		}
	}
	if(i == nullptr)
	{
		c.valid = false;
		return c;
	}

	c.valid = true;
	c.inst	= i;
	c.pc	= pc;
	c.data	= { d_rd(inst), d_rs1(inst), d_rs2(inst), i->immfunc != nullptr ? i->immfunc(inst) : 0, inst };
	return c;
}
