/*! \file profile_manager.c *******************************************************
 *
 * \brief profile manager task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# if defined(OPERATION_NAVIS)

# include "profile_manager.h"

# include <stdio.h>
# include <string.h>
# include <time.h>
# include <math.h>
# include <sys/time.h>

# include "FreeRTOS.h"
# include "semphr.h"

# include "tasks.controller.h"
# include "data_exchange_packet.h"
# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"
# include "profile_packet.shared.h"
# include "sram_memory_map.controller.h"
//# define sram_memcpy memcpy
# define sram_memset memset     //  FIXME

# include "datalogfile.h"
# include "frames.h"
  # include "telemetry.h"
# include "modem.h"
# include "syslog.h"
# include "io_funcs.controller.h"
# include "config.controller.h"
# include "extern.controller.h"

# include "zlib.h"

# define BRS_TEST 0

//*****************************************************************************
// Settings
//*****************************************************************************

//*****************************************************************************
// Local Variables
//*****************************************************************************

// Task control
static Bool gRunTask = FALSE;
static Bool gTaskIsRunning = FALSE;

static xSemaphoreHandle gTaskCtrlMutex = NULL;

// packet receiving queue
# define N_RX_PACKETS 6
static xQueueHandle rxPackets = NULL;

//  Use RAM for data received via packet transfer
typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Frame_t                          OCR_Frame;
  MCOMS_Frame_t                      MCOMS_Frame;
} any_received_data_t;
static data_exchange_data_package_t received_data_package;
static data_exchange_type_t         received_data_type = DE_Type_Nothing;

//  Use SRAM for sending data via packets
// static data_exchange_data_package_t sending_data_package;

//  Profile manager can be in different modes, and each mode needs variables to keep track:
//    *  profile acquisition  --> need profile meta data
//    *  profile transmission --> need to keep track

# define PMG_N_DATAFILES        4
typedef enum {
  PMG_DT_Starboard_Radiometer = 0,
  PMG_DT_Port_Radiometer      = 1,
  PMG_DT_OCR                  = 2,
  PMG_DT_MCOMS                = 3}
pmg_datatype_t;

# define PMG_PROFILE_FOLDER "NAVIS"
static char const* pmg_datafile_extension[PMG_N_DATAFILES] = { "SBD", "PRT", "OCR", "MCM" };

uint16_t const NO_PACKET_IN_TRANSFER = 0xFFFF;
typedef enum { PCK_Unsent, PCK_Sent, PCK_Confirmed } Packet_Status_t;
# define BST_Unsent    0x01
# define BST_Sent      0x02
# define BST_Confirmed 0x04

//! \brief  Generate profile management directory name.
//!
//! return  pointer to static char[]
char const* profile_manager_LogDirName () {
  return EMMC_DRIVE PMG_PROFILE_FOLDER;
}

# if 0
typedef struct pm_profining_data {
  U16 profile_number;
  time_t start_of_profiling;
  S32    yyyy_ddd;    //  4-digit year (2015 = 15, etc), 3-digit day of year, as an integer number
  F32    last_pressure_value;
  U16    total_frames[4];
} pm_profiling_data_t;
static pm_profiling_data_t pm_profiling = { 0, 0, 0, 0.0, {0,0,0,0} };

typedef struct pm_transmitting_data {
  U16    profile_number;
  S32    yyyy_ddd;    //  4-digit year (2015 = 15, etc), 3-digit day of year, as an integer number
  U8     total_packets[4];
  U8     txed_packets[4];
} pm_transmitting_data_t;
static pm_transmitting_data_t pm_transmitting = { 0, 0, {0,0,0,0}, {0,0,0,0} };
# endif

# define FIXED_BURST_SIZE 4096
typedef struct transmit_instructions {

//int   max_raw_packet_size;
  enum  { REP_GRAY, REP_BINARY } representation;
  enum  { BITPLANE, NO_BITPLANE } use_bitplanes;
  int   noise_bits_remove;
  enum  { ZLIB, NO_COMPRESSION } compression;
  enum  { ASCII85, BASE64, NO_ENCODING } encoding;
  int   burst_size;     

} transmit_instructions_t;
static transmit_instructions_t const tx_instruct = { /* 32*1024, */ REP_GRAY, BITPLANE, 0, ZLIB, ASCII85, FIXED_BURST_SIZE };

//*****************************************************************************
// Local Tasks Implementation
//*****************************************************************************
# if 0
static void timeval_add_usec ( struct timeval* tv, uint32_t usec ) {

  if ( tv ) {
    uint32_t  sec = usec / 1000000;
             usec = usec % 1000000;

    tv->tv_usec += usec;

    if ( tv->tv_usec > 1000000 ) {
              sec += tv->tv_usec / 1000000;
      tv->tv_usec  = tv->tv_usec % 1000000;
    }

    tv->tv_sec += sec;
  }
}
# endif

static void serialize_2byte ( uint8_t* destination, uint16_t value ) {
  destination[0] = value>>8;
  destination[1] = value&0xFF;
}

static void serialize_4byte ( uint8_t* destination, uint32_t value ) {
  destination[0] = (value>>24)&0xFF;
  destination[1] = (value>>16)&0xFF;
  destination[2] = (value>> 8)&0xFF;
  destination[3] =  value     &0xFF;
}

