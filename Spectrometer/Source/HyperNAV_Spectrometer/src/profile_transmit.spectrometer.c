# include "profile_transmit.spectrometer.h"

# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include "zlib.h"

# include "profile_packet.spectrometer.h"

int packet_process_transmit ( Profile_Data_Packet_t* original, transmit_instructions_t* tx_instruct, uint8_t* bstStatus, const char* data_dir, int tx_fd ) {

  // Process packet
  //   Binary -> Graycode -> Bitplaned -> Compressed -> ASCII-Encoded

  //  Represent spectra pixel values in graycode
  //

  Profile_Data_Packet_t represent;
    
  //  TODO: Round to proper position before graycoding if noise_remove>0

  if ( tx_instruct->representation == REP_GRAY ) {
    memset( &represent, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bin2gray( original, &represent ) ) {
      fprintf ( stderr, "Failed in data_packet_bin2gray()\n" );
      return 1;
    } else {
    //data_packet_save ( &represent, data_dir, "G" );
    }
  } else {
    memcpy( &represent, original, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  gray %d\n", p );

  //  Arrange the spectral data in bitplanes
  //

  Profile_Data_Packet_t arranged;

  if ( tx_instruct->use_bitplanes == BITPLANE ) {
    memset( &arranged, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bitplane( &represent, &arranged, tx_instruct->noise_bits_remove ) ) {
      fprintf ( stderr, "Failed in data_packet_bitplane()\n" );
      return 1;
    }
    //data_packet_save ( &arranged, data_dir, "B" );
  } else {
    memcpy( &arranged, &represent, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  plne %d\n", p );

  //  Compress the packet
  //

  Profile_Data_Packet_t compressed;
    
  if ( tx_instruct->compression == ZLIB ) {
    memset( &compressed, 0, sizeof(Profile_Data_Packet_t) );
    char compression = 'G';
    if ( data_packet_compress( &arranged, &compressed, compression ) ) {
      fprintf ( stderr, "Failed in data_packet_compress()\n" );
      return 1;
    }
    //data_packet_save ( &compressed, data_dir, "C" );
  } else {
    memcpy( &compressed, &arranged, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  zlib %d\n", p );

  //  Encode the packet
  //

  Profile_Data_Packet_t encoded;
    
  if ( tx_instruct->encoding != NO_ENCODING ) {
    memset( &encoded, 0, sizeof(Profile_Data_Packet_t) );
    char encoding;
    switch ( tx_instruct->encoding ) {
    case ASCII85: encoding = 'A'; break;
    case BASE64 : encoding = 'B'; break;
    default     : encoding = 'N'; break;
    }
    if ( data_packet_encode( &compressed, &encoded, encoding ) ) {
      fprintf ( stderr, "Failed in data_packet_compress()\n" );
      return 1;
    }
    //data_packet_save ( &encoded, data_dir, "E" );
  } else {
    memcpy( &encoded, &compressed, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  zlib %d\n", p );

  //  Transmit the packet
  //

  return data_packet_txmit ( &encoded, tx_instruct->burst_size, tx_fd, bstStatus );
}

