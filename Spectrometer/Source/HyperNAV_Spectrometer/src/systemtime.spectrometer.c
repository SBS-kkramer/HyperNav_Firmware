/*!	\file	systemtime.spectrometer.c
 *	\brief	System time utilities
 * Created: 13/07/2016 2:08:27 PM
 *  Author: sfeener
 */ 


#include "compiler.h"
#include "ast.h"

#include "avr32rerrno.h"
#include <scif_uc3c.h>
#include <stdio.h>
# include <ctype.h>
# include <string.h>
# include <stdlib.h>
# include <time.h>
# include <sys/time.h>

#include <sysclk.h>

# include "FreeRTOS.h"
# include "task.h"
# include "queue.h"

#include "systemtime.spectrometer.h"

//#define AST_USE_32KHZ		//uncomment to use 32 kHz osc - NOT FULLY SUPPORTED/TESTED YET

//*****************************************************************************
// AST Prescalar determination
//*****************************************************************************
#ifdef AST_USE_32KHZ
	/*	AST Prescalar determination:
		psel = log(Fosc/Fast)/log(2)-1, where Fosc is the frequency of the
		oscillator you are using (32768 Hz here) and Fast the desired frequency (1024 Hz)
		psel = log(32768/1024)/log(2) - 1 = 4
		*/
	#define AST_PSEL_32KHZ_1024HZ		4
	#define AST_SHIFT					10		//2^10 = 1024
	#define AST_COUNTER_TO_SECONDS(x)	(x>>AST_SHIFT)
	//#define AST_SUBSECONDS(x)			(x % (1 << AST_SHIFT))
#elif 1	// use PB clock for higher resolution
	/*	AST Prescalar determination:
		psel = log(Fosc/Fast)/log(2)-1, where Fosc is the frequency of the
		oscillator you are using (14745600 Hz here) and Fast the desired frequency (115200 Hz)
		psel = log(14745600/115200)/log(2) - 1 = 6
		*/
	#define AST_PSEL					6
	#define FAST_CLK					115200
	#define AST_COUNTER_TO_SECONDS(x)	(x/FAST_CLK)
#else 	// PB clock (PBA) at high speed
	/*	AST Prescalar determination:
		psel = log(Fosc/Fast)/log(2)-1, where Fosc is the frequency of the
		oscillator you are using (14745600 Hz here) and Fast the desired frequency (115200 Hz)
		psel = log(4*14745600/115200)/log(2) - 1 = 8
		*/
	#define AST_PSEL					8
	#define FAST_CLK					115200
	#define AST_COUNTER_TO_SECONDS(x)	(x/FAST_CLK)	
#endif

F64 AST_Counter_to_Second( U32 counter ) {
    return ((F64)counter)/FAST_CLK;
}

U32 Second_to_AST_Counter( F64 second ) {
    return second*FAST_CLK;
}


//*****************************************************************************
// Local Variables
//*****************************************************************************

// System time offset in seconds
static U32 systimeOffset = 0;
		


