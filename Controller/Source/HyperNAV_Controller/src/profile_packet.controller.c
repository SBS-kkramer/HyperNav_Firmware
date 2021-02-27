# include "profile_packet.controller.h"

# include <libgen.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <unistd.h>

# include "zlib.h"

# include "files.h"

# if 0
static int info_packet_burst_header24 ( Profile_Info_Packet_t* pip, int bNum, int value, unsigned char* header, uint16_t header_len ) {

  if ( !header || header_len<24 ) return 1;

  uint16_t         nn = 0;
  unsigned char tmpStr[32];


                                                  memcpy( header,    "BRST", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", pip->HYNV_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  snprintf ( tmpStr, 6, "%05hu", pip->PROF_num ); memcpy( header+nn, tmpStr, 5 ); nn += 5;
  snprintf ( tmpStr, 5, "%04hu", pip->PCKT_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  if ( -1 == bNum ) {  //  Terminal burst for a given packet
                                                  memcpy( header+nn, "ZZZ",  3 ); nn += 3;
                                                  memcpy( header+nn, "ZZZZ", 4 ); nn += 4;
  } else { //  Normal Burst or Burst ZERO (report number of bursts via value field)
    snprintf ( tmpStr, 4, "%03d",   bNum%1000  ); memcpy( header+nn, tmpStr, 3 ); nn += 3;
    snprintf ( tmpStr, 5, "%04hu", value%10000 ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  }

  return 0;
}
# endif

# if 0
//  Generate 24-byte packet header
static int info_packet_header24 ( Profile_Info_Packet_t* pip, unsigned char* header, uint16_t header_len ) {

  if ( !header || header_len<24 ) return 1;

  uint16_t nn = 0;
  char tmpStr[32];

                                                  memcpy( header,    "HYNV", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", pip->HYNV_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
                                                  memcpy( header+nn, "PRF" , 3 ); nn += 3;
  snprintf ( tmpStr, 6, "%05hu", pip->PROF_num ); memcpy( header+nn, tmpStr, 5 ); nn += 5;
                                                  memcpy( header+nn, "PCKT", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", pip->PCKT_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;

  return 0;
}
# endif
# if 0
int info_packet_fromBytes( Profile_Info_Packet_t* pip, unsigned char* asBytes, uint16_t nBytes ) {

  if ( !pip ) return 1;
  if ( !asBytes ) return 1;
  if ( nBytes < 32+BEGPAK_META ) return 1;

  memcpy ( pip->num_dat_SBRD , asBytes   , 4 );
  memcpy ( pip->num_dat_PORT , asBytes+ 4, 4 );
  memcpy ( pip->num_dat_OCR  , asBytes+ 8, 4 );
  memcpy ( pip->num_dat_MCOMS, asBytes+12, 4 );

  memcpy ( pip->num_pck_SBRD , asBytes+16, 4 );
  memcpy ( pip->num_pck_PORT , asBytes+20, 4 );
  memcpy ( pip->num_pck_OCR  , asBytes+24, 4 );
  memcpy ( pip->num_pck_MCOMS, asBytes+28, 4 );

  memcpy ( pip->meta_info, asBytes+32, BEGPAK_META );

  return 0;
}
# endif
# if 0
static int info_packet_asBytes( Profile_Info_Packet_t* pip, uint16_t* nBytes ) {

  uint16_t nn = 0;

  /* memcpy ( dest+nn, pip->num_dat_SBRD , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_dat_PORT , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_dat_OCR  , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_dat_MCOMS, 4 ); */ nn+=4;

  /* memcpy ( dest+nn, pip->num_pck_SBRD , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_pck_PORT , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_pck_OCR  , 4 ); */ nn+=4;
  /* memcpy ( dest+nn, pip->num_pck_MCOMS, 4 ); */ nn+=4;

  /* memcpy ( dest+nn, pip->meta_info, BEGPAK_META ); */ nn+=BEGPAK_META;

  *nBytes = nn;

  return 0;
}
# endif
# if 0
static int info_packet_write_asBursts ( Profile_Info_Packet_t* pip, int burst_size, int* numBurstsInPIP, int burstToTx ) {

  if ( !pip ) return 1;

  unsigned char*  data = (unsigned char*)pip;
  uint16_t const nData = 4*4 + 4*4 + BEGPAK_META;

  int numBursts = 1 + ( nData - 1 ) / burst_size;

  unsigned char header[24];
  char CRC32[9];
  uLong crc;

  if ( burstToTx == 0 ) {

    *numBurstsInPIP = numBursts;

    //  Burst ZERO is special: header only
    //
    info_packet_burst_header24 ( pip, 0, numBursts, header, 24 );

    //  Calculate header's CRC
    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != write ( fd, header, 24 ) ) { return 1; }
    if(  8 != write ( fd, CRC32,   8 ) ) { return 1; }

  } else if ( 1 <= burstToTx && burstToTx <= *numBursts ) {

    unsigned char* burst_data = data + (burstToTx-1) * burst_size;
    int sz = (burstToTx<numBursts) ? burst_size : ( nData - (numBursts-1)*burst_size );

    info_packet_burst_header24 ( pip, burstToTx, sz, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    crc = crc32 ( crc, burst_data, sz );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != write ( fd, header,     24 ) ) { return 1; }
    if(  8 != write ( fd, CRC32,       8 ) ) { return 1; }
    if( sz != write ( fd, burst_data, sz ) ) { return 1; }

  } else {

    //  Burst TERMINATOR is special: header only
    info_packet_burst_header24 ( pip, -1, 0, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != write ( fd, header, 24 ) ) { return 1; }
    if(  8 != write ( fd, CRC32,   8 ) ) { return 1; }
  }

  return 0;
}
# endif

# if 0
static int info_packet_write ( Profile_Info_Packet_t* pip, int fd ) {

  if ( !pip ) return 1;
  if ( fd < 0 ) return 1;

  unsigned char  header[25];  //  Header size is 24, add 1 byte for NUL terminator
  info_packet_header24 ( pip, header, 24 );
  header[24] = 0;

  unsigned char* data;
  uint16_t       nn;

  if ( info_packet_asBytes ( pip, &data, &nn ) ) {
    return 1;
  }

# ifdef ZLIB_IN_FW
  uLong crc = crc32 ( 0L, Z_NULL, 0 );
  crc = crc32 ( crc, header, 24 );
  crc = crc32 ( crc, data,   nn );
# else
  uint32_t crc = 0;
# endif
  char CRC32[9];
  snprintf ( CRC32, 9, "%08lX", crc );

  if( 24 != write ( fd, header, 24 ) ) { vPortFree ( data ); return 1; }
  if(  8 != write ( fd, CRC32,   8 ) ) { vPortFree ( data ); return 1; }
  if( nn != write ( fd, data,   nn ) ) { vPortFree ( data ); return 1; }

  vPortFree ( data );
  return 0;
}
# endif
# if 0
int info_packet_txmit ( Profile_Info_Packet_t* pip, int burst_size, uint8_t bstStat[] ) {

  if ( !pip ) return 1;

  if ( 0 == burst_size ) {
    //int rv = info_packet_write ( pip );
    return 1;
  } else {
    return info_packet_write_asBursts ( pip, burst_size, bstStat );
  }
}
# endif
int16_t info_packet_save_native ( Profile_Info_Packet_t* pip, const char* file_name ) {

  if ( !pip || !file_name ) return -1;

  fHandler_t fh;
  
  if ( FILE_OK != f_open ( file_name, O_WRONLY | O_CREAT, &fh ) ) {
    return -2;
  }

  if ( sizeof(Profile_Info_Packet_t) != f_write ( &fh, pip, sizeof(Profile_Info_Packet_t) ) ) {
    f_close ( &fh );
    return -1;
  }
  
  f_close ( &fh );

  return 0;
}

int16_t info_packet_retrieve_native ( Profile_Info_Packet_t* pip, const char* file_name ) {

  if ( !pip || !file_name ) return -1;

  fHandler_t fh;
  
  if ( FILE_OK != f_open ( file_name, O_RDONLY, &fh ) ) {
    return -2;
  }

  if ( sizeof(Profile_Info_Packet_t) != f_read ( &fh, pip, sizeof(Profile_Info_Packet_t) ) ) {
    f_close ( &fh );
    return -1;
  }

  f_close ( &fh );

  return 0;
}

# if 0
static int data_packet_burst_header24 ( Profile_Data_Packet_t* packet, int bNum, int value, unsigned char* header, uint16_t header_len ) {

  if ( !header || header_len<24 ) return 1;

  uint16_t         nn = 0;
  unsigned char tmpStr[32];

                                                     memcpy( header,    "BRST", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", packet->HYNV_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  snprintf ( tmpStr, 6, "%05hu", packet->PROF_num ); memcpy( header+nn, tmpStr, 5 ); nn += 5;
  snprintf ( tmpStr, 5, "%04hu", packet->PCKT_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  if ( -1 == bNum ) {  //  Terminal burst for a given packet
                                                  memcpy( header+nn, "ZZZ",  3 ); nn += 3;
                                                  memcpy( header+nn, "ZZZZ", 4 ); nn += 4;
  } else { //  Normal Burst or Burst ZERO (report number of bursts via value field)
    snprintf ( tmpStr, 4, "%03d",   bNum%1000  ); memcpy( header+nn, tmpStr, 3 ); nn += 3;
    snprintf ( tmpStr, 5, "%04hu", value%10000 ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
  }

  return 0;
}
# endif

# if 0
//  Generate 24-byte packet header
static int data_packet_header24 ( Profile_Data_Packet_t* packet, unsigned char* header, uint16_t header_len ) {

  if ( !header || header_len<24 ) return 1;

  uint16_t nn = 0;
  char tmpStr[32];

                                                     memcpy( header,    "HYNV", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", packet->HYNV_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;
                                                     memcpy( header+nn, "PRF" , 3 ); nn += 3;
  snprintf ( tmpStr, 6, "%05hu", packet->PROF_num ); memcpy( header+nn, tmpStr, 5 ); nn += 5;
                                                     memcpy( header+nn, "PCKT", 4 ); nn += 4;
  snprintf ( tmpStr, 5, "%04hu", packet->PCKT_num ); memcpy( header+nn, tmpStr, 4 ); nn += 4;

  return 0;
}
# endif

# if 0
int data_packet_fromBytes( Profile_Data_Packet_t* packet, unsigned char* asBytes, uint16_t nBytes ) {

  if ( !packet ) return 1;
  if ( !asBytes ) return 1;
  if ( nBytes < 32 ) return 1;  //  but 32 checks only for the initial part

  memcpy ( &(packet->header.sensor_type),        asBytes   ,  1 );
  memcpy ( &(packet->header.empty_space),        asBytes+ 1,  1 );
  memcpy (   packet->header.sensor_ID,           asBytes+ 2, 10 );
  memcpy (   packet->header.number_of_data,      asBytes+12,  4 );

  memcpy ( &(packet->header.representation),     asBytes+16,  1 );
  memcpy ( &(packet->header.noise_bits_removed), asBytes+17,  1 );
  memcpy ( &(packet->header.compression),        asBytes+18,  1 );
  memcpy ( &(packet->header.ASCII_encoding),     asBytes+19,  1 );

  memcpy (   packet->header.compressed_sz ,      asBytes+20,  6 );
  memcpy (   packet->header.encoded_sz,          asBytes+26,  6 );

  //  This completes the 'about' section of the packet

  //  Either uncompressed data ...
  if ( packet->header.compression == '0' ) {

    //  Currently only accept compressed packets
    return 1;

# if 0
    if ( packet->header.sensor_type == 'S' || packet->header.sensor_type == 'P' ) {

      uint16_t number_of_data;
      sscanf ( packet->header.number_of_data, "%hu", &number_of_data );

      uint16_t number_of_bytes;
      if ( packet->header.noise_bits_removed == 'N' ) {
        number_of_bytes = number_of_data*N_SPEC_PIX*sizeof(uint16_t);
        memcpy ( dest+nn, packet->contents.structured.sensor_data.pixel, number_of_bytes ); nn += number_of_bytes;
      } else {  //  TODO!!!
        number_of_bytes = number_of_data*(N_SPEC_PIX/(8*sizeof(uint16_t)))*((8*sizeof(uint16_t)) - (packet->header.noise_bits_removed-'0'));
        memcpy ( dest+nn, packet->contents.structured.sensor_data.bitplanes, number_of_bytes ); nn += number_of_bytes;
      }
      number_of_bytes = number_of_data*sizeof(Spec_Aux_Data_t);
      memcpy ( dest+nn, packet->contents.structured.aux_data.spec_aux, number_of_bytes );
    } else {
      //  TODO other sensors
    }

  // ... or else compressed or encoded bytes
# endif
  } else {
    uint16_t number_of_bytes;
    if ( packet->header.ASCII_encoding != 'N' ) {
      sscanf ( packet->header.encoded_sz, "%hu", &number_of_bytes );
    } else {
      sscanf ( packet->header.compressed_sz, "%hu", &number_of_bytes );
    }

    if ( nBytes < 32+number_of_bytes ) return 1;

    memcpy ( packet->contents.flat_bytes, asBytes+32, number_of_bytes );
  }

  return 0;
}
# endif
# if 0
static int data_packet_asBytes( Profile_Data_Packet_t* packet, unsigned char** asBytes, uint16_t* nBytes ) {

  unsigned char* dest = pvPortMalloc ( sizeof(Profile_Data_Packet_t) );
  if ( !dest ) return 1;
  uint16_t nn = 0;

  memcpy ( dest+nn, &(packet->header.sensor_type),        1 ); nn+=1;
  memcpy ( dest+nn, &(packet->header.empty_space),        1 ); nn+=1;
  memcpy ( dest+nn,   packet->header.sensor_ID,          10 ); nn+=10;
  memcpy ( dest+nn,   packet->header.number_of_data,      4 ); nn+=4;

  memcpy ( dest+nn, &(packet->header.representation),     1 ); nn+=1;
  memcpy ( dest+nn, &(packet->header.noise_bits_removed), 1 ); nn+=1;
  memcpy ( dest+nn, &(packet->header.compression),        1 ); nn+=1;
  memcpy ( dest+nn, &(packet->header.ASCII_encoding),     1 ); nn+=1;

  memcpy ( dest+nn,   packet->header.compressed_sz ,      6 ); nn+=6;
  memcpy ( dest+nn,   packet->header.encoded_sz,          6 ); nn+=6;

  //  This completes the header section of the packet

  //  Either uncompressed data ...
  if ( packet->header.compression == '0' ) {
# if 0
    if ( packet->header.sensor_type == 'S' || packet->header.sensor_type == 'P' ) {
      uint16_t number_of_data;
      sscanf ( packet->header.number_of_data, "%hu", &number_of_data );

      uint16_t number_of_bytes;
      if ( packet->header.noise_bits_removed == 'N' ) {
        number_of_bytes = number_of_data*N_SPEC_PIX*sizeof(uint16_t);
        memcpy ( dest+nn, packet->contents.structured.sensor_data.pixel, number_of_bytes ); nn += number_of_bytes;
      } else {  //  TODO!!!
        number_of_bytes = number_of_data*(N_SPEC_PIX/(8*sizeof(uint16_t)))*((8*sizeof(uint16_t)) - (packet->header.noise_bits_removed-'0'));
        memcpy ( dest+nn, packet->contents.structured.sensor_data.bitplanes, number_of_bytes ); nn += number_of_bytes;
      }
      number_of_bytes = number_of_data*sizeof(Spec_Aux_Data_t);
      memcpy ( dest+nn, packet->contents.structured.aux_data.spec_aux, number_of_bytes );
    } else {
      //  TODO other sensors
    }
# endif
  // ... or else compressed or encoded bytes
  } else {
    uint16_t number_of_bytes;
    if ( packet->header.ASCII_encoding != 'N' ) {
      sscanf ( packet->header.encoded_sz, "%hu", &number_of_bytes );
    } else {
      sscanf ( packet->header.compressed_sz, "%hu", &number_of_bytes );
    }
    memcpy ( dest+nn, packet->contents.flat_bytes, number_of_bytes ); nn += number_of_bytes;
  }

  *asBytes = dest;
  *nBytes = nn;

  return 0;
}
# endif
# if 0
static int data_packet_write_asBursts ( Profile_Data_Packet_t* packet, int burst_size, int fd, uint8_t bstStat[] ) {

  if ( !packet ) return 1;
  if ( fd  < 0 ) return 1;

  unsigned char* data;
  uint16_t      nData;

  if ( data_packet_asBytes ( packet, &data, &nData ) ) {
    return 1;
  }
  //fprintf ( stderr, "data_packet_write_asBursts() - %hd bytes\n", nData );

  int const numBursts = 1 + ( nData - 1 ) / burst_size;

  unsigned char header[24];
  char CRC32[9];
  uLong crc;
  useconds_t delay;

  if ( 1 == bstStat[0] ) {
    //  Burst ZERO is special: header only
    //
    data_packet_burst_header24 ( packet, 0, numBursts, header, 24 );

    //  Calculate header's CRC
    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != write ( fd, header, 24 ) ) { vPortFree ( data ); return 1; }
    if(  8 != write ( fd, CRC32,   8 ) ) { vPortFree ( data ); return 1; }

    //  Simulate modem speed
    delay = (1000*32) / (BAUDRATE/10);
  //fprintf ( stderr, "msleep %d\n", delay );
    usleep ( 1000*delay );
  }

  int b;
  for ( b=1; b<=numBursts; b++ ) {

    if ( 1 == bstStat[b] ) {
      unsigned char* burst_data = data + (b-1) * burst_size;
      int sz = (b<numBursts) ? burst_size : ( nData - (numBursts-1)*burst_size );

      data_packet_burst_header24 ( packet, b, sz, header, 24 );

      crc = crc32 ( 0L, Z_NULL, 0 );
      crc = crc32 ( crc, header, 24 );
      crc = crc32 ( crc, burst_data, sz );
      snprintf ( CRC32, 9, "%08X", crc );

      if( 24 != write ( fd, header,     24 ) ) { vPortFree ( data ); return 1; }
      if(  8 != write ( fd, CRC32,       8 ) ) { vPortFree ( data ); return 1; }
      if( sz != write ( fd, burst_data, sz ) ) { vPortFree ( data ); return 1; }

      //  Simulate modem speed
      delay = (1000*(sz+32)) / (BAUDRATE/10);
   // fprintf ( stderr, "msleep %d\n", delay );
      usleep ( 1000*delay );
    }

  }
 
  vPortFree ( data );

  //  Burst TERMINATOR is special: header only
  //  Always send this, to facilitate receiver data assembly
  //
  data_packet_burst_header24 ( packet, -1, 0, header, 24 );

  crc = crc32 ( 0L, Z_NULL, 0 );
  crc = crc32 ( crc, header, 24 );
  snprintf ( CRC32, 9, "%08X", crc );

  if( 24 != write ( fd, header, 24 ) ) { return 1; }
  if(  8 != write ( fd, CRC32,   8 ) ) { return 1; }

  //  Simulate modem speed
  delay = (1000*32) / (BAUDRATE/10);
//fprintf ( stderr, "msleep %d\n", delay );
  usleep ( 1000*delay );

  return 0;
}
# endif
# if 0
static int data_packet_write ( Profile_Data_Packet_t* packet, int fd ) {

  if ( !packet ) return 1;
  if ( fd  < 0 ) return 1;

  unsigned char  header[25];  //  Header size is 24, add 1 byte for NUL terminator
  data_packet_header24 ( packet, header, 24 );
  header[24] = 0;

  unsigned char* data;
  uint16_t      nData;

  if ( data_packet_asBytes ( packet, &data, &nData ) ) {
    return 1;
  }

# ifdef ZLIB_IN_FW
  uLong crc = crc32 ( 0L, Z_NULL, 0 );
  crc = crc32 ( crc, header,  24 );
  crc = crc32 ( crc, data, nData );
# else
  uint32_t crc = 0;
# endif
  char CRC32[9];
  snprintf ( CRC32, 9, "%08lX", crc );

  if(    24 != write ( fd, header,  24 ) ) { vPortFree ( data ); return 1; }
  if(     8 != write ( fd, CRC32,    8 ) ) { vPortFree ( data ); return 1; }
  if( nData != write ( fd, data, nData ) ) { vPortFree ( data ); return 1; }

  vPortFree ( data );
  return 0;

}
# endif
# if 0
int data_packet_txmit ( Profile_Data_Packet_t* packet, int burst_size, int fd, uint8_t bstStat[] ) {

  if ( !packet ) return 1;
  if ( fd  < 0 ) return 1;

  if ( 0 == burst_size ) {
    int rv = data_packet_write ( packet, fd );
    return rv;
  } else {
    int rv = data_packet_write_asBursts ( packet, burst_size, fd, bstStat );
    return rv;
  }
}
# endif

int16_t data_packet_save_native ( Profile_Data_Packet_t* packet, const char* file_name ) {

  if ( !packet || !file_name ) return -1;

  fHandler_t fh;
  
  if ( FILE_OK != f_open ( file_name, O_WRONLY | O_CREAT, &fh ) ) {
    return -2;
  }

  if ( sizeof(Profile_Data_Packet_t) != f_write ( &fh, packet, sizeof(Profile_Data_Packet_t) ) ) {
    f_close ( &fh );
    return -1;
  }
  
  f_close ( &fh );

  return 0;
}

# if 0
static int data_packet_read( Profile_Data_Packet_t* p, FILE* fq ) {

  if ( !p ) return 1;
  if ( !fq ) return 1;

  char header[24];
  if ( 1 != fread ( header, 24, 1, fq ) ) { return 1; }

  unsigned char CRC   [8];
  if ( 1 != fread ( CRC, 8, 1, fq ) ) { return 1; }

  unsigned char data [32];

  if ( 1 != fread ( data, 32, 1, fq ) ) { return 1; }

  //  Parse header
  if ( 3 != sscanf ( header, "HYNV%4huPRF%5huPCKT%4hu", &(p->HYNV_num), &(p->PROF_num), &(p->PCKT_num) ) ) {
    fprintf ( stderr, "data_packet_read() header %24.24s parse error\n", header );
  }

  //  Parse data
  p->header.sensor_type            = data[ 0];
  p->header.empty_space            = data[ 1];
  memcpy ( p->header.sensor_ID,      data+ 2, 10 );
  memcpy ( p->header.number_of_data, data+12,  4 );
  p->header.representation         = data[16];
  p->header.noise_bits_removed     = data[17];
  p->header.compression            = data[18];
  p->header.ASCII_encoding         = data[19];
  memcpy ( p->header.compressed_sz,  data+20,   6 );
  memcpy ( p->header.encoded_sz,     data+26,   6 );

  if ( ( p->header.sensor_type == 'S' || p->header.sensor_type == 'P' )
    &&   p->header.noise_bits_removed == 'N'
    &&   p->header.compression == '0'
    &&   p->header.ASCII_encoding == 'N' ) {

    uint16_t number_of_data;
    sscanf ( p->header.number_of_data, "%hu", &number_of_data );
    fread ( &(p->contents.structured.sensor_data.pixel), number_of_data*N_SPEC_PIX*sizeof(uint16_t), 1, fq);
    fread ( &(p->contents.structured.aux_data.spec_aux), number_of_data*sizeof(Spec_Aux_Data_t), 1, fq);

    //  TODO -- Verify CRC

    return 0;
  } else {
    return 1;
  }
}
# endif

int16_t data_packet_retrieve_native ( Profile_Data_Packet_t* packet, const char* file_name ) {

  if ( !packet || !file_name ) return -1;

  fHandler_t fh;
  
  if ( FILE_OK != f_open ( file_name, O_RDONLY, &fh ) ) {
    return -2;
  }

  if ( sizeof(Profile_Data_Packet_t) != f_read ( &fh, packet, sizeof(Profile_Data_Packet_t) ) ) {
    f_close ( &fh );
    return -1;
  }

  f_close ( &fh );

  return 0;
}

# if 0
int16_t data_packet_bin2gray ( Profile_Data_Packet_t* bin, Profile_Data_Packet_t* gray ) {

  //  Make sure input packet data are in binary, not bitplaned,
  //  uncompressed and unencoded.
  //
  if ( bin->header.representation != 'B'
    || bin->header.noise_bits_removed != 'N'
    || bin->header.compression != '0'
    || bin->header.ASCII_encoding != 'N'
    || !( bin->header.sensor_type == 'S' || bin->header.sensor_type == 'P' ) ) {

    fprintf ( stderr, "data_packet_bin2gray() fail %c %c %c %c %c\n", bin->header.representation, bin->header.noise_bits_removed, bin->header.compression, bin->header.ASCII_encoding, bin->header.sensor_type );
    return 1;
  }
  
  //  Copy most of header and adjust some entries
  //
  gray->HYNV_num    = bin->HYNV_num;
  gray->PROF_num    = bin->PROF_num;
  gray->PCKT_num    = bin->PCKT_num;
  gray->header.sensor_type = bin->header.sensor_type;
  gray->header.empty_space = bin->header.empty_space;
  memcpy ( gray->header.sensor_ID, bin->header.sensor_ID, 10 );
  memcpy ( gray->header.number_of_data, bin->header.number_of_data, 4 );
  gray->header.representation     = 'G';
  gray->header.noise_bits_removed = 'N';
  gray->header.compression        = '0';
  gray->header.ASCII_encoding     = 'N';
  memcpy ( gray->header.compressed_sz, "      ", 6 );
  memcpy ( gray->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data, i, p;
  sscanf ( gray->header.number_of_data, "%hu", &number_of_data );
  // convert pixels from binary to gray code
  //
  for ( i=0; i<number_of_data; i++ ) {
    for ( p=0; p<N_SPEC_PIX; p++ ) {
      uint16_t const val = bin->contents.structured.sensor_data.pixel[i][p];
      gray->contents.structured.sensor_data.pixel[i][p] = val ^ ( val >> 1 );
    }
  }

  // copy auxiliary 'as is'
  //
  memcpy ( gray->contents.structured.aux_data.spec_aux,
            bin->contents.structured.aux_data.spec_aux,  number_of_data*sizeof(Spec_Aux_Data_t) );
  
  return 0;
}
# endif

# if 0
int data_packet_gray2bin ( Profile_Data_Packet_t* gray, Profile_Data_Packet_t* bin ) {

  //  Make sure input packet data are in graycode, not bitplaned,
  //  uncompressed and unencoded.
  //
  if ( gray->header.representation != 'G'
    || gray->header.noise_bits_removed != 'N'
    || gray->header.compression != '0'
    || gray->header.ASCII_encoding != 'N'
    || !( gray->header.sensor_type == 'S' || gray->header.sensor_type == 'P' ) ) {
    fprintf ( stderr, "data_packet_gray2bin() fail %c %c %c %c %c\n", gray->header.representation, gray->header.noise_bits_removed, gray->header.compression, gray->header.ASCII_encoding, gray->header.sensor_type );
    return 1;
  }
  
  //  Copy most of header and adjust some entries
  //
  bin->HYNV_num    = gray->HYNV_num;
  bin->PROF_num    = gray->PROF_num;
  bin->PCKT_num    = gray->PCKT_num;
  bin->header.sensor_type = gray->header.sensor_type;
  bin->header.empty_space = gray->header.empty_space;
  memcpy ( bin->header.sensor_ID, gray->header.sensor_ID, 10 );
  memcpy ( bin->header.number_of_data, gray->header.number_of_data, 4 );
  bin->header.representation     = 'B';
  bin->header.noise_bits_removed = 'N';
  bin->header.compression        = '0';
  bin->header.ASCII_encoding     = 'N';
  memcpy ( bin->header.compressed_sz, "      ", 6 );
  memcpy ( bin->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data, i, p;
  sscanf ( bin->header.number_of_data, "%hu", &number_of_data );
  // convert pixels from binary to gray code
  //
  for ( i=0; i<number_of_data; i++ ) {
    for ( p=0; p<N_SPEC_PIX; p++ ) {
      uint16_t val = gray->contents.structured.sensor_data.pixel[i][p];

      uint16_t mask;
      for ( mask = val>>1; mask != 0; mask >>= 1 ) {
        val = val ^ mask;
      }

      bin->contents.structured.sensor_data.pixel[i][p] = val;
    }
  }

  // copy auxiliary 'as is'
  //
  memcpy (  bin->contents.structured.aux_data.spec_aux,
           gray->contents.structured.aux_data.spec_aux,  number_of_data*sizeof(Spec_Aux_Data_t) );
  
  return 0;
}
# endif

# if 0
int16_t data_packet_bitplane ( Profile_Data_Packet_t* pix, Profile_Data_Packet_t* bp, uint16_t remove_noise_bits ) {

  //  Make sure input packet is uncompressed and unencoded
  //  bitplaning can handle either binary or grayscale
  if ( pix->header.noise_bits_removed != 'N'
    || pix->header.compression != '0'
    || pix->header.ASCII_encoding != 'N' ) {
    return 1;
  }

  //  Copy most of header and adjust some entries
  //
  bp->HYNV_num    = pix->HYNV_num;
  bp->PROF_num    = pix->PROF_num;
  bp->PCKT_num    = pix->PCKT_num;
  bp->header.sensor_type = pix->header.sensor_type;
  bp->header.empty_space = pix->header.empty_space;
  memcpy ( bp->header.sensor_ID, pix->header.sensor_ID, 10 );
  memcpy ( bp->header.number_of_data, pix->header.number_of_data, 4 );
  bp->header.representation     = pix->header.representation;
  char numString[4];
  if ( remove_noise_bits > 0x0F ) remove_noise_bits = 0x0F;
  snprintf ( numString, 2, "%hx", remove_noise_bits );
  bp->header.noise_bits_removed = numString[0];
  bp->header.compression        = '0';
  bp->header.ASCII_encoding     = 'N';

  memcpy ( bp->header.compressed_sz, "      ", 6 );
  memcpy ( bp->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data, i, p;
  sscanf ( bp->header.number_of_data, "%hu", &number_of_data );

  //  Bitplane the pixels
  //

  uint16_t low_mask = 0x0001 << remove_noise_bits;
  uint16_t bit_mask;
  uint16_t plane_item = 0;
  for ( bit_mask = 0x8000; bit_mask >= low_mask; bit_mask >>= 1 ) {

    uint8_t  plane_byte = 0;
    uint8_t  plane_mask = 0x80;

    for ( i=0; i<number_of_data; i++ ) {
    for ( p=0; p<N_SPEC_PIX; p++ ) {
      uint16_t const val = pix->contents.structured.sensor_data.pixel[i][p];

      if ( val & bit_mask ) {
        plane_byte |= plane_mask;
      }

      if ( 0x01 != plane_mask ) {
        plane_mask >>= 1;
      } else {
        bp->contents.structured.sensor_data.bitplanes[plane_item] = plane_byte;
        plane_item++;
        plane_byte = 0;
        plane_mask = 0x80;
      }

    }
    }

  }
//fprintf ( stderr, "data_packet_bitplane() converted %hd items\n", plane_item );

  //  copy auxiliary 'as is'
  //
  memcpy ( bp->contents.structured.aux_data.spec_aux,
          pix->contents.structured.aux_data.spec_aux,  number_of_data*sizeof(Spec_Aux_Data_t) );
  
  return 0;
}
# endif

# if 0
int data_packet_debitplane ( Profile_Data_Packet_t* bp, Profile_Data_Packet_t* pix ) {

  //  Make sure input packet is uncompressed and unencoded
  //  bitplaning can handle either binary or grayscale
  if ( bp->header.compression != '0'
    || bp->header.ASCII_encoding != 'N' ) {
    return 1;
  }

  //  Copy most of header and adjust some entries
  //
  pix->HYNV_num    = bp->HYNV_num;
  pix->PROF_num    = bp->PROF_num;
  pix->PCKT_num    = bp->PCKT_num;
  pix->header.sensor_type = bp->header.sensor_type;
  pix->header.empty_space = bp->header.empty_space;
  memcpy ( pix->header.sensor_ID, bp->header.sensor_ID, 10 );
  memcpy ( pix->header.number_of_data, bp->header.number_of_data, 4 );
  pix->header.representation     = bp->header.representation;

  char numString[4];
  uint16_t removed_noise_bits = bp->header.noise_bits_removed - '0';

  pix->header.noise_bits_removed = 'N';  //  ALERT -- This is a loss of information
  pix->header.compression        = '0';
  pix->header.ASCII_encoding     = 'N';

  memcpy ( pix->header.compressed_sz, "      ", 6 );
  memcpy ( pix->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data, i, p;
  sscanf ( bp->header.number_of_data, "%hu", &number_of_data );

  //  De-Bitplane the pixels
  //

    //  Initialize all to zero
    for ( i=0; i<number_of_data; i++ ) {
    for ( p=0; p<N_SPEC_PIX; p++ ) {
        pix->contents.structured.sensor_data.pixel[i][p] = 0;
    }
    }

  uint16_t low_mask = 0x0001 << removed_noise_bits;
  uint16_t bit_mask;
  uint16_t plane_item = 0;
  for ( bit_mask = 0x8000; bit_mask >= low_mask; bit_mask >>= 1 ) {

    uint8_t  plane_byte = 0;
    uint8_t  plane_mask = 0x80;

    for ( i=0; i<number_of_data; i++ ) {
    for ( p=0; p<N_SPEC_PIX; p++ ) {

      if ( 0x80 == plane_mask ) {
        plane_byte = bp->contents.structured.sensor_data.bitplanes[plane_item];

      }

      if ( plane_byte & plane_mask ) {
        pix->contents.structured.sensor_data.pixel[i][p] |= bit_mask;
      }

      if ( 0x01 != plane_mask ) {
        plane_mask >>= 1;
      } else {
        plane_item++;
        plane_mask = 0x80;
      }

    }
    }

  }

  //  copy auxiliary 'as is'
  //
  memcpy ( pix->contents.structured.aux_data.spec_aux,
            bp->contents.structured.aux_data.spec_aux,  number_of_data*sizeof(Spec_Aux_Data_t) );
  
  return 0;
}
# endif

# if 0
int16_t data_packet_compress ( Profile_Data_Packet_t* unc, Profile_Data_Packet_t* com, char algorithm ) {

  //  Make sure input packet is uncompressed and unencoded
  //  bitplaning can handle either binary or grayscale
  if ( unc->header.compression != '0'
    || unc->header.ASCII_encoding != 'N' ) {
    return 1;
  }

  if ( algorithm != 'G' ) {
    return 2;
  }

  //  Copy most of header and adjust some entries
  //
  com->HYNV_num    = unc->HYNV_num;
  com->PROF_num    = unc->PROF_num;
  com->PCKT_num    = unc->PCKT_num;
  com->header.sensor_type = unc->header.sensor_type;
  com->header.empty_space = unc->header.empty_space;
  memcpy ( com->header.sensor_ID, unc->header.sensor_ID, 10 );
  memcpy ( com->header.number_of_data, unc->header.number_of_data, 4 );
  com->header.representation     = unc->header.representation;
  com->header.noise_bits_removed = unc->header.noise_bits_removed;
  com->header.compression        = algorithm;
  com->header.ASCII_encoding     = 'N';

  memcpy ( com->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data;
  sscanf ( unc->header.number_of_data, "%hu", &number_of_data );

  //  Compress bitplanes and auxiliary data using zlib
  //

  {
    int rv;

    z_stream strm;
    strm.zalloc  = pvPortMalloc;   //  FIXME
    strm.zfree   = vPortFree;   //  FIXME
    strm.opaque  = Z_NULL;   //  FIXME
    int level    = Z_DEFAULT_COMPRESSION; // 0..9
    int method   = Z_DEFLATED;
    int windowBits = 15;   // 9..15  Memory used = 1<<(windowBits+2)  **** when using 8, as permitted according to DOC, get failure at inflate()  ****
    int memLevel   =  8;   // 1..9   Memory used = 1<<(memLevel+9)
    int strategy   = Z_DEFAULT_STRATEGY;  // Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE (run-lengh encoding)

    windowBits = 9;
    memLevel   = 2;
    strategy   = Z_FILTERED;

    if ( Z_OK != deflateInit2 ( &strm, level, method, windowBits, memLevel, strategy ) ) {
      fprintf ( stderr, "deflateInit2() failed\n" );
    }

    strm.avail_in = number_of_data*2*N_SPEC_PIX;
    strm.next_in  = unc->contents.structured.sensor_data.bitplanes;

    strm.avail_out = FLAT_SZ;
    strm.next_out  = com->contents.flat_bytes;

# if 1
    if ( Z_STREAM_ERROR ==  (rv = deflate ( &strm, Z_NO_FLUSH ) ) ) {
      fprintf ( stderr, "deflate() failed %d\n", rv );
    }

    int done_1 = FLAT_SZ-strm.avail_out;
    //fprintf ( stderr, "deflate() outBP %d -> %d\n", number_of_data*2*N_SPEC_PIX, done_1 );

    if ( strm.avail_in ) {
      fprintf ( stderr, "deflate() did not consume all input %d\n", strm.avail_in );
    }

    strm.avail_in = number_of_data*sizeof(Spec_Aux_Data_t);
    strm.next_in  = (Bytef*)unc->contents.structured.aux_data.spec_aux;

    strm.avail_out = FLAT_SZ - done_1;
    strm.next_out  = com->contents.flat_bytes + done_1;

    if ( Z_STREAM_END != (rv = deflate ( &strm, Z_FINISH ) ) ) {
      fprintf ( stderr, "deflate() failed to finish (%d)\n", rv );
    }

    int done_2 = (FLAT_SZ-done_1) - strm.avail_out;
    //fprintf ( stderr, "deflate() outAUX %d -> %d\n", number_of_data*sizeof(Spec_Aux_Data_t), done_2 );

    (void)deflateEnd ( &strm );

    uint16_t total  = done_1 + done_2;
    //fprintf ( stderr, "deflate() out total %d -> %hd\n", number_of_data*2*N_SPEC_PIX+number_of_data*sizeof(Spec_Aux_Data_t), total );

# else
    if ( Z_STREAM_END != (rv = deflate ( &strm, Z_FINISH ) ) ) {
      fprintf ( stderr, "deflate() failed to finish (%d)\n", rv );
    }

    int done_1 = FLAT_SZ-strm.avail_out;

    (void)deflateEnd ( &strm );

    uint16_t total  = done_1;
    fprintf ( stderr, "deflate() out total %d -> %hd\n", number_of_data*2*N_SPEC_PIX, total );
# endif

    char numString[8]; 
    snprintf ( numString, 7, "%6hu", total );
    memcpy ( com->header.compressed_sz, numString, 6 );
  }
  
  return 0;
}
# endif

# if 0

int data_packet_uncompress ( Profile_Data_Packet_t* cmp, Profile_Data_Packet_t* ucp ) {
  //  Make sure input packet is compressed and unencoded
  //  bitplaning can handle either binary or grayscale
  if ( cmp->header.compression != 'G'
    || cmp->header.ASCII_encoding != 'N' ) {
    return 1;
  }

  //  Copy most of header and adjust some entries
  //
  ucp->HYNV_num    = cmp->HYNV_num;
  ucp->PROF_num    = cmp->PROF_num;
  ucp->PCKT_num    = cmp->PCKT_num;
  ucp->header.sensor_type = cmp->header.sensor_type;
  ucp->header.empty_space = cmp->header.empty_space;
  memcpy ( ucp->header.sensor_ID, cmp->header.sensor_ID, 10 );
  memcpy ( ucp->header.number_of_data, cmp->header.number_of_data, 4 );
  ucp->header.representation     = cmp->header.representation;
  ucp->header.noise_bits_removed = cmp->header.noise_bits_removed;
  ucp->header.compression        = '0';
  ucp->header.ASCII_encoding     = 'N';

  memcpy ( ucp->header.encoded_sz,    "      ", 6 );

  uint16_t number_of_data;
  sscanf ( cmp->header.number_of_data, "%hu", &number_of_data );

  uint16_t number_of_bytes;
  sscanf ( cmp->header.compressed_sz, "%hu", &number_of_bytes );

  //  Uncompress via zlib and rebuild bitplanes and auxiliary data
  //

  {
    int rv;

    z_stream strm;

    strm.zalloc  = pvPortMalloc;
    strm.zfree   = vPortFree;
    strm.opaque  = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    int windowBits = 15;   // 8..15  Memory used = 1<<(windowBits+2)

    windowBits = 9;

    if ( Z_OK != inflateInit2 ( &strm, windowBits ) ) {
      fprintf ( stderr, "inflateInit2() failed\n" );
    }

    strm.avail_in = (int)number_of_bytes;
    strm.next_in  = cmp->contents.flat_bytes;

    strm.avail_out = FLAT_SZ;
    strm.next_out  = ucp->contents.flat_bytes;

    rv = inflate ( &strm, Z_FINISH );
    switch ( rv ) {
    case Z_STREAM_ERROR:
    case Z_NEED_DICT:
    case Z_DATA_ERROR:
    case Z_MEM_ERROR:
      fprintf ( stderr, "inflate() failed %d\n", rv );
    }

    uint16_t uncompressed_sz = FLAT_SZ-strm.avail_out;

 // fprintf ( stderr, "inflate() out %hu -> %hu [have open space of %d]\n", number_of_bytes, uncompressed_sz, strm.avail_out );

    (void)inflateEnd ( &strm );

    if ( uncompressed_sz != number_of_data*2*N_SPEC_PIX+number_of_data*sizeof(Spec_Aux_Data_t) ) {
      fprintf ( stderr, "data_packet_uncompress() uncompressed to unexpected size: %hu instead of %d\n",
             uncompressed_sz, number_of_data*2*N_SPEC_PIX+number_of_data*sizeof(Spec_Aux_Data_t) );
    }

    //  The first part (spectrum) of the flat_bytes already overlays the bitplanes array.
    //
    //  The second part (auxiliary) is copied:
    memmove ( ucp->contents.structured.aux_data.spec_aux,
             (ucp->contents.flat_bytes)+number_of_data*2*N_SPEC_PIX,
              number_of_data*sizeof(Spec_Aux_Data_t) );

    memcpy ( ucp->header.compressed_sz, "      ", 6 );
  }
  
  return 0;
}
# endif

# if 0
int16_t data_packet_encode ( Profile_Data_Packet_t* unenc, Profile_Data_Packet_t* enc, char encoding ) {

  //  Make sure input packet is compressed and unencoded
  //  TODO: Add ability to encode ANY packet

  if ( unenc->header.compression == '0'
    || unenc->header.ASCII_encoding != 'N' ) {
    return 1;
  }

  //  encodings: 'A' == ASCII85
  //             'B' == BASE64

  if ( encoding != 'A' /* || encoding != 'B' */ ) {
    return 2;
  }

  //  Copy most of header and adjust some entries
  //
  enc->HYNV_num    = unenc->HYNV_num;
  enc->PROF_num    = unenc->PROF_num;
  enc->PCKT_num    = unenc->PCKT_num;
  enc->header.sensor_type = unenc->header.sensor_type;
  enc->header.empty_space = unenc->header.empty_space;
  memcpy ( enc->header.sensor_ID, unenc->header.sensor_ID, 10 );
  memcpy ( enc->header.number_of_data, unenc->header.number_of_data, 4 );
  enc->header.representation     = unenc->header.representation;
  enc->header.noise_bits_removed = unenc->header.noise_bits_removed;
  enc->header.compression        = unenc->header.compression;
  enc->header.ASCII_encoding     = encoding;

  memcpy ( enc->header.compressed_sz, unenc->header.compressed_sz, 6 );

  uint16_t number_of_bytes;
  sscanf ( unenc->header.compressed_sz, "%hu", &number_of_bytes );

  //  Encode
  //
  uint16_t encoded_sz = 0;


  fprintf ( stderr, "Encoding %hu bytes\n", number_of_bytes );

  uint16_t bytes = 0;
  uint32_t number = 0;
  uint16_t i;
  for ( i=0; i<number_of_bytes; i++ ) {

    uint8_t uByte = unenc->contents.flat_bytes[i];
    number |= uByte << (24-(bytes*8));
    bytes++;

    if ( 4 == bytes ) {

      if ( 0 == number ) {
        enc->contents.flat_bytes[encoded_sz++] = 'z';
  //fprintf ( stderr, "Encoding 'z'\n", number_of_bytes );
      } else if ( 0x20202020 == number ) {
        enc->contents.flat_bytes[encoded_sz++] = 'y';
  //fprintf ( stderr, "Encoding 'y'\n", number_of_bytes );
      } else {

        uint8_t encoded[5];
        int idx;
        for ( idx=4; idx>=0; idx-- ) {
          encoded[idx] = (uint8_t) ( (number%85) + 33 );
          number /= 85;
        }
        memcpy ( enc->contents.flat_bytes+encoded_sz, encoded, 5 );
        encoded_sz += 5;

      }

      bytes = 0;
      number = 0;
    }

  }

  if ( bytes ) {

  //fprintf ( stderr, "Encoding end %08x %d\n", number, bytes );
        uint8_t encoded[5];
        int idx;
        for ( idx=4; idx>=0; idx-- ) {
          encoded[idx] = (uint8_t) ( (number%85) + 33 );
          number /= 85;
        }
        memcpy ( enc->contents.flat_bytes+encoded_sz, encoded, 1+bytes );
        encoded_sz += 1+bytes;
  }

    char numString[8]; 
    snprintf ( numString, 7, "%6hu", encoded_sz );
    memcpy ( enc->header.encoded_sz, numString, 6 );

  return 0;
}
# endif

# if 0
int data_packet_decode ( Profile_Data_Packet_t* enc, Profile_Data_Packet_t* dec ) {

  //  Make sure input packet is compressed and unencoded
  //  TODO: Add ability to encode ANY packet

  if ( enc->header.compression == '0'
    || enc->header.ASCII_encoding == 'N' ) {
    return 1;
  }

  //  encodings: 'A' == ASCII85
  //             'B' == BASE64

  if ( enc->header.ASCII_encoding != 'A' /* || encoding != 'B' */ ) {
    return 2;
  }

  //  Copy most of header and adjust some entries
  //
  dec->HYNV_num    = enc->HYNV_num;
  dec->PROF_num    = enc->PROF_num;
  dec->PCKT_num    = enc->PCKT_num;
  dec->header.sensor_type = enc->header.sensor_type;
  dec->header.empty_space = enc->header.empty_space;
  memcpy ( dec->header.sensor_ID, enc->header.sensor_ID, 10 );
  memcpy ( dec->header.number_of_data, enc->header.number_of_data, 4 );
  dec->header.representation     = enc->header.representation;
  dec->header.noise_bits_removed = enc->header.noise_bits_removed;
  dec->header.compression        = enc->header.compression;
  dec->header.ASCII_encoding     = 'N';

  memcpy ( dec->header.compressed_sz, enc->header.compressed_sz, 6 );

  uint16_t compressed_sz;
  sscanf ( enc->header.compressed_sz, "%hu", &compressed_sz );
  uint16_t encoded_sz = 0;
  sscanf ( enc->header.encoded_sz, "%hu", &encoded_sz );

  //  Decode
  //
  uint16_t decoded_sz = 0;

  fprintf ( stderr, "Decoding %hu bytes\n", encoded_sz );

  uint16_t bytes = 0;
  uint32_t number = 0;
  const uint32_t pow85[5] = { 85u*85u*85u*85u, 85u*85u*85u, 85u*85u, 85u, 1u };

  uint16_t i;
  for ( i=0; i<encoded_sz; i++ ) {

    uint8_t ch = enc->contents.flat_bytes[i];

    if ( 0==bytes && 'z' == ch ) {
      number = 0;
      bytes  = 5;
    } else if ( 0==bytes && 'y' == ch ) {
      number = 0x20202020;
      bytes  = 5;
    } else if ( '!' <= ch && ch <= 'u' ) {
      number += (uint32_t)( pow85[bytes] * ( ch-'!' ) );
      bytes++;
    } else {
      //  ch outside specified range
      fprintf ( stderr, "Decoding OOR at %hu char %02x\n", i, ch );
      return 3;
    }

    if ( 5 == bytes ) {

      dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>24)        );
      dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>16) & 0xFF );
      dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>> 8) & 0xFF );
      dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number    ) & 0xFF );

      bytes = 0;
      number = 0;
    }

  }

  if ( 1 == bytes ) {
    //  if partial bytes, there must be at least two
    return 4;
  } else if ( 2 == bytes ) {
    number += 85L*85L*84L;
    fprintf ( stderr, "Decoding end %08x 2->1\n", number );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>24)        );
  } else if ( 3 == bytes ) {
    number += 85L*84L;
    fprintf ( stderr, "Decoding end %08x 3->2\n", number );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>24)        );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>16) & 0xFF );
  } else if ( 4 == bytes ) {
    number += 84;
    fprintf ( stderr, "Decoding end %08x 4->3\n", number );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>24)        );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>>16) & 0xFF );
    dec->contents.flat_bytes[decoded_sz++] = (uint8_t) ((number>> 8) & 0xFF );
  } else if ( 5 == bytes ) {
      fprintf ( stderr, "Decoding unexpected end of data\n" );
  }

  fprintf ( stderr, "Decoded  %hu bytes\n", decoded_sz );

  memcpy ( dec->header.encoded_sz, "      ", 6 );

  return 0;
}

