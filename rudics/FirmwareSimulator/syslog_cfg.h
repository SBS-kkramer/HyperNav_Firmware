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
# ifndef FW_SIMULATION
# define SYSLOG_TLM_SUPPORT
# endif
# define SYSLOG_STDO_SUPPORT


//-----------------------------------------------------------------------------
// SETUP
//-----------------------------------------------------------------------------
// Max path length
//
# define LOG_MAX_PATHNAME	32
// Path where to store the log file
//
# ifdef FW_SIMULATION
# define LOG_PATH_DEF	"./"
# else
# define LOG_PATH_DEF	"0:\\LOG"
# endif
// Log file name
//
# define LOG_FILE_DEF	"syslog.log"
// Rotation log file name (older messages will be stored here)
//
# define LOG_OLD_FILE_DEF "sysold.log"
// Log file size limit (in bytes)
//
# define LOG_MAX_SIZE_BYTES_DEF	1024000


# endif /* SYSLOG_CFG_H_ */
