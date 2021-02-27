/*!	\file main.controller.c
 *
 *	\brief Main function for the HyperNAV Controller firmware.
 *
 *	@author Burkhard Plache
 *	@date   2015-08-02
 *
 **********************************************************************************/

//	These include files are standard C
//
//	# include None

//	These include files are for Atmel provided components
//
#  include "FreeRTOS.h"
#  include "task.h"

# include <string.h>

# include "intc.h"
# include "flashc.h"
# include "watchdog.h"
# include "telemetry.h"

//	These include files support low level functionality
//
# include "power.h"
# include "hypernav.sys.controller.h"

//	These include files are in the local src directory
//
# include "command.controller.h"
# include "tasks.controller.h"
# include "aux_data_acquisition.h"
# include "stream_log.h"
# include "profile_manager.h"
# include "data_exchange.controller.h"

# include "extern.controller.h"

char* BFile = __FILE__;
char* BDate = __DATE__;
char* BTime = __TIME__;

//*****************************************************************************
// Local Variables
//*****************************************************************************

#if 0 && defined(ENABLE_USB)

//  Telemetry mode switching task handler
xTaskHandle TLMModeTaskHandler = NULL;

static void vTlmModeTask(void* pvParameters)
{
    portTickType xLastWakeTime = xTaskGetTickCount();
    //static bool vbus_int_enabled = false;

    while (TRUE)
    {
        vTaskDelayUntil(&xLastWakeTime, configTSK_TLM_MODE_PERIOD_MS/portTICK_RATE_MS);

        // Switch to appropriate telemetry mode on USB VBus transitions

        // If current telemetry is USART and USB is plugged in switch to USB telemetry
        if( tlm_getMode() == USART_TLM && Is_usb_vbus_high() && Is_device_enumerated()) {
            tlm_switchToUSBTLM();

            //SYSLOG ( SYSLOG_DEBUG, "Switching to USB telemetry.");
        }

        // If current telemetry is USB and USB cable is unplugged switch to USART telemetry
        if( tlm_getMode() == USB_TLM && !Is_usb_vbus_high()) {
            tlm_switchToSerialTLM();
            //SYSLOG ( SYSLOG_DEBUG, "Switching to USART telemetry.");
        }
    }
}

#endif


//*****************************************************************************
// Stack overflow hook
void vApplicationStackOverflowHook( __attribute__((unused)) xTaskHandle* pxTask_notUsed, signed portCHAR *pcTaskName)
{
  tlm_send( "\r\n\r\n", 4, 0 );
  tlm_send( "Stack overflow in ", 18, 0 );
  tlm_send( (void*)pcTaskName, strlen((char*)pcTaskName), 0);
  tlm_send( "\r\n\r\n", 4, 0 );

  while(1);
}

//*****************************************************************************
// Idle task hook
void vApplicationIdleHook( void )
{
}

static Bool create_controller_tasks() {

    // Create tasks
    Bool tasksCreatedOK = TRUE;

    // Start Setup+Command Task
    tasksCreatedOK &= command_controller_createTask();

    // Start Task-Monitor Task
    tasksCreatedOK &= taskMonitor_controller_createTask();

    // Data Exchange Hub
    tasksCreatedOK &= data_exchange_controller_createTask();

    // Auxiliary Data Acquisition (OCR504 & MCOMS)
    tasksCreatedOK &= aux_data_acquisition_createTask();

# if defined(OPERATION_AUTONOMOUS)
    // Start Stream-Output & Log-to-File Task
    tasksCreatedOK &= stream_log_createTask();
# endif

# if defined(OPERATION_NAVIS)
    // Profile Management
    tasksCreatedOK &= profile_manager_createTask();
# endif

    return tasksCreatedOK;
}



// Main: Start tasks and scheduler
int main(void) {

    hnv_sys_ctrl_board_init();

    //  Initialize watchdog to reset after 15 seconds.
    watchdog_enable ( 30000000LL );
	
    //  Drop to boot loader if this is a complete failure
    U32 const boot_mode = 0xA5A5BEEFL;
    flashc_memcpy((void *)(AVR32_FLASHC_USER_PAGE_ADDRESS+0), &boot_mode, sizeof(boot_mode), true);

    //  Make sure we don't start if the power is insufficient (i.e. brown out)
    pwr_BOR();

    //  Initialize interrupt handling.
    INTC_init_interrupts();

    //  Enable interrupts globally.
    Enable_global_interrupt();

    if( !create_controller_tasks() ) {
        //  If this fails, trigger watchdog reset
        while (1);
    }

    //  Restart application after watchdog reset if getting this far
    U32 const app_mode = 1234567890L;
    flashc_memcpy((void *)(AVR32_FLASHC_USER_PAGE_ADDRESS+0), &app_mode, sizeof(boot_mode), true);

    //  Start task scheduler
    vTaskStartScheduler();

    //  Will only get past vTaskStartScheduler()
    //  if there was insufficient memory to create the idle task.
    //  Otherwise the scheduler will be running tasks.

    //  This is a complete failure.
	//  Drop to boot loader.
    flashc_memcpy((void *)(AVR32_FLASHC_USER_PAGE_ADDRESS+0), &boot_mode, sizeof(boot_mode), true);

    //  Trigger watchdog reset
    while (1);

    //  Avoid compiler warning
    return 0;
}


