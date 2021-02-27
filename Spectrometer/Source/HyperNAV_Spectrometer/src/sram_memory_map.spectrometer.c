/*! \file sram_memory_map.controller.c*************************************************
 *
 * \brief controller sram_memory_map variable definitions
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-11-19
 *
  ***************************************************************************/

# include "sram_memory_map.spectrometer.h"

# include "spectrometer_data.h"
# include "profile_packet.shared.h"

typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Data_t                            OCR_Data;
  MCOMS_Data_t                        MCOMS_Data;
  Profile_Info_Packet_t      Profile_Info_Packet;
  Profile_Data_Packet_t      Profile_Data_Packet;

} any_local_data_t;

//  Currently, all memory is allocated in blocks that are multiples of 1 kB
//  If memory gets more tight, this simplification may be relaxed.
//  Block sizes and arrangements are meant to be transparent to the tasks using the blocks.

# define BLOCK_MULTIPLE 1024

# define ANY_DATA_BLOCK_SIZE (BLOCK_MULTIPLE*(1+(sizeof(any_local_data_t)-1)/BLOCK_MULTIPLE))

//  The data exchange tasks needs:
//    2 x Arbitrary Data Items
//    May receive two spec data packets in quick succession from data acquisition
//    Receives Profile_*_Packet from profile manager to send to profile processor

# define SRAM_DX_START SPEC_GND_END_TIME_ADDR	

sram_pointer2 const sram_DXS_1 = (sram_pointer2) SRAM_DX_START;
sram_pointer2 const sram_DXS_2 = (sram_pointer2) (SRAM_DX_START + ANY_DATA_BLOCK_SIZE/2);

//  Profile manager
//    1 x Profile_*_Packet for transmit to Spec Board

# define SRAM_PM_START (SRAM_DX_START + 2*(ANY_DATA_BLOCK_SIZE/2))

sram_pointer2 const sram_PMG = (sram_pointer2)SRAM_PM_START;

# if 0
bool sram_memory_sufficient() {
  return (S32)(SRAM_DX_START+3*ANY_DATA_BLOCK_SIZE) <= SRAM_SIZE;	
}

void sram_read2 ( uint16_t* destination, sram_pointer sram_source, size_t num_2bytes ) {
  size_t i;
  for ( i=0; i<num_2bytes; i++ ) {
    destination[i] = sram_source[i];
  }
}

void sram_write2 ( sram_pointer sram_destination, uint16_t* source, size_t num_2bytes ) {
  size_t i;
  for ( i=0; i<num_2bytes; i++ ) {
    sram_destination[i] = source[i];
  }
}

void sram_sram_copy2 ( sram_pointer sram_destination, sram_pointer sram_source, size_t num_2bytes ) {
  size_t i;
  for ( i=0; i<num_2bytes; i++ ) {
    sram_destination[i] = sram_source[i];
  }
}
# endif

void copy_to_ram_from_sram( uint8_t* ram, sram_pointer2 sram, size_t nBytes ) {

# if 1
  uint16_t* ram2 = (uint16_t*)ram;
  for ( int i=0; i<(nBytes/2); i++ ) {
    ram2[i] = sram[i];
  }
# else
  for ( int i=0; i<nBytes; i+=2 ) {
    union { uint8_t u8[2]; uint16_t u16 } d;
    d.u16 = sram_pointer2[i/2];
    ram[i  ] = d.u8[0];
    ram[i+1] = d.u8[1];
  }
# endif
}

