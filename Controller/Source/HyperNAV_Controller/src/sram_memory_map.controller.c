/*! \file sram_memory_map.controller.c*************************************************
 *
 * \brief controller sram_memory_map variable definitions
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-11-19
 *
  ***************************************************************************/

# include "sram_memory_map.controller.h"

# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"
# include "profile_packet.shared.h"

typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Data_t                            OCR_Data;
  MCOMS_Data_t                        MCOMS_Data;
  Profile_Info_Packet_t      Profile_Info_Packet;
  Profile_Data_Packet_t      Profile_Data_Packet;

} any_local_data_t;

//  Currently, all memory is allocated in blocks that are multiples of 1 kB
//  If memory gets more thight, this simplification may be relaxed.
//  Block sizes and arrangements are meant to be transparent to the tasks using the blocks.

# define BLOCK_MULTIPLE 1024

# define ANY_DATA_BLOCK_SIZE (BLOCK_MULTIPLE*(1+(sizeof(any_local_data_t)-1)/BLOCK_MULTIPLE))

//  The data exchange tasks needs:
//    2 x Arbitrary Data Items
//    May receive two spec data packets in quick succession from data acquisition
//    Receives Profile_*_Packet from profile manager to send to profile processor

sram_pointer const sram_DXC_1 = SRAM;
sram_pointer const sram_DXC_2 = SRAM + ANY_DATA_BLOCK_SIZE;

//  Profile manager
//    2 x Profile_*_Packet for process & transmit

sram_pointer const sram_PMG_1 = SRAM + 2*ANY_DATA_BLOCK_SIZE;
sram_pointer const sram_PMG_2 = SRAM + 3*ANY_DATA_BLOCK_SIZE;


bool sram_memory_sufficient() {
  return (S32)(4*ANY_DATA_BLOCK_SIZE) <= SRAM_SIZE;	
}

void sram_read ( U8* destination, sram_pointer sram_source, size_t num_bytes ) {
  size_t i;
  for ( i=0; i<num_bytes; i++ ) {
    destination[i] = sram_source[i];
  }
}

void sram_write ( sram_pointer sram_destination, U8* source, size_t num_bytes ) {
  size_t i;
  for ( i=0; i<num_bytes; i++ ) {
    sram_destination[i] = source[i];
  }
}

void sram_sram_copy ( sram_pointer sram_destination, sram_pointer sram_source, size_t num_bytes ) {
  size_t i;
  for ( i=0; i<num_bytes; i++ ) {
    sram_destination[i] = sram_source[i];
  }
}

