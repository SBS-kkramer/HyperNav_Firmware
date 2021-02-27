/*! \file systemtime.c *************************************************************
 *
 * \brief System time utilities
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2010-11-16
 *
 **********************************************************************************/

#include "compiler.h"
#include "ds3234.h"
#include "rtc.h"
#include <sys/time.h>
#include "avr32rerrno.h"

#include "systemtime.h"


// Internal RTC
#define INT_RTC	(&AVR32_RTC)

// Internal RTC time resolution.
// Counts to seconds conversion is => time(s) = counts / 2^(INT_RTC_RES) OR time(s) = counts >> INT_RTC_RES
// Valid range = [-1,14]
// Ex.
// INT_RTC_RES = 10 -> 1s = 1024 counts
#define INT_RTC_RES	10



//*****************************************************************************
// Local Variables
//*****************************************************************************

// System time offset in seconds
static U32 nsystimeOffset = 0;



//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! Initialize internal RTC module & external RTC
S16 time_init(U32 fPBA)
{

#ifndef NO_RTC

	// Initialize internal RTC
	//
	// If Fosc 2^15 Hz = 32KHz = 32768Hz =>
	//
	// PSEL		fRTC	Seconds resolution (bits)
	//	0		16K		14
	//	1		8K		13
	//	2		4K		12
	//	...		...		...
	//	15		1/2		-1
	//
	// => PSEL = 14 - INT_RTC_RES
	if(!rtc_init(INT_RTC, RTC_OSC_32KHZ, 14-INT_RTC_RES))
	{
		avr32rerrno = EIRTC;
		return NTIME_FAIL;
	}

	// Enable the internal RTC
	rtc_enable(INT_RTC);

#endif

	// Initialize external RTC
	if(ds3234_init(fPBA) != DS3234_OK)
	{
		avr32rerrno = EERTC;
		return NTIME_FAIL;
	}

	// Clear any pending alarm state in the RTC
	if(ds3234_ackAlarm()!= DS3234_OK)
	{
		avr32rerrno = EERTC;
		return NTIME_FAIL;
	}

	// Retrieve time from external RTC and return
	return time_ext2sys();
}


//! \brief Synchronize system time with external RTC (ext_RTC->systime)
S16 time_ext2sys(void)
{
	U32 extTime;

	// Read external RTC
	if(ds3234_getTime(&extTime) != DS3234_OK)
	{
		avr32rerrno = EERTC;
		return NTIME_FAIL;
	}
	else
	{
		// Set offset to ext RTC time
		nsystimeOffset = extTime;
		// Reset systime counter
		rtc_set_value(INT_RTC, 0);

		return NTIME_OK;
	}
}


//! \brief Load external RTC with internal system time (systime->ext_RTC)
S16 time_sys2ext(void)
{
	U32 systime;

	// Get internal time in seconds
	systime = (rtc_get_value(INT_RTC) >> INT_RTC_RES) + nsystimeOffset;

	// Set the external RTC
	if(ds3234_setTime(systime) != DS3234_OK)
	{
		avr32rerrno = EERTC;
		return NTIME_FAIL;
	}
	else
		return NTIME_OK;
}


// Overriding '_gettimeofday()'
int _gettimeofday(struct timeval *__p, void *__tz);

int _gettimeofday(struct timeval *__p, __attribute__((unused)) void *__tz)
{
	U32 sysrtcCounts = rtc_get_value(INT_RTC);
	U32 subsecondCounts;

	// Sanity check
	if(__p == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	__p->tv_sec = (sysrtcCounts >> INT_RTC_RES) + nsystimeOffset;

	subsecondCounts = sysrtcCounts % (1 << INT_RTC_RES);

	__p->tv_usec = 1000000 * ((F64)subsecondCounts /(1 << INT_RTC_RES));

	return 0;
}


// Defining 'settimeofday()'
int settimeofday(const struct timeval *tv , __attribute__((unused)) const struct timezone *tz)
{
	U32 subsecondCounts;

	// Sanity check
	if(tv == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	// Calculate sub-second counts
	subsecondCounts = ((F64)tv->tv_usec / 1000000.0) * (1 << INT_RTC_RES);

	// Store seconds in the offset and reset the internal RTC to the sub-second counts
	nsystimeOffset = tv->tv_sec;
	rtc_set_value(INT_RTC, subsecondCounts);

	// Update external RTC time
	time_sys2ext();

	return 0;
}


