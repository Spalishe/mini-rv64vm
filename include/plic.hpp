#pragma once
#include "structs.hpp"

class PLIC : public Device
{
  public:
	PLIC(uint64_t start, uint64_t size, Hart* hart, uint32_t max_sources, uint32_t num_harts);

	void set_pending(uint32_t source_id, bool pending);

	uint64_t read(uint64_t addr, MemorySize size);
	void write(uint64_t addr, MemorySize size, uint64_t val);
	Hart* hart;

  private:
	struct Context
	{
		uint32_t threshold = 0;
		std::vector<uint32_t> enables;
		int hart_id			 = -1;
		bool supervisor_mode = false;
		bool interrupt_line	 = false;
	};

	uint32_t max_sources;
	uint32_t num_harts;
	uint32_t num_contexts;
	uint32_t pending_words_count;
	uint32_t enable_words_count;

	std::vector<uint32_t> priorities; // indexes 0..max_sources, 0 not used
	std::vector<uint32_t> pending_words;
	std::vector<Context> contexts;

	uint32_t find_highest_pending_enabled(uint32_t ctx_idx);

	// Claim: returns irq ID and resets it pending bit
	uint32_t claim_interrupt(uint32_t ctx_idx);

	// Complete: resets pending bit to complete irq
	void complete_interrupt(uint32_t ctx_idx, uint32_t id);

	void update_context(uint32_t ctx_idx);

	void update_all_contexts();

	void signal_cpu_interrupt(bool supervisor_mode, bool level);
};
