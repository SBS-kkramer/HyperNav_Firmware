# include "command.controller.h"
# include "command.errorcodes.h"
# include "errorcodes.h"

# include <ctype.h>
# include <stdio.h>
# include <string.h>
# include <time.h>
# include <sys/time.h>

# include "watchdog.h"
# include "pm.h"

# include "PCBSupervisor.h"
# include "PCBSupervisor_cfg.h"
# include "avr32rerrno.h"
# include "hypernav.sys.controller.h"
# include "sysmon.h"

# include "files.h"

# include "crc.h"
# include "extern.controller.h"
# include "filesystem.h"
# include "info.h"
# include "io_funcs.controller.h"
# include "syslog.h"
# include "modem.h"
# include "telemetry.h"
# include "version.controller.h"
# include "wavelength.h"

# include "config.controller.h"
# include "tasks.controller.h"
# include "data_exchange_packet.h"
# include "config_data.h"
# include "stream_log.h"
# include "data_exchange.controller.h"
# include "aux_data_acquisition.h"
# include "profile_manager.h"

# include "setup.controller.h"

// SPI testing
# include "spi.h"
# include "gpio.h"
// Serial sensor testing
#include "generic_serial.h"
#include "SatlanticHardware.h"

//  Tested modules:
# include <board.h>

# include "ds3234.h"
# include "spi.controller.h"

# include "smc_sram.h"
# include "smc.h"
#include "ocr_data.h"
#include "usart.h"
#include "mcoms_data.h"

#include "pdmabuffer.h"

#define OPERATION_AUTONOMOUS

# define GPLP_STATE_STOPPED  0x00000001
# define GPLP_STATE_LMD      0x00000002
# define GPLP_STATE_DRK      0x00000004
# define GPLP_STATE_LD       0x00000008
# define GPLP_STATE_SIGNAL   0xDEADBEEF

// packet receiving queue
# define N_RX_PACKETS 6
static xQueueHandle rxPackets = NULL;

//  Use RAM for data sending via packet transfer
typedef union {
  Config_Data_t          Config_Data;
} any_sending_data_t;
static data_exchange_data_package_t sending_data_package;

////////////////////////////////

static char local_tmp_str[128];

static Transfer_Status = Tx_Sts_Not_Started;

# if 0
typedef struct {
  enum {
    CC_EXPECT_Nothing,
    CC_EXPECT_ACK_DAQ_START,
    CC_EXPECT_ACK_DAQ_STOP,
    CC_EXPECT_ACK_PRF_BEG,
    CC_EXPECT_ACK_PRF_END,
    CC_EXPECT_ACK_TRX_BEG,
    CC_EXPECT_ACK_TRX_END
  }                       response;
  struct timeval          untilWhen;
  char*                   sendBack;
} cc_expect_t;
# endif

typedef struct {
    char* name;
    Access_Mode_t id;
} Access_Option_t;

static Access_Option_t accessOption[] =
{
  { "User", Access_User },
  { "Admin", Access_Admin },
  { "Factory", Access_Factory }
};

static const S16 MAX_ACCESS_OPTIONS = sizeof(accessOption)/sizeof(Access_Option_t);

static bool XM_use1k  = false;
static bool XM_useCRC = false;

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
# if 0
//  Compare timeval argument to current time.
//  Return -1 is argument is before  now
//  Return  0 is argument is exactly now
//  Return  1 is argument is pasr    now
//
static uint8_t timeval_compare_to_now ( struct timeval* tv ) {

  struct timeval now; gettimeofday ( &now, NULL );

  if ( tv->tv_sec < now.tv_sec ) {
    return -1;
  } else if ( tv->tv_sec > now.tv_sec ) {
    return  1;
  } else {
    if ( tv->tv_usec < now.tv_usec ) {
      return -1;
    } else if ( tv->tv_usec > now.tv_usec ) {
      return  1;
    } else {
      return  0;
    }
  }
}
# endif
static S16 Cmd_Ping ( char* option )
{
  data_exchange_packet_t packet;

  packet.from       = DE_Addr_ControllerBoard_Commander;
  packet.type       = DE_Type_Ping;
  memcpy ( packet.data.Ping_Message, "Ping", 4 );

  for ( packet.to = DE_Addr_Nobody+1; packet.to < DE_Addr_Void; packet.to++ )
  {
    if ( 0 == strcasecmp ( option, taskAcronym( packet.to ) ) )
    {
      data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
    }
  }

  return CEC_Ok;
}


static S16 Cmd_AllPing ()
{
  data_exchange_packet_t packet;

  packet.from       = DE_Addr_ControllerBoard_Commander;
  packet.type       = DE_Type_Ping;

  for ( packet.to = DE_Addr_Nobody+1; packet.to < DE_Addr_Void; packet.to++ ) {
    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
  }

  return CEC_Ok;
}


static S16 Cmd_Query ( char* option ) {

  S8 const queryAll = ( option[0] == '*' && option[1] == 0 );

  data_exchange_packet_t packet;

  packet.from              = DE_Addr_ControllerBoard_Commander;
  packet.type              = DE_Type_Command;
  packet.data.Command.Code = CMD_ANY_Query;

  for ( packet.to = DE_Addr_Nobody+1; packet.to < DE_Addr_Void; packet.to++ ) {
    if ( queryAll || 0 == strcasecmp( option, taskAcronym(packet.to) ) ) {
      data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
    }
  }

  return CEC_Ok;
}

static S16 Cmd_Access ( char* option, char* value, Access_Mode_t* access_mode ) {

# if 0
    Bool match = false;
    S16 try = 0;

    do {
        if ( 0 == strncasecmp ( option, accessOption[try].name, strlen(accessOption[try].name) ) ) {
            match = true;
        } else {
            try++;
        }
    } while ( !match && try < MAX_ACCESS_OPTIONS );

    if ( try == MAX_ACCESS_OPTIONS ) {
        return CEC_UnknownAccessOption;
    } else {
        switch ( accessOption[try].id ) {
        case Access_User:
            *access_mode = Access_User;
            return CEC_Ok;
            break;
        case Access_Admin:
            if ( 0 == strcmp( value, CFG_Get_Admin_Password() ) ) {
                *access_mode = Access_Admin;
                return CEC_Ok;
            } else {
                return CEC_WrongAccessPassword;
            }
            break;
        case Access_Factory:
            {
                char datestr[10];
                time_t now;
                time ( &now );
                strftime ( datestr, sizeof(datestr)-1, "%Y%j", gmtime ( &now ) );

                snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "%s%05hu%s", CFG_Get_Sensor_Type_AsString(), CFG_Get_Serial_Number(), datestr );
                U32 crc32 = fCrc16Bit((U8*)glob_tmp_str);
                //io_out_string(glob_tmp_str); io_out_string("\r\n");
                crc32 <<= 16;
                glob_tmp_str[0] = glob_tmp_str[8]; glob_tmp_str[8] = glob_tmp_str[14]; glob_tmp_str[14] = glob_tmp_str[0];
                //io_out_string(glob_tmp_str); io_out_string("\r\n");
                crc32 |= fCrc16Bit((U8*)(glob_tmp_str+1));
                //snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "%lx\t%lx\r\n", crc32, strtoul(value,0,16) );
                //io_out_string(glob_tmp_str); io_out_string("\r\n");

                if ( 0 == strcmp( value, CFG_Get_Factory_Password() )
                  || 0 == strcmp( value, "%Y:%m:%d"+1 ) // Obfuscating the back door, password is "Y:%m:%d"
                  || crc32 == strtoul(value,0,16) ) {
                    *access_mode = Access_Factory;
                    return CEC_Ok;
                } else {
                    // Slow down brute force attacks
                    vTaskDelay((portTickType)TASK_DELAY_MS( 1000 ));
                    return CEC_WrongAccessPassword;
                }
            }
            break;
        default:
            return CEC_UnknownAccessOption;
            break;
        }
    }
# else
  *access_mode = Access_Factory;
  return CEC_Ok;
# endif
}

static void Cmd_Test_RTC() {

  //  Set of commands for RTC testing
  //  Commands are
  //      <command_string>[,option[,value]*]
  typedef enum {
    //
    RTC_Init,
    RTC_SetTime,
    RTC_GetTime,
    RTC_SetValue,
    RTC_GetValue,
    RTC_SetAlarm,
    RTC_Help,
    RTC_Quit
  } RTCCommandID;

  typedef struct {
    char* name;
    RTCCommandID id;
  } RTCCommand_t;

  static RTCCommand_t RTCCmd[] = {
    { "Init",      RTC_Init },
    { "SetTime",   RTC_SetTime },
    { "GetTime",   RTC_GetTime },
    { "SetVal",    RTC_SetValue },
    { "GetVal",    RTC_GetValue },
    { "SetAlarm",  RTC_SetAlarm },
    { "Help",      RTC_Help },
    { "Quit",      RTC_Quit }
  };

  static const S16 MAX_RTC_CMDS = sizeof(RTCCmd)/sizeof(RTCCommand_t);

  io_out_string( "Test_RTC> " );

  while(1) {  //  Break out via 'quit' command

    //  Handle input via RS-232
    char cmdStr[64];

    U16 const     timeout = 3600;  // 1 hour = 3600 s
    U16 const idleTimeout = 3000;  // 3 seconds = 3000 ms
    S16 const inLength = io_in_getstring ( cmdStr, sizeof(cmdStr), timeout, idleTimeout );

    if ( inLength > 0 ) {
      char* pCmd = cmdStr;

      //  Skip leading white space
      while ( *pCmd == ' ' || *pCmd == '\t' ) pCmd++;

      Bool match = false;
      S16 try = 0;
      char* arg = 0;

      do {
        if ( 0 == strncasecmp ( pCmd, RTCCmd[try].name, strlen(RTCCmd[try].name) ) ) {
          match = true;
          arg = pCmd + strlen( RTCCmd[try].name );
        } else {
          try++;
        }
      } while ( !match && try < MAX_RTC_CMDS );

      if ( try < MAX_RTC_CMDS ) {

        while ( *arg == ' ' || *arg == '\t' ) arg++;

        char* const option = arg;

        while ( isalnum( *arg ) || *arg == '_' || *arg =='*' ) arg++;

        char* value;

        if ( 0 == *arg ) {
          value = "";
        } else {
          while ( *arg == ' ' || *arg == '\t' || *arg == '=' ) arg++;
          value = arg;
        }

        U32 time;
        U8  address;
        U8  data;

        switch ( RTCCmd[try].id ) {

        case RTC_Init:     io_dump_U32( (U32)FOSC0, " FOSC0\r\n" );
                           ds3234_init( (U32)FOSC0 );
                           io_out_string ( "Init done\r\n" );
                           break;
        case RTC_SetTime:  time = atol( option );
                           io_out_string ( "Setting RTC time to: " );
                           io_dump_U32 ( time, "\r\n" );
                           ds3234_setTime( time);
                           break;
        case RTC_GetTime:  ds3234_getTime( &time );
                           io_out_string ( "RTC time is: " );
                           io_dump_U32 ( time, "\r\n" );
                           break;
        case RTC_SetValue: address = atoi ( option );
                           data    = atoi ( value );
                           io_out_string ( "Setting RTC value " );
                           io_dump_X8( address, " to " );
                           io_dump_X8( data, "\r\n" );
                           ds3234_writeSRAMData( address, data );
                           break;
        case RTC_GetValue: address = atoi ( option );
                           ds3234_readSRAMData( address, &data );
                           io_out_string ( "RTC value " );
                           io_dump_X8( address, " is " );
                           io_dump_X8( data, "\r\n" );
                           break;
        case RTC_SetAlarm: io_out_string( "Set Alarm Test not implemented yet\r\n" );
                           break;
        case RTC_Help:     {
                           int i;
                           io_out_string( "Commands:" );
                           for ( i=0; i<MAX_RTC_CMDS; i++ ) {
                             io_out_string( " " );
                             io_out_string( RTCCmd[i].name );
                           }
                           io_out_string( "\r\n" );
                           }
                           break;
        case RTC_Quit:     return;
        }

      }

      io_out_string( "Test_RTC> " );
    } else if ( inLength == 0 ) {
      io_out_string( "Test_RTC> " );
    }
  }
}

static void Cmd_Test_FSYS() {

  //  Set of commands for FSYS testing
  //  Commands are
  //      <command_string>[,option[,value]*]
  typedef enum {
    //
    FS_Init,
    FS_Probe,
    FS_Space,
    FS_FDisk,
    FS_Format,
    FS_Mount,
    FS_MkFs,
    FS_Setup,
    FS_List,
    FS_MkDir,
    FS_Help,
    FS_Quit
  } FSYSCommandID;

  typedef struct {
    char* name;
    FSYSCommandID id;
  } FSYSCommand_t;

  static FSYSCommand_t FSYSCmd[] = {
    { "Init",   FS_Init },
    { "Probe",  FS_Probe },
    { "Space",  FS_Space },
    { "FDisk",  FS_FDisk },
    { "Format", FS_Format },
    { "Mount",  FS_Mount },
    { "MkFs",   FS_MkFs },
    { "Setup",  FS_Setup },
    { "List",   FS_List },
    { "MkDir",  FS_MkDir },
    { "Help",   FS_Help },
    { "Quit",   FS_Quit }
  };

  static const S16 MAX_FSYS_CMDS = sizeof(FSYSCmd)/sizeof(FSYSCommand_t);

  io_out_string( "Test_FSYS> " );

  while(1) {  //  Break out via 'quit' command

    //  Handle input via RS-232
    char cmdStr[64];

    U16 const     timeout = 3600;  // 1 hour = 3600 s
    U16 const idleTimeout = 3000;  // 3 seconds = 3000 ms
    S16 const inLength = io_in_getstring ( cmdStr, sizeof(cmdStr), timeout, idleTimeout );

    if ( inLength > 0 ) {
      char* pCmd = cmdStr;

      //  Skip leading white space
      while ( *pCmd == ' ' || *pCmd == '\t' ) pCmd++;

      Bool match = false;
      S16 try = 0;
      char* arg = 0;

      do {
        if ( 0 == strncasecmp ( pCmd, FSYSCmd[try].name, strlen(FSYSCmd[try].name) ) ) {
          match = true;
          arg = pCmd + strlen( FSYSCmd[try].name );
        } else {
          try++;
        }
      } while ( !match && try < MAX_FSYS_CMDS );

      if ( try < MAX_FSYS_CMDS ) {

        while ( *arg == ' ' || *arg == '\t' ) arg++;

        char* const option = arg;

        while ( isalnum( *arg ) || *arg == '_' || *arg =='*' ) arg++;

        char* value;

        if ( 0 == *arg ) {
          value = "";
        } else {
          while ( *arg == ' ' || *arg == '\t' || *arg == '=' ) arg++;
          value = arg;
        }

        switch ( FSYSCmd[try].id ) {

        case FS_Init:      gpio_clr_gpio_pin(EMMC_NRST);    // ensure reset line low
                           vTaskDelay((portTickType)TASK_DELAY_MS( 1 ));
                           gpio_set_gpio_pin(EMMC_NRST);    // trigger rest
                           // wait tRSCA for init to complete (200 us min)
                           vTaskDelay((portTickType)TASK_DELAY_MS( 1 ));
                           io_out_string ( "SET\r\n" );
                           break;
        case FS_Probe:     f_fsProbe()  //  This function = Mount + Space
                           ? io_out_string ( "f_fsProbe() ok\r\n" )
                           : io_out_string ( "f_fsProbe() failed\r\n" );
                           break;
        case S:     {
                           U64 avail = 0;
                           U64 total = 0;
                           ( FILE_OK == f_space ( EMMC_DRIVE_CHAR, &avail, &total ) )
                           ? io_out_string ( "f_space() ok\r\n" )
                           : io_out_string ( "f_space() failed\r\n" );
                           io_own_S32 ( "Avail ", (S32)(avail>>32)       , ""     );
                           io_own_S32 (      " ", (S32)(avail&0xFFFFFFFF), "\r\n" );
                           io_own_S32 ( "Total ", (S32)(total>>32)       , ""     );
                           io_own_S32 (      " ", (S32)(total&0xFFFFFFFF), "\r\n" );
                           }
                           break;
        case FS_FDisk:     { // Disk partitioning
                           DWORD const plist[] = {100, 0, 0, 0};
                           BYTE work[_MAX_SS];
                           ( FR_OK == FATFs_f_fdisk ( EMMC_DRIVE_CHAR-'0', plist, work ) )
                           ? io_out_string ( "FATFs_f_fdisk() ok\r\n" )
                           : io_out_string ( "FATFs_f_fdisk() failed\r\n" );
                           }
                           break;
        case FS_Format:    {
                           BYTE const drive_num = 0;

                           DWORD const plist[] = {100, 0, 0, 0};
                           {
                           BYTE work[_MAX_SS];
                           FATFs_f_fdisk(drive_num, plist, work);  // Here 'drive_num' is physical drive number
                           }
                           {
                           FATFS Fatfs;
                           FATFs_f_mount(drive_num, &Fatfs);       // Here 'drive_num' is logical drive number
                           }
                           if(FATFs_f_mkfs(drive_num, 0, 0) != FR_OK) {// Here 'drive_num' is logical drive number
                               io_out_string ( "FATFs_f_mkfs() failed\r\n" );
                           } else {
                               FSYS_Setup();
                               io_out_string ( "Formatted ok\r\n" );
                           }
                           }
                           break;
        case FS_Mount:     {
                           FATFS Fatfs;
                           ( FR_OK == FATFs_f_mount ( EMMC_DRIVE_CHAR-'0', &Fatfs ) )
                           ? io_out_string ( "FATFs_f_mount() ok\r\n" )
                           : io_out_string ( "FATFs_f_mount() failed\r\n" );
                           }
                           break;
        case FS_MkFs:      {
                           ( FR_OK == FATFs_f_mkfs ( EMMC_DRIVE_CHAR-'0', 0, 0 ) )
                           ? io_out_string ( "FATFs_f_mkfs() ok\r\n" )
                           : io_out_string ( "FATFs_f_mkfs() failed\r\n" );
                           }
                           break;
        case FS_Setup:     {
                           ( FSYS_OK == FSYS_Setup () )
                           ? io_out_string ( "FSYS_Setup() ok\r\n" )
                           : io_out_string ( "FSYS_Setup() failed\r\n" );
                           }
                           break;

        case FS_List:      if ( FILE_OK == file_initListing( EMMC_DRIVE ) )
                           {

                             io_out_string ( "Listing: " );
                             io_out_string ( EMMC_DRIVE );
                             io_out_string ( "\r\n" );

                             char fName[64];
                             U32 fSize;
                             Bool isDir;
                             struct tm when;
                             while ( FILE_OK == file_getNextListElement ( fName, sizeof(fName), &fSize, &isDir, &when) ) {

                               io_out_string ( fName );
                               io_out_string ( "\r\n" );
                             }

                           } else {
                             io_out_string ( "Cannot list " );
                             io_out_string ( EMMC_DRIVE );
                             io_out_string ( "\r\n" );
                           }
                           break;

        case FS_MkDir:     {
                           char dir[32];
                           strcpy ( dir, EMMC_DRIVE );
                           strcat ( dir, option );

                           if ( !f_exists(dir) ) {
                             ( FILE_OK == file_mkDir( dir ) )
                             ? io_out_string( "Directory created\r\n" )
                             : io_out_string( "Directory not created\r\n" );
                           } else {
                             io_out_string ( "Directory already present\r\n" );
                           }
                           }
                           break;
        case FS_Help:      {
                           int i;
                           io_out_string( "Commands:" );
                           for ( i=0; i<MAX_FSYS_CMDS; i++ ) {
                             io_out_string( " " );
                             io_out_string( FSYSCmd[i].name );
                           }
                           io_out_string( "\r\n" );
                           }
                           break;
        case FS_Quit:      return;
        }

      }

      io_out_string( "Test_FSYS> " );
    } else if ( inLength == 0 ) {
      io_out_string( "Test_FSYS> " );
    }
  }
}

