/*!	\file 	LTC1451_cfg.h
 *	\brief	LTC1451 12-bit voltage-output DAC configuration file
 *
 *	@author		Scott Feener, Satlantic Inc.
 *	@date		2011-08-09
 *
 * History:
 * 	2011-08-09, SF: File started.
 */

#ifndef LTC1451_CFG_H_
#define LTC1451_CFG_H_

// SPI Master
#define	LTC1451_SPI		(&AVR32_SPI0)

// SPI chip select#
#define	LTC1451_NPCS	5

// SPI bits per data transfer (8 or 16)
#define LTC1451_SPI_BITS	8


#endif /* LTC1451_CFG_H_ */
