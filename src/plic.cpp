#include "../include/plic.hpp"
#include "../include/fields.hpp"

PLIC::PLIC(uint64_t start, uint64_t size, Hart* hart, uint32_t max_sources, uint32_t num_harts)
	: Device(start, size),
	  num_contexts(num_harts * 2),
	  max_sources(max_sources),
	  hart(hart)
{
	hart->mmap->add_region(start, size);

	pending_words_count = (max_sources + 31) / 32;
	enable_words_count	= pending_words_count;

	priorities.resize(max_sources + 1, 0);
	pending_words.resize(pending_words_count, 0);

	contexts.resize(num_contexts);
	for(uint32_t i = 0; i < num_contexts; ++i)
	{
		contexts[i].enables.resize(enable_words_count, 0);
		contexts[i].threshold	   = 0;
		contexts[i].interrupt_line = false;

		contexts[i].hart_id			= static_cast<int>(i / 2);
		contexts[i].supervisor_mode = (i % 2) == 1;
	}
}

void PLIC::set_pending(uint32_t source_id, bool pending)
{
	if(source_id == 0 || source_id > max_sources)
		return;

	uint32_t word_idx = source_id / 32;
	uint32_t bit_mask = 1U << (source_id % 32);

	if(pending)
	{
		pending_words[word_idx] |= bit_mask;
	}
	else
		pending_words[word_idx] &= ~bit_mask;

	update_all_contexts();
}

uint64_t PLIC::read(uint64_t addr, MemorySize size)
{
	if(size != MemorySize::Word) return 0;

	uint64_t offset = addr;

	// Priority registers
	if(offset < 0x1000)
	{
		uint32_t idx = static_cast<uint32_t>(offset / 4);
		if(idx >= 1 && idx <= max_sources)
			return priorities[idx];
		return 0;
	}

	// Pending (read-only)
	if(offset >= 0x1000 && offset < 0x1080)
	{
		uint32_t word_idx = static_cast<uint32_t>((offset - 0x1000) / 4);
		if(word_idx < pending_words_count)
			return pending_words[word_idx];
		return 0;
	}

	// Enable registers
	if(offset >= 0x2000 && offset < 0x200000)
	{
		uint32_t ctx_idx = static_cast<uint32_t>((offset - 0x2000) / 0x80);
		if(ctx_idx >= num_contexts) return 0;

		uint32_t word_offset = static_cast<uint32_t>(((offset - 0x2000) % 0x80) / 4);
		if(word_offset < enable_words_count)
			return contexts[ctx_idx].enables[word_offset];
		return 0;
	}

	// Threshold and Claim
	[[likely]] if(offset >= 0x200000 && offset < 0x200000 + num_contexts * 0x1000)
	{
		uint32_t ctx_idx = static_cast<uint32_t>((offset - 0x200000) / 0x1000);
		if(ctx_idx >= num_contexts) return 0;

		uint32_t sub_offset = static_cast<uint32_t>((offset - 0x200000) % 0x1000);
		if(sub_offset == 0)
		{
			return contexts[ctx_idx].threshold;
		}
		else if(sub_offset == 4)
		{
			return claim_interrupt(ctx_idx);
		}
	}
	return 0;
}

