/*!	\file 	LTC1451.c
 *	\brief	LTC1451 12-bit voltage-output DAC
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
#include "LTC1451.h"
#include "LTC1451_cfg.h"

#ifdef FREERTOS_USED
	#include "FreeRTOS.h"
	#include "task.h"
	//#include "queue.h"
	//#include "semphr.h"
#endif


/*! \brief	Sends a 16-bit value to the DAC
 *
 * 	The LTC1451 only requires 12-bits of data.
 * 	While the AVR32 has a configurable number of data bits per transfer,
 *  this function assumes 8 or 16 bit transfers.
 *  The bits are clocked in MSB first, so the top 4 bits are lost.
 *
 * 	\param	counts		The 16-bit data to transfer.  Only the 12 lsbs are used.
 * 	\param 	makeAtomic	If true, interrupts are disabled during the transfer to prevent corruption
 */
void LTC1451_UpdateDAC(U16 counts, bool makeAtomic)
{
	U16 dataIO;
#ifndef FREERTOS_USED
	bool ints_enabled = Is_global_interrupt_enabled();
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
	spi_selectChip(LTC1451_SPI, LTC1451_NPCS);

#if LTC1451_SPI_BITS == 8
		// transfer high byte
		spi_write(LTC1451_SPI, counts >> 8);
		spi_read(LTC1451_SPI, &dataIO);	// Read to prevent an SPI overrun
		// transfer low byte
		spi_write(LTC1451_SPI, counts);
		spi_read(LTC1451_SPI, &dataIO);	// Read to prevent an SPI overrun
#elif LTC1451_SPI_BITS == 16
		// transfer all 16 bits
		spi_write(LTC1451_SPI, counts);
		spi_read(LTC1451_SPI, &dataIO);	// Read to prevent an SPI overrun
#else
	#error "Invalid LTC1451_SPI_BITS setting"
#endif

	// Unselect chip
	spi_unselectChip(LTC1451_SPI, LTC1451_NPCS);

	if (makeAtomic) {
#ifdef FREERTOS_USED
		taskEXIT_CRITICAL();	// end critical section
#else
		// re-enable interrupts if they were enabled on entry
		if (ints_enabled)	Enable_global_interrupt();
#endif
	}
}
