/*
 * info.c
 *
 *  Created on: Dec 14, 2010
 *      Author: plache
 */

# include "info.spectrometer.h"

# include <stdio.h>
# include <string.h>
# include <time.h>
# include <sys/time.h>

# include "extern.spectrometer.h"
# include "config.spectrometer.h"

# include "io_funcs.spectrometer.h"

# include "version.spectrometer.h"

void Info_SensorID () {

	size_t const remaining = sizeof(glob_tmp_str);

	snprintf( glob_tmp_str, remaining, "Firmware      Version , " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_SPEC_FW_VERSION_PATCH HNV_SPEC_CUSTOM "\r\n" );
    io_out_string(glob_tmp_str);
}

bool Info_SensorState () {

	size_t const remaining = sizeof(glob_tmp_str);

	char const* DateTimeFormat = "%Y-%m-%dT%H:%M:%SZ";	//	Or DATE_TIME_ISO_FORMAT?

	time_t current_time = time((time_t*)0);
	snprintf( glob_tmp_str, remaining, "Current  Time   (UTC) , %s\r\n",
					str_time_formatted ( current_time, DateTimeFormat) );
    io_out_string(glob_tmp_str);

	return true;
}

bool Info_SensorDevices () {

	size_t const remaining = sizeof(glob_tmp_str);

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
	strftime( glob_tmp_str, remaining, "System time           , %Y-%m-%dT%H:%M:%SZ\r\n",gmtime(&time.tv_sec));
	io_out_string(glob_tmp_str);

	/*
	 *  Some info on the physical state of the instrument
	 */

	//char flStr[32];

	//	The One-wire temperature sensors have a response time
	//	of up to 720 ms. Waited long enough to reasonably
	//	expect good values.

//	S16 max = 10;
//	delay_ms ( 600 );

//	do {
//		delay_ms ( 150 );
//		measure_physical   ( &ph );
//		measure_electrical ( &el );
//	} while ( ( !ph.tempHousing_valid
//		     || !ph.tempSpec_valid
//		     || !ph.humidity_valid )
//		   && max-- > 0 );

	//
//	if ( !ph.tempHousing_valid    || -10.0 > ph.tempHousing    || ph.tempHousing    > 45.0
//	  || !ph.tempSpec_valid       || -10.0 > ph.tempSpec       || ph.tempSpec       > 45.0
//	  || !ph.tempLampOrTfrm_valid || -10.0 > ph.tempLampOrTfrm || ph.tempLampOrTfrm > 45.0 ) {
//		all_ok = false;
//		ErrChar = '!';
//	} else if ( ph.tempHousing    > 35.0
//			 || ph.tempSpec       > 35.0
//			 || ph.tempLampOrTfrm > 35.0 ) {
//				ErrChar = '*';
//			}

//	snprintf( glob_tmp_str, remaining, "Temperatures Hs Sp Lm , %.1f %.1f %.1f%c\r\n",
//			ph.tempHousing_valid ? printAbleF32(ph.tempHousing) : -99.9,
//			ph.tempSpec_valid ? printAbleF32(ph.tempSpec) : -99.9,
//			ph.tempLampOrTfrm_valid ? printAbleF32(ph.tempLampOrTfrm) : -99.9,
//		    ErrChar ); ErrChar = ' ';

//    io_out_string(glob_tmp_str);

	//	Humidity

//	if ( ph.humidity_valid && ph.humidity > 90.0 ) {
//		all_ok = false;
//		ErrChar = '!';
//	} else if ( ph.humidity_valid && ph.humidity > 50.0 ) {
//		ErrChar = '*';
//	}

//	snprintf( glob_tmp_str, remaining, "Humidity              , %.1f%c\r\n",
//			ph.humidity_valid ? printAbleF32(ph.humidity) : -99.9,
//		    ErrChar ); ErrChar = ' ';
//    io_out_string(glob_tmp_str);
//

	//  FIXME: Add pressure sensor test
	//  FIXME: Add tilt/heading sensor test

    //	Do not check voltages unless main power supplied.
    //	Low main power implies running from USB.

    return all_ok;

}