static void Cmd_Test_SRAM( char* value ) {

  U8 const vv = ( *value ) ? ( atoi ( value ) & 0xFF ) : 0;

  U32 ram_size, errors = 0;
  volatile U8 *ext_ram = SRAM;
  U8 tmp;

  io_own_S32("\r\nChecking External SRAM at Addr ", (S32)&ext_ram[0], ": ");
  //io_out_string("\r\nChecking External SRAM\r\n");
  io_out_string("\r\nSize of Ext SRAM: ");
  io_dump_U32((U32)SRAM_SIZE, " bytes, ");
  io_dump_U32((U32)SRAM_SIZE/1024, " kB\r\n");

  for (ram_size = 0; ram_size < (U32)SRAM_SIZE; ram_size++) {
    smc_write_8(SRAM, ram_size, vv);            // write character to SRAM
    tmp = ext_ram[ram_size];
  #if 0            // display RAM check - takes several minutes
    io_out_S32("Ext SRAM 0x%08X: ", (S32)&ext_ram[ram_size]);
    io_out_X8(ext_ram[ram_size], "\r\n");
  #endif
    if (tmp != vv) {
      errors++;
      io_out_S32("Ext SRAM error at 0x%08X\r\n", (S32)&ext_ram[ram_size]);
    }
  }

  io_out_string("Ext SRAM test completed with ");
  io_dump_U32(errors, " errors out of ");
  io_dump_U32(ram_size, " bytes tested\r\n");
}

