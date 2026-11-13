#include "../include/consts.hpp"
#include "../include/structs.hpp"

MemoryReturn MMIO::store(
	Hart* h,
	uint64_t addr,
	MemorySize size,
	uint64_t val)
{
	const uint64_t end = 0x80000000ULL + memory_size;

	if(addr >= 0x80000000ULL && addr <= end - (uint64_t)size) [[likely]]
	{
		// DRAM
		h->amo_check_reservation(addr);
		mmap->store(addr, size, val);
		return { true, 0, 0 };
	}

	// Looking up for devices in this range
	for(const auto& dev : devices)
	{
		if(addr >= dev->start && addr <= dev->start + dev->size - (uint64_t)size)
		{
			h->amo_check_reservation(addr);
			dev->write(
				addr - dev->start,
				size,
				val);

			return { true, 0, 0 };
		}
	}

	return { false, 7, addr };
}

MemoryReturn MMIO::load(
	Hart* h,
	uint64_t addr,
	MemorySize size,
	void* val)
{
	uint64_t out;

	const uint64_t end = 0x80000000ULL + memory_size;

	if(addr >= 0x80000000ULL && addr <= end - (uint64_t)size) [[likely]]
	{
		out = mmap->load(addr, size);

		goto success;
	}

	for(const auto& dev : devices)
	{
		if(addr >= dev->start && addr <= dev->start + dev->size - (uint64_t)size)
		{
			out = dev->read(
				addr - dev->start,
				size);

			goto success;
		}
	}

	return { false, 5, addr };

success:
	switch(size)
	{
		case MemorySize::Byte:
			*(uint8_t*)val = out;
			break;

		case MemorySize::Halfword:
			*(uint16_t*)val = out;
			break;

		case MemorySize::Word:
			*(uint32_t*)val = out;
			break;

		case MemorySize::Doubleword:
			*(uint64_t*)val = out;
			break;
	}

	return { true, 0, 0 };
}
