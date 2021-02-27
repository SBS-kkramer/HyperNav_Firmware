/*
 *  File: numerical.c
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

/*
 *  All functions related to converting the ISUS spectrum
 *  into a nitrate value.
 *
 *   * I/O and use of extinction coefficients and reference spectrum
 *   * Calculating averages, absorbances, ...
 *
 *  2004-Jul-30 BP: Initial version
 */

# if 0

# include "numerical.h"

# include "extern.h"

//	The following defines the location of the ASCII cal files,
//	visible to the sensor user.
# define CALDIR	"CAL"

//!	\brief	Generate cal file directory name.
//!
//!	return	pointer to static char[]	if file name was generated
//!			0							if name could not be generated
char* NUM_MakeCalDirName () {

	return   EMMC_DRIVE CALDIR;
}

# if 0

# include <ctype.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <math.h>
# include <sys/stat.h>

# include <assert.h>
# include <compiler.h>

# include <time.h>
# include <sys/time.h>

# include "board.h"

/*
 *  Driver APIs
 */

# include "files.h"
# include "syslog.h"
# include "watchdog.h"

/*
 *  Firmware
 */

# include "config.h"
# include "io_funcs.h"

# include "matrix_processing.h"
# include "dynamic_memory.h"
# include "zcoef.h"


// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif

# define ELC_NO_CALFILE			0x0801
# define ELC_BAD_CALFILE		0x0802
# define ELC_PROCESSING			0x0811

//	These files reside on the internal_drive,
//	invisible to the sensor user.
# define CALFILE_BINARY_NAME	"cal_bin.dat"
# define CALFILE_TEMPRY_NAME	"cal_bin.tmp"
# define CALFILE_BACKUP_NAME	"cal_bin.bck"

static U32 const Binary_Version = 4;

# define CAL_DATE_LEN 32
typedef struct num_cal_file_in_memory {

	bool is_valid;							//  When this structure is initialized, set to false.
											//	When structure is filled (read from binary file), set to true.
	S16	num_species;						//	The actual number of species in the file.

	S16	nitrate_species;					//	Index of nitrate coefficients [default=1 MBARI]
	S16	seawater_species;					//  Index of seawater coefficients [default=0 MBARI]
	S16	temp_sw_species;					//	Index of temp*sw coefficients [default=2 Satlantic, N/A for MBARI]
	S16	hs_species;							//	Index of HS coefficients [default=2 for MBARI]

	F64 wavelength [SPEC_PIXELS];
	F64 ex_coeff [NUM_MAX_SPECIES][SPEC_PIXELS];
	F64 reference  [SPEC_PIXELS];			//	The DIW Spectrum (dark corrected)

	F64 tempCal;							//	Temperature at calibration, via
	char calFileDate[CAL_DATE_LEN];	//  As a string.
	U16	integrationPeriod;					//	Spectrometer Integration Period at calibration, via

	char filename [16];	//	<8.3>-name

	// Added five items at Firmware Version 2.3.0 / Binary_Version 4

	S16	tsCorrectable;						//	Either 0 (false) or 1 (true)
											//	specifies if the calibration was done for T-S-Correction (i.e., using true LNSW).
	F64	nitrateCorrectionFactor;			//	Factor applied to nitrate concentration after processing,
											//	only use for freshwater processing.
	F64	nitrateConcentration;				//	Nitrate concentration [in uM] during calibration.
											//	For Freshwater Calibration using two samples: = sqrt(conc1*conc2)
											//	For Seawater Calibration: The nitrate concentration
	F64	seawaterConcentration;				//	For Seawater Calibration: The salinity in PSU.
											//	For Freshwater Calibration: Missing
	S16	opticalPathLength;					//	Either 5 or 10 [mm].

} num_cal_file_in_memory;

static num_cal_file_in_memory num_cal_file = { false };


/*
 *  Functions to fake a CAL file
 */

static void genericENO3 ( F64 wl[], F64 eno3[], int n ) {

    const F64 E0 = 0.0086366;
    const F64 L0 = 204.45;
    const F64 SG = 16.362;

    int i;
    for ( i=0; i<n; i++ ) {
    	const F64 dL = wl[i] - L0;
    	const F64 dL_SG = dL / SG;
    	eno3[i] = E0 * exp ( -dL_SG*dL_SG );
    	if ( eno3[i] < 1e-10 ) eno3[i] = 0;
    }
}

static void genericESWA ( F64 wl[], F64 T, F64 eswa[], int n ) {

    const F64 C0 =   1.14728;
    const F64 C1 =  -0.0563405;
    const F64 L0 = 172.88;
    const F64 L1 =   0.391586;
    const F64 S0 =  16.8451;
    const F64 S1 =  -0.0417979;

    const F64 CT = C0 + C1*T;
    const F64 LT = L0 + L1*T;
    const F64 ST = S0 + S1*T;

    int i;
    for ( i=0; i<n; i++ ) {
    	const F64 dL = wl[i] - LT;
    	const F64 dL_SG = dL / ST;
    	eswa[i] = exp ( CT - dL_SG*dL_SG );
    	if ( eswa[i] < 1e-10 ) eswa[i] = 0;
    }

}