//
//  A Generic Access Point to test subsystems.
//  The name of the subsystem us passed in the 'option' argument.
//
//  Ideally, each subsystem to be tested provides a test function,
//  accepting one optional 'value' argument.
//
//  E.G., to test the Real Time Clock,
//  there would be a function RTC_Test()
//
//  Test functions can be fully automated (which is where the 'value' can come handy)
//  or may require operator input.
//
//  Also, there is the need to either pet the watchdog periodically,
//  or to tell the task monitoring task to ignore inactivity in this task.
//
static S16 Cmd_Test ( char* option, char* value, Access_Mode_t access_mode ) {

  if ( 1 || access_mode >= Access_Admin )
  {

    if ( 0 == strncasecmp ( "RTC", option, 3 ) )
    {

      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;
      Cmd_Test_RTC();
      gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;
      return CEC_Ok;
    }

    else if ( 0 == strncasecmp ( "FSYS", option, 4 ) )
    {

      //  TODO - Move code from 'Special' to here.
      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;
      Cmd_Test_FSYS( value );
      gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;
      return CEC_Ok;
    }

    else if ( 0 == strncasecmp ( "SRAM", option, 4 ) )
    {
      //  TODO - Move code from 'Special' to here.
      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;
      Cmd_Test_SRAM( value );
      gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;
      return CEC_Ok;

    }
    
    else if ( 0 == strncasecmp ( "Stream", option, 4 ) )
    {
      data_exchange_packet_t packet;

      packet.from               = DE_Addr_ControllerBoard_Commander;
      packet.to                 = DE_Addr_AuxDataAcquisition;
      packet.type               = DE_Type_Command;
      packet.data.Command.Code  = CMD_ADQ_StartStreaming;
      packet.data.Command.value.s32[0] = -1;

      data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );

      return CEC_Ok;

    } else if ( 0 == strncasecmp ( "Stop", option, 4 ) ) {

      data_exchange_packet_t packet;

      packet.from               = DE_Addr_ControllerBoard_Commander;
      packet.to                 = DE_Addr_AuxDataAcquisition;
      packet.type               = DE_Type_Command;
      packet.data.Command.Code  = CMD_ADQ_Stop;
      packet.data.Command.value.s32[0] = -1;

      data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );

      return CEC_Ok;

    } else {

      //  option unknown

    }
  }

  return CEC_Failed;

}
# if 0
//
//  SPI1 subsystem test.
//
static S16 Cmd_SPItest (char* data) {

    int count;
    char slaveResponse[] = "  ";
    U16 slaveData;


#if 0
    // GPIO map for SPI.
    static const gpio_map_t spi1Map = SPI_MASTER_1_PINS_CFG;

    // SPI chip select options
    spi_options_t spi1Options = SPI_MASTER_1_OPTIONS_CS0;

    // Assign I/Os to SPI.
//  gpio_enable_module(spi1Map, sizeof(spi1Map) / sizeof(spi1Map[0]));

    // Initialize as master.
    // (Any options set can be passed as only the mode fault detection field is used by 'spi_initMaster()')
//  spi_initMaster(SPI_MASTER_1, &spi1Options);

    // Set selection mode: fixed peripheral select, pcs decode mode, no delay between chip selects.
    spi_selectionMode(SPI_MASTER_1, SPI_MASTER_1_VARIABLE_PS, SPI_MASTER_1_DECODE_MODE, SPI_MASTER_1_PBA_CS_DELAY);

    // Configure chip-selects
    // CS0
    spi_setupChipReg(SPI_MASTER_1, &spi1Options, FOSC0);

    // Enable internal pull-up on MISO line
    gpio_enable_pin_pull_up(AVR32_SPI1_MISO_0_0_PIN);

    // Enable SPI.
    spi_enable(SPI_MASTER_1);
#endif


    // Send data
    if ( spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {

        // Select chip.  Second argument hardwires for spec board.
        spi_selectChip( SPI_MASTER_1, 0 );
        int dataLength = strlen(data);

        // Send string one character at a time
        for (count=0; count<dataLength; ) {
            // Send a byte
            spi_write( SPI_MASTER_1, data[count++] );

            // Look for returned byte.
            // They are delayed by one byte wrt to TX bytes
            spi_read( SPI_MASTER_1, &slaveData );
            if ( slaveData != 0 ) {
                // Print it to the screen
                slaveResponse[0] = (U8) slaveData;
                io_out_string( slaveResponse );
            }
        }

        // Do one more dummy write to get the last (delayed) byte from the slave.
        data = NULL;
        spi_write( SPI_MASTER_1, data[0] );
        // Look for the last returned byte
        spi_read( SPI_MASTER_1, &slaveData );
        if (slaveData != 0) {
            // Print it to the screen
            slaveResponse[0] = (U8) slaveData;
            io_out_string( slaveResponse );
        }

        spi_unselectChip( SPI_MASTER_1, 0);
    }

    return CEC_Ok;

}
# endif
# if 0
static S16 Cmd_SPIpins ( char* option, char* value ) {
# if 0
  if ( option ) {
    if ( 0 == strcasecmp ( "TX", option ) ) {
      Bool yn = ( value && value[0] == '1' ) ? true : false; SPI_I_Want_To_Send(yn); io_out_string ( yn ? "yesTX\r\n" : "noTX\r\n" );
    } else if ( 0 == strcasecmp( "RX", option ) ) {
      Bool yn = ( value && value[0] == '1' ) ? true : false; SPI_I_Can_Receive (yn); io_out_string ( yn ? "yesRX\r\n" : "noRX\r\n" );
    } else {
      io_out_string( SPI_You_Want_To_Send() ? "Other TX\r\n" : "Other not TX\r\n" );
      io_out_string( SPI_You_Can_Receive () ? "Other RX\r\n" : "Other not RX\r\n" );
    }
  } else {
      io_out_string ( "NooP\r\n" );
  }
# else
      io_out_string ( "NOOP\r\n" );
# endif
  return CEC_Ok;
}
# endif
//
// Retrieve humidity and temperature
//
//static S16 Cmd_Humidity (void)
//{
//    F32 val;
//
//    // should already be init...
//
//    if (sysmon_getHumiditySHT2X(&val) == SYSMON_FAIL) {
//        io_out_string("Error retrieving humidity");
//        return CEC_Failed;
//    } else
//        io_out_F32("Humidity: %.1f %%\r\n", val);
//
//    if (sysmon_getTemperature(&val) == SYSMON_FAIL) {
//        io_out_string("Error retrieving temperature");
//        return CEC_Failed;
//    }
//    else
//        io_out_F32("Temperature: %.1f C\r\n", val);
//
//    return CEC_Ok;
//}


static S16 Cmd_SystemMonitor(void)
{
    F32 vMain, vP3V3;
    S16 ret = CEC_Ok; 

    // system monitor should already be init

//  if (Cmd_Humidity() == CEC_Failed)
//      ret = CEC_Failed;

    // measure voltages
    if (sysmon_getVoltages(&vMain, &vP3V3) == SYSMON_FAIL) {
        io_out_string("Error retrieving voltages");
        ret = CEC_Failed;
    } else {
        io_own_F32( "VMAIN: ", vMain, 2, " V\r\n" );
        io_own_F32( "P3V3:  ", vP3V3, 2, " V\r\n" );
    }

    // measure current?

    return ret;
}



//  Set of options / values understood by Cmd_Modem()
//
//    option     value
//    ------     -----
//    BAUD       baudrate
//    COMM       -
//    AT         Manual, Automatic
//
static S16 Cmd_Modem (char* option, char* value)
{
  S16 ret = CEC_Ok; 
  char* msg = "";
  char rt[64];

  if ( 0 == strcasecmp( "BAUD", option ) )
  {
    U32 baudrate = atoi (value);

    if ( MDM_OK != mdm_setBaudrate  (baudrate))
    {
      ret = CEC_Failed;
    }

  }

  else if ( 0 == strcasecmp( "WAIT", option ))
  {

    int continue_checking = 1;
    int unresponsive = 1;

    do {

      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

      char in;

      if ( tlm_recv( &in, 1, TLM_PEEK | TLM_NONBLOCK ) ) {

        tlm_recv ( &in, 1, TLM_NONBLOCK );

        if ( in == '$' ) {
          continue_checking = 0;
        }
      }

      mdm_dtr_toggle_off_on();
      mdm_rts(1);

      int dsr = mdm_get_dsr();
      if  ( mdm_chat("AT","OK",2,"\r") > 0 )
      {
        snprintf ( rt, 63, "Modem is  responding, dsr=%d\r\n", dsr );
        tlm_send ( rt, strlen(rt), 0 );
        unresponsive= 0;
      }
      else
      {
        snprintf ( rt, 63, "Modem not responding, dsr=%d\r\n", dsr );
        tlm_send ( rt, strlen(rt), 0 );
        vTaskDelay((portTickType)TASK_DELAY_MS(500));
      }

    } while ( continue_checking && unresponsive );

    mdm_dtr (0);
    mdm_rts (0);

  }
  
  else if ( 0 == strcasecmp( "MONITOR", option ) )
  {

    gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

    int terminated = 0;

    do {

      snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n",
                     mdm_carrier_detect() ? "CD+" : "cd-",
                     mdm_get_dsr () ? "DSR+" : "dsr-",
                     mdm_get_dtr () ? "DTR+" : "dtr-",
                     mdm_get_cts () ? "CTS+":"cts-" );

      tlm_send ( rt, strlen(rt), 0 );

      char in;

      if ( tlm_recv( &in, 1, TLM_PEEK | TLM_NONBLOCK ) ) {

        tlm_recv ( &in, 1, TLM_NONBLOCK );

        if ( in == '$' ) {
          terminated = 1;
        }
      } else {

        vTaskDelay((portTickType)TASK_DELAY_MS(500));
      }

    } while ( !terminated );

  } else if ( 0 == strcasecmp( "COMM", option ) ) {

    mdm_dtr_toggle_off_on();
    mdm_rts(1);

    vTaskDelay((portTickType)TASK_DELAY_MS(500));
    snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
    tlm_send ( rt, strlen(rt), 0 );

    vTaskDelay((portTickType)TASK_DELAY_MS(500));
    snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
    tlm_send ( rt, strlen(rt), 0 );

    vTaskDelay((portTickType)TASK_DELAY_MS(500));
    snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
    tlm_send ( rt, strlen(rt), 0 );

    vTaskDelay((portTickType)TASK_DELAY_MS(500));
    snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
    tlm_send ( rt, strlen(rt), 0 );

    int terminated = 0;
    int updateMonitor = 20;
    int cnt_alive = 0;

    do {

      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

      char in;

      if ( updateMonitor>=20  ) {
        snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
        tlm_send ( rt, strlen(rt), 0 );
        updateMonitor=0;
      } else {
        updateMonitor++;
      }

      if  ( tlm_recv( &in, 1, TLM_PEEK | TLM_NONBLOCK ) ) {

        tlm_recv ( &in, 1, TLM_NONBLOCK );

        if ( in == '$' )
        {
          terminated = 1;
        }
        else
        {
          if ( in == '\r' )
          {
            updateMonitor = 0;
          }
          else
          {
            updateMonitor = -40;
          }
          tlm_send ( &in, 1, 0 );
          cnt_alive = 0;
          mdm_send ( &in, 1, 0, 0 );
        }
      }

      if ( mdm_recv( &in, 1, MDM_PEEK | MDM_NONBLOCK ) ) {

        mdm_recv ( &in, 1, MDM_NONBLOCK );
        tlm_send ( &in, 1, 0 );
        cnt_alive = 0;
      }

      vTaskDelay((portTickType)TASK_DELAY_MS(50));

      cnt_alive++;
      if  ( cnt_alive>=100 )
      {
        msg = "Alive\r\n";
        tlm_send ( msg, strlen(msg), 0 );
        cnt_alive = 0;
      }

    } while ( !terminated );

    mdm_dtr(0);
    mdm_rts(0);

  }

  else if ( 0 == strcasecmp( "*", option ) )
  {

    mdm_rts(1);

    gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

    if ( !mdm_wait_for_dsr() )
    {
      msg = "Modem did NOT assert DSR. ABORT.\r\n";
      tlm_send ( msg, strlen(msg), 0 );

      msg = "Modem log:\r\n";
      tlm_send ( msg, strlen(msg), 0 );
      mdm_log (1);

      return CEC_Failed;
    }

    if ( mdm_getAtOk () )
    {
        msg = "Modem responding\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }
    else
    {
        msg = "Modem is NOT responding\r\n";
        tlm_send ( msg, strlen(msg), 0 );

        msg = "Modem log:\r\n";
        tlm_send ( msg, strlen(msg), 0 );
        mdm_log (1);

        return CEC_Failed;
    }

    msg = "Modem  init: ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( mdm_isRegistered () <= 0 )
    {
        msg = "Failed\r\n";
        tlm_send ( msg, strlen(msg), 0 );

        msg = "Modem log:\r\n";
        tlm_send ( msg, strlen(msg), 0 );
        mdm_log (1);

        return CEC_Failed;

    }
    else
    {
        msg = "OK\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }

# if 0 //  These are only used during development and testing
    if ( mdm_isIridium() ) {
        msg = "Modem is Iridium\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    } else {
        msg = "Modem is not Iridium\r\n";
        tlm_send ( msg, strlen(msg), 0 );

        msg = "Modem log:\r\n";
        tlm_send ( msg, strlen(msg), 0 );
        mdm_log (1);

        return CEC_Failed;
    }

    msg = "Modem FWrev: ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( mdm_getFwRev ( rt, 64 ) ) {
        tlm_send ( rt, strlen(rt), 0 );
        msg = "\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    } else {
        msg = "N/A\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }


    msg = "Modem ICCID: ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( mdm_getIccid ( rt, 64 ) ) {
        tlm_send ( rt, strlen(rt), 0 );
        msg = "\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    } else {
        msg = "N/A\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }


    msg = "Modem IMEI:  ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( mdm_getImei ( rt, 64 ) ) {
        tlm_send ( rt, strlen(rt), 0 );
        msg = "\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    } else {
        msg = "N/A\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }


    msg = "Modem model: ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( mdm_getModel ( rt, 64 ) ) {
        tlm_send ( rt, strlen(rt), 0 );
        msg = "\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    } else {
        msg = "N/A\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }
# endif

    msg = "Modem configuration: ";
    tlm_send ( msg, strlen(msg), 0 );
    int rv;
    if ( 1 == ( rv = mdm_configure () ) )
    {
        msg = "OK\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }
    else
    {
        snprintf ( rt, 63, "N/A %d\r\n", rv );
        tlm_send ( rt, strlen(rt), 0 );
    }

    msg = "Modem reg:   ";
    tlm_send ( msg, strlen(msg), 0 );
    if ( !mdm_isRegistered () )
    {
        msg = "Failed\r\n";
        tlm_send ( msg, strlen(msg), 0 );
    }
    else
    {
        msg = "OK\r\n";
        tlm_send ( msg, strlen(msg), 0 );

        int attempt = 0;
        int signalStrength = 0;
        int const neededSignalStrength = 25;

        do
        {

          msg = "Modem sgnal: ";
          tlm_send ( msg, strlen(msg), 0 );
          int num, min, avg, max;
          if ( ( num = mdm_getSignalStrength ( 3+attempt, &min, &avg, &max ) ) > 0 )
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

        } while ( attempt++<=5 && signalStrength < neededSignalStrength );

        if ( signalStrength >= neededSignalStrength )
        {
          msg = "Modem dial:  ";
          tlm_send ( msg, strlen(msg), 0 );

          if ( mdm_connect ("ATD00881600005183\r", 90) <= 0 )
          {
            msg = "Failed\r\n";
            tlm_send ( msg, strlen(msg), 0 );

            snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
            tlm_send ( rt, strlen(rt), 0 );

          }
          else
          {
            msg = "Connected\r\n";
            tlm_send ( msg, strlen(msg), 0 );

            snprintf (rt, sizeof(rt), "Modem %s %s %s %s\r\n",
                      mdm_carrier_detect() ? "CD+" : "cd-",
                      mdm_get_dsr() ? "DSR+" : "dsr-",
                      mdm_get_dtr() ? "DTR+" : "dtr-",
                      mdm_get_cts() ? "CTS+" : "cts-"
                     );

            tlm_send ( rt, strlen(rt), 0 );

# if 1
            msg = "Modem login: ";
            tlm_send ( msg, strlen(msg), 0 );

            if ( mdm_login(120, "f0002\r", "S(n)=2n\r" ) <= 0 )
            {

              msg = "NO PROMPT\r\n";
              tlm_send ( msg, strlen(msg), 0 );

            }
            else
            {

              msg = "got prompt\r\n";
              tlm_send ( msg, strlen(msg), 0 );

              msg = "Modem communicating: ";
              tlm_send ( msg, strlen(msg), 0 );

              int bytesSent;
              int mdm_rv;
              if ( (mdm_rv = mdm_communicate( 300, 20, &bytesSent ) ) <= 0 )
              {

                if ( 0 == mdm_rv ) {
                  msg = "Timed Out\r\n";
                } else {
                  msg = "Lost Connection or Failed Sending\r\n";
                }
                tlm_send ( msg, strlen(msg), 0 );

              }

              else
              {

                snprintf ( rt, sizeof(rt), "sent %d bytes\r\n", bytesSent );
                tlm_send ( rt, strlen(rt), 0 );

                msg = "Modem logout: ";
                tlm_send ( msg, strlen(msg), 0 );

                if ( mdm_logout() <= 0 ) {

                  msg = "FAILED.\r\n";
                  tlm_send ( msg, strlen(msg), 0 );

                } else {

                  msg = "done.\r\n";
                  tlm_send ( msg, strlen(msg), 0 );
                }
              }
            }

            snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
            tlm_send ( rt, strlen(rt), 0 );
# else
            msg = "Reading";
            tlm_send ( msg, strlen(msg), 0 );

            struct timeval read_until; gettimeofday ( &read_until, NULL ); read_until.tv_sec += 30;

            int nRead = 0;
            int nTries = 30000/50;

            do {

              char next;
              if ( mdm_recv ( &next, 1, MDM_NONBLOCK ) ) {
                  tlm_send( &next, 1, 0 );
                  nRead++;
              } else {
                  vTaskDelay((portTickType)TASK_DELAY_MS(50));
              }

              if ( 0 == nTries%20 ) { msg = "."; tlm_send ( msg, strlen(msg), 0 ); }

            } while ( nTries-->0 );
//   } while ( timeval_compare_to_now ( &read_until ) <= 0 );

            snprintf ( rt, 63, " %d characters from connection\r\n", nRead );
            tlm_send ( rt, strlen(rt), 0 );
# endif
          }

          msg = "Modem +ESC+: ";
          tlm_send ( msg, strlen(msg), 0 );
  
          if ( mdm_escape() ) {
            msg = "back in command mode\r\n";
          } else {
            msg = "FAILED\r\n";
          }
          tlm_send ( msg, strlen(msg), 0 );

          snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
          tlm_send ( rt, strlen(rt), 0 );

          msg = "Modem hngup: ";
          tlm_send ( msg, strlen(msg), 0 );

          if ( mdm_hangup() ) {
            msg = "done\r\n";
          } else {
            msg = "FAILED\r\n";
          }
          tlm_send ( msg, strlen(msg), 0 );

          snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
          tlm_send ( rt, strlen(rt), 0 );

          msg = "Modem cnnct: ";
          tlm_send ( msg, strlen(msg), 0 );
          int connection = mdm_getLCC ();
          snprintf ( rt, 63, "%s\r\n", (connection==6) ? "Disconnected" : "STILL CONNECTED, ERROR" );
          tlm_send ( rt, strlen(rt), 0 );

          snprintf ( rt, sizeof(rt), "Modem %s %s %s %s\r\n", mdm_carrier_detect()?"CD+":"cd-", mdm_get_dsr()?"DSR+":"dsr-", mdm_get_dtr()?"DTR+":"dtr-", mdm_get_cts()?"CTS+":"cts-" );
          tlm_send ( rt, strlen(rt), 0 );
        }
    }
  
    msg = "Modem done.\r\n";
    tlm_send ( msg, strlen(msg), 0 );

    mdm_rts(0);
    mdm_dtr(0);

    msg = "Modem log:\r\n";
    tlm_send ( msg, strlen(msg), 0 );
    mdm_log (1);

  } else {
        ret = CEC_Failed;
  }

  return ret;
}



S16 DialString (char dial[], int len)
{

  fHandler_t fh;
  if ( FILE_OK != f_open ( "0:\\DIAL", O_RDONLY, &fh ) )
  {
    return CEC_Failed;
  }

  char rd;
  int i = 0;
  while ( 1 == f_read ( &fh, &rd, 1 ) )
  {
    if  (i < len - 1)
      dial[i++] = rd;
  }
  dial[i] = 0;

  return CEC_Ok;
}



static S16 Cmd_Dial (char* option)
{
  if  ( !option || !option[0] )
  {
    char dial[32];

    if  (CEC_Ok == DialString( dial, 31 ) )
    {
      io_out_string( dial );
      io_out_string( "\r\n" );
    }
    else
    {
      return CEC_Failed;
    }

  }
  else
  {
    fHandler_t fh;

    if  ( FILE_OK != f_open ( "0:\\DIAL", O_WRONLY | O_CREAT, &fh ) )
    {
      return CEC_Failed;
    }

    f_write ( &fh, option, strlen(option) );
    char LF = '\r';
    f_write ( &fh, &LF, 1 );

    f_close ( &fh );
  }

  return CEC_Ok;
}




//
//  An ancillary RS232 sensor subsystem test.
//
static S16 Cmd_SerialSensorTest (char *sensor, char* option, char* value) {

    //  Make sure firmware will not reboot while executing command
    gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

    int const maxChar = 32;
    uint8_t gsp_byte;
    uint8_t gsp_char[maxChar+1];  // +1 for trailing '\0' character
    int nChar = 0;
    int totalChar = 0;
    int nTry = ( value && *value ) ? (1000*atoi(value)) : 6000;
    if (nTry>30000) nTry = 30000;
    if (nTry<1000 ) nTry = 1000;

    if ( 0 == strncasecmp ( "OCR", sensor, 3 ) ) {

        uint32_t baud_rate = OCR_USART_BAUDRATE;
        if ( option && *option ) baud_rate = atoi(option);

        io_out_S32("Testing OCR port at %ld:\r\n", (S32)baud_rate );

        if (0 != GSP_Init ( GSP_OCR, FOSC0, baud_rate, OCR_FRAME_LENGTH, 32 ))
            io_out_string("GSP_Init failed\r\n");

        //GSP_Send(GSP_OCR, "Hello\r\n", 7, GSP_NONBLOCK);

        while ( nChar<maxChar && nTry-- ) {

            if ( 1 == GSP_Recv( GSP_OCR, &gsp_byte, 1, GSP_NONBLOCK ) ) {
                gsp_char[nChar++] = gsp_byte;
            } else {
                vTaskDelay((portTickType)TASK_DELAY_MS(1));
            }

            if ( nChar == maxChar ) {
                gsp_char[nChar] = 0;
                io_out_string( (char*)(gsp_char) );    // output collected characters
                totalChar += nChar;
                nChar=0;
            }
        }
                gsp_char[nChar] = 0;
                io_out_string( (char*)(gsp_char) );    // output collected characters
                io_out_S32   ( "Received %ld characters:\r\n", (S32)(totalChar+nChar) );
                nChar=0;

        GSP_Deinit ( GSP_OCR );

    } else if (0 == strncasecmp("MCOMS", sensor, 5)) {

        uint32_t baud_rate = MCOMS_USART_BAUDRATE;
        if ( option && *option ) baud_rate = atoi(option);

        io_out_S32("Testing MCOMS port at %ld:\r\n", (S32)baud_rate );

        if (0 != GSP_Init ( GSP_MCOMS, FOSC0, baud_rate, MCOMS_FRAME_LENGTH, 32 ))
            io_out_string("GSP_Init failed\r\n");

        //usart_putchar(MCOMS_USART, 'H');    // this works!

        // int ret = (int32_t)GSP_Send(GSP_MCOMS, "Hello\r\n", 7, GSP_NONBLOCK);
        // io_out_S32("%ld returned from send\r\n", ret);

        while ( nChar<maxChar && nTry-- ) {

            if ( 1 == GSP_Recv( GSP_MCOMS, &gsp_byte, 1, GSP_NONBLOCK ) ) {
                gsp_char[nChar++] = gsp_byte;
            } else {
                vTaskDelay((portTickType)TASK_DELAY_MS(5));
            }

            if ( nChar == maxChar ) {
                gsp_char[nChar] = 0;
                io_out_string( (char*)(gsp_char) );    // output collected characters
                totalChar += nChar;
                nChar=0;
            }
        }

                gsp_char[nChar] = 0;
                io_out_string( (char*)(gsp_char) );    // output collected characters
                io_out_S32   ( "Received %ld characters:\r\n", (S32)(totalChar+nChar) );
                nChar=0;

        GSP_Deinit ( GSP_MCOMS );

    } else {
        io_out_string("Usage: serialsensor OCR|MCOMS [baudrate]\r\n");
        return CEC_Failed;
    }

    return CEC_Ok;
}


static S16 Cmd_Special ( char* option, char* value, Access_Mode_t access_mode ) {

    //  Supported special commands:
    //    *  SRAMRead address
    //    *  DumpCFG
    //    F  SetCFG
    //    F  WipeCFG
    //    F  NewSetup
    //    F  WipeFSys
    //    F  SetupFSys
    //  # F  FillFSys
    //    *  DarkTest
    //    *  XM1k, XM128, XMCRC, XMCS

    if ( 0 == strcasecmp( "DumpCFG", option ) ) {

        dump_UPG();
        dump_RTC();
        return CEC_Ok;

    } else if ( 0 == strcasecmp( "SetCFG", option ) && access_mode >= Access_Factory ) {

        char* next;
        U16 const loc = strtol ( value, &next, 10 );
        U8  setTo;
        U8  howMany;

        if ( !next || 0 == next+1 ) {
            setTo = 0;
            howMany = 1;
        } else {
            setTo = strtol ( next+1, &next, 10 );
            if ( !next || 0 == next+1 ) {
                howMany = 1;
            } else {
                howMany = atoi ( next+1 );
            }
        }

        U16 offset;
        for ( offset = 0; offset < howMany; offset ++ ) {
            if ( loc+offset < 256 ) {
                CFG_VarSaveToRTC ( (char*)0, loc+offset, &setTo, (U8)1 );
            } else if ( loc+offset < 512 ) {
                CFG_VarSaveToUPG ( (char*)0, loc+offset, &setTo, (U8)1 );
            }
        }

        return CEC_Ok;

    } else if ( 0 == strcasecmp( "WipeCFG", option ) && access_mode >= Access_Factory ) {

        U8 const u8_val = atoi(value);
        wipe_UPG( u8_val );
        wipe_RTC( u8_val );
        CFG_WipeBackup();
        return CEC_Ok;

# if 0
    } else if ( 0 == strcasecmp( "NewSetup", option ) && access_mode >= Access_Factory ) {


        U8 const u8_val = 255;
        wipe_UPG( u8_val );
        wipe_RTC( u8_val );
        CMD_wipe( '0', 'Z' );   //  Z == 0x5A == 0b01011010
        CMD_wipe( '1', 'Z' );
        return CEC_Ok;

    } else if ( 0 == strcasecmp ( "WipeFSys", option) && access_mode >= Access_Factory ) {

        CMD_wipe( value[0], value[1] );
        return CEC_Ok;
# endif
# if 1 // XMTST
    } else if ( 0 == strcasecmp ( "XM1k", option) ) {
        XM_use1k  = true;
        return CEC_Ok;
    } else if ( 0 == strcasecmp ( "XM128", option) ) {
        XM_use1k  = false;
        return CEC_Ok;
    } else if ( 0 == strcasecmp ( "XMCRC", option) ) {
        XM_useCRC = true;
        return CEC_Ok;
    } else if ( 0 == strcasecmp ( "XMCS", option) ) {
        XM_useCRC = false;
        return CEC_Ok;
# endif
    } else {
        return CEC_Failed;
    }
}

# if 0
static S16 Cmd_Info ( char* option, char* result, S16 r_max_len ) {

    if ( 0 == strcasecmp( "FirmwareVersion", option ) ) {
        strncpy ( result,
                HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH,
                r_max_len-1 );
        return CEC_Ok;
    } else {
        return CEC_Failed;
    }
}
# endif

static S16 Cmd_SelfTest () {

    return Info_SensorDevices( 0, "" )
         ? CEC_Ok
         : CEC_Failed;
}

static S16 Cmd_Sensor ( char* option ) {

    if ( 0 == strcasecmp ( "ID", option ) )
    {
        Info_SensorID ( (fHandler_t*)0, "SATFHR," );
    }
    else if ( 0 == strcasecmp ( "State", option ) )
    {
        Info_SensorState ( (fHandler_t*)0, "SATFHR," );
    }
    else if ( 0 == strcasecmp ( "Devices", option ) )
    {
        Info_SensorDevices ( (fHandler_t*)0, "SATFHR," );
    }
    else if ( 0 == strcasecmp ( "Processing", option ) )
    {
        Info_ProcessingParameters ( (fHandler_t*)0, "SATFHR," );
    }
    else
    {
        return CEC_Failed;
    }

    return CEC_Ok;
}


void command_controller_reboot () {

    // reset SV user register
    if (CFG_PCB_Supervisor_Available == CFG_Get_PCB_Supervisor()) {
        S8 errval;
        // reset register, no messages
        supervisorWriteUserRegister(SPV_CHECK_REG, SPV_CHECK_REG_RSTVAL, &errval);
    }

    io_out_string ( "Reboot" );
    watchdog_enable(450000);
    while(1) { vTaskDelay((portTickType)TASK_DELAY_MS(77)); io_out_string ( "." ); }


}

static S16 CMD_DropToBootloader() {

    U32 const boot_mode = 0xA5A5BEEFL;

    flashc_memcpy((void *)(AVR32_FLASHC_USER_PAGE_ADDRESS+0), &boot_mode, sizeof(boot_mode), true);

    return CEC_Ok;
}

//  Set of commands understood by command shell:
//  Commands are
//      <command_string> [--option|--option value|--option=value]
typedef enum {
    //  General Information
//  CS_Info,
    CS_SelfTest,
    CS_Sensor,
    //  Configuration parameters
    CS_Get,
    CS_Set,
    //  Setup
# ifdef STEPWISE_UNSHELVING
    CS_AccelerCal,
    CS_CompassCal,
# endif
    //  File access
    CS_List,
    CS_Output,
    CS_Dump,
    CS_Send,
    CS_Receive,
    CS_CRC,
    CS_Delete,
    CS_Erase,
    //  Testing & Debugging
    CS_Ping,
    CS_AllPing,
    CS_Query,
    CS_Test,
//  CS_SPItest,
//  CS_SPIpins,
    CS_OCR,
    CS_MCOMS,
//  CS_Humidity,
    CS_SysMon,
    CS_Modem,
    CS_Dial,
    CS_Special,
    CS_Slow,
    //  Program control
    CS_Access,
    CS_FirmwareUpgrade,
    CS_Reboot,
    //  Accept but ignore
    CS_Dollar
} ShellCommandID;

typedef struct {
    char* name;
    ShellCommandID id;
} ShellCommand_t;

static ShellCommand_t shellCmd[] = {
        //  General Information
//      { "$Info",       CS_Info },      //  Support legacy command for SUNACom convenience
        { "SelfTest",    CS_SelfTest },
        { "Sensor",      CS_Sensor },
        //  Configuration parameters
        { "Get",         CS_Get },
        { "Set",         CS_Set },
        //  Setup
# ifdef STEPWISE_UNSHELVING
        { "CompassCal",  CS_CompassCal },
        { "AccCal",      CS_AccelerCal },
# endif
        //  File access
        { "List",        CS_List },
        { "Output",      CS_Output },
        { "Dump",        CS_Dump },
        { "Send",        CS_Send },
        { "Receive",     CS_Receive },
        { "CRC",         CS_CRC },
        { "Delete",      CS_Delete },
        { "Erase",       CS_Erase },
        //  Testing & Debugging
        { "Ping",        CS_Ping },
        { "AllPing",     CS_AllPing },
        { "Query",       CS_Query },
        { "Test",         },
//      { "SPItest",     CS_SPItest },
//      { "SPIpins",     CS_SPIpins },
        { "OCRTest",     CS_OCR },
        { "MCOMSTest",   CS_MCOMS },
//      { "Humidity",    CS_Humidity },
        { "SysMon",      CS_SysMon },
        { "Modem",       CS_Modem },
        { "Dial",        CS_Dial },
        { "Special",     CS_Special },
        { "Slow",        CS_Slow },
        //  Program control
        { "Access",      CS_Access },
        { "Upgrade",     CS_FirmwareUpgrade },
        { "Reboot",      CS_Reboot },
        { "$",           CS_Dollar }     //  Support legacy command for SUNACom convenience
};

static const S16 MAX_SHELL_CMDS = sizeof(shellCmd) / sizeof (ShellCommand_t);


static void CMD_Handle ( char* cmd, Access_Mode_t* access_mode, int* ui_flag )
{

  gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

  bool changed_cfg = false;
  Bool needReboot = false;

  S32  waitReboot = 0;
  Bool match = false;
  S16 try = 0;
  char* arg = 0;

  //  Skip leading white space
  while ( *cmd == ' ' || *cmd == '\t' ) cmd++;

  do 
  {
    if  (0 == strncasecmp (cmd, shellCmd[try].name, strlen (shellCmd[try].name)))
    {
      match = true;
      arg = cmd + strlen(shellCmd[try].name);
    }
    else
    {
      try++;
    }
  } while ( !match && try < MAX_SHELL_CMDS );

  S16 cec = CEC_Ok;
  char result[64];
  result[0] = 0;

  if  ( try == MAX_SHELL_CMDS )
  {
    if  (*cmd == 0)
    {
      cec = CEC_EmptyCommand;
    }
    else
    {
      cec = CEC_UnknownCommand;
    }
  }
  
  else
  {
    //  Split the remainder of the command into
    //  option and (optional) value parts

    //  Skip over spaces and "--" option prefix.
    //  Nothing will be done if this is an option-less command.
    while ( *arg == ' '
         || *arg == '\t'
         || *arg == '-' )
    {
      arg++;
    }
    //  Option argument to current command starts now
    char* option = arg;

    //  Skip over option argument characters to see if value string is following
    while ( isalnum ( *arg )
         || *arg == '_'
         || *arg == '#'
         || *arg == '*' )
    {
      arg++;
    }

    char* value;

    if ( 0 == *arg )
    {
      //  No value in command string.
      value = "";
    }
    
    else
    {
      //  Skip over white space / assignment character
      while ( *arg == ' '
           || *arg == '\t'
           || *arg == '=' )
      {
        *arg = 0;   //  Terminate the option string.
        arg++;
      }
      value = arg;    //  Value starts at first non-space/non-assignment next character.
    }

    if ( CFG_Get_Message_Level() >= CFG_Message_Level_Trace )
    {
      io_out_string ( "CMD:\t" );
      io_out_string ( shellCmd[try].name );
      io_out_string ( "\t" );
      io_out_string ( option );
      io_out_string ( "\t" );
      io_out_string ( value );
      io_out_string ( "\r\n" );
    }

    //  Make sure firmware will not get stuck while executing command
    if ( true /* CFG_Watchdog_On == CFG_Get_Watchdog() */ )
    {
      watchdog_reenable();
    }

    switch ( shellCmd[try].id )
    {

    //  General Information - Only at Op_Idle
//    case CS_Info:        cec = Cmd_Info     ( option, result, sizeof(result) ); break;
      case CS_SelfTest:    cec = Cmd_SelfTest ();                                 break;
      case CS_Sensor:      cec = Cmd_Sensor   ( option );                         break;

      //  Configuration parameters - Only at Op_Idle
      case CS_Get:         cec = CFG_CmdGet   ( option, result, sizeof(result) ); break;
      case CS_Set:         cec = CFG_CmdSet   ( option, value, *access_mode );
                           if ( CEC_Ok == cec ) { changed_cfg = true; }
                           break;

      //  Setup
# ifdef STEPWISE_UNSHELVING
      case CS_CompassCal:  cec = Cmd_CompassCal  ();
      case CS_AccelerCal:  cec = Cmd_AccelerCal  ();
# endif
        //  File access - Only at Op_Idle
        case CS_List:        cec = FSYS_CmdList    ( option                     );            break;
        case CS_Output:      cec = FSYS_CmdDisplay ( option, value, access_mode, 'A' );       break;
        case CS_Dump:        cec = FSYS_CmdDisplay ( option, value, access_mode, 'X' );       break;
        case CS_Send:        cec = FSYS_CmdSend    ( option, value, access_mode, XM_use1k );  break;
        case CS_Receive:     cec = FSYS_CmdReceive ( option, value, access_mode, XM_useCRC ); break;
        case CS_CRC:         cec = FSYS_CmdCRC     ( option, value, result, sizeof(result) ); break;
        case CS_Delete:      cec = FSYS_CmdDelete  ( option, value, access_mode );            break;
        case CS_Erase:       cec = FSYS_CmdErase   ( option, value, access_mode );            break;

        //  Testing & Debugging
        case CS_Ping:        cec = Cmd_Ping        ( option );                      break;
        case CS_AllPing:     cec = Cmd_AllPing     ();                              break;
        case CS_Query:       cec = Cmd_Query       ( option );                      break;
        case CS_Test:        cec = Cmd_Test        ( option, value, *access_mode ); break;
//      case CS_SPItest:     cec = Cmd_SPItest     ( option );                      break;
//      case CS_SPIpins:     cec = Cmd_SPIpins     ( option, value );               break;
        case CS_OCR:         cec = Cmd_SerialSensorTest( "OCR",   option, value );  break;
        case CS_MCOMS:       cec = Cmd_SerialSensorTest( "MCOMS", option, value );  break;
//      case CS_Humidity:    cec = Cmd_Humidity     ();                             break;
        case CS_SysMon:      cec = Cmd_SystemMonitor();                             break;
        case CS_Modem:       cec = Cmd_Modem       ( option, value );               break;
        case CS_Dial:        cec = Cmd_Dial        ( option );                      break;

        //  Special function access, Factory PW protected - Only at Op_Idle
        case CS_Special:     cec = Cmd_Special     ( option, value, *access_mode ); break;

        case CS_Slow:        (*ui_flag) |= 0x01;                                       break;

        //  Program control - Only at Op_Idle
        case CS_Access:      cec = Cmd_Access      ( option, value, access_mode );  break;

        case CS_FirmwareUpgrade:
                             cec = CMD_DropToBootloader();
                             if ( CEC_Ok == cec )
                             {
                               CFG_Set_Firmware_Upgrade ( CFG_Firmware_Upgrade_Yes );
                               needReboot = true;
                               if  ( option[0] )
                                 waitReboot = atoi ( option );
                             }
                             break;

        case CS_Reboot:      cec = CEC_Ok;
                             needReboot = true;
                             if  (option[0])
                               waitReboot = atoi ( option );
                             break;


        case CS_Dollar:      //  Accept "$", "$$", ...
                             //  Reject "$[^$]*"
                             while ( *cmd == '$' ) cmd++;
                             if ( *cmd ) cec = CEC_Failed;
                             else        cec = CEC_Ok;
                             break;

        default:             break;
        }
    }

    if ( CEC_Ok == cec ) {
        if ( shellCmd[try].id != CS_Dollar ) {
            //  Output result (if present)
            io_out_string( "$Ok " );
            io_out_string( result );
            io_out_string( "\r\n" );
        } else {
            //  Dollar sign is a special case.
            //  It serves as a request to re-send the prompt.
            io_out_string( "\r\n" );
        }
    } else if ( cec == CEC_EmptyCommand ) {
        io_out_string( "\r\n" );
    } else {
        //  Output error code
        io_out_S32 ( "$Error: %ld\r\n", (S32)cec );
    }

    if ( needReboot ) {

        if ( changed_cfg ) CFG_Save( true );

        //  FIXME  First send 'StopAll' command to all tasks !!! and then delay and/or wait for confirmation
        hnv_sys_spec_board_stop();

        while ( waitReboot > 0 ) {
            io_out_S32( "Reboot wait %d\r\n", waitReboot );
            vTaskDelay((portTickType)TASK_DELAY_MS( 1000 ));
            waitReboot--;
        }

        command_controller_reboot();
    }

    //  Disable watchdog again, so no reset occurs while waiting for next input
    //  watchdog_disable ();

    gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;

    return;
}

# if defined(OPERATION_NAVIS)
//  Set of commands accepted in APM Operation Mode:
//  Commands are
//      <command_string>[,option[,value]*]
typedef enum {
    //
    CS_W,
    CS_HNVSTS,
    CS_PRFBEG,
    CS_PRFSTS,
    CS_PRFEND,
    CS_TXBEG,
    CS_TXSTS,
    CS_TXRLT,
    CS_TXEND,
    CS_GPS,
//  CS_CTD,
    CS_TIME,
    CS_FIRMWARE,
    CS_SERIAL,
    CS_SLP,
} APMCommandID;

typedef struct {
    char* name;
    APMCommandID id;
} APMCommand_t;

static APMCommand_t APMCmd[] = {
    //  APM operation control
    { "W",         CS_W },
    { "HNVSTS",    CS_HNVSTS },
    { "PRFBEG",    CS_PRFBEG },
    { "PRFSTS",    CS_PRFSTS },
    { "PRFEND",    CS_PRFEND },
    { "TXBEG",     CS_TXBEG },
    { "TXSTS",     CS_TXSTS },
    { "TXRLT",     CS_TXRLT },
    { "TXEND",     CS_TXEND },
    { "GPS",       CS_GPS },
//  { "CTD",       CS_CTD },
    { "TIME",      CS_TIME },
    { "FIRMWARE",  CS_FIRMWARE },
    { "SERIAL",    CS_SERIAL },
    { "SLP",       CS_SLP }
};

static const S16 MAX_APM_CMDS = sizeof(APMCmd)/sizeof(APMCommand_t);

static S16 APM_CMD_Handle ( char* cmd ) {

    data_exchange_packet_t packet;

    Bool match = false;
    S16 try = 0;
    char* arg = 0;

    //  Skip leading white space
    while ( *cmd == ' ' || *cmd == '\t' ) cmd++;

    do {
        if ( 0 == strncasecmp ( cmd, APMCmd[try].name, strlen(APMCmd[try].name) ) ) {
            match = true;
            arg = cmd + strlen( APMCmd[try].name );
        } else {
            try++;
        }
    } while ( !match && try < MAX_APM_CMDS );

    S16 cec = CEC_Ok;

    if ( try == MAX_APM_CMDS ) {
        if ( *cmd == 0 ) {
            cec = CEC_EmptyCommand;
        } else {
            cec = CEC_UnknownCommand;
        }
    } else {
        //  Split the remainder of the command into
        //  optional option and optional value parts

        //  Option argument to current command starts now
        char* option = arg;
        char* value;

        if ( *arg != ',' ) {
            option = "";
            value  = "";
        } else {
            arg++;
            option = arg;

            //  Option part of command is all letters or a (float) number
            while ( isalnum ( *arg ) || '.' == *arg ) {
                arg++;
            }

            if ( 0 == *arg ) {
                //  Nothing more in command string.
                value = "";
            } else if ( *arg != ',' ) {
                //  Ignore unexpected trailing end
                value = "";
            } else {
                value = arg+1;  //  There may be more than one value in the comma separated value list
            }
        }

        int16_t option_int = (*option)?atoi(option):0;
        int16_t value_int  = (*value)?atoi(value):0;


        if ( CFG_Get_Message_Level() >= CFG_Message_Level_Trace ) {
            io_out_string ( "CMD:\t" );
            io_out_string ( APMCmd[try].name );
            io_out_string ( "\t" );
            io_out_string ( option );
            io_out_string ( "\t" );
            io_out_string ( value );
            io_out_string ( "\r\n" );
        }

        //  Make sure firmware will not get stuck while executing command
        gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

        switch ( APMCmd[try].id ) {

        //  Execute received command
        case CS_W:
                io_out_string ( "$ACK,W\r\n" );
                break;
        case CS_HNVSTS:
                io_out_string ( "$ACK,HNVSTS," );
                switch ( CFG_Get_Operation_State() ) {
                    case CFG_Operation_State_Prompt:       io_out_string ( "OK\r\n"   ); break;
                    case CFG_Operation_State_Profiling:    io_out_string ( "PRF\r\n"  ); break;
                    case CFG_Operation_State_Transmitting: io_out_string ( "TX\r\n"   ); break;
                    default:                               io_out_string ( "FAIL\r\n" ); break;
                }
                break;
        case CS_PRFBEG:
                if ( CFG_Operation_State_Profiling == CFG_Get_Operation_State() ) {
                        //  No need to act
                } else {
                    if ( CFG_Operation_State_Transmitting == CFG_Get_Operation_State() ) {
                        //  End transmitting
                        packet.to         = DE_Addr_ProfileManager;
                        packet.from       = DE_Addr_ControllerBoard_Commander;
                        packet.type       = DE_Type_Command;
                        packet.data.Command.Code = CMD_PMG_Transmit_End;
                        data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                        //  Do not need confirmation
                        //  The CMD_PMG_Profile packet immediately following
                        //  is the relevant command that needs acknowledgement.
                    }
                    //  Begin profiling
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Profile;
                    packet.data.Command.value.s64 = 2;  //  Number of light frames following a dark frame
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Profiling );
                }
                io_out_string ( "$ACK,PRFBEG,OK\r\n" );
                break;
        case CS_PRFSTS:
                io_out_string ( "$ACK,PRFSTS," );
                switch ( CFG_Get_Operation_State() ) {
                    case CFG_Operation_State_Profiling:    io_out_string ( "PRF\r\n"   ); break;
                    case CFG_Operation_State_Prompt:       io_out_string ( "DONE\r\n"  ); break;
                    case CFG_Operation_State_Transmitting:
                    default:                               io_out_string ( "NOPRF\r\n" ); break;
                }
                break;
        case CS_PRFEND:
                if ( CFG_Operation_State_Profiling == CFG_Get_Operation_State() ) {
                    //  End profiling
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Profile_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Prompt );
                } else if ( CFG_Operation_State_Transmitting == CFG_Get_Operation_State() ) {
                    //  End transmitting
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Transmit_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Prompt );
                }
                io_out_string ( "$ACK,PRFEND,OK\r\n" );
                break;
        case CS_TXBEG:
                io_out_string ( "$ACK,TXBEG,OK\r\n" );
                if ( CFG_Operation_State_Transmitting == CFG_Get_Operation_State() ) {
                    //  No need to act
                } else {
                    if ( CFG_Operation_State_Profiling == CFG_Get_Operation_State() ) {
                        //  End profiling
                        packet.to         = DE_Addr_ProfileManager;
                        packet.from       = DE_Addr_ControllerBoard_Commander;
                        packet.type       = DE_Type_Command;
                        packet.data.Command.Code = CMD_PMG_Profile_End;
                        data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                        //  Do not need confirmation
                        //  The CMD_PMG_Transmit packet immediately following
                        //  is the relevant command that needs acknowledgement.
                    }
                    //  Begin transmitting
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Transmit;
                    packet.data.Command.value.s16[0] = option_int;  //  Undocumented test argument
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Transmitting );
                    Transfer_Status = Tx_Sts_Txed_00Percent;
                }
                break;
        case CS_TXSTS:
                io_out_string ( "$ACK,TXSTS," );
                switch ( CFG_Get_Operation_State() ) {
                    case CFG_Operation_State_Transmitting: // io_out_string ( "TX\r\n"   ); break;
                    case CFG_Operation_State_Prompt:       // io_out_string ( "DONE\r\n"  ); break;
                                       switch ( Transfer_Status ) {
                                       case Tx_Sts_Not_Started       : io_out_string ( "NOTX\r\n" ); break;
                                       case Tx_Sts_No_Profile        : io_out_string ( "DONE\r\n"   ); break;
                                       case Tx_Sts_Empty_Profile     : io_out_string ( "DONE\r\n"   ); break;
                                       case Tx_Sts_Package_Fail      : io_out_string ( "DONE\r\n"   ); break;
                                       case Tx_Sts_Modem_Fail        : io_out_string ( "DONE\r\n"   ); break;
                                       case Tx_Sts_Early_Termination : io_out_string ( "DONE\r\n"   ); break;
                                       case Tx_Sts_Txed_00Percent    : io_out_string ( "TX\r\n"   ); break;
                                       case Tx_Sts_Txed_25Percent    : io_out_string ( "TX\r\n"   ); break;
                                       case Tx_Sts_Txed_50Percent    : io_out_string ( "TX\r\n"   ); break;
                                       case Tx_Sts_Txed_75Percent    : io_out_string ( "TX\r\n"   ); break;
                                       case Tx_Sts_AllDone           : io_out_string ( "DONE\r\n"  ); break;
                                       }
                                       break;
                    case CFG_Operation_State_Profiling:
                    default:                               io_out_string ( "NOTX\r\n" ); break;
                }
                break;
        case CS_TXRLT:  //  Expansion to future Navis API
                io_out_string ( "$ACK,TXRLT," );
                switch ( CFG_Get_Operation_State() ) {
                    case CFG_Operation_State_Transmitting:
                    case CFG_Operation_State_Prompt:
                                       switch ( Transfer_Status ) {
                                       case Tx_Sts_Not_Started       : io_out_string ( "NOTX\r\n" ); break;
                                       case Tx_Sts_No_Profile        : io_out_string ( "DONE,1\r\n"   ); break;
                                       case Tx_Sts_Empty_Profile     : io_out_string ( "DONE,2\r\n"   ); break;
                                       case Tx_Sts_Package_Fail      : io_out_string ( "DONE,3\r\n"   ); break;
                                       case Tx_Sts_Modem_Fail        : io_out_string ( "DONE,4\r\n"   ); break;
                                       case Tx_Sts_Early_Termination : io_out_string ( "DONE,5\r\n"   ); break;
                                       case Tx_Sts_Txed_00Percent    : io_out_string ( "TX,0\r\n"   ); break;
                                       case Tx_Sts_Txed_25Percent    : io_out_string ( "TX,25\r\n"   ); break;
                                       case Tx_Sts_Txed_50Percent    : io_out_string ( "TX,50\r\n"   ); break;
                                       case Tx_Sts_Txed_75Percent    : io_out_string ( "TX,75\r\n"   ); break;
                                       case Tx_Sts_AllDone           : io_out_string ( "DONE,0\r\n"  ); break;
                                       }
                                       break;
                    case CFG_Operation_State_Profiling:
                    default:                               io_out_string ( "NOTX\r\n" ); break;
                }
                break;
        case CS_TXEND:
                io_out_string ( "$ACK,TXEND,OK\r\n" );
                if ( CFG_Operation_State_Profiling == CFG_Get_Operation_State() ) {
                    //  End profiling
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Profile_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Prompt );
                } else if ( CFG_Operation_State_Transmitting == CFG_Get_Operation_State() ) {
                    //  End transmitting
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Transmit_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    CFG_Set_Operation_State( CFG_Operation_State_Prompt );
                }
                break;
        case CS_GPS:
                {
                //  parse GPS values
                    double latitude, longitude;
                    if ( strlen(option) > 0
                      && 1 == sscanf ( option, "%lf", &latitude )
                      && strlen(value) > 0
                      && 1 == sscanf ( value, "%lf", &longitude ) ) {

                        io_out_F64    ( "$ACK,GPS,%.6f", latitude );
                        io_out_F64    (         ",%.6f\r\n", longitude );

                        CFG_Set_Latitude  ( latitude  );
                        CFG_Set_Longitude ( longitude );

                        //  TODO -- Save to permanent memory, so these values are accessible to sun direction/shadow calculation
                    } else {
                        io_out_string ( "$NAK,GPS," );
                        io_out_string ( option );
                        io_out_string ( "," );
                        io_out_string ( value );
                        io_out_string ( "\r\n" );
                    }
                }
                break;
      # if 0
        case CS_CTD:
                //  TODO parse CTD values
                io_out_string ( "ACK,CTD\r\n" );
                break;
      # endif
        case CS_TIME:
                {
                //  Parse date/time value - use for clock synchronization
                //  Only update time when at prompt.
                //  During profiling or transmission, do NOT change time!
                uint32_t time;
                if ( CFG_Operation_State_Prompt == CFG_Get_Operation_State()
                  && strlen(option) > 0
                  && 1 == sscanf ( option, "%lu", &time )
                  && time>0 ) {
                    io_print_U32 ( "$ACK,TIME,%lu\r\n", time );
                    //  TODO -- Update clock
                    struct timeval tv;
                    tv.tv_sec = time;
                    tv.tv_usec = 0;
                    settimeofday ( &tv, 0 );
                    time_sys2ext();
                } else {
                    io_out_string ( "$NAK,TIME," );
                    io_out_string ( option );
                    io_out_string ( "\r\n" );

                }
                }
                break;
        case CS_FIRMWARE:
                io_out_string ( "$ACK,FIRMWARE," HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH "\r\n" );
                break;
        case CS_SERIAL:
                {
                U16 const sn = CFG_Get_Serial_Number();
                char sn_as_string[8];
                snprintf ( sn_as_string, sizeof(sn_as_string), "%hu", sn );
                io_out_string ( "$ACK,SERIAL," ); io_out_string ( sn_as_string ); io_out_string ( "\r\n" );
                }
                break;
        case CS_SLP:
                io_out_string ( "$ACK,SLP\r\n" );
                if ( CFG_Operation_State_Profiling == CFG_Get_Operation_State() ) {
                    //  End profiling
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Profile_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    //  TODO -- Wait for confirmation
                } else if ( CFG_Operation_State_Transmitting == CFG_Get_Operation_State() ) {
                    //  End transmitting
                    packet.to         = DE_Addr_ProfileManager;
                    packet.from       = DE_Addr_ControllerBoard_Commander;
                    packet.type       = DE_Type_Command;
                    packet.data.Command.Code = CMD_PMG_Transmit_End;
                    data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
                    //  TODO - wait for confirmation
                }
                //  TODO Some delay
                //  TODO Enter low power (PCB Supervisor) sleep
                //  Execution should never get here!
                //  io_out_string ( "NAK,SLP,FAILED\r\n" );
                break;

        default:
                io_out_string ( "$NAK,NC" );
                if ( option && *option ) {
                    io_out_string ( "," );
                    io_out_string ( option );
                    if ( value && *value ) {
                        io_out_string ( "," );
                        io_out_string ( value );
                    }
                }
                io_out_string ( "\r\n" );
                break;
        }
    }

    return cec;
}
# endif