//! \brief  Start profiling
//!
//! @param  
//! return   0  OK
//! return  -1  FAILED due to misformatted profileID parameter
//! return  -2  FAILED due to file system access failure
static int16_t profile_start( uint16_t profileID ) {

  //  Check for valid profileID
  //
  if ( !profileID ) {
    return -1;
  }

  char pID_str[8];
  S32_to_str_dec ( (S32)profileID, pID_str, sizeof(pID_str), 5 );

  //  Generic variable, able to hold any file or folder name.
  //
  char fname[64];

  // Generic variable for file handler.
  //
  fHandler_t fh;

  //  Generic variable for strings to be written to file.
  //
  char msg[16];

  //  Check if top-level profiling folder exists.
  //  If it does not exist, create it.
  //  If creation fails, return error code.
  //
  strncpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER, 64 );

  if ( !f_exists( fname ) ) {
    if ( !FILE_OK == file_mkDir( fname ) ) {
      return -11;
    }
  }

  //  Check if navis profile record file (NRF) exists.
  //  If it does not exist, create it.
  //      If creation fails, return error code.
  //  If it does exist, confirm that the exisiting profileID is not yet present.
  //
  strncpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" PMG_PROFILE_FOLDER ".NRF", 64 );

  if ( !f_exists( fname ) ) {
    if ( FILE_OK != f_open ( fname, O_WRONLY | O_CREAT, &fh ) ) {
      return -21;
    }
    f_close ( &fh );
  } else {

    //  Make sure there is no entry for current profileID.
    //
    if ( FILE_OK != f_open ( fname, O_RDWR, &fh ) ) {
      return -22;
    }

    int conflict = 0;

    while ( 8 == f_read( &fh, msg, 8 ) ) {
      //  Make sure 8 byte record is well formatted:
      //  Expect [ACTZ]%05d\r\n
      if ( ( msg[0] == 'A' || msg[0] == 'C' || msg[0] == 'T' || msg[0] == 'Z' )
        && msg[6] == '\r' && msg[7] == '\n' ) {

        //  Find out if an entry for this profile is already present.
        //
        if ( profileID == atoi( msg+1 ) ) {

          f_seek ( &fh, 8, FS_SEEK_CUR_RE );  //  Rewind 8 bytes
          msg[0] = 'X';  //  Indicate profile renamed
          f_write ( &fh, msg, 1 );

	  int renamed = 0;
	  char letter;

	  for ( letter='A'; letter<='Z' && !renamed; letter++ ) {

	    strcpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
	    strcat ( fname, pID_str );

            char gname[64];
	    strcpy ( gname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
	    strcat ( gname, pID_str );

	    char ulz[4]; ulz[0] = '_'; ulz[1] = letter; ulz[2] = 0;
	    strcat ( gname, ulz );

	    if ( !f_exists( gname ) ) {
              f_move ( fname, gname );
	      renamed = 1;
	    }
	  }

	  if ( letter>'Z' ) {
            conflict = 1;
	  }

        }
      }
    }

    f_close ( &fh );

    if ( conflict ) {
      return -23;
    }
  }

  //  Check if folder for this profile exists.
  //  If it does not exist, create it.
  //  If creation fails, return error code.
  //
  strcpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( fname, pID_str );

  if ( !f_exists(fname) ) {
    if ( !FILE_OK == file_mkDir( fname ) ) {
      return -31;
    }
  }

  //  Create status file containing 5 lines:
  //    number of starboard data
  //    number of port      data
  //    number of ocr       data
  //    number of mcoms     data
  //  FIXME - In the future, may introduce free running pressure sensor ???
  //    "InProg"
  //
  strcpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( fname, pID_str );
  strcat ( fname, "\\" );
  strcat ( fname, pID_str );
  strcat ( fname, ".STS" );

  if ( FILE_OK != f_open ( fname, O_WRONLY | O_CREAT, &fh ) ) {
    return -41;
  }

  int i;
  for ( i=0; i<4; i++ ) {
    strcpy ( msg, "000000\r\n" );
    if ( (S32)strlen(msg) != f_write( &fh, msg, strlen(msg) ) ) {
      f_close ( &fh );
      return -(42-i);
    }
  }

  char const* status = "InProg\r\n";
  if ( (S32)strlen(status) != f_write( &fh, status, strlen(status) ) ) {
    f_close ( &fh );
    return -46;
  }

  f_close ( &fh );

  //  Create data log files
  //

  for ( i=0; i<PMG_N_DATAFILES; i++ ) {

    char data_file_name[34];
    strcpy ( data_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
    strcat ( data_file_name, pID_str );
    strcat ( data_file_name, "\\" );
    strcat ( data_file_name, pID_str );
    strcat ( data_file_name, "." );
    strcat ( data_file_name, pmg_datafile_extension[i] );

    if ( f_exists ( data_file_name ) ) {

      //  Unexpected: This data log file should not exist.
      //
      return -(51-2*i);

    } else {

      //  Create data log file.
      //
      if ( FILE_OK != f_open ( data_file_name, O_WRONLY | O_CREAT, &fh ) ) {
        return -(51-2*i-1);
      }

      f_close ( &fh );
    }
  }

  //  Add current profile to navis profile record file (NRF).
  //
  strncpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" PMG_PROFILE_FOLDER ".NRF", 64 );

  if ( FILE_OK != f_open ( fname, O_WRONLY | O_APPEND, &fh ) ) {
    return -61;
  }

  strcpy ( msg, "A" );
  strcat ( msg, pID_str );
  strcat ( msg, "\r\n" );

  if ( 8 != f_write( &fh, msg, 8 ) ) {
    f_close ( &fh );
    return -62;
  }

  f_close ( &fh );

  return 0;
}

static int16_t write_spec_data ( uint16_t profileID, Spectrometer_Data_t* spec_data ) {

  if ( !spec_data ) return -1;

  char const* extension;
  if ( 0 == spec_data->aux.side)
  {
    extension = pmg_datafile_extension[PMG_DT_Port_Radiometer];
  }
  
  else if ( 1 == spec_data->aux.side  )
  {
    extension = pmg_datafile_extension[PMG_DT_Starboard_Radiometer];
  }

  else
  {
    return -2;
  }

  char pID_str[8];
  S32_to_str_dec ( (S32)profileID, pID_str, sizeof(pID_str), 5 );

  char data_file_name[34];
  strcpy ( data_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "." );
  strcat ( data_file_name, extension );

  if ( !f_exists ( data_file_name ) ) {

      //  Unexpected: This data log file should exist.
      //
      return -3;

  }
  
  else
  {
      //  Append to data log file.
      //
      fHandler_t fh;

      if ( FILE_OK != f_open ( data_file_name, O_WRONLY | O_APPEND, &fh ) )
      {
        return -10;
      }

      int16_t written = -12;

      if ( sizeof(Spectrometer_Data_t) == f_write( &fh, spec_data, sizeof(Spectrometer_Data_t) ) )
      {
        written = 1;
      }

      f_close ( &fh );

      return written;
  }
}

static int16_t write_ocr_data ( uint16_t profileID, OCR_Frame_t* ocr_frame ) {

  if ( !ocr_frame ) return -1;

  char const* extension = pmg_datafile_extension[PMG_DT_OCR];

  char pID_str[8];
  S32_to_str_dec ( (S32)profileID, pID_str, sizeof(pID_str), 5 );

  char data_file_name[34];
  strcpy ( data_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "." );
  strcat ( data_file_name, extension );

  if ( !f_exists ( data_file_name ) ) {

      //  Unexpected: This data log file should exist.
      //
      return -3;

  } else {

      //  Append to data log file.
      //
      fHandler_t fh;

      if ( FILE_OK != f_open ( data_file_name, O_WRONLY | O_APPEND, &fh ) ) {
        return -10;
      }

      //  Data Frame Format:
      //  AS10 SATDI4####
      //  AS10 Timer
      //  BS2  Delay
      //  BU4  Pixel1
      //  BU4  Pixel2
      //  BU4  Pixel3
      //  BU4  Pixel4
      //  BU2  Volts
      //  BU2  Temperature
      //  BU1  Frame Counter
      //  BU1  Check Sum
      //  BU2  CRLF Terminator
      //    46 total bytes
      //

      uint16_t checksum = 0;

      //  TODO calculate checksum

      int16_t written = -12;

      if ( !checksum ) {

        OCR_Data_t od;

        memcpy ( &(od.pixel[0]), ocr_frame->frame+22, 4 );
        memcpy ( &(od.pixel[1]), ocr_frame->frame+26, 4 );
        memcpy ( &(od.pixel[2]), ocr_frame->frame+30, 4 );
        memcpy ( &(od.pixel[3]), ocr_frame->frame+34, 4 );

        od.aux.acquisition_time = ocr_frame->aux.acquisition_time;

        if ( sizeof(OCR_Data_t) == f_write( &fh, &od, sizeof(OCR_Data_t) ) ) {
          written = 1;
        }
      }

      f_close ( &fh );

      return written;
  }
}

static int16_t write_mcoms_data ( uint16_t profileID, MCOMS_Frame_t* mcoms_frame ) {

  if ( !mcoms_frame ) return -1;

  char const* extension = pmg_datafile_extension[PMG_DT_MCOMS];

  char pID_str[8];
  S32_to_str_dec ( (S32)profileID, pID_str, sizeof(pID_str), 5 );

  char data_file_name[34];
  strcpy ( data_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "\\" );
  strcat ( data_file_name, pID_str );
  strcat ( data_file_name, "." );
  strcat ( data_file_name, extension );

  if ( !f_exists ( data_file_name ) ) {

      //  Unexpected: This data log file should exist.
      //
      return -3;

  } else {

      //  Append to data log file.
      //
      fHandler_t fh;

      if ( FILE_OK != f_open ( data_file_name, O_WRONLY | O_APPEND, &fh ) ) {
        return -10;
      }

      //  MCOMS is variable length frame.
      //  The address points to the start of the frame, but the frame is not NUL character terminated.
      //  Search for CRLF terminator in frame, but cannot use strstr() due to non NUL termination.
      //  Start searching from the end of the frame.
      //
      MCOMS_Data_t md;

      int16_t written = -12;

      if ( 12 == sscanf ( (char*)mcoms_frame->frame, "MCOMSC-%*d"
                                              "\t%hd\t%hd\t%hd\t%ld"
                                              "\t%hd\t%hd\t%hd\t%ld"
                                         "\t%*d\t%hd\t%hd\t%hd\t%ld\r\n",
                          &(md.chl_led),  &(md.chl_low),  &(md.chl_hgh),  &(md.chl_value),
                           &(md.bb_led),   &(md.bb_low),   &(md.bb_hgh),   &(md.bb_value),
                         &(md.fdom_led), &(md.fdom_low), &(md.fdom_hgh), &(md.fdom_value) ) ) {

        md.aux.acquisition_time = mcoms_frame->aux.acquisition_time;

        if ( sizeof(MCOMS_Data_t) == f_write( &fh, &md, sizeof(MCOMS_Data_t) ) ) {
          written = 1;
        }
      }

      f_close ( &fh );
  
      return written;
  }
}

static int16_t profile_stop( uint16_t profileID, uint16_t frames[] ) {

  //  Generic variable, able to hold any file or folder name.
  //
  char fname[64];

  // Generic variable for file handler.
  //
  fHandler_t fh;

  //  Generic variable for strings to be written to file.
  //
  char msg[16];

  //  Create status file: Number of frames of each sensor + "IsDone"
  //    number of starboard data
  //    number of port      data
  //    number of ocr       data
  //    number of mcoms     data
  //  FIXME - In the future, may introduce free running pressure sensor ???
  //    "InProg"
  //
  char pID_str[8];
  S32_to_str_dec ( (S32)profileID, pID_str, sizeof(pID_str), 5 );

  strcpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( fname, pID_str );
  strcat ( fname, "\\" );
  strcat ( fname, pID_str );
  strcat ( fname, ".STS" );

  //  Do not execute this twice.
  //
  if ( FILE_OK == f_exists( fname ) ) return -1;

  if ( FILE_OK != f_open ( fname, O_WRONLY | O_CREAT, &fh ) ) {
    return -1;
  }

  int i;
  for ( i=0; i<4; i++ ) {
    S32_to_str_dec ( (S32)frames[i], msg, sizeof(msg), 6 );
    strcat ( msg, "\r\n" );

    S32 msg_len = strlen(msg);
    if ( msg_len != f_write( &fh, msg, msg_len ) ) {
      f_close ( &fh );
      return -2;
    }
  }

  char const* status = "IsDone\r\n";
  if ( (S32)strlen(status) != f_write( &fh, status, strlen(status) ) ) {
    f_close ( &fh );
    return -3;
  }

  f_close ( &fh );

  //  Update navis profile record file
  //
  strncpy ( fname, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" PMG_PROFILE_FOLDER ".NRF", 64 );

  if ( FILE_OK != f_open ( fname, O_RDWR, &fh ) ) {
    return -1;
  }

  int found_profile_id = 0;

  while ( 8 == f_read( &fh, msg, 8 ) ) {
    //  Make sure 8 byte record is well formatted:
    //  Expect [ACTZ]%05d\r\n
    if ( ( msg[0] == 'A' || msg[0] == 'C' || msg[0] == 'T' || msg[0] == 'Z' )
      && msg[6] == '\r' && msg[7] == '\n' ) {

      //  Find entry for this profile.
      //
      if ( profileID == atoi( msg+1 ) ) {
        f_seek ( &fh, 8, FS_SEEK_CUR_RE );  //  Rewind 8 bytes
        msg[0] = 'C';  //  Indicate profile completion
        if ( 1 == f_write ( &fh, msg, 1 ) ) {
          found_profile_id = 1;
        }
      }
    }
  }

  f_close ( &fh );

  return 0;
}



static int info_packet_burst_header24 ( Profile_Info_Packet_t* pip, 
                                        int                    bNum,
                                        int                    value,
                                        unsigned char*         header,
                                        uint16_t               header_len
                                      )
{
  if ( !header || header_len<24 ) return 1;

  uint16_t          nn = 0;
  uint16_t      tmpVal = 0;
  unsigned char tmpStr[32];

  memcpy ( header,    "BRST", 4 ); nn += 4;
  memcpy ( &tmpVal, pip->HYNV_num, 2 );
  S32_to_str_dec ( (S32)tmpVal, tmpStr, sizeof(tmpStr), 4 );
  memcpy ( header + nn, tmpStr, 4 ); nn += 4;
  
  memcpy (&tmpVal, pip->PROF_num, 2 );
  S32_to_str_dec ( (S32)tmpVal, tmpStr, sizeof(tmpStr), 5 );
  memcpy (header + nn, tmpStr, 5 ); nn += 5;
  
  memcpy   ( &tmpVal, pip->PCKT_num, 2 );
  S32_to_str_dec ( (S32)tmpVal, tmpStr, sizeof(tmpStr), 4 );
  memcpy( header + nn, tmpStr, 4 ); nn += 4;

  if  ( -1 == bNum )
  {
    //  Terminal burst for a given packet
    memcpy( header + nn, "ZZZ",  3 ); nn += 3;
    memcpy( header + nn, "ZZZZ", 4 ); nn += 4;
  }
  else
  {
    //  Normal Burst or Burst ZERO (report number of bursts via value field)
    S32_to_str_dec ( (S32)( bNum % 1000 ), tmpStr, sizeof(tmpStr), 3 );
    memcpy( header+nn, tmpStr, 3 );
    nn += 3;
    
    S32_to_str_dec ( (S32)(value % 10000), tmpStr, sizeof(tmpStr), 4 );
    memcpy( header+nn, tmpStr, 4 );
    nn += 4;
  }

  return 0;
}



static int info_packet_bursts_transmit (Profile_Info_Packet_t* pip, 
                                        int                    burst_size,
                                        int*                   numBurstsInPIP,
                                        int                    burstToTx
                                       )
{
  if  ( !pip )
    return 1;

  unsigned char*  data = ((unsigned char*)pip) + 6;
  uint16_t const nData = 4 * 4  +  4 * 4 + BEGPAK_META;

  int const numBursts = 1 + ( nData - 1 ) / burst_size;

  unsigned char header[24];
  char CRC32[9];
  uLong crc;

  if  (burstToTx == 0)
  {
    *numBurstsInPIP = numBursts;

    //  Burst ZERO is special: header only
    //
    info_packet_burst_header24 ( pip, 0, numBursts, header, 24 );

    //  Calculate header's CRC
    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if  ( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if  (  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
  }

  else if ( 1 <= burstToTx  &&  burstToTx <= numBursts )
  {
    unsigned char* burst_data = data + (burstToTx-1) * burst_size;
    int sz = (burstToTx < numBursts) ? burst_size : ( nData - (numBursts-1)*burst_size );

    info_packet_burst_header24 ( pip, burstToTx, sz, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    crc = crc32 ( crc, burst_data, sz );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if(  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
    if( sz != mdm_send ( burst_data, sz, MDM_USE_CTS, 5+sz/180 ) ) return 1;
  }

  else
  {
    //  Burst TERMINATOR is special: header only
    info_packet_burst_header24 ( pip, -1, 0, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if  ( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if  (  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
  }

  return 0;
}



static int data_packet_burst_header24 (Profile_Data_Packet_t* packet,
                                       int                    bNum,
                                       int                    value,
                                       unsigned char*         header,
                                       uint16_t               header_len
                                      )
{
  if ( !header || header_len<24 ) return 1;

  uint16_t         nn = 0;
  uint16_t      tmpVal;
  unsigned char tmpStr[32];

  memcpy (header,  "BRST", 4 ); nn += 4;

  memcpy (&tmpVal, packet->HYNV_num, 2 );
  snprintf ( tmpStr, 5, "%04hu", tmpVal );
  memcpy (header + nn, tmpStr, 4 ); nn += 4;

  memcpy   ( &tmpVal, packet->PROF_num, 2 );
  snprintf ( tmpStr, 6, "%05hu", tmpVal );
  memcpy( header+nn, tmpStr, 5 ); nn += 5;
  
  
  memcpy   ( &tmpVal, packet->PCKT_num, 2 );
  snprintf ( tmpStr, 5, "%04hu", tmpVal );
  memcpy (header + nn, tmpStr, 4); nn += 4;
  
  if  ( -1 == bNum)
  {
    //  Terminal burst for a given packet
    memcpy( header+nn, "ZZZ",  3 ); nn += 3;
    memcpy( header+nn, "ZZZZ", 4 ); nn += 4;
  }
  else
  {
    //  Normal Burst or Burst ZERO (report number of bursts via value field)
    snprintf ( tmpStr, 4, "%03d",   bNum%1000  );
    memcpy (header + nn, tmpStr, 3);  nn += 3;
    
    snprintf ( tmpStr, 5, "%04hu", value % 10000 );
    memcpy (header + nn, tmpStr, 4);  nn += 4;
  }

  return 0;
}



static int data_packet_size( Profile_Data_Packet_t* packet ) {

  //  Currently, neither compression nore ASCII encoding is implemented.
  //
  if ( packet->header.compression    != '0' ) return 0;
  if ( packet->header.ASCII_encoding != 'N' ) return 0;

  //  Find out the data type,
  //  and calculate the size of a single data item.
  //
  int size_of_single_datum = 0;
  int mxData = 0;

  switch ( packet->header.sensor_type ) {
  case 'S':
  case 'P':
            size_of_single_datum = (2048/8) * (16-(packet->header.noise_bits_removed-'0'))
                                 + SPEC_AUX_SERIAL_SIZE;
            mxData = MXHNV;
            break;
  case 'O':
            size_of_single_datum = 4*4 + OCR_AUX_SERIAL_SIZE;
            mxData = MXOCR;
            break;
  case 'M':
            size_of_single_datum = 3*(3*2+4) + MCOMS_AUX_SERIAL_SIZE;
            mxData = MXMCM;
            break;

  default:  return 0;
  }

  if ( size_of_single_datum <= 0 ) return 0;

  char numString[16];
  memcpy ( numString, packet->header.number_of_data, 4 );
  int number_of_data;
  sscanf ( numString, "%d", &number_of_data );

  if ( number_of_data <= 0 || number_of_data > mxData ) return 0;

  return number_of_data * size_of_single_datum;
}

static int data_packet_bursts_transmit ( Profile_Data_Packet_t* packet, int burst_size, int* numBurstsInPDP, int burstToTx ) {

  if ( !packet ) return 1;

  unsigned char*  data = ((unsigned char*)packet) + 6;
  uint16_t const nData = 32 + data_packet_size ( packet );  //  32: Packet header information
  if ( nData <=32 ) {
    return 1;
  }

  int const numBursts = 1 + ( nData - 1 ) / burst_size;

  unsigned char header[24];
  char CRC32[9];
  uLong crc;

  if  (burstToTx == 0 )
  {

    *numBurstsInPDP = numBursts;

    //  Burst ZERO is special: header only
    //
    data_packet_burst_header24 ( packet, 0, numBursts, header, 24 );

    //  Calculate header's CRC
    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if(  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
  }

  else if  ( 1 <= burstToTx  &&  burstToTx <= numBursts )
  {
    unsigned char* burst_data = data + (burstToTx-1) * burst_size;
    int sz = (burstToTx < numBursts) ? burst_size : ( nData - (numBursts - 1) * burst_size );

    data_packet_burst_header24 ( packet, burstToTx, sz, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    crc = crc32 ( crc, burst_data, sz );
    snprintf ( CRC32, 9, "%08X", crc );

    if  ( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if  (  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
    if  ( sz != mdm_send ( burst_data, sz, MDM_USE_CTS, 5+sz/180 ) ) return 1;
  }
  else
  {
 
    //  Burst TERMINATOR is special: header only
    //  Always send this, to facilitate receiver data assembly
    //
    data_packet_burst_header24 ( packet, -1, 0, header, 24 );

    crc = crc32 ( 0L, Z_NULL, 0 );
    crc = crc32 ( crc, header, 24 );
    snprintf ( CRC32, 9, "%08X", crc );

    if( 24 != mdm_send ( header, 24, MDM_USE_CTS, 5 ) ) return 1;
    if(  8 != mdm_send ( CRC32,   8, MDM_USE_CTS, 5 ) ) return 1;
  }

  return 0;
}

//////////////////////////////////////////////////////////////////////////
//
//  Read four sensor data files (Starboard, Port, OCR, MCOMS), and
//  on the fly, generate compressed packets, and
//  write those to individual packet files.
//
//  Later, during transfer, the finished packets can be read
//  and written as they are.
//
static int16_t profile_package (uint16_t*                      tx_profile_id,
                                Profile_Packet_Definition_t*   ppd,
                                transmit_instructions_t const* tx_instruct
                               )
{
  char numString[16];

  //  Use the profile_id value in the data structure to mark its content as valid
  //
  //  For now, it is invalid.
  //
  ppd->profile_id = 0;

  //  If the caller does not provide a profile ID for the transfer,
  //  retrieve the next profile in the queue from file.
  //
  if ( 0 == *tx_profile_id ) {
    //  TODO
  }

  if ( 0 == *tx_profile_id ) return -1;

  S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );

  char ppd_file_name[34];
  strcpy ( ppd_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( ppd_file_name, numString );
  strcat ( ppd_file_name, "\\" );
  strcat ( ppd_file_name, numString );
  strcat ( ppd_file_name, ".PPD" );
  
  if ( f_exists( ppd_file_name ) )
  {

    //  Retrieve existing ppd

    fHandler_t fh;

    if ( FILE_OK != f_open ( ppd_file_name, O_RDONLY, &fh ) )
    {
      return -11;
    }

    if ( sizeof(Profile_Packet_Definition_t) != f_read ( &fh, ppd, sizeof(Profile_Packet_Definition_t) ) )
    {
      f_close ( &fh );
      return -12;
    }

  }
  else
  {

    //  Get general information and at the end write ppd to file (for future re-use)
    //
    S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );

    char status_file_name[34];
    strcpy ( status_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
    strcat ( status_file_name, numString );
    strcat ( status_file_name, "\\" );
    strcat ( status_file_name, numString );
    strcat ( status_file_name, ".STS" );
  
    fHandler_t fh;

    if ( FILE_OK != f_open ( status_file_name, O_RDONLY, &fh ) )
    {
      return -3;
    }

    char msg[16];

    //  Read number of Starboard data sets
    //
    if ( 8 != f_read( &fh, msg, 8 ) )
    {
      f_close ( &fh );
      return -4;
    }
    msg[8] = 0;
    sscanf ( msg, "%hd", &(ppd->numData_SBRD) );

    //  Read number of Port data sets
    //
    if ( 8 != f_read( &fh, msg, 8 ) )
    {
      f_close ( &fh );
      return -4;
    }
    msg[8] = 0;
    sscanf ( msg, "%hd", &(ppd->numData_PORT) );

    //  Read number of OCR data sets
    //
    if ( 8 != f_read( &fh, msg, 8 ) )
    {
      f_close ( &fh );
      return -4;
    }
    msg[8] = 0;
    sscanf ( msg, "%hd", &(ppd->numData_OCR) );

    //  Read number of MCOMS data sets
    //
    if ( 8 != f_read( &fh, msg, 8 ) )
    {
      f_close ( &fh );
      return -4;
    }
    msg[8] = 0;
    sscanf ( msg, "%hd", &(ppd->numData_MCOMS) );

    //  Read acquisition status ("InProg\r\n" | "IsDone\r\n")
    //
    if ( 8 != f_read( &fh, msg, 8 ) ) {
      f_close ( &fh );
      return -4;
    }
    msg[8] = 0;

    //  TODO - How to handle not completed profiles???
    if ( strncmp( "IsDone\r\n", msg, 8 ) ) {
      //f_close ( &fh );
      //return -5;
    }

    f_close ( &fh );

    ppd->numPackets_SBRD  = ( ppd->numData_SBRD )
                          ? ( 1 + ( ppd->numData_SBRD  - 1) / MXHNV )
                          : 0;
    ppd->numPackets_PORT  = ( ppd->numData_PORT )
                          ? ( 1 + ( ppd->numData_PORT  - 1) / MXHNV )
                          : 0;
    ppd->numPackets_OCR   = ( ppd->numData_OCR  )
                          ? ( 1 + ( ppd->numData_OCR   - 1) / MXOCR )
                          : 0;
    ppd->numPackets_MCOMS = ( ppd->numData_MCOMS )
                          ? ( 1 + ( ppd->numData_MCOMS - 1) / MXMCM )
                          : 0;

    //  Use the profile_id value in the data structure to mark its content as valid
    //
    ppd->profile_id = *tx_profile_id;

    //  Write ppd to file for future re-use
    //
    if ( FILE_OK != f_open ( ppd_file_name, O_WRONLY | O_CREAT, &fh ) ) {
      return -6;
    }

    if ( sizeof(Profile_Packet_Definition_t) != f_write ( &fh, ppd, sizeof(Profile_Packet_Definition_t) ) ) {
      f_close ( &fh );
      return -7;
    }

    f_close ( &fh );
  }

  //  Generate the packets and write to file:
  //  Packet                           0                                == Info Packet
  //  Packet                        1 ... nSbrd                         == Starboard Radiometer Packets
  //  Packet nSbrd                + 1 ... nSbrd + nPort                 == Port Radiometer Packets
  //  Packet nSbrd + nPort        + 1 ... nSbrd + nPort + nOCR          == OCR Packets
  //  Packet nSbrd + nPort + nOCR + 1 ... nSbrd + nPort + nOCR + nMCOMS == MCOMS Packets
  //

  char packet_file_name[34];

  uint16_t num_packets = 0;  //  In the end, will be 1 + nSbrd + nPort + nOCR + nMCOMS

  Profile_Info_Packet_t pip;

  serialize_2byte ( pip.HYNV_num, ppd->profiler_sn );
  serialize_2byte ( pip.PROF_num, ppd->profile_id );
  serialize_2byte ( pip.PCKT_num, num_packets );

  snprintf ( numString, sizeof(numString), "%04hd", ppd->numData_SBRD );
  memcpy   ( pip.num_dat_SBRD, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numData_PORT );
  memcpy   ( pip.num_dat_PORT, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numData_OCR );
  memcpy   ( pip.num_dat_OCR, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numData_MCOMS );
  memcpy   ( pip.num_dat_MCOMS, numString, 4 );

  snprintf ( numString, sizeof(numString), "%04hd", ppd->numPackets_SBRD );
  memcpy   ( pip.num_pck_SBRD, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numPackets_PORT );
  memcpy   ( pip.num_pck_PORT, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numPackets_OCR );
  memcpy   ( pip.num_pck_OCR, numString, 4 );
  snprintf ( numString, sizeof(numString), "%04hd", ppd->numPackets_MCOMS );
  memcpy   ( pip.num_pck_MCOMS, numString, 4 );

  memset   ( pip.meta_info, '_', 6*16 );

  strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
  S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
  strncat ( packet_file_name, numString, 5 );
  strcat  ( packet_file_name, "\\" );
  strncat ( packet_file_name, numString, 5 );
  strcat  ( packet_file_name, ".P" );
  S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
  strncat ( packet_file_name, numString, 2 );;

  if ( 0 > info_packet_save_native( &pip, packet_file_name ) ) {
    return -1;
  }
  
  num_packets = 1;

tlm_send ( "PIP\r\n", 5, 0 );

  //////////////////////////////////////////////////////////////////////////
  //
  //  Starboard radiometer data -> packet files
  //
 Spectrometer_Data_t sp;
  {
  char sbrd_file_name[34];
  strncpy ( sbrd_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
  S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
  strncat ( sbrd_file_name, numString, 5 );
  strcat  ( sbrd_file_name, "\\" );
  strncat ( sbrd_file_name, numString, 5 );
  strcat  ( sbrd_file_name, "." );
  S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
  strcat ( sbrd_file_name, pmg_datafile_extension[PMG_DT_Starboard_Radiometer] );

  if ( f_exists ( sbrd_file_name ) ) {

    fHandler_t sfh;   //  starboard radiometer file handler

    if ( FILE_OK != f_open ( sbrd_file_name, O_RDONLY, &sfh ) ) {
      return -10;
    }

    uint16_t sP;
    for ( sP=1; sP<=ppd->numPackets_SBRD; sP++, num_packets++  )
    {

      tlm_send ( "SBP\r\n", 5, 0 );

      Profile_Data_Packet_t* raw = (Profile_Data_Packet_t*)sram_PMG_1;

      serialize_2byte ( raw->HYNV_num, ppd->profiler_sn );
      serialize_2byte ( raw->PROF_num, ppd->profile_id );
      serialize_2byte ( raw->PCKT_num, num_packets );

      raw->header.sensor_type = 'S';
      raw->header.empty_space = ' ';
      memcpy ( raw->header.sensor_ID, "SATYLU0000", 10 );  //  FIXME

      uint16_t number_of_data;
      if ( sP<ppd->numPackets_SBRD )
      {
       number_of_data = MXHNV;
      } else {  //  The last packet may contain fewer items
       number_of_data =  ppd->numData_SBRD
                      - (ppd->numPackets_SBRD-1)*MXHNV;
      }
      
      S32_to_str_dec ( (S32)number_of_data, numString, sizeof(numString), 4 );
      memcpy ( raw->header.number_of_data, numString, 4 );

      raw->header.representation     = 'G';
      raw->header.noise_bits_removed = '0' + tx_instruct->noise_bits_remove;
      int    const    LSB_Divisor = 1<<tx_instruct->noise_bits_remove;

      int d;
      for ( d=0; d<number_of_data; d++ ) {

        Spectrometer_Data_t* static_spec_data = &sp;

        f_read( &sfh, static_spec_data, sizeof(Spectrometer_Data_t) );

        //
        //  In-place: LSB-round and gray-code.
        //
        int p;
        for ( p=0; p < N_SPEC_PIX; p++ )
        {
          uint16_t const rndPx = (uint16_t) round ( ( (double)static_spec_data->hnv_spectrum[p] ) / LSB_Divisor );
          static_spec_data->hnv_spectrum[p] = rndPx ^ (rndPx>>1);
        }

        //
        //  Now generate bit-planes.
        //  This code is klunky, re-using tested code that workes for 'after all data are available'.
        //  Used klunky code to make sure nothing is broken.
        //
        int plane_item = 0;
        int bit_mask;
        for ( bit_mask = 0x8000>>tx_instruct->noise_bits_remove; bit_mask >= 0x0001; bit_mask >>= 1 )
        {
          uint8_t plane_byte = 0;
          uint8_t plane_mask = 0x80;

          int dd;
          for (dd = 0;  dd < number_of_data;  ++dd)
          {  
            //  Here is the KLUNKY part: Will only use the d==dd part of the loop, the d!=dd part is used to ensure the proper plane_item is generated.
            for ( p=0; p<N_SPEC_PIX; p++ )
            {
              if ( d==dd )
              {
                if ( static_spec_data->hnv_spectrum[p] & bit_mask )
                {
                  plane_byte |= plane_mask;
                }
              }
              if ( 0x01 != plane_mask )
              {
                plane_mask >>= 1;
              }
              else
              {
                if ( d==dd )
                {
                  raw->contents.structured.sensor_data.bitplanes[plane_item] = plane_byte;
                }
                plane_item++;
                plane_byte = 0;
                plane_mask = 0x80;
              }
            }
          }
        }

        //
        //  Serialize auxiliary data.
        //
# warning FIX TransFer Packet Spectrometer Auxiliary Content
        int nn = d*SPEC_AUX_SERIAL_SIZE;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.acquisition_time.tv_sec );   nn+=4;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.acquisition_time.tv_usec );  nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.integration_time );          nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.sample_number );             nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.dark_average );              nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.dark_noise );                nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.light_minus_dark_up_shift ); nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.spectrometer_temperature  ); nn+=2;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.pressure );                  nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_T_counts );         nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_T_duration );       nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_P_counts );         nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_P_duration );       nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.sun_azimuth  );              nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.housing_heading  );          nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.housing_pitch  );            nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.housing_roll  );             nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.spectrometer_pitch  );       nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.spectrometer_roll  );        nn+=2;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.tag );                       nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial + nn, static_spec_data->aux.side  );                     nn+=2;
