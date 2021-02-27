/*! \file avr32rerrno.c *************************************************************
 *
 * \brief 'AVR32 resources library' error codes
 *
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-10
 *
 **********************************************************************************/

#include "compiler.h"
#include "avr32rerrno.h"


// Define errno variable
S16 avr32rerrno = ENOERROR;


// Error strings

//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************
const char* avr32r_strerror(void)
{

	const char* ret;

	switch(avr32rerrno)
	{
	// -- No error --
	case ENOERROR:	ret = "No error"; break;
	// -- Arguments sanity check error --
	case EPARAM:	ret = "Invalid argument(s)"; break;
	// -- Dynamic memory allocation error --
	case EMALLOC:	ret = "Memory allocation failure"; break;
	// -- Generic MCU error --
	case EMCU:		ret = "MCU fatal error"; break;
	// DataFlash not ready
	case EDFNRDY:	ret = "DataFlash not ready"; break;
	// eMMC not ready
	case EMMCNRDY:	ret = "eMMC not ready"; break;
	// Could not open file
	case EFOPEN:	ret = "Could not open file"; break;
	// File system navigator error
	case ENAV:		ret = "File system navigator error"; break;
	// Bad file position
	case EBADFPOS:	ret = "Bad file position";	break;
	// File does not exist
	case ENOFILE:	ret = "File does not exist"; break;
	// Drive does not exist
	case ENODRIVE:	ret = "Drive does not exist"; break;
	// USART error
	case EUSART:	ret = "USART error"; break;
	// Buffer error
	case EBUFF:		ret = "Buffer error"; break;
	// Generic I/O error
	case ERRIO:		ret = "I/O error";	break;
	// External RTC Error
	case EERTC:		ret = "External RTC error"; break;
	// Internal RTC Error
	case EIRTC:		ret = "Internal RTC error"; break;
	// Generic OW error
	case EOW:		ret = "One-Wire error"; break;
	// Rail Overvoltage
	case EOVRV:		ret = "Rail overvoltage"; break;
	// Power supply malfunction
	case ESPLY:		ret = "Power supply malfunction"; break;
	// Reference malfunction
	case EREF:		ret = "Reference malfunction"; break;
	// UV Lamp malfunction
	case EUVLAMP:	ret = "UV lamp malfunction"; break;
	// ADC error
	case EADC:		ret = "ADC error"; break;
	// Temperature sensor error
	case ETSENSOR:	ret = "Temperature sensor error"; break;
	// Generic battery backed variable error
	case EBBV:		ret = "Battery-backed variable error"; break;
	// Unknown error code
	default: 		ret = "Unknown error"; break;
	}

	return ret;
}


//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************
