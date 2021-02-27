/*
 *	ALERT: config.h is auto-generated from CodeTools files.
 *	Do NOT edit.
 *	Read file makeCode.sh for explanations / insructions.
 *
 *  File: config.h
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

#ifndef CONFIG_H_
#define CONFIG_H_

# include <compiler.h>
# include "flashc.h"
# include "command.controller.h"

# define CFG_OK		 0
# define CFG_FAIL	-1

//# define ELC_FLASH_CARD_FULL		0x0101
//# define ELC_FILE_OPEN_FAILURE	0x0102
//# define ELC_CFG_NOT_AVAILABE	0x0201
//# define ELC_MEAS_SPEC			0x0401
//# define ELC_MEAS_LAMP			0x0402
//# define ELC_MEAS_TIMEOUT		0x0403
//# define ELC_NO_CALFILE			0x0801
//# define ELC_BAD_CALFILE		0x0802
//# define ELC_PROCESSING			0x0811
//# define ELC_PROCESSING			0x0811

// Some hard coded values, not to be configured.

# define CFG_Get_Watchdog_Duration()	15000000L	//	15 seconds (in microseconds)
# define CFG_Get_Supercap_Charge_Duration() 2		//	2 seconds

# define CFG_GetFullFrameChannelBegin() 0	//	We are null based
# define CFG_GetFullFrameChannelEnd() 256

# define CFG_GetReducedFrameChannelBegin() 34
# define CFG_GetReducedFrameChannelEnd() 66	//	For a total of 32 channels

//
//	Derived, not set directly.
//		reference sample data rate during the spec's integration period
//		spectrometer integration period (period var) is in milliseconds
//		then convert to milliseconds
//

# define CFG_Get_Reference_Sample_Period() \
	( (U32) ((float) (CFG_Get_Spectrometer_Integration_Period()*CFG_Get_Light_Averages()) \
	        /(float) CFG_Get_Reference_Samples()) - 3 )


//	Configuration management functions

//!	\brief	Save configuration to persistent memory
S16 CFG_Save( bool useBackup );
//!	\brief	Retrieve configuration from persistent memory
S16 CFG_Retrieve( bool useBackup );
//!	\brief	Write all configuration values to user output
void CFG_OORToDefault();
void CFG_Print();
S16 CFG_VarSaveToUPG ( char* m, U16 location, U8* src, U8 size );
S16 CFG_VarSaveToRTC ( char* m, U16 location, U8* src, U8 size );
S16 CFG_WipeBackup();

//	Accessing individual configuration parameters
//	With some exceptions, for a parameter <P> of a given
//	data type <T>, there are two access functions:
//	<T> GFG_Get<P> ( void );
//	bool CFG_Set<P> ( <T> new_value );
//	The "Get" function returns the currently configured parameter value,
//	the "Set" function returns CFG_OK if successful,
//						   and CFG_FAIL if unsuccessful (e.g., out-of-range).
//	For enumerated data types, there are also Set..._AsString() functions,
//	that parse a string matching the name of the enumerated value, and
//	will assign the enumerated type via the regular Set() function.
//	Default, min, and max values for the parameters are given via #define
//	For some parameters, named #defines are provided to give meaning to values.
//	All defines use a 8-letter code, followed by an underscore, followed by a designator.

S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len );
S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode );
void CFG_Print ();
S16 CFG_OutOfRangeCounter ( bool correctOOR, bool setRTCToDefault );
void wipe_RTC( U8 value );
void dump_RTC();
void wipe_UPG( U8 value );
void dump_UPG();

//	Build Parameters


//
//	Configuration Parameter Sensor_Type
//	
//
# define UPG_LOC_SENSTYPE (256)

typedef enum {
	CFG_Sensor_Type_HyperNavRadiometer = 1,
	CFG_Sensor_Type_HyperNavTestHead = 2
} CFG_Sensor_Type;

# define SENSTYPE_DEF CFG_Sensor_Type_HyperNavRadiometer

CFG_Sensor_Type CFG_Get_Sensor_Type(void);
S16 CFG_Set_Sensor_Type( CFG_Sensor_Type );
void CFG_Setup_Sensor_Type();
char* CFG_Get_Sensor_Type_AsString(void);
S16 CFG_Set_Sensor_Type_AsString( char* );

//
//	Configuration Parameter Sensor_Version
//	
//
# define UPG_LOC_SENSVERS (260)

typedef enum {
	CFG_Sensor_Version_V1 = 1
} CFG_Sensor_Version;

# define SENSVERS_DEF CFG_Sensor_Version_V1

CFG_Sensor_Version CFG_Get_Sensor_Version(void);
S16 CFG_Set_Sensor_Version( CFG_Sensor_Version );
void CFG_Setup_Sensor_Version();
char* CFG_Get_Sensor_Version_AsString(void);
S16 CFG_Set_Sensor_Version_AsString( char* );

//
//	Configuration Parameter Serial_Number
//	
//
# define UPG_LOC_SERIALNO (264)

# define SERIALNO_DEF 0
# define SERIALNO_MIN 1
# define SERIALNO_MAX 9999
U16 CFG_Get_Serial_Number(void);
S16 CFG_Set_Serial_Number( U16 );
void CFG_Setup_Serial_Number();
S16 CFG_Set_Serial_Number_AsString( char* );

//
//	Configuration Parameter Firmware_Version
//	Is Patch + 256* Minor + 256*256 * Major
//
# define UPG_LOC_FRWRVERS (268)

# define FRWRVERS_DEF 0
U32 CFG_Get_Firmware_Version(void);
S16 CFG_Set_Firmware_Version( U32 );

//
//	Configuration Parameter PCB_Supervisor
//	
//
# define UPG_LOC_PWRSVISR (280)

typedef enum {
	CFG_PCB_Supervisor_Available = 1,
	CFG_PCB_Supervisor_Missing = 2
} CFG_PCB_Supervisor;

# define PWRSVISR_DEF CFG_PCB_Supervisor_Missing

CFG_PCB_Supervisor CFG_Get_PCB_Supervisor(void);
S16 CFG_Set_PCB_Supervisor( CFG_PCB_Supervisor );
void CFG_Setup_PCB_Supervisor();
char* CFG_Get_PCB_Supervisor_AsString(void);
S16 CFG_Set_PCB_Supervisor_AsString( char* );

//
//	Configuration Parameter USB_Switch
//	
//
# define UPG_LOC_USBSWTCH (284)

typedef enum {
	CFG_USB_Switch_Available = 1,
	CFG_USB_Switch_Missing = 2
} CFG_USB_Switch;

# define USBSWTCH_DEF CFG_USB_Switch_Missing

CFG_USB_Switch CFG_Get_USB_Switch(void);
S16 CFG_Set_USB_Switch( CFG_USB_Switch );
void CFG_Setup_USB_Switch();
char* CFG_Get_USB_Switch_AsString(void);
S16 CFG_Set_USB_Switch_AsString( char* );

//
//	Configuration Parameter Spec_Starboard_Serial_Number
//	
//
# define UPG_LOC_SPCSBDSN (368)

# define SPCSBDSN_DEF 0
U32 CFG_Get_Spec_Starboard_Serial_Number(void);
S16 CFG_Set_Spec_Starboard_Serial_Number( U32 );
void CFG_Setup_Spec_Starboard_Serial_Number();
S16 CFG_Set_Spec_Starboard_Serial_Number_AsString( char* );

//
//	Configuration Parameter Spec_Port_Serial_Number
//	
//
# define UPG_LOC_SPCPRTSN (372)

# define SPCPRTSN_DEF 0
U32 CFG_Get_Spec_Port_Serial_Number(void);
S16 CFG_Set_Spec_Port_Serial_Number( U32 );
void CFG_Setup_Spec_Port_Serial_Number();
S16 CFG_Set_Spec_Port_Serial_Number_AsString( char* );

//
//	Configuration Parameter Frame_Starboard_Serial_Number
//	
//
# define UPG_LOC_FRMSBDSN (376)

# define FRMSBDSN_DEF 1
# define FRMSBDSN_MIN 0
# define FRMSBDSN_MAX 999
U16 CFG_Get_Frame_Starboard_Serial_Number(void);
S16 CFG_Set_Frame_Starboard_Serial_Number( U16 );
void CFG_Setup_Frame_Starboard_Serial_Number();
S16 CFG_Set_Frame_Starboard_Serial_Number_AsString( char* );

//
//	Configuration Parameter Frame_Port_Serial_Number
//	
//
# define UPG_LOC_FRMPRTSN (380)

# define FRMPRTSN_DEF 1
# define FRMPRTSN_MIN 0
# define FRMPRTSN_MAX 999
U16 CFG_Get_Frame_Port_Serial_Number(void);
S16 CFG_Set_Frame_Port_Serial_Number( U16 );
void CFG_Setup_Frame_Port_Serial_Number();
S16 CFG_Set_Frame_Port_Serial_Number_AsString( char* );

//
//	Configuration Parameter Admin_Password
//	
//
# define UPG_LOC_ADMIN_PW (416)


# define ADMIN_PW_DEF ""
char* CFG_Get_Admin_Password(void);
S16 CFG_Set_Admin_Password( char* );
void CFG_Setup_Admin_Password();

//
//	Configuration Parameter Factory_Password
//	
//
# define UPG_LOC_FCTRY_PW (424)


# define FCTRY_PW_DEF ""
char* CFG_Get_Factory_Password(void);
S16 CFG_Set_Factory_Password( char* );
void CFG_Setup_Factory_Password();

//
//	Configuration Parameter Long_Wait_Flag
//	
//
# define UPG_LOC_WAITFLAG (448)

typedef enum {
	CFG_Long_Wait_Flag_Wait = 1,
	CFG_Long_Wait_Flag_Skip = 2
} CFG_Long_Wait_Flag;

# define WAITFLAG_DEF CFG_Long_Wait_Flag_Wait

CFG_Long_Wait_Flag CFG_Get_Long_Wait_Flag(void);
S16 CFG_Set_Long_Wait_Flag( CFG_Long_Wait_Flag );

//	Accelerometer Parameters


//
//	Configuration Parameter Accelerometer_Mounting
//	
//
# define UPG_LOC_ACCMNTNG (292)

# define ACCMNTNG_DEF 0
F32 CFG_Get_Accelerometer_Mounting(void);
S16 CFG_Set_Accelerometer_Mounting( F32 );
S16 CFG_Set_Accelerometer_Mounting_AsString( char* );

//
//	Configuration Parameter Accelerometer_Vertical_x
//	
//
# define UPG_LOC_ACCVERTX (296)

# define ACCVERTX_DEF 0
S16 CFG_Get_Accelerometer_Vertical_x(void);
S16 CFG_Set_Accelerometer_Vertical_x( S16 );
S16 CFG_Set_Accelerometer_Vertical_x_AsString( char* );

//
//	Configuration Parameter Accelerometer_Vertical_y
//	
//
# define UPG_LOC_ACCVERTY (300)

# define ACCVERTY_DEF 0
S16 CFG_Get_Accelerometer_Vertical_y(void);
S16 CFG_Set_Accelerometer_Vertical_y( S16 );
S16 CFG_Set_Accelerometer_Vertical_y_AsString( char* );

//
//	Configuration Parameter Accelerometer_Vertical_z
//	
//
# define UPG_LOC_ACCVERTZ (304)

# define ACCVERTZ_DEF 0
S16 CFG_Get_Accelerometer_Vertical_z(void);
S16 CFG_Set_Accelerometer_Vertical_z( S16 );
S16 CFG_Set_Accelerometer_Vertical_z_AsString( char* );

//	Compass Parameters


//
//	Configuration Parameter Magnetometer_Min_x
//	
//
# define UPG_LOC_MAG_MINX (320)

# define MAG_MINX_DEF 0
S16 CFG_Get_Magnetometer_Min_x(void);
S16 CFG_Set_Magnetometer_Min_x( S16 );
S16 CFG_Set_Magnetometer_Min_x_AsString( char* );

//
//	Configuration Parameter Magnetometer_Max_x
//	
//
# define UPG_LOC_MAG_MAXX (324)

# define MAG_MAXX_DEF 0
S16 CFG_Get_Magnetometer_Max_x(void);
S16 CFG_Set_Magnetometer_Max_x( S16 );
S16 CFG_Set_Magnetometer_Max_x_AsString( char* );

//
//	Configuration Parameter Magnetometer_Min_y
//	
//
# define UPG_LOC_MAG_MINY (328)

# define MAG_MINY_DEF 0
S16 CFG_Get_Magnetometer_Min_y(void);
S16 CFG_Set_Magnetometer_Min_y( S16 );
S16 CFG_Set_Magnetometer_Min_y_AsString( char* );

//
//	Configuration Parameter Magnetometer_Max_y
//	
//
# define UPG_LOC_MAG_MAXY (332)

# define MAG_MAXY_DEF 0
S16 CFG_Get_Magnetometer_Max_y(void);
S16 CFG_Set_Magnetometer_Max_y( S16 );
S16 CFG_Set_Magnetometer_Max_y_AsString( char* );

//
//	Configuration Parameter Magnetometer_Min_z
//	
//
# define UPG_LOC_MAG_MINZ (336)

# define MAG_MINZ_DEF 0
S16 CFG_Get_Magnetometer_Min_z(void);
S16 CFG_Set_Magnetometer_Min_z( S16 );
S16 CFG_Set_Magnetometer_Min_z_AsString( char* );

//
//	Configuration Parameter Magnetometer_Max_z
//	
//
# define UPG_LOC_MAG_MAXZ (340)

# define MAG_MAXZ_DEF 0
S16 CFG_Get_Magnetometer_Max_z(void);
S16 CFG_Set_Magnetometer_Max_z( S16 );
S16 CFG_Set_Magnetometer_Max_z_AsString( char* );

//	GPS Parameters


//
//	Configuration Parameter Latitude
//	
//
# define UPG_LOC_GPS_LATI (480)

# define GPS_LATI_DEF 0
F64 CFG_Get_Latitude(void);
S16 CFG_Set_Latitude( F64 );
S16 CFG_Set_Latitude_AsString( char* );

//
//	Configuration Parameter Longitude
//	
//
# define UPG_LOC_GPS_LONG (488)

# define GPS_LONG_DEF 0
F64 CFG_Get_Longitude(void);
S16 CFG_Set_Longitude( F64 );
S16 CFG_Set_Longitude_AsString( char* );

//
//	Configuration Parameter Magnetic_Declination
//	
//
# define UPG_LOC_MAGDECLI (496)

# define MAGDECLI_DEF 0
F64 CFG_Get_Magnetic_Declination(void);
S16 CFG_Set_Magnetic_Declination( F64 );
S16 CFG_Set_Magnetic_Declination_AsString( char* );

//	Pressure Parameters


//
//	Configuration Parameter Digiquartz_Serial_Number
//	
//
# define UPG_LOC_DIGIQZSN (128)

# define DIGIQZSN_DEF 0
U32 CFG_Get_Digiquartz_Serial_Number(void);
S16 CFG_Set_Digiquartz_Serial_Number( U32 );
S16 CFG_Set_Digiquartz_Serial_Number_AsString( char* );

//
//	Configuration Parameter Digiquartz_U0
//	
//
# define UPG_LOC_DIGIQZU0 (136)

# define DIGIQZU0_DEF 0
F64 CFG_Get_Digiquartz_U0(void);
S16 CFG_Set_Digiquartz_U0( F64 );
S16 CFG_Set_Digiquartz_U0_AsString( char* );

//
//	Configuration Parameter Digiquartz_Y1
//	
//
# define UPG_LOC_DIGIQZY1 (144)

# define DIGIQZY1_DEF 0
F64 CFG_Get_Digiquartz_Y1(void);
S16 CFG_Set_Digiquartz_Y1( F64 );
S16 CFG_Set_Digiquartz_Y1_AsString( char* );

//
//	Configuration Parameter Digiquartz_Y2
//	
//
# define UPG_LOC_DIGIQZY2 (152)

# define DIGIQZY2_DEF 0
F64 CFG_Get_Digiquartz_Y2(void);
S16 CFG_Set_Digiquartz_Y2( F64 );
S16 CFG_Set_Digiquartz_Y2_AsString( char* );

//
//	Configuration Parameter Digiquartz_Y3
//	
//
# define UPG_LOC_DIGIQZY3 (160)

# define DIGIQZY3_DEF 0
F64 CFG_Get_Digiquartz_Y3(void);
S16 CFG_Set_Digiquartz_Y3( F64 );
S16 CFG_Set_Digiquartz_Y3_AsString( char* );

//
//	Configuration Parameter Digiquartz_C1
//	
//
# define UPG_LOC_DIGIQZC1 (168)

# define DIGIQZC1_DEF 0
F64 CFG_Get_Digiquartz_C1(void);
S16 CFG_Set_Digiquartz_C1( F64 );
S16 CFG_Set_Digiquartz_C1_AsString( char* );

//
//	Configuration Parameter Digiquartz_C2
//	
//
# define UPG_LOC_DIGIQZC2 (176)

# define DIGIQZC2_DEF 0
F64 CFG_Get_Digiquartz_C2(void);
S16 CFG_Set_Digiquartz_C2( F64 );
S16 CFG_Set_Digiquartz_C2_AsString( char* );

//
//	Configuration Parameter Digiquartz_C3
//	
//
# define UPG_LOC_DIGIQZC3 (184)

# define DIGIQZC3_DEF 0
F64 CFG_Get_Digiquartz_C3(void);
S16 CFG_Set_Digiquartz_C3( F64 );
S16 CFG_Set_Digiquartz_C3_AsString( char* );

//
//	Configuration Parameter Digiquartz_D1
//	
//
# define UPG_LOC_DIGIQZD1 (192)

# define DIGIQZD1_DEF 0
F64 CFG_Get_Digiquartz_D1(void);
S16 CFG_Set_Digiquartz_D1( F64 );
S16 CFG_Set_Digiquartz_D1_AsString( char* );

//
//	Configuration Parameter Digiquartz_D2
//	
//
# define UPG_LOC_DIGIQZD2 (200)

# define DIGIQZD2_DEF 0
F64 CFG_Get_Digiquartz_D2(void);
S16 CFG_Set_Digiquartz_D2( F64 );
S16 CFG_Set_Digiquartz_D2_AsString( char* );

//
//	Configuration Parameter Digiquartz_T1
//	
//
# define UPG_LOC_DIGIQZT1 (208)

# define DIGIQZT1_DEF 0
F64 CFG_Get_Digiquartz_T1(void);
S16 CFG_Set_Digiquartz_T1( F64 );
S16 CFG_Set_Digiquartz_T1_AsString( char* );

//
//	Configuration Parameter Digiquartz_T2
//	
//
# define UPG_LOC_DIGIQZT2 (216)

# define DIGIQZT2_DEF 0
F64 CFG_Get_Digiquartz_T2(void);
S16 CFG_Set_Digiquartz_T2( F64 );
S16 CFG_Set_Digiquartz_T2_AsString( char* );

//
//	Configuration Parameter Digiquartz_T3
//	
//
# define UPG_LOC_DIGIQZT3 (224)

# define DIGIQZT3_DEF 0
F64 CFG_Get_Digiquartz_T3(void);
S16 CFG_Set_Digiquartz_T3( F64 );
S16 CFG_Set_Digiquartz_T3_AsString( char* );

//
//	Configuration Parameter Digiquartz_T4
//	
//
# define UPG_LOC_DIGIQZT4 (232)

# define DIGIQZT4_DEF 0
F64 CFG_Get_Digiquartz_T4(void);
S16 CFG_Set_Digiquartz_T4( F64 );
S16 CFG_Set_Digiquartz_T4_AsString( char* );

//
//	Configuration Parameter Digiquartz_T5
//	
//
# define UPG_LOC_DIGIQZT5 (240)

# define DIGIQZT5_DEF 0
F64 CFG_Get_Digiquartz_T5(void);
S16 CFG_Set_Digiquartz_T5( F64 );
S16 CFG_Set_Digiquartz_T5_AsString( char* );

//
//	Configuration Parameter Digiquartz_Temperature_Divisor
//	
//
# define UPG_LOC_DQTEMPDV (248)

# define DQTEMPDV_DEF 1
S16 CFG_Get_Digiquartz_Temperature_Divisor(void);
S16 CFG_Set_Digiquartz_Temperature_Divisor( S16 );
S16 CFG_Set_Digiquartz_Temperature_Divisor_AsString( char* );

//
//	Configuration Parameter Digiquartz_Pressure_Divisor
//	
//
# define UPG_LOC_DQPRESDV (252)

# define DQPRESDV_DEF 1
S16 CFG_Get_Digiquartz_Pressure_Divisor(void);
S16 CFG_Set_Digiquartz_Pressure_Divisor( S16 );
S16 CFG_Set_Digiquartz_Pressure_Divisor_AsString( char* );

//
//	Configuration Parameter Simulated_Start_Depth
//	
//
# define UPG_LOC_SIMDEPTH (344)

# define SIMDEPTH_DEF 0
S16 CFG_Get_Simulated_Start_Depth(void);
S16 CFG_Set_Simulated_Start_Depth( S16 );
S16 CFG_Set_Simulated_Start_Depth_AsString( char* );

//
//	Configuration Parameter Sumulated_Ascent_Rate
//	
//
# define UPG_LOC_SIMASCNT (348)

# define SIMASCNT_DEF 0
S16 CFG_Get_Sumulated_Ascent_Rate(void);
S16 CFG_Set_Sumulated_Ascent_Rate( S16 );
S16 CFG_Set_Sumulated_Ascent_Rate_AsString( char* );

//	Profile Parameters


//
//	Configuration Parameter Profile_Upper_Interval
//	
//
# define UPG_LOC_PUPPIVAL (96)

# define PUPPIVAL_DEF 0
F32 CFG_Get_Profile_Upper_Interval(void);
S16 CFG_Set_Profile_Upper_Interval( F32 );
S16 CFG_Set_Profile_Upper_Interval_AsString( char* );

//
//	Configuration Parameter Profile_Upper_Start
//	
//
# define UPG_LOC_PUPPSTRT (100)

# define PUPPSTRT_DEF 0
F32 CFG_Get_Profile_Upper_Start(void);
S16 CFG_Set_Profile_Upper_Start( F32 );
S16 CFG_Set_Profile_Upper_Start_AsString( char* );

//
//	Configuration Parameter Profile_Middle_Interval
//	
//
# define UPG_LOC_PMIDIVAL (104)

# define PMIDIVAL_DEF 0
F32 CFG_Get_Profile_Middle_Interval(void);
S16 CFG_Set_Profile_Middle_Interval( F32 );
S16 CFG_Set_Profile_Middle_Interval_AsString( char* );

//
//	Configuration Parameter Profile_Middle_Start
//	
//
# define UPG_LOC_PMIDSTRT (108)

# define PMIDSTRT_DEF 0
F32 CFG_Get_Profile_Middle_Start(void);
S16 CFG_Set_Profile_Middle_Start( F32 );
S16 CFG_Set_Profile_Middle_Start_AsString( char* );

//
//	Configuration Parameter Profile_Lower_Interval
//	
//
# define UPG_LOC_PLOWIVAL (112)

# define PLOWIVAL_DEF 0
F32 CFG_Get_Profile_Lower_Interval(void);
S16 CFG_Set_Profile_Lower_Interval( F32 );
S16 CFG_Set_Profile_Lower_Interval_AsString( char* );

//
//	Configuration Parameter Profile_Lower_Start
//	
//
# define UPG_LOC_PLOWSTRT (116)

# define PLOWSTRT_DEF 0
F32 CFG_Get_Profile_Lower_Start(void);
S16 CFG_Set_Profile_Lower_Start( F32 );
S16 CFG_Set_Profile_Lower_Start_AsString( char* );

//	Setup Parameters


//
//	Configuration Parameter Configuration_Status
//	
//
# define BBV_LOC_STUPSTUS 0

typedef enum {
	CFG_Configuration_Status_Required = 1,
	CFG_Configuration_Status_Done = 2
} CFG_Configuration_Status;

# define STUPSTUS_DEF CFG_Configuration_Status_Required

CFG_Configuration_Status CFG_Get_Configuration_Status(void);
S16 CFG_Set_Configuration_Status( CFG_Configuration_Status );
char* CFG_Get_Configuration_Status_AsString(void);
S16 CFG_Set_Configuration_Status_AsString( char* );

//
//	Configuration Parameter Firmware_Upgrade
//	
//
# define BBV_LOC_FWUPGRAD 12

typedef enum {
	CFG_Firmware_Upgrade_Yes = 1,
	CFG_Firmware_Upgrade_No = 2
} CFG_Firmware_Upgrade;

# define FWUPGRAD_DEF CFG_Firmware_Upgrade_No

CFG_Firmware_Upgrade CFG_Get_Firmware_Upgrade(void);
S16 CFG_Set_Firmware_Upgrade( CFG_Firmware_Upgrade );

//	Input_Output Parameters


//
//	Configuration Parameter Message_Level
//	
//
# define BBV_LOC_MSGLEVEL 17

typedef enum {
	CFG_Message_Level_Error = 1,
	CFG_Message_Level_Warn = 2,
	CFG_Message_Level_Info = 3,
	CFG_Message_Level_Debug = 4,
	CFG_Message_Level_Trace = 5
} CFG_Message_Level;

# define MSGLEVEL_DEF CFG_Message_Level_Info

CFG_Message_Level CFG_Get_Message_Level(void);
S16 CFG_Set_Message_Level( CFG_Message_Level );
char* CFG_Get_Message_Level_AsString(void);
S16 CFG_Set_Message_Level_AsString( char* );

//
//	Configuration Parameter Message_File_Size
//	
//
# define BBV_LOC_MSGFSIZE 18

# define MSGFSIZE_DEF 2
# define MSGFSIZE_MIN 0
# define MSGFSIZE_MAX 65
U8 CFG_Get_Message_File_Size(void);
S16 CFG_Set_Message_File_Size( U8 );
S16 CFG_Set_Message_File_Size_AsString( char* );

//
//	Configuration Parameter Data_File_Size
//	
//
# define BBV_LOC_DATFSIZE 35

# define DATFSIZE_DEF 2
# define DATFSIZE_MIN 1
# define DATFSIZE_MAX 65
U8 CFG_Get_Data_File_Size(void);
S16 CFG_Set_Data_File_Size( U8 );
S16 CFG_Set_Data_File_Size_AsString( char* );

//
//	Configuration Parameter Data_File_Needs_Header
//	
//
# define BBV_LOC_DATFHEAD 36

typedef enum {
	CFG_Data_File_Needs_Header_Yes = 1,
	CFG_Data_File_Needs_Header_No = 2
} CFG_Data_File_Needs_Header;

# define DATFHEAD_DEF CFG_Data_File_Needs_Header_Yes

CFG_Data_File_Needs_Header CFG_Get_Data_File_Needs_Header(void);
S16 CFG_Set_Data_File_Needs_Header( CFG_Data_File_Needs_Header );

//
//	Configuration Parameter Data_File_LastDayOfYear
//	
//
# define BBV_LOC_DFLSTDOY 37

# define DFLSTDOY_DEF 0
U32 CFG_Get_Data_File_LastDayOfYear(void);
S16 CFG_Set_Data_File_LastDayOfYear( U32 );

//
//	Configuration Parameter Output_Frame_Subsampling
//	
//
# define BBV_LOC_OUTFRSUB 41

# define OUTFRSUB_DEF 4
# define OUTFRSUB_MIN 0
# define OUTFRSUB_MAX 12
U8 CFG_Get_Output_Frame_Subsampling(void);
S16 CFG_Set_Output_Frame_Subsampling( U8 );
S16 CFG_Set_Output_Frame_Subsampling_AsString( char* );

//
//	Configuration Parameter Log_Frames
//	
//
# define BBV_LOC_LOGFRAMS 42

typedef enum {
	CFG_Log_Frames_Yes = 1,
	CFG_Log_Frames_No = 2
} CFG_Log_Frames;

# define LOGFRAMS_DEF CFG_Log_Frames_Yes

CFG_Log_Frames CFG_Get_Log_Frames(void);
S16 CFG_Set_Log_Frames( CFG_Log_Frames );
char* CFG_Get_Log_Frames_AsString(void);
S16 CFG_Set_Log_Frames_AsString( char* );

//
//	Configuration Parameter Acquisition_Counter
//	
//
# define BBV_LOC_ACQCOUNT 48

# define ACQCOUNT_DEF 0
U32 CFG_Get_Acquisition_Counter(void);
S16 CFG_Set_Acquisition_Counter( U32 );
S16 CFG_Set_Acquisition_Counter_AsString( char* );

//
//	Configuration Parameter Continuous_Counter
//	
//
# define BBV_LOC_CNTCOUNT 52

# define CNTCOUNT_DEF 0
U32 CFG_Get_Continuous_Counter(void);
S16 CFG_Set_Continuous_Counter( U32 );
S16 CFG_Set_Continuous_Counter_AsString( char* );

//	Testing Parameters


//
//	Configuration Parameter Data_Mode
//	
//
# define BBV_LOC_DATAMODE 80

typedef enum {
	CFG_Data_Mode_Real = 1,
	CFG_Data_Mode_Fake = 2
} CFG_Data_Mode;

# define DATAMODE_DEF CFG_Data_Mode_Real

CFG_Data_Mode CFG_Get_Data_Mode(void);
S16 CFG_Set_Data_Mode( CFG_Data_Mode );
void CFG_Setup_Data_Mode();
char* CFG_Get_Data_Mode_AsString(void);
S16 CFG_Set_Data_Mode_AsString( char* );

//	Operational Parameters


//
//	Configuration Parameter Operation_Mode
//	Startup; replace FixedTime by Burnin command?
//
# define BBV_LOC_OPERMODE 96

typedef enum {
	CFG_Operation_Mode_Continuous = 1,
	CFG_Operation_Mode_APM = 2
} CFG_Operation_Mode;

# define OPERMODE_DEF CFG_Operation_Mode_Continuous

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
CFG_Operation_Mode CFG_Get_Operation_Mode(void);
S16 CFG_Set_Operation_Mode( CFG_Operation_Mode );
char* CFG_Get_Operation_Mode_AsString(void);
S16 CFG_Set_Operation_Mode_AsString( char* );
# endif

//
//	Configuration Parameter Operation_Control
//	
//
# define BBV_LOC_OPERCTRL 97

typedef enum {
	CFG_Operation_Control_Duration = 1,
	CFG_Operation_Control_Samples = 2
} CFG_Operation_Control;

# define OPERCTRL_DEF CFG_Operation_Control_Duration

CFG_Operation_Control CFG_Get_Operation_Control(void);
S16 CFG_Set_Operation_Control( CFG_Operation_Control );
char* CFG_Get_Operation_Control_AsString(void);
S16 CFG_Set_Operation_Control_AsString( char* );

//
//	Configuration Parameter Operation_State
//	
//
# define BBV_LOC_OPERSTAT 98

typedef enum {
	CFG_Operation_State_Prompt = 1,
	CFG_Operation_State_Streaming = 2,
	CFG_Operation_State_Profiling = 3,
	CFG_Operation_State_Transmitting = 4
} CFG_Operation_State;

# define OPERSTAT_DEF CFG_Operation_State_Streaming

CFG_Operation_State CFG_Get_Operation_State(void);
S16 CFG_Set_Operation_State( CFG_Operation_State );

//
//	Configuration Parameter System_Reset_Time
//	Time when current execution started
//
# define BBV_LOC_SYSRSTTM 102

# define SYSRSTTM_DEF 0
U32 CFG_Get_System_Reset_Time(void);
S16 CFG_Set_System_Reset_Time( U32 );

//
//	Configuration Parameter System_Reset_Counter
//	
//
# define BBV_LOC_SYSRSTCT 106

# define SYSRSTCT_DEF 0
U32 CFG_Get_System_Reset_Counter(void);
S16 CFG_Set_System_Reset_Counter( U32 );

//
//	Configuration Parameter Power_Cycle_Counter
//	
//
# define BBV_LOC_PWRCYCNT 110

# define PWRCYCNT_DEF 0
U32 CFG_Get_Power_Cycle_Counter(void);
S16 CFG_Set_Power_Cycle_Counter( U32 );

//
//	Configuration Parameter Countdown
//	most Operation Modes
//
# define BBV_LOC_COUNTDWN 114

# define COUNTDWN_DEF 15
# define COUNTDWN_MIN 3
# define COUNTDWN_MAX 3600
U16 CFG_Get_Countdown(void);
S16 CFG_Set_Countdown( U16 );
S16 CFG_Set_Countdown_AsString( char* );

//
//	Configuration Parameter APM_Command_Timeout
//	0 means no timeout
//
# define BBV_LOC_APMCMDTO 116

# define APMCMDTO_DEF 10
U16 CFG_Get_APM_Command_Timeout(void);
S16 CFG_Set_APM_Command_Timeout( U16 );
S16 CFG_Set_APM_Command_Timeout_AsString( char* );

//	Acquisition Parameters


//
//	Configuration Parameter Saturation_Counts
//	
//
# define BBV_LOC_SATURCNT 120

# define SATURCNT_DEF 50000
U16 CFG_Get_Saturation_Counts(void);
S16 CFG_Set_Saturation_Counts( U16 );
S16 CFG_Set_Saturation_Counts_AsString( char* );

//
//	Configuration Parameter Number_of_Clearouts
//	
//
# define BBV_LOC_NUMCLEAR 122

# define NUMCLEAR_DEF 3
U8 CFG_Get_Number_of_Clearouts(void);
S16 CFG_Set_Number_of_Clearouts( U8 );
S16 CFG_Set_Number_of_Clearouts_AsString( char* );

#endif /* CONFIG_H_ */