/**
 *!	\brief	Read CAL file to memory. Scan file content for data & place into array arguments.
 */
static bool num_parseCalFile (

	num_cal_file_in_memory* cal_file,
	fHandler_t fh ) {

	S32 f_size = f_getSize ( &fh );

	if ( FILE_FAIL == f_size ) {
		SYSLOG ( SYSLOG_ERROR, "Empty file." );
		return false;
	}

	U16 const block_size = 224;	//	Make longer than the longest expected line

	char* f_data = pvPortMalloc ( 2*block_size+1 );

	if ( !f_data ) {
		io_out_heap ( "NUM" );
		SYSLOG ( SYSLOG_ERROR, "Out of memory %ld.", 2*block_size+1 );
		return false;
	}

	char* const block_1 = f_data;
	char* const block_2 = f_data + block_size;

	S32 remaining_size = f_size;
	U16 to_read = remaining_size > 2*block_size ? 2*block_size : (U16)remaining_size;

	if ( to_read != f_read ( &fh, f_data, to_read ) ) {
		SYSLOG ( SYSLOG_ERROR, "Cannot read data." );
		vPortFree ( f_data );
		return false;
	}

	//	Initialize cal file structure so missing values are at acceptable values.
	cal_file->tempCal = 0;
	memset ( cal_file->calFileDate, 0, CAL_DATE_LEN );
	cal_file->integrationPeriod = 0;
	// Added five items at Firmware Version 2.3.0 / Binary_Version 4
	cal_file->tsCorrectable = 0;
	cal_file->nitrateCorrectionFactor = 0;
	cal_file->nitrateConcentration = 0;
	cal_file->seawaterConcentration = 0;
	cal_file->opticalPathLength = 0;

	//  Terminate data with an additional NUL character.
	//  This makes scanning for lines with an automatic
	//  end-of-data marker easier.
	f_data [ to_read ] = 0;
	remaining_size -= to_read;

	char* next_start = f_data;

	//  The very last line from the header is needed
	//  to interpret the content of the data below.
	//  We handle this by keeping a pointer to the
	//  most recent (header) line, and when finding
	//  the first data line, we know that the previous
	//  line is the last header line.
	bool need_column_information = true;
	char previous_line_copy[128];
	U16  num_columns = 0;
	U16  channel = 0;

	bool parsing_state = true;

	//  Start scanning the file content for individual lines
	char* this_line = scan_to_next_EOL ( next_start, &next_start );

	while ( this_line ) {

		//io_out_string( "Parse " );
		//io_out_S32( next_start-f_data, ", " );
		//io_out_S32( remaining_size, ": '" );
		//io_out_string( this_line );
		//io_out_string( "'\r\n" );

		if ( 'H' == this_line[0] && ',' == this_line[1] ) {

			//  Check for and extract key values
			//  The (key, value) pair delimiters are inconsistent.
			//  Thus, include the delimiter (space, comma) in the key string.

			char* const key[] = {
				"T_CAL_SWA ",	//	Sample temperature during calibration
				"T_CAL_FWA ",	//	Sample temperature during calibration
				"INT_PERIOD ",
				"File creation time ",
				"Optical Path Length,",	//	Path length in mm
				"CONC_CAL_SWA ",		//	PSU of seawater during calibration
				"CONC_CAL_NO3 ",		//	Concentration of nitrate during calibration
				"NO3_CORRECTION ",		//	Correction factor in freshwater
				"T_S_CORRECTABLE"		//	Keyword present or absent
			};
			const S16 nKeys = 9;

			int k;
			for ( k=0; k<nKeys; k++ ) {
				char* pos = strstr ( this_line, key[k] );
				if ( pos ) {
					char* token = pos + strlen(key[k]);
					switch(k) {
					case 0:
					case 1:	cal_file->tempCal = atof(token); break;
					case 2: cal_file->integrationPeriod = atol (token); break;
					case 3:	if ( 0 == cal_file->calFileDate[0] ) {	// Take the topmost entry
								strncpy ( cal_file->calFileDate, token, CAL_DATE_LEN-1 ); }
							break;
					case 4: cal_file->opticalPathLength = atol (token); break;
					case 5:	cal_file->seawaterConcentration = atof (token); break;
					case 6:	cal_file->nitrateConcentration = atof (token); break;
					case 7:	/* cal_file->nitrateCorrectionFactor = atof (token); */ break;
					case 8:	cal_file->tsCorrectable = 1; break;
					default: break;	// cannot happen
					}
					break;	//	After matching key is found, drop out of loop. - Only minor execution time efficiency, not necessary for correct code behaviour.
				}
			}

			//  Make a copy of the most recent line
			strncpy ( previous_line_copy, this_line, sizeof(previous_line_copy)-1);

		} else if ( 'E' == this_line[0] && ',' == this_line[1] ) {

			if ( need_column_information ) {

				if ( 0 == strncasecmp ( "H,W", previous_line_copy, 3 ) ) {

					//  This is a Satlantic CAL file.
					//  Determine number of columns and their content from file
					cal_file->nitrate_species = -1;
					cal_file->seawater_species = -1;
					cal_file->temp_sw_species = -1;
					cal_file->hs_species = -1;

					//  Count number of ',' in previous line.
					//  Assume that the data columns are
					//  E,Wavelength,Ex0,Ex1,...,ExN,Reference
					//  and use the first three extinction
					//  coefficients (ignore the rest).


					//	Forward to second comma, the first was for wavelength

					num_columns = 0;	//  Wavelength column
					char* plp = strchr ( previous_line_copy+2, ',' );

					while ( plp && *plp ) {

						//	previous_line points to the comma
						//	advance one position and increment num_columns

						num_columns++;
						plp++;

						//  Some cal files have an 'E' character in front of the column heading,
						//  e.g., ENO3,ESWA,...
						//  whereas others use NO3,SWA,...
						if ( 'E' == *plp ) plp++;

						//  See what the next label starts with,
						//	and use to determine the content of the following column.
						//  FIXME SILENT ASSUMPTION: No space after comma.
						//  FIXME SILENT ASSUMPTION: First character uniquely determines species.

						switch ( *plp ) {
						case 'n':
						case 'N':	cal_file->nitrate_species = num_columns-1; break;
						case 'a':
						case 'A':
						case 's':
						case 'S':	cal_file->seawater_species = num_columns-1; break;
						case 't':
						case 'T':	cal_file->temp_sw_species = num_columns-1; break;
						case 'h':
						case 'H':	cal_file->hs_species = num_columns-1; break;
						default :   /*  Unknown substance or Ref/DIW  */ break;
						}

						plp = strchr ( plp, ',' );
					}

					num_columns++;

					//  Species = Columns excluding first (wavelength) and last (reference)
					cal_file->num_species = num_columns - 2;

					if ( cal_file->num_species > NUM_MAX_SPECIES ) {
						CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
						CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
						CFG_Set_Last_Error_Time ( time((time_t*)0) );
						SYSLOG ( SYSLOG_ERROR, "More than 3 chemicals in CAL file. Use only 1 to 3." );

						cal_file->num_species = NUM_MAX_SPECIES;
						parsing_state = false;
					}
				} else {
					//  This is a MBARI CAL file
					//  Number of columns and content are pre-defined.
					num_columns = 5;
					cal_file->num_species = 3;

					cal_file->nitrate_species = 1;
					cal_file->seawater_species = 0;
					cal_file->temp_sw_species = -1;
					cal_file->hs_species = 2;
				}

				//  this previous_line is no longer needed.
				need_column_information = false;
			}

			//  Scan the this_line line,
			//  and assign numbers to arrays.
			//  Each line format is "E,234.5,0.0123,0.0321,0.00987,12345.6"
			//  for a file containing wavelength, three extinction coefficients,
			//  and the reference spectrum.

			U16   col = 0;
			char* pos = this_line+1;	//	Point to the very first ','

			while ( pos && *pos ) {

				col++;
				pos++;

				if ( 1 == col ) {
					cal_file->wavelength[channel] = atof ( pos );
				} else if ( num_columns == col ) {
					cal_file->reference[channel] = atof ( pos );
				} else if ( col < num_columns ) {
					S16 species = col - 2;
					if ( species < NUM_MAX_SPECIES ) {
						cal_file->ex_coeff[species][channel] = atof ( pos );
					} else {
						//  Cannot utilize, ignore (or report error)?
					}
				} else {
					//  Unexpected column
					CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
					CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
					CFG_Set_Last_Error_Time ( time((time_t*)0) );
					SYSLOG ( SYSLOG_ERROR, "Unexpected column in CAL file." );
					parsing_state = false;
				}

				pos = strchr ( pos, ',' );
			}

			if ( col != num_columns ) {
				CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
				CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
				CFG_Set_Last_Error_Time ( time((time_t*)0) );
				SYSLOG ( SYSLOG_ERROR, "Missing column %hu (of %hu) at channel %hu.", col, num_columns, channel );
				parsing_state = false;
			}

			channel ++;

		} else {

			if ( this_line[0] == '\032' ) {
				//  This is an XMODEM transfer artifact. Ignore.
			} else {
				CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
				CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
				CFG_Set_Last_Error_Time ( time((time_t*)0) );
				SYSLOG ( SYSLOG_ERROR, "Unexpected line type '0x%hx' in CAL file.", (U16)this_line[0] );
				parsing_state = false;
			}
		}

		if ( next_start > block_2 && remaining_size ) {

			//	Move the second block into the first block,
			//	and adjust the current position.
			memcpy ( block_1, block_2, block_size );
			next_start -= block_size;

			U16 to_read = remaining_size > block_size ? block_size : (U16)remaining_size;
			if ( to_read != f_read ( &fh, block_2, to_read ) ) {
				SYSLOG ( SYSLOG_ERROR, "Cannot read data." );
				vPortFree ( f_data );
				return false;
			}

			block_2[ to_read ] = 0;
			remaining_size -= to_read;
		}

		this_line = scan_to_next_EOL ( next_start, &next_start );
	}

	vPortFree ( f_data );

	//  Confirm that wavelength agree with ZCOEFs

	if ( zc_NumCoefs() > 0 ) {
		S16 numDiffs = 0;
		if ( ZC_OK == zc_verifyWL ( cal_file->wavelength, channel, &numDiffs ) ) {
			if ( numDiffs > 0 ) {
				CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
				CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
				CFG_Set_Last_Error_Time ( time((time_t*)0) );
				SYSLOG ( SYSLOG_ERROR, "Wavelength mismatch in %d of %d "
						"channels between CAL file and ZCoeffs.", numDiffs, channel );
				parsing_state = false;
			}
		}
	}

	if ( channel < CFG_GetFullFrameChannelEnd() ) {
		CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
		CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
		CFG_Set_Last_Error_Time ( time((time_t*)0) );
		SYSLOG ( SYSLOG_ERROR, "Only %hu channels in CAL file.", channel );
		parsing_state = false;
	}

	//  Pad the arrays.
	//  This avoids having random or non-float values
	//  in nominally float locations.
	while ( channel < SPEC_PIXELS ) {

		cal_file->wavelength [channel] = 999.0;  // Flag invalid by strange value
		S16 species;
		for ( species=0; species<NUM_MAX_SPECIES; species++ ) {
			cal_file->ex_coeff[species][channel] = cal_file->ex_coeff[species][channel-1];
		}
		cal_file->reference[channel] = 63999.0;

		channel++;
	}

	if ( 3 == cal_file->num_species ) {

		//	MBARI cal files may contain all zeroes in the 3rd species.
		//	That is to be treated as a 2-species cal file.
		bool all_zero = true;
		S16 channel = 0;
		while ( all_zero && channel < SPEC_PIXELS ) {
			all_zero = ( 0.0 == cal_file->ex_coeff[2][channel] );
			channel++;
		}

		if ( all_zero ) {
			cal_file->num_species = 2;
			cal_file->hs_species = -1;
		}
	}

	//	To make really sure
	if ( -1 == cal_file->hs_species
	  && -1 == cal_file->temp_sw_species
	  && -1 != cal_file->seawater_species ) {
		cal_file->num_species = 2;
	}

	if ( cal_file->integrationPeriod
	  && cal_file->integrationPeriod != CFG_Get_Spectrometer_Integration_Period() ) {
		SYSLOG ( SYSLOG_WARNING, "Mismatch of spectrometer integration period %hd %hd",
			cal_file->integrationPeriod, CFG_Get_Spectrometer_Integration_Period() );
	}

	return parsing_state;
}