# if defined(OPERATION_AUTONOMOUS)

//  Set of commands accepted in Streaming Mode:
//  Commands are
//      <command_string> [--option|--option value|--option=value]
typedef enum {
    //  Operation control
    CS_Stream,
    CS_Start,
    CS_Darks,
    CS_Cal,
    CS_Eng,
    CS_Stop,
} StreamingCommandID;

typedef struct {
    char* name;
    StreamingCommandID id;
} StreamingCommand_t;

static StreamingCommand_t streamingCmd[] = {
        //  Streaming operation control
        { "Stream",  CS_Stream },
        { "Start",   CS_Start },
        { "Darks",   CS_Darks },
        { "Cal",     CS_Cal },
        { "Eng",     CS_Eng },
        { "Stop",    CS_Stop }
};

static const S16 MAX_STREAMING_CMDS = sizeof(streamingCmd)/sizeof(StreamingCommand_t);

static S16 STRM_CMD_Handle ( char* cmd /*, cc_expect_t* cc_expect */ ) {

    data_exchange_packet_t packet;

    Bool match = false;
    S16 try = 0;
    char* arg = 0;

    //  Skip leading white space
    while ( *cmd == ' ' || *cmd == '\t' ) cmd++;

    do {
        if ( 0 == strncasecmp ( cmd, streamingCmd[try].name, strlen(streamingCmd[try].name) ) ) {
            match = true;
            arg = cmd + strlen(streamingCmd[try].name);
        } else {
            try++;
        }
    } while ( !match && try < MAX_STREAMING_CMDS );

    S16 cec = CEC_Ok;
    char result[64];
    result[0] = 0;

    if ( try == MAX_STREAMING_CMDS ) {
        if ( *cmd == 0 ) {
            cec = CEC_EmptyCommand;
        } else {
            cec = CEC_UnknownCommand;
        }
    } else {
        //  Split the remainder of the command into
        //  option and (optional) value parts

        //  Skip over spaces and "--" option prefix.
        //  Nothing will be done if this is an option-less command.
        while ( *arg == ' '
             || *arg == '\t'
             || *arg == '-' ) {
            arg++;
        }
        //  Option argument to current command starts now
        char* option = arg;

        //  Skip over option argument characters to see if value string is following
        while ( isalnum ( *arg )
             || *arg == '_'
             || *arg == '*' ) {
            arg++;
        }

        int16_t option_int = (*option)?atoi(option):0;

        char* value;

        if ( 0 == *arg ) {
            //  No value in command string.
            value = "";
        } else {
            //  Skip over white space / assignment character
            while ( *arg == ' '
                 || *arg == '\t'
                 || *arg == '=' ) {
                *arg = 0;   //  Terminate the option string.
                arg++;
            }
            value = arg;    //  Value starts at first non-space/non-assignment next character.
        }

        int16_t value_int = (*value)?atoi(value):0;

        if ( CFG_Get_Message_Level() >= CFG_Message_Level_Trace ) {
            io_out_string ( "CMD:\t" );
            io_out_string ( streamingCmd[try].name );
            io_out_string ( "\t" );
            io_out_string ( option );
            io_out_string ( "\t" );
            io_out_string ( value );
            io_out_string ( "\r\n" );
        }

        //  Make sure firmware will not get stuck while executing command
        gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

        switch ( streamingCmd[try].id ) {
        //  Option control
        case CS_Stream:      
                            packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Start;
                            packet.data.Command.value.s16[0] = 0;
                            packet.data.Command.value.s16[1] = stop
                           s
                              
                              
                              
                              
                              ;  //  Integratin time; 0 means auto adjust
                            packet.data.Command.value.s16[2] = value_int;   //  Dark/Light frequency
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_START;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
                            //  Mark current OP state in the GPLP register.
                            //  If a watchdog reset occurs, can recover.
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_LMD);
                            cec = CEC_Ok;
                            break;
        case CS_Start:      
                            packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Start;
                            packet.data.Command.value.s16[0] = 1;
                            packet.data.Command.value.s16[1] = option_int;  //  Integration time; 0 means auto adjust
                            packet.data.Command.value.s16[2] = value_int;   //  Dark/Light frequency
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_START;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
                            //  Mark current OP state in the GPLP register.
                            //  If a watchdog reset occurs, can recover.
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_LMD);
                            cec = CEC_Ok;
                            break;
        case CS_Cal:
                            packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Start;
                            packet.data.Command.value.s16[0] = 2;
                            packet.data.Command.value.s16[1] = option_int;  //  Integration time; 0 means auto adjust
                            packet.data.Command.value.s16[2] = value_int;   //  Dark/Light frequency
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_START;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
                        //  cec = CEC_Ok;
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_LD);
                            break;
        case CS_Darks:
                            packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Start;
                            packet.data.Command.value.s16[0] = 3;
                            packet.data.Command.value.s16[1] = option_int;  //  Integration time; 0 means scan through all
                            packet.data.Command.value.s16[2] = value_int;   //  run length for each integration time
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_START;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_DRK);
                            cec = CEC_Ok;
                            break;
        case CS_Eng:
                            packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Start;
                            packet.data.Command.value.s16[0] = 4;
                            packet.data.Command.value.s16[1] = option_int;  //  Delay between frames
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_START;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_DRK);
                            cec = CEC_Ok;
                            break;

        case CS_Stop:       packet.to         = DE_Addr_StreamLog;
                            packet.from       = DE_Addr_ControllerBoard_Commander;
                            packet.type       = DE_Type_Command;
                            packet.data.Command.Code = CMD_SLG_Stop;
                            data_exchange_packet_router( DE_Addr_ControllerBoard_Commander, &packet );
