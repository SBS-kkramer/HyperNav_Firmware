/*!	\file mainSpectrometer.c
 *
 *	\brief Spectrometer board main file
 *
 *	NEED MORE INFO HERE 
 *
 * Created: 7/23/2015 12:51:37 PM
 *  Author: Scott
 */ 


/*!
 * \mainpage
 * 
 *	This is the Spectrometer PCB documentation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Environment header files. */
#include "power_clocks_lib.h"

/* Scheduler header files. */
#include "FreeRTOS.h"
#include "task.h"

#if 1
/* Demo file headers. */
#include "partest.h"
#include "serial.h"
#include "integer.h"
#include "comtest.h"
#include "flash.h"
#include "PollQ.h"
#include "semtest.h"
#include "dynamic.h"
#include "BlockQ.h"
#include "death.h"
#include "flop.h"
#endif

#include "asf.h"
#include "system.h"
#include "SPECtasks.h"
#include "Operation.h"

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************

/*! \brief Initializes the PCB on power up to a safe state
 */
static void main_InitBoard( void );

/*! \brief Main Spectrometer task
 */
static void vMainSpecTask( void *pvParameters );

//*****************************************************************************
// Local variables
//*****************************************************************************
static FState_t functional_state = STATE_COMMANDED;

int main( void )
{
	// could configure security bits here if need be
	
	main_InitBoard();	// initialize board to a safe state
		
	
	// Configure Osc0 in crystal mode (i.e. use of an external crystal source, with
	// frequency FOSC0) with an appropriate startup time then switch the main clock
	// source to Osc0.
	pcl_switch_to_osc(PCL_OSC0, FOSC0, OSC0_STARTUP);

	// Watchdog disable (needed to avoid cyclic resets when rebooting from firmware using WDT)
	wdt_disable();

	portDBG_TRACE("Spectrometer code starting, FOSC0=%ld", (long)FOSC0);			

	// start internal system clock early to get more accurate startup time, if needed
	SYS_InitTime();

	#if 1	// not sure if this section is required
	// Disable all interrupts.
	Disable_global_interrupt();

	// Initialize interrupt handling.
	INTC_init_interrupts();
	// Enable interrupts globally.
	Enable_global_interrupt();
	
	portDBG_TRACE("Interrupts enabled");
	#endif
	
	//TODO load configuration?

	// Create main spectrometer task
	xTaskCreate(vMainSpecTask, configTSK_SPEC_TSK_NAME, configTSK_SPEC_TSK_STACK_SIZE, NULL, configTSK_SPEC_TSK_PRIORITY, &gMainSPECTaskHandler);

	portDBG_TRACE("main task created");
	
	//initialize sampling engine
	//OP_initialize();
	
	/* Start the scheduler. */
	vTaskStartScheduler();
	
	// If this section of code is reached that means the main task could not be allocated
	// This is a fatal failure. The sensor will not operate.
	portDBG_TRACE("FATAL ERROR: Could not start scheduler");	//or use "DEBUG()"
	while(1);

	return 0;
}
/*-----------------------------------------------------------*/


static void main_InitBoard( void )
{
	portDBG_TRACE("Initializing PCB...");

	
	//TODO initialize board
	
	#if (BOARD != USER_BOARD)
		#if (BOARD == STK600_RCUC3C0)
			#error "Initializing for STK600 dev board.  Use BOARD=USER_BOARD"
		#else	
			#error "Invalid board definition.  Use BOARD=USER_BOARD"
		#endif	
	#endif
	
	
	
}


static void vMainSpecTask( void *pvParameters )
{
	// initialize board driver
	//initDriver();
	
	portDBG_TRACE("initDriver complete");
	
	// Retrieve configuration from non-volatile memory and configure internal modules accordingly
	//configure();

	// Configure system log size limits
	//syslog_setMaxFileSize(STG_GetMsgFileSize());


	// display size of user settings - we have 512 bytes max
	//SYSLOG(SYSLOG_DEBUG, "User settings size: %d bytes", sizeof(userFlashData_t));

	// Print reset information
	//printResetInfo();

	 


	//-------------------------------------------------------------------------
	// Main finite state machine (functional states)
	//-------------------------------------------------------------------------

	while(1)
	{
		
		switch(functional_state)
		{		
			case STATE_COMMANDED:	
				portDBG_TRACE("STATE_COMMANDED");
				//shell_Task();
				//OP_clearWakeupAlarm();  // Clear a pending alarm that expired while in commanded state
				functional_state = STATE_OPERATING;
				break;

			case STATE_OPERATING:	
				portDBG_TRACE("STATE_OPERATING");
				//functional_state = OP_operatingState();
				break;

			//case STATE_SIMSLEEP:
			//	functional_state = OP_simSleepState();
			//	break;

			//case STATE_TESTSHELL:	
				//testShell(TRUE);
				//functional_state = STATE_OPERATING;
				//break;

			default:
				functional_state = STATE_COMMANDED;	// Force to commanded mode to correct problem
			break;
		
		}
	}	


	/*	Should the task implementation ever break out of the above loop than this task must be deleted 
		before reaching the end of the function.
	 */
	vTaskDelete(NULL);
	
	portDBG_TRACE("Out of state machine");
}