static bool num_write_CAL_binary_basic( num_cal_file_in_memory* cal_file, char* destination_file_name ) {

	fHandler_t fh;
	S16 const f_state = f_open ( destination_file_name, O_WRONLY | O_CREAT, &fh );

	if ( FILE_FAIL == f_state ) {
		return false;
	}

	bool ok_write = true;

	if ( sizeof(U32) != f_write ( &fh, &Binary_Version, sizeof(U32) ) ) ok_write = false;

	if ( sizeof(S16) != f_write ( &fh, &cal_file->num_species, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(S16) != f_write ( &fh, &cal_file->nitrate_species, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(S16) != f_write ( &fh, &cal_file->seawater_species, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(S16) != f_write ( &fh, &cal_file->temp_sw_species, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(S16) != f_write ( &fh, &cal_file->hs_species, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(F64) != f_write ( &fh, &cal_file->tempCal, sizeof(F64) ) ) ok_write = false;
	if ( CAL_DATE_LEN != f_write (&fh, cal_file->calFileDate, CAL_DATE_LEN ) ) ok_write = false;
	if ( sizeof(U16) != f_write ( &fh, &cal_file->integrationPeriod, sizeof(U16) ) ) ok_write = false;

    U16 const nBytes = sizeof(F64)*SPEC_PIXELS;

    if ( nBytes != f_write ( &fh, cal_file->wavelength,  nBytes ) ) ok_write = false;
    int species;
    for ( species=0; species<NUM_MAX_SPECIES; species++ ) {
    	if ( nBytes != f_write ( &fh, cal_file->ex_coeff[species], nBytes ) ) ok_write = false;
    }
    if ( nBytes != f_write ( &fh, cal_file->reference, nBytes ) ) ok_write = false;

    if ( sizeof(cal_file->filename) != f_write ( &fh, cal_file->filename, sizeof(cal_file->filename) ) ) ok_write = false;

	// Added four items at Firmware Version 2.3.0
	if ( sizeof(S16) != f_write ( &fh, &cal_file->tsCorrectable, sizeof(S16) ) ) ok_write = false;
	if ( sizeof(F64) != f_write ( &fh, &cal_file->nitrateCorrectionFactor, sizeof(F64) ) ) ok_write = false;
	if ( sizeof(F64) != f_write ( &fh, &cal_file->nitrateConcentration, sizeof(F64) ) ) ok_write = false;
	if ( sizeof(F64) != f_write ( &fh, &cal_file->seawaterConcentration, sizeof(F64) ) ) ok_write = false;
	if ( sizeof(S16) != f_write ( &fh, &cal_file->opticalPathLength, sizeof(S16) ) ) ok_write = false;

	f_close ( &fh );

	return ok_write;
}

/*
 *  For fast access to all coefficients, wavelength values.
 */
static bool num_write_CAL_Binary ( num_cal_file_in_memory* cal_file ) {

# define CAL_BIN_FNAME	EMMC_DRIVE CALFILE_BINARY_NAME
# define CAL_BIN_TMPRY	EMMC_DRIVE CALFILE_TEMPRY_NAME
# define CAL_BIN_BCKUP	EMMC_DRIVE CALFILE_BACKUP_NAME

	//	Copy current binary to backup
	f_delete( CAL_BIN_TMPRY );
	f_move( CAL_BIN_FNAME, CAL_BIN_TMPRY );

	bool ok_write = num_write_CAL_binary_basic( cal_file, CAL_BIN_FNAME );

	if ( !ok_write ) {
		//	Delete failed binary file
		f_delete( CAL_BIN_FNAME );
		//	Retrieve backup
		f_move( CAL_BIN_TMPRY, CAL_BIN_FNAME );
	} else {
		f_delete( CAL_BIN_TMPRY );

		if ( !num_write_CAL_binary_basic( cal_file, CAL_BIN_BCKUP ) ) {
			f_delete( CAL_BIN_BCKUP );
		}
	}

	return ok_write;

}

char const* NUM_GetActiveCalFileName() {

	if ( !num_cal_file.is_valid ) {
		NUM_InitializeData();
	}

	if ( !num_cal_file.is_valid ) {
		return "";
	} else {
		return num_cal_file.filename;
	}
}

S16 NUM_CalFileSpecies() {

	if ( !num_cal_file.is_valid ) {
		NUM_InitializeData();
	}

	if ( !num_cal_file.is_valid ) {
		return 0;
	} else {
		return num_cal_file.num_species;
	}
}

static bool num_read_CAL_Binary ( num_cal_file_in_memory* cal_file ) {


	fHandler_t fh;
	S16 f_state = f_open ( CAL_BIN_FNAME, O_RDONLY, &fh );

	if ( FILE_FAIL == f_state ) {
		//  Try backup file on eMMC
		f_state = f_open ( CAL_BIN_BCKUP, O_RDONLY, &fh );
		if ( FILE_FAIL == f_state ) {
			return false;
		}
	}

	bool ok_read = true;

	U32 file_Binary_Version;

	//	If there is nothing to read, cannot do anything. Fail.
	if ( sizeof(U32) != f_read ( &fh, &file_Binary_Version, sizeof(U32) ) ) {
		f_close ( &fh );
		return false;
	}

	if ( sizeof(S16) != f_read ( &fh, &cal_file->num_species, sizeof(S16) ) ) ok_read = false;
	if ( sizeof(S16) != f_read ( &fh, &cal_file->nitrate_species, sizeof(S16) ) ) ok_read = false;
	if ( sizeof(S16) != f_read ( &fh, &cal_file->seawater_species, sizeof(S16) ) ) ok_read = false;
	if ( sizeof(S16) != f_read ( &fh, &cal_file->temp_sw_species, sizeof(S16) ) ) ok_read = false;
	if ( sizeof(S16) != f_read ( &fh, &cal_file->hs_species, sizeof(S16) ) ) ok_read = false;
	if ( sizeof(F64) != f_read ( &fh, &cal_file->tempCal, sizeof(F64) ) ) ok_read = false;
	if ( CAL_DATE_LEN != f_read ( &fh, cal_file->calFileDate, CAL_DATE_LEN ) ) ok_read = false;
	if ( sizeof(U16) != f_read ( &fh, &cal_file->integrationPeriod, sizeof(U16) ) ) ok_read = false;

    U16 const nBytes = sizeof(F64)*SPEC_PIXELS;

    if ( nBytes != f_read ( &fh, cal_file->wavelength,  nBytes ) ) ok_read = false;
    int species;
    for ( species=0; species<NUM_MAX_SPECIES; species++ ) {
    	if ( nBytes != f_read ( &fh, cal_file->ex_coeff[species], nBytes ) ) ok_read = false;
    }
    if ( nBytes != f_read ( &fh, cal_file->reference, nBytes ) ) ok_read = false;

    if ( sizeof(cal_file->filename) != f_read ( &fh, cal_file->filename, sizeof(cal_file->filename) ) ) ok_read = false;

	// Added five items at Firmware Version 2.3.0 / Binary_Version 4
	// If current binary cal file is old version,
	// fix binary file on the fly by
	// assigning default values to new entries, and
	// writing a new binary file.
	bool changed_version = false;

	if ( file_Binary_Version <= 3 ) {
		cal_file->tsCorrectable = 0;
		cal_file->nitrateCorrectionFactor = 0;
		cal_file->nitrateConcentration = 0;
		cal_file->seawaterConcentration = 0;
		cal_file->opticalPathLength = 0;
		changed_version = true;
	} else {
		if ( sizeof(S16) != f_read ( &fh, &cal_file->tsCorrectable, sizeof(S16) ) ) ok_read = false;
		if ( sizeof(F64) != f_read ( &fh, &cal_file->nitrateCorrectionFactor, sizeof(F64) ) ) ok_read = false;
		if ( sizeof(F64) != f_read ( &fh, &cal_file->nitrateConcentration, sizeof(F64) ) ) ok_read = false;
		if ( sizeof(F64) != f_read ( &fh, &cal_file->seawaterConcentration, sizeof(F64) ) ) ok_read = false;
		if ( sizeof(S16) != f_read ( &fh, &cal_file->opticalPathLength, sizeof(S16) ) ) ok_read = false;
	}

	f_close ( &fh );

	if ( changed_version ) {
			num_write_CAL_Binary( cal_file );
	}

	return ok_read;
}

static void num_ShowBinary( num_cal_file_in_memory* cal_file ) {

	//	FIXME	snprintf ( "%f" )

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile Species %hd\r\n", cal_file->num_species ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile NO3 at  %hd\r\n", cal_file->nitrate_species ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile SWA at  %hd\r\n", cal_file->seawater_species ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile TSW at  %hd\r\n", cal_file->temp_sw_species ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile HS  at  %hd\r\n", cal_file->hs_species ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile TCAL    %f\r\n", printAbleF64(cal_file->tempCal) ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile CalDate %s\r\n",  cal_file->calFileDate ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile IntPer  %hu\r\n", cal_file->integrationPeriod ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile TSCor   %hd\r\n", cal_file->tsCorrectable ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile CorrNO3 %f\r\n", printAbleF64(cal_file->nitrateCorrectionFactor) ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile ConcNO3 %f\r\n", printAbleF64(cal_file->nitrateConcentration) ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile ConcSW  %f\r\n", printAbleF64(cal_file->seawaterConcentration) ); io_out_string(glob_tmp_str);
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile Path    %hd\r\n", cal_file->opticalPathLength ); io_out_string(glob_tmp_str);

	U16	first_idx;
	U16 final_idx;

	NUM_WavelengthRange_to_PixelRange (
			CFG_Get_Fit_Wavelength_Low(), CFG_Get_Fit_Wavelength_High(),
			&first_idx, &final_idx );

	S16 p;
	for ( p=0; p<SPEC_PIXELS; p++ ) {
		if ( first_idx <= p && p < final_idx ) {
			snprintf ( glob_tmp_str, sizeof(glob_tmp_str) , "CalFile E %.2f %.5f %.5f %.5f %.1f\r\n",
					printAbleF64( cal_file->wavelength[p]),
					printAbleF64( cal_file->ex_coeff[0][p]),
					printAbleF64( cal_file->ex_coeff[1][p]),
					printAbleF64( cal_file->ex_coeff[2][p]),
					printAbleF64( cal_file->reference[p]) );
			io_out_string(glob_tmp_str);
		}
	}
}


# if 0
static void num_delete_CAL_Binary () {
	char cal_bfname [ strlen(internal_drive) + strlen(CALFILE_BINARY_NAME) + 1 ];
	strcpy ( cal_bfname, internal_drive );
	strcat ( cal_bfname, CALFILE_BINARY_NAME );

	f_delete ( cal_bfname );
}
# endif

# if 0
//!	\brief	Generate a cal file name for the revision letter.
//!
//!	@param	filename	provide the generated file name here.
//!	@param	maxlen		size of char array passed
//!	@param	letter		revision letter for file name (A-Z).
//!	@param	fallback_to_default		permit the letter '-' to be accepted as a fallback to the initial default file.
//!
//!	return	NUM_OK		if file name could be generated
//!			NUM_FAIL	if file name could not be generated
S16 NUM_MakeCalFileName ( char* filename, size_t maxlen, U16 serialNumber, char letter, bool fallback_to_default ) {

	char* cal_file_dir = NUM_MakeCalDirName();
	if ( !cal_file_dir )
		return NUM_FAIL;

	letter = toupper ( letter );

	if ( ( 'A' <= letter && letter <= 'Z' ) || ( fallback_to_default && letter == '-' ) ) {

		char* cal_file_id;
		switch ( CFG_Get_Sensor_Type() ) {
		case CFG_Sensor_Type_ISUS:
			cal_file_id = "ISUS";
			break;
		case CFG_Sensor_Type_SUNA:
			cal_file_id = "SUNA";
			break;
		default:
			return NUM_FAIL;
			break;
		}

		U16 const serial_no = CFG_Get_Serial_Number();

		S32 written = snprintf ( filename, maxlen,
				"%s\\%s%03d%c.CAL", cal_file_dir, cal_file_id, serial_no, letter );

		if ( written > 0 && written < maxlen ) {
			return NUM_OK;
		} else {
			return NUM_FAIL;
		}
	} else {
		return NUM_FAIL;
	}
}
# endif

bool NUM_HaveCoefs () {
	return num_cal_file.is_valid;
}

F64  NUM_PixelToWavelength ( int pixel ) {
	if ( NUM_HaveCoefs () ) {
		if ( pixel < 0 ) pixel = 0;
		if ( pixel >= SPEC_PIXELS ) pixel = SPEC_PIXELS-1;
		return num_cal_file.wavelength[ pixel ];
	} else {
		return 0.0;
	}
}

S16 NUM_WavelengthRange_to_PixelRange (
		F32	const wl_low,
		F32 const wl_high,
		U16* px_low,
		U16* px_high ) {

	if ( !NUM_HaveCoefs () ) {
		return NUM_FAIL;
	} else {

		*px_low = 0xFFFF;
		*px_high = 0;

		U16 channel;

		*px_low = SPEC_PIXELS-1;
		//  Find the first channel with a wavelength at or above wl_low
		for ( channel=0; channel<SPEC_PIXELS; channel++ ) {
			if ( num_cal_file.wavelength[channel] >= wl_low ) {
				*px_low = channel;
				break;
			}
		}

		*px_high = 0;
		for ( channel=SPEC_PIXELS-1; channel>0; channel-- ) {
			if ( num_cal_file.wavelength[channel] <= wl_high ) {
				*px_high = channel;
				break;
			}
		}

		if ( 0xFFFF == *px_low || 0 == *px_high ) {
			return NUM_FAIL;
		} else {
			return NUM_OK;
		}
	}
}

/**
 *!	\brief	Read the cal file from its binary file
 *!			This call is needed once during startup.
 */
S16 NUM_InitializeData() {

	num_cal_file.is_valid = num_read_CAL_Binary( &num_cal_file );

	if ( num_cal_file.is_valid ) {

		if ( CFG_Get_Message_Level() >= CFG_Message_Level_Trace ) {
			num_ShowBinary( &num_cal_file );
		}
		return NUM_OK;

	} else {
		return NUM_FAIL;
	}
}

/**
 *!	\brief	Read ASCII cal file and write as binary file.
 */
S16 NUM_Rebuild( char const* fullfilename ) {

	fHandler_t fh;
	S16 const f_state = f_open ( fullfilename, O_RDONLY, &fh );

	if ( FILE_FAIL == f_state ) {
		CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
		CFG_Set_Last_Error_Code ( ELC_NO_CALFILE );
		CFG_Set_Last_Error_Time ( time((time_t*)0) );
		SYSLOG ( SYSLOG_ERROR, "Cannot open CAL file %s.", fullfilename );
		return NUM_FAIL;
	}

	if ( !num_parseCalFile ( &num_cal_file, fh ) ) {
		CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
		CFG_Set_Last_Error_Code ( ELC_NO_CALFILE );
		CFG_Set_Last_Error_Time ( time((time_t*)0) );
		SYSLOG ( SYSLOG_ERROR, "Cannot read from CAL file %s.", fullfilename );
		f_close ( &fh );
		return NUM_FAIL;
	}

	f_close ( &fh );

	/*
	 *  Write cal data as binary to file,
	 *  for faster read-back during startup.
	 *
	 *  If there is a problem,
	 *  revert to current binary CAL file.
	 */

	char* base = strrchr ( fullfilename, '\\' );
	if ( base ) {
		strcpy ( num_cal_file.filename, base+1 );
	} else {
		return NUM_FAIL;
	}

	//	If writing the binary file fails,
	//	reverts back to the current binary file.
	if ( !num_write_CAL_Binary( &num_cal_file ) ) {

		CFG_Set_Error_Counter(CFG_Get_Error_Counter()+1);
		CFG_Set_Last_Error_Code ( ELC_BAD_CALFILE );
		CFG_Set_Last_Error_Time ( time((time_t*)0) );
		SYSLOG ( SYSLOG_ERROR, "Cannot write binary CAL data." );

		return NUM_FAIL;
	}

	if ( num_cal_file.num_species < CFG_Get_Concentrations_To_Fit() ) {
		SYSLOG ( SYSLOG_WARNING, "New CAL file supports fewer species than configured fit concentrations. Configuration value was reduced." );
		CFG_Set_Concentrations_To_Fit ( (U8)num_cal_file.num_species );
	}

	return NUM_OK;
}

S16 NUM_BuildDefaultCalFile( void ) {

	S16 ch;

	//	Build wavelength values from Z-coefficients
	if ( ZC_FAIL == zc_makeWL ( num_cal_file.wavelength, SPEC_PIXELS ) ) {
		return NUM_FAIL;
	}

	num_cal_file.is_valid = false;

	//	Always build the default cal file for NO3, ASW, T*ASW
	num_cal_file.num_species = 3;
	num_cal_file.nitrate_species = 0;
	num_cal_file.seawater_species = 1;
	num_cal_file.temp_sw_species = 2;
	num_cal_file.hs_species = -1;

	num_cal_file.tempCal = 20.0;
	memset ( num_cal_file.calFileDate, 0, CAL_DATE_LEN );
	num_cal_file.integrationPeriod = CFG_Get_Spectrometer_Integration_Period();

	genericENO3 ( num_cal_file.wavelength, num_cal_file.ex_coeff[0], SPEC_PIXELS );

	genericESWA ( num_cal_file.wavelength, 20.0F, num_cal_file.ex_coeff[1], SPEC_PIXELS );

	F64 const T_low = 19.0;
	F64 const T_hgh = 21.0;
	F64* eswa_low = pvPortMalloc ( SPEC_PIXELS * sizeof(F64) );
	genericESWA ( num_cal_file.wavelength, T_low, eswa_low, SPEC_PIXELS );
	F64* eswa_hgh = pvPortMalloc ( SPEC_PIXELS * sizeof(F64) );
	genericESWA ( num_cal_file.wavelength, T_hgh, eswa_hgh, SPEC_PIXELS );

	for ( ch=0; ch<SPEC_PIXELS; ch++ ) {
		num_cal_file.ex_coeff[2][ch] = 1000.0 // for scaling
									 * ( eswa_hgh[ch] - eswa_low[ch] )
									 / ( T_hgh - T_low );
		if ( num_cal_file.ex_coeff[2][ch] < 1e-10 )
			num_cal_file.ex_coeff[2][ch] = 0;
	}

	vPortFree ( eswa_low );
	vPortFree ( eswa_hgh );

	if ( MEASURE_OK != measure_spectrum ( num_cal_file.reference ) ) {
		SYSLOG ( SYSLOG_ERROR, "No Reference Spectrum." );
		return NUM_FAIL;
	}

	char* dirName = NUM_MakeCalDirName();
	char fn [ strlen(dirName) + 1 + 8 + 1 + 3 + 1 ];
	snprintf ( fn, sizeof(fn), "%s\\%s%04hd_.CAL",
			dirName, "SNA",
			CFG_Get_Serial_Number()%10000 );

	if ( f_exists(fn) ) {
		//	Replace existing one with new one
		f_delete ( fn );
	}

	fHandler_t fh;
	if ( FILE_FAIL == f_open( fn, O_WRONLY|O_CREAT, &fh ) ) {
		SYSLOG ( SYSLOG_ERROR, "Cannot Open Cal File %s.", fn );
		return NUM_FAIL;
	}


	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,%s %04hd extinction coefficients and reference spectra\r\n",
			CFG_Get_Sensor_Type_AsString(), CFG_Get_Serial_Number() );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,File generated by Firmware,,,,\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,File format version 3\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,T_CAL_SWA %.2f\r\n", printAbleF64(num_cal_file.tempCal) );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,INT_PERIOD %hu\r\n", num_cal_file.integrationPeriod );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,Wavelength,nm\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,NITRATE,uM\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,AUX1,none\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,AUX2,none\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,Reference,Counts\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "H,Wavelength,NO3,SWA,T*SWA,Reference\r\n" );
	f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );

	for ( ch=0; ch<SPEC_PIXELS; ch++ ) {
		snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "E,%.2f,%.14g,%.14g,%.14g,%.1f\r\n",
				printAbleF64(num_cal_file.wavelength[ch]),
				printAbleF64(num_cal_file.ex_coeff[0][ch]),
				printAbleF64(num_cal_file.ex_coeff[1][ch]),
				printAbleF64(num_cal_file.ex_coeff[2][ch]),
				printAbleF64(num_cal_file.reference[ch]) );
		f_write ( &fh, glob_tmp_str, strlen(glob_tmp_str) );
	}

	f_close ( &fh );

	if ( NUM_OK == NUM_Rebuild(fn) ) {
		//	Read back just written binary data,
		//	to make sure all went ok, and will work at next bootup.
		return NUM_InitializeData();
	} else {
		SYSLOG ( SYSLOG_ERROR, "Cannot Rebuild NUM." );
		f_delete ( fn );
		return NUM_FAIL;
	}
}
# endif

# endif
