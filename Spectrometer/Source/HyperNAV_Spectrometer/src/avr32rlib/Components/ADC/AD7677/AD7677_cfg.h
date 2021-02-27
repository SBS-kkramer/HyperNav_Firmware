/*!	\file	AD7677_cfg.h
 *
 *	\brief	AD7677 - 16 bit, 1MSPS differential ADC.  Configuration file.
 *
 * Created: 7/2/2016 11:50:41 PM
 *  Author: sfeener
 *
 * @author	Scott Feener
 * @date	2016-07-02
 */ 


#ifndef AD7677_CFG_H_
#define AD7677_CFG_H_


#include "compiler.h"

#define AD7677_PARALLEL_MODE	0
#define AD7677_SERIAL_MODE		1
#define AD7677_OPERATING_MODE	AD7677_PARALLEL_MODE

#if (AD7677_OPERATING_MODE == AD7677_SERIAL_MODE)
#error	"AD7677 serial mode not currently supported"
#endif

#define AD7677_DATA_STRAIGHT	0
#define AD7677_DATA_TWOS_C		1
#define AD7677_DATA_FORMAT		AD7677_DATA_STRAIGHT

#if (AD7677_DATA_FORMAT == AD7677_DATA_TWOS_C)
#error	"AD7677 two's complement data mode not currently supported"
#endif


// Reference voltage
#define AD7677_VREF		3.0






#endif /* AD7677_CFG_H_ */