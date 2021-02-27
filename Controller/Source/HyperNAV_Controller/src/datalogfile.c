/*
 *  File: datalogfile.c
 */

/*
 *  Creation of data log files.
 */

# include "datalogfile.h"

# include <ctype.h>
# include <stdio.h>
# include <string.h>

# include "syslog.h"
# include "files.h"
# include "extern.controller.h"

//  The log file name will be up to 33 characters long.
//  "0:/FREEFALL/YY-MM-DD/FF-#####.raw"   ---  for free fall profiles
//  "0:/FREEFALL/YY-MM-DD/CC-#####.raw"   ---  for calibration
//  "0:/FREEFALL/YY-MM-DD/DD-#####.raw"   ---  for dark characterization

static char dlf_current_file_name[34] = "";

//! \brief  Start logging
//!         Must call DLF_Start() before calling DLF_Write()
//!
//! @param  yy_mm_dd    date string, specifies the current date and determines log folder
//! return   0  OK
//! return  -1  FAILED due to misformatted yy_mm_dd parameter
//! return  -2  FAILED due to file system access failure
int16_t DLF_Start( char* yy_mm_dd, char type ) {

  //  Check for properly formatted date string
  //
  if ( 8 != strlen(yy_mm_dd)
    || !isdigit( yy_mm_dd[0])
    || !isdigit( yy_mm_dd[1])
    ||  '-'  !=  yy_mm_dd[2]
    || !isdigit( yy_mm_dd[3])
    || !isdigit( yy_mm_dd[4])
    ||  '-'  !=  yy_mm_dd[5]
    || !isdigit( yy_mm_dd[6])
    || !isdigit( yy_mm_dd[7]) ) {

    return (int16_t)(-1);
  }

  //  Declate filder and file names
  //
  fHandler_t fh;

  char* subDir;
  switch ( type ) {
  case 'p': case 'P':  subDir = "PROFILE"; break;
  case 'f': case 'F':  subDir = "FREEFALL"; break;
  case 'd': case 'D':  subDir = "FREEFALL"; break;  //  FIXME == DARKCHAR
  case 'c': case 'C':  subDir = "FREEFALL"; break;  //  FIXME == CALIB
  default           :  subDir = "FREEFALL"; break;
  }

  char top_folder_name[12];
  char log_folder_name[21];
  char info_file_name[34];

  //  First, check if folder for this date exists
  snprintf ( top_folder_name, 12, "0:\\%s", subDir );
  snprintf ( log_folder_name, 21, "0:\\%s\\%s", subDir, yy_mm_dd );
  snprintf (  info_file_name, 34, "0:\\%s\\%s\\INFOFILE.BIN", subDir, yy_mm_dd );

  //  Create top level folder
  if ( !f_exists( top_folder_name ) ) {
    //  Folder does not exist: create folder
    if ( !FILE_OK == file_mkDir( top_folder_name ) ) {
      return (int16_t)(-2);
    }
  }

  uint16_t counter = 0;

  if ( !f_exists(log_folder_name) ) {
    //  Folder does not exist: create folder
    if ( !FILE_OK == file_mkDir( log_folder_name ) ) {
      return (int16_t)(-3);
    }

    counter = 1;

  } else {

    //  Folder exist: get counter from info file, increment, write back.
    if ( FILE_OK != f_open( info_file_name, O_RDONLY, &fh ) ) {
      return (int16_t)(-4);
    }

    if ( sizeof(counter) != f_read( &fh, &counter, sizeof(counter) ) ) {
      f_close ( &fh );
      return (int16_t)(-5);
    }

    f_close ( &fh );

    counter ++;
  }

  //  Create new or replace existing info file with counter = 1
  if ( FILE_OK != f_open ( info_file_name, O_WRONLY | O_CREAT, &fh ) ) {
    return (int16_t)(-6);
  }

  if ( sizeof(counter) != f_write( &fh, &counter, sizeof(counter) ) ) {
    f_close ( &fh );
    return (int16_t)(-7);
  }

  f_close ( &fh );

  //  Create log file name using counter
  char* ID;
  switch ( type ) {
  case 'p': case 'P':  ID = "PP"; break;
  case 'f': case 'F':  ID = "FF"; break;
  case 'd': case 'D':  ID = "DD"; break;
  case 'c': case 'C':  ID = "CC"; break;
  default           :  ID = "XX"; break;
  }
  snprintf ( dlf_current_file_name, 34, "0:\\%s\\%s\\%2.2s-%05hu.raw", subDir, yy_mm_dd, ID, counter );

  if ( f_exists ( dlf_current_file_name ) ) {
    //  This is unexpected.
    //  A data log file of this name should not exist.
    //  Ignore this error, and just append to the file.
    if ( FILE_OK != f_open ( dlf_current_file_name, O_WRONLY | O_APPEND, &fh ) ) {
      return (int16_t)(-8);
    }

    if ( 10 != f_write( &fh, "\r\nAPPEND\r\n", 10 ) ) {
      f_close ( &fh );
      return (int16_t)(-9);
    }

  } else {
    //  Make sure the new log file can be generated.
    if ( FILE_OK != f_open ( dlf_current_file_name, O_WRONLY | O_CREAT, &fh ) ) {
      return (int16_t)(-10);
    }

    if ( 7 != f_write( &fh, "BEGIN\r\n", 7 ) ) {
      f_close ( &fh );
      return (int16_t)(-11);
    }
  }

  f_close ( &fh );

  return (int16_t)0;
}