/*DBG*/ //serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spec_min  );                 nn+=2;
/*DBG*/ //serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spec_max  );                 nn+=2;
      }

      //  ALERT: zlib is not working without tweaks to memory allocation / memory layout
      //         For now, cannot use data compression.
      if ( 0 ) {
        //
        //  Compress data
        //
        raw->header.compression = 'G';
        uint16_t total = 0;

        z_stream strm;
        strm.zalloc  = pvPortMalloc;   //  FIXME
        strm.zfree   = vPortFree;   //  FIXME
        strm.opaque  = Z_NULL;   //  FIXME
        int level    = Z_DEFAULT_COMPRESSION; // 0..9
        int method   = Z_DEFLATED;
        int windowBits =  9;   // 9..15  Memory used = 1<<(windowBits+2)  **** when using 8, as permitted according to DOC, get failure at inflate()  ****
        int memLevel   =  2;   // 1..9   Memory used = 1<<(memLevel+9)
        int strategy   = Z_DEFAULT_STRATEGY;  // Z_FILTERED, Z_HUFFMAN_ONLY, Z_RLE (run-lengh encoding)

        windowBits = 9;
        memLevel   = 2;
        strategy   = Z_FILTERED;

        if ( Z_OK != deflateInit2 ( &strm, level, method, windowBits, memLevel, strategy ) ) {
        }

        strm.avail_in = number_of_data*2*N_SPEC_PIX;  // FIXME -- LSB (noise) bits removed
        strm.next_in  = raw->contents.structured.sensor_data.bitplanes;

        strm.avail_out = FLAT_SZ;
        strm.next_out  = sram_PMG_2;

        int rv;
        if ( Z_STREAM_ERROR ==  (rv = deflate ( &strm, Z_NO_FLUSH ) ) ) {
        }

        int done_1 = FLAT_SZ-strm.avail_out;

        if ( strm.avail_in ) {
        }

        strm.avail_in = number_of_data*SPEC_AUX_SERIAL_SIZE;
        strm.next_in  = (Bytef*)raw->contents.structured.aux_data.spec_serial;

        strm.avail_out = FLAT_SZ - done_1;
        strm.next_out  = sram_PMG_2 + done_1;

        if ( Z_STREAM_END != (rv = deflate ( &strm, Z_FINISH ) ) ) {
        }

        int done_2 = (FLAT_SZ-done_1) - strm.avail_out;

        (void)deflateEnd ( &strm );

        total = done_1 + done_2;
        memcpy ( raw->contents.flat_bytes, sram_PMG_2, total );

        snprintf ( numString, 7, "%6hu", total );
        memcpy ( raw->header.compressed_sz, numString, 6 );
      } else {
        memcpy ( raw->contents.flat_bytes, raw->contents.structured.sensor_data.bitplanes,
                                            256*(16-tx_instruct->noise_bits_remove)*number_of_data );
        memcpy ( raw->contents.flat_bytes + 256*(16-tx_instruct->noise_bits_remove)*number_of_data,
                        raw->contents.structured.aux_data.spec_serial, SPEC_AUX_SERIAL_SIZE*number_of_data );

        raw->header.compression = '0';
        memcpy ( raw->header.compressed_sz, "      ", 6 );
      }

      //
      //  Do NOT ASCII85 Encode
      //
      raw->header.ASCII_encoding     = 'N';
      memcpy ( raw->header.encoded_sz,    "      ", 6 );

      //
      //  Write packet to file
      //
      strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
      S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, "\\" );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, ".P" );
      S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
      strncat ( packet_file_name, numString, 2 );

      if ( 0 > data_packet_save_native ( raw, packet_file_name ) ) {
        f_close( &sfh );
        return -1;
      }
    }

    f_close( &sfh );
  }
  }

  //////////////////////////////////////////////////////////////////////////
  //
  //  Port radiometer data -> packet files
  //
  {
  char port_file_name[34];

  strncpy ( port_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
  S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
  strncat ( port_file_name, numString, 5 );
  strcat  ( port_file_name, "\\" );
  strncat ( port_file_name, numString, 5 );
  strcat  ( port_file_name, "." );
  S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
  strcat ( port_file_name, pmg_datafile_extension[PMG_DT_Port_Radiometer] );
  
  if ( f_exists ( port_file_name ) ) {

    fHandler_t pfh;   //  port radiometer file handler

    if ( FILE_OK != f_open ( port_file_name, O_RDONLY, &pfh ) ) {
      return -10;
    }

    uint16_t pP;
    for ( pP=1; pP<=ppd->numPackets_PORT; pP++, num_packets++  ) {

tlm_send ( "PTP\r\n", 5, 0 );

      Profile_Data_Packet_t* raw = (Profile_Data_Packet_t*)sram_PMG_1;

      serialize_2byte ( raw->HYNV_num, ppd->profiler_sn );
      serialize_2byte ( raw->PROF_num, ppd->profile_id );
      serialize_2byte ( raw->PCKT_num, num_packets );

      raw->header.sensor_type = 'P';
      raw->header.empty_space = ' ';
      memcpy ( raw->header.sensor_ID, "SATYLU0000", 10 );  //  FIXME

      uint16_t number_of_data;
      if ( pP<ppd->numPackets_PORT ) {
       number_of_data = MXHNV;
      } else {  //  The last packet may contain fewer items
       number_of_data =  ppd->numData_PORT
                      - (ppd->numPackets_PORT-1)*MXHNV;
      }

      S32_to_str_dec ( (S32)number_of_data, numString, sizeof(numString), 4 );
      memcpy ( raw->header.number_of_data, numString, 4 );

      raw->header.representation     = 'G';
      raw->header.noise_bits_removed = '0' + tx_instruct->noise_bits_remove;
      int    const    LSB_Divisor = 1<<tx_instruct->noise_bits_remove;

      int d;
      for ( d=0; d<number_of_data; d++ ) {

        Spectrometer_Data_t* static_spec_data = &sp;

        f_read( &pfh, static_spec_data, sizeof(Spectrometer_Data_t) );

        //
        //  In-place: LSB-round and gray-code.
        //
        int p;
        for ( p=0; p<N_SPEC_PIX; p++ ) {
          uint16_t const rndPx = (uint16_t) round ( ( (double)static_spec_data->hnv_spectrum[p] ) / LSB_Divisor );
          static_spec_data->hnv_spectrum[p] = rndPx ^ (rndPx>>1);
        }

        //
        //  Now generate bit-planes.
        //  This code is klunky, re-using tested code that workes for 'after all data are available'.
        //  Used klunky code to make sure nothing is broken.
        //
        int plane_item = 0;
        int bit_mask;
        for ( bit_mask = 0x8000>>tx_instruct->noise_bits_remove; bit_mask >= 0x0001; bit_mask >>= 1 ) {
          uint8_t plane_byte = 0;
          uint8_t plane_mask = 0x80;

          int dd;
          for ( dd=0; dd<number_of_data; dd++ ) {  //  Here is the KLUNKY part: Will only use the d==dd part of the loop, the d!=dd part is used to ensure the proper plane_item is generated.
          for ( p=0; p<N_SPEC_PIX; p++ ) {
            if ( d==dd ) {
              if ( static_spec_data->hnv_spectrum[p] & bit_mask ) {
                plane_byte |= plane_mask;
              }
            }
            if ( 0x01 != plane_mask ) {
              plane_mask >>= 1;
            } else {
              if ( d==dd ) {
                raw->contents.structured.sensor_data.bitplanes[plane_item] = plane_byte;
              }
              plane_item++;
              plane_byte = 0;
              plane_mask = 0x80;
            }
          }
          }
        }

        //
        //  Serialize auxiliary data.
        //
# warning FIX TransFer Packet Spectrometer Auxiliary Content
        int nn = d*SPEC_AUX_SERIAL_SIZE;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.acquisition_time.tv_sec );   nn+=4;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.acquisition_time.tv_usec );  nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.integration_time );          nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.sample_number );             nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.dark_average );              nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.dark_noise );                nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.light_minus_dark_up_shift ); nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spectrometer_temperature  ); nn+=2;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure );                  nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_T_counts );         nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_T_duration );       nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_P_counts );         nn+=4;
