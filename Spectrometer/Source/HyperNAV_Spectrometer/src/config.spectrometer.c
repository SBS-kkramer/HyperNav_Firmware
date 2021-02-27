/*
 *  File: config.c
 *  HyperNav Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

# include "config.spectrometer.h"
# include "errorcodes.h"

# include <errno.h>
# include <ctype.h>
# include <string.h>
# include <stdlib.h>
# include <time.h>
# include <sys/time.h>

# include "io_funcs.spectrometer.h"
# include "version.spectrometer.h"

/*
 *   Maintain same order as Get...() calls in config.h file
 */

# define CFG_DATA_ID_DEFAULT      0
# define CFG_DATA_ACC_MOUNT      32.0
# define CFG_DATA_ACC_DEFAULT     0
# define CFG_DATA_MAG_DEFAULT     0
# define CFG_DATA_LAT_DEFAULT     0.0
# define CFG_DATA_LON_DEFAULT     0.0
# define CFG_DATA_MAGDEC_DEFAULT  0.0
# define CFG_DATA_PRES_DEFAULT    0.0
# define CFG_DATA_PRES_DIVISOR    1
# define CFG_DATA_PRES_SIMULATE   0

# define CFG_DATA_PROF_UPP_INTERVAL  0.0
# define CFG_DATA_PROF_UPP_START    10.0
# define CFG_DATA_PROF_MID_INTERVAL 20.0
# define CFG_DATA_PROF_MID_START   200.0
# define CFG_DATA_PROF_LOW_INTERVAL 50.0
# define CFG_DATA_PROF_LOW_START   500.0

# define CFG_DATA_FRAME_NO_SERIAL_NUMBER 0

# define CFG_DATA_SATURATION_COUNTS 50000
# define CFG_DATA_NUMBER_OF_CLEAROUTS 3

static Config_Data_t cfg_data = {

	.content      = CD_Empty,

	.id           = CFG_DATA_ID_DEFAULT,

	.acc_mounting = CFG_DATA_ACC_MOUNT,
	.acc_x        = CFG_DATA_ACC_DEFAULT,
	.acc_y        = CFG_DATA_ACC_DEFAULT,
	.acc_z        = CFG_DATA_ACC_DEFAULT,

	.mag_min_x    = CFG_DATA_MAG_DEFAULT,
	.mag_max_x    = CFG_DATA_MAG_DEFAULT,
	.mag_min_y    = CFG_DATA_MAG_DEFAULT,
	.mag_max_y    = CFG_DATA_MAG_DEFAULT,
	.mag_min_z    = CFG_DATA_MAG_DEFAULT,
	.mag_max_z    = CFG_DATA_MAG_DEFAULT,

    .gps_lati     = CFG_DATA_LAT_DEFAULT,
    .gps_long     = CFG_DATA_LON_DEFAULT,
    .magdecli     = CFG_DATA_MAGDEC_DEFAULT,

    .digiqzu0     = CFG_DATA_PRES_DEFAULT,
    .digiqzy1     = CFG_DATA_PRES_DEFAULT,
    .digiqzy2     = CFG_DATA_PRES_DEFAULT,
    .digiqzy3     = CFG_DATA_PRES_DEFAULT,
    .digiqzc1     = CFG_DATA_PRES_DEFAULT,
    .digiqzc2     = CFG_DATA_PRES_DEFAULT,
    .digiqzc3     = CFG_DATA_PRES_DEFAULT,
    .digiqzd1     = CFG_DATA_PRES_DEFAULT,
    .digiqzd2     = CFG_DATA_PRES_DEFAULT,
    .digiqzt1     = CFG_DATA_PRES_DEFAULT,
    .digiqzt2     = CFG_DATA_PRES_DEFAULT,
    .digiqzt3     = CFG_DATA_PRES_DEFAULT,
    .digiqzt4     = CFG_DATA_PRES_DEFAULT,
    .digiqzt5     = CFG_DATA_PRES_DEFAULT,

    .dqtempdv     = CFG_DATA_PRES_DIVISOR,
    .dqpresdv     = CFG_DATA_PRES_DIVISOR,

    .simdepth     = CFG_DATA_PRES_SIMULATE,
    .simascnt     = CFG_DATA_PRES_SIMULATE,

    .puppival     = CFG_DATA_PROF_UPP_INTERVAL,
    .puppstrt     = CFG_DATA_PROF_UPP_START,
    .pmidival     = CFG_DATA_PROF_MID_INTERVAL,
    .pmidstrt     = CFG_DATA_PROF_MID_START,
    .plowival     = CFG_DATA_PROF_LOW_INTERVAL,
    .plowstrt     = CFG_DATA_PROF_LOW_START,

    .frmprtsn     = CFG_DATA_FRAME_NO_SERIAL_NUMBER,
    .frmsbdsn     = CFG_DATA_FRAME_NO_SERIAL_NUMBER,

    .saturcnt     = CFG_DATA_SATURATION_COUNTS,
    .numclear     = CFG_DATA_NUMBER_OF_CLEAROUTS,
};

