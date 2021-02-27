/*! \file power.spectrometer.c *************************************************************
 *
 * \brief System power.spectrometer management.
 * This modules handles all power.spectrometer management related tasks including:
 * - Low power sleep mode with wake-up on external RTC or telemetry activity
 * - Power source monitoring
 * - Appropriate internal MCU modules configuration for power saving
 *
 * @note For information on sleep modes handling see appnote AVR32739.
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-12-08
 *
 * @revised
 * 2011-10-25	-DAS- Brushing up power loss handling
 *
 * 2016-06-30, SF: Adding spectrometer power enable/disable functions
 **********************************************************************************/

# if COMPILE_POWER_SPECTROMETER_C
#include "power.spectrometer.h"

#include "compiler.h"
#include "FreeRTOS.h"

#include "board.h"
//#include "ds3234.h"
//#include "eic.h"
#include "intc.h"
//#include "pm.h"
#include "gpio.h"
#include "avr32rerrno.h"
#include "time.h"
#include "cycle_counter.h"
#include "wdt.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
//#include "files.h"

#include "time.h"
//#include "telemetry.h"
//#include "usb_drv.h"
//#include "config.h"

#include "cgs.h"

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
//static void disableUnusedModules(void);	    // Disable unused core modules
//static void configureInterrupts(void);		// Configure normal operation power related interrupts
//static void configureBORInterrupt(void);	// Configure brown-out reset related interrupt
//static void wakeupHW(void);				    // Wake up external hardware
//static void fSleep(void);					// Fallback sleep
//static void prepareMCU4Sleep(void);		    // MCU prep routine for static sleep (from AppNote AVR32739)
# if 0
static void suspendAllTasks(void);	        // Suspend all tasks (used when shutting down)
# endif
//static void lowPowerStall(void);			// Do nothing until main power is restored or browns out completely

// Power loss protection interrupt handling
//static void powerLossDetectedISR(void) __attribute__ ((naked));								// Power Loss detection
//static portBASE_TYPE powerLossDetectedISR_non_naked(void) __attribute__ ((__noinline__));	// Power Loss detection non_naked behaviour

// Tasks
//static void vPLSTask(void* pvParameters);	// Power Loss shutdown task

//*****************************************************************************
// Local variables
//*****************************************************************************
//static xSemaphoreHandle PLSTaskSemaphore;	// Power Loss task semaphore
//static Bool PLSTaskCreated = false;

//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************
/*! \brief Initialize power
 * @note This function is called once upon system init.
 */
S16 pwr_init(void)
{
	// Disable unused modules to save power
	disableUnusedModules();

	// Configure and enable external interrupt sources
	configureInterrupts();

	// Create Power Loss Shutdown semaphore and task
	if(!PLSTaskCreated)
	{
		vSemaphoreCreateBinary(PLSTaskSemaphore);
		xSemaphoreTake(PLSTaskSemaphore, 1);	 // First call will always pass. We need this so that 'vPLSTask' blocks
		// Create Power Loss Shutdown monitor task
		if(pdPASS != xTaskCreate(vPLSTask, configTSK_PLS_NAME, configTSK_PLS_STACK_SIZE, NULL, configTSK_PLS_PRIORITY, NULL))
			return PWR_FAIL;
		PLSTaskCreated = true;
	}

	// Turn on external HW (and enable shutdown on PWRGOOD de-assertion).
	wakeupHW();

	return PWR_OK;
}


/* \brief Self shutdown
 * @note Sequence is as follows:
 * 1 - De-initialize the board driver
 * 2 - Disconnect from battery supply -> Power should start browning-out. From now on execution can terminate anywhere ahead.
 * 3 - Put board peripheral HW to sleep
 * 4 - Put MCU to sleep. Under normal circumstances this would be the farthest the execution could get.
 * 5 - Shutdown failure handling -> wait for external wake-up and force a WDT reset
 */
