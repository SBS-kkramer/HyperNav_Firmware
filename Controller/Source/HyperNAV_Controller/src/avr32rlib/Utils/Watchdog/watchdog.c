/*! \file watchdog.c *************************************************************
 *
 * \brief Watchdog timer
 *
 * @note Watchdog timer MUST be disabled before entering a 'Static' sleep mode,
 * and probably SHOULD be disabled before entering any other sleep mode.
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-17
 *
 **********************************************************************************/

#include "compiler.h"
#include "wdt.h"
#include "pm.h"

#include "watchdog.h"



//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! \brief Enable WDT with specified timeout period.
U64 watchdog_enable(U64 timeout_us)
{
	return wdt_enable(timeout_us);
}

//! \brief Reenable WDT with last timeout period specified.
void watchdog_reenable(void)
{
	wdt_reenable();
}

//! \brief Enable WDT with specified timeout.
void watchdog_disable(void)
{
	wdt_disable();
}

//! \brief Clear WDT
void watchdog_clear(void)
{
	wdt_clear();
}

//! \brief Force a WDT reset immediately
void watchdog_resetMCU(void)
{
	wdt_reset_mcu();
}

//! \brief Check whether the last reset was triggered by the WDT
//! @note Useful to log WDT resets
Bool watchdog_tripped(void)
{
	volatile avr32_pm_t* pm = &AVR32_PM;

	return pm->RCAUSE.wdt;
}

Bool is_watchdog_enabled(void)
{
	return (AVR32_WDT.ctrl & AVR32_WDT_CTRL_EN_MASK);
}
