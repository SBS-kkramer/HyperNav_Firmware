/*! \file power.h**********************************************************
 *
 * \brief System power management.
 * This modules handles all power management related tasks including:
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

#include "compiler.h"
#include "hypernav.sys.controller.h"
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

typedef struct PWRCFG_T
{
	Bool hasSupercaps;		// Indicates the presence of backup power
	Bool hasPCBSupervisor;	// Indicates the presence of PCB Supervisor circuitry

}powerConfig_t;

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
S16 pwr_lpSleep(struct timeval* time, wakeup_t* wuReason, hnv_sys_ctrl_param_t* reinitParameter, hnv_sys_ctrl_status_t* reinitStatus);


/*!	\brief	Put the system into USB sleep mode
	* @param	wakeTime		Optional wake-up time. Pass a NULL pointer if no wake-up time is to be set.
	* @param	wuReason		OUTPUT: Wake-up reason (PWR_EXTRTC_WAKEUP, PWR_TELEMETRY_WAKEUP, or PWR_UNKNOWN_WAKEUP).
	* @param	reinitParameter INPUT:	Contains arguments to sub-systems.
	* @param	reinitStatus	OUTPUT: Wake-up reinitialization status.
	* @param	fromAPItest		INPUT: set true if calling from API test function  - tweaks some operational settings
	* @return	PWR_OK: HyperNav system reinitialized successfully after waking-up.
	* @return 	PWR_FAIL: HyperNav system could not reinitialize properly after waking-up. Check 'nsyserrno'.
	* @note: Use this sleep mode only for long periods of sleeping. System will wake-up at the
	* set wake-up time or when the telemetry reception line is asserted. The sub-system will be
	* automatically reinitialized upon wake-up, and execution will resume at the line after this function call.
 */
S16 pwr_USBsleep(struct timeval* wakeTime, wakeup_t* wuReason, hnv_sys_ctrl_param_t* reinitParameter, hnv_sys_ctrl_status_t* reinitStatus, bool fromAPItest);



//! \brief Disconnect from battery power
//! @note This function is to be used to economize batery power and should only be called when alternate power is available.
S16 pwr_disconnectFromBatt(void);


//! \brief Self shutdown.
//! @note This function is to be used to shutdown the system. Motherboard will orderly de-init its resources and disconnect
//! itself from battery power.
void pwr_shutdown(void);


//! \brief Enable supercaps charging
//void pwr_enableSupercaps(void);


//! \brief Disable supercaps charging
//void pwr_disableSupercaps(void);


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


//!\brief Configure power subsystem
//! @param newPwrCfg A power configuration structure
void pwr_setConfig(powerConfig_t newPwrCfg);


//!\brief Retrieve power subsystem configuration
//! @param pwrCfg OUTPUT: Power configuration structure
void pwr_getConfig(powerConfig_t *pwrCfg);


portBASE_TYPE powerLossDetectedISR_non_naked(void) __attribute__ ((__noinline__));	// Power Loss detection non_naked behaviour


#endif /* POWER_H_ */