void pwr_shutdown(void)
{
	// Close all open files (Note that files must be closed before suspending tasks)
	f_closeAll();

	// System will be de-initialized so suspend all tasks that may be using resources.
	//(If a running task is switched in after the system is deinitialized it can lead to unpredictable behavior)
	vTaskSuspendAll();

	// De-initialize system
	hnv_sys_spec_deinit();

	// Stall in case disconnection from battery fails or relay board is not present or if backup power kicked in.
	lowPowerStall();
}


/*! \brief Disable power monitor emergency shutdown (ESD).
 * Disables self-shutdown on power monitor PWRGOOD de-assertion. To be used to temporarily disable self-shutdowns.
 * @note Use sparingly. For example: when it is known that a safe transient will occur. Re-enable immediately.
 */
void pwr_disableESD(void)
{
	// Disable POWER_GOOD interrupt
	eic_disable_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

	// Disable USB Vbus OK interrupt
	eic_disable_line(&AVR32_EIC, PWR_VBUS_OK_INT_LINE);

}


/*! \brief Enable power monitor emergency shutdown (ESD).
 * Enables self-shutdown on power monitor PWRGOOD de-assertion.
 */
void pwr_enableESD(void)
{
	// Clear any pending interrupt flag
	//! @note: This clears events that occurred while powerloss protection was masked.
	//!        If the interrupt condition persists upon exiting this function the handler will be called.
	eic_clear_interrupt_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);
	eic_clear_interrupt_line(&AVR32_EIC, PWR_VBUS_OK_INT_LINE);

	// Enable POWER_GOOD interrupt
	eic_enable_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

	// Enable USB Vbus OK interrupt
	eic_enable_line(&AVR32_EIC, PWR_VBUS_OK_INT_LINE);

}


#if 1
// TODO Diego: Do we want to use this?
//
/*! \brief Brown-Out Reset routine
 *  @note This routine handles brown out resets. A call to this routine should be the first instruction executed by 'main()'
 */
void pwr_BOR(void)
{
	int count;
	int const countLimit = 1000;
	Bool powerSafe;
	int disconnectAttempts = 5;

	// Check whether the reset cause was a brown-out
	if((&AVR32_PM)->RCAUSE.bod)
	{
		// Disable watchdog
		wdt_disable();

		// Configure wake-up source
		configureBORInterrupt();

		do
		{
			// Ensure external HW shutdown
			gpio_clr_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP);//gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);

			do
			{
				// Go to sleep
				eic_clear_interrupt_line(&AVR32_EIC,PWR_PWRGOOD_INT_LINE);
				prepareMCU4Sleep();
				SLEEP(AVR32_PM_SMODE_STATIC);

				// Power was restored. If power is stable, continue to shut-down. Otherwise go back to sleep.
				count = 0;
				powerSafe = false;
				while(count < countLimit)
				{
					if(gpio_get_pin_value(PWR_PWRGOOD_PIN))
						count++;
					else
						break;
				}
				if(count >= countLimit)
					powerSafe = true;

			}while(!powerSafe);

		}while(disconnectAttempts--); // Repeat until we can disconnect from the battery, in which case a power on reset is generated.

		// Something went very bad: there is enough power, we attempted to disconnect several times and
		// we are still powered (possible relay board failure). We are going to go into sleep mode and wait for a telemetry to
		// wake us up. Then we will force a WDT reset.
		configureInterrupts();										// Reconfigure interrupts for TELEMETRY wake-up
		fSleep();													// Go to sleep until TELEMETRY wake-up, then force WDT reset
	}
}
#endif


//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

// MCU prep routine for static sleep (from AppNote AVR32739)
static void prepareMCU4Sleep(void)
{
	// MCU preparation for static sleep (from AVR32739)
	//
	// 1 . Stop all HSB masters to avoid data transfers during sleep on the bus
	pm_disable_module(&AVR32_PM, AVR32_PDCA_CLK_HSB);
	pm_disable_module(&AVR32_PM, AVR32_PDCA_CLK_PBA);

	// 2 . Read any register on the PB bus to make sure the CPU is not left waiting for a write
	AVR32_INTC.ipr[0];

	// 3 . Disable modules that communicate with external HW before disabling their clocks to avoid erratic behavior
	pm_disable_module(&AVR32_PM, AVR32_USART0_CLK_PBA);	// Telemetry port
	pm_disable_module(&AVR32_PM, AVR32_SPI0_CLK_PBA);	// SPI Bus 0
	pm_disable_module(&AVR32_PM, AVR32_MCI_CLK_PBB);	// MMC controller
	//
}


