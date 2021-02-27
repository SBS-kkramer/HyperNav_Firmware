/*!    \file main.spectrometer.c
 *
 *    \brief Main function for the HyperNav Spectrometer firmware.
 *
 *    @author Burkhard Plache
 *    @date   2015-08-02
 */ 


/*!
 * \mainpage
 * 
 *    This is the Spectrometer PCB documentation
 */

//    These include files are standard C
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Environment header files. */
#include "power_clocks_lib.h"

//    These include files are from the Atmel Software Framework (ASF)

#include "FreeRTOS.h"
#include "task.h"

# include "spi.spectrometer.h"
# include "pressure.h"
# include "cgs.h"
# include "sn74v283.h"
# include "AD7677.h"
# include "shutters.h"

# if 0
/* Demo file headers. */
#include "serial.h"
#include "partest.h"
#include "comtest.h"
#include "integer.h"
#include "flash.h"
#include "PollQ.h"
#include "semtest.h"
#include "dynamic.h"
#include "BlockQ.h"
#include "death.h"
#include "flop.h"
# endif

#include "asf.h"

#include "gpio.h"
//#include "usart.h"

// for serial port testing
#include "io_funcs.spectrometer.h"
//#include <avr32/uc3c0512c.h>
//#include <avr32/pm_412.h>

#include "board.h"

#include <wdt.h>
#include "sysclk.h"

//    These include files are in the local src directory
#include "tasks.spectrometer.h"
#include "command.spectrometer.h"
#include "data_exchange.spectrometer.h"
#include "data_acquisition.h"
#include "hypernav.sys.spectrometer.h"
#include "cgs.h"

#include "orientation.h"


//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************

/*! \brief Initializes the PCB on power up to a safe state
 */
static void main_InitBoard( void );


/*! \brief Main Spectrometer task
 */
//static void vMainSpecTask( void *pvParameters );


//*****************************************************************************
// Local variables
//*****************************************************************************
static Bool create_spectrometer_tasks(void) {

    // Create tasks
    Bool tasksCreatedOK = true;

    portDBG_TRACE("creating tasks");

    // Start Setup+Command Task
    tasksCreatedOK &= command_spectrometer_createTask();

    // Start Task-Monitor Task
    tasksCreatedOK &= taskMonitor_spectrometer_createTask();

    // Data Exchange Hub
    tasksCreatedOK &= data_exchange_spectrometer_createTask();

    // Data Acquisition
    tasksCreatedOK &= data_acquisition_createTask();
	//#warning "data task removed"
	
	portDBG_TRACE("tasks created");

    return tasksCreatedOK;
}



//*****************************************************************************
// Main function
//*****************************************************************************
int main( void )
{
    // could configure security bits here if need be
	
	//-- Initialize MCU --
	sysclk_init();
	

    main_InitBoard();  // initialize board to a safe state

    // Watchdog disable (needed to avoid cyclic resets when rebooting from firmware using WDT)
    wdt_disable();


    // Start internal 32kHz system clock early to get more accurate startup time, if needed
//    SYS_InitTime();

# if REUSE_SUNA_CODE
# error
    //  Initialize with a long timeout value.
    watchdog_enable ( (U64)60000000L );
    //  Prevent permanent resets in case there was a previous watchdog triggered reset.
    watchdog_disable ();
    //  Make sure we don't start if the power is insufficient (i.e. brown out)
    pwr_BOR();
# endif

 # if 1    // not sure if this section is required
    // Disable all interrupts.
    Disable_global_interrupt();

    // Initialize interrupt handling.
    INTC_init_interrupts();

    // ...SPI interrupt will be registered later.

    // Enable interrupts globally.  cpu_irq_enable()
    Enable_global_interrupt();

    portDBG_TRACE("Interrupts enabled");
 # endif

    //  Set watchdog timer to 15 seconds
    wdt_opt_t opt = {
        .us_timeout_period = 60000000   //  For testing, increased to 60 seconds. FIXME back to 15 seconds
    };
    wdt_enable(&opt);

    // Create the tasks, but they don't start running yet
    if( !create_spectrometer_tasks() ) {
        //# warning "If tasks not created, cannot use telemetry out!"
        portDBG_TRACE("\r\nFATAL ERROR - Could not allocate tasks!");
        //  Eventually expect watchdog reset
        while (1);
    }

    // Start the scheduler, that starts running the tasks
    vTaskStartScheduler();

    // If this section of code is reached that means the main task could not be allocated
    // This is a fatal failure. The sensor will not operate.
    portDBG_TRACE("FATAL ERROR: Could not start scheduler");  //or use "DEBUG()"
    while(1);

    return 0;
}
/*-----------------------------------------------------------*/





static void main_InitBoard( void )
{
  #if (BOARD != USER_BOARD)
    #if (BOARD == STK600_RCUC3C0)
      #error "Initializing for STK600 dev board.  Use BOARD=USER_BOARD"
    #else
      #error "Invalid board definition.  Use BOARD=USER_BOARD"
    #endif
  #endif

  //  Initialize GPIO pins for all components and utilities.
  //  Start with those components that potentially consume
  //  power, and turn off as soon as possible.
  //

  //  The spectrometers
  //
  CGS_InitGpio();

  //  TODO - Write CT API functions, minimal: CT_InitGpio()  !!!
  //
  gpio_configure_pin(CT_ON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// ensure CT sensor is OFF
//gpio_configure_pin(CT_RXEN_N, ...

  // Configure maintenance port pins
  gpio_configure_pin(MNT_RXEN_N, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);  // receiver enabled
  gpio_configure_pin(MNT_FORCEOFF_N, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);  // transmitter enabled
  gpio_configure_pin(MNT_FORCEON, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);  // RS-232 forceon input low
  gpio_configure_pin(MNT_RXINVALID_N, GPIO_DIR_INPUT);  // MNT_nRXINVALID is an input

  // the actual maintenance port (debug) port gets initialized automatically in the freertos example code

  // with all ancillary power disabled, no need to configure all pins - default tristate should be ok
  // (we aren't worried about minimum power consumption)

  //  Configure the GPIO lines used for the
  //  controller-board to spectroemter-board communication
  //
  SPI_InitGpio();

#if (MCU_BOARD_REV==REV_A) 
  gpio_configure_pin(RLY_EN, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
#endif  
  
 
  //  Initialize gpio pins for ADCs
  //
  ad7677_InitGpio();

  //  The FIFOs
  //
#if (MCU_BOARD_REV==REV_A) 
  //  No FIFOs in Rev A
#else	// rev B
  SN74V283_InitGpio();
#endif  
  
  //  The two shutters in the radiometer head
  //
  shutters_InitGpio();
  
  //  The Paro-Scientific Pressure Sensor
  //
  PRES_InitGpio();

  portDBG_TRACE("Initialized PCB GPIOs...");  
  
  //TODO initialize board

  //main_InitTWIInterface();  // initialize TWI (I2C) interface

}

# if 0
#include "telemetry.h"

//*****************************************************************************
// Stack overflow hook
void vApplicationStackOverflowHook(xTaskHandle* pxTask_notUsed, signed portCHAR *pcTaskName)
{
  tlm_send( "\r\n\r\n", 4, 0 );
  tlm_send( "Stack overflow in ", 18, 0 );
  tlm_send( (void*)pcTaskName, strlen((char*)pcTaskName), 0);
  tlm_send( "\r\n\r\n", 4, 0 );

  while(1);
}
# endif