/*DBG*/ //serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.pressure_P_duration );       nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.sun_azimuth  );              nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.housing_heading  );          nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.housing_pitch  );            nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.housing_roll  );             nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spectrometer_pitch  );       nn+=2;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spectrometer_roll  );        nn+=2;
        serialize_4byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.tag );                       nn+=4;
        serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.side  );                     nn+=2;
/*DBG*/ //serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spec_min  );                 nn+=2;
/*DBG*/ //serialize_2byte ( raw->contents.structured.aux_data.spec_serial+nn, static_spec_data->aux.spec_max  );                 nn+=2;
      }

      //  ALERT: zlib is not working without tweaks to memory allocation / memory layout
      //         For now, cannot use data compression.
      if ( 0 ) {
        //
        //  Compress data
        //
      } else {
        memcpy ( raw->contents.flat_bytes, raw->contents.structured.sensor_data.bitplanes,
                                            256*(16-tx_instruct->noise_bits_remove)*number_of_data );
        memcpy ( raw->contents.flat_bytes + 256*(16-tx_instruct->noise_bits_remove)*number_of_data,
                        raw->contents.structured.aux_data.spec_serial, SPEC_AUX_SERIAL_SIZE*number_of_data );

        raw->header.compression = '0';
        memcpy ( raw->header.compressed_sz, "      ", 6 );
      }

      //
      //  Do NOT ASCII85 Encode
      //
      raw->header.ASCII_encoding     = 'N';
      memcpy ( raw->header.encoded_sz,    "      ", 6 );

      //
      //  Write packet to file
      //
      strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
      S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, "\\" );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, ".P" );
      S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
      strncat ( packet_file_name, numString, 2 );

      if ( 0 > data_packet_save_native ( raw, packet_file_name ) ) {
        f_close( &pfh );
        return -1;
      }
    }

    f_close( &pfh );
  }
  }

  //////////////////////////////////////////////////////////////////////////
  //
  //  OCR data -> packet files
  //
  {
  char ocr_file_name[34];
  strncpy ( ocr_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
  S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
  strncat ( ocr_file_name, numString, 5 );
  strcat  ( ocr_file_name, "\\" );
  strncat ( ocr_file_name, numString, 5 );
  strcat  ( ocr_file_name, "." );
  S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
  strcat ( ocr_file_name, pmg_datafile_extension[PMG_DT_OCR] );

  if ( f_exists ( ocr_file_name ) ) {

    fHandler_t ofh;   //  ocr file handler

    if ( FILE_OK != f_open ( ocr_file_name, O_RDONLY, &ofh ) ) {
      return -10;
    }

    uint16_t oP;
    for ( oP=1; oP<=ppd->numPackets_OCR; oP++, num_packets++  ) {

tlm_send ( "OCP\r\n", 5, 0 );

      Profile_Data_Packet_t* raw = (Profile_Data_Packet_t*)sram_PMG_1;

      serialize_2byte ( raw->HYNV_num, ppd->profiler_sn );
      serialize_2byte ( raw->PROF_num, ppd->profile_id );
      serialize_2byte ( raw->PCKT_num, num_packets );

      raw->header.sensor_type = 'O';
      raw->header.empty_space = ' ';
      memcpy ( raw->header.sensor_ID, "SATYLU0000", 10 );  //  FIXME

      uint16_t number_of_data;
      if ( oP<ppd->numPackets_OCR ) {
       number_of_data = MXOCR;
      } else {  //  The last packet may contain fewer items
       number_of_data =  ppd->numData_OCR
                      - (ppd->numPackets_OCR-1)*MXOCR;
      }
      S32_to_str_dec ( (S32)number_of_data, numString, sizeof(numString), 4 );
      memcpy ( raw->header.number_of_data, numString, 4 );

      raw->header.representation     = 'B';
      raw->header.noise_bits_removed = 'N';
      raw->header.compression        = '0';
      raw->header.ASCII_encoding     = 'N';

      int d;
      for ( d=0; d<number_of_data; d++ ) {

        OCR_Data_t od;
        f_read( &ofh, &od, sizeof(OCR_Data_t) );

        int const offset = 4*4*d;
        int p;
        for ( p=0; p<4; p++ ) {
          serialize_4byte ( raw->contents.structured.sensor_data.ocr_serial+offset+4*p, od.pixel[p] );
        }

        serialize_4byte ( raw->contents.structured.aux_data.ocr_serial+d*OCR_AUX_SERIAL_SIZE, od.aux.acquisition_time.tv_sec );

        //  Copy to flat_bytes
        //
        memcpy ( raw->contents.flat_bytes, raw->contents.structured.sensor_data.ocr_serial,
                                            4*4*number_of_data );
        memcpy ( raw->contents.flat_bytes + 4*4*number_of_data,
                        raw->contents.structured.aux_data.spec_serial, OCR_AUX_SERIAL_SIZE*number_of_data );
      }

      memcpy ( raw->header.compressed_sz, "      ", 6 );
      memcpy ( raw->header.encoded_sz,    "      ", 6 );

      strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
      S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, "\\" );
      strncat ( packet_file_name, numString, 5 );
      strcat  ( packet_file_name, ".P" );
      S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
      strncat ( packet_file_name, numString, 2 );

      if ( 0 > data_packet_save_native ( raw, packet_file_name ) ) {
        f_close( &ofh );
        return -1;
      }
    }

    f_close( &ofh );
  }
  }

  //////////////////////////////////////////////////////////////////////////
  //
  //  MCOMS data -> packet files
  //
  {
    char mcoms_file_name[34];
    
    strncpy ( mcoms_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
    S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
    strncat ( mcoms_file_name, numString, 5 );
    strcat  ( mcoms_file_name, "\\" );
    strncat ( mcoms_file_name, numString, 5 );
    strcat  ( mcoms_file_name, "." );
    S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
    strcat ( mcoms_file_name, pmg_datafile_extension[PMG_DT_MCOMS] );
    
    if  ( f_exists ( mcoms_file_name ) )
    {
      fHandler_t mfh;   //  mcoms file handler
    
      if ( FILE_OK != f_open ( mcoms_file_name, O_RDONLY, &mfh ) )
      {
        return -10;
      }
    
      uint16_t mP;
      for  (mP = 1;  mP <= ppd->numPackets_MCOMS;  mP++, num_packets++)
      {
        tlm_send ("MCP\r\n", 5, 0);

        Profile_Data_Packet_t* raw = (Profile_Data_Packet_t*)sram_PMG_1;

        serialize_2byte ( raw->HYNV_num, ppd->profiler_sn );
        serialize_2byte ( raw->PROF_num, ppd->profile_id );
        serialize_2byte ( raw->PCKT_num, num_packets );
        
        raw->header.sensor_type = 'M';
        raw->header.empty_space = ' ';
        memcpy ( raw->header.sensor_ID, "SATYLU0000", 10 );  //  FIXME
        
        uint16_t number_of_data;
        if  (mP < ppd->numPackets_MCOMS)
        {
          number_of_data = MXMCM;
        }
        else
        {
          //  The last packet may contain fewer items
          number_of_data =  ppd->numData_MCOMS
                         - (ppd->numPackets_MCOMS-1)*MXMCM;
        }

        S32_to_str_dec ( (S32)number_of_data, numString, sizeof(numString), 4 );
        
        memcpy ( raw->header.number_of_data, numString, 4 );
        
        raw->header.representation     = 'B';
        raw->header.noise_bits_removed = 'N';
        raw->header.compression        = '0';
        raw->header.ASCII_encoding     = 'N';
        
        int d;
        for ( d=0; d<number_of_data; d++ )
        {
          MCOMS_Data_t md;
          f_read( &mfh, &md, sizeof(MCOMS_Data_t) );
        
          int const offset = 3*(3*2+4)*d;
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+ 0, md. chl_led );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+ 2, md. chl_low );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+ 4, md. chl_hgh );
          serialize_4byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+ 6, md. chl_value );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+10, md.  bb_led );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+12, md.  bb_low );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+14, md.  bb_hgh );
          serialize_4byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+16, md.  bb_value );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+20, md.fdom_led );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+22, md.fdom_low );
          serialize_2byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+24, md.fdom_hgh );
          serialize_4byte ( raw->contents.structured.sensor_data.mcoms_serial+offset+26, md.fdom_value );
        
          serialize_4byte ( raw->contents.structured.aux_data.mcoms_serial+d*OCR_AUX_SERIAL_SIZE, md.aux.acquisition_time.tv_sec );
        
          //  Copy to flat_bytes
          //
          memcpy ( raw->contents.flat_bytes, raw->contents.structured.sensor_data.mcoms_serial,
                                              3*(3*2+4)*number_of_data );
          memcpy ( raw->contents.flat_bytes + 3*(3*2+4)*number_of_data,
                          raw->contents.structured.aux_data.mcoms_serial, MCOMS_AUX_SERIAL_SIZE*number_of_data );
        }
        
        memcpy ( raw->header.compressed_sz, "      ", 6 );
        memcpy ( raw->header.encoded_sz,    "      ", 6 );
        
        strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
        S32_to_str_dec ( (S32)(*tx_profile_id), numString, sizeof(numString), 5 );
        strncat ( packet_file_name, numString, 5 );
        strcat  ( packet_file_name, "\\" );
        strncat ( packet_file_name, numString, 5 );
        strcat  ( packet_file_name, ".P" );
        S32_to_str_dec ( (S32)num_packets, numString, sizeof(numString), 2 );
        strncat ( packet_file_name, numString, 2 );
        
        if ( 0 > data_packet_save_native ( raw, packet_file_name ) )
        {
          f_close( &mfh );
          return -1;
        }
      }

      f_close( &mfh );
    }
  }

  return 0;
}



