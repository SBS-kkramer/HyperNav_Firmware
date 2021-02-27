/*
 *  File: datalogfile.h
 *  Isus Firmware version 2
 *  Copyright 2004 Satlantic Inc.
 */

# ifndef _DATALOGFILE_H_
# define _DATALOGFILE_H_

# include <stdint.h>

# include "files.h"

# include "config.controller.h"

# define DLF_OK	 0
# define DLF_FAIL	-1

//!	\brief	Generate log file directory name.
//!
//!	return	pointer to static char[]	if file name was generated
//!			0							if name could not be generated
char const* dlf_LogDirName ();

# if 0
/**
 *!	\brief	A wrapper function to open the appropriate frame log file for writing.
 *!
 *!	@param	type	The log file type and frame type determine the file name.
 */
S16 dlf_openLogFile ( CFG_Log_File_Type log_file_type, CFG_Frame_Type frame_type,
		fHandler_t* fh, bool* write_info_header );
# endif

//! \brief  Start logging
//!         Must call DLF_Start() before calling DLF_Write()
//!
//! @param  yy_mm_dd    date string, specifies the current date and determines log folder
//! return   0  OK
//! return  -1  FAILED due to file system access failure
int16_t DLF_Start( char* yy_mm_dd, char type );

//! \brief  Stop logging
void DLF_Stop();

//! \brief  Log data (maximum write of 2^16 bytes)
//!
//! @param  
//! @param  
//! return   number of bytes written to file
//! return  -1  FAILED due to not being started
//! return  -2  FAILED due to write to file failure
int32_t DLF_Write( uint8_t* data, uint16_t size );


# endif /* _DATALOGFILE_H_ */