void PLIC::write(uint64_t addr, MemorySize size, uint64_t value)
{
	if(size != MemorySize::Word) return;

	uint64_t offset = addr;

	// Priority
	if(offset < 0x1000)
	{
		uint32_t idx = static_cast<uint32_t>(offset / 4);
		if(idx >= 1 && idx <= max_sources)
		{
			priorities[idx] = static_cast<uint32_t>(value & 0x7FFFFFFF);
			update_all_contexts();
		}
		return;
	}

	// Pending is read-only
	if(offset >= 0x1000 && offset < 0x1080) return;

	// Enable
	if(offset >= 0x2000 && offset < 0x200000)
	{
		uint32_t ctx_idx = static_cast<uint32_t>((offset - 0x2000) / 0x80);
		if(ctx_idx >= num_contexts) return;

		uint32_t word_offset = static_cast<uint32_t>(((offset - 0x2000) % 0x80) / 4);
		if(word_offset < enable_words_count)
		{
			contexts[ctx_idx].enables[word_offset] = static_cast<uint32_t>(value);
			update_context(ctx_idx);
		}
		return;
	}

	// Threshold and Complete
	[[likely]] if(offset >= 0x200000 && offset < 0x200000 + num_contexts * 0x1000)
	{
		uint32_t ctx_idx = static_cast<uint32_t>((offset - 0x200000) / 0x1000);
		if(ctx_idx >= num_contexts) return;

		uint32_t sub_offset = static_cast<uint32_t>((offset - 0x200000) % 0x1000);
		if(sub_offset == 0)
		{
			contexts[ctx_idx].threshold = static_cast<uint32_t>(value & 0x7FFFFFFF);
			update_context(ctx_idx);
		}
		else if(sub_offset == 4)
		{
			complete_interrupt(ctx_idx, static_cast<uint32_t>(value));
		}
		return;
	}
}

uint32_t PLIC::find_highest_pending_enabled(uint32_t ctx_idx)
{
	const Context& ctx	   = contexts[ctx_idx];
	uint32_t best_id	   = 0;
	uint32_t best_priority = 0;

	uint32_t word_count = (max_sources + 31) / 32;
	for(uint32_t word = 0; word < word_count; ++word)
	{
		uint32_t active = pending_words[word] & ctx.enables[word];

		while(active)
		{
			uint32_t bit = std::countr_zero(active);
			uint32_t id	 = word * 32 + bit;

			if(id != 0 && id <= max_sources)
			{
				uint32_t prio = priorities[id];

				if(prio > ctx.threshold)
				{
					if(prio > best_priority || (prio == best_priority && id < best_id))
					{
						best_priority = prio;
						best_id		  = id;
					}
				}
			}

			active &= active - 1;
		}
	}
	return best_id;
}

uint32_t PLIC::claim_interrupt(uint32_t ctx_idx)
{
	uint32_t id = find_highest_pending_enabled(ctx_idx);
	if(id != 0)
	{
		uint32_t word = id / 32;
		uint32_t bit  = id % 32;
		pending_words[word] &= ~(1U << bit);
		update_context(ctx_idx);
	}
	return id;
}

void PLIC::complete_interrupt(uint32_t ctx_idx, uint32_t id)
{
	if(id == 0 || id > max_sources) return;
	uint32_t word = id / 32;
	uint32_t bit  = id % 32;
	pending_words[word] &= ~(1U << bit);
	update_context(ctx_idx);
}

void PLIC::update_context(uint32_t ctx_idx)
{
	Context& ctx	  = contexts[ctx_idx];
	bool should_raise = (find_highest_pending_enabled(ctx_idx) != 0);
	if(should_raise != ctx.interrupt_line)
	{
		ctx.interrupt_line = should_raise;
		signal_cpu_interrupt(ctx.supervisor_mode, should_raise);
	}
}

void PLIC::update_all_contexts()
{
	for(uint32_t i = 0; i < num_contexts; ++i)
		update_context(i);
}

void PLIC::signal_cpu_interrupt(bool supervisor_mode, bool level)
{
	const uint64_t MEIP_BIT = 1ULL << 11;
	const uint64_t SEIP_BIT = 1ULL << 9;

	if(supervisor_mode)
	{
		if(level)
			hart->csrs[CSR_MIP] |= (1 << 9);
		else
			hart->csrs[CSR_MIP] &= ~(1 << 9);
	}
	else
	{
		if(level)
			hart->csrs[CSR_MIP] |= (1 << 11);
		else
			hart->csrs[CSR_MIP] &= ~(1 << 11);
	}
}
