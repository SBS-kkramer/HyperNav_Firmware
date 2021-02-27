/*
 *  File: extern.h
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

#ifndef EXTERN_H_
#define EXTERN_H_

# include "compiler.h"

/*
 *  The following are assigned in the file that contains the main() function
 */

//extern char* BFile;
//extern char* BDate;
//extern char* BTime;
char const* buildVersion();

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

//	Set these drives at startup. Normally,
//	these refer to EMMC_DRIVE and DATA_FLASH_DRIVE, respectively
//	If EMMC_DRIVE is not available,
//	try to get around by using DATA_FLASH_DRIVE instead.
//	If no drive is available, both pointers are NULL.

# define EMMC_DRIVE_CHAR '0'
//# define DATA_FLASH_DRIVE_CHAR '1'

# define EMMC_DRIVE       "0:\\"
//# define DATA_FLASH_DRIVE "1:\\"

#endif /* EXTERN_H_ */
