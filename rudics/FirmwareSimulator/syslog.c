/*! \file syslog.c *************************************************************
 *
 * \brief System log utility
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2011-03-30
 *
 **********************************************************************************/

#include "syslog.h"
#include "syslog_cfg.h"

# include <stdbool.h>

#include "time.h"
#include "sys/time.h"
# ifndef FW_SIMULATION
# include "systemtime.h"
# endif
#include "string.h"
#include "stdarg.h"
#include "stdio.h"

#ifdef SYSLOG_FILE_SUPPORT
# ifdef FW_SIMULATION
# include <unistd.h>
# else
# include "files.h"
# endif
#endif

#ifdef SYSLOG_TLM_SUPPORT
# ifndef FW_SIMULATION
# include "telemetry.h"
# endif
#endif


// Verify that an output support is specified
#ifndef SYSLOG_FILE_SUPPORT
#ifndef SYSLOG_TLM_SUPPORT
#ifndef	SYSLOG_STDO_SUPPORT
#error "SYSLOG - No output support specified!"
#endif
#endif
#endif


//*****************************************************************************
// Local Variables
//*****************************************************************************

// Log verbosity
static syslogVerbosity_t syslogVerbosity = SYSLOG_NOTICE;	// Default = least verbose

// Log outputs
static bool logToFile = true;								// Default = log to file only
static bool logToTelemetry = false;
static bool logToSTDO = false;

// Default size limit
static uint32_t  gMaxFileSize = LOG_MAX_SIZE_BYTES_DEF;

// Paths
static char gLogPath[LOG_MAX_PATHNAME+1] = LOG_PATH_DEF;
static char gLogFileName[LOG_MAX_PATHNAME+1] = LOG_FILE_DEF;
static char gOldLogFileName[LOG_MAX_PATHNAME+1] = LOG_OLD_FILE_DEF;


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

// Set log verbosity
int8_t syslog_setVerbosity(syslogVerbosity_t v) {

	if( SYSLOG_ERROR <= v && v <= SYSLOG_DEBUG) {

		syslogVerbosity = v;
		return  0;

	} else {

		return -1;
	}
}


// Enable/Disable log outputs
int8_t syslog_enableOut(syslogOut_t out) {

	switch(out) {

	case SYSLOG_TLM:  logToTelemetry = true; return  0; break;
	case SYSLOG_FILE: logToFile      = true; return  0; break;
	case SYSLOG_JTAG: logToSTDO      = true; return  0; break;

	default:		                         return -1; break;
	}
}

int8_t syslog_disableOut(syslogOut_t out ) {

	switch(out) {

	case SYSLOG_TLM:  logToTelemetry = false; return  0; break;
	case SYSLOG_FILE: logToFile      = false; return  0; break;
	case SYSLOG_JTAG: logToSTDO      = false; return  0; break;

	default:		                  return -1; break;
	}
}

// Set max log file size [kB]
void syslog_setMaxFileSize(uint16_t maxFileSizeKB) {

	gMaxFileSize = ((uint32_t)maxFileSizeKB)*1024L;
}

# if 0
// Configure paths and file names
static void syslog_setPaths(const char* new_logPath, const char* new_logFileName, const char* new_oldLogFileName)
{
	if(new_logPath)
	{
		strncpy(gLogPath, new_logPath, LOG_MAX_PATHNAME);
		gLogPath[LOG_MAX_PATHNAME] = '\0';
	}

	if(new_logFileName)
	{
		strncpy(gLogFileName, new_logFileName, LOG_MAX_PATHNAME);
		gLogFileName[LOG_MAX_PATHNAME] = '\0';
	}

	if(new_oldLogFileName)
	{
		strncpy(gOldLogFileName, new_oldLogFileName, LOG_MAX_PATHNAME);
		gOldLogFileName[LOG_MAX_PATHNAME] = '\0';
	}
}
# endif