// Disable unused modules for power saving
static void disableUnusedModules(void)
{

	#ifdef PWR_DISABLE_USART1
	pm_disable_module(&AVR32_PM, AVR32_USART1_CLK_PBA);
#endif

#ifdef PWR_DISABLE_USART2
	pm_disable_module(&AVR32_PM, AVR32_USART2_CLK_PBA);
#endif

#ifdef PWR_DISABLE_USART3
	pm_disable_module(&AVR32_PM, AVR32_USART3_CLK_PBA);
#endif

#ifdef PWR_DISABLE_SSC
	pm_disable_module(&AVR32_PM, AVR32_SSC_CLK_PBA);
#endif

#ifdef PWR_DISABLE_TWIM0
	pm_disable_module(&AVR32_PM, AVR32_TWIM0_CLK_PBA);
#endif

#ifdef PWR_DISABLE_TWIM1
	pm_disable_module(&AVR32_PM, AVR32_TWIM1_CLK_PBA);
#endif

#ifdef PWR_DISABLE_ADC
	pm_disable_module(&AVR32_PM, AVR32_ADC_CLK_PBA);
#endif

#ifdef PWR_DISABLE_ABDAC
	pm_disable_module(&AVR32_PM, AVR32_ABDAC_CLK_PBA);
#endif

#ifdef PWR_DISABLE_USBHS
	pm_disable_module(&AVR32_PM, AVR32_USBB_CLK_PBB);
	pm_disable_module(&AVR32_PM, AVR32_USBB_CLK_HSB);
#endif

#ifdef PWR_DISABLE_HRAM
	pm_disable_module(&AVR32_PM, AVR32_HRAMC0_CLK_HSB);
	pm_disable_module(&AVR32_PM, AVR32_HRAMC1_CLK_HSB);
#endif

#ifdef PWR_DISABLE_DMACA
	pm_disable_module(&AVR32_PM, AVR32_DMACA_CLK_HSB);
#endif

#ifdef PWR_DISABLE_AES
	pm_disable_module(&AVR32_PM, AVR32_AES_CLK_PBB);
#endif

	//Disable OCD if debugging is not used (if reset source is not the debugger)
	if( !((&AVR32_PM)->RCAUSE.jtag) && !((&AVR32_PM)->RCAUSE.jtaghard) && !((&AVR32_PM)->RCAUSE.ocdrst))
	{
		pm_disable_module(&AVR32_PM, AVR32_OCD_CLK_CPU );
	}
}

# if 0
//! Wake-up peripheral hardware. Power-good interrupt needs to be temporarily disabled due to transient in PWR_GOOOD line
//! when de-asserting the SLEEP# line.
static void wakeupHW(void)
{
	// Temporarily disable POWER_GOOD interrupt
	pwr_disableESD();

	//debug hardware indicator:
	//gpio_clr_gpio_pin(AVR32_PIN_PX46);gpio_set_gpio_pin(AVR32_PIN_PX46); gpio_clr_gpio_pin(AVR32_PIN_PX46);

	// Turn on HW and wait for transient to finish
	//gpio_set_gpio_pin(PWR_SYSTEM_NSLEEP);
	gpio_enable_pin_pull_up(PWR_SYSTEM_NSLEEP);
	gpio_set_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP);