//! \brief  Stop logging
void DLF_Stop() {

  if ( dlf_current_file_name[0] == 0 ) {
    //  Was never started. Ignore.
    return;
  }

  fHandler_t fh;

  if ( FILE_OK != f_open ( dlf_current_file_name, O_WRONLY | O_APPEND, &fh ) ) {
    return;
  }

  if ( 5 != f_write( &fh, "END\r\n", 5 ) ) {
    f_close ( &fh );
    return;
  }

  f_close ( &fh );

  dlf_current_file_name[0] = 0;

  return;
}

//! \brief  Log data (maximum of 2^15 bytes per call)
//!
//! @param  
//! @param  
//! return   number of bytes written to file
//! return  -1  FAILED due to write to file failure
//! return  -2  FAILED due to DLG_Start() not done/succeeded
//! return  -3  FAILED due to file open error
int32_t DLF_Write( uint8_t* data, uint16_t size ) {

  if ( 0 == dlf_current_file_name[0] ) {
    return (int32_t)(-2);
  }

  fHandler_t fh;

  if ( FILE_OK != f_open ( dlf_current_file_name, O_WRONLY | O_APPEND, &fh ) ) {
    return (int16_t)(-3);
  }

  int32_t written = f_write( &fh, data, size );
  f_close ( &fh );

  return written;
}

//  FIXME -- Multiple directories must be supported!
# define FREEFALLDIR "FREEFALL"

//! \brief  Generate Free-falling profiling directory name.
//!
//! return  pointer to constant char*
char const* dlf_LogDirName () {

  return EMMC_DRIVE FREEFALLDIR;
}


# if 0

# include <math.h>
# include <stdio.h>
# include <string.h>


# include "syslog.h"
# include "command.controller.h"
# include "extern.controller.h"
# include "frames.h"
# include "filesystem.h"
# include "io_funcs.controller.h"

# define DATADIR "DAT"

# define ELC_FLASH_CARD_FULL		0x0101
# define ELC_FILE_OPEN_FAILURE	0x0102

//!	\brief	Generate log file directory name.
//!
//!	return	pointer to static char[]	if file name was generated
//!			0							if name could not be generated
char const* dlf_dataLogDirName () {

	return EMMC_DRIVE DATADIR;
}


/**
 *!	\brief	A wrapper function to open the appropriate frame log file for writing.
 *!
 *!	@param	type	The log file type determines the file name.
 */
