/*! \file SatlanticHardware.h ********************************************************
 *
 * \brief Satlantic Hardware Definitions Fork File
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-12-10
 *
 *
 * History:
 *	2015-06-07, SF: Added E980030 definition
 **********************************************************************************/

 #ifndef _SATLANTICHARDWARE_H_
#define _SATLANTICHARDWARE_H_

# error
//#include "compiler.h"

#if (BOARD != USER_BOARD)
	#error "Compile symbol BOARD not set to USER_BOARD"
#endif
#ifndef MCU_BOARD_DN
	#error "MCU_BOARD_DN not defined"
#endif

/*! \name All MCU Boards Integer Identifiers
 *
 *  Use board drawing number, without revision.
 *
 *  Required compile symbol: MCU_BOARD_DN
 *  	e.g. MCU_BOARD_DN=E950061
 *
 *  Board revision (A-Z) is defined elsewhere with
 *  the MCU_BOARD_REV symbol.
 */
//! @{
# define UNKNOWN_MCU_BOARD	0	//!< unknown implementation - never use 0 for a good implementation!
# define E950061			1	//!< Nitrate Motherboard
# define E980030			2	//!< HyperNAV Controller Board
# define E980030_DEV_BOARD	3	//!< HyperNAV Controller Development board (SUNA main board)
// Add more as needed
//! @}

/*!	\name All Daughter Boards Identifiers
 *
 * 	If required for system implementation.
 * 	Use board drawing number, without revision.
 *
 * 	Required compile symbol: DAUGHTER_BOARD_DN
 *  	e.g. DAUGHTER_BOARD_DN=E950064
 */
//! @{
# define UNKNOWN_DAUGHTER_BOARD	0	//!< unknown implementation - never use 0 for a good implementation!
# define E950068				1	//!< Analog and SDI-12 daughter board
// Add more as needed
//! @}

/*! \name Board Revision Identifiers
 *
 *  Required compile symbol: MCU_BOARD_REV
 *  	e.g. MCU_BOARD_REV=REV_A
 *  Can also be used with adapter board,
 *  	e.g. DAUGHTER_BOARD_REV=REV_A
 */
//! @{
#define REV_UNKNOWN	0
#define REV_A		1
#define REV_B		2
#define REV_C		3
#define REV_D		4
#define REV_E		5
#define REV_F		6
#define REV_G		7
#define REV_H		9
#define REV_I		9
#define REV_J		10
// Add more as needed
//! @}


/*!	\name	MCU Board selections
 *
 */
//! @{

#if (MCU_BOARD_DN == E950061)
	#include	<Nitrate/E950061/E950061.h>
#elif (MCU_BOARD_DN == E980030)	|| (MCU_BOARD_DN == E980030_DEV_BOARD)
	#include	"E980030.h"
//#elif		// other boards
#else
	#error "Board not supported"
#endif
//! @}



#endif  // _SATLANTICHARDWARE_H_
