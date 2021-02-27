/*! \file watchdog.h**********************************************************
 *
 * \brief Watchdog timer API
 *
 * @note Watchdog timer MUST be disabled before entering a 'Static' sleep mode,
 * and probably SHOULD be disabled before entering any other sleep mode.
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-17
 *
  ***************************************************************************/

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include "compiler.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define WDT_OK		0
#define WDT_FAIL	-1

//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Enable WDT with specified timeout period.
//! @note Timer resolution is very coarse so actual timeout will differ. (Tout = f_RC^-1 * 2^(PSEL+1)
//! where PSEL can take integer values from 0 to 35). Watchdog is not disabled after a WDT reset.
//! @return Actual timeout in us
U64 watchdog_enable(U64 timeout_us);

//! \brief Reenable WDT with last timeout period specified.
void watchdog_reenable(void);

//! \brief Disable WDT
void watchdog_disable(void);

//! \brief Clear WDT
void watchdog_clear(void);

//! \brief Check whether the last reset was triggered by the WDT
//! @note Useful to log WDT resets
Bool watchdog_tripped(void);

//!	\brief	returns true if watchdog currently enabled
Bool is_watchdog_enabled(void);

#endif /* WATCHDOG_H_ */
