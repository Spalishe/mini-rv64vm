#include "../include/uart.hpp"
#include "../include/plic.hpp"

UART::UART(uint64_t start, uint64_t size, PLIC* plic)
	: Device(start, size),
	  irq_num(1),
	  dlab(false),
	  dll(0),
	  dlm(0),
	  ier(0),
	  iir(0x01),
	  fcr(0),
	  fifo_enabled(false),
	  lcr(0),
	  mcr(0),
	  lsr(LSR_THR_EMPTY | LSR_TEMT),
	  msr(0),
	  scr(0),
	  thr(0),
	  rhr(0),
	  tx_irq_pending(false),
	  rx_irq_pending(false),
	  overrun_error(0),
	  plic(plic)
{
	plic->hart->mmap->add_region(start, size);
}

void UART::trigger_irq()
{
	plic->set_pending(irq_num, true);
}

void UART::clear_irq()
{
	plic->set_pending(irq_num, false);
}

uint8_t UART::calc_iir_locked()
{
	// Priority: 1 = RX timeout -> 2 = RX data available -> 3 = TX empty
	if((ier & 0x01) && (lsr & LSR_DATA_READY))
		return IIR_RX_AVAILABLE;
	if((ier & 0x02) && (lsr & LSR_THR_EMPTY))
		return IIR_THR_EMPTY;
	return IIR_NO_INT;
}

void UART::update_iir()
{
	uint8_t new_iir = calc_iir_locked();
	iir				= new_iir;

	bool irq_active = (new_iir != IIR_NO_INT);
	if(irq_active)
		trigger_irq();
	else
		clear_irq();

	if(irq_active)
	{
		if(new_iir == IIR_RX_AVAILABLE)
			rx_irq_pending = true;
		else if(new_iir == IIR_THR_EMPTY)
			tx_irq_pending = true;
	}
	else
	{
		rx_irq_pending = false;
		tx_irq_pending = false;
	}
}

uint64_t UART::read(uint64_t addr, MemorySize size)
{
	uint64_t offset = addr;
	uint8_t reg		= offset;
	uint64_t value	= 0;

	switch(reg)
	{
		[[likely]] case 0: // RHR (read) or DLL (if DLAB=1)
			if(dlab)
				value = dll;
			else
			{
				if(fifo_enabled)
				{
					if(!fifo_buffer.empty())
					{
						value = fifo_buffer.front();
						fifo_buffer.pop();
						if(fifo_buffer.empty())
							lsr &= ~LSR_DATA_READY;
					}
					else
						value = 0;
				}
				else
				{
					value = rhr;
					lsr &= ~LSR_DATA_READY;
				}
				// overrun reset after read
				if(lsr & LSR_OE)
					lsr &= ~LSR_OE;

				update_iir();
			}
			break;

		case 1: // IER (read) or DLM (if DLAB=1)
			if(dlab)
				value = dlm;
			else
				value = ier;
			break;

		case 2: // IIR (read)
			value = iir;
			if(value == IIR_RX_AVAILABLE && rx_irq_pending)
			{
				rx_irq_pending = false;
				if(calc_iir_locked() == IIR_NO_INT)
					update_iir();
			}
			else if(value == IIR_THR_EMPTY && tx_irq_pending)
			{
				tx_irq_pending = false;
				if(calc_iir_locked() == IIR_NO_INT)
					update_iir();
			}
			break;

		case 3: // LCR
			value = lcr;
			break;
		case 4: // MCR
			value = mcr;
			break;
		case 5: // LSR
			value = lsr;
			break;
		case 6: // MSR
			value = msr;
			break;
		case 7: // SCR
			value = scr;
			break;
		default:
			break;
	}
	return value;
}

void UART::write(uint64_t addr, MemorySize size, uint64_t value)
{
	uint64_t offset	   = addr;
	uint8_t reg		   = offset;
	uint8_t byte_value = value & 0xFF;

	switch(reg)
	{
		[[likely]] case 0: // THR (write) or DLL (if DLAB=1)
			if(dlab)
				dll = byte_value;
			else
			{
				thr = byte_value;
				fputc(thr, stdout);
				fflush(stdout);

				if((ier & 0x02) && !tx_irq_pending)
				{
					update_iir();
				}
				else
				{
					update_iir();
				}
			}
			break;

		case 1: // IER (write) or DLM (if DLAB=1)
			if(dlab)
				dlm = byte_value;
			else
			{
				ier = byte_value & 0x0F;
				update_iir();
			}
			break;

		case 2: // FCR (FIFO Control)
		{
			fcr			  = byte_value;
			bool old_fifo = fifo_enabled;
			fifo_enabled  = (fcr & 0x01);

			if(fifo_enabled && !old_fifo && (lsr & LSR_DATA_READY))
			{
				fifo_buffer.push(rhr);

				if(fifo_buffer.empty())
					lsr &= ~LSR_DATA_READY;
				else
					lsr |= LSR_DATA_READY;
			}

			else if(!fifo_enabled && old_fifo && !fifo_buffer.empty())
			{
				rhr = fifo_buffer.front();

				std::queue<uint8_t> empty;
				fifo_buffer.swap(empty);
				lsr |= LSR_DATA_READY;
			}

			// Clear fifo
			if(fcr & 0x02)
			{
				std::queue<uint8_t> empty;
				fifo_buffer.swap(empty);
				lsr &= ~LSR_DATA_READY;
				rx_irq_pending = false;
				update_iir();
			}
			if(fcr & 0x04)
			{
				// TX FIFO not emulating
			}
			break;
		}
		case 3: // LCR
			lcr	 = byte_value;
			dlab = (lcr & 0x80) != 0;
			break;

		case 4: // MCR
			mcr = byte_value;
			break;

		case 5: // LSR (read-only)
		case 6: // MSR (read-only)
			break;

		case 7: // SCR
			scr = byte_value;
			break;

		default:
			break;
	}
}

void UART::receive_byte(uint8_t byte)
{
	// FIFO overflow
	if(fifo_enabled && fifo_buffer.size() >= 512)
	{
		lsr |= LSR_OE; // Overrun Error
		return;
	}

	if(fifo_enabled)
	{
		fifo_buffer.push(byte);
		lsr |= LSR_DATA_READY;
	}
	else
	{
		if(lsr & LSR_DATA_READY)
			lsr |= LSR_OE;
		else
			rhr = byte;
		lsr |= LSR_DATA_READY;
	}

	update_iir();
}

bool UART::can_receive() const
{
	if(fifo_enabled)
		return fifo_buffer.size() < 512;
	return !(lsr & LSR_DATA_READY);
}