# ifdef USE_COMMAND_CONFIRMATION
                            //  Trigger: Expect confirmation
                        //  cc_expect->response = CC_EXPECT_ACK_DAQ_STOP;
                        //  gettimeofday ( &(cc_expect->untilWhen), NULL );
                        //  timeval_add_usec ( &(cc_expect->untilWhen), 2500000 );
# else
                            CFG_Set_Operation_State( CFG_Operation_State_Prompt );
# endif
                            pm_write_gplp ( &AVR32_PM, 0, GPLP_STATE_STOPPED);
                            cec = CEC_Ok;
                            break;

        default:            break;
        }
    }

    if ( CEC_Ok == cec ) {
        //  Output result (if present)
        io_out_string( "$OK " );
        io_out_string( result );
        io_out_string( "\r\n" );
    } else if ( cec == CEC_EmptyCommand ) {
        io_out_string( "\r\n" );
    } else {
        //  Ignore error code
        //  io_out_S32 ( "$Error: %ld\r\n", (S32)cec );
    }

    return cec;
}

# endif

void command_controller_loop( wakeup_t wakeup_reason ) {

  pm_write_gplp ( &AVR32_PM, 0, 0x12481248 );
  pm_write_gplp ( &AVR32_PM, 1, 0x00000000 );

# if 0 //def WATERMARKING
  //  To monitor memory use.
  //  Remove in deployed revision.
  //
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif

  //  During development, make all commands freely accessible.
  //  TODO: For deployment, change to Access_User
  //
  Access_Mode_t access_mode = Access_Factory;

  //  To assist in debugging
  //
  if ( CFG_Get_Message_Level() >= CFG_Message_Level_Debug ) {
    dump_UPG();
    vTaskDelay((portTickType)TASK_DELAY_MS( 1000 ));
    dump_RTC();
    vTaskDelay((portTickType)TASK_DELAY_MS( 1000 ));
    CFG_Print();
  }

# ifdef DEBUG
  //  During development. TODO: Remove in deployment revision. 
  //
  io_out_string ( "Acronym" "\t" "Task-Name" "\r\n" );

  data_exchange_address_t dx_addr;
  for ( dx_addr = DE_Addr_Nobody+1; dx_addr < DE_Addr_Void; dx_addr++ ) {
    io_out_string ( taskAcronym(dx_addr) );
    io_out_string ( "\t" );
    io_out_string ( taskName(dx_addr) );
    io_out_string ( "\r\n" );
  }
  io_out_string ( "\r\n" );
# endif

# if 0
  //  May (!) use this variable for Float Operation, to generate proper ACK responses
  //  TODO: Properly implement / test
  //
  cc_expect_t cc_expect = { .sendBack = 0,
                            .response = CC_EXPECT_Nothing,
                            .untilWhen = { .tv_sec = 0, .tv_usec = 0 }
                          };
# endif

  //  Get all tasks going.
  //
  data_exchange_controller_resumeTask();
# if defined(OPERATION_AUTONOMOUS)
                stream_log_resumeTask();
# endif
# if defined(OPERATION_NAVIS)
           profile_manager_resumeTask();
# endif
      aux_data_acquisition_resumeTask();

  //  Need to know times to have timeouts operating
  //
  struct timeval now;
  gettimeofday( &now, (void*)0 );

  //  Operation mode and operation state determine action after power up.

  CFG_Operation_State previous_operation_state = CFG_Get_Operation_State();

  switch ( previous_operation_state ) {
    case CFG_Operation_State_Prompt:       /*  as expected - no concern - no action  */  break;
    case CFG_Operation_State_Streaming:    io_out_string( "OpState was Streaming\r\n" ); break;
    case CFG_Operation_State_Profiling:    io_out_string( "OpState was Profiling\r\n" ); break;
    case CFG_Operation_State_Transmitting: io_out_string( "OpState was Txmitting\r\n" ); break;
  }

  Bool self_start = false;
  time_t self_start_time = 0;
  time_t self_start_msg_time = 0;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
  switch ( CFG_Get_Operation_Mode() ) {

  case CFG_Operation_Mode_APM:

      //  If this is a wakeup from low power sleep,
      //  done during profiling ascent to conserve power.
      //  then continue autonomous profiling
      if ( wakeup_reason == PWR_EXTRTC_WAKEUP
        && CFG_Operation_State_Profiling == CFG_Get_Operation_State()
       # ifdef TODO
        && Wakeup_Time - Sleep_Time < _30_minutes
       # endif
        ) {
          CFG_Set_Operation_State ( CFG_Operation_State_Profiling );
        } else {
          CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
        }

      //  Cannot start at this point,
      //  because board-to-board configuration exchange
      //  has not happened yet.
      //
      self_start = true;

      break;

  case CFG_Operation_Mode_Continuous:

      CFG_Set_Operation_State ( CFG_Operation_State_Prompt );

      //  Cannot start at this point,
      //  because board-to-board configuration exchange
      //  has not happened yet.
      //
      self_start = true;

      break;

  default:
      //  Fallback: stream output in unknown operation mode
      CFG_Set_Operation_Mode( CFG_Operation_Mode_Continuous );

      CFG_Set_Operation_State ( CFG_Operation_State_Prompt );

      //  Cannot start at this point,
      //  because board-to-board configuration exchange
      //  has not happened yet.
      //
      self_start = true;

      break;
  }
# elif defined(OPERATION_AUTONOMOUS)

      CFG_Set_Operation_State ( CFG_Operation_State_Prompt );

      //  Cannot start at this point,
      //  because board-to-board configuration exchange
      //  has not happened yet.
      //
      self_start = true;

# elif defined(OPERATION_NAVIS)

      //  If this is a wakeup from low power sleep,
      //  done during profiling ascent to conserve power.
      //  then continue autonomous profiling
      if ( wakeup_reason == PWR_EXTRTC_WAKEUP
        && CFG_Operation_State_Profiling == CFG_Get_Operation_State()
       # ifdef TODO
        && Wakeup_Time - Sleep_Time < _30_minutes
       # endif
        ) {
          CFG_Set_Operation_State ( CFG_Operation_State_Profiling );
        } else {
          CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
        }

      //  Cannot start at this point,
      //  because board-to-board configuration exchange
      //  has not happened yet.
      //
      self_start = true;

# endif

  data_exchange_address_t const myAddress = DE_Addr_ControllerBoard_Commander;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
  if ( CFG_Operation_Mode_Continuous == CFG_Get_Operation_Mode() ) {
    io_out_string( "HyperNav> " );
  }
# elif defined(OPERATION_AUTONOMOUS)
    io_out_string( "HyperNav> " );
# elif defined(OPERATION_NAVIS)
    io_out_string( "HNV> " );
# endif

  //  Initially, regardless of operation mode,
  //  ping the spectrometer board until it responds:  SB_STT_Unknown -> SB_STT_Alive
  //  Then, sync time, send configuration parameters & query state until ready: SB_STT_Alive -> SB_STT_Ready
  //  Only after that, start data acquisition.
  //
  enum { SB_STT_Unknown = 0, SB_STT_Alive, SB_STT_Ready } specBoard_state = SB_STT_Unknown;
  time_t next_specBoard_Ping_or_Query = now.tv_sec + 2;

  int input_overwrite = 0;

  while ( 1 ) {

    # if 0 //def WATERMARKING
      //  Report stack water mark, if changed to previous water mark
      //
      stackWaterMark = uxTaskGetStackHighWaterMark(0);

      if ( stackWaterMark < stackWaterMark_threshold ) {
        while ( stackWaterMark < stackWaterMark_threshold ) stackWaterMark_threshold/=2;
        data_exchange_packet_t packet_message;
        packet_message.to   = DE_Addr_ControllerBoard_Commander;
        packet_message.from = myAddress;
        packet_message.type = DE_Type_Syslog_Message;
        packet_message.data.Syslog.number = SL_MSG_StackWaterMark;
        packet_message.data.Syslog.value  = stackWaterMark;
        data_exchange_packet_router ( myAddress, &packet_message );
      }
    # endif

      // FIXME
      //THIS_TASK_IS_RUNNING(gHNV_SetupCmdCtrlTask_Status);
      gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

      data_exchange_packet_t packet_rx_via_queue;
      Bool gotPacket = false;

      while ( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) ) {

        gotPacket = true;
        // io_out_string ( "DBG SCC RX packet\r\n" );

        if ( packet_rx_via_queue.to != myAddress ) {

          //  This is not supposed to happen
          //
          syslog_out( SYSLOG_ERROR, "CommandController",
                          "Received misdirected packet type %d from %s.",
                          packet_rx_via_queue.type,
                          taskAcronym(packet_rx_via_queue.from) );

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
          //  A non DE_Addr_Nobody value will be used as a flag
          //  to send the packet_response.
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
              packet_response.data.Response.Code = RSP_SLG_Query;
              packet_response.data.Response.value.u64 = CFG_Get_Operation_State();
              break;

            case CMD_CMC_StartSpecBoard:

              io_out_string( "CMC <- StartSpecBoard\r\n" );
              // FIXME -- Stop all operation
              specBoard_state = SB_STT_Unknown;
              break;

            case CMD_CMC_Transfer_Status:

              switch ( packet_rx_via_queue.data.Command.value.s16[0] ) {
              case Tx_Sts_Not_Started       : /*    Cannot happen    */                                                                           break;
              case Tx_Sts_No_Profile        : Transfer_Status = Tx_Sts_No_Profile;        CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              case Tx_Sts_Empty_Profile     : Transfer_Status = Tx_Sts_Empty_Profile;     CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              case Tx_Sts_Package_Fail      : Transfer_Status = Tx_Sts_Package_Fail;      CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              case Tx_Sts_Modem_Fail        : Transfer_Status = Tx_Sts_Modem_Fail;        CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              case Tx_Sts_Early_Termination : Transfer_Status = Tx_Sts_Early_Termination; CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              case Tx_Sts_Txed_00Percent    : Transfer_Status = Tx_Sts_Txed_00Percent;    /* continue transmitting... */                          break;
              case Tx_Sts_Txed_25Percent    : Transfer_Status = Tx_Sts_Txed_25Percent;    /* continue transmitting... */                          break;
              case Tx_Sts_Txed_50Percent    : Transfer_Status = Tx_Sts_Txed_50Percent;    /* continue transmitting... */                          break;
              case Tx_Sts_Txed_75Percent    : Transfer_Status = Tx_Sts_Txed_75Percent;    /* continue transmitting... */                          break;
              case Tx_Sts_AllDone           : Transfer_Status = Tx_Sts_AllDone;           CFG_Set_Operation_State ( CFG_Operation_State_Prompt ); break;
              }

              break;

            default:
              snprintf ( local_tmp_str, sizeof(local_tmp_str),
                       "\r\n%s <- %s Command[ %d %llu ]",
                       taskAcronym(myAddress),
                       taskAcronym(packet_rx_via_queue.from),
                       packet_rx_via_queue.data.Command.Code,
                       packet_rx_via_queue.data.Command.value.u64 );
              io_out_string( local_tmp_str );
            }

            break;

          case DE_Type_Response:

# ifdef USE_COMMAND_CONFIRMATION
            if ( cc_expect.response != CC_EXPECT_Nothing ) { // Expecting a response to generate ACK response to float

              S8 got_expected_response = 0;

              switch ( cc_expect.response ) {
# if 0
              case CC_EXPECT_ACK_DAQ_START: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                              && RSP_PMG_Profiling_Begun == packet_rx_via_queue.data.Response.Code ) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Profiling );
                                            }
                                          }
                                          break;
              case CC_EXPECT_ACK_DAQ_STOP: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                              && RSP_PMG_Profiling_Ended == packet_rx_via_queue.data.Response.Code ) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
                                            }
                                          }
                                          break;
              case CC_EXPECT_ACK_PRF_BEG: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                              && RSP_PMG_Profiling_Begun == packet_rx_via_queue.data.Response.Code ) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Profiling );
                                            }
                                          }
                                          break;
              case CC_EXPECT_ACK_PRF_END: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                            && RSP_PMG_Profiling_Ended == packet_rx_via_queue.data.Response.Code ) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
                                            }
                                          }
                                          break;
              case CC_EXPECT_ACK_TRX_BEG: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                            && RSP_PMG_Transmitting_Begun == packet_rx_via_queue.data.Response.Code) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Transmitting );
                                            }
                                          }
                                          break;
              case CC_EXPECT_ACK_TRX_END: if ( DE_Addr_ProfileManager == packet_rx_via_queue.from
                                            && RSP_PMG_Transmitting_Ended == packet_rx_via_queue.data.Response.Code) {
                                            got_expected_response = 1;
                                            if ( packet_rx_via_queue.data.Response.value.rsp == RSP_ANY_ACK ) {
                                              CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
                                            }
                                          }
                                          break;
