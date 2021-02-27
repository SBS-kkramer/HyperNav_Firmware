/*! \file systemtime.spectrometer.h********************************************************
 *
 * \brief System time.
 *
 * The system time MUST be queried and set through 'gettimeofday()' and
 * 'settimeofday()' standard library functions (include 'time.h' and 'sys/time.h').
 *
 * The system time needs to be periodically synchronized with a stable external
 * reference (in this case the HyperNAV controller board) by calling 'time_ext2sys()' regularly. 
 * Assuming the MCU does not go into a Static sleep, the system time is maintained internally
 * for ~10 hours (high-resolution mode) or ~47 days (32 kHz operation) before it overflows.
 *
 * FOR HYPERNAV, HIGH RESOLUTION MODE IS REQUIRED
 *
 * Initial setting of the external time reference can be achieved by first setting the
 * system time via 'settimeofday()' and then calling 'time_sys2ext()'.
 *
 * @note The system timer works with a precision of ~8.7 us or ~1ms, however, the external
 *  and system clocks could be up to a second apart right after synchronization.
 *
 *	Based on code developed by Diego Sorrentino for the SUNA instrument
 *
 * @author      Scott Feener, Satlantic LP.
 * @date        2016-07-13
 *
  ***************************************************************************/

#ifndef SYSTEMTIME_SPECTROMETER_H_
#define SYSTEMTIME_SPECTROMETER_H_


#include "compiler.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define STIME_OK	0
#define STIME_FAIL	-1

//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Initialize internal system time module
S16 STIME_time_init(void);

//! \brief Synchronize system time with external RTC (ext_RTC->systime)
//!	@param	external time value (in seconds)
//! @return NTIME_OK: Success.	NTIME_FAIL: Failure, check 'nsyserrno'.
//S16 STIME_time_ext2sys(U32 extTime);

//! \brief Return internal system time (systime->ext_RTC)
U32 STIME_time_sys2ext(void);

F64 AST_Counter_to_Second( U32 counter );
U32 Second_to_AST_Counter( F64 second );

#endif /* SYSTEMTIME_SPECTROMETER_H_*/
