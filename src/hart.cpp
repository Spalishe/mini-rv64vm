#include "../include/consts.hpp"
#include "../include/fields.hpp"
#include "../include/structs.hpp"

Hart::Hart()
{
	pc				   = memory_start;
	csrs[CSR_STIMECMP] = UINT64_MAX;
}

void Hart::tick()
{
	regs[0] = 0; // GND
	if(check_for_interrupts()) [[unlikely]]
	{
		return;
	}
	if(WFI) [[unlikely]]
	{
		// WFI must wake for pending locally-enabled interrupts even when
		// interrupts are globally disabled (e.g. default_idle_call).
		if((csrs[CSR_MIP] & csrs[CSR_MIE]) == 0)
			return;
		WFI = false;
	}
	uint32_t inst			= mmap->load(pc, MemorySize::Word);
	InstructionCache& cache = decoder.decode(pc, inst);

	if(cache.valid == false)
	{
		trap(2, inst, false); // Cause ILLEGAL INSTRUCTION exception with tval of our faulty instruction
		return;
	}
	csrs[CSR_CYCLE]++;
	ExecReturn out = cache.inst->func(*this, cache.data);
	if(out.is_success)
	{
		// increase pc
		csrs[CSR_INSTRET]++;
		if(!out.forbid_change_pc) pc += out.increase_pc;
	}
	else
	{
		trap(out.cause, out.tval, false);
	}
}

bool Hart::check_for_interrupts()
{
	uint64_t pending_all = csrs[CSR_MIP] & csrs[CSR_MIE];
	if(!pending_all) [[likely]]
		return false;

	uint64_t mideleg = csrs[CSR_MIDELEG];

	bool m_global = (mode == PrivilegeMode::Machine) ? (csrs[CSR_MSTATUS] >> 3 & 1) : (mode < PrivilegeMode::Machine);
	if(m_global)
	{
		uint64_t pending_m = pending_all & ~mideleg;
		if(pending_m) [[likely]]
		{
			int irq = 63 - std::countl_zero(pending_m);
			trap(irq, 0, true);
			return true;
		}
	}

	bool s_global = (mode == PrivilegeMode::Supervisor) ? (csrs[CSR_MSTATUS] >> 1 & 1) : (mode < PrivilegeMode::Supervisor);
	if(s_global)
	{
		uint64_t pending_s = (csrs[CSR_MIP] & SE_MASK & mideleg) & (csrs[CSR_MIE] & SE_MASK);
		if(pending_s) [[likely]]
		{
			int irq = 63 - std::countl_zero(pending_s);
			trap(irq, 0, true);
			return true;
		}
	}

	return false;
}

void Hart::trap(uint64_t cause, uint64_t tval, bool is_interrupt)
{
	WFI						= false;
	uint64_t trap_pc		= pc;
	PrivilegeMode prev_mode = mode;

	uint64_t medeleg   = csrs[CSR_MEDELEG];
	uint64_t mideleg   = csrs[CSR_MIDELEG];
	bool delegate_to_s = false;
	if(prev_mode != PrivilegeMode::Machine)
	{
		if(is_interrupt)
			delegate_to_s = (mideleg >> cause) & 1ULL;
		else
			delegate_to_s = (medeleg >> cause) & 1ULL;
	}

	csrs[CSR_MSTATUS] &= ~(1ULL << 17); // MPRV = 0
	if(delegate_to_s)
	{
		// Supervisor
		mode			 = PrivilegeMode::Supervisor;
		uint64_t vector	 = (((csrs[CSR_STVEC] & 3ULL) == 1 && is_interrupt) ? 4 * cause : 0);
		pc				 = (csrs[CSR_STVEC] & ~3ULL) + vector;
		csrs[CSR_SEPC]	 = trap_pc;
		csrs[CSR_SCAUSE] = ((is_interrupt ? (1ULL << 63) : 0) | cause);
		csrs[CSR_STVAL]	 = tval;

		uint64_t sie = (csrs[CSR_MSTATUS] >> 1) & 1ULL;
		csrs[CSR_MSTATUS] &= ~((1ULL << 5) | (1ULL << 1) | (1ULL << 8));
		csrs[CSR_MSTATUS] |= (sie << 5);
		uint64_t spp = (prev_mode != PrivilegeMode::User) ? 1ULL : 0ULL;
		csrs[CSR_MSTATUS] |= (spp << 8);
	}
	else
	{
		// Machine
		mode			 = PrivilegeMode::Machine;
		uint64_t vector	 = (((csrs[CSR_MTVEC] & 3ULL) == 1 && is_interrupt) ? 4 * cause : 0);
		pc				 = (csrs[CSR_MTVEC] & ~3ULL) + vector;
		csrs[CSR_MEPC]	 = trap_pc;
		csrs[CSR_MCAUSE] = ((is_interrupt ? (1ULL << 63) : 0) | cause);
		csrs[CSR_MTVAL]	 = tval;
		uint64_t mie	 = (csrs[CSR_MSTATUS] >> 3) & 1ULL;
		csrs[CSR_MSTATUS] &= ~((1ULL << 7) | (1ULL << 3) | (3ULL << 11));
		csrs[CSR_MSTATUS] |= (mie << 7);				  // MPIE = mie
		csrs[CSR_MSTATUS] |= ((uint64_t)prev_mode << 11); // MPP = prev_mode
	}
}

void Hart::csr_write(uint16_t csr, uint64_t val)
{
	switch(csr)
	{
		case CSR_MSTATUS:
			csrs[CSR_MSTATUS] = (csrs[CSR_MSTATUS] & MSTATUS_RO_MASK) | (val & ~MSTATUS_RO_MASK);
			break;
		case CSR_SSTATUS:
			csrs[CSR_MSTATUS] = (csrs[CSR_MSTATUS] & ~SSTATUS_MASK) | (val & SSTATUS_MASK);
			break;
		case CSR_SIE:
			csrs[CSR_MIE] = (csrs[CSR_MIE] & ~SE_MASK) | (val & SE_MASK);
			break;
		case CSR_SIP:
			csrs[CSR_MIP] = (csrs[CSR_MIP] & ~SE_MASK) | (val & SE_MASK);
			break;
		case CSR_MVENDORID:
		case CSR_MARCHID:
		case CSR_MIMPID:
			// Read-only
			(void)csr;
			(void)val;
			break;
		default:
			csrs[csr] = val;
	}
}
uint64_t Hart::csr_read(uint16_t csr)
{
	switch(csr)
	{
		case CSR_MSTATUS:
			return csrs[CSR_MSTATUS];
		case CSR_SSTATUS:
			return csrs[CSR_MSTATUS] & SSTATUS_MASK;
		case CSR_SIE:
			return csrs[CSR_MIE] & SE_MASK;
		case CSR_SIP:
			return csrs[CSR_MIP] & SE_MASK;
		case CSR_MCYCLE:
			return csrs[CSR_CYCLE];
		case CSR_TIME:
			return ctime;
		case CSR_INSTRET:
		case CSR_MINSTRET:
			return csrs[CSR_INSTRET];
		default:
			return csrs[csr];
	}
}
