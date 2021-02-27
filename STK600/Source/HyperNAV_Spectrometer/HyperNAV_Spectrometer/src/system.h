/*! \file system.h
 *
 * Created: 7/24/2015 4:31:30 PM
 *  Author: Scott
 */ 


#ifndef SYSTEM_H_
#define SYSTEM_H_

//TODO	Update for DOxygen

#include <compiler.h>
#include "confSpectrometer.h"

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define SYS_OK			0
#define SYS_FAIL		-1


//*****************************************************************************
// Exported data types
//*****************************************************************************

// System initialization info. This structure is returned by 'sys_init()' to report which sub-systems
// succeeded/failed to initialize. TRUE = Sub-system initialized OK, FALSE = Sub-system failed to initialize.
typedef struct SYSINIT_T
{
	//Bool cpu;		// CPU clock initialization result
	//Bool pwr;		// Power sub-system initialization result
	//Bool tlm;		// Telemetry port initialization result
	Bool time;		// Time sub-system initialization result
	//Bool sysmon;	// System monitor initialization result
	//Bool file;		// File system initialization result
	//Bool usb;		// USB sub-system init result
	//Bool adc;		// internal adc

}sysinit_t;


//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Initialize sensor system.
//! This function initializes the several modules that compose the sensor sub-system. It
//! MUST be called upon initialization correctly set up the system
//! @param	initStatus		Output:	Initialization status. Find here information about which modules did not initialize.
//! @return SYS_OK:Success , SYS_FAIL: Failure, check 'avr32rerrno'
//S16 sys_init(sysinit_t* initStatus );


S16	SYS_InitTime(void);



#endif /* SYSTEM_H_ */