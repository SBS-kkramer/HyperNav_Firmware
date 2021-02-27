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

# include "smc_sram.h"

//  The data exchange tasks needs:
//    2 x Arbitrary Data Items
//    May receive two spec data packets in quick succession from data acquisition
//    Receives Profile_*_Packet from profile manager to send to profile processor

extern sram_pointer const sram_DXC_1;
extern sram_pointer const sram_DXC_2;

//  Profile manager
//    2 x Profile_*_Packet for process & transmit

extern sram_pointer const sram_PMG_1;
extern sram_pointer const sram_PMG_2;

//  API

bool sram_memory_sufficient();

void sram_read  ( U8* destination, sram_pointer sram_source, size_t num_bytes );
void sram_write ( sram_pointer sram_destination, U8* source, size_t num_bytes );
void sram_sram_copy ( sram_pointer sram_destination, sram_pointer sram_source, size_t num_bytes );


#endif /* _SRAM_MEMORY_MAP_CONTROLLER_H_ */