/* 2011-12-7, SF: doesn't seem to help glitch
	// try modulating SLEEP# pin to attempt a soft start...
	U16 i;
	for (i=0; i<1000; i++) {
		//cpu_delay_us(1, FOSC0);
		gpio_tgl_gpio_pin(PWR_SYSTEM_NSLEEP);
	}
	gpio_set_gpio_pin(PWR_SYSTEM_NSLEEP);
*/
	/*
	U16 i;
	for (i=0;i<15;i++) {

		//gpio_set_gpio_pin(AVR32_PIN_PX46); gpio_clr_gpio_pin(AVR32_PIN_PX46);
		gpio_tgl_gpio_pin(PWR_SYSTEM_NSLEEP);
	}
	gpio_set_gpio_pin(PWR_SYSTEM_NSLEEP);
	*/

	delay_ms(25);		// was 5 ms, but soft-start of hardware requires additional time

	//gpio_set_gpio_pin(AVR32_PIN_PX46); gpio_clr_gpio_pin(AVR32_PIN_PX46);



	// Re-enable POWER_GOOD interrupt
	pwr_enableESD();
}
# endif

#if 0
//! Fallback sleep. To be used as a fallback in case disconnecting from battery relay fails.
static void fSleep(void)
{
	// Put external HW to sleep
	gpio_clr_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP);//gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);
	gpio_disable_pin_pull_up(PWR_SYSTEM_NSLEEP);	// conserve power - no current through pullup
	delay_ms(100);	// Let any transients stabilize

	// Clear interrupts before going to sleep (normal byte reception fires the interrupt)
	eic_clear_interrupt_line(&AVR32_EIC, PWR_RXFLAG_INT_LINE);
	// Enable wake-up source
	eic_enable_line(&AVR32_EIC, PWR_RXFLAG_INT_LINE);

	// Prevent PWRGOOD from waking up the MCU when in fallback sleep. Theoretically it should not wake it up because PWRGOOD is
	// tied to a SYNC interrupt. However during testing it was found that the MCU would wake-up unless this interrupt was disabled here.
	eic_disable_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

	// MCU preparation for static sleep (from AVR32739)
	prepareMCU4Sleep();

	// Disable WDT to avoid cycling when input voltage is low for a long time (WDT expiration wakes-up the MCU):
	// MCU finds POWERGOOD low and sleeps -> external event wakes up MCU -> WDT reset forced ->
	// MCU finds POWERGOOD low and sleeps -> WDT wakes up MCU -> WDT reset forced -> and so on for ever until voltage becomes good (unlikely to happen)
	wdt_disable();

	// MCU goes to sleep while supply voltage browns-out
	SLEEP(AVR32_PM_SMODE_STATIC);


	//----------Under normal operating conditions this code should never be reached ---------------

	// Shut-down failed (power was not removed), we remained sleeping and some event woke us up => Force a WDT reset
	wdt_enable(PWR_SHUTDOWN_WDT_TIMEOUT_US);
	while(1);

	//---------------------------------------------------------------------------------------------
}
#endif

# if 0
// Do nothing until main power is restored or browns out completely
static void lowPowerStall(void)
{
	struct timeval t1, t2;

	// Disable PWRGOOD interrupt
	eic_disable_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

	// Put external HW to sleep
	gpio_clr_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP);//gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);
	gpio_disable_pin_pull_up(PWR_SYSTEM_NSLEEP);	// conserve power - no current through pullup
	//delay_ms(100);	// Let any transients stabilize
	//All other tasks suspended, delay_ms() not working.
	gettimeofday(&t1, NULL);
	do {
		gettimeofday(&t2, NULL);
	}while ( (1000000*(t2.tv_sec - t1.tv_sec))
			+(t2.tv_usec - t1.tv_usec) < 100000 );


	// TODO: Find the proper duration over which to check for good power.
	//		 5 seconds was initially used, but leads to poor response
	//		 when re-connecting sensor.

	// Monitor PWRGOOD line until brown out or until power in line is reliable for at least 5 continuous seconds.
	// If power is reliable again, force a WDT reset
	U8 powerGoodRequired = 50;
	do
	{
		//	Make sure the power monitoring continues
		//	for as long as the supercaps provide power.
		//	A watchdog reset at this point is undesirable,
		//	unless, of course, the power is good.

		wdt_clear();

		//delay_ms(100);	All other tasks suspended, delay_ms() not working.
		gettimeofday(&t1, NULL);
		do {
			gettimeofday(&t2, NULL);
		}while ( (1000000*(t2.tv_sec - t1.tv_sec))
				+(t2.tv_usec - t1.tv_usec) < 100000 );

		if(gpio_get_pin_value(PWR_PWRGOOD_PIN) || Is_usb_vbus_high())
			powerGoodRequired --;
		else
			powerGoodRequired = 20;

	} while ( powerGoodRequired > 0 );

	// Power is back again, force a reset
	//watchdog_enable(PWR_SHUTDOWN_WDT_TIMEOUT_US);
	wdt_enable( 500000 );
	while(1);
}
# endif

