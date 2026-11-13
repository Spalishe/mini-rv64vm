#include "../include/structs.hpp"
#include <cstdint>
#include <format>

MemoryRegion* MemoryMap::find_region(uint64_t address)
{
	for(auto& region : regions)
	{
		if(address >= region->start && address < (region->start + region->size))
		{
			return region.get();
		}
	}
	throw std::runtime_error(std::format("Not found region for address: {:08x}", address));
}

uint64_t MemoryMap::load(uint64_t address, MemorySize size)
{
	MemoryRegion* region = find_region(address);
	uint8_t* ptr		 = region->get_ptr(address);
	switch(size)
	{
		case MemorySize::Byte:
			return *(uint8_t*)ptr;
		case MemorySize::Halfword:
			return *(uint16_t*)ptr;
		case MemorySize::Word:
			return *(uint32_t*)ptr;
		case MemorySize::Doubleword:
			return *(uint64_t*)ptr;
	}
	return 0;
}

void MemoryMap::store(uint64_t address, MemorySize size, uint64_t value)
{
	MemoryRegion* region = find_region(address);
	uint8_t* ptr		 = region->get_ptr(address);
	switch(size)
	{
		case MemorySize::Byte:
			*(uint8_t*)ptr = value;
			break;
		case MemorySize::Halfword:
			*(uint16_t*)ptr = value;
			break;
		case MemorySize::Word:
			*(uint32_t*)ptr = value;
			break;
		case MemorySize::Doubleword:
			*(uint64_t*)ptr = value;
			break;
	}
}
