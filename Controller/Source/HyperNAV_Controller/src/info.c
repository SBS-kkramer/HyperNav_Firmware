/*
 * info.c
 *
 *  Created on: Dec 14, 2010
 *      Author: plache
 */

# include "info.h"

# include <stdio.h>
# include <string.h>

# include "usb_drv.h"
# include "usb_standard_request.h"

# include "avr32rerrno.h"
# include "files.h"
# include "gpio.h"
# include "power.h"
# include "watchdog.h"

# include "task.h"

# include "extern.controller.h"
# include "config.controller.h"
# include "io_funcs.controller.h"
# include "version.controller.h"
# include "wavelength.h"

void Info_SensorID ( fHandler_t* fh, char* prefix ) {

	strcpy ( glob_tmp_str, prefix );
	size_t const used = strlen(glob_tmp_str);
	char* const content = glob_tmp_str + used;
	size_t const remaining = sizeof(glob_tmp_str) - used;

    snprintf( content, remaining, "%s Serial    Number , SN:%04hu\r\n",
			CFG_Get_Sensor_Type_AsString(), CFG_Get_Serial_Number() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

    snprintf( content, remaining, "Application      Name , %s-%s\r\n",
			CFG_Get_Sensor_Type_AsString(), CFG_Get_Sensor_Version_AsString() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	snprintf( content, remaining, "Firmware      Version , " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH HNV_CTRL_CUSTOM "\r\n" );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	snprintf( content, remaining, "App Build        Date , %s\r\n", buildVersion() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
	snprintf ( content, remaining, "Operating        Mode , %s\r\n", CFG_Get_Operation_Mode_AsString() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
# endif
}

bool Info_SensorState ( fHandler_t* fh, char* prefix ) {

	strcpy ( glob_tmp_str, prefix );
	size_t const used = strlen(glob_tmp_str);
	char* const content = glob_tmp_str + used;
	size_t const remaining = sizeof(glob_tmp_str) - used;

	char const* DateTimeFormat = DATE_TIME_APF_FORMAT;	//	Or DATE_TIME_ISO_FORMAT?

	snprintf( content, remaining, "Power Up Time   (UTC) , %s\r\n",
					str_time_formatted ( CFG_Get_System_Reset_Time(), DateTimeFormat) );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	time_t current_time = time((time_t*)0);
	snprintf( content, remaining, "Current  Time   (UTC) , %s\r\n",
					str_time_formatted ( current_time, DateTimeFormat) );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	snprintf( content, remaining, "Power Cycle   Counter , %lu\r\n", CFG_Get_Power_Cycle_Counter() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	snprintf( content, remaining, "System Reset  Counter , %lu\r\n", CFG_Get_System_Reset_Counter() );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

# if 0
	snprintf( content, remaining, "Alarm    Time   (GMT) , %s\r\n",
					str_time_formatted ( wakeup_sec, DateTimeFormat) );
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
# endif

	return true;
}

bool Info_SensorDevices ( fHandler_t* fh, char* prefix ) {

	strcpy ( glob_tmp_str, prefix );
	size_t const used = strlen(glob_tmp_str);
	char* const content = glob_tmp_str + used;
	size_t const remaining = sizeof(glob_tmp_str) - used;

	bool all_ok = true;
//	char ErrChar = ' ';

	//physical_t ph;
	//electrical_t el;

	//	Start a measurement, repeat a bit later
	//  because the very first measurement can be invalid.

	//measure_physical   ( &ph );
	//measure_electrical ( &el );

	struct timeval time;
	gettimeofday(&time, NULL);
	strftime( content, remaining, "System time           , %Y-%m-%dT%H:%M:%SZ\r\n",gmtime(&time.tv_sec));
	fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	//	Check for availability of flash memory drive and eMMC memory.
	//	eMMC may be switched off, depending on factory configuration.
	S16 drive = 0;

    U64 total;
    U64 avail;

    if ( FILE_OK == f_space ( '0'+drive, &avail, &total ) ) {
   		snprintf ( content, remaining,
   			"Disk      Size : Free , %llu : %llu\r\n", total, avail );
    } else {
    	snprintf ( content, remaining, "Disk not available!\r\n" );
    	all_ok = false;
    }
    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

	/*
	 *  Some info on the physical state of the instrument
	 */

	//char flStr[32];

	//	The One-wire temperature sensors have a response time
	//	of up to 720 ms. Waited long enough to reasonably
	//	expect good values.

    return all_ok;
}

void Info_ProcessingParameters ( fHandler_t* fh, char* prefix ) {

	strcpy ( glob_tmp_str, prefix );
	size_t const used = strlen(glob_tmp_str);
	char* const content = glob_tmp_str + used;
	size_t const remaining = sizeof(glob_tmp_str) - used;

		S16 side;
		for ( side=0; side<2; side++ ) {
	
		  S16 nz = wl_NumCoefs( side );
		  if( nz > 0){
			snprintf( content, remaining, "Zeiss Coefficient Vals");
			S16 i;
			for(i = 0; i < nz; i++){
				snprintf ( glob_tmp_str+strlen(glob_tmp_str), sizeof(glob_tmp_str)-strlen(glob_tmp_str), ", %e",
						printAbleF64(wl_GetCoef (side, i) ) );
			}
			snprintf ( glob_tmp_str+strlen(glob_tmp_str), sizeof(glob_tmp_str)-strlen(glob_tmp_str), "\r\n" );
		    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
		  } else {

			snprintf( content, remaining, "Zceof File      Error , ZCoefs = %hd\r\n", nz );
		    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
		  }
		}
# if 0
		snprintf( content, remaining, "Spec Pre  scans taken , %hu\r\n", CFG_GetNumberOfPreDataScans() );
	    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

		snprintf( content, remaining, "Spec Lite scans taken , %hu\r\n", CFG_GetNumberOfLightAvgs() );
	    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);

		snprintf( content, remaining, "Spec Dark scans taken , %hu\r\n", CFG_GetNumberOfDarkAvgs() );
	    fh ? f_write ( fh, glob_tmp_str, strlen(glob_tmp_str) ) : io_out_string(glob_tmp_str);
# endif

}
