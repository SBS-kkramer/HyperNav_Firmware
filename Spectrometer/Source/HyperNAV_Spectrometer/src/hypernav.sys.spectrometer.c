/*! \file hypernav.sys.spectrometer.c *****************************************
 *
 * \brief HyperNav system - spectrometer - control
 *
 *
 * @author      BP, Satlantic
 * @date        2015-08-19
 *
  ***************************************************************************/

#include "hypernav.sys.spectrometer.h"

#include <compiler.h>
#include <board.h>

//  ASF
#include "gpio.h"
# include "sysclk.h"

#include "power_clocks_lib.h"
#include "spi.h"
#include "intc.h"
#include "ast.h"
#include "wdt.h"
#include "FreeRTOS.h"
#include "task.h"
//#include "pm.h"
#include "smc.h"
//#include "sysmon.h"
#include "systemtime.spectrometer.h"
//#include "onewire.h"
#include "telemetry.h"
//#include "spectrometer.h"

#include "avr32rerrno.h"
#include "power.spectrometer.h"
#include "twi_mux.h"

# if 0
// USB
#include "usb_task.h"
#include "device_mass_storage_task.h"
#include "ctrl_access.h"

// PCB Supervisor
#include "PCBSupervisor.h"
#include "PCBSupervisor_cfg.h"
#include "cycle_counter.h"
# endif

// for SPI interrupt
//#include <avr/interrupt.h>
#include "io_funcs.spectrometer.h"

#include "spi.spectrometer.h"
#include <stdio.h>

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
// Local functions
//*****************************************************************************
static void initExternalBusInterface(void);	// Initialize EBI for external RAM and Spectrometer interface
static void initSPI0(U32 fPBA);				// Initialize SPI bus 0 as Slave (for board-to-board communication)
static void initSPI1(U32 fPBA);				// Initialize SPI bus 1 as Master (for flash memory)
static void initUnused(void);				// Initialize unused resources


//*****************************************************************************
// Local variables
//*****************************************************************************

// Sensor driver status. This variable holds the initialization status of several sub-systems.
static hnv_sys_spec_status_t nsysInit;


//! \brief Initialize HyperNav system.
S16 hnv_sys_spec_init(hnv_sys_spec_param_t* initParameter, hnv_sys_spec_status_t* initStatus )
{
	// Initialize result
	nsysInit.cpu = FALSE;
	nsysInit.time = FALSE;
	nsysInit.sysmon = FALSE;
	nsysInit.tlm = FALSE;
	nsysInit.spec = FALSE;

	// ensure hardware modules disabled
	// !!	gpio_clr_gpio_open_drain_pin(PWR_SYSTEM_NSLEEP);
	//gpio_clr_gpio_pin(PWR_SYSTEM_NSLEEP);

	do
	{
		nsysInit.cpu = TRUE;
		#warning system clock initialization moved to main()

		// -- Initialize unused resources --
		initUnused();

		//-- Initialize EBI --
		initExternalBusInterface();

		portDBG_TRACE("EBI init");

		//-- Initialize SPI Bus 0 Slave ---
		initSPI0(sysclk_get_pbc_hz());		// SPI0 uses PBC clock domain			//initSPI0(FOSC0);
		//#warning Check initSPI0() to make sure it still works after clock freq change

		portDBG_TRACE("SPI0 init");
		vTaskDelay((portTickType)TASK_DELAY_MS( 500 ));
		//		portDBG_TRACE("Delay by 500ms");
		//		portDBG_TRACE("Nothing");

		//-- Initialize SPI Bus 1 Master ---
		initSPI1(sysclk_get_pbc_hz());		// SPI1 uses PBC clock domain			//initSPI0(FOSC0);
		//#warning Check initSPI1() to make sure it still works after clock freq change

		portDBG_TRACE("SPI1 init");
		vTaskDelay((portTickType)TASK_DELAY_MS( 500 ));
		//		portDBG_TRACE("Delay by 500ms");
		//		portDBG_TRACE("Nothing");

		//-- Initialize telemetry port --
		//if(tlm_init(FOSC0, initParameter->baudrate) != TLM_OK)   <- preferred
		//#warning Check tlm_init() to make sure it still works after clock freq change
		//if(tlm_init(FOSC0, TLM_USART_BAUDRATE) != TLM_OK)
		// USART3
		if(tlm_init(TLM_USART_CLOCK, TLM_USART_BAUDRATE) != TLM_OK)
			break;
		nsysInit.tlm = TRUE;
		portDBG_TRACE("tlm init");

		//-- Initialize system clock --
		// Note that the 32 kHZ clock is started in Main
//		if(time_init(FOSC0) != NTIME_OK)
	//		break;

		if (STIME_time_init() != STIME_OK)
			break;
		nsysInit.time = TRUE;
		portDBG_TRACE("STIME init");

		//-- Initialize System Monitor --
// !!		if(sysmon_init(FOSC0) != SYSMON_OK) break;
		nsysInit.sysmon = TRUE;

		//-- Initialize spectrometer --
// !!		if(spec_init(FOSC0) != SPEC_OK)	break;
		nsysInit.spec = TRUE;

		//-- Initialize Two-Wire Multiplexer
		twi_mux_start();
		portDBG_TRACE("twi_mux_start");

		// All sub-systems initialized correctly.
		*initStatus = nsysInit;
		return HNV_SYS_SPEC_OK;

	} while(0);

	// Something went wrong
	*initStatus = nsysInit;
	return HNV_SYS_SPEC_FAIL;
}