void CFG_Set( Config_Data_t* config_data ) {
  if ( config_data ) memcpy ( &cfg_data, config_data, sizeof(Config_Data_t) );
}

uint32_t CFG_Get_ID () {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_ID_DEFAULT;
  else                                return cfg_data.id;
}

F32 CFG_Get_Acc_Mounting () {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_ACC_MOUNT;
  else                                return cfg_data.acc_mounting;
}

int16_t CFG_Get_Acc_X () {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_ACC_DEFAULT;
  else                                return cfg_data.acc_x;
}

int16_t CFG_Get_Acc_Y () {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_ACC_DEFAULT;
  else                                return cfg_data.acc_y;
}

int16_t CFG_Get_Acc_Z () {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_ACC_DEFAULT;
  else                                return cfg_data.acc_z;
}

int16_t CFG_Get_Mag_Min_X() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_min_x;
}

int16_t CFG_Get_Mag_Max_X() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_max_x;
}

int16_t CFG_Get_Mag_Min_Y() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_min_y;
}

int16_t CFG_Get_Mag_Max_Y() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_max_y;
}

int16_t CFG_Get_Mag_Min_Z() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_min_z;
}

F64 CFG_Get_Latitude() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_LAT_DEFAULT;
  else                                return cfg_data.gps_lati;
}

F64 CFG_Get_Longitude() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_LON_DEFAULT;
  else                                return cfg_data.gps_long;
}

F64 CFG_Get_Magnetic_Declination() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAGDEC_DEFAULT;
  else                                return cfg_data.magdecli;
}

int16_t CFG_Get_Mag_Max_Z() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_MAG_DEFAULT;
  else                                return cfg_data.mag_max_z;
}

F64 CFG_Get_Pres_U0() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzu0;
}

F64 CFG_Get_Pres_Y1() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzy1;
}

F64 CFG_Get_Pres_Y2() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzy2;
}

F64 CFG_Get_Pres_Y3() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzy3;
}

F64 CFG_Get_Pres_C1() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzc1;
}

F64 CFG_Get_Pres_C2() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzc2;
}

F64 CFG_Get_Pres_C3() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzc3;
}

F64 CFG_Get_Pres_D1() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzd1;
}

F64 CFG_Get_Pres_D2() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzd2;
}


F64 CFG_Get_Pres_T1() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzt1;
}

F64 CFG_Get_Pres_T2() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzt2;
}

F64 CFG_Get_Pres_T3() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzt3;
}

F64 CFG_Get_Pres_T4() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzt4;
}

F64 CFG_Get_Pres_T5() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DEFAULT;
  else                                return cfg_data.digiqzt5;
}

int16_t CFG_Get_Pres_TDiv() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DIVISOR;
  else                                return cfg_data.dqtempdv;
}

int16_t CFG_Get_Pres_PDiv() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_DIVISOR;
  else                                return cfg_data.dqpresdv;
}

int16_t CFG_Get_Pres_Simulated_Depth() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_SIMULATE;
  else                                return cfg_data.simdepth;
}

int16_t CFG_Get_Pres_Simulated_Ascent_Rate() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PRES_SIMULATE;
  else                                return cfg_data.simascnt;
}