# define CON_MDM_Asserted_DSR    0x0001
# define CON_MDM_Responded_OK    0x0002
# define CON_MDM_Is_Initialized  0x0004
# define CON_MDM_Is_Configured   0x0008
# define CON_MDM_Is_Registered   0x0010
# define CON_MDM_Has_Signal      0x0020
# define CON_MDM_Dialed_In       0x0040
# define CON_MDM_Logged_In       0x0080
# define CON_MDM_Communicating   0x0100

static int open_rudics_server ( int* con_state )
{
  *con_state = 0;

  char rt[64];
  char* msg;

  mdm_rts(1);

  if ( !mdm_wait_for_dsr() )
  {
    mdm_dtr(0); return 0;
  }

  *con_state |= CON_MDM_Asserted_DSR;

  if ( !mdm_getAtOk() )
  {
    mdm_dtr(0); return 0;
  }

  *con_state |= CON_MDM_Responded_OK;

  if  ( 1 != mdm_configure () )
  {
    mdm_dtr(0);
    return 0;
  }

  *con_state |= CON_MDM_Is_Initialized;
  *con_state |= CON_MDM_Is_Configured;

  if  ( !mdm_isRegistered () )
  {
    if  ( !mdm_isRegistered () )
    {
      if  ( !mdm_isRegistered () )
      {
        if  ( !mdm_isRegistered () )
        {
          mdm_dtr(0);
          return 0;
        }
      }
    }
  }
  
  *con_state |= CON_MDM_Is_Registered;

  {
    int attempt = 0;
    int signalStrength = 0;
    int const neededSignalStrength = 25;

    do
    {
      msg = "Modem sgnal: ";
      tlm_send ( msg, strlen(msg), 0 );
      
      int num, min, avg, max;
      if  (( num = mdm_getSignalStrength ( 3+attempt, &min, &avg, &max ) ) > 0 )
      {
        signalStrength = avg;
        snprintf ( rt, 63, "%d (%d) [%d..%d]\r\n", avg, num, min, max );
        tlm_send ( rt, strlen(rt), 0 );
      }
      else
      {
        msg = "N/A";
        tlm_send ( msg, strlen(msg), 0 );
      }
    }
    while ( attempt++<=5 && signalStrength < neededSignalStrength );

    if ( signalStrength < neededSignalStrength )
    {
      mdm_dtr(0); return 0;
    }
  }

  *con_state |= CON_MDM_Has_Signal;
  
  char dial[32];
  DialString (dial, 31);

  if  ( mdm_connect ( dial, 90) <= 0 )
  {
    if  ( mdm_connect ( dial, 90) <= 0 )
    {
      if  ( mdm_connect ( dial, 90) <= 0 )
      {
        io_out_string ( dial );
        io_out_string ( "\r\n" );
        {
          char* pD = dial; 
          while ( *pD )
          {
            io_out_S32 ( "# %ld\r\n", *pD );
            pD++;
          }
        }

        mdm_dtr(0); return 0;
      }
    }
  }
 
  *con_state |= CON_MDM_Dialed_In;

  char login[8];
  login[0] = 'f';
  S32_to_str_dec ( (S32)CFG_Get_Serial_Number (), login + 1, sizeof(6), 4 );
  login[5] = '\r';
  login[6] = 0;


  //
  //  Initialy, wait 15 sec for login to be offered.
  //  If no login is offered, then send a Line-Feed ('\n'),
  //  which is supposed to trigget the terminal process
  //  forked by the rudics server to offer the "login:" prompt.
  //  Repeat a few times.
  //

  if  ( mdm_login(60, login, "S(n)=2n\r" ) <= 0 )
  { 
    io_out_string ( "Retry\r\n" );
    char* const LF_exit_LF = "\nexit\n";

    mdm_send ( LF_exit_LF, 6, 0, 1 );
    vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
    
    if  ( mdm_login (25, login, "S(n)=2n\r" ) <= 0 )
    {
      io_out_string ( "Retry\r\n" );
      mdm_send ( LF_exit_LF, 6, 0, 1 );
      vTaskDelay ( (portTickType)TASK_DELAY_MS( 3000 ) );
    
      if  ( mdm_login (25, login, "S(n)=2n\r" ) <= 0 )
      {
        io_out_string ( "Failed\r\n" );
        io_out_string ( login );
        io_out_string ( "\r\n" );
        {
          char* pD = login;
          while ( *pD ) 
          {
            io_out_S32 ( "# %ld\r\n", *pD );
            pD++;
          }
        }
        mdm_dtr (0);
        return 0;
      }
    }
  }

  *con_state |= CON_MDM_Logged_In;

  char const* command = "brx\n";
  int const cmdlen = strlen(command);
  time_t const  send_timeout = 2;

  if  (cmdlen != mdm_send( command, cmdlen, 0, send_timeout ) )
  {
    mdm_dtr(0);
    return 0;
  }

  *con_state |= CON_MDM_Communicating;

  vTaskDelay ((portTickType)TASK_DELAY_MS ( 3000 ) );

  return 1;
}



static void close_rudics_server (int con_state)
{
  mdm_logout();  //  Deliberately ignoring failure
  mdm_escape();  //  Deliberately ignoring failure
  mdm_hangup();  //  Deliberately ignoring failure
  mdm_dtr(0);

  return;
}



# define PMG_PRF_TX_ALL_DONE 0
# define PMG_PRF_TX_CONTINUE 1
# define PMG_PRF_TX_MDM_FAIL 2
static int pmg_profile_transmission (uint16_t         profileID,
                                     uint16_t         numPackets, 
                                     Packet_Status_t* packet_status,
                                     uint16_t*        packet_bursts,
                                     uint32_t*        burst_status 
                                    )
{
  char mmm[32];

  static int connected = 0;
  static int connection_state = 0;

  static uint16_t packet_in_transfer = NO_PACKET_IN_TRANSFER;
  static Profile_Info_Packet_t  transferring_pip;
  /*static*/ Profile_Data_Packet_t* transferring_pdp = sram_PMG_2;

  if  (packet_in_transfer == NO_PACKET_IN_TRANSFER  ||  packet_status[packet_in_transfer] != PCK_Unsent)
  {
    //  FIXME -- In final version, check for == confirmed

    packet_in_transfer = NO_PACKET_IN_TRANSFER;

    //  No packet currently transferring. Find next packet to transfer.
    int  p;
    for  (p = 1;  p < numPackets;  p++)
    {

      if  (PCK_Unsent == packet_status[p])
      {
        packet_in_transfer = p;

        char packet_file_name[34];
        strncpy ( packet_file_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\", 34 );
        S32_to_str_dec ( (S32)(profileID), mmm, sizeof(mmm), 5 );
        strncat ( packet_file_name, mmm, 5 );
        strcat  ( packet_file_name, "\\" );
        strncat ( packet_file_name, mmm, 5 );
        strcat  ( packet_file_name, ".P" );
        S32_to_str_dec ( (S32)packet_in_transfer, mmm, sizeof(mmm), 2 );
        strncat ( packet_file_name, mmm, 2 );

        if ( 0 == packet_in_transfer )
        {
          info_packet_retrieve_native (&transferring_pip, packet_file_name );
        }
        else
        {
          data_packet_retrieve_native( 12, packet_file_name );
        }

        break;
      }
    }

    if  (packet_in_transfer == NO_PACKET_IN_TRANSFER )
    {
      if ( connected )
      {
        //  Let BRX terminate, then log out
        vTaskDelay( (portTickType)TASK_DELAY_MS( 75000 ) );

        close_rudics_server (connection_state);
        connected = 0;
        connection_state = 0;
      }
      return PMG_PRF_TX_ALL_DONE;
    }
  }

  //  At this point, if packet_in_transfer == NO_PACKET_IN_TRANSFER,
  //  that implies all packets were already sent.
  //

  if ( packet_in_transfer == NO_PACKET_IN_TRANSFER )
  {
    //  FIXME: Check for rudics received/resend records.
    //
    char rudicsMessage[128];
    if   (mdm_recv (rudicsMessage, 128, MDM_NONBLOCK))
    {
      //  Resend Burst request
      //  ...
# if 0
      uint16_t const requested_profile = packet_rx_via_queue.data.Command.value.u16[0];

      if  (requested_profile == profileID)
      {
        uint16_t const requested_packet = packet_rx_via_queue.data.Command.value.u16[1];

        if  ( /* 0 <= requested_packet && */ requested_packet < numPackets )
        {
          switch ( packet_status[requested_packet] )
          {
          case PCK_Unsent:
                  // nothing to do, was already planning to send
                  break;
          case PCK_Sent:
                  packet_status[requested_packet] = PCK_Unsent;
                  break;
          case PCK_Confirmed:
                  //  Assume the earlier confirmation was in error
                  packet_status[requested_packet] = PCK_Unsent;
                  break;
          }
        }
      }
# endif
      //  Received burst confirmation
      //  ...
    }

    //  Check for status of all packets.
    //  If all sent and all confirmed / no resend requests
    //  then  close rudics connection & return PMG_PRF_TX_ALL_DONE;
    //
  }

  if  (packet_in_transfer != NO_PACKET_IN_TRANSFER)
  {

    if  (!connected  ||  !mdm_carrier_detect ())
    {
      //  (Re)-establish connection to rudics server
      //
      if  (!open_rudics_server (&connection_state))
      {
        snprintf (mmm, sizeof(mmm), "RUDICS Not Connected %04x\r\n", connection_state);
        tlm_send (mmm, strlen(mmm), 0);
        close_rudics_server( connection_state);
        connected = 0;
        return PMG_PRF_TX_MDM_FAIL;
      }
      else
      {
        tlm_send ( "RUDICS Connected\r\n", 19, 0 );
        connected = 1;
      }
    }

    if  (connected)
    {

      //   snprintf ( mmm, sizeof(mmm), "ToTx %02hu %04x\r\n", packet_in_transfer, burst_status[packet_in_transfer] );
      //   tlm_send ( mmm, strlen(mmm), 0 );

      if  (0 == packet_in_transfer)
      {
        if  ((BST_Unsent << (3 * 0)) & burst_status[packet_in_transfer])
        {
          int nB;
          if  (!info_packet_bursts_transmit (&transferring_pip, tx_instruct.burst_size, &nB, 0))
          {
            packet_bursts[packet_in_transfer] = nB;
            burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*0));
            burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*0));
            snprintf ( mmm, sizeof(mmm), "Txed %02hu.0\r\n", packet_in_transfer );
            tlm_send ( mmm, strlen(mmm), 0 );
            return PMG_PRF_TX_CONTINUE;
          }
        }
        else
        {
          int b; int nB = packet_bursts[packet_in_transfer];
          for  (b = 1;  b <= nB + 1;  b++)
          {
            if  ((BST_Unsent << (3 * b))  &  burst_status[packet_in_transfer])
            {
              if  (!info_packet_bursts_transmit (&transferring_pip, tx_instruct.burst_size, &nB, b))
              {
                burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*b));
                burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*b));
                snprintf ( mmm, sizeof(mmm), "Txed %02hu.%d\r\n", packet_in_transfer, b );
                tlm_send ( mmm, strlen(mmm), 0 );

                if ( b==nB )
                {
                  //  Send terminating burst immediately after last data burst
                  if ( !info_packet_bursts_transmit( &transferring_pip, tx_instruct.burst_size, &nB, nB+1 ) )
                  {
                    burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*(b+1)));
                    burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*(b+1)));
                    packet_status[packet_in_transfer] = PCK_Sent;
                  }
                }
                return PMG_PRF_TX_CONTINUE;
              }
            }
          }
        }
      }
      else
      {
        if  ((BST_Unsent << (3 * 0)) & burst_status[packet_in_transfer])
        {
          int nB;
          if  (!data_packet_bursts_transmit (transferring_pdp, tx_instruct.burst_size, &nB, 0))
          {
            packet_bursts[packet_in_transfer] = nB;
            burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*0));
            burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*0));
            snprintf ( mmm, sizeof(mmm), "Txed %02hu.0\r\n", packet_in_transfer );
            tlm_send ( mmm, strlen(mmm), 0 );
            return PMG_PRF_TX_CONTINUE;
          }
        }
        else
        {
          int b; int nB = packet_bursts[packet_in_transfer];
          for ( b=1; b<=nB; b++ )
          {
            if  ((BST_Unsent<<(3*b)) & burst_status[packet_in_transfer])
            {
              if ( !data_packet_bursts_transmit( transferring_pdp, tx_instruct.burst_size, &nB, b ) )
              {
                burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*b));
                burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*b));
                snprintf ( mmm, sizeof(mmm), "Txed %02hu.%d\r\n", packet_in_transfer, b );
                tlm_send ( mmm, strlen(mmm), 0 );

                if ( b==nB )
                {
                  //  Send terminating burst immediately after last data burst
                  if ( !data_packet_bursts_transmit( transferring_pdp, tx_instruct.burst_size, &nB, nB+1 ) )
                  {
                    burst_status [packet_in_transfer] &= ~(BST_Unsent<<(3*(b+1)));
                    burst_status [packet_in_transfer] |=  (BST_Sent  <<(3*(b+1)));
                    packet_status[packet_in_transfer] = PCK_Sent;
                  }
                }
                return PMG_PRF_TX_CONTINUE;
              }
            }
          }
        }
      }
    }
  }

  return PMG_PRF_TX_CONTINUE;
}



