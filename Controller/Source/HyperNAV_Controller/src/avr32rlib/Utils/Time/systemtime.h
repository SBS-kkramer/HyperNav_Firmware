/*! \file systemtime.h********************************************************
 *
 * \brief System time.
 *
 * The system time MUST be queried and set through 'gettimeofday()' and
 * 'settimeofday()' standard library functions (include 'time.h' and 'sys/time.h').
 *
 * The system time needs to be periodically synchronized with a stable external
 * reference (external RTC) by calling 'time_ext2sys()' regularly. Assuming the
 * MCU does not go into a Static sleep, the system time is maintained internally
 * for ~47 days before it overflows.
 *
 * Initial setting of the external time reference can be achieved by first setting the
 * system time via 'settimeofday()' and then calling 'time_sys2ext()'.
 *
 * @note The system timer works with a precision of ~1ms, however, the external
 *  and system clocks could be up to a second apart right after synchronization.
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-16
 *
  ***************************************************************************/


#ifndef NSYSTIME_CFG_H_
#define NSYSTIME_CFG_H_

#include "compiler.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define NTIME_OK	0
#define NTIME_FAIL	-1


//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Initialize internal system time module (internal RTC & external RTC)
//! @param fPBA	PBA bus clock frequency (Hz)
S16 time_init(U32 fPBA);

//! \brief Synchronize system time with external RTC (ext_RTC->systime)
//! @return NTIME_OK: Success.	NTIME_FAIL: Failure, check 'nsyserrno'.
S16 time_ext2sys(void);

//! \brief Load external RTC with internal system time (systime->ext_RTC)
//! @return NTIME_OK: Success.	NTIME_FAIL: Failure, check 'nsyserrno'.
S16 time_sys2ext(void);



#endif /* NSYSTIME_CFG_H_ */