F32 CFG_Get_Prof_Upper_Interval() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_UPP_INTERVAL;
  else                                return cfg_data.puppival;
}

F32 CFG_Get_Prof_Upper_Start() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_UPP_START;
  else                                return cfg_data.puppstrt;
}

F32 CFG_Get_Prof_Middle_Interval() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_MID_INTERVAL;
  else                                return cfg_data.pmidival;
}

F32 CFG_Get_Prof_Middle_Start() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_MID_START;
  else                                return cfg_data.pmidstrt;
}

F32 CFG_Get_Prof_Lower_Interval() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_LOW_INTERVAL;
  else                                return cfg_data.plowival;
}

F32 CFG_Get_Prof_Lower_Start() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_PROF_LOW_START;
  else                                return cfg_data.plowstrt;
}

uint16_t CFG_Get_Frame_Port_Serial_Number() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_FRAME_NO_SERIAL_NUMBER;
  else                                return cfg_data.frmprtsn;
}

uint16_t CFG_Get_Frame_Starboard_Serial_Number() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_FRAME_NO_SERIAL_NUMBER;
  else                                return cfg_data.frmsbdsn;
}

uint16_t CFG_Get_Saturation_Counts() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_SATURATION_COUNTS;
  else                                return cfg_data.saturcnt;
}

uint8_t CFG_Get_Number_of_Clearouts() {
  if ( CD_Empty == cfg_data.content ) return CFG_DATA_NUMBER_OF_CLEAROUTS;
  else                                return cfg_data.numclear;
}

# if 0
S16 CFG_Save () {

  S16 rv = CFG_OK;

  return rv;
}

S16 CFG_Retrieve () {

  S16 rv = CFG_OK;

//U32 mVar;
//U8 enum_asU8;

  //  Build Parameters

  return rv;
}


//  Build Parameters

//
//  Configuration Parameter Firmware_Version
//  Is Patch + 256* Minor + 256*256 * Major
//
U32 CFG_Get_Firmware_Version(void) {
  return cfg_data.frwrvers;
}

S16 CFG_Set_Firmware_Version( U32 new_value ) {
    return CFG_FAIL;
}

//
//  Configuration Parameter Admin_Password
//
//
char* CFG_Get_Admin_Password(void) {
  return cfg_data.admin_pw;
}

S16 CFG_Set_Admin_Password( char* new_value ) {
    return CFG_FAIL;
}
# endif


S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len ) {

  S16 cec = CEC_Ok;

  if ( !result || r_max_len < 16 ) {
    return CEC_Failed;
  }

  result[0] = 0;

  //  Build Parameters



  if ( 0 == strcasecmp ( option, "ADMIN_PW" ) ) {
    //strncpy ( result, CFG_Get_Admin_Password(), r_max_len_m1 );

  //  Setup Parameters

  //  Operational Parameters

  } else if ( 0 == strcasecmp ( option, "FirmwareVersion" ) ) {
    //strncpy ( result, HNV_SPEC_FW_VERSION_MAJOR "." HNV_SPEC_FW_VERSION_MINOR "." HNV_SPEC_FW_VERSION_PATCH, r_max_len_m1 );


  } else if ( 0 == strcasecmp ( option, "cfg" ) ) {
    CFG_Print();
  } else {
    cec = CEC_Failed;
  }

  return cec;
}

S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode ) {

  if ( 0 == option || 0 == *option ) return CEC_Failed;
  if ( 0 == value  || 0 == *value  ) return CEC_Failed;

  S16 cec = CEC_Ok;


  //  Build Parameters

  if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "ADMIN_PW" ) ) {
    //if ( CFG_FAIL == CFG_Set_Admin_Password ( value ) ) cec = CEC_Failed;

  //  Setup Parameters

  //  Input_Output Parameters

  } else {
    cec = CEC_Failed;
  }

  return cec;
}