# endif
              case CC_EXPECT_Nothing:     //  Never reached. Added to remove compiler warning
                                          break;
              }

              if ( got_expected_response ) {

                io_out_string ( "ACK," );
                io_out_string ( cc_expect.sendBack );
                cc_expect.sendBack = 0;
                cc_expect.response = CC_EXPECT_Nothing;

              } else {

                snprintf ( local_tmp_str, sizeof(local_tmp_str),
                           "\r\n%s <- %s Mismatched Response[ %d %llx ]",
                           taskAcronym(myAddress),
                           taskAcronym(packet_rx_via_queue.from),
                           packet_rx_via_queue.data.Response.Code,
                           packet_rx_via_queue.data.Response.value.u64 );
                io_out_string( local_tmp_str );

                //  FIXME!
                CFG_Set_Operation_State ( CFG_Operation_State_Prompt );
              }

            } else
# endif
            {
              switch ( packet_rx_via_queue.data.Response.Code ) {

              case RSP_ALL_Ping:

                //  Was waiting for a response to the ping sent to the spectrometer board.
                //
                if ( SB_STT_Unknown == specBoard_state
                  && packet_rx_via_queue.from == DE_Addr_SpectrometerBoard_Commander ) {

                  // io_out_string ( "CMC <-PING<- CMS\r\n" );

                  //  As soon as the spectrometer board is available,
                  //  send it the configuration parameters.
                  //
                  if ( pdTRUE == xSemaphoreTake( sending_data_package.mutex, portMAX_DELAY ) ) {
                    if ( EmptyRAM == sending_data_package.state ) {

                      data_exchange_packet_t packet;

                      packet.from = myAddress;
                      packet.to   = DE_Addr_SpectrometerBoard_Commander;

                      //  Synchronize the time between the boards
                      //
                      packet.type = DE_Type_Command;
                      packet.data.Command.Code  = CMD_CMS_SyncTime;
                      gettimeofday( &now, (void*)0 );
                      packet.data.Command.value.u32[0] = now.tv_sec;
                      data_exchange_packet_router ( myAddress, &packet );

                      //  Send the configuration parameters
                      //
                      Config_Data_t* cd = (Config_Data_t*) sending_data_package.address;

                      cd -> content = CD_Full;

                      cd -> id           = CFG_Get_Power_Cycle_Counter();

                      cd -> acc_mounting = CFG_Get_Accelerometer_Mounting();
                      cd -> acc_x        = CFG_Get_Accelerometer_Vertical_x();
                      cd -> acc_y        = CFG_Get_Accelerometer_Vertical_y();
                      cd -> acc_z        = CFG_Get_Accelerometer_Vertical_z();

                      cd -> mag_min_x    = CFG_Get_Magnetometer_Min_x();
                      cd -> mag_max_x    = CFG_Get_Magnetometer_Max_x();
                      cd -> mag_min_y    = CFG_Get_Magnetometer_Min_y();
                      cd -> mag_max_y    = CFG_Get_Magnetometer_Max_y();
                      cd -> mag_min_z    = CFG_Get_Magnetometer_Min_z();
                      cd -> mag_max_z    = CFG_Get_Magnetometer_Max_z();

                      cd -> gps_lati     = CFG_Get_Latitude();
                      cd -> gps_long     = CFG_Get_Longitude();
                      cd -> magdecli     = CFG_Get_Magnetic_Declination();

                      cd -> digiqzu0     = CFG_Get_Digiquartz_U0();
                      cd -> digiqzy1     = CFG_Get_Digiquartz_Y1();
                      cd -> digiqzy2     = CFG_Get_Digiquartz_Y2();
                      cd -> digiqzy3     = CFG_Get_Digiquartz_Y3();
                      cd -> digiqzc1     = CFG_Get_Digiquartz_C1();
                      cd -> digiqzc2     = CFG_Get_Digiquartz_C2();
                      cd -> digiqzc3     = CFG_Get_Digiquartz_C3();
                      cd -> digiqzd1     = CFG_Get_Digiquartz_D1();
                      cd -> digiqzd2     = CFG_Get_Digiquartz_D2();
                      cd -> digiqzt1     = CFG_Get_Digiquartz_T1();
                      cd -> digiqzt2     = CFG_Get_Digiquartz_T2();
                      cd -> digiqzt3     = CFG_Get_Digiquartz_T3();
                      cd -> digiqzt4     = CFG_Get_Digiquartz_T4();
                      cd -> digiqzt5     = CFG_Get_Digiquartz_T5();

                      cd -> dqtempdv     = CFG_Get_Digiquartz_Temperature_Divisor();
                      cd -> dqpresdv     = CFG_Get_Digiquartz_Pressure_Divisor();

                      cd -> simdepth     = CFG_Get_Simulated_Start_Depth();
                      cd -> simascnt     = CFG_Get_Sumulated_Ascent_Rate();

                      cd -> puppival     = CFG_Get_Profile_Upper_Interval();
                      cd -> puppstrt     = CFG_Get_Profile_Upper_Start();
                      cd -> pmidival     = CFG_Get_Profile_Middle_Interval();
                      cd -> pmidstrt     = CFG_Get_Profile_Middle_Start();
                      cd -> plowival     = CFG_Get_Profile_Lower_Interval();
                      cd -> plowstrt     = CFG_Get_Profile_Lower_Start();

                      cd -> frmprtsn     = CFG_Get_Frame_Port_Serial_Number();
                      cd -> frmsbdsn     = CFG_Get_Frame_Starboard_Serial_Number();

                      cd -> saturcnt     = CFG_Get_Saturation_Counts();
                      cd -> numclear     = CFG_Get_Number_of_Clearouts();

                      sending_data_package.state = FullRAM;

                      packet.type = DE_Type_Configuration_Data;
                      packet.data.DataPackagePointer = &sending_data_package;
                      data_exchange_packet_router( myAddress, &packet );

                      //  Query the spectrometer board
                      //
                      packet.type = DE_Type_Command;
                      packet.data.Command.Code = CMD_ANY_Query;
                      data_exchange_packet_router( myAddress, &packet );

                    //io_out_S16 ( "CMC TX ACC XYZ %hd ", cd->acc_x );
                    //io_out_S16 (        " %hd ", cd->acc_y );
                    //io_out_S16 (        " %hd ", cd->acc_z );
                    //io_out_S32 (       " ID %ld /QU CMS\r\n", cd->id );
                    }

                    xSemaphoreGive( sending_data_package.mutex );
                  }

                  specBoard_state = SB_STT_Alive;


                } else {
# if 0
                  snprintf ( local_tmp_str, sizeof(local_tmp_str),
                           "\r\n%s <- %s PING Response[ %u %llx ]",
                           taskAcronym(myAddress),
                           taskAcronym(packet_rx_via_queue.from),
                           packet_rx_via_queue.data.Response.Code,
                           packet_rx_via_queue.data.Response.value.u64 );
                  io_out_string( local_tmp_str );
# endif
                }

                break;

              case RSP_CMS_Query:

                //  Was waiting for a response to the query sent to the spectrometer board.
                //
                if ( ( SB_STT_Unknown == specBoard_state || SB_STT_Alive == specBoard_state )
                  && packet_rx_via_queue.from == DE_Addr_SpectrometerBoard_Commander ) {

                  if ( packet_rx_via_queue.data.Response.value.u32[0] == CFG_Get_Power_Cycle_Counter()
                    && packet_rx_via_queue.data.Response.value.u32[1]  > 0 ) {

                    specBoard_state = SB_STT_Ready;

                    self_start = true;
                    gettimeofday( &now, (void*)0 );
                    self_start_msg_time = now.tv_sec;
                    self_start_time = now.tv_sec + CFG_Get_Countdown();
                  }
                } else {
                  snprintf ( local_tmp_str, sizeof(local_tmp_str),
                           "\r\n%s <- %s Query Response[ %u %llx ]",
                           taskAcronym(myAddress),
                           taskAcronym(packet_rx_via_queue.from),
                           packet_rx_via_queue.data.Response.Code,
                           packet_rx_via_queue.data.Response.value.u64 );
                  io_out_string( local_tmp_str );
                }
                break;

              case RSP_ADQ_Query:
              case RSP_DXC_Query:
              case RSP_PMG_Query:
              case RSP_SLG_Query:
              case RSP_DAQ_Query:
              case RSP_DXS_Query:
              case RSP_PPR_Query:
                snprintf ( local_tmp_str, sizeof(local_tmp_str),
                           "\r\n%s <- %s Query Response[ %u %llx ]",
                           taskAcronym(myAddress),
                           taskAcronym(packet_rx_via_queue.from),
                           packet_rx_via_queue.data.Response.Code,
                           packet_rx_via_queue.data.Response.value.u64 );
                io_out_string( local_tmp_str );
                break;
              default:
                // TODO SYSLOG unexpected responses instread of simply outputting
                snprintf ( local_tmp_str, sizeof(local_tmp_str),
                           "\r\n%s <- %s Unexpected Response[ %u %llx ]",
                           taskAcronym(myAddress),
                           taskAcronym(packet_rx_via_queue.from),
                           packet_rx_via_queue.data.Response.Code,
                           packet_rx_via_queue.data.Response.value.u64 );
                io_out_string( local_tmp_str );
              }
            }

            break;

          case DE_Type_Syslog_Message:

            if ( CFG_Get_Message_Level() >= CFG_Message_Level_Warn ) {
                snprintf ( local_tmp_str, sizeof(local_tmp_str),
                       "\r\n%s <- %s Syslog[ %s %llu ]",
                       taskAcronym(myAddress),
                       taskAcronym(packet_rx_via_queue.from),
                       syslogAcronym(packet_rx_via_queue.data.Syslog.number),
                       packet_rx_via_queue.data.Syslog.value );
                io_out_string( local_tmp_str );
            }

            break;

          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            //  Ignoring - not expecting data!
            if ( CFG_Get_Message_Level() >= CFG_Message_Level_Error ) {
                snprintf ( local_tmp_str, sizeof(local_tmp_str),
                       "\r\n%s <- %s Unexpected packet type %d",
                       taskAcronym(myAddress),
                       taskAcronym(packet_rx_via_queue.from),
                       packet_rx_via_queue.type );
                io_out_string( local_tmp_str );
            }

            //  This packet contains a data pointer.
            //  Declare it empty (throw away the data).

            if ( pdTRUE == xSemaphoreTake( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {

              if ( FullRAM == packet_rx_via_queue.data.DataPackagePointer->state ) packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              if ( FullSRAM == packet_rx_via_queue.data.DataPackagePointer->state ) packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;

              xSemaphoreGive( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }

            //  TODO: Log an error message

            break;

          //  Do NOT add a "default:" here.
          //  That way, the compiler will generate a warning
          //  if one possible packet type was missed.

          }

          //  Do not pass received packet anywhere

          //  Send back the response packet to give data pointer back to sender
          if ( packet_response.to != DE_Addr_Nobody ) {
            data_exchange_packet_router ( myAddress, &packet_response );
          }
        }

# if 0
        //  Check if an expected acknowledgment has timed out.
        //  If so, send back a NAK, and clear the expectation.
        if ( cc_expect.response != CC_EXPECT_Nothing ) {
          if ( timeval_compare_to_now ( &(cc_expect.untilWhen) ) > 0 ) {
            io_out_string ( "NAK," );
            io_out_string ( cc_expect.sendBack );
            cc_expect.sendBack = 0;
            cc_expect.response = CC_EXPECT_Nothing;
          }
        }
# endif
      }

      if ( SB_STT_Unknown == specBoard_state ) {

        gettimeofday( &now, (void*)0 );
        if ( now.tv_sec > next_specBoard_Ping_or_Query ) {
            data_exchange_packet_t packet;

            packet.to         = DE_Addr_SpectrometerBoard_Commander;
            packet.from       = myAddress;
            packet.type       = DE_Type_Ping;
            data_exchange_packet_router( myAddress, &packet );
            next_specBoard_Ping_or_Query = now.tv_sec + 2;
          //io_out_string ( "CMC pinging CMS\r\n" );
        }

      } else if ( SB_STT_Alive == specBoard_state ) {

        gettimeofday( &now, (void*)0 );
        if ( now.tv_sec > next_specBoard_Ping_or_Query ) {

            //  As soon as the spectrometer board is available,
            //  send it the configuration parameters.
            //
            if ( pdTRUE == xSemaphoreTake( sending_data_package.mutex, portMAX_DELAY ) ) {
              if ( EmptyRAM == sending_data_package.state ) {

                data_exchange_packet_t packet;

                packet.from = myAddress;
                packet.to   = DE_Addr_SpectrometerBoard_Commander;

                //  Synchronize the time between the boards
                //
                packet.type = DE_Type_Command;
                packet.data.Command.Code  = CMD_CMS_SyncTime;
                gettimeofday( &now, (void*)0 );
                packet.data.Command.value.u32[0] = now.tv_sec;
                data_exchange_packet_router ( myAddress, &packet );

                //  Send the configuration parameters
                //
                Config_Data_t* cd = (Config_Data_t*) sending_data_package.address;

                cd -> content = CD_Full;

                cd -> id = CFG_Get_Power_Cycle_Counter();

                cd -> acc_x = 0;
                cd -> acc_y = 0;
                cd -> acc_z = 0;

                cd -> mag_min_x = CFG_Get_Magnetometer_Min_x();
                cd -> mag_max_x = CFG_Get_Magnetometer_Max_x();
                cd -> mag_min_y = CFG_Get_Magnetometer_Min_y();
                cd -> mag_max_y = CFG_Get_Magnetometer_Max_y();
                cd -> mag_min_z = CFG_Get_Magnetometer_Min_z();
                cd -> mag_max_z = CFG_Get_Magnetometer_Max_z();

                sending_data_package.state = FullRAM;

                packet.type = DE_Type_Configuration_Data;
                packet.data.DataPackagePointer = &sending_data_package;
                data_exchange_packet_router( myAddress, &packet );

                //  Query the spectrometer board
                //
                packet.type = DE_Type_Command;
                packet.data.Command.Code = CMD_ANY_Query;
                data_exchange_packet_router( myAddress, &packet );

              //io_out_S32 ( "CMC ST/CD %ld /QU CMS\r\n", cd->id );
              }

              xSemaphoreGive( sending_data_package.mutex );
            }

            next_specBoard_Ping_or_Query = now.tv_sec + 5;
        }

      } else if ( SB_STT_Ready == specBoard_state ) {

        if ( previous_operation_state != CFG_Operation_State_Prompt ) {

          //  TODO --- Continue aborted operation
          //
        //io_out_string ( "TODO - Continue aborted operation\r\n" );
          previous_operation_state = CFG_Operation_State_Prompt;

          //  Cross-check Operation Mode
          //

        } else if ( self_start ) {

          gettimeofday( &now, (void*)0 );
          if ( now.tv_sec > self_start_time ) {

            data_exchange_packet_t packet;

            packet.from       = myAddress;
            packet.type       = DE_Type_Command;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
            if ( CFG_Operation_Mode_Continuous == CFG_Get_Operation_Mode() ) {
# endif
# if defined(OPERATION_AUTONOMOUS)
              packet.to       = DE_Addr_StreamLog;
              packet.data.Command.Code = CMD_SLG_Start;
              packet.data.Command.value.s16[0] = 1;   //  L&D data acquisition
              packet.data.Command.value.s16[1] = 0;   //  Integration time; 0 means auto adjust
              packet.data.Command.value.s16[2] = 0;   //  run length for each integration time; 0 means default
              CFG_Set_Operation_State( CFG_Operation_State_Streaming );
# endif
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
            } else {
# endif
# if defined(OPERATION_NAVIS)
              packet.to       = DE_Addr_ProfileManager;
              packet.data.Command.Code = CMD_PMG_Profile;
              packet.data.Command.value.s64 = 6;
              CFG_Set_Operation_State( CFG_Operation_State_Profiling );
# endif
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
            }
# endif

            data_exchange_packet_router( myAddress, &packet );

            self_start = false;

          } else {
           if ( now.tv_sec > self_start_msg_time ) {
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
            if ( CFG_Operation_Mode_Continuous == CFG_Get_Operation_Mode() ) {
                io_out_S32 ( "Start data acquisition in %ld seconds\r\n", self_start_time-now.tv_sec );
            }
# elif defined(OPERATION_AUTONOMOUS)
                io_out_S32 ( "Start data acquisition in %ld seconds\r\n", self_start_time-now.tv_sec );
# endif
            self_start_msg_time = now.tv_sec + 1;
           }
          }
        }

      }

      Bool gotInput = false;

      //  Prioritize input handling:
      //  If a packet was just received, check again for a packet.
      //  Only if no packet received, check for serial input. 
      //
      char c;
      if ( !gotPacket && tlm_recv ( &c, 1, TLM_PEEK | TLM_NONBLOCK )
# if 0
                      && cc_expect.response == CC_EXPECT_Nothing
# endif
         ) {

        //  Handle input via RS-232
        //  only if not waiting for completion/confirmation of previous command
        char cmd[64];

        //    TODO    Adjust timeouts to deployment requirements
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
        U16     timeout = ( CFG_Operation_Mode_APM == CFG_Get_Operation_Mode() )
                        ?    5
                        :   15;  //  seconds
        U16 idleTimeout = ( CFG_Operation_Mode_APM == CFG_Get_Operation_Mode() )
                        ? 3000
                        : 5000;  //  milliseconds
# elif defined(OPERATION_AUTONOMOUS)
        U16     timeout = 15;    //  seconds
        U16 idleTimeout = 5000;  //  milliseconds
# elif defined(OPERATION_NAVIS)
        U16     timeout = 5;     //  seconds
        U16 idleTimeout = 3000;  //  milliseconds
# endif

        S16 const inLength = io_in_getstring ( cmd, sizeof(cmd), timeout, idleTimeout );

        if ( inLength > 0 ) {

          gotInput = true;

          self_start = false;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
          if ( CFG_Operation_Mode_APM == CFG_Get_Operation_Mode() ) {
# endif

# if defined(OPERATION_NAVIS)
            if ( specBoard_state == SB_STT_Ready || input_overwrite ) { //  Do not accept APM Commands until spec board is ready - Assert readyness via "W" -> "ACK,W"
              if ( CEC_UnknownCommand == APM_CMD_Handle ( cmd ) ) {
                // If command was not a valid APM command,
                // and the sensor is at the prompt (i.e., not profiling or transmitting)
                // try for generic commands
                if ( CFG_Operation_State_Prompt == CFG_Get_Operation_State() ) {
                    if ( input_overwrite ) {
                        int ui_flag = 0;
                        CMD_Handle ( cmd, &access_mode, &ui_flag );
                        if ( ui_flag & 0x01 ) { io_out_string( "30/15000\r\n" ); timeout = 30; idleTimeout = 15000; }
                    }
                }
              }

              io_out_string( "HNV> " );

            } else {
                if ( 'w' == cmd[0] || 'W' == cmd[0] ) {
                    io_out_string ( "$NAK,W\r\n" );
                }
                if ( '@' == cmd[0] ) input_overwrite = 1;
            }

            if ( '$' == cmd[0] ) input_overwrite = 1;

# endif

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
          } else if ( CFG_Operation_Mode_Continuous == CFG_Get_Operation_Mode() ) {
# endif

# if defined(OPERATION_AUTONOMOUS)
            if ( specBoard_state == SB_STT_Ready || input_overwrite ) { //  Do not accept Streaming Commands until spec board is ready
              if ( CEC_UnknownCommand == STRM_CMD_Handle ( cmd /*, &cc_expect */ ) ) {
                // If command was not valid streaming command,
                // and the sensor is at the prompt (i.e., not streaming)
                // try for generic commands
                if ( CFG_Operation_State_Prompt == CFG_Get_Operation_State() ) {
                    int ui_flag = 0;
                    CMD_Handle ( cmd, &access_mode, &ui_flag );
                    if ( ui_flag & 0x01 ) { timeout = 30; idleTimeout = 15000; }
                } else {
                  io_out_S32 ( "$Error: %ld\r\n", (S32)CEC_CannotProcess );
                }
              }
            } else {
                if ( '@' == cmd[0] ) input_overwrite = 1;
            }

            switch ( specBoard_state ) {
            case SB_STT_Unknown:  io_out_string ( "SpecBrd Sync  -- " "HyperNav> " ); break;
            case SB_STT_Alive:    io_out_string ( "SpecBrd Setup -- " "HyperNav> " ); break;
            case SB_STT_Ready:    //  No output is good output
            default:              io_out_string (                     "HyperNav> " ); break;
            }
# endif

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
          } else {
            //  Impossible: Only 2 operation modes defined.
            //  Fallback in case of damage to internal memory.
            int ui_flag = 0;
            CMD_Handle ( cmd, &access_mode, &ui_flag );
            if ( ui_flag & 0x01 ) { timeout = 30; idleTimeout = 15000; }
            io_out_string( "HyperNav> " );
          }
# endif

        } else if ( inLength == 0 ) {
            //  Empty string input
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
          if ( CFG_Operation_Mode_Continuous == CFG_Get_Operation_Mode() ) {
            switch ( specBoard_state ) {
            case SB_STT_Unknown:  io_out_string ( "SpecBrd Sync  -- " "HyperNav> " ); break;
            case SB_STT_Alive:    io_out_string ( "SpecBrd Setup -- " "HyperNav> " ); break;
            case SB_STT_Ready:    //  No output is good output
            default:              io_out_string (                     "HyperNav> " ); break;
            }
          }
# elif defined(OPERATION_AUTONOMOUS)
            switch ( specBoard_state ) {
            case SB_STT_Unknown:  io_out_string ( "SpecBrd Sync  -- " "HyperNav> " ); break;
            case SB_STT_Alive:    io_out_string ( "SpecBrd Setup -- " "HyperNav> " ); break;
            case SB_STT_Ready:    //  No output is good output
            default:              io_out_string (                     "HyperNav> " ); break;
            }
# else
            io_out_string( "HNV> " );
# endif
        } else {
          //  No input withing timeout
          //  --> Do not output prompt
        }
      }

      // Sleep
      gHNV_SetupCmdCtrlTask_Status = TASK_SLEEPING;
      if ( gotPacket || gotInput ) {
        // Only very brief delay if input is handled
        vTaskDelay((portTickType)TASK_DELAY_MS(5));
      } else {
        vTaskDelay((portTickType)TASK_DELAY_MS(HNV_SetupCmdCtrlTask_PERIOD_MS));
      }
      gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;
  }

}

