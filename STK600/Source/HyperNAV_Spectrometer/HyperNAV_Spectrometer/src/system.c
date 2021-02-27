/*! \file system.c
 *
 * Created: 7/24/2015 4:30:07 PM
 *  Author: Scott
 */ 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <preprocessor.h>
#include <avr32/io.h>
#include <stdint.h>
//#include <compiler.h>

#include "gpio.h"
#include "power_clocks_lib.h"

// AVR32 Resources Library API
//#include "sysmon.h"
//#include "systemtime.h"
//#include "onewire.h"
//#include "telemetry.h"
//#include "avr32rerrno.h"
//#include "watchdog.h"
//#include "files.h"
//#include "syslog.h"

#include "system.h"

//#include "power.h"
//#include "serial.h"
//#include "telemetry.h"
//#include "comms.h"
#include "confSpectrometer.h"
//#include "gpio.h"
//#include "spi.h"
//#include "userpage.h"

#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "portmacro.h"

#include "ast.h"
//#include "time.h"
//#include "sys/time.h"

//#include <adcifb.h>


//#include "asf.h"

/*	AST Prescalar determination:
	psel = log(Fosc/Fast)/log(2)-1, where Fosc is the frequency of the
	oscillator you are using (32768 Hz here) and Fast the desired frequency (1024 Hz)
	psel = log(32768/1024)/log(2) - 1 = 4
	*/
#define AST_PSEL_32KHZ_1024HZ		4
#define AST_SHIFT					10		//2^10 = 1024
#define AST_COUNTER_TO_SECONDS(x)	(x>>AST_SHIFT)
//#define AST_SUBSECONDS(x)			(x % (1 << AST_SHIFT))


S16	SYS_InitTime(void)
{
	static bool init = false;
	scif_osc32_opt_t opt;
	unsigned long ast_counter = 0;
	
	portDBG_TRACE("Initializing AST");
		
	opt.freq_hz =	FOSC32;
	opt.mode =		FOSC32_MODE;
	opt.startup =	OSC32_STARTUP;
	
	if (init) {		
		portDBG_TRACE("AST already init");
		return SYS_OK;
	}
	
	/* Start OSC_32KHZ */
	if (scif_start_osc32(&opt, true) == 0) {
		portDBG_TRACE("OSC32 failed to start");
	}

	// PAR firmware (AT32UC3L0256) required starting the oscillator in calendar mode and 
	// then switching to counter mode; we'll assume that isn't required
	
	// use the AST as a 32 bit counter, with clock running at 1024 Hz
	/* Initialize the AST */
	if (!ast_init_counter(&AVR32_AST, AST_OSC_32KHZ, AST_PSEL_32KHZ_1024HZ, ast_counter)) {
		portDBG_TRACE("AST init failed");
		return SYS_FAIL;
	}
	
	else {
		portDBG_TRACE("AST init");
	}
	
	//TODO: setup overflow interrupt as overflow occurs after ~47days?
	
	/* Enable the AST */
	ast_enable(&AVR32_AST);
	
	#if 0
		while (1) {
			ast_counter = ast_get_counter_value(&AVR32_AST);
			//portDBG_TRACE("AST counter: %lu seconds: %lu.%lu", ast_counter, AST_COUNTER_TO_SECONDS(ast_counter), AST_SUBSECONDS(ast_counter));
			portDBG_TRACE("AST counter: %lu seconds: %lu", ast_counter, AST_COUNTER_TO_SECONDS(ast_counter));
		}
	#endif
	
	
	init = true;
	
	return SYS_OK;
}