S16	STIME_time_init(void)
{
	static bool init = false;
	unsigned long ast_counter = 0;
	scif_osc32_opt_t opt;
	
	opt.freq_hz =	FOSC32;
	opt.mode =		FOSC32_MODE;
	opt.startup =	OSC32_STARTUP;
	
	if (init) {
		return STIME_OK;
	}

	/* Start OSC_32KHZ */
	// Even if not used for AST, required for spectrometers, etc
	if (scif_start_osc32(&opt, true) < 0 ) {
		portDBG_TRACE("OSC32 failed to start");
		return STIME_FAIL;
	}
#ifdef AST_USE_32KHZ
	// PAR firmware (AT32UC3L0256) required starting the oscillator in calendar mode and
	// then switching to counter mode; we'll assume that isn't required
	
	// Use the AST as a 32 bit counter, with clock running at 1024 Hz
	/* Initialize the AST */
	if (!ast_init_counter(&AVR32_AST, AST_OSC_32KHZ, AST_PSEL_32KHZ_1024HZ, ast_counter)) {
		portDBG_TRACE("AST init failed");
		return STIME_FAIL;
	} else {
# ifdef DEBUG
		portDBG_TRACE("AST init");
# endif
	}

	#warning "Need overflow interrupt for >47 days operation"
	//TODO: setup overflow interrupt as overflow occurs after ~47days?  Shouldn't be needed as we should only
	// be awake for the duration of a profile and data transmission
#else
	//if (sysclk_get_pba_hz() != 14745600)
	//portDBG_TRACE("ERROR, unexpected PBA value\r\n");
	
	// Use the AST as a 32 bit counter, with clock running at 115200 Hz generated from PB
	/* Initialize the AST */
	if (!ast_init_counter(&AVR32_AST, AST_OSC_PB, AST_PSEL, ast_counter)) {
		portDBG_TRACE("AST init failed");
		return STIME_FAIL;
	} else {
# ifdef DEBUG
		portDBG_TRACE("AST init");
# endif
	}
	
	#warning "Need overflow interrupt or support for reset for >10 HOURS operation"
	//TODO: setup overflow interrupt as overflow occurs after ~10h?  Shouldn't be needed as we should only
	// be awake for the duration of a profile and data transmission
#endif
	
	/* Enable the AST */
	ast_enable(&AVR32_AST);
	
#if 0
	char s[64];
	while (1) {
		ast_counter = ast_get_counter_value(&AVR32_AST);
		//io_dump_U32("AST seconds: %lu", AST_COUNTER_TO_SECONDS(ast_counter));
		//portDBG_TRACE("AST counter: %lu seconds: %lu", ast_counter, AST_COUNTER_TO_SECONDS(ast_counter));
		snprintf(s, sizeof(s), "AST counter: %lu seconds: %lu micro: %\r\n", ast_counter, AST_COUNTER_TO_SECONDS(ast_counter));
		portDBG_TRACE(s);
		
		vTaskDelay(500);
	}
#endif
	
	init = true;
	
	return STIME_OK;
}


#if 0
//! \brief Synchronize system time with external RTC (ext_RTC->systime)
S16 STIME_time_ext2sys(U32 extTime)
{
	// Set offset to ext RTC time
	systimeOffset = extTime;
	
	// Reset systime counter
	ast_set_counter_value(&AVR32_AST, 0);

	return STIME_OK;
}
#endif


//! \brief return system time in seconds (systime->ext_RTC)
U32 STIME_time_sys2ext(void)
{
	U32 systime;

	// Get internal time in seconds
	systime =  AST_COUNTER_TO_SECONDS(ast_get_counter_value(&AVR32_AST)) + systimeOffset;

	return systime;
}

// Overriding '_gettimeofday()'
int _gettimeofday(struct timeval *__p, void *__tz);

int _gettimeofday(struct timeval *__p, void *__tz)
{
	U32 sysrtcCounts = ast_get_counter_value(&AVR32_AST);
	U32 subsecondCounts;

	// Sanity check
	if(__p == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	__p->tv_sec = AST_COUNTER_TO_SECONDS(sysrtcCounts) + systimeOffset;

	subsecondCounts = sysrtcCounts % FAST_CLK;	

	__p->tv_usec = 1000000 * ((F64)subsecondCounts / FAST_CLK);

	return 0;
}

// Defining 'settimeofday()'
int settimeofday(const struct timeval *tv , const struct timezone *tz)
{
	U32 subsecondCounts;

	// Sanity check
	if(tv == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	// Calculate sub-second counts
	subsecondCounts = ((F64)tv->tv_usec / 1000000.0) * FAST_CLK;

	// Store seconds in the offset and reset the internal RTC to the sub-second counts
	systimeOffset = tv->tv_sec;
	ast_set_counter_value(&AVR32_AST, subsecondCounts);

	return 0;
}
