# ifndef _PM_PROFILE_TRANSMIT_H_
# define _PM_PROFILE_TRANSMIT_H_

# include <unistd.h>
# include <stdint.h>

typedef struct transmit_instructions {

  int   max_raw_packet_size;
  enum  { REP_GRAY, REP_BINARY } representation;
  enum  { BITPLANE, NO_BITPLANE } use_bitplanes;
  int   noise_bits_remove;
  enum  { ZLIB, NO_COMPRESSION } compression;
  enum  { ASCII85, BASE64, NO_ENCODING } encoding;
  int   burst_size;	

} transmit_instructions_t;

int profile_transmit ( const char* data_dir, uint16_t proID, transmit_instructions_t* tx_instruct, uint16_t port, char* host_ip );

# endif
