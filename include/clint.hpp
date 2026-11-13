#pragma once
#include "structs.hpp"
#include <atomic>
#include <thread>

#define CLINT_MSWI_SIZE	  0x4000
#define CLINT_MTIMER_SIZE 0x8000
#define FREQ			  10000000
class CLINT : public Device
{
  public:
	CLINT(uint64_t base, uint64_t size, Hart* hart);
	~CLINT()
	{
		thr_working.store(false);
		if(thr.joinable()) thr.join();
	}

  private:
	// Memory-mapped registers
	std::vector<uint32_t> msip;		// one per HART, 32-bit
	std::vector<uint64_t> mtimecmp; // one per HART, 64-bit
	uint64_t mtime;					// global timer
	uint64_t begin;
	std::thread thr;
	std::atomic<bool> thr_working;
	uint64_t countr;

	Hart* hart;
	uint64_t read(uint64_t addr, MemorySize size);
	void write(uint64_t addr, MemorySize size, uint64_t val);

	uint64_t read_mswi(uint64_t offset);
	uint64_t read_mtimer(uint64_t offset);
	void write_mswi(uint64_t offset, uint64_t value);
	void write_mtimer(uint64_t offset, uint64_t value);
	void update_mip();
	void thread_func();
};