//! \brief Initialize USB.
S16 hnv_sys_spec_startUSB() {
#if defined(ENABLE_USB) && defined(FREERTOS_USED)
	//-- Initialize USB --
	if ( !ctrl_access_init() )
		return HNV_SYS_SPEC_FAIL;

	// Initialize USB clock.
	pcl_configure_usb_clock();

	// Initialize USB tasks.
	usb_task_init();				// Composite device task

	// Mass storage task
	device_mass_storage_task_init();
#endif
	return HNV_SYS_SPEC_OK;
}


//! \brief De-initialize HyperNav system.
//S16 hnv_sys_spec_deinit(Bool keepTLM)
S16 hnv_sys_spec_deinit(void)
{
	//-- De-initialize spectrometer --
// !!	if(nsysInit.spec) spec_deinit();

	//-- De-initialize system monitor --
// !!	if(nsysInit.sysmon) sysmon_deinit();
//delay_ms( 1000 );

	//-- De-initialize telemetry port --
	//if ((!keepTLM) &&(nsysInit.tlm)) tlm_deinit();
// !!	if (nsysInit.tlm) tlm_deinit();

	//-- De-initialize SPI0 --
	// SPI0 MOSI should go high-Z, otherwise it could actively drive powered-off ICs
	gpio_set_gpio_open_drain_pin(AVR32_SPI0_MOSI_0_0_PIN);
	// Disable SPI0 MISO pull-up to avoid driving powered-off ICs
	gpio_disable_pin_pull_up(AVR32_SPI0_MISO_0_0_PIN);


	return HNV_SYS_SPEC_OK;
}

# if 0
//! \brief Query HyperNav system driver version
void hnv_sys_spec_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[])
{
	if(major != NULL)	*major = VER_MAJOR;
	if(minor != NULL)	*minor = VER_MINOR;
	if(patch != NULL)	*patch = VER_PATCH;
	if(variant != NULL)	*variant = FW_VARIANT;
	if(var_desc != NULL) *var_desc = VER_FWV_DESCRIPTION;
}
# endif

//! \brief Check last reset cause
//! @return 'HNV_SYS_SPEC_WDT_RST' 		- Last reset caused by watchdog timer expiration
//! @return 'HNV_SYS_SPEC_PCYCLE_RST' 	- Last reset caused by power cycling
//! @return 'HNV_SYS_SPEC_EXT_RST' 		- Last reset caused by asserting the external RESET line
//! @return 'HNV_SYS_SPEC_BOD_RST' 		- Last reset caused by internal brown-out detection circuit on 3.3V line
//! @return 'HNV_SYS_SPEC_CPUERR_RST' 	- Last reset caused by CPU error
//! @return 'HNV_SYS_SPEC_UNKNOWN_RST'	- Last reset caused by unknown causes
U8	hnv_sys_spec_resetCause(void)
{
	volatile avr32_pm_t* pm = &AVR32_PM;

	// Check reset cause
	if(pm->RCAUSE.wdt)
		return HNV_SYS_SPEC_WDT_RST;
	if(pm->RCAUSE.por)
		return HNV_SYS_SPEC_PCYCLE_RST;
	if(pm->RCAUSE.ext)
		return HNV_SYS_SPEC_EXT_RST;
	if(pm->RCAUSE.bod)
		return HNV_SYS_SPEC_BOD_RST;
	if(pm->RCAUSE.cpuerr)
		return HNV_SYS_SPEC_CPUERR_RST;
	else
		return HNV_SYS_SPEC_UNKNOWN_RST;
}



