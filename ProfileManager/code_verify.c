# include "code_verify.h"

# include <stdio.h>
# include <stdlib.h>
# include <string.h>

# include "profile_description.h"
# include "profile_packet.h"

int code_verify ( const char* data_dir, uint16_t proID ) {
  const char* const function_name = "code_verify()";

  // Get information about the profile to be trensferred
  //
  Profile_Description_t profile_description;
  profile_description_retrieve ( &profile_description, data_dir, proID );

  // Define packets
  //
  Profile_Packet_Definition_t packet_definition;
  packet_definition.profiler_sn    = profile_description.profiler_sn;
  packet_definition.profile_yyddd  = profile_description.profile_yyddd ;

  packet_definition.numData_SBRD   = profile_description.nSBRD;
  packet_definition.numData_PORT   = profile_description.nPORT;
  packet_definition.numData_OCR    = profile_description.nOCR;
  packet_definition.numData_MCOMS  = profile_description.nMCOMS;

  packet_definition.nPerPacket_SBRD   = 5;
  packet_definition.nPerPacket_PORT   = 5;
  packet_definition.nPerPacket_OCR    = 500;
  packet_definition.nPerPacket_MCOMS  = 500;

  if ( profile_description.nSBRD ) {
    packet_definition.numPackets_SBRD   = 1 +  ( profile_description.nSBRD -1 ) / packet_definition.nPerPacket_SBRD;
  } else {
    packet_definition.numPackets_SBRD   = 0;
  }
  if ( profile_description.nPORT ) {
    packet_definition.numPackets_PORT   = 1 +  ( profile_description.nPORT -1 ) / packet_definition.nPerPacket_PORT;
  } else {
    packet_definition.numPackets_PORT   = 0;
  }
  if ( profile_description.nOCR ) {
    packet_definition.numPackets_OCR    = 1 +  ( profile_description.nOCR  -1 ) / packet_definition.nPerPacket_OCR;
  } else {
    packet_definition.numPackets_OCR    = 0;
  }
  if ( profile_description.nMCOMS ) {
    packet_definition.numPackets_MCOMS  = 1 +  ( profile_description.nMCOMS-1 ) / packet_definition.nPerPacket_MCOMS;
  } else {
    packet_definition.numPackets_MCOMS  = 0;
  }

  // Generate packet files from profile data files
  // (split by size and prepend by packet header)
  //
  uint16_t num_packets = 0;
  char fname[32];

  // generate_packetFile_000();
  {
    Profile_Info_Packet_t pip;
    profile_packet_definition_to_info_packet ( &packet_definition, &pip );

    int m;
    for ( m=0; m<BEGPAK_META; m++ ) {
      pip.meta_info[m] = '.';
    }

    info_packet_save  ( &pip, data_dir, "Z" );

    num_packets ++;
  }

  sprintf ( fname, "%s/%05d/M.sbd", data_dir, profile_description.profile_yyddd );
  FILE* fp = fopen ( fname, "r" );

  uint16_t sP;
  for ( sP=1; sP<=packet_definition.numPackets_SBRD; sP++, num_packets++  ) {
    Profile_Data_Packet_t raw;
    char numString[16];

    raw.HYNV_num = 770;
    raw.PROF_num = packet_definition.profile_yyddd;
    raw.PCKT_num = num_packets;

    raw.sensor_type = 'S';
    raw.empty_space = ' ';
    memcpy ( raw.sensor_ID, "SATYLU0000", 10 );
    uint16_t number_of_data;
    if ( sP<packet_definition.numPackets_SBRD ) {
     number_of_data = packet_definition.nPerPacket_SBRD;
    } else {  //  The last packet may contain fewer items
     number_of_data = profile_description.nSBRD - (packet_definition.numPackets_SBRD-1)*packet_definition.nPerPacket_SBRD;
    }
    snprintf ( numString, 5, "% 4hu", number_of_data );
    memcpy ( raw.number_of_data, numString, 4 );

    raw.representation     = 'B';
    raw.noise_bits_removed = 'N';
    raw.compression        = '0';
    raw.ASCII_encoding     = 'N';

    int d;
    for ( d=0; d<number_of_data; d++ ) {
      Spectrometer_Data_t h;
      fread( &h, sizeof(Spectrometer_Data_t), 1, fp );

      memcpy ( raw.contents.structured.sensor_data.pixel[d], h.light_minus_dark_spectrum, N_SPEC_PIX*sizeof(uint16_t));
      memcpy ( &(raw.contents.structured.aux_data.spec_aux[d]), &(h.aux), sizeof(Spec_Aux_Data_t) );
    }

    uint16_t data_size = number_of_data * ( N_SPEC_PIX*sizeof(uint16_t) + sizeof(Spec_Aux_Data_t) );

    snprintf ( numString, 7, "% 6hu", data_size );
    memcpy ( raw.compressed_sz, "      ", 6 );
    memcpy ( raw.encoded_sz,    "      ", 6 );

    data_packet_save ( &raw, data_dir, "Z" );
  }
  fclose( fp );
  
  // TODO PORT, OCR, MCOMS

  // num_packets = num_data * sizeof(packet_data) / sizeof(data)
  uint16_t p;
  //  TODO: handle packet 0
  for ( p=1; p<num_packets; p++ ) {
    int16_t rv;
    // Read a packet
    //
    Profile_Data_Packet_t original; memset( &original, 0, sizeof(Profile_Data_Packet_t) );
    data_packet_retrieve ( &original, data_dir, "Z", proID, p );

    // Process packets
    //   Binary -> [Graycode ->] Bitplaned -> Compressed -> ASCII-Encoded

    uint16_t noise_remove = 0;
    //  TODO: Round to proper position before graycoding if noise_remove>0

    Profile_Data_Packet_t gray; memset( &gray, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bin2gray( &original, &gray ) ) {
      fprintf ( stderr, "Failed in data_packet_bin2gray()\n" );
    }
    data_packet_save ( &gray, data_dir, "G" );

    Profile_Data_Packet_t bitplane; memset( &bitplane, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bitplane( &gray, &bitplane, noise_remove ) ) {
      fprintf ( stderr, "Failed in data_packet_bitplane()\n" );
    }
    data_packet_save ( &bitplane, data_dir, "B" );

    Profile_Data_Packet_t compressed; memset( &compressed, 0, sizeof(Profile_Data_Packet_t) );
    char compression = 'G';
    if ( data_packet_compress( &bitplane, &compressed, compression ) ) {
      fprintf ( stderr, "Failed in data_packet_compress()\n" );
    }
    data_packet_save ( &compressed, data_dir, "C" );

    Profile_Data_Packet_t encoded; memset( &encoded, 0, sizeof(Profile_Data_Packet_t) );
    char encoding = 'A';
    if ( data_packet_encode( &compressed, &encoded, encoding ) ) {
      fprintf ( stderr, "Failed in data_packet_encode()\n" );
    }
    data_packet_save ( &encoded, data_dir, "E" );

    // Invert process packets
    //   ASCII-Encoded -> Compressed -> Bitplaned -> [Graycode ->] Raw
    //
    Profile_Data_Packet_t decoded; memset( &decoded, 0, sizeof(Profile_Data_Packet_t) );
    if ( rv = data_packet_decode( &encoded, &decoded ) ) {
      fprintf ( stderr, "Failed in data_packet_decode():%hd\n", rv );
    }
    data_packet_save ( &decoded, data_dir, "d" );

    data_packet_compare ( &compressed, &decoded, 'C', 'd' );

    Profile_Data_Packet_t uncompressed; memset( &uncompressed, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_uncompress( &compressed, &uncompressed ) ) {
      fprintf ( stderr, "Failed in data_packet_uncompress()\n" );
    }
    data_packet_save ( &uncompressed, data_dir, "u" );

    data_packet_compare ( &bitplane, &uncompressed, 'B', 'u' );

    Profile_Data_Packet_t debitplaned; memset( &debitplaned, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_debitplane( &bitplane, &debitplaned ) ) {
      fprintf ( stderr, "Failed in data_packet_debitplane()\n" );
    }
    data_packet_save ( &debitplaned, data_dir, "p" );

    data_packet_compare ( &gray, &debitplaned, 'G', 'd' );

    Profile_Data_Packet_t reconstituted; memset( &reconstituted, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_gray2bin( &debitplaned, &reconstituted ) ) {
      fprintf ( stderr, "Failed in data_packet_gray2bin()\n" );
    }
    data_packet_save ( &reconstituted, data_dir, "r" );

    //  Verify: original vs. reconstituted,
    //  taking removed noise bits into account.

    data_packet_compare ( &original, &reconstituted, 'O', 'r' );
  }

  return 0;
}

