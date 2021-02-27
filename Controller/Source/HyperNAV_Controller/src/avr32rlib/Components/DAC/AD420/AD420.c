/*!	\file 	AD420.c
 *	\brief	AD420 16-bit current-output (4-20 mA, 0-20 mA, or 0-24 mA) DAC
 *
 *	@author		Scott Feener, Satlantic Inc.
 *	@date		2011-08-09
 *
 * History:
 * 	2011-08-09, SF: File started.
 */

#include "compiler.h"
#include "dbg.h"
#include "spi.h"
#include "AD420.h"
#include "AD420_cfg.h"
#include "gpio.h"
#include "cycle_counter.h"

#ifdef FREERTOS_USED
	#include "FreeRTOS.h"
	#include "task.h"
	//#include "queue.h"
	//#include "semphr.h"
#endif


/*! \brief	Sends a 16-bit value to the DAC
 *
 *	The AD420 is a 16-bit current output DAC that can be configured for
 *	4-20 mA, 0-20 mA, or 0-24 mA output.  It also supports a voltage output
 *	when supported with an external buffer.  The output mode is selected
 *	by pin configuration on the PCB.
 *
 *	Presently, this firmware expects 4-20 mA output.
 *
 *	The coding is 16-bit straight binary, i.e. when configured for 4 - 20 mA output
 *
 *	Code 	Output Current
 *	----	--------------
 *	0x0000	4 mA
 *	0x0001	4.00024 mA
 *	...
 *	0x8000	8 mA
 *	...
 *	0xFFFF	19.99976 mA
 *
 *  This function supports 8 or 16 bit SPI transfers via compile time option.
 *
 * 	\param	counts		The 16-bit data to transfer.
 * 	\param 	makeAtomic	If true, interrupts are disabled during the transfer to prevent corruption
 */
void AD420_UpdateDAC(U16 counts, bool makeAtomic)
{
	U16 dataIO;
#ifndef FREERTOS_USED
	bool ints_enabled = Is_global_interrupt_enabled();
#endif

#if ((AD420_I_RANGE != AD420_4_20MA)&&(AD420_I_RANGE != AD420_0_20MA)&&(AD420_I_RANGE != AD420_0_24MA))
#error "Invalid AD420_I_RANGE setting"
#endif

	/*
	 * A atomic code block (critical section) that disables interrupts
	 * should be used here to prevent accidental clocking and/or latching
	 * of invalid SPI data (by accessing other SPI devices).
	 */
	if (makeAtomic) {
#ifdef FREERTOS_USED
		taskENTER_CRITICAL();	// start critical section
#else
		// disable interrupts if they were enabled on entry
		if (ints_enabled)	Disable_global_interrupt();
#endif
	}

	// Select chip
	spi_selectChip(AD420_SPI, AD420_NPCS);

#if AD420_SPI_BITS == 8
		// transfer high byte
		spi_write(AD420_SPI, counts >> 8);
		spi_read(AD420_SPI, &dataIO);	// Read to prevent an SPI overrun
		// transfer low byte
		spi_write(AD420_SPI, counts);
		spi_read(AD420_SPI, &dataIO);	// Read to prevent an SPI overrun
#elif AD420_SPI_BITS == 16
		// transfer all 16 bits
		spi_write(AD420_SPI, counts);
		spi_read(AD420_SPI, &dataIO);	// Read to prevent an SPI overrun
#else
	#error "Invalid AD420_SPI_BITS setting"
#endif

	// Unselect chip
	spi_unselectChip(AD420_SPI, AD420_NPCS);

	if (makeAtomic) {
#ifdef FREERTOS_USED
		taskEXIT_CRITICAL();	// end critical section
#else
		// re-enable interrupts if they were enabled on entry
		if (ints_enabled)	Enable_global_interrupt();
#endif
	}
}

void AD420_Clear(void)
{
	gpio_set_gpio_pin(AD420_CLEAR);
	cpu_delay_us(1, FOSC0);		// minimum 50 ns required
	gpio_clr_gpio_pin(AD420_CLEAR);
}

