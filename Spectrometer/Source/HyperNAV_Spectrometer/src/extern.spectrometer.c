/*
 *  File: extern.c
 *  HyperNav Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

/*
 *  Define the globally accessible variables
 *  of the ISUS Firmware.
 *
 *  2004-Jul-30 BP: Initial version
 */

# include "extern.spectrometer.h"

# include <stdio.h>
# include <string.h>

# define GENERATE_BOARD_MESSAGE
# include "board.h"

bool  global_start_new_A_file = false;
bool  global_reset_was_power_up = false;

char glob_tmp_str[TMP_STR_LEN];

char const* buildVersion() {

	int month, day, year;
	char mStr[8];
	static const char month_names[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	sscanf( __DATE__, "%s %d %d", mStr, &day, &year);
	month = (strstr(month_names, mStr)-month_names)/3+1;

	static char ISO[32];
	snprintf(ISO, 32, "%d-%02d-%02dT" __TIME__, year, month, day);
	return ISO;
}
