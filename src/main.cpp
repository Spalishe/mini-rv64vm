#include "../include/clint.hpp"
#include "../include/console_input.hpp"
#include "../include/consts.hpp"
#include "../include/fields.hpp"
#include "../include/plic.hpp"
#include "../include/structs.hpp"
#include "../include/uart.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

std::vector<char> read_file(char* path)
{
	auto file = std::ifstream(path, std::ios::binary);
	if(file.fail())
	{
		std::cout << "File open error: " << path << ": " << strerror(errno) << std::endl;
		exit(1);
	}
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<char> buffer(size);
	if(size > 0)
	{
		file.read(buffer.data(), size);
	}
	if(file.fail() || file.gcount() != size)
	{
		std::cout << "File read error: " << path << " (read " << file.gcount() << " of " << size << " bytes)" << std::endl;
		exit(1);
	}
	return buffer;
}

static uint32_t read_be32(const char* p)
{
	return ((uint32_t)(uint8_t)p[0] << 24) | ((uint32_t)(uint8_t)p[1] << 16) | ((uint32_t)(uint8_t)p[2] << 8) | (uint32_t)(uint8_t)p[3];
}

int main(int argc, char* argv[])
{
	MemoryMap* mmap = new MemoryMap;
	mmap->add_region(memory_start, memory_size);

	MMIO* mmio = new MMIO;
	mmio->mmap = mmap;
	Hart core;
	core.mmap = mmap;
	core.mmio = mmio;

	CLINT clint = CLINT(0x02000000, 0x10000, &core);
	PLIC plic	= PLIC(0x0C000000, 0x400000, &core, 64, 1);
	UART uart	= UART(0x10000000, 0x100, &plic);

	mmio->add_device(&plic);
	mmio->add_device(&uart);
	mmio->add_device(&clint);

	// Load Firmware
	if(argc < 3)
	{
		std::cout << "Specify arguments: DTB path; Firmware path; Ramdisk path (optional, loaded at 0x84000000)" << std::endl;
		return 1;
	}

	uint8_t* buf = mmap->find_region(memory_start)->buf;

	char* dtb_path	  = argv[1];
	auto dtb		  = read_file(dtb_path);
	uint64_t dtb_offs = memory_size - 0x20000;

	if(dtb.size() < 8 || (uint8_t)dtb[0] != 0xd0 || (uint8_t)dtb[1] != 0x0d || (uint8_t)dtb[2] != 0xfe || (uint8_t)dtb[3] != 0xed)
	{
		std::cerr << "DTB error: " << dtb_path << " is not a flattened device tree (bad magic)." << std::endl;
		std::cerr << "Argument order is: DTB, firmware, optional ramdisk." << std::endl;
		return 1;
	}
	uint32_t dtb_total = read_be32(dtb.data() + 4);
	if((uint64_t)dtb_total != dtb.size())
	{
		std::cerr << "DTB error: header totalsize is " << dtb_total << " bytes, but the file is " << dtb.size() << " bytes." << std::endl;
		std::cerr << "The file was likely corrupted by CRLF conversion (git autocrlf or a text-mode copy)." << std::endl;
		std::cerr << "Mark *.dtb/*.bin as binary and re-checkout them, or copy them in binary mode." << std::endl;
		return 1;
	}

	memcpy(buf + dtb_offs, dtb.data(), dtb.size());

	core.regs[10]		   = 0; // MHARTID
	core.csrs[CSR_MHARTID] = 0;
	core.csrs[CSR_MISA]	   = (1ULL << 63) | 1 | (1 << 8) | (1 << 12) | (1 << 18); // RV64IMAS
	core.regs[11]		   = memory_start + dtb_offs;							  // DTB PATH

	char* bios_path = argv[2];
	auto bios		= read_file(bios_path);
	if(bios.empty() || bios.size() > memory_size)
	{
		std::cerr << "Firmware error: " << bios_path << " has invalid size " << bios.size() << " bytes." << std::endl;
		return 1;
	}

	static const uint8_t elf_magic[4] = { 0x7f, 'E', 'L', 'F' };
	static const uint8_t fdt_magic[4] = { 0xd0, 0x0d, 0xfe, 0xed };
	if(bios.size() >= 4 && memcmp(bios.data(), elf_magic, 4) == 0)
	{
		std::cerr << "Firmware error: " << bios_path << " is an ELF file, not a raw binary." << std::endl;
		std::cerr << "The VM executes raw bytes from 0x" << std::hex << memory_start << std::dec << "; use objcopy -O binary first." << std::endl;
		return 1;
	}
	if(bios.size() >= 4 && memcmp(bios.data(), fdt_magic, 4) == 0)
	{
		std::cerr << "Firmware error: " << bios_path << " looks like a device tree. Arguments are probably swapped." << std::endl;
		std::cerr << "Argument order is: DTB, firmware, optional ramdisk." << std::endl;
		return 1;
	}

	memcpy(buf, bios.data(), bios.size());

	std::cerr << "Loaded DTB: " << dtb.size() << " bytes, firmware: " << bios.size() << " bytes" << std::endl;

	if(argc >= 4)
	{
		char* rd_path = argv[3];
		auto rd		  = read_file(rd_path);
		if(rd.size() > memory_size - 0x4000000)
		{
			std::cerr << "Ramdisk error: " << rd_path << " is too large (" << rd.size() << " bytes)." << std::endl;
			return 1;
		}
		memcpy(buf + 0x4000000, rd.data(), rd.size());
		std::cerr << "Loaded ramdisk: " << rd.size() << " bytes" << std::endl;
	}

	ConsoleInput input;
	uint64_t ticks = 0;

	while(1)
	{
		if((++ticks & 0x3FF) == 0)
		{
			uint8_t byte;
			while(uart.can_receive() && input.poll(byte))
				uart.receive_byte(byte);
		}

		core.tick();
	}

	return 0;
}
