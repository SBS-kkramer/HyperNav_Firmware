/*
 *
 *
 *  Created on: 2011-08-09
 *      Author: Scott
 */
/*!	\file 	AD420_cfg.h
 *	\brief	AD420 16-bit current-output (4-20 mA, 0-20 mA, or 0-24 mA) DAC configuration file
 *
 *	@author		Scott Feener, Satlantic Inc.
 *	@date		2011-08-09
 *
 * History:
 * 	2011-08-09, SF: File started.
 */


#ifndef AD420_CFG_H_
#define AD420_CFG_H_

// define allowed current output settings - do not change
#define AD420_4_20MA	0		// 4-20 mA
#define AD420_0_20MA	1		// 0-20 mA
#define AD420_0_24MA	2		// 0-24 mA

//
// Application specific settings:
//

// SPI Master
#define	AD420_SPI		(&AVR32_SPI0)

// SPI chip select#
#define	AD420_NPCS	6

// SPI bits per data transfer (8 or 16)
#define AD420_SPI_BITS	8

// Current output mode - board configuration
#define AD420_I_RANGE	AD420_4_20MA

// hardware clearing control
#define AD420_CLEAR				AVR32_PIN_PB04



#endif /* AD420_CFG_H_ */