// Log message
void syslog_out( syslogVerbosity_t v, const char* func, const char* fmt, ...) {

	// Filter out messages by verbosity
	if( syslogVerbosity >= v) {

    # define MAX_MSG_LEN 256
	  char message[MAX_MSG_LEN];

		// Assemble message
		//
		// Timetag
		//
              # ifdef FW_SIMULATION
		time_t now = time( (time_t*)0 );
		strftime( message, MAX_MSG_LEN, "[%Y/%m/%d %H:%M:%S]\t", gmtime(&now));
              # else
		struct timeval time;
		gettimeofday(&time, NULL);
		strftime(message, MAX_MSG_LEN, "[%Y/%m/%d %H:%M:%S] ",gmtime(&time.tv_sec));
              # endif

		size_t avail_len = MAX_MSG_LEN - strlen(message) - 1;

		// Function tag
		//
		strncat( message, func,   avail_len ); avail_len = MAX_MSG_LEN - strlen(message) - 1;
		strncat( message, "()\t", avail_len ); avail_len = MAX_MSG_LEN - strlen(message) - 1;

		// Severity
		//
		switch(v) {
		case SYSLOG_ERROR  : strncat( message, "ERROR\t"  , avail_len ); break;
		case SYSLOG_WARNING: strncat( message, "WARNING\t", avail_len ); break;
		case SYSLOG_NOTICE : strncat( message, "NOTICE\t" , avail_len ); break;
		case SYSLOG_INFO   : strncat( message, "INFO\t"   , avail_len ); break;
		case SYSLOG_DEBUG  : strncat( message, "DEBUG\t"  , avail_len ); break;
		default: break;
		}

		avail_len = MAX_MSG_LEN - strlen(message) - 3;

		// Append formatted arguments to message, and terminate message
		//
	        va_list ap;
		va_start( ap, fmt);
		vsnprintf( message+strlen(message), avail_len, fmt, ap);
		va_end( ap);

		avail_len = MAX_MSG_LEN - strlen(message) - 1;

		strncat( message, "\r\n", avail_len );

		message[MAX_MSG_LEN] = '\0';			// Ensure NUL termination

		// Send message to corresponding destinations
		//
# ifdef SYSLOG_STDO_SUPPORT
		if( logToSTDO ) {
                    # ifdef FW_SIMULATION
			fprintf( stderr,message);
                    # else
			printf( message );
                    # endif
		}
# endif

#ifdef SYSLOG_TLM_SUPPORT
# ifndef FW_SIMULATION
		if(logToTelemetry)
			tlm_send(message, strlen(message), 0);
# endif
#endif

#ifdef SYSLOG_FILE_SUPPORT
		if( logToFile ) {

			char logFileName[LOG_MAX_PATHNAME+1];
			char oldLogFileName[LOG_MAX_PATHNAME+1];

			// Assemble file names
			snprintf(logFileName,LOG_MAX_PATHNAME,"%s\\%s", gLogPath, gLogFileName);
			snprintf(oldLogFileName,LOG_MAX_PATHNAME,"%s\\%s", gLogPath, gOldLogFileName);
			logFileName[LOG_MAX_PATHNAME] = '\0';
			oldLogFileName[LOG_MAX_PATHNAME] = '\0';

# ifdef FW_SIMULATION
			FILE* fp = fopen ( logFileName, "a" );

			if ( fp ) {
				long fsize = ftell ( fp );
				if ( fsize + strlen(message) > gMaxFileSize ) {
					fclose ( fp );
					unlink ( oldLogFileName );
					if ( rename ( logFileName, oldLogFileName ) ) {
					  // if cannot rename, then remove
					  unlink ( logFileName );
					}
					fp = fopen ( logFileName, "w" );
				}
			}

			if ( fp ) {
                          fwrite ( message, strlen(message), 1, fp );
                          fclose ( fp );
			}

# else
			uint16_t open_flag;
			fHandler_t logFile;

			//Open file
			if ( f_exists(logFileName) ) {
				open_flag = O_WRONLY | O_APPEND;
			} else {
				open_flag = O_WRONLY | O_CREAT;
			}

			bool fileIsOpen = ( FILE_OK == f_open(logFileName, open_flag, &logFile) );

			if ( fileIsOpen ) {

				// Figure out if it is too large already. If it is, rotate.
				if( f_getSize(&logFile) > gMaxFileSize)
				{
					f_close(&logFile);
					f_delete(oldLogFileName);
					// Rename log file.
					// The destination file name most be without the drive designator.
					if ( FILE_OK != f_move(logFileName, oldLogFileName ) ) {
						//	If log file cannot be renamed, delete it.
						f_delete(logFileName);
					}

					// Open a new log file
					fileIsOpen = ( FILE_OK == f_open(logFileName, O_WRONLY | O_CREAT, &logFile) );
				}
			}

			if ( fileIsOpen ) {
				// Log message
				f_write(&logFile, message, strlen(message));
				// Close log file
				f_close(&logFile);
			}
# endif
		}
#endif

	}

}
