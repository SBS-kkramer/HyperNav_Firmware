/*! \file syslog.h**********************************************************
 *
 * \brief System log utility
 *
 * Features:
 * - Time tagged messages
 * - Dynamic verbosity control
 * - Messages can be simultaneously streamed to different outputs, ex.: telemetry, log file, stdout
 * - Rotating log files (log file size can be limited and rotated when limited is reached)  
 *
 * @author      Diego, Satlantic Inc.
 * @date        2011-03-30
 *
  ***************************************************************************/

# ifndef SYSLOG_CFG_H_
# define SYSLOG_CFG_H_

//-----------------------------------------------------------------------------
// OUTPUT STREAMS SUPPORT
//-----------------------------------------------------------------------------

// Comment / uncomment as needed
# define SYSLOG_FILE_SUPPORT
# define SYSLOG_TLM_SUPPORT
# define SYSLOG_STDO_SUPPORT


//-----------------------------------------------------------------------------
// SETUP
//-----------------------------------------------------------------------------

# define SYSLOG_PATH  "0:\\LOG"                  // Path where to store the log file
# define SYSLOG_FILE  SYSLOG_PATH "\\syslog.log" // Full log file name
# define SYSLOG_OLD   SYSLOG_PATH "\\sysold.log" // Rotation log file name (older messages will be stored here)

# define LOG_MAX_SIZE_BYTES_DEF	1024000 // Log file size limit (in bytes)


# endif /* SYSLOG_CFG_H_ */