static void specdata_fake( Spectrometer_Data_t* s, int side )
{
  s->aux.side = side;

  int i;
  for ( i=0; i<N_SPEC_PIX; i++ )
  {
    s->hnv_spectrum[i] = rand() & 0xFFFF;
  }
}



static void ocrframe_fake( OCR_Frame_t* o )
{
  o->aux.acquisition_time.tv_sec = 0;
}

static void mcomsframe_fake( MCOMS_Frame_t* m )
{
  m->aux.acquisition_time.tv_sec = 0;
}



static Transfer_Status_t profile_fake( uint16_t profileID )
{

  int16_t rv;

  char profile_folder_name[24];
  char numString[8];
  S32_to_str_dec ( (S32)(profileID), numString, sizeof(numString), 5 );
  strcpy ( profile_folder_name, EMMC_DRIVE PMG_PROFILE_FOLDER "\\" );
  strcat ( profile_folder_name, numString );

  if ( !f_exists( profile_folder_name ) )
  {

    # if 1 //  Do not generate fake profile
    return Tx_Sts_No_Profile;

    # else //  Do generate a fake profile (for testing without a spectrometer board)

    io_out_S32( "FP %ld\r\n", (S32)profileID );

    if ( (rv=profile_start( profileID )) < 0 )
    {
      char msg[32];
      snprintf ( msg, 31, "PS %hd\r\n", rv );
      tlm_send( msg, strlen(msg), 0 );
      return Tx_Sts_No_Profile;
    }

    uint16_t frames[4] = { 5, 5, 5, 5 };
    int i;
    
    for ( i=0; i<frames[0]; i++ )
    {
      Spectrometer_Data_t sp;
      Spectrometer_Data_t* static_spec_data = &sp;
      specdata_fake ( static_spec_data, 0 );
      if ( write_spec_data ( profileID, static_spec_data ) <= 0 ) return Tx_Sts_Empty_Profile;
    }
    tlm_send ( "SS\r\n", 4, 0 );

    for ( i=0; i<frames[1]; i++ )
    {
      Spectrometer_Data_t sp;
      Spectrometer_Data_t* static_spec_data = &sp;
      specdata_fake ( static_spec_data, 1 );
      if ( write_spec_data ( profileID, static_spec_data ) <= 0 )
        return Tx_Sts_Empty_Profile;
    }
    tlm_send ( "SP\r\n", 4, 0 );

    for ( i=0; i<frames[2]; i++ )
    {
      OCR_Frame_t ocr;
      ocrframe_fake( &ocr );
      if ( write_ocr_data ( profileID, &ocr ) <= 0 ) return Tx_Sts_Empty_Profile;
    }
    tlm_send ( "OC\r\n", 4, 0 );

    for ( i=0; i<frames[3]; i++ )
    {
      MCOMS_Frame_t mcoms;
      snprintf ( (char*)mcoms.frame, 127, "MCOMSC-123\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\r\n", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 );
      mcoms.aux.acquisition_time.tv_sec = 333;
      mcoms.aux.acquisition_time.tv_usec = 4444;
      mcomsframe_fake( &mcoms );
      if ( write_mcoms_data ( profileID, &mcoms ) <= 0 ) return Tx_Sts_Empty_Profile;
    }
    tlm_send ( "MC\r\n", 4, 0 );

    if ( profile_stop( profileID, frames ) < 0 )
      return Tx_Sts_Empty_Profile;
    # endif
  }

  Profile_Packet_Definition_t ppd;
  if ( ( rv = profile_package ( &profileID, &ppd, &tx_instruct ) ) < 0 )
  {
    return Tx_Sts_Package_Fail;
  }

  uint16_t const numPackets = 1
                            + ppd.numPackets_SBRD
                            + ppd.numPackets_PORT
                            + ppd.numPackets_OCR
                            + ppd.numPackets_MCOMS;

  Packet_Status_t* packet_status = pvPortMalloc ( numPackets * sizeof(Packet_Status_t) );

  uint16_t* packet_bursts = pvPortMalloc ( numPackets * sizeof(uint16_t) );
  int const maxBursts = 2 + 1 + ( sizeof(Profile_Data_Packet_t)-1 ) / tx_instruct.burst_size;
  uint32_t* burst_status  = (uint32_t*) pvPortMalloc ( numPackets * sizeof(uint32_t) );

  Transfer_Status_t tx_sts = Tx_Sts_Package_Fail;

  if  ( !packet_status || !packet_bursts || !burst_status )
  {
    tx_sts = Tx_Sts_Package_Fail;
  }
  else
  {
    int p;
    for ( p=0; p<numPackets; p++ )
    {
      packet_status[p] = PCK_Unsent;
      packet_bursts[p] = 0;
      int b;
      burst_status[p] = 0;
      for ( b=0; b<maxBursts; b++ )
      {
        burst_status[p] |= (BST_Unsent<<(3*b));
      }
    }

    int PMG_PRF_TX_value;
    while ( PMG_PRF_TX_CONTINUE ==
         ( PMG_PRF_TX_value = pmg_profile_transmission ( profileID, numPackets,
                             packet_status, packet_bursts, burst_status ) ) )
    {
      vTaskDelay( (portTickType)TASK_DELAY_MS( 500 ) );
    }

    //  Interpret pmg_profile_transmission() return value
    //
    if ( PMG_PRF_TX_value == PMG_PRF_TX_ALL_DONE )
    {
      tx_sts = Tx_Sts_AllDone;
    }
    else
    {
      tx_sts = Tx_Sts_Modem_Fail;
    }
  }

  vPortFree( packet_status );
  packet_status = 0;
  vPortFree( packet_bursts );
  packet_bursts = 0;
  vPortFree( burst_status );
  burst_status = 0;

  return tx_sts;
}



static int16_t rudics_connect ()
{
  //
  //  Assertain basic modem communication and status
  //

  if  (!mdm_wait_for_dsr ())
  {
    return -1;
  }

  //
  //  AT command: Modem responds with OK
  //
  if  ( !mdm_getAtOk() )
  {
    return -1;
  }

  //
  //  Force required configuration,
  //  do not rely on Navis to provide necessary configuration.
  //
  if  (!mdm_configure ())
  {
    return -1;
  }

  //
  //  At this point in time, modem should be registered to the network.
  //  The modem has been powered for a number of seconds:
  //   *  Navis to power modem
  //   *  Message Navis to HyperNav
  //   *  HyperNav internal setup to make ready for transfer
  //
  if  (!mdm_isRegistered ())
  {
    return -1;
  }

  //
  //  Only proceed if the signal strength is strong.
  //  Signal strength is related to transfer rate via
  //  50 = 5 bars, 100% data rate
  //  40 = 4 bars,  50% data rate
  //  30 = 3 bars,  10% data rate
  //
  {
    int attempt = 0;
    int signalStrength = 0;
    int const neededSignalStrength = 35;

    do
    {
      int min, avg, max;
      if  (mdm_getSignalStrength ( 3+attempt, &min, &avg, &max ) > 0 )
      {
        signalStrength = avg;
      }

    } while ( attempt++<=5 && signalStrength < neededSignalStrength );

    if ( signalStrength < neededSignalStrength )
    {
      return -1;
    }
  }

  //
  //  Dial into the network
  //  Under ideal condition, this takes 20 seconds.
  //  Giving a 90 second timeout is generous.
  //
  //  This is the point where the modem responds "CONNECT 19200".
  //
  char dial[32];
  DialString( dial, 31 );
  io_out_string ( dial );
  io_out_string ( "\r\n" );

  //if ( mdm_connect("ATD00881600005183\r", 90) <= 0 ) {
  if  (mdm_connect (dial, 90) <= 0)
  {
    return -1;
  }

  //
  //  Wait for the login prompt,
  //  then log into the account,
  //  and wait for for the shell prompt.
  //
  char login[8];
  login[0] = 'f';
  S32_to_str_dec ( (S32)CFG_Get_Serial_Number(), login+1, sizeof(6), 4 );
  login[5] = '\r';
  login[6] = 0;

  io_out_string ( login );
  io_out_string ( "\r\n" );

  //if ( mdm_login(120, "f0002\r", "S(n)=2n\r" ) <= 0 ) {
  if ( mdm_login(120, login, "S(n)=2n\r" ) <= 0 )
  {
    return -1;
  }

  return 0;
}
# if 0
static int16_t rudics_is_connected() {
  return mdm_carrier_detect();
}
# endif


static int16_t rudics_disconnect ()
{
  //
  //  The modem should always respond to the escape command.
  //  Try twice, but do not abort, even after failures.
  //
  if  (mdm_escape() <= 0)
  {
    mdm_escape();
  }

  //
  //  Send "ATH" (hangup) command to the modem.
  //  (Faster than DTR deassertion).
  //
  if ( mdm_hangup() <= 0 )
  {
    //  Report to engineering log
  }

  mdm_log (1);

  //
  //  Hardware hangup using DTR deassertion.
  //  Expect modem to deassert DSR after 10 seconds.
  //
  mdm_dtr(0);

  int timeout = 15;

  do 
  {
    vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );
  } while ( mdm_get_dsr() && --timeout>0 );

  return mdm_get_dsr() ? 0 : -1;
}



//
//  Return values:
//    OK    ==  0
//    FAIL  <   0
//
static int16_t transfer_start (uint16_t*                       tx_profile_id,
                               Profile_Packet_Definition_t*    ppd,
                               transmit_instructions_t const*  tx_instruct
                              )
{

  //
  //  Actions:
  //    For current profile, generate packets and store in files.
  //    Modem setup, dial in, rudics login, session start.
  //

  //  TODO:
  //  Make sure to not trigger the watchdog reset while performing
  //  long execution time tasks (e.g., file I/O, packet generation, dial in).
  //

  if  (profile_package ( tx_profile_id, ppd, tx_instruct ) < 0 )
  {
    return -1;
  }

  if  (rudics_connect () < 0)
  {
    return -1;
  }

  return 0;
}



static int16_t transfer_stop ()
{
  return rudics_disconnect(); 
}



