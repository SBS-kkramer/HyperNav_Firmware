/*! \file avr32rerrno.h**********************************************************
 *
 * \brief 'AVR32 resources library' error codes
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/

#ifndef AVR32RERRNO_H_
#define AVR32RERRNO_H_

#include "compiler.h"

// HyperNav system error number
extern S16 avr32rerrno;


//*****************************************************************************
// Returned constants
//*****************************************************************************

// -- No error --
#define ENOERROR	0

// -- Arguments sanity check error --
#define EPARAM		1

// -- Dynamic memory allocation error --
#define EMALLOC		2

// -- Generic MCU error --
#define EMCU		3

// --File system errors--

// DataFlash not ready
#define EDFNRDY		4
// eMMC not ready
#define EMMCNRDY	5
// Could not open file
#define EFOPEN		6
// File system navigator error
#define ENAV		7
// Bad file position
#define EBADFPOS	8
// File does not exist
#define ENOFILE		9
// Drive does not exist
#define ENODRIVE	10

// --Telemetry port errors--

// USART error
#define EUSART		11
// Buffer error
#define EBUFF		12

// Generic I/O error
#define ERRIO		13
// External RTC Error
#define EERTC		14
// Internal RTC Error
#define EIRTC		15


// -- One Wire Bus errors --

// Generic error
#define	EOW			16


// -- Lamp errors --

// Rail Overvoltage
#define EOVRV		17
// Power supply malfunction
#define ESPLY		18
// Reference malfunction
#define EREF		19
// UV Lamp malfunction
#define EUVLAMP		20


// -- System monitor errors --

// ADC error
#define EADC		21
// Temperature sensor error
#define ETSENSOR	22


// -- Battery backed up vars --

// Generic error
#define EBBV		23


// File error
#define EFERR		24

//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Return a descriptive error string for the current 'avr32rerrno'
//! @return Error string
const char* avr32r_strerror(void);


#endif /* AVR32RERRNO_H_ */