# if 0
//	Using vTaskSuspendAll() instead!
// Suspend all tasks (used when shutting down)
static void suspendAllTasks(void)
{
#ifdef FREERTOS_USED
	/* Telemetry mode monitor task */
	if(TLMModeTaskHandler != NULL)
		vTaskSuspend(TLMModeTaskHandler);

	/* USB device CDC task */
	if(USBCDCTaskHandler != NULL)
		vTaskSuspend(USBCDCTaskHandler);

	/* USB device mass-storage task */
	if(USBMSTaskHandler != NULL)
		vTaskSuspend(USBMSTaskHandler);

	/* USB device task */
	if(usb_device_tsk != NULL)
		vTaskSuspend(usb_device_tsk);

	/* USB task */
	if(usb_device_tsk != NULL)
		vTaskSuspend(USBTaskHandler);


#ifdef USE_FIRMWARE_MAIN

	/* Main HyperNav Task */
	if(MainNitrateTaskHandler != NULL)
		vTaskSuspend(MainNitrateTaskHandler);
#else

	/* API Test Task */
	if(APITestTaskHandler != NULL)
			vTaskSuspend(APITestTaskHandler);

#endif
#endif
}
# endif

//*****************************************************************************
// Interrupts
//*****************************************************************************
#if 1
// Configure brown-out reset related interrupt.
// A level-triggered interrupt is configure to wake-up the MCU when sleeping due to a brown-out reset.
// A high level will be generated by the power monitor when supply voltage returns to safe operation levels.
static void configureBORInterrupt(void)
{
	// EIC options for each pin
	eic_options_t eic_options[1];

	// 'Power good' interrupt request line - Configured as ASYNC to wake up MCU when sleeping after BOR
	//
	// Enable level-triggered interrupt.
	eic_options[0].eic_mode   = EIC_MODE_LEVEL_TRIGGERED;
	// Interrupt will trigger on rising edge / high level (when power comes back)
	eic_options[0].eic_level  = EIC_LEVEL_HIGH_LEVEL;
	// Initialize in asynchronous mode
	eic_options[0].eic_async  = EIC_ASYNCH_MODE;
	// Set the interrupt line number.
	eic_options[0].eic_line   = PWR_PWRGOOD_INT_LINE;


	// Map the interrupt lines to the GPIO pins with the right peripheral functions.
	static const gpio_map_t EIC_GPIO_MAP =
	{
			{PWR_PWRGOOD_PIN, 	PWR_PWRGOOD_PIN_FUNCTION}
	};
	gpio_enable_module(EIC_GPIO_MAP,sizeof(EIC_GPIO_MAP) / sizeof(EIC_GPIO_MAP[0]));

	// Disable all interrupts.
	Disable_global_interrupt();

	// Init the EIC controller with the options
	eic_init(&AVR32_EIC, eic_options,1);

	// Enable EIC for PWR_GOOD pin
	eic_enable_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

	// Disable interrupts for other pins (just in case). The only wake-up source will be power restoration.
	eic_disable_line(&AVR32_EIC, PWR_RTC_ALARM_INT_LINE);
	eic_disable_line(&AVR32_EIC, PWR_RXFLAG_INT_LINE);


	// Enable all interrupts.
	Enable_global_interrupt();

}
#endif

