/*! \file ds3234_cfg.h ************************************************************
 *
 * \brief DS3234 Real Time Clock Device Driver configuration file.
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2011-1-28
 *
 *
 **********************************************************************************/
#ifndef DS3234_RTC_CFG_H_
#define DS3234_RTC_CFG_H_

#include <compiler.h>

/*! \name DS3234 RTC
 */
//! @{
// SPI address
#define DS3234_SPI                      (&AVR32_SPI0)
// SPI CHIPSELECT#
#define DS3234_SPI_NPCS                 0
// SPI bits per data transfer (8 or 16)
#define DS3234_SPI_BITS					8
//! @}




#endif /* DS3234_RTC_CFG_H_ */
