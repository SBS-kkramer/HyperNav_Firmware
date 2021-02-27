/*
 *  File: config.spectrometer.h
 *  HyperNAV Spectrometer Board Firmware
 */

#ifndef CONFIG_SPECTROMETER_H_
#define CONFIG_SPECTROMETER_H_

# include <compiler.h>
# include "command.spectrometer.h"
# include "config_data.h"

# define CFG_OK    0
# define CFG_FAIL -1

//  Configuration management functions

void CFG_Set( Config_Data_t* config_data );

uint32_t CFG_Get_ID ( void );

F32     CFG_Get_Acc_Mounting ( void );
int16_t CFG_Get_Acc_X ( void );
int16_t CFG_Get_Acc_Y ( void );
int16_t CFG_Get_Acc_Z ( void );

int16_t CFG_Get_Mag_Min_X( void );
int16_t CFG_Get_Mag_Max_X( void );
int16_t CFG_Get_Mag_Min_Y( void );
int16_t CFG_Get_Mag_Max_Y( void );
int16_t CFG_Get_Mag_Min_Z( void );
int16_t CFG_Get_Mag_Max_Z( void );

F64 CFG_Get_Latitude( void );
F64 CFG_Get_Longitude( void );
F64 CFG_Get_Magnetic_Declination( void );

F64 CFG_Get_Pres_U0( void );
F64 CFG_Get_Pres_Y1( void );
F64 CFG_Get_Pres_Y2( void );
F64 CFG_Get_Pres_Y3( void );
F64 CFG_Get_Pres_C1( void );
F64 CFG_Get_Pres_C2( void );
F64 CFG_Get_Pres_C3( void );
F64 CFG_Get_Pres_D1( void );
F64 CFG_Get_Pres_D2( void );
F64 CFG_Get_Pres_T1( void );
F64 CFG_Get_Pres_T2( void );
F64 CFG_Get_Pres_T3( void );
F64 CFG_Get_Pres_T4( void );
F64 CFG_Get_Pres_T5( void );
int16_t CFG_Get_Pres_TDiv( void );
int16_t CFG_Get_Pres_PDiv( void );
int16_t CFG_Get_Pres_Simulated_Depth( void );
int16_t CFG_Get_Pres_Simulated_Ascent_Rate( void );

F32 CFG_Get_Prof_Upper_Interval( void );
F32 CFG_Get_Prof_Upper_Start( void );
F32 CFG_Get_Prof_Middle_Interval( void );
F32 CFG_Get_Prof_Middle_Start( void );
F32 CFG_Get_Prof_Lower_Interval( void );
F32 CFG_Get_Prof_Lower_Start( void );

uint16_t CFG_Get_Frame_Port_Serial_Number( void );
uint16_t CFG_Get_Frame_Starboard_Serial_Number( void );

uint16_t CFG_Get_Saturation_Counts( void );
uint8_t CFG_Get_Number_of_Clearouts( void );

//!  \brief  Save configuration to persistent memory
//S16 CFG_Save(void);
//!  \brief  Retrieve configuration from persistent memory
//S16 CFG_Retrieve(void);
//!  \brief  Write all configuration values to user output
void CFG_Print(void);

//  Accessing individual configuration parameters
//  With some exceptions, for a parameter <P> of a given
//  data type <T>, there are two access functions:
//  <T> GFG_Get<P> ( void );
//  bool CFG_Set<P> ( <T> new_value );
//  The "Get" function returns the currently configured parameter value,
//  the "Set" function returns CFG_OK if successful,
//                         and CFG_FAIL if unsuccessful (e.g., out-of-range).
//  For enumerated data types, there are also Set..._AsString() functions,
//  that parse a string matching the name of the enumerated value, and
//  will assign the enumerated type via the regular Set() function.
//  Default, min, and max values for the parameters are given via #define
//  For some parameters, named #defines are provided to give meaning to values.
//  All defines use a 8-letter code, followed by an underscore, followed by a designator.

S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len );
S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode );

#endif /* CONFIG_SPECTROMETER_H_ */