// Configure external interrupts (RTC alarm and serial port for waking-up. System power monitor for 'emergency shutdown')
static void configureInterrupts(void)
{
	const int numIntLines = 4;

	// EIC options for each pin
	eic_options_t eic_options[numIntLines];

	// Enable internal pull-ups
	gpio_enable_pin_pull_up(PWR_PWRGOOD_PIN);
	gpio_enable_pin_pull_up(PWR_RTC_ALARM_PIN);
	gpio_enable_pin_pull_up(PWR_RXFLAG_PIN);
	gpio_enable_pin_pull_up(PWR_VBUS_OK_PIN);

	// 'Power good' interrupt request line
	//
	// Enable edge-triggered interrupt.
	eic_options[0].eic_mode   = EIC_MODE_EDGE_TRIGGERED;
	// Interrupt will trigger on falling edge
	eic_options[0].eic_edge  = EIC_EDGE_FALLING_EDGE;
	// Initialize in synchronous mode : interrupt is synchronized to the clock
	eic_options[0].eic_async  = EIC_SYNCH_MODE;
	// Set the interrupt line number.
	eic_options[0].eic_line   = PWR_PWRGOOD_INT_LINE;

	// RTC alarm interrupt request line
	//
	// Enable level-triggered interrupt.
	eic_options[1].eic_mode   = EIC_MODE_LEVEL_TRIGGERED;
	// Interrupt will trigger on low level
	eic_options[1].eic_level  = EIC_LEVEL_LOW_LEVEL;
	// Initialize in asynchronous mode (needed to wake up from static sleep)
	eic_options[1].eic_async  = EIC_ASYNCH_MODE;
	// Set the interrupt line number.
	eic_options[1].eic_line   = PWR_RTC_ALARM_INT_LINE;

	// Telemetry (serial port) activity
	//
	// Enable level-triggered interrupt.
	eic_options[2].eic_mode   = EIC_MODE_EDGE_TRIGGERED;
	// Interrupt will trigger on falling edge
	eic_options[2].eic_edge  = EIC_EDGE_FALLING_EDGE;
	// Initialize in asynchronous mode (needed to wake up from static sleep)
	eic_options[2].eic_async  = EIC_ASYNCH_MODE;
	// Set the interrupt line number.
	eic_options[2].eic_line   = PWR_RXFLAG_INT_LINE;

	// 'USB VBUS OK' interrupt request line
	//
	// Enable edge-triggered interrupt.
	eic_options[3].eic_mode   = EIC_MODE_EDGE_TRIGGERED;
	// Interrupt will trigger on falling edge
	eic_options[3].eic_edge  = EIC_EDGE_FALLING_EDGE;
	//
	// Enable level-triggered interrupt.
	//eic_options[3].eic_mode   = EIC_MODE_LEVEL_TRIGGERED;
	// Interrupt will trigger on low level
	//eic_options[3].eic_level  = EIC_LEVEL_HIGH_LEVEL;
	// Initialize in synchronous mode : interrupt is synchronized to the clock
	eic_options[3].eic_async  = EIC_SYNCH_MODE;
	// Set the interrupt line number.
	eic_options[3].eic_line   = PWR_VBUS_OK_INT_LINE;


	// Map the interrupt lines to the GPIO pins with the right peripheral functions.
	static const gpio_map_t EIC_GPIO_MAP =
	{
			{PWR_PWRGOOD_PIN, 	PWR_PWRGOOD_PIN_FUNCTION},
			{PWR_VBUS_OK_PIN, 	PWR_VBUS_OK_PIN_FUNCTION},
			{PWR_RTC_ALARM_PIN, PWR_RTC_ALARM_PIN_FUNCTION},
			{PWR_RXFLAG_PIN, 	PWR_RXFLAG_PIN_FUNCTION}
	};
	gpio_enable_module(EIC_GPIO_MAP, numIntLines);

	// Disable all interrupts.
	Disable_global_interrupt();

	// Register an interrupt handler for power loss detection
	INTC_register_interrupt(&powerLossDetectedISR, PWR_PWRGOOD_IRQ, PWR_PWRGOOD_PRIO);
	INTC_register_interrupt(&powerLossDetectedISR, PWR_VBUS_OK_IRQ, PWR_VBUS_OK_PRIO);

	// Init the EIC controller with the options
	eic_init(&AVR32_EIC, eic_options,numIntLines);

	// Enable interrupts for PWR_GOOD pin and USB VBUS OK
	eic_enable_interrupt_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);
	eic_enable_interrupt_line(&AVR32_EIC, PWR_VBUS_OK_INT_LINE);

	// Enable all interrupts.
	Enable_global_interrupt();
}