static void emergencyMenuEntry() {

    char dummy;
    if ( tlm_recv ( &dummy, 1, TLM_PEEK | TLM_NONBLOCK ) > 0 ) {
        tlm_recv ( &dummy, 1, TLM_NONBLOCK );
        if ( 'X' == dummy /* || '$' == dummy */ ) {
            if ( tlm_recv ( &dummy, 1, TLM_PEEK | TLM_NONBLOCK ) > 0 ) {
                tlm_recv ( &dummy, 1, TLM_NONBLOCK );
                if ( 'X' == dummy /* || '$' == dummy */ ) {

                   io_out_string( "EmergencyBootloader()\r\n" );
                   CMD_DropToBootloader();
                   hnv_sys_spec_board_stop();
                   command_controller_reboot();

                }
            }
        }
    }
}

//*************************************************************************
//! \brief  StartController()  Setup+Command task
//*************************************************************************
static void StartController( __attribute__((unused)) void *pvParameters_notUsed ) {

    /*  Initialize system  */

    char* upMsg = 0;

    {
    hnv_sys_ctrl_param_t initParameter;
    initParameter.tlm_baudrate = TLM_DEFAULT_USART_BAUDRATE;
    initParameter.mdm_baudrate = 19200; // MDM_USART_BAUDRATE;
    hnv_sys_ctrl_status_t initStatus;

    avr32rerrno = 0;

    if ( HNV_SYS_CTRL_FAIL == hnv_sys_ctrl_init( &initParameter, &initStatus ) ) {

        if ( !initStatus.file ) {
            //  One reason for this failure may be that this is the very first execution
            //  after assembly, where the file systems have not been formatted.
            //  This forces entry into setup_system().
        } else {
            upMsg = "System failed to initialize.";
            goto sys_failed;
        }
    }
    }

    io_out_string ( "\r\n\r\n" );

    //  Should this be inside hnv_sys_ctrl_init()? - BP 2016-07-21
    {
        FATFS Fatfs;
        FATFs_f_mount ( EMMC_DRIVE_CHAR-'0', &Fatfs );
    }

    //-- Initialize SPI Bus 1 Master ---
    // Moved here from further up.
    // BP 2016-July-21
    initSPI1(FOSC0);
    initSPI0(FOSC0);

#if 0
    if(!gpio_get_pin_value(PWR_PWRGOOD_PIN) && Is_usb_vbus_low()) {     // additional check, shouldn't really be necessary
        io_out_string ( "Low power shutdown." );
        pwr_shutdown();
    }
#endif

    //  The task controller handles all watchdog related issues
    taskMonitor_controller_startTask();

//  gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;
    gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;


    //  We want to see startup errors related to RTC/CFG,
    //  settings will kick in as soon as CFG is retrieved.
    syslog_setVerbosity ( SYSLOG_NOTICE );
    syslog_enableOut ( SYSOUT_TLM );
    syslog_disableOut( SYSOUT_FILE );   //  Write to file only after supercaps are charged
    syslog_disableOut( SYSOUT_JTAG );

    S16 cfg_oor;

    if ( CFG_FAIL == CFG_Retrieve( false ) ) {

        //  This indicates the User Page or RTC is not working.
        //  Serious problem!
        syslog_out ( SYSLOG_ERROR, "StartController", "Missing sensor configuration." );
        cfg_oor = 999;  //  Worst case.
    } else {
        //  Check if CFG is sane.
        //  If no, there are a few possibilities:
        //  (1) First time up - need to run setup - most out-of-range
        //  (2) First time up setup failed, repeat.
        //      Both (1) and (2) can also be seen by absence of backup.
        //  (3) RTC battery was swapped or user page corrupted
        //      copy backup to RTC & user page

        cfg_oor = CFG_OutOfRangeCounter( false, false );
    }

# if 0
    if ( 0 != cfg_oor ) {

        CFG_Print();
        syslog_out ( SYSLOG_ERROR, "StartController", "Configuration %d out of range.", cfg_oor );

        //  Primary configuration was not ok.
        //  Retrieve backup; then use the one that is better.
        if ( CFG_OK == CFG_Retrieve( true ) ) {

            if ( cfg_oor > CFG_OutOfRangeCounter( false, false ) ) {
                //  Backup is better than primary configuration.
                CFG_Print();
                cfg_oor = CFG_OutOfRangeCounter( false, false );
                syslog_out ( SYSLOG_NOTICE, "StartController", "Using backup configuration %d.", cfg_oor );
                //  Write Backup to Primary CFG.
                CFG_Save( false );
            } else {
                //  Backup is not better than primary configuration.
                //  Use primary.
                CFG_Retrieve( false );
            }
        } else {
            CFG_Retrieve( false );
            cfg_oor = CFG_OutOfRangeCounter( false, false );
            syslog_out ( SYSLOG_ERROR, "StartController", "No backup configuration." );
        }
    }

    if ( 999 == cfg_oor ) {

        //  This is not an issue of a few out-of-range values
        //  after a firmware upgrade introduced new configuration
        //  parameters.
        //  Force setup.

    } else if ( cfg_oor > 0 ) {

        //  If only a few are out-of-range,
        //  suspect we have a firmware upgrade
        //  with a few new configuration parameters.
        //  Set out-of-range values to default, and write to RTC and User Page and backup.
        //  Setup will only be executed if explicitly requested (CFG_Configuration_Status)
        //  or if setting to default does not resolve all range-restricted configuration parameters.

        CFG_OutOfRangeCounter( true, false );
        cfg_oor = CFG_OutOfRangeCounter( false, false );
        //  Save improved configuration for future use.
        CFG_Save( false );
        //  Do not save to backup unless all is ok.
        if ( 0==cfg_oor ) CFG_Save( true );
        syslog_out( SYSLOG_ERROR, "StartController", "Repair configuration %d.", cfg_oor );
    }
# endif

    //  Allow emergency entry into Menu
    emergencyMenuEntry();

# if 0
    if ( !initStatus.file   //  Cover the case of a non-formatted file system
      || 0 != cfg_oor
      || CFG_Configuration_Status_Done != CFG_Get_Configuration_Status()
      || CFG_Get_Firmware_Upgrade() == CFG_Firmware_Upgrade_Yes ) {

        if ( SETUP_OK == setup_system() ) {
            //  Got here after timeout.
            //  This means that problems may persist:
            //  (1) CFG is out-of-range (i.e., has wrong values)
            //  (2) There was no CFG Backup
            //  Normally, the (1) and (2) combined means that
            //  the system has not been setup yet.
            //  But, an operator should have intercepted
            //  a timeout loop in setup_system().
            //  The timeout, however, occurred.
            //  Assume that errors (1) and (2) are spurious,
            //  and operate in default configuration mode
        }
    }
# endif

    //  Allow emergency entry into Menu
    emergencyMenuEntry();

    {
    // Configure power subsystem
    powerConfig_t powerConfig;
    powerConfig.hasSupercaps = false; //(CFG_SuperCaps_Available == CFG_Get_SuperCaps());
    powerConfig.hasPCBSupervisor = (CFG_PCB_Supervisor_Available == CFG_Get_PCB_Supervisor());
    pwr_setConfig(powerConfig);
    //
    }

    CFG_Set_System_Reset_Time( time ( (time_t*)0 ) );

    //  Set watchdog timeout value.
    watchdog_enable ( CFG_Get_Watchdog_Duration() );

# if 0
    if ( TLM_DEFAULT_USART_BAUDRATE != CFG_Get_Baud_Rate_AsValue() ) {
        //  There was garbled output when calling tlm_setBaudrate()
        //  Inserting a delay fixed the problem.
        //  Not really understood what happened.
        vTaskDelay((portTickType)TASK_DELAY_MS( 50 ));
        tlm_setBaudrate( CFG_Get_Baud_Rate_AsValue() );
        vTaskDelay((portTickType)TASK_DELAY_MS( 50 ));
    }
# endif

//  struct timeval now;
//  now.tv_sec = (46L*365L+12L)*24L*60L*60L;
//  now.tv_usec = 0;
//  settimeofday( &now, (void*)0 );

#if defined(ENABLE_USB)
    //  Start USB if available
    if ( 1 /* CFG_USB_Switch_Available == CFG_Get_USB_Switch() */ ) {
        if ( HNV_SYS_CTRL_FAIL == hnv_sys_ctrl_startUSB() ) {
                syslog_out( SYSLOG_ERROR, "StartController", "Could not start USB" );
# if 0 //  No switching of telemetry mode!
        } else {   // Create telemetry mode switcher task
            if( pdPASS != xTaskCreate(vTlmModeTask,
                    configTSK_TLM_MODE_NAME,
                    configTSK_TLM_MODE_STACK_SIZE, NULL,
                    configTSK_TLM_MODE_PRIORITY, &TLMModeTaskHandler))
                syslog_out( SYSLOG_ERROR, "StartController", "Telemetry mode switcher could not start");

            //  FIXME:  Set USB Mass Storage Thread to lowest priority.
            //          USB Mass Storage only to be active in command_controller_loop().
# endif
        }
    }
#endif

    //  Setup syslog
    switch ( CFG_Get_Message_Level() ) {
    case CFG_Message_Level_Error: syslog_setVerbosity ( SYSLOG_ERROR ); break;
    case CFG_Message_Level_Warn: syslog_setVerbosity ( SYSLOG_WARNING ); break;
    case CFG_Message_Level_Info: syslog_setVerbosity ( SYSLOG_NOTICE ); break;
    case CFG_Message_Level_Debug: syslog_setVerbosity ( SYSLOG_DEBUG ); break;
    case CFG_Message_Level_Trace: syslog_setVerbosity ( SYSLOG_DEBUG ); break;
    default: syslog_setVerbosity ( SYSLOG_NOTICE ); break;
    }

    syslog_setMaxFileSize( CFG_Get_Message_File_Size() * 1024L );
    syslog_enableOut ( SYSOUT_TLM  );
    syslog_disableOut( SYSOUT_FILE );

if ( 65535 == CFG_Get_Serial_Number() ) {
  CFG_Set_Sensor_Type(CFG_Sensor_Type_HyperNavRadiometer);
  CFG_Set_Sensor_Version(CFG_Sensor_Version_V1);
  CFG_Set_Serial_Number( 1 );
  CFG_Set_PCB_Supervisor( CFG_PCB_Supervisor_Available );
  CFG_Set_USB_Switch ( CFG_USB_Switch_Missing );
  CFG_Set_Spec_Starboard_Serial_Number( 111 );
  CFG_Set_Spec_Port_Serial_Number ( 222 );
  CFG_Set_Frame_Starboard_Serial_Number ( 111 );
  CFG_Set_Frame_Port_Serial_Number ( 222 );
}

    syslog_out( SYSLOG_NOTICE, "StartController", "%s SN:%04d %s", CFG_Get_Sensor_Type_AsString(), CFG_Get_Serial_Number(), CFG_Get_Sensor_Version_AsString() );
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
    syslog_out( SYSLOG_NOTICE, "StartController", "FW Generic Version " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH HNV_CTRL_CUSTOM );
# elif defined(OPERATION_AUTONOMOUS)
    syslog_out( SYSLOG_NOTICE, "StartController", "FW Autonomous Version " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH HNV_CTRL_CUSTOM );
# elif defined(OPERATION_NAVIS)
    syslog_out( SYSLOG_NOTICE, "StartController", "FW NAVIS Version " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH HNV_CTRL_CUSTOM );
# else
#  error NO OPERATION defined
# endif
    syslog_out( SYSLOG_NOTICE, "StartController", "Built %s", buildVersion() );
    if ( upMsg ) syslog_out( SYSLOG_NOTICE, "StartController", upMsg );

    bool haveSupervisor = false;

# if 0
    if ( 1 /* CFG_PCB_Supervisor_Available == CFG_Get_PCB_Supervisor() TODO */ ) {
        S8 errorval;
        if ( PCBSUPERVISOR_OK == supervisorInitialize(&errorval) ) {
            haveSupervisor = true;
        } else {
            powerConfig.hasPCBSupervisor = FALSE;
            pwr_setConfig(powerConfig);

        }
    }
# endif

    /*
     *  Find out what happened before this execution.
     *
     *  (1) This may be firmware-only related,
     *      i.e., no power cycling involved.
     *      In that case, no PCB Supervisor querying.
     *  (2) This may have been externally driven.
     *      In that case, may check what PCB Supervisor says.
     */
    char* resetMsg;

    bool checkSupervisorWakeup = false;
    global_start_new_A_file = false;

    switch ( hnv_sys_ctrl_resetCause() ) {
    case HNV_SYS_CTRL_WDT_RST:  // io_out_string ( "RC HNV_SYS_CTRL_WDT_RST\r\n" );
        resetMsg = "watchdog timeout";
        global_start_new_A_file = true;     //  For APF mode without power-cycling control

        // if supercaps haven't discharged, lowPowerStall() may trigger a watchdog reset
        // which can fool fixed time mode, therefore must check supervisor
        if (haveSupervisor)
        {
            U8 check_value;
            S8 errval;
            if ( PCBSUPERVISOR_OK == supervisorReadUserRegister ( (U8)SPV_CHECK_REG, &check_value, &errval ) )
            {
                if (check_value == SPV_CHECK_REG_CKVAL)
                {
                    // we had a watchdog timeout when in deep sleep, most likely due to supercaps not discharging
                    checkSupervisorWakeup = true;
                    // reset register
                    if (PCBSUPERVISOR_FAIL == supervisorWriteUserRegister(SPV_CHECK_REG, SPV_CHECK_REG_RSTVAL, &errval))
                    {
                        syslog_out( SYSLOG_WARNING, "StartController", "Failed to write SV user register (%hd).", (S16)errval );
                    }
                    else syslog_out( SYSLOG_DEBUG, "StartController", "Reset SV user register." );
                }
                else
                {
                    syslog_out(SYSLOG_DEBUG, "StartController", "SV user register: %hd.", (S16)check_value);
                }

            }
            else
            {
                syslog_out( SYSLOG_WARNING, "StartController", "Failed to read SV user register." );
            }

        }

        break;
    case HNV_SYS_CTRL_PCYCLE_RST: // io_out_string ( "RC HNV_SYS_CTRL_PCYCLE_RST\r\n" );
        //  Increment the power cycle counter.
        CFG_Set_Power_Cycle_Counter(CFG_Get_Power_Cycle_Counter()+1);
        global_reset_was_power_up = true;   //  For data log file naming and header output
        global_start_new_A_file = true;
        resetMsg = "power cycling";
        checkSupervisorWakeup = true;
        break;
    case HNV_SYS_CTRL_EXT_RST: // io_out_string ( "RC HNV_SYS_CTRL_EXT_RST\r\n" );
        resetMsg = "external reset";
        break;
    case HNV_SYS_CTRL_BOD_RST: // io_out_string ( "RC HNV_SYS_CTRL_BOD_RST\r\n" );
        resetMsg = "internal brown-out";
        break;
    case HNV_SYS_CTRL_CPUERR_RST: // io_out_string ( "RC HNV_SYS_CTRL_CPUERR_RST\r\n" );
        resetMsg = "CPU error";
        break;
    case HNV_SYS_CTRL_UNKNOWN_RST: // io_out_string ( "RC HNV_SYS_CTRL_UNKNOWN_RST\r\n" );
        resetMsg = "unknown event";
        //  Unknown happens when the supervisor wakes the firmware prior to full shutdown.
        //  This can happen when sending telemetry soon after the SUNA entered DEEP SLEEP,
        //  e.g., in periodic mode.  To make the SUNA more responsive, check supervisor_wakeup,
        //  and permit breaking into command shell inside those operating modes that support it.
        checkSupervisorWakeup = true;
        break;
    default:
        resetMsg = "no event";
    }

    wakeup_t supervisor_wakeup = PWR_UNKNOWN_WAKEUP;

    char* svsrMsg = "";

    if ( checkSupervisorWakeup && haveSupervisor ) {

        U8 wakeupCause;
        S8 errorval;
        if ( PCBSUPERVISOR_OK == supervisorReadUserRegister ( (U8)SPV_WAKEUP_SOURCE_REG, &wakeupCause, &errorval ) )
        {
            switch ( wakeupCause )
            {
            case SPV_WOKE_FROM_RTC:     //  Real Time Clock
                supervisor_wakeup = PWR_EXTRTC_WAKEUP;
                svsrMsg = ", sv clock wakeup";
                break;

            case SPV_WOKE_FROM_RS232:   //  RS-232
                supervisor_wakeup = PWR_TELEMETRY_WAKEUP;
                svsrMsg = ", sv telemetry wakeup";
                break;

            case SPV_WOKE_FROM_USB_INSERTION:
                supervisor_wakeup = PWR_TELEMETRY_WAKEUP;
                svsrMsg = ", sv usb insert wakeup";
                break;

            case SPV_WOKE_FROM_OC:      //  Unused: Open-Circuit Telemetry
                //  Ignore
                svsrMsg = ", sv unknown wakeup";
                break;

            default:            //  Other
                svsrMsg = ", sv default wakeup";
                break;
            }
        }
    }

    syslog_out( SYSLOG_NOTICE, "StartController", "Reset from %s%s", resetMsg, svsrMsg );

# if 0
    CFG_Set_System_Reset_Counter(CFG_Get_System_Reset_Counter()+1);

    //  Optional startup messages

    CFG_Message_Level const msgLevel = CFG_Get_Message_Level();

    if ( msgLevel >= CFG_Message_Level_Trace ) {

        io_out_string ( "Trace mode.\r\n" );
    //  io_out_memUsage ( "Up" );  ---  TODO  --- Currently failing
        CFG_Print();
    }
# endif

    syslog_out( SYSLOG_DEBUG, "StartController", "System resets: %ld, power cycles: %ld",
                CFG_Get_System_Reset_Counter(), CFG_Get_Power_Cycle_Counter() );

    syslog_out( SYSLOG_DEBUG, "StartController", "Power up at %s (%lu)",
            str_time_formatted ( CFG_Get_System_Reset_Time(), DATE_TIME_ISO_FORMAT ),
            CFG_Get_System_Reset_Time() );

    if ( 0 /* Code ERROR below - always resetting! */ ) {
        struct timeval time;
        gettimeofday(&time, NULL);
        struct tm* asDateTime = gmtime(&time.tv_sec);

        if ( ! ( 2017 <= asDateTime->tm_year && asDateTime->tm_year <= 2020 ) ) {
            struct tm tt;
            if ( strptime ( "2010/01/01 00:00:00", DATE_TIME_IO_FORMAT, &tt ) ) {
                struct timeval tv;
                tv.tv_sec = mktime ( &tt );
                tv.tv_usec = 0;
                settimeofday ( &tv, 0 );
                time_sys2ext();
            }

            syslog_out( SYSLOG_ERROR, "StartController", "Time was OOR - Reset." );
        }
    }

# if 0
    //  Get wavelength values from files into memory
    //
    S16 side;
    for ( side=0; side<2; side++ ) {
        wl_Load ( side );
    }
# endif

# if 0
    //
    //  At DEBUG level, display info
    //
    if ( msgLevel >= CFG_Message_Level_Debug ) {
        Info_SensorID ( (fHandler_t*)0, "" );
        Info_SensorState ( (fHandler_t*)0, "" );
        Info_SensorDevices ( (fHandler_t*)0, "", false );
        Info_ProcessingParameters( (fHandler_t*)0, "" );
    }
# endif

    {
      volatile avr32_pm_t* pm = &AVR32_PM;
# if 0
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->mcctrl    = %08x", pm->mcctrl     );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->cksel     = %08x", pm->cksel      );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->cpumask   = %08x", pm->clkmask[0] );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->hsbmask   = %08x", pm->clkmask[1] );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->pbamask   = %08x", pm->clkmask[2] );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->pbbmask   = %08x", pm->clkmask[3] );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->pll0      = %08x", pm->pll[0]     );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->pll1      = %08x", pm->pll[1]     );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->oscctrl0  = %08x", pm->oscctrl0   );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->oscctrl1  = %08x", pm->oscctrl1   );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->oscctrl32 = %08x", pm->oscctrl32  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->poscsr    = %08x", pm->poscsr     );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl0   = %08x", pm->gcctrl[0]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl1   = %08x", pm->gcctrl[1]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl2   = %08x", pm->gcctrl[2]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl3   = %08x", pm->gcctrl[3]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl4   = %08x", pm->gcctrl[4]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gcctrl5   = %08x", pm->gcctrl[5]  );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->bod       = %08x", pm->bod        );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->bod33     = %08x", pm->bod33      );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->rcause    = %08x", pm->rcause     );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->awen      = %08x", pm->awen       );
# endif
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gplp0     = %08x", pm_read_gplp(pm,0) );
      syslog_out( SYSLOG_NOTICE, "StartController", "PM->gplp1     = %08x", pm_read_gplp(pm,1) );
    }


    //  Start spectrometer board
    //  and allow it to initialize before initating other actions
    //
    hnv_sys_spec_board_start();
    vTaskDelay((portTickType)TASK_DELAY_MS( 3000 ));

    //  Allow emergency entry into Menu
    //
    emergencyMenuEntry();

    //  Enter command loop, which never returns
    //
    command_controller_loop( supervisor_wakeup );

    //  Goto here if fatal error occurred
