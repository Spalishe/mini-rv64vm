#include "../include/clint.hpp"
#include "../include/fields.hpp"
#include "chrono"

inline uint64_t timer_clocksource(uint64_t freq)
{
	auto now			 = std::chrono::steady_clock::now();
	auto duration		 = now.time_since_epoch();
	uint64_t ns			 = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
	// Split the conversion so `ns * freq` cannot overflow uint64 on long-running
	// hosts (uptime * freq must stay below 2^64; <= 5e6 Hz that is ~114k years).
	constexpr uint64_t B = 1000000000ULL;
	return (ns / B) * freq + (ns % B) * freq / B;
}

CLINT::CLINT(uint64_t start, uint64_t size, Hart* hart)
	: Device(start, size),
	  msip(1, 0),
	  mtimecmp(1),
	  hart(hart)
{
	mtime = 0;
	hart->mmap->add_region(start, size);
	thr_working.store(true);
	thr	   = std::thread(&CLINT::thread_func, this);
	countr = 0;
	begin  = timer_clocksource(FREQ);
}

void CLINT::thread_func()
{
	while(thr_working.load(std::memory_order_acquire))
	{
		mtime = timer_clocksource(FREQ) - begin;
		update_mip();
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

uint64_t CLINT::read(uint64_t addr, MemorySize size)
{
	uint64_t off = addr;

	if(off < CLINT_MSWI_SIZE)
	{
		return read_mswi(off);
	}
	else if(off >= CLINT_MSWI_SIZE && off < CLINT_MSWI_SIZE + CLINT_MTIMER_SIZE) [[likely]]
	{
		return read_mtimer(off - CLINT_MSWI_SIZE);
	}

	return 0;
}

void CLINT::write(uint64_t addr, MemorySize size, uint64_t value)
{
	uint64_t off = addr;

	if(off < CLINT_MSWI_SIZE)
	{
		write_mswi(off, value);
	}
	else if(off >= CLINT_MSWI_SIZE && off < CLINT_MSWI_SIZE + CLINT_MTIMER_SIZE)
	{
		write_mtimer(off - CLINT_MSWI_SIZE, value);
	}
}

uint64_t CLINT::read_mswi(uint64_t offset)
{
	uint64_t hart_id = offset >> 2;
	return msip[hart_id];
}
uint64_t CLINT::read_mtimer(uint64_t offset)
{
	uint64_t hart_id = offset >> 3;
	if(offset == 0x7FF8)
	{
		return mtime;
	}
	return mtimecmp[hart_id];
}
void CLINT::write_mswi(uint64_t offset, uint64_t value)
{
	uint64_t hart_id = offset >> 2;
	msip[hart_id]	 = value;
	update_mip();
}
void CLINT::write_mtimer(uint64_t offset, uint64_t value)
{
	uint64_t hart_id = offset >> 3;
	if(offset == 0x7FF8)
	{
		mtime = value;
		update_mip();
		return;
	}

	mtimecmp[hart_id] = value;
	update_mip();
}

void CLINT::update_mip()
{
	uint64_t now = mtime;

	hart->ctime = now;

	if(msip[0] & 1)
		hart->csrs[CSR_MIP] |= (1 << 3);
	else
		hart->csrs[CSR_MIP] &= ~(1 << 3);

	if(now >= mtimecmp[0])
		hart->csrs[CSR_MIP] |= (1 << 7);
	else
		hart->csrs[CSR_MIP] &= ~(1 << 7);

	uint64_t stimecmp = hart->csrs[CSR_STIMECMP];
	if(now >= stimecmp && stimecmp != UINT64_MAX)
		hart->csrs[CSR_MIP] |= (1 << 5);
	else
		hart->csrs[CSR_MIP] &= ~(1 << 5);
}
