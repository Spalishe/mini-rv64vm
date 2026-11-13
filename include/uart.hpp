#pragma once
#include "structs.hpp"
#include <queue>

#define IIR_NO_INT		 0x01
#define IIR_THR_EMPTY	 0x02
#define IIR_RX_AVAILABLE 0x04

// LSR bits
#define LSR_DATA_READY	 0x01
#define LSR_THR_EMPTY	 0x20
#define LSR_TEMT		 0x40
#define LSR_OE			 0x02
class PLIC;

class UART : public Device
{
  public:
	UART(uint64_t start, uint64_t size, PLIC* plic);
	void receive_byte(uint8_t byte);
	bool can_receive() const;

  private:
	// UART registers
	uint8_t rhr = 0;	// Receiver Holding Register (read)
	uint8_t thr = 0;	// Transmitter Holding Register (write)
	uint8_t ier = 0;	// Interrupt Enable Register
	uint8_t iir = 0x01; // Interrupt Identification Register (no interrupt pending)
	uint8_t fcr = 0;	// FIFO Control Register
	uint8_t lcr = 0;	// Line Control Register
	uint8_t mcr = 0;	// Modem Control Register
	uint8_t lsr = 0x60; // Line Status Register (THR empty + line idle)
	uint8_t msr = 0;	// Modem Status Register
	uint8_t scr = 0;	// Scratch Register

	// Divisor latch registers (when LCR[7] = 1)
	uint8_t dll = 0; // Divisor Latch Low
	uint8_t dlm = 0; // Divisor Latch High

	uint8_t overrun_error = 0;
	bool rx_irq_pending	  = false;
	bool tx_irq_pending	  = false;

	PLIC* plic;
	int irq_num;
	bool dlab = false; // Divisor Latch Access Bit (from LCR[7])

	bool fifo_enabled;
	std::queue<uint8_t> fifo_buffer;

	void trigger_irq();
	void clear_irq();
	void update_iir();
	uint8_t calc_iir_locked();
	uint64_t read(uint64_t addr, MemorySize size);
	void write(uint64_t addr, MemorySize size, uint64_t value);
	void reset();
};