sys_failed:

    //  watchdog_disable ();

    syslog_out( SYSLOG_ERROR, "StartController", "SYSTEM FAILED. Shutdown." );
    pwr_shutdown();
}

void commander_controller_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

Bool command_controller_createTask(void) {

  static Bool gTaskCreated = FALSE;

  if(!gTaskCreated) {

    // Create Setup+Command Task
    if (pdPASS != xTaskCreate( StartController,
                  HNV_SetupCmdCtrlTask_NAME, HNV_SetupCmdCtrlTask_STACK_SIZE, NULL,
                  HNV_SetupCmdCtrlTask_PRIORITY, &gHNV_SetupCmdCtrlTask_Handler)) {
      return FALSE;
    }
    //  Different from all other tasks,
    //  this task does not support pause/resume.
    //  It is always running,
    //  thus there is no need to have a gTaskCtrlMutex

    // Create message queue
    if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) ) {
      return FALSE;
    }

    //  Allocate memory for data received via packets
    sending_data_package.mutex = xSemaphoreCreateMutex();
    sending_data_package.state = EmptyRAM;
    sending_data_package.address = (void*) pvPortMalloc ( sizeof(any_sending_data_t) );
    if ( NULL == sending_data_package.mutex
      || NULL == sending_data_package.address ) {
      return FALSE;
    }

    gTaskCreated = TRUE;
    return TRUE;

  } else {

    return TRUE;
  }
}
