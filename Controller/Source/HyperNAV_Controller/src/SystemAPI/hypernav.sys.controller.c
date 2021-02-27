/*! \file hypernav.sys.controller.c *****************************************
 *
 * \brief HyperNav system - controller - control
 *
 *
 * @author      BP, Satlantic
 * @date        2015-08-19
 *
  ***************************************************************************/

#include "hypernav.sys.controller.h"

#include <compiler.h>
#include <board.h>
#include "power_clocks_lib.h"
#include "gpio.h"
#include "spi.h"
#include "delay.h"
#include "intc.h"
#include "pm.h"
#include "smc.h"
#include "files.h"
#include "power.h"
#include "sysmon.h"
#include "systemtime.h"
#include "onewire.h"
#include "modem.h"
#include "telemetry.h"
#include "avr32rerrno.h"

// SPI
#include "board.h"

// USB
#include "usb_task.h"
#include "device_mass_storage_task.h"
#include "ctrl_access.h"

// PCB Supervisor
#include "PCBSupervisor.h"
#include "PCBSupervisor_cfg.h"
#include "cycle_counter.h"


//*****************************************************************************
// Version
//*****************************************************************************

// -- API Version Info --
#define	VER_MAJOR	1
#define	VER_MINOR	1
#define	VER_PATCH	0

// Variant information (to accommodate for customizations)

// Default firmware variant
#define VER_FWV_DEFAULT	0

#if FW_VARIANT==VER_FWV_DEFAULT
#define VER_FWV_DESCRIPTION	"default"
#else
#error "Unknown or unspecified variant, cannot build."
#endif


//*****************************************************************************
// Local functions implementation
//*****************************************************************************

/*! \brief Initialize SPI bus 0 (for Sysmon, ExtRTC, DataFlash, Keller, & Expansion)
 *
 *  \param fPBA  Frequency in Hz of SPI module input clock (peripheral bus A clock)
 */
//static
void initSPI0(U32 fPBA)
{
	  // GPIO map for SPI.
	  static const gpio_map_t spi0Map = SPI_MASTER_0_PINS_CFG;

	  // SPI chip select options
	  spi_options_t spiOptions03 = SPI_MASTER_0_OPTIONS_CS0;
	  spi_options_t spiOptions47 = SPI_MASTER_0_OPTIONS_CS1;

	  // Assign I/Os to SPI.
	  gpio_enable_module(spi0Map, sizeof(spi0Map) / sizeof(spi0Map[0]));


	  // Initialize as master.
	  // (Any options set can be passed as only the mode fault detection field is used by 'spi_initMaster()')
	  spi_initMaster(SPI_MASTER_0, &spiOptions03);

	  // Set selection mode: fixed peripheral select, pcs decode mode, no delay between chip selects.
	  spi_selectionMode(SPI_MASTER_0, 0, SPI_MASTER_0_DECODE_MODE, 0);

	  // Configure chip-selects
	  //
	  // CS0 to CS3
	  spi_setupChipReg(SPI_MASTER_0, &spiOptions03, fPBA);
	  // CS4 to CS7
	  spi_setupChipReg(SPI_MASTER_0, &spiOptions47, fPBA);

	  // Enable internal pull-up on MISO line
	  gpio_enable_pin_pull_up(AVR32_SPI0_MISO_0_0_PIN);

	  // Enable SPI.
	  spi_enable(SPI_MASTER_0);
}


/*! \brief Initialize SPI bus 1 (for Spec Board)
 *
 *  \param fPBA  Frequency in Hz of SPI module input clock (peripheral bus A clock)
 */