//! I M P O R T A N T   N O T E
//!
//! In FreeRTOS, interrupt service routines that can result in a context switch, such as the ones
//! implementing deferred interrupt processing, have to be defined in a particular way.
//! These special considerations are highly dependent on the port. The following routines are based on the 'serial.c'
//! example provided along with the AVR32 UC3A FreeRTOS port.


// Power loss detection, may trigger the power loss shutdown routine
__attribute__((__naked__))
void powerLossDetectedISR(void)
{
	/* This ISR can cause a context switch, so the first statement must be a
	call to the portENTER_SWITCHING_ISR() macro.  This must be BEFORE any
	variable declarations. */
	portENTER_SWITCHING_ISR();

	powerLossDetectedISR_non_naked();

	/* Exit the ISR. If a power loss was confirmed then a context switch will occur. */
	portEXIT_SWITCHING_ISR();
}


__attribute__ ((__noinline__))
portBASE_TYPE powerLossDetectedISR_non_naked(void)
{
	portBASE_TYPE xHigherPrioTaskWoken = pdFALSE;	// To tell whether a context switch is needed

	// Filter out brief glitches
	cpu_delay_us(PWR_MAX_GLITCH_US, sysclk_get_cpu_hz());		//FOSC0);

	// Is power still down after a short tolerable glitch? If so, unlock the power loss shutdown (PLS) task.
	// @note: Power loss condition is: (NO main power) AND (NO USB power)

//	if(!gpio_get_pin_value(PWR_PWRGOOD_PIN) && !gpio_get_pin_value(PWR_VBUS_OK_PIN))
//#if (MCU_BOARD_REV==REV_B)
	if(!gpio_get_pin_value(PWR_PWRGOOD_PIN) && !gpio_get_pin_value(PWR_VBUS_OK_PIN) && Is_usb_vbus_low())
//#else
//	if (!gpio_get_pin_value(PWR_PWRGOOD_PIN) && Is_usb_vbus_low())
//#endif
	{
		// Condition will most likely persist so we have to disable the interrupt to allow the PLS task take over
		eic_disable_interrupt_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);

		// Power was not restored -> Unlock Power Loss Shutdown task
		// Because FreeRTOS is not supposed to run with nested interrupts, all OS calls must be in a critical section.
		portENTER_CRITICAL();
		xSemaphoreGiveFromISR(PLSTaskSemaphore, &xHigherPrioTaskWoken);
		portEXIT_CRITICAL();
	}

	// Acklowledge service
	eic_clear_interrupt_line(&AVR32_EIC, PWR_PWRGOOD_INT_LINE);
	eic_clear_interrupt_line(&AVR32_EIC, PWR_VBUS_OK_INT_LINE);

	// The return value will be used by portEXIT_SWITCHING_ISR() to know if it should perform a vTaskSwitchContext().
	return xHigherPrioTaskWoken;
}



//*****************************************************************************
// Power Loss Shutdown Task
//*****************************************************************************

static void vPLSTask(void* pvParameters)
{
	for(;;)
	{
		// Lock until semaphore is given
		xSemaphoreTake(PLSTaskSemaphore, portMAX_DELAY);

		// Shutdown
		pwr_shutdown();
	}
}
# endif



//////////////////////////////////////////////////////////////////



