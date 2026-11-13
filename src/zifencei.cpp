#include "../include/structs.hpp"

ExecReturn exec_FENCE_I(Hart& hart, InstructionData& inst)
{
	for(int i = 0; i < hart.decoder.CACHE_SIZE; i++)
	{
		hart.decoder.cache[i].valid = false;
	}
	return { true, false, 4, 0, 0 };
}

void InstructionDecoder::init_zifencei()
{
	add_instruction("*****************001*****0001111", exec_FENCE_I);
}