// static
void initSPI1(U32 fPBA)
{
	// GPIO map for SPI.
	static const gpio_map_t spi1Map = SPI_MASTER_1_PINS_CFG;

	// SPI chip select options
	spi_options_t spi1Options = SPI_MASTER_1_OPTIONS_CS0;

	// Assign I/Os to SPI.
	gpio_enable_module( spi1Map, sizeof(spi1Map) / sizeof(spi1Map[0]) );

	// Initialize as master.
	// (Any options set can be passed as only the mode fault detection field is used by 'spi_initMaster()')
	spi_initMaster( SPI_MASTER_1, &spi1Options );

	// Set selection mode: fixed peripheral select, pcs decode mode, no delay between chip selects.
	spi_selectionMode( SPI_MASTER_1, SPI_MASTER_1_VARIABLE_PS, SPI_MASTER_1_DECODE_MODE, SPI_MASTER_1_PBA_CS_DELAY );

	// Configure chip-selects
	// CS0
	spi_setupChipReg( SPI_MASTER_1, &spi1Options, fPBA );

	// Enable internal pull-up on MISO line
	gpio_enable_pin_pull_up( AVR32_SPI1_MISO_0_0_PIN) ;

	// Enable SPI.
	spi_enable( SPI_MASTER_1 );

	// Set up the additional custom control lines.
	// I think I must be doing this incorrectly - there are no simple config or direction commands.
	//gpio_local_init();
	//gpio_local_enable_pin_output_driver(CTRL_RDY);
	//gpio_enable_gpio_pin(CTRL_RDY);
	//gpio_local_set_gpio_pin(CTRL_RDY);
	gpio_enable_pin_pull_up(CTRL_RDY);
	gpio_set_gpio_pin(CTRL_RDY);
	// SPEC1_RDY__INT2 is an input
	// SPEC2_RDY__INT7 is an input
	
	// TO DO: Set up interrupts, if needed
	// SPEC1_RDY__INT2_LINE
	// SPEC2_RDY__INT7_LINE

}


//!\brief Initialize external bus interface (memory controller(s))
static void initExternalBusInterface(void)
{
#ifndef NO_EBI
	smc_init(FOSC0);
#else
	#warning "No EBI support!"
#endif
}


//!\brief Initialize unused resources
static void initUnused(void)
{
#ifndef PCB_SUPERVISOR_SUPPORTED
	// Spare GPIO
	gpio_enable_pin_pull_up(SPARE_GPIO);
#else
	gpio_set_gpio_pin(SUPERVISOR_CSn);	// ensure supervisor is deselected
#endif

	// Pressure sensor
// 	gpio_enable_pin_pull_up(PRES_RXD_PIN);
// 	gpio_enable_pin_pull_up(PRES_TXD_PIN);
// 	gpio_enable_pin_pull_up(PRES_ON_PIN);
// 	gpio_enable_pin_pull_up(PRES_RTS_DE_PIN);

	//Aux pump control
// 	gpio_enable_pin_pull_up(PUMP_D_FROM_SENSOR_PIN);
// 	gpio_enable_pin_pull_up(PUMP_D_TO_SENSOR_PIN);
// 	gpio_clr_gpio_pin(PUMP_PWR_ON_PIN);
// 	gpio_clr_gpio_pin(PUMP_SENS_COMM_ON_PIN);
// 	gpio_clr_gpio_pin(PUMP_SENS_PWR_ON_PIN);
}
//*****************************************************************************
// Local variables
//*****************************************************************************
static hnv_sys_ctrl_status_t nsysInit;


//*****************************************************************************
// Exported functions implementation
//*****************************************************************************
#if 0
//!	\brief re-initialize system when waking up from USB sleep
S16 hnv_sys_ctrl_reinit_fromUSB(hnv_sys_ctrl_param_t* initParameter, hnv_sys_ctrl_status_t* initStatus )
{
	// as most modules weren't disabled,we can do a minimal re-initialization sequence

	// ensure hardware modules disabled
	gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);
	gpio_set_gpio_pin(SUPERVISOR_CSn);

	// -- Initialize unused resources --
	initUnused();

	// Enable internal pull-up on MISO line so that SPI works again
	gpio_enable_pin_pull_up(AVR32_SPI0_MISO_0_0_PIN);

}
#endif

//! \brief Initializes the PCB on power up or reset to a safe, predictable state
void hnv_sys_ctrl_board_init(void) {
#ifdef E980030_H_
	// Controller board can reset spectrometer MCU - let's hold it in reset until we are ready
	gpio_disable_pin_pull_up(SPEC_RESET_N);				// explicitly disable internal pull-up resistor - use pullup on spec board
	gpio_clr_gpio_open_drain_pin(SPEC_RESET_N);			// force and hold spec MCU in reset state
#else
	#error UNKNOWN PCB!
#endif
}