# if 0
static int16_t data_packet_process_transmit ( Profile_Data_Packet_t* original, Profile_Data_Packet_t* to_transmit, transmit_instructions_t const* tx_instruct, uint16_t profileID, uint16_t pNum ) {
  char packet_file_name[34];

  // Process packet
  //   Binary -> Graycode -> Bitplaned -> Compressed (-> ASCII-Encoded)
  //
  //   ASCII Encoding is not needed!

  //  TODO: Round to proper position before graycoding if noise_remove>0

  //  Graycoding is done in place
  //
  if ( tx_instruct->representation == REP_GRAY ) {
    if ( data_packet_bin2gray( original, original ) ) {
      //fprintf ( stderr, "Failed in data_packet_bin2gray()\n" );
      return -1;
    }
    snprintf ( packet_file_name, 34, EMMC_DRIVE PMG_PROFILE_FOLDER "\\%05hu\\%05hu.G%02d", profileID, profileID, pNum );
    data_packet_save_native ( original, packet_file_name );
  }

  //  Arrange the spectral data in bitplanes
  //
  if ( tx_instruct->use_bitplanes == BITPLANE ) {
    sram_memset( to_transmit, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bitplane( original, to_transmit /*, tx_instruct->noise_bits_remove */ /* If planning on bit removal, must modify graycoding */ ) ) {
      //fprintf ( stderr, "Failed in data_packet_bitplane()\n" );
      return -1;
    }
    snprintf ( packet_file_name, 34, EMMC_DRIVE PMG_PROFILE_FOLDER "\\%05hu\\%05hu.B%02d", profileID, profileID, pNum );
    data_packet_save_native ( to_transmit, packet_file_name );
  } else {
    sram_memcpy( to_transmit, original, sizeof(Profile_Data_Packet_t) );
  }

  //  Compress the packet
  //
  if ( tx_instruct->compression == ZLIB ) {
    sram_memset( original, 0, sizeof(Profile_Data_Packet_t) );
    char compression = 'G';
    if ( data_packet_compress( to_transmit, original, compression ) ) {
      //fprintf ( stderr, "Failed in data_packet_compress()\n" );
      return -1;
    }
    snprintf ( packet_file_name, 34, EMMC_DRIVE PMG_PROFILE_FOLDER "\\%05hu\\%05hu.C%02d", profileID, profileID, pNum );
    data_packet_save_native ( original, packet_file_name );
  } else {
    sram_memcpy( to_transmit, original, sizeof(Profile_Data_Packet_t) );
  }

# if 0
  //  Encode the packet
  //
  if ( tx_instruct->encoding != NO_ENCODING ) {
    memset( to_transmit, 0, sizeof(Profile_Data_Packet_t) );
    char encoding;
    switch ( tx_instruct->encoding ) {
    case ASCII85: encoding = 'A'; break;
    case BASE64 : encoding = 'B'; break;
    default     : encoding = 'N'; break;
    }
    if ( data_packet_encode( original, to_transmit, encoding ) ) {
      //fprintf ( stderr, "Failed in data_packet_encode()\n" );
      return -1;
    }
    snprintf ( packet_file_name, 34, EMMC_DRIVE PMG_PROFILE_FOLDER "\\%05hu\\%05hu.E%02d", profileID, profileID, pNum );
    data_packet_save_native ( to_transmit, packet_file_name );
  } else {
    memcpy( original, to_transmit, sizeof(Profile_Data_Packet_t) );
  }
# endif

  //  FIXME --  Serialize data for save transfer -- Network Byte Order???
  //  Transmit the packet
  //

  return 0; //data_packet_txmit ( &encoded, tx_instruct->burst_size, bstStatus );
}
# endif

