# ifndef _PM_PROFILE_TRANSMIT_H_
# define _PM_PROFILE_TRANSMIT_H_

# include <unistd.h>
# include <stdint.h>

# include "profile_packet.shared.h"

typedef struct transmit_instructions {

  int   max_raw_packet_size;
  enum  { REP_GRAY, REP_BINARY } representation;
  enum  { BITPLANE, NO_BITPLANE } use_bitplanes;
  int   noise_bits_remove;
  enum  { ZLIB, NO_COMPRESSION } compression;
  enum  { ASCII85, BASE64, NO_ENCODING } encoding;
  int   burst_size;	

} transmit_instructions_t;

int packet_process_transmit ( Profile_Data_Packet_t* original, transmit_instructions_t* tx_instruct, uint8_t* bstStatus, const char* data_dir, int tx_fd );

# endif
