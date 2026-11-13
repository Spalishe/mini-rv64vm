#pragma once
#include <cstdint>
#include <memory>
#include <vector>

enum class MemorySize
{
	Byte,
	Halfword,
	Word,
	Doubleword
};

enum class PrivilegeMode : int
{
	User	   = 0,
	Supervisor = 1,
	Machine	   = 3
};

struct MemoryReturn
{
	bool is_success = true;
	char exc_code	= 0;
	uint64_t tval	= 0;
};

struct ExecReturn
{
	bool is_success		  = true;
	bool forbid_change_pc = false;
	char increase_pc	  = 4;
	char cause			  = 0;
	uint64_t tval		  = 0;
};

struct MemoryRegion
{
	uint64_t start;
	uint64_t size;
	uint8_t* buf = nullptr;

	MemoryRegion(MemoryRegion&& other) noexcept
		: buf(other.buf), size(other.size), start(other.start)
	{
		other.start = 0;
		other.buf	= nullptr;
		other.size	= 0;
	}

	MemoryRegion& operator=(MemoryRegion&& other) noexcept
	{
		if(this != &other)
		{
			if(buf) delete[] buf;

			buf	 = other.buf;
			size = other.size;

			other.buf  = nullptr;
			other.size = 0;
		}
		return *this;
	}

	MemoryRegion(uint64_t start, uint64_t size) : start(start), size(size)
	{
		buf = new uint8_t[size];
	}
	~MemoryRegion()
	{
		if(buf != nullptr)
		{
			delete[] buf;
		}
	}
	uint8_t* get_ptr(uint64_t address)
	{
		if(address < start || address > (start + size))
		{
			return 0;
		}
		return (uint8_t*)buf + (address - start);
	}
};
struct MemoryMap
{
	uint64_t load(uint64_t address, MemorySize size);
	void store(uint64_t address, MemorySize size, uint64_t value);
	MemoryRegion* find_region(uint64_t address);

	void add_region(uint64_t start_address, uint64_t size)
	{
		regions.push_back(std::make_unique<MemoryRegion>(start_address, size));
	}
	std::vector<std::unique_ptr<MemoryRegion>> regions;
};

struct Device
{
	uint64_t start;
	uint64_t size;
	Device(uint64_t start, uint64_t size) : start(start), size(size)
	{
	}
	virtual uint64_t read(uint64_t offset, MemorySize size)			   = 0;
	virtual void write(uint64_t offset, MemorySize size, uint64_t val) = 0;
	virtual ~Device()												   = default;
};

struct Hart; // forward declaration
struct MMIO
{
	MemoryReturn load(Hart* hart, uint64_t address, MemorySize size, void* val);
	MemoryReturn store(Hart* hart, uint64_t address, MemorySize size, uint64_t value);

	MemoryMap* mmap = nullptr;
	std::vector<Device*> devices;
	template <typename T>
	void add_device(T* dev)
	{
		devices.push_back(dev);
	}
};

struct InstructionData
{
	uint8_t rd;
	uint8_t rs1;
	uint8_t rs2;
	uint64_t imm;
	uint32_t inst;
};
struct Instruction
{
	uint32_t mask;
	uint32_t match;
	ExecReturn (*func)(Hart& h, InstructionData& data);
	uint64_t (*immfunc)(uint32_t inst);
};

struct InstructionCache
{
	Instruction* inst;
	InstructionData data;
	uint64_t pc;
	bool valid = false;
};

struct Reservation
{
	uint64_t addr;
	MemorySize size;
	bool valid;
};

struct InstructionDecoder
{
	InstructionDecoder();
	~InstructionDecoder()
	{
		delete[] cache;
	}
	static constexpr int CACHE_SIZE = 1 << 16;
	std::vector<Instruction> insts;
	InstructionCache* cache;
	void add_instruction(std::string instruction_mask, ExecReturn (*func)(Hart& h, InstructionData& data), uint64_t (*immfunc)(uint32_t inst) = nullptr);
	InstructionCache& decode(uint64_t pc, uint32_t inst);

	void init_rv64i();
	void init_rv64m();
	void init_rv64a();
	void init_zicsr();
	void init_zifencei();
	void init_priv();

	static inline int32_t sext(uint32_t val, int bits)
	{
		int32_t shift = 32 - bits;
		return (int32_t)(val << shift) >> shift;
	}
	static inline uint64_t imm_Zicsr(uint32_t inst)
	{
		return (inst >> 20);
	}
	static inline uint64_t imm_I(uint32_t inst)
	{
		return sext(inst >> 20, 12);
	}
	static inline uint64_t imm_S(uint32_t inst)
	{
		uint32_t imm = ((inst >> 7) & 0x1f)
					   | ((inst >> 20) & 0xfe0);

		return sext(imm, 12);
	}
	static inline uint64_t imm_B(uint32_t inst)
	{
		return sext(((inst & 0x00000F00) >> 7) | ((inst & 0x7E000000) >> 20) | ((inst & 0x00000080) << 4) | ((inst & 0x80000000) >> 19),
					13);
	}
	static inline uint64_t imm_U(uint32_t inst)
	{
		return (int64_t)(int32_t)(inst & 0xFFFFF000u);
	}
	static inline uint64_t imm_J(uint32_t inst)
	{
		return sext(((inst & 0x7FE00000) >> 20) | ((inst & 0x00100000) >> 9) | ((inst & 0x000FF000)) | ((inst & 0x80000000) >> 11),
					21);
	}
	static inline uint64_t shamt(uint32_t inst)
	{
		return (uint32_t)(imm_I(inst) & 0x1f);
	}
	static inline uint64_t shamt64(uint32_t inst)
	{
		return (uint32_t)(imm_I(inst) & 0x3f);
	}
};

struct Hart
{
	Hart();
	uint64_t pc;
	uint64_t regs[32];
	MMIO* mmio		= nullptr;
	MemoryMap* mmap = nullptr;
	InstructionDecoder decoder;
	void tick();
	PrivilegeMode mode = PrivilegeMode::Machine;
	Reservation reservation;

	bool WFI = false;
	uint64_t csrs[4096];
	uint64_t ctime;

	void trap(uint64_t cause, uint64_t tval, bool is_interrupt);
	bool check_for_interrupts();

	uint64_t csr_read(uint16_t csr);
	void csr_write(uint16_t csr, uint64_t val);
	inline void amo_check_reservation(uint64_t pa)
	{
		if(reservation.valid && reservation.addr >= pa && pa <= reservation.addr + (int)reservation.size)
		{
			reservation.valid = false;
		}
	}
};
