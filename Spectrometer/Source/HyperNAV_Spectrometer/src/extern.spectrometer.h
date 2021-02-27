/*
 *  File: extern.h
 *  HyperNav Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

#ifndef EXTERN_H_
#define EXTERN_H_

# include <time.h>
# include <stdio.h>

# include "compiler.h"

/*
 *  The following are assigned in the file that contains the main() function
 */

char const* buildVersion(void);

//	This global flag is used to determine
//	if a new 'A'-style data log file has to be started.
//	'A'-style files are per 'A'cquisition files, typically for profiles.
//	Power-cycling triggers the use of a new file number.
extern bool  global_start_new_A_file;
extern bool  global_reset_was_power_up;

//	Use this array whenever a message
//  is generated to be sent out immediately.
# define TMP_STR_LEN 256
extern char glob_tmp_str[TMP_STR_LEN];

#endif /* EXTERN_H_ */