//*****************************************************************************
// Local functions implementation
//*****************************************************************************

//! \brief Initialize SPI bus 0 (for spectrometer-controller inter-board communications)
static void initSPI0(U32 fPBA)
{
	  // GPIO map for SPI.
	  static const gpio_map_t spi0Map = SPI_SLAVE_0_PINS_CFG;

	  // SPI chip select options
	  spi_options_t spiOptions = SPI_SLAVE_0_OPTIONS_CS0;

	  // Assign I/Os to SPI.
	  gpio_enable_module(spi0Map, sizeof(spi0Map) / sizeof(spi0Map[0]));

	  // Initialize as slave
	  spi_initSlave( SPI_SLAVE_0, spiOptions.bits, SPI_SLAVE_0_DECODE_MODE );

	  // Enable SPI.
	  spi_enable( SPI_SLAVE_0 );

	  //  Note: Originally, the GPIO lines were configured here via SPI_InitGpio().
	  //  Moved to very early in the execution, called indirectly from main().
}


//! \brief Initialize SPI bus 1 (for 64 MB flash memory)
static void initSPI1(U32 fPBA)
{
	// GPIO map for SPI.
	static const gpio_map_t spi1Map = SPI_MASTER_1_PINS_CFG;

	// SPI chip select options
	spi_options_t spi1options = SPI_MASTER_1_OPTIONS_CS0;

	// Assign I/Os to SPI.
	gpio_enable_module( spi1Map, sizeof(spi1Map) / sizeof(spi1Map[0]) );

	// Initialize as master
	spi_initMaster( SPI_MASTER_1, &spi1options );

	//Setup configuration for chip connected to CS0
	spi_setupChipReg( SPI_MASTER_1, &spi1options, sysclk_get_pba_hz() );

	// Enable SPI.
	spi_enable( SPI_MASTER_1 );

	//  Note: Originally, the GPIO lines were configured here via SPI_InitGpio().
	//  Moved to very early in the execution, called indirectly from main().
}


//!\brief Initialize external bus interface (memory controller(s))
static void initExternalBusInterface(void)
{
#ifndef NO_EBI
	smc_init(sysclk_get_hsb_hz());			//smc_init(FOSC0);
#else
	#warning "No EBI support!"
#endif
}


// Initialize unused resources
static void initUnused(void)
{
	// Pressure sensor
	//gpio_enable_pin_pull_up(PRES_RXD_PIN);
	//gpio_enable_pin_pull_up(PRES_TXD_PIN);
	//gpio_enable_pin_pull_up(PRES_ON_PIN);
	//gpio_enable_pin_pull_up(PRES_RTS_DE_PIN);

	gpio_get_pin_value(MDM_DTR);
	gpio_get_pin_value(MDM_DSR);
	gpio_get_pin_value(MDM_RX);
	gpio_get_pin_value(MDM_TX);
	gpio_get_pin_value(MDM_RX_EN_N);
	gpio_get_pin_value(MDM_TX_EN);

# if 0
	gpio_get_pin_value(RLY_RST_EN);
# endif

}







//! \brief Reboot sensor
void sys_reboot(void)
{
	// Trigger a WDT reset
	//TODO	comms_msg("\r\nRebooting");
	wdt_opt_t wdtopt = {
		.us_timeout_period = (uint64_t)500000  // Timeout Value
	};
	wdt_enable(&wdtopt);

	vTaskSuspendAll();			// Avoid idle task clearing WDT

	while(1);
}


