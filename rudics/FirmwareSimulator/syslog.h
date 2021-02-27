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
 * - Return values: 0 - Success
 *                 -1 - Error
 *
 * @author      Diego, Satlantic Inc.
 * @date        2011-03-30
 *
  ***************************************************************************/

#ifndef SYSLOG_H_
#define SYSLOG_H_

#include <stdint.h>

// System log outputs
typedef enum {SYSLOG_TLM, SYSLOG_STD, SYSLOG_FILE} syslogOut_t;

// Verbosity settings
typedef enum {SYSLOG_ERROR=0, SYSLOG_WARNING, SYSLOG_NOTICE, SYSLOG_INFO, SYSLOG_DEBUG} syslogVerbosity_t;

//*****************************************************************************
// Exported functions
//*****************************************************************************

// Set log verbosity
//
int8_t syslog_setVerbosity(syslogVerbosity_t v);

// Enable/Disable log outputs
//
int8_t syslog_enableOut(syslogOut_t out);
int8_t syslog_disableOut(syslogOut_t out);

// Set max log file size [KB]
//
void syslog_setMaxFileSize(uint16_t maxFileSizeKB);

// Log message
//
void syslog_out(syslogVerbosity_t v, const char* func, const char* fmt, ...);


#endif /* SYSLOG_H_ */