void hnv_sys_spec_board_start()
{
	gpio_set_gpio_open_drain_pin(SPEC_RESET_N);			// allow spec MCU to run
}

void hnv_sys_spec_board_stop()
{
	gpio_clr_gpio_open_drain_pin(SPEC_RESET_N);			// force spec MCU to reset state
}

void hnv_sys_spec_board_reset()
{
	gpio_clr_gpio_open_drain_pin(SPEC_RESET_N);			// force spec MCU to reset state
    vTaskDelay( (portTickType)TASK_DELAY_MS( 2 ) );    // allow time for MCU to reset
	gpio_set_gpio_open_drain_pin(SPEC_RESET_N);			// allow spec MCU to run
}


//! \brief Initialize HyperNav system.
S16 hnv_sys_ctrl_init(hnv_sys_ctrl_param_t* initParameter, hnv_sys_ctrl_status_t* initStatus )
{
	// Initialize result
	nsysInit.cpu = FALSE;
	nsysInit.pwr = FALSE;
	nsysInit.time = FALSE;
	nsysInit.sysmon = FALSE;
	nsysInit.tlm = FALSE;
	nsysInit.mdm = FALSE;
	nsysInit.file = FALSE;

	// ensure hardware modules disabled
	gpio_clr_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP); //gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);
	gpio_set_gpio_pin(SUPERVISOR_CSn);

	do
	{
		//-- Initialize MCU --
		// Switch to external oscillator 0.
		if(pcl_switch_to_osc(PCL_OSC0, FOSC0, OSC0_STARTUP) != PASS)
		{
			avr32rerrno = EMCU;
			break;
		}
		nsysInit.cpu = TRUE;

		// -- Initialize unused resources --
		initUnused();

		//-- Initialize EBI --
		initExternalBusInterface();

		//-- Initialize SPI Bus 0 Master ---
		initSPI0(FOSC0);
		vTaskDelay( (portTickType)TASK_DELAY_MS( 500 ) );

		//-- Initialize SPI Bus 1 Master ---
		// Moved further down, because locating it here caused a lock-up of the firmware at startup.
		// BP 2016-July-21
  		// initSPI1(FOSC0);
		  #warning "SKF 2016-10-10 This lockup may have been caused by invalid assignment of SYSMON_ON (SUNA carryover)"
		 
		//-- Initialize telemetry port --
		if(tlm_init(FOSC0, initParameter->tlm_baudrate) != TLM_OK)
		break;
		nsysInit.tlm = TRUE;

		//-- Initialize modem port --
		if(mdm_init(FOSC0, initParameter->mdm_baudrate) != MDM_OK)
		break;
		nsysInit.mdm = TRUE;

		//-- Initialize power sub-system --
		if(pwr_init() != PWR_OK) 
			break;
		nsysInit.pwr = TRUE;

		//-- Initialize system clock --
		if(time_init(FOSC0) != NTIME_OK) 
			break;
		nsysInit.time = TRUE;

		//-- Initialize System Monitor --
		if(sysmon_init(FOSC0) != SYSMON_OK) 
			break;
		nsysInit.sysmon = TRUE;

	/*
		//test hardware reset of EMMC
		gpio_clr_gpio_pin(AVR32_PIN_PB00);
		cpu_delay_us(100, FOSC0);
		gpio_set_gpio_pin(AVR32_PIN_PB00);
	*/

		//-- Initialize file system --
		if(!f_fsProbe()) { //break;

			// attempt hardware reset of eMMC
			gpio_clr_gpio_pin(EMMC_NRST);	// ensure reset line low
            vTaskDelay( (portTickType)TASK_DELAY_MS( 1 ) );
			gpio_set_gpio_pin(EMMC_NRST);	// trigger rest
            vTaskDelay( (portTickType)TASK_DELAY_MS( 1 ) ); // wait tRSCA for init to complete (200 us min)

			// retry init
			if(!f_fsProbe()) break;
		}
		nsysInit.file = TRUE;	

	//-- Initialize SPI Bus 1 Master ---
	// Moved here from further up.
	// BP 2016-July-21
//	initSPI1(FOSC0);
//  vTaskDelay( (portTickType)TASK_DELAY_MS( 500 ) );
//
//  Moved out of here into  command.controller.c :: StartController() - BP 2016-07-21

		// All sub-systems initialized correctly.
		*initStatus = nsysInit;
		return HNV_SYS_CTRL_OK;

	} while(0);

	// Something went wrong
	*initStatus = nsysInit;
	return HNV_SYS_CTRL_FAIL;
}