void CFG_Print () {

  io_out_string ( "\r\n" );

  io_out_string ( "CHECKFLG " ); io_out_S32( "%ld\r\n", CFG_Get_ID() );

  io_out_string ( "FRMPRTSN " ); io_out_S32( "%hd\r\n", CFG_Get_Frame_Port_Serial_Number() );
  io_out_string ( "FRMSBDSN " ); io_out_S32( "%hd\r\n", CFG_Get_Frame_Starboard_Serial_Number() );

  io_out_string ( "ACCMNTNG " ); io_out_F32( "%.2f\r\n", CFG_Get_Acc_Mounting() );
  io_out_string ( "ACCVERTX " ); io_out_S16( "%hd\r\n", CFG_Get_Acc_X() );
  io_out_string ( "ACCVERTY " ); io_out_S16( "%hd\r\n", CFG_Get_Acc_Y() );
  io_out_string ( "ACCVERTZ " ); io_out_S16( "%hd\r\n", CFG_Get_Acc_Z() );

  io_out_string ( "MAGMIN_X " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Min_X() );
  io_out_string ( "MAGMAX_X " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Max_X() );
  io_out_string ( "MAGMIN_Y " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Min_Y() );
  io_out_string ( "MAGMAX_Y " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Max_Y() );
  io_out_string ( "MAGMIN_Z " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Min_Z() );
  io_out_string ( "MAGMAX_Z " ); io_out_S16( "%hd\r\n", CFG_Get_Mag_Max_Z() );

  io_out_string ( "GPS_LATI " ); io_out_F64( "%14.6f\r\n", CFG_Get_Latitude() );
  io_out_string ( "GPS_LONG " ); io_out_F64( "%14.6f\r\n", CFG_Get_Longitude() );
  io_out_string ( "MAGDECLI " ); io_out_F64( "%14.6f\r\n", CFG_Get_Magnetic_Declination() );

  io_out_string ( "DIGIQZU0 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_U0() );
  io_out_string ( "DIGIQZY1 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_Y1() );
  io_out_string ( "DIGIQZY2 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_Y2() );
  io_out_string ( "DIGIQZY3 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_Y3() );
  io_out_string ( "DIGIQZC1 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_C1() );
  io_out_string ( "DIGIQZC2 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_C2() );
  io_out_string ( "DIGIQZC3 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_C3() );
  io_out_string ( "DIGIQZD1 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_D1() );
  io_out_string ( "DIGIQZD2 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_D2() );
  io_out_string ( "DIGIQZT1 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_T1() );
  io_out_string ( "DIGIQZT2 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_T2() );
  io_out_string ( "DIGIQZT3 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_T3() );
  io_out_string ( "DIGIQZT4 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_T4() );
  io_out_string ( "DIGIQZT5 " ); io_out_F64( "%14f\r\n", CFG_Get_Pres_T5() );
  io_out_string ( "DQTEMPDV " ); io_out_S16( "%hd\r\n", CFG_Get_Pres_TDiv() );
  io_out_string ( "DQPRESDV " ); io_out_S16( "%hd\r\n", CFG_Get_Pres_PDiv() );
  io_out_string ( "SIMDEPTH " ); io_out_S16( "%hd\r\n", CFG_Get_Pres_Simulated_Depth() );
  io_out_string ( "SIMASCNT " ); io_out_S16( "%hd\r\n", CFG_Get_Pres_Simulated_Ascent_Rate() );

  io_out_string ( "PUPPIVAL " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Upper_Interval() );
  io_out_string ( "PUPPSTRT " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Upper_Start() );
  io_out_string ( "PMIDIVAL " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Middle_Interval() );
  io_out_string ( "PMIDSTRT " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Middle_Start() );
  io_out_string ( "PLOWIVAL " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Lower_Interval() );
  io_out_string ( "PLOWSTRT " ); io_out_F32( "%10f\r\n", CFG_Get_Prof_Lower_Start() );

  io_out_string ( "SATURCNT " ); io_out_S32( "%ld\r\n", (S32)CFG_Get_Saturation_Counts() );
  io_out_string ( "NUMCLEAR " ); io_out_S16( "%hd\r\n", (S16)CFG_Get_Number_of_Clearouts() );

  io_out_string ( "\r\n" );

}