S16 dlf_openLogFile ( CFG_Log_File_Type log_file_type, CFG_Frame_Type frame_type,
		fHandler_t* fh, bool* write_header_info ) {

	//	First see if the data log directory exists.

	if ( !f_exists(EMMC_DRIVE DATADIR) ) {
		if ( !FILE_OK == file_mkDir( EMMC_DRIVE DATADIR ) ) {
			return DLF_FAIL;
		}
	}

	char* extension;

	switch ( frame_type ) {
	case CFG_Frame_Type_Full_Binary:
		extension = "BIN";
		break;
	default:
		extension = "CSV";
		break;
	}

	//	Build the log file name according to configured setting

	char logname[3+9+8+1+3+1];	// Drive:\ + <dir-name>\ + <8.3 Filename) + NUL character

# if 0
	if ( CFG_Get_Burnin_Hours() > 0 ) {	//	B files

		snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\B_%02hu_%03hu.%s",
				(U16)CFG_Get_Burnin_Number()%100, CFG_Get_Burnin_Segment()%1000, extension );

	} else if ( CFG_Get_Dark_Hours() > 0 ) {

		snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\D_%02hu_%03hu.%s",
				(U16)CFG_Get_Dark_Number()%100, CFG_Get_Dark_Segment()%1000, extension );

	} else
# endif
	{

		switch ( log_file_type ) {

		case CFG_Log_File_Type_Continuous:	//	C files

			snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\C%07lu.%s",
					CFG_Get_Continuous_Counter() % 10000000, extension );

			if ( f_exists(logname) ) {
				fHandler_t fh;
				if ( FILE_OK == f_open( logname, O_RDONLY, &fh ) ) {
					if ( ( ( f_getSize( &fh ) >= (1024L*1022L) ) * CFG_Get_Data_File_Size() ) ) {
						CFG_Set_Continuous_Counter(CFG_Get_Continuous_Counter()+1);
						snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\C%07lu.%s",
							CFG_Get_Continuous_Counter() % 10000000, extension );
					}
					f_close ( &fh );
				}
			}

			break;

		case CFG_Log_File_Type_Acquisition:	//	A files

			// Once during an event (event = from power up to power down)
			// check if the acq number should be incremented.
			if ( global_start_new_A_file ) {

				time_t now = time( (time_t*)0 );
				time_t last = CFG_Get_AcqFile_Creation_Time();

				if ( now < last /* to catch bad 'last' values */
				  || now+15 > last + 60*CFG_Get_AcqFile_Minimum_Duration() ) {
					CFG_Set_AcqFile_Creation_Time( now );
					CFG_Set_Acquisition_Counter(CFG_Get_Acquisition_Counter()+1);
				}
				global_start_new_A_file = false;
			}

			snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\A%07lu.%s",
					CFG_Get_Acquisition_Counter() % 10000000, extension );
			break;

		case CFG_Log_File_Type_Daily: {	//	D files

			time_t current_time = time((time_t*)0);
			struct tm* tp = gmtime(&current_time);
			U16 yy = 1900+tp->tm_year;
			U16 doy = (tp->tm_yday%366)+1;

			snprintf( logname, sizeof(logname), EMMC_DRIVE DATADIR "\\D%04d%03d.%s",
					yy, doy, extension );
			}
			break;

		default:
			return DLF_FAIL;
		}
	}

	S16 f_open_return;

	if ( f_exists(logname) ) {
		if ( CFG_Data_File_Needs_Header_Yes == CFG_Get_Data_File_Needs_Header() ) {
			//	This happens is a processing parameter was changed.
			*write_header_info = true;
			CFG_Set_Data_File_Needs_Header ( CFG_Data_File_Needs_Header_No );
		}
		f_open_return = f_open ( logname, O_WRONLY | O_APPEND, fh );
	} else {
		//	Each new data log file gets a header.
		*write_header_info = true;
		f_open_return = f_open ( logname, O_WRONLY | O_CREAT, fh );
	}

	if( FILE_OK == f_open_return ) {
		return DLF_OK;
	} else {
		syslog_out ( SYSLOG_ERROR, "dlf_openLogFile", "Cannot open '%s'", logname );
		return DLF_FAIL;
	}
}

# endif
