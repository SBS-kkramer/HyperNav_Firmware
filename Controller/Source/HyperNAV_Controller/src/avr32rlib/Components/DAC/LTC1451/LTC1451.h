/*!	\file 	LTC1451.h
 *	\brief	LTC1451 12-bit voltage-output DAC
 *
 *	@author		Scott Feener, Satlantic Inc.
 *	@date		2011-08-09
 *
 * History:
 * 	2011-08-09, SF: File started.
 */

#ifndef LTC1451_H_
#define LTC1451_H_

#include "compiler.h"

#define LTC1451_MAX_COUNTS	4095

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
void LTC1451_UpdateDAC(U16 counts, bool makeAtomic);


#endif /* LTC1451_H_ */
