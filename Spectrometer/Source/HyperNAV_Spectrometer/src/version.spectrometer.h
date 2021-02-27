/*
 * version.spectrometer.h
 *
 *  Created on: Nov 12, 2010
 *      Author: plache
 */

#ifndef _VERSION_SPECTROMETER_H_
#define _VERSION_SPECTROMETER_H_

# include "version.hypernav.h"

# define PROGRAM_NAME  "HyperNAV.Spectrometer"
# define HNV_SPEC_FW_VERSION_PATCH "0"

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

# define CUSTOM_VERSION        MAIN_VERSION
//# define CUSTOM_VERSION        TESTING_VERSION
//# define CUSTOM_VERSION        DEVELOP_VERSION

# if   CUSTOM_VERSION == MAIN_VERSION
#   define HNV_SPEC_CUSTOM        ""
# elif CUSTOM_VERSION == TESTING_VERSION
#   define HNV_SPEC_CUSTOM        ", Test Version"
# else
#   define HNV_SPEC_CUSTOM        ", Development Version"
# endif

# undef MAIN_VERSION
# undef TESTING_VERSION
# undef DEVELOP_VERSION

#endif /* _VERSION_SPECTROMETER_H_ */
