/*!	\file 	AD420.h
 *	\brief	AD420 16-bit current-output (4-20 mA, 0-20 mA, or 0-24 mA) DAC
 *
 *	@author		Scott Feener, Satlantic Inc.
 *	@date		2011-08-09
 *
 * History:
 * 	2011-08-09, SF: File started.
 */


#ifndef AD420_H_
#define AD420_H_


#define AD420_MAX_COUNTS	65535

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
void AD420_UpdateDAC(U16 counts, bool makeAtomic);

/*!	\brief Unconditionally sets the AD420 output to minimum
 *
 * Valid VIH will unconditionally force the output to go to the minimum of its
 * programmed range. After CLEAR is removed the DAC output will remain at this
 * value. The data in the input register is unaffected.
 */
void AD420_Clear(void);

#endif /* AD420_H_ */
