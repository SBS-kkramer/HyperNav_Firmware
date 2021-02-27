/*! \file power.spectrometer.h**********************************************************
 *
 * \brief System power.spectrometer management.
 * This modules handles all power.spectrometer management related tasks including:
 * - Low power sleep mode with wake-up on external RTC or telemetry activity
 * - Power source monitoring
 * - Appropriate internal MCU modules configuration for power saving
 * - Supercap
 * - Battery relay
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-12-08
 *
 * @revised
 * 2011-10-25	-DAS- Brushing up power loss handling
  ***************************************************************************/


#ifndef POWER_H_
#define POWER_H_

# if COMPILE_POWER_SPECTROMETER_C
#include "compiler.h"
#include "hypernav.sys.spectrometer.h"
#include "sys/time.h"
#include "FreeRTOS.h"

//*****************************************************************************
// Returned constants
//*****************************************************************************

#define PWR_OK		0
#define PWR_FAIL	-1

//*****************************************************************************
// Exported Data Types
//*****************************************************************************

// Wake-up events
typedef enum {PWR_UNKNOWN_WAKEUP, PWR_EXTRTC_WAKEUP, PWR_TELEMETRY_WAKEUP} wakeup_t;


//*****************************************************************************
// Exported functions
//*****************************************************************************

/*! \brief Initialize power
 * @note This function is called once upon system init.
 */
S16 pwr_init(void);


/*! \brief Put the system into low power sleep
* @param	wuTime			Optional wake-up time. Pass a NULL pointer if no wake-up time is to be set.
* @param	wuReason		OUTPUT: Wake-up reason (PWR_EXTRTC_WAKEUP, PWR_TELEMETRY_WAKEUP, or PWR_UNKNOWN_WAKEUP).
* @param	reinitParameter INPUT:	Contains arguments to sub-systems.
* @param	reinitStatus	OUTPUT: Wake-up reinitialization status.
* @return	PWR_OK: HyperNav system reinitialized successfully after waking-up.
* @return 	PWR_FAIL: HyperNav system could not reinitialize properly after waking-up. Check 'nsyserrno'.
* @note: Use this sleep mode only for long periods of sleeping. System will wake-up at the
* set wake-up time or when the telemetry reception line is asserted. The sub-system will be
* automatically reinitialized upon wake-up, and execution will resume at the line after this function call.
*/
S16 pwr_lpSleep(struct timeval* time, wakeup_t* wuReason, hnv_sys_spec_param_t* reinitParameter, hnv_sys_spec_status_t* reinitStatus);


//! \brief Self shutdown.
//! @note This function is to be used to shutdown the system. Motherboard will orderly de-init its resources and disconnect
//! itself from battery power.
void pwr_shutdown(void);


/*! \brief Disable power monitor emergency shutdown (ESD).
 * Disables self-shutdown on power monitor PWRGOOD de-assertion. To be used to temporarily disable self-shutdowns.
 * @note Use sparingly. For example: when it is known that a safe transient will occur. Re-enable immediately.
 */
void pwr_disableESD(void);


/*! \brief Enable power monitor emergency shutdown (ESD).
 * Enables self-shutdown on power monitor PWRGOOD de-assertion.
 */
void pwr_enableESD(void);


/*! \brief Brown-Out Reset routine
 *  @note This routine handles brown out resets. A call to this routine should be the first instruction executed by 'main()'
 */
void pwr_BOR(void);


portBASE_TYPE powerLossDetectedISR_non_naked(void) __attribute__ ((__noinline__));	// Power Loss detection non_naked behaviour

# endif

#endif /* POWER_H_ */