S16 hnv_sys_ctrl_startUSB() {
#if defined(ENABLE_USB) && defined(FREERTOS_USED)
	//-- Initialize USB --
	if ( !ctrl_access_init() )
		return HNV_SYS_CTRL_FAIL;

	// Initialize USB clock.
	pcl_configure_usb_clock();

	// Initialize USB tasks.
	usb_task_init();				// Composite device task

	// Mass storage task
	device_mass_storage_task_init();
#endif
	return HNV_SYS_CTRL_OK;
}


//! \brief De-initialize HyperNav system.
//S16 hnv_sys_ctrl_deinit(Bool keepTLM)
S16 hnv_sys_ctrl_deinit(void)
{
	//-- De-initialize file system --
	f_closeAll();

	//-- De-initialize system monitor --
	if(nsysInit.sysmon) sysmon_deinit();
    vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );

	//-- De-initialize telemetry port --
	//if ((!keepTLM) &&(nsysInit.tlm)) tlm_deinit();
	if (nsysInit.tlm) tlm_deinit();

	//-- De-initialize modem port --
	if (nsysInit.mdm) mdm_deinit();

	//-- De-initialize SPI0 --
	// SPI0 MOSI should go high-Z, otherwise it could actively drive powered-off ICs
	gpio_set_gpio_open_drain_pin(AVR32_SPI0_MOSI_0_0_PIN);
	// Disable SPI0 MISO pull-up to avoid driving powered-off ICs
	gpio_disable_pin_pull_up(AVR32_SPI0_MISO_0_0_PIN);


	return HNV_SYS_CTRL_OK;
}


//! \brief Query HyperNav system driver version
void hnv_sys_ctrl_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[])
{
	if(major != NULL)	*major = VER_MAJOR;
	if(minor != NULL)	*minor = VER_MINOR;
	if(patch != NULL)	*patch = VER_PATCH;
	if(variant != NULL)	*variant = FW_VARIANT;
	if(var_desc != NULL) *var_desc = VER_FWV_DESCRIPTION;
}


//! \brief Check last reset cause
//! @return 'HNV_SYS_CTRL_WDT_RST' 		- Last reset caused by watchdog timer expiration
//! @return 'HNV_SYS_CTRL_PCYCLE_RST' 	- Last reset caused by power cycling
//! @return 'HNV_SYS_CTRL_EXT_RST' 		- Last reset caused by asserting the external RESET line
//! @return 'HNV_SYS_CTRL_BOD_RST' 		- Last reset caused by internal brown-out detection circuit on 3.3V line
//! @return 'HNV_SYS_CTRL_CPUERR_RST' 	- Last reset caused by CPU error
//! @return	'HNV_SYS_CTRL_UNKNOWN_RST'	- Last reset caused by unknown causes
U8	hnv_sys_ctrl_resetCause(void)
{
	volatile avr32_pm_t* pm = &AVR32_PM;

	// Check reset cause
	if(pm->RCAUSE.wdt)
		return HNV_SYS_CTRL_WDT_RST;
	if(pm->RCAUSE.por)
		return HNV_SYS_CTRL_PCYCLE_RST;
	if(pm->RCAUSE.ext)
		return HNV_SYS_CTRL_EXT_RST;
	if(pm->RCAUSE.bod)
		return HNV_SYS_CTRL_BOD_RST;
	if(pm->RCAUSE.cpuerr)
		return HNV_SYS_CTRL_CPUERR_RST;
	else
		return HNV_SYS_CTRL_UNKNOWN_RST;
}


