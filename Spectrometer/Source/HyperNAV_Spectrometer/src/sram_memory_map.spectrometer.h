/*! \file sram_memory_map.controller.h*************************************************
 *
 * \brief controller sram_memory_map configuration and variable declarations
 *
 * @author      BP, Satlantic
 * @date        2015-11-19
 *
  ***************************************************************************/

#ifndef _SRAM_MEMORY_MAP_CONTROLLER_H_
#define _SRAM_MEMORY_MAP_CONTROLLER_H_

# include <compiler.h>
# include <stdint.h>

# include "smc_sram.h"


//  Addresses used by the radiometer readout (ADC)
//  These values index into 16-bit memory.
//
#define SPECA_BUF_ADDR			0
#define SPECA_END_TIME_ADDR		2400
#define SPECB_BUF_ADDR			2500
#define SPECB_END_TIME_ADDR		4900
#define SPEC_GND_BUF_ADDR		5000
#define SPEC_GND_END_TIME_ADDR	7400

//  Addresses used in data_exchange.spectrometer.c
//
extern sram_pointer2 const sram_DXS_1;
extern sram_pointer2 const sram_DXS_2;

//  Profile manager
//    1 x Profile_*_Packet for transmit to Spec Board

extern sram_pointer2 const sram_PMG;

//  API

//bool sram_memory_sufficient(void);

//void sram_read2  ( U16* destination, sram_pointer sram_source, size_t num_2bytes );
//void sram_write2 ( sram_pointer sram_destination, U16* source, size_t num_2bytes );
//void sram_sram_copy2 ( sram_pointer sram_destination, sram_pointer sram_source, size_t num_2bytes );

void copy_to_ram_from_sram( uint8_t* ram, sram_pointer2 sram, size_t nBytes );

#endif /* _SRAM_MEMORY_MAP_CONTROLLER_H_ */