//  Profile Manager Task
//
static void profile_manager_loop( __attribute__((unused)) void* pvParameters )
{
# if 0
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  //while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif

  //
  //  The profile manager has two mutually exclusive tasks:
  //
  //  (1) Profiling
  //      The profile manager stores all data acquired during profiling.
  //
  //  (2) Tramsmitting
  //      The profile manager retrieves the profile data,
  //      assembles them into packets, and
  //      sends those to the profile processor.
  //

  //  The below state transition table indicates
  //  expected commands/input ++
  //  wrong    commands/input --  (unexpected, refuse, NAK, repeat or ignore)
  //
  //  When in PMG_Idle (initial state)
  //    Incoming data         --  Re-stop DAQ
  //    CMD_PMG_Profile       ++  Start Profile    ACK  -->  PMG_Profiling
  //    CMD_PMG_Profile_End   --  Re-stop DAQ      ACK
  //    CMD_PMG_Transmit(ID)  ++  Start Transfer   ACK  -->  PMG_Transmitting
  //    CMD_PMG_SendPacket(n) --  Re-stop PPR
  //    CMD_PMG_Transmit_End  --  Re-stop PPR      ACK
  //    any other command     --  Ignore
  //
  //  When in PMG_Profiling
  //    Incoming data         ++  Expected, save to file
  //    CMD_PMG_Profile       --  Re-start DAQ
  //    CMD_PMG_Profile_End   ++  End Profile      ACK  -->  PMG_Idle
  //    CMD_PMG_Transmit      ++  End Profile,
  //                              Start Transfer   ACK  -->  PMG_Transmitting
  //    CMD_PMG_SendPacket(n) --  Ignore
  //    CMD_PMG_Transmit_End  --  Ignore           NAK
  //    any other command     --  Ignore
  //
  //  When in PMG_Transmitting
  //    Incoming data         --  Re-stop DAQ
  //    CMD_PMG_Profile       --  Ignore           NAK
  //    CMD_PMG_Profile_End   --  Ignore           NAK
  //    CMD_PMG_Transmit      --  Re-start PPR
  //    CMD_PMG_SendPacket(n) ++  Send next packet
  //    CMD_PMG_Transmit_End  ++  End Transfer     ACK  -->  PMG_Idle
  //
  //  End profile/transmit commands always force into IDLE
  //

  static enum { PMG_Idle, PMG_Profiling, PMG_Offloading, PMG_Transmitting } pmg_state = PMG_Idle;
  static Bool Offloading_is_Ongoing = FALSE;
  static Bool Offloading_is_Done = FALSE;
  static Bool Received_TXBEG_command = FALSE;

  //  Data used during profiling
  //
  static uint16_t profile_id = 0;
  static uint16_t profile_frames[PMG_N_DATAFILES];

  //  Data used during transmitting
  //
  static uint16_t tx_profile_id = 0;
  static Profile_Packet_Definition_t ppd;
  static uint16_t numPackets = 0;
  //static Packet_Status_t* packet_status = 0;
  //static uint16_t*        packet_bursts = 0;
  //static uint16_t packet_in_transfer = NO_PACKET_IN_TRANSFER;  //  And also currently in memory
  //  Profile_Info_Packet_t  transferring_pip;
  //  Profile_Data_Packet_t* const transferring_pdp = sram_PMG_2;
  //static uint32_t* burst_status = 0;

  data_exchange_address_t const myAddress = DE_Addr_ProfileManager;

  for  (;;)
  {
    if  (gRunTask)
    {
      gTaskIsRunning = TRUE;

# if 0
      //  Report stack water mark, if changed from previous water mark
      //
      stackWaterMark = uxTaskGetStackHighWaterMark(0);

      if  (stackWaterMark < stackWaterMark_threshold)
      {
        while  ( stackWaterMark < stackWaterMark_threshold )
          stackWaterMark_threshold /= 2;
# if 0
        data_exchange_packet_t packet_message;
        packet_message.to   = DE_Addr_ControllerBoard_Commander;
        packet_message.from = myAddress;
        packet_message.type = DE_Type_Syslog_Message;
        packet_message.data.Syslog.number = SL_MSG_StackWaterMark;
        packet_message.data.Syslog.value  = stackWaterMark;
        data_exchange_packet_router ( myAddress, &packet_message );
# else
        char swm[8];
        S32_to_str_dec ( (S32)stackWaterMark, swm, sizeof(swm), 0 );
        io_out_string ( "PMG SWM " ); io_out_string ( swm ); io_out_string ( "\r\n" );
# endif
      }
# endif

      //  Check for incoming packets
      //  ERROR if packet.to not myself (myAddress)
      //  Accept   type == Ping, Command, DE_Type_HyperNav_Spectrometer_Data, DE_Type_HyperNav_Auxiliary_Data
      //  ERROR if other type

      data_exchange_packet_t packet_rx_via_queue;

      if  (pdPASS == xQueueReceive ( rxPackets, &packet_rx_via_queue, 0))
      {
        if  ( packet_rx_via_queue.to != myAddress )
        {

          //  This is not supposed to happen
          //
          data_exchange_packet_t packet_error;

          packet_error.from               = myAddress;
          packet_error.to                 = DE_Addr_ControllerBoard_Commander;
          packet_error.type               = DE_Type_Syslog_Message;
          packet_error.data.Syslog.number = SL_ERR_MisaddressedPacket;
          packet_error.data.Syslog.value  = packet_rx_via_queue.from;

          data_exchange_packet_router( myAddress, &packet_error );

          //  Mark data as free to prevent lockup.
          //
          switch ( packet_rx_via_queue.type ) {

          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }
          break;

          case DE_Type_Ping:
          case DE_Type_Command:
          case DE_Type_Response:
          case DE_Type_Syslog_Message:
            //  No action required. Packet ignored.
          break;

          case DE_Type_Nothing:
          break;

          //  Do NOT add a "default:" here.
          //  That way, the compiler will generate a warning
          //  if one possible packet type was missed.
          }

        } else {

          //  If there is a need to respond,
          //  the following address will be overwritten.
          //  A packet_response,to != DE_Addr_Nobody value
          //  will be used as a flag to send packet_response.
          data_exchange_packet_t packet_response;
          packet_response.to = DE_Addr_Nobody;

          switch ( packet_rx_via_queue.type ) {

          case DE_Type_Nothing:
            break;

          case DE_Type_Ping:

            packet_response.to = packet_rx_via_queue.from;
            packet_response.from = myAddress;
            packet_response.type = DE_Type_Response;
            packet_response.data.Response.Code = RSP_ALL_Ping;
            packet_response.data.Response.value.u64 = 0;  //  Will be ignored
            break;

          case DE_Type_Command:

            switch ( packet_rx_via_queue.data.Command.Code ) {

            case CMD_ANY_Query:
              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_PMG_Query;
              switch ( pmg_state ) {
              case PMG_Idle: packet_response.data.Response.value.rsp = RSP_PMG_Query_Is_Idle; break;
              case PMG_Profiling: packet_response.data.Response.value.rsp = RSP_PMG_Query_Is_Rx; break;
              case PMG_Transmitting: packet_response.data.Response.value.rsp = RSP_PMG_Query_Is_Tx; break;
              }
              break;

            case CMD_PMG_Profile:

              //  PMG_Idle            -> Accept & act
              //  PMG_Profiling       -> Resend CMD_DAQ_Profiling
              //  PMG_Transmitting    -> Ignore (quietly)

              //  During development only: Report back to commander
              if (1) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_PMG_Started;
                  packet_message.data.Syslog.value  = 0;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if ( PMG_Idle == pmg_state ) {

                {
                  struct timeval now; gettimeofday ( &now, NULL );
                  struct tm date_time; gmtime_r ( &(now.tv_sec), &date_time );
                  char yyddd[16];

                  strftime ( yyddd, sizeof(yyddd)-1, "%y%j", &date_time );
                  sscanf ( yyddd, "%hu", &profile_id );

                  int16_t rv = profile_start( profile_id );

                  if ( !rv ) {
                    pmg_state = PMG_Profiling;
                    int i; for ( i=0; i<PMG_N_DATAFILES; i++ ) profile_frames[i] = 0;
                  } else {
                    //  Profile was not started
                    //
                    profile_id = 0;

                    //  Report back to commander that CMD_PMG_Profile failed
                    //
                    data_exchange_packet_t packet_message;
                    packet_message.to   = DE_Addr_ControllerBoard_Commander;
                    packet_message.from = myAddress;
                    packet_message.type = DE_Type_Syslog_Message;
                    packet_message.data.Syslog.number = SL_ERR_PMG_StartFail;
                    packet_message.data.Syslog.value  = rv;
                    data_exchange_packet_router ( myAddress, &packet_message );
                  }
                }

              }

              //  Alert: Do not else-if the next block!
              //  Must execute following block when pmg_state was changed from PMG_Idle to PMG_Profiling.

              if ( PMG_Profiling == pmg_state ) {

                  //  If start up has already succeeded (via earlier sent command),
                  //  resend this command to data acquisition task.
                  //  That way, re-sending the start command
                  //  is a safe approach to re-attempting a start.
                  packet_response.to = DE_Addr_DataAcquisition;
                  packet_response.from = myAddress;
                  packet_response.type = DE_Type_Command;
                  packet_response.data.Command.Code = CMD_DAQ_Profiling;
//  FIXME         packet_response.data.Command.value.s64 = 0;
                  packet_response.data.Command.value.s64 = packet_rx_via_queue.data.Command.value.s64;
              }
      
              if ( PMG_Transmitting == pmg_state ) {
                //  FIXME -- Currently silently ignoring
              }

              break;

            case CMD_PMG_Profile_End:

              //  During development only: Report back to commander
              if (1) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_PMG_Stopped;
                  packet_message.data.Syslog.value  = 0;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if ( PMG_Profiling == pmg_state ) {
                pmg_state = PMG_Idle;
//              profile_stop( profile_id, profile_frames );
//              tx_profile_id = profile_id;
//              profile_id = 0;
              }  //  Alert: Do not else-if the next block! Must execute following block when pmg_state was changed.
              
              if ( PMG_Idle == pmg_state ) {

                //  If stopping has already succeeded (via earlier sent command),
                //  resend this command to data acquisition task.
                //  That way, re-sending the start command
                //  is a safe approach to re-attempting a stop.

                //  Forward to DAQ
                packet_response.to = DE_Addr_DataAcquisition;
                packet_response.from = myAddress;
                packet_response.type = DE_Type_Command;
                packet_response.data.Command.Code = CMD_DAQ_Profiling_End;
                packet_response.data.Command.value.s64 = 0;
              }

              if ( PMG_Offloading == pmg_state ) {
                //  If the state is already offloading,
                //  the spectrometer board is already done with profiling.
                //  No need to pass on the PRFEND.
              }

              if ( PMG_Transmitting == pmg_state ) {
                //  FIXME -- Currently silently ignoring
              }

              break;

            case CMD_PMG_Offloading:

              //  This condition occurs when the HyperNav determines
              //  it is time to stop profiling before the NAVIS sends
              //  the PRFEND command.
              //
              if ( PMG_Profiling == pmg_state ) {
                pmg_state = PMG_Offloading;
              }

              //  Alert: Do not else-if the next block! Must execute following block when pmg_state was changed.
              
              //  The following condition occurs
              //  if profiling was terminated via the PRFEND command.
              //
              if ( PMG_Idle == pmg_state && profile_id ) {
                pmg_state = PMG_Offloading;
              }

              if ( PMG_Offloading == pmg_state ) {
                //  Ignore
              }

              if ( PMG_Transmitting == pmg_state ) {
                //  FIXME -- Currently silently ignoring
              }

              Offloading_is_Ongoing = TRUE;
              Offloading_is_Done = FALSE;

              break;

            case CMD_PMG_Offloading_Done:

	      {
              char const* OffDoneMsg = "$MSG,TXRLT,DONE,0\r\n";
              tlm_send ( OffDoneMsg, strlen(OffDoneMsg), 0);
	      }

              if ( PMG_Offloading == pmg_state ) {
                profile_stop( profile_id, profile_frames );
              }

              Offloading_is_Ongoing = FALSE;
              Offloading_is_Done = TRUE;

              break;

//  TODO - If command == TXBEG or TXBEG,DATE start profile transfer

            case CMD_PMG_Transmit:

              Received_TXBEG_command = TRUE;

              if ( PMG_Profiling == pmg_state ) {

                //  Stop DAQ
                //
                packet_response.to = DE_Addr_DataAcquisition;
                packet_response.from = myAddress;
                packet_response.type = DE_Type_Command;
                packet_response.data.Command.Code = CMD_DAQ_Profiling_End;
                packet_response.data.Command.value.s64 = 0;

//              profile_stop( profile_id, profile_frames );
//              tx_profile_id = profile_id;
//              profile_id = 0;
              
                pmg_state = PMG_Idle;
              }

              //  Allow caller to request transfer of a past profile
              //
              if ( packet_rx_via_queue.data.Command.value.s16[0] ) {
                    tx_profile_id = packet_rx_via_queue.data.Command.value.s16[0];
//                  char swm[8];
//                  S32_to_str_dec ( (S32)tx_profile_id, swm, sizeof(swm), 0 );
//                  io_out_string ( "PMG TX# " ); io_out_string ( swm ); io_out_string ( "\r\n" );
              }
# if 0
# if 1
              gHNV_ProfileManagerTask_Status = TASK_IN_SETUP;
              profile_fake( tx_profile_id );
              gHNV_ProfileManagerTask_Status = TASK_RUNNING;
# else
              if ( PMG_Idle == pmg_state ) {

                int maxBursts = 2 + 1 + ( sizeof(Profile_Data_Packet_t)-1 ) / tx_instruct.burst_size;


                gHNV_ProfileManagerTask_Status = TASK_IN_SETUP;
                int16_t tx_rv = transfer_start ( &tx_profile_id, &ppd, &tx_instruct );
                gHNV_ProfileManagerTask_Status = TASK_RUNNING;

                if ( 0==tx_rv ) {

                  numPackets = 1
                             + ppd.numPackets_SBRD
                             + ppd.numPackets_PORT
                             + ppd.numPackets_OCR
                             + ppd.numPackets_MCOMS;
                  packet_status = pvPortMalloc ( numPackets * sizeof(Packet_Status_t) );
                  packet_bursts = pvPortMalloc ( numPackets * sizeof(uint16_t) );
                  burst_status  = (uint32_t*) pvPortMalloc ( numPackets * sizeof(uint32_t) );
                }

                if ( tx_rv || !packet_status || !packet_bursts || !burst_status ) {

                  //  free packet_status & burst_status (if non-null)
                  if ( packet_status ) { vPortFree(packet_status); packet_status=0; }
                  if ( packet_bursts ) { vPortFree(packet_bursts); packet_bursts=0; }
                  if (  burst_status ) { vPortFree( burst_status);  burst_status=0; }
                  
                  //  TODO = Report failure to commander

                } else {
                  int p;
                  for ( p=0; p<numPackets; p++ ) {
                    packet_status[p] = PCK_Unsent;
                    packet_bursts[p] = 0;
                    int b;
		    burst_status[p] = 0;
                    for ( b=0; b<maxBursts; b++ ) {
                      burst_status[p] |= (BST_Unsent<<(3*b));
                    }
                  }
                  packet_in_transfer = NO_PACKET_IN_TRANSFER;
                  pmg_state = PMG_Transmitting;
                }
              }
# endif
              {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Command;
                  packet_message.data.Command.Code = CMD_CMC_Transfer_Status;  //  Kludge: Normally, do not send _PMG_ to CMC
                  packet_message.data.Command.value.s16[0] = Tx_Sts_AllDone;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }
# endif

              break;

            case CMD_PMG_Transmit_End:

              //  During development only: Report back to commander
              if (1) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_PMG_Stopped;
                  packet_message.data.Syslog.value  = 0;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if ( PMG_Transmitting == pmg_state ) {

                //  Terminate transmit
                //
//              transfer_stop();

//              pmg_state = PMG_Idle;
              }

              if ( PMG_Profiling == pmg_state ) {
                //  Ignore
              }

              break;

              default:
                {
                data_exchange_packet_t packet_message;
                packet_message.to   = DE_Addr_ControllerBoard_Commander;
                packet_message.from = myAddress;
                packet_message.type = DE_Type_Syslog_Message;
                packet_message.data.Syslog.number = SL_ERR_UnexpectedCommand;
                packet_message.data.Syslog.value  = packet_rx_via_queue.data.Command.Code;
                data_exchange_packet_router ( myAddress, &packet_message );
                }
            }

            break;

          case DE_Type_Response:
            //  TODO - Handle expected ACK/NAK from DAQ or PP
            break;

          case DE_Type_Spectrometer_Data:

            //  This packet contains a spectrometer data address.
            //  Make a local copy of the data.

            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  if ( pmg_state == PMG_Profiling || pmg_state == PMG_Offloading ) {
                    //  RAM to RAM copy
                    memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Spectrometer_Data_t) );
                    received_data_type = DE_Type_Spectrometer_Data;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  if ( pmg_state == PMG_Profiling || pmg_state == PMG_Offloading ) {
                    //  SRAM to RAM copy
                    sram_read ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Spectrometer_Data_t) );
                    received_data_type = DE_Type_Spectrometer_Data;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "profile_manager", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_OCR_Frame:

            //  This packet contains a OCR data address.
            //  Make a local copy of the data.

            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  if ( pmg_state == PMG_Profiling ) {
                    //  RAM to RAM copy
                    memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(OCR_Frame_t) );
                    received_data_type = DE_Type_OCR_Frame;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  if ( pmg_state == PMG_Profiling ) {
                    //  SRAM to RAM copy (TODO: when local memory is SRAM, make this SRAM to SRAM copy
                    received_data_type = DE_Type_OCR_Frame;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "profile_manager", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_MCOMS_Frame:

            //  This packet contains a MCOMS data address.
            //  Make a local copy of the data.

            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  if ( pmg_state == PMG_Profiling ) {
                    //  RAM to RAM copy
                    memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(MCOMS_Frame_t) );
                    received_data_type = DE_Type_MCOMS_Frame;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  if ( pmg_state == PMG_Profiling ) {
                    //  SRAM to RAM copy
                    sram_read ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(MCOMS_Frame_t) );
                    received_data_type = DE_Type_MCOMS_Frame;
                    received_data_package.state = FullRAM;
                  }
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "profile_manager", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:

            syslog_out ( SYSLOG_ERROR, "profile_manager", "Discarding unexpected incoming data %d", packet_rx_via_queue.type );

            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }

          break;

          default:
            /*
            io_out_string ( "ERROR SLG ignoring unimplemented packet type\r\n" );
            */
            break;

          }

          //  Send back the response packet
          if ( packet_response.to != DE_Addr_Nobody ) {
            data_exchange_packet_router ( myAddress, &packet_response );
          }

          if ( FullRAM == received_data_package.state ) {

            if ( PMG_Profiling == pmg_state || PMG_Offloading == pmg_state ) {
            
              switch ( received_data_type ) {
              
              case DE_Type_Spectrometer_Data:
              
                if  ( 1 == write_spec_data ( profile_id, received_data_package.address ) )
                {
                  Spectrometer_Data_t* sdt = received_data_package.address;
                  if ( 0 == sdt->aux.side )
                  {
                    profile_frames[PMG_DT_Port_Radiometer]++;
                  }

                  else if ( 1 == sdt->aux.side )
                  {
                    profile_frames[PMG_DT_Starboard_Radiometer]++;
                  }
                }
                break;
              
              case DE_Type_OCR_Frame:
              
                if ( 1 == write_ocr_data ( profile_id, received_data_package.address ) ) {
                  profile_frames[PMG_DT_OCR]++;
                }
                break;
              
              case DE_Type_MCOMS_Frame:
              
                if ( 1 == write_mcoms_data ( profile_id, received_data_package.address ) ) {
                  profile_frames[PMG_DT_MCOMS]++;
                }
                break;
              
              case DE_Type_Ping:
              case DE_Type_Command:
              case DE_Type_Response:
              case DE_Type_Syslog_Message:
              case DE_Type_Configuration_Data:
              case DE_Type_Profile_Info_Packet:
              case DE_Type_Profile_Data_Packet:
              case DE_Type_Nothing:
              //  Ignore, cannot happen
                break;
              
              }
            
             received_data_type = DE_Type_Nothing;
             received_data_package.state = EmptyRAM;
            
            } else {
              //  Unexpectedly received data
              //  TODO   re-stop DAQ!
            }
          }
        }


       }

       if ( !Received_TXBEG_command ) {

         if ( Offloading_is_Ongoing ) {
           static int nDelay = 100;  //  Expect this to cause 30 second delay

           if ( nDelay > 0 ) {
             nDelay--;
           } else {
             char const* OffActiveMsg = "$MSG,TXRLT,OFFLOADING\r\n";
             tlm_send ( OffActiveMsg, strlen(OffActiveMsg), 0);
             nDelay = 100;
           }
         }

         if ( Offloading_is_Done ) {
         
           static int nResend = 5;

           if ( nResend > 0 ) {

             static int nDelay = 100;  //  Expect this to cause 30 second delay

             if ( nDelay > 0 ) {
               nDelay--;
             } else {
               char const* OffDoneMsg = "$MSG,TXRLT,DONE,0\r\n";
               tlm_send ( OffDoneMsg, strlen(OffDoneMsg), 0);
               nResend--;
               nDelay = 100;
             }
           }
         }

       }

       if ( Offloading_is_Done && Received_TXBEG_command ) {

              gHNV_ProfileManagerTask_Status = TASK_IN_SETUP;
              Transfer_Status_t tx_status = profile_fake( tx_profile_id ? tx_profile_id : profile_id );
              gHNV_ProfileManagerTask_Status = TASK_RUNNING;

              data_exchange_packet_t packet_message;
              packet_message.to   = DE_Addr_ControllerBoard_Commander;
              packet_message.from = myAddress;
              packet_message.type = DE_Type_Command;
              packet_message.data.Command.Code = CMD_CMC_Transfer_Status;
              packet_message.data.Command.value.s16[0] = tx_status;
              data_exchange_packet_router ( myAddress, &packet_message );

              //  Cleanup
              //
              profile_id = 0;
              tx_profile_id = 0;
              pmg_state = PMG_Idle;

              Offloading_is_Done = FALSE;
              Received_TXBEG_command = FALSE;
       }

    } else {
      gTaskIsRunning = FALSE;
    }

    // Sleep
    gHNV_ProfileManagerTask_Status = TASK_SLEEPING;
    vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_ProfileManagerTask_PERIOD_MS ) );
    gHNV_ProfileManagerTask_Status = TASK_RUNNING;
  }

}

//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

//  Send a packet to the profile manager task.
//  The profile maneger tasks expects only two types of packets:
//  (a) command packets
//  (b) data packets from data_acquisition and auxiliary_acquisition
//
void profile_manager_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
    // io_out_string ( "DBG SLG push packet\r\n" );
  }
}

// Resume running the task
Bool profile_manager_resumeTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = TRUE;

  // Wait for task to start running
  while(!gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}


// Pause the task
Bool profile_manager_pauseTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = FALSE;

  // Wait for task to stop running
  while(gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}


// Allocate the task and associated control mutex
Bool profile_manager_createTask(void)
{
  static Bool taskCreated = FALSE;

  if(!taskCreated) {

    // Create task
    if ( pdPASS != xTaskCreate( profile_manager_loop,
                  HNV_ProfileManagerTask_NAME, HNV_ProfileManagerTask_STACK_SIZE, NULL,
                  HNV_ProfileManagerTask_PRIORITY, &gHNV_ProfileManagerTask_Handler)) {
      return FALSE;
    }

    // Create control mutex
    if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) ) {
      return FALSE;
    }

    // Create message queue
    if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) ) {
      return FALSE;
    }

    //  Allocate memory for data received via packets
    received_data_package.mutex = NULL;  //  No need for mutex on this package, all handled in-task!
    received_data_package.state = EmptyRAM;
    received_data_package.address = (void*) pvPortMalloc ( sizeof(any_received_data_t) );
    if ( NULL == (received_data_package.address) ) {
      return FALSE;
    }

# if 0
    //  Use SRAM for data to be sent via packets
    sending_data_package.mutex = xSemaphoreCreateMutex();
    if ( NULL == sending_data_package.mutex ) {
      return FALSE;
    }

    sending_data_package.state = EmptySRAM;
    sending_data_package.address = sram_PMG;
# endif

    //  All local task variables are set up

    taskCreated = TRUE;
    return TRUE;

  } else {

    return TRUE;
  }
}

# endif