int data_packet_compare( Profile_Data_Packet_t* p1, Profile_Data_Packet_t* p2, char ID1, char ID2 ) {

  fprintf ( stderr, "Comparing %c %c\n", ID1, ID2 );
  uint16_t metaDiff = 0;

  if ( p1->HYNV_num != p2->HYNV_num ) { metaDiff++; fprintf ( stderr, "Diff: HYNV_num '%hu' '%hu'\n", p1->HYNV_num, p2->HYNV_num ); }
  if ( p1->PROF_num != p2->PROF_num ) { metaDiff++; fprintf ( stderr, "Diff: PROF_num '%hu' '%hu'\n", p1->PROF_num, p2->PROF_num ); }
  if ( p1->PCKT_num != p2->PCKT_num ) { metaDiff++; fprintf ( stderr, "Diff: PCKT_num '%hu' '%hu'\n", p1->PCKT_num, p2->PCKT_num ); }

  if ( p1->header.sensor_type != p2->header.sensor_type ) { metaDiff++; fprintf ( stderr, "Diff: header.sensor_type '%c' '%c'\n", p1->header.sensor_type, p2->header.sensor_type ); }
  if ( p1->header.empty_space != p2->header.empty_space ) { metaDiff++; fprintf ( stderr, "Diff: header.empty_space '%c' '%c'\n", p1->header.empty_space, p2->header.empty_space ); }
  if ( memcmp( p1->header.sensor_ID, p2->header.sensor_ID, 10 ) ) { metaDiff++; fprintf ( stderr, "Diff: header.sensor_ID '%10.10' '%10.10'\n", p1->header.sensor_ID, p2->header.sensor_ID ); }
  if ( memcmp( p1->header.number_of_data, p2->header.number_of_data, 4 ) ) { metaDiff++; fprintf ( stderr, "Diff: header.number_of_data '%4.4s' '%4.4s'\n", p1->header.number_of_data, p2->header.number_of_data ); }

  if ( p1->header.representation != p2->header.representation ) { metaDiff++; fprintf ( stderr, "Diff: header.representation '%c' '%c'\n", p1->header.representation, p2->header.representation ); }
  if ( p1->header.noise_bits_removed != p2->header.noise_bits_removed ) { metaDiff++; fprintf ( stderr, "Diff: header.noise_bits_removed '%c' '%c'\n", p1->header.noise_bits_removed, p2->header.noise_bits_removed ); }
  if ( p1->header.compression != p2->header.compression ) { metaDiff++; fprintf ( stderr, "Diff: header.compression '%c' '%c'\n", p1->header.compression, p2->header.compression ); }
  if ( p1->header.ASCII_encoding != p2->header.ASCII_encoding ) { metaDiff++; fprintf ( stderr, "Diff: header.ASCII_encoding '%c' '%c'\n", p1->header.ASCII_encoding, p2->header.ASCII_encoding ); }

  if ( memcmp( p1->header.compressed_sz, p2->header.compressed_sz, 6 ) ) { metaDiff++; fprintf ( stderr, "Diff: header.compressed_sz '%6.6s' '%6.6s'\n", p1->header.compressed_sz, p2->header.compressed_sz ); }
  if ( memcmp( p1->header.encoded_sz, p2->header.encoded_sz, 6 ) ) { metaDiff++; fprintf ( stderr, "Diff: header.encoded_sz '%6.6s' '%6.6s'\n", p1->header.encoded_sz, p2->header.encoded_sz ); }

  uint16_t number_of_data, d, p;
  sscanf ( p1->header.number_of_data, "%hu", &number_of_data );

  if ( metaDiff ) {
      fprintf ( stderr, "Not comparing %hu data spectra\n", number_of_data );
  } else {
    if ( p1->header.noise_bits_removed == 'N'
      && p1->header.compression == '0'
      && p1->header.ASCII_encoding == 'N'
      && ( p1->header.sensor_type == 'S' || p1->header.sensor_type == 'P' ) ) {

      for ( d=0; d<number_of_data; d++ ) {
      for ( p=0; p<N_SPEC_PIX; p++ ) {
        if ( p1->contents.structured.sensor_data.pixel[d][p]
          != p2->contents.structured.sensor_data.pixel[d][p] ) {
            metaDiff++; fprintf ( stderr, "Diff: px[%hu][%hu] '%04hx' '%04hx'\n", d, p, p1->contents.structured.sensor_data.pixel[d][p], p2->contents.structured.sensor_data.pixel[d][p] ); }
      }
      }

      Spec_Aux_Data_t* aux1 = p1->contents.structured.aux_data.spec_aux;
      Spec_Aux_Data_t* aux2 = p2->contents.structured.aux_data.spec_aux;

      for ( d=0; d<number_of_data; d++ ) {
        if ( aux1->integration_time != aux2->integration_time ) { metaDiff++; fprintf ( stderr, "Diff:aux:integration_time %hu %hu\n", aux1->integration_time, aux2->integration_time ); }
        if ( aux1->sample_number != aux2->sample_number ) { metaDiff++; fprintf ( stderr, "Diff:aux:sample_number %hu %hu\n", aux1->sample_number, aux2->sample_number ); }
        if ( aux1->dark_average != aux2->dark_average ) { metaDiff++; fprintf ( stderr, "Diff:aux:dark_average %hu %hu\n", aux1->dark_average, aux2->dark_average ); }
        if ( aux1->dark_noise != aux2->dark_noise ) { metaDiff++; fprintf ( stderr, "Diff:aux:dark_noise %hu %hu\n", aux1->dark_noise, aux2->dark_noise ); }
        if ( aux1->spectrometer_temperature != aux2->spectrometer_temperature ) { metaDiff++; fprintf ( stderr, "Diff:aux:spectrometer_temperature %hd %hd\n", aux1->spectrometer_temperature, aux2->spectrometer_temperature ); }
        if ( aux1->acquisition_time.tv_sec != aux2->acquisition_time.tv_sec ) { metaDiff++; fprintf ( stderr, "Diff:aux:acquisition_time.tv_sec %d %d\n", aux1->acquisition_time.tv_sec, aux2->acquisition_time.tv_sec ); }
        if ( aux1->acquisition_time.tv_usec != aux2->acquisition_time.tv_usec ) { metaDiff++; fprintf ( stderr, "Diff:aux:acquisition_time.tv_usec %d %d\n", aux1->acquisition_time.tv_usec, aux2->acquisition_time.tv_usec ); }
        if ( aux1->pressure != aux2->pressure ) { metaDiff++; fprintf ( stderr, "Diff:aux:pressure %f %f\n", aux1->pressure, aux2->pressure ); }
        if ( aux1->float_heading != aux2->float_heading ) { metaDiff++; fprintf ( stderr, "Diff:aux:float_heading %hu %hu\n", aux1->float_heading, aux2->float_heading ); }
        if ( aux1->float_yaw != aux2->float_yaw ) { metaDiff++; fprintf ( stderr, "Diff:aux:float_yaw %hd %hd\n", aux1->float_yaw, aux2->float_yaw ); }
        if ( aux1->float_roll != aux2->float_roll ) { metaDiff++; fprintf ( stderr, "Diff:aux:float_roll %hd %hd\n", aux1->float_roll, aux2->float_roll ); }
        if ( aux1->radiometer_yaw != aux2->radiometer_yaw ) { metaDiff++; fprintf ( stderr, "Diff:aux:radiometer_yaw %hd %hd\n", aux1->radiometer_yaw, aux2->radiometer_yaw ); }
        if ( aux1->radiometer_roll != aux2->radiometer_roll ) { metaDiff++; fprintf ( stderr, "Diff:aux:radiometer_roll %hd %hd\n", aux1->radiometer_roll, aux2->radiometer_roll ); }
        if ( aux1->tag != aux2->tag ) { metaDiff++; fprintf ( stderr, "Diff:aux:tag %hu %hu\n", aux1->tag, aux2->tag ); }
        if ( aux1->side != aux2->side ) { metaDiff++; fprintf ( stderr, "Diff:aux:side %hu %hu\n", aux1->side, aux2->side ); }
      }

      // fread ( &(p->contents.structured.aux_data.spec_aux), number_of_data*sizeof(Spec_Aux_Data_t), 1, fq);
    }
  }

  return metaDiff;
}
# endif
