/*
 * version.controller.h
 *
 *  Created on: Nov 12, 2010
 *      Author: plache
 */

#ifndef _VERSION_CONTROLLER_H_
#define _VERSION_CONTROLLER_H_

# include "version.hypernav.h"

# define PROGRAM_NAME  "HyperNAV.Controller"
//# define HNV_CTRL_FW_VERSION_PATCH "0"  // Initial build
//# define HNV_CTRL_FW_VERSION_PATCH "1"    // Added Tx_Sts_Txed_00Percent
//# define HNV_CTRL_FW_VERSION_PATCH "2"    // Removed packet 0 from modem transfer - temporary test only
# define HNV_CTRL_FW_VERSION_PATCH "3"    // Reduce light per dark to 1 to reduce data rate. Had to much data for transfer.
                                          // Add "$MSG,TXRLT,DONE,0" after frame offloading

/*
 *  Identify custom versions:
 */

# define MAIN_VERSION                   0
# define TESTING_VERSION                998
# define DEVELOP_VERSION                999

/*
 *  Define the CUSTOM version,
 *  and below a distinct string for each CUSTOM_VERSION
 */

//# define CUSTOM_VERSION        MAIN_VERSION
# define CUSTOM_VERSION        TESTING_VERSION
//# define CUSTOM_VERSION        DEVELOP_VERSION

# if   CUSTOM_VERSION == MAIN_VERSION
#   define HNV_CTRL_CUSTOM        ""
# elif CUSTOM_VERSION == TESTING_VERSION
#   define HNV_CTRL_CUSTOM        ", Test Version"
# else
#   define HNV_CTRL_CUSTOM        ", Development Version"
# endif

# undef MAIN_VERSION
# undef TESTING_VERSION
# undef DEVELOP_VERSION

#endif /* _VERSION_CONTROLLER_H_ */
