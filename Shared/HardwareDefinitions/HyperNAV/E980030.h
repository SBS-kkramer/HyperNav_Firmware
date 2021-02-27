/*!	\file E980030.h
 *
 *	\brief	HyperNAV Controller Board hardware definition file
 *
 *	@author	Scott Feener, Satlantic LP
 *	@date	7/6/2015 11:00:24 PM
 */ 

#ifndef E980030_H_
#define E980030_H_

#include "compiler.h"

#if (MCU_BOARD_DN == E980030)
  # ifdef GENERATE_BOARD_MESSAGE
    # pragma message(">>> Compiling for HyperNAV Controller Board - E980030 <<<")
    # warning "FYI >>> Compiling for HyperNAV Controller Board - E980030 <<<"
  # endif
#elif (MCU_BOARD_DN == E980030_DEV_BOARD)
  # ifdef GENERATE_BOARD_MESSAGE
    # warning "FYI >>> Compiling for HyperNAV Controller Development Board (SUNA Motherboard) <<<"	
  # endif
#else 
    # error "Invalid MCU_BOARD_DN symbol definition"
#endif



#if (MCU_BOARD_REV==REV_A)
  # ifdef GENERATE_BOARD_MESSAGE
    # warning "FYI >>> Compiling for revision A <<<"
  # endif
	#define PCB_SUPERVISOR_SUPPORTED	1
//#elif (MCU_BOARD_REV==REV_B)
#else
	#error "Currently unsupported E980030 revision.  Check pin assignments."
#endif


#define SPEC_RESET_N	AVR32_PIN_PB04		// Controller pin that can be used to reset spec board - use as open-drain, spec board has pullup

#if 1
/*! \name Oscillator Definitions
 */
//! @{

// RCOsc has no custom calibration by default. Set the following definition to
// the appropriate value if a custom RCOsc calibration has been applied to your
// part.
//#define FRCOSC          AVR32_PM_RCOSC_FREQUENCY              //!< RCOsc frequency: Hz.

#define FOSC32          32768                                 //!< Osc32 frequency: Hz.
#define OSC32_STARTUP   AVR32_PM_OSCCTRL32_STARTUP_8192_RCOSC //!< Osc32 startup time: RCOsc periods.

#define FOSC0           12000000                              //!< Osc0 frequency: Hz.
#define OSC0_STARTUP    AVR32_PM_OSCCTRL0_STARTUP_2048_RCOSC  //!< Osc0 startup time: RCOsc periods.

#define FOSC1           14645600                              //!< Osc1 frequency: Hz.
#define OSC1_STARTUP    AVR32_PM_OSCCTRL1_STARTUP_2048_RCOSC  //!< Osc1 startup time: RCOsc periods.

//! @}


/*! \name SPI Master Bus 0
 */
//! @{

#define	SPI_MASTER_0			(&AVR32_SPI0)

// Pin settings
#define SPI_MASTER_0_PINS_CFG	{\
	{AVR32_SPI0_SCK_0_0_PIN,  AVR32_SPI0_SCK_0_0_FUNCTION}, \
	{AVR32_SPI0_MISO_0_0_PIN, AVR32_SPI0_MISO_0_0_FUNCTION}, \
	{AVR32_SPI0_MOSI_0_0_PIN, AVR32_SPI0_MOSI_0_0_FUNCTION}, \
	{AVR32_SPI0_NPCS_0_2_PIN, AVR32_SPI0_NPCS_0_2_FUNCTION}, \
	{AVR32_SPI0_NPCS_1_0_PIN, AVR32_SPI0_NPCS_1_0_FUNCTION}, \
	{AVR32_SPI0_NPCS_2_0_PIN, AVR32_SPI0_NPCS_2_0_FUNCTION}, \
	}


// Peripheral selection settings
//#define SPI_MASTER_0_DECODE_MODE	1	//	Decoding
#define SPI_MASTER_0_DECODE_MODE	0	//	No decoding


// Options for CS0 (CS0 to CS3 if decoding is used)
//-------------------------------------------------
// 4MHz
// 8 bits per transfer
// 1-bit delay between slave select and clock
// No delay between transfers
// Stay active after last transfer
// SPI Mode 3 - CPOL=1, NCPHA=0 (setup-falling ,latch-rising & clock normally high)
// Disable mode fault detection
#define SPI_MASTER_0_OPTIONS_CS0	{\
			.reg          = 0,		\
			.baudrate     = 4000000,\
			.bits         = 8,		\
			.spck_delay   = 10,      \
			.trans_delay  = 0,      \
			.stay_act     = 1,      \
			.spi_mode     = 3,      \
			.modfdis      = 1       \
		}


// Options for CS1 (CS4 to CS7 if decoding is used)
//-------------------------------------------------
// 1MHz or 50 kHz
// 8 bits per transfer
// 1-bit delay between slave select and clock
// No delay between transfers
// Stay active after last transfer
// SPI Mode 0 - CPOL=0, CPHA=1 (setup-falling ,latch-rising & clock normally low)
// Disable mode fault detection
#ifndef PCB_SUPERVISOR_SUPPORTED
#define CS1BAUDRATE 1000000
#else
#define CS1BAUDRATE 50000
#endif
#define SPI_MASTER_0_OPTIONS_CS1 {  \
			.reg          = 1,		\
			.baudrate     = CS1BAUDRATE,\
			.bits         = 8,		\
			.spck_delay   = 10,      \
			.trans_delay  = 0,      \
			.stay_act     = 1,      \
			.spi_mode     = 0,      \
			.modfdis      = 1       \
		}

// Add more configs if needed

//! @}


/*! \name SPI Master Bus 1
 */
//! @{

#define	SPI_MASTER_1			(&AVR32_SPI1)

// Pin settings
#define SPI_MASTER_1_PINS_CFG	{\
	{AVR32_SPI1_SCK_0_0_PIN,  AVR32_SPI1_SCK_0_0_FUNCTION}, \
	{AVR32_SPI1_MISO_0_0_PIN, AVR32_SPI1_MISO_0_0_FUNCTION}, \
	{AVR32_SPI1_MOSI_0_0_PIN, AVR32_SPI1_MOSI_0_0_FUNCTION}, \
	{AVR32_SPI1_NPCS_0_0_PIN, AVR32_SPI1_NPCS_0_0_FUNCTION}, \
	{AVR32_SPI1_NPCS_1_1_PIN, AVR32_SPI1_NPCS_1_1_FUNCTION}, \
	}
	
// Peripheral selection settings
//#define SPI_MASTER_1_DECODE_MODE	1	//	Decoding
#define SPI_MASTER_1_DECODE_MODE	0	//	No decoding
#define SPI_MASTER_1_PBA_CS_DELAY   0   //  No delay in peripheral bus A (PBA) periods between chip selects
#define SPI_MASTER_1_VARIABLE_PS    0   //  Target slave is selected in transfer register for every character to transmit

// Options for CS0 (CS0/2 if decoding is used)
//-------------------------------------------------
// 4MHz
// 8 bits per transfer
// 10-bit delay between slave select and clock
// No delay between transfers
// Stay active after last transfer
// SPI Mode 0 - CPOL=0, CPHA=1 (setup-falling ,latch-rising & clock normally low)
// Disable mode fault detection
#define SPI_MASTER_1_OPTIONS_CS0	{\
			.reg          = 0,		\
			.baudrate     = 400000,\
			.bits         = 8,		\
			.spck_delay   = 10,     \
			.trans_delay  = 10,      \
			.stay_act     = 1,      \
			.spi_mode     = 0,      \
			.modfdis      = 1       \
		}

// Add more configs if needed

//! @}



/*! \name Push button(s)
 */
//! @{
#define BUTTON_PUSH_BUTTON_S1          AVR32_PIN_PB10
#define BUTTON_PUSH_BUTTON_S1_PRESSED  0
//! @}


/*! \name Data Flash Memory (AT45DBX)
 */
//! @{
//#include "at45dbx.h"
#define DATAFLASH_SPI			SPI_MASTER_0
#define DATAFLASH_SPI_NPCS		1

// Memory component

//! SPI bus
#define AT45DBX_SPI				DATAFLASH_SPI
//! First chip select used by AT45DBX components on the SPI module instance.
//! AT45DBX_SPI_NPCS0_PIN always corresponds to this first NPCS, whatever it is.
#define AT45DBX_SPI_FIRST_NPCS      DATAFLASH_SPI_NPCS
//! Size of AT45DBX data flash memories to manage.
#define AT45DBX_MEM_SIZE            AT45DBX_2MB
////! Number of AT45DBX components to manage.
#define AT45DBX_MEM_CNT             1
//! SPI master speed in Hz.
#define AT45DBX_SPI_MASTER_SPEED    4000000		// Has to be consistent with SPI configuration

//! @}




/*! \name MCI Connections of the SD/MMC Slots
 */
//! @{
#define SD_SLOT_MCI                       (&AVR32_MCI)
#define SD_4_BIT	DISABLE

// Needed so that the driver does not check for card presence pin when we have an embedded MMC (eMMC)
// Also used for bailing-out card write protection check for eMMCs
#define MCI_EMMC_IN_SLOT_A

//! 4-bits connector pin
#define SD_SLOT_4BITS                      1
#define SD_SLOT_4BITS_CLK_PIN              AVR32_MCI_CLK_0_PIN
#define SD_SLOT_4BITS_CLK_FUNCTION         AVR32_MCI_CLK_0_FUNCTION
#define SD_SLOT_4BITS_CMD_PIN              AVR32_MCI_CMD_1_0_PIN
#define SD_SLOT_4BITS_CMD_FUNCTION         AVR32_MCI_CMD_1_0_FUNCTION
#define SD_SLOT_4BITS_DATA0_PIN            AVR32_MCI_DATA_8_0_PIN
#define SD_SLOT_4BITS_DATA0_FUNCTION       AVR32_MCI_DATA_8_0_FUNCTION
#define SD_SLOT_4BITS_DATA1_PIN            AVR32_MCI_DATA_9_0_PIN
#define SD_SLOT_4BITS_DATA1_FUNCTION       AVR32_MCI_DATA_9_0_FUNCTION
#define SD_SLOT_4BITS_DATA2_PIN            AVR32_MCI_DATA_10_0_PIN
#define SD_SLOT_4BITS_DATA2_FUNCTION       AVR32_MCI_DATA_10_0_FUNCTION
#define SD_SLOT_4BITS_DATA3_PIN            AVR32_MCI_DATA_11_0_PIN
#define SD_SLOT_4BITS_DATA3_FUNCTION       AVR32_MCI_DATA_11_0_FUNCTION
#define SD_SLOT_4BITS_CARD_DETECT          AVR32_PIN_PB08
#define SD_SLOT_4BITS_CARD_DETECT_VALUE    0
#define SD_SLOT_4BITS_WRITE_PROTECT        AVR32_PIN_PB06
#define SD_SLOT_4BITS_WRITE_PROTECT_VALUE  1

//! 8-bits connector pin
#define SD_SLOT_8BITS                      0
#define SD_SLOT_8BITS_CLK_PIN              AVR32_MCI_CLK_0_PIN
#define SD_SLOT_8BITS_CLK_FUNCTION         AVR32_MCI_CLK_0_FUNCTION
// PX40 Test
//#define SD_SLOT_8BITS_CLK_PIN              91
//#define SD_SLOT_8BITS_CLK_FUNCTION         1
#define SD_SLOT_8BITS_CMD_PIN              AVR32_MCI_CMD_0_PIN
#define SD_SLOT_8BITS_CMD_FUNCTION         AVR32_MCI_CMD_0_FUNCTION
#define SD_SLOT_8BITS_DATA0_PIN            AVR32_MCI_DATA_0_PIN
#define SD_SLOT_8BITS_DATA0_FUNCTION       AVR32_MCI_DATA_0_FUNCTION
#define SD_SLOT_8BITS_DATA1_PIN            AVR32_MCI_DATA_1_PIN
#define SD_SLOT_8BITS_DATA1_FUNCTION       AVR32_MCI_DATA_1_FUNCTION
#define SD_SLOT_8BITS_DATA2_PIN            AVR32_MCI_DATA_2_PIN
#define SD_SLOT_8BITS_DATA2_FUNCTION       AVR32_MCI_DATA_2_FUNCTION
#define SD_SLOT_8BITS_DATA3_PIN            AVR32_MCI_DATA_3_PIN
#define SD_SLOT_8BITS_DATA3_FUNCTION       AVR32_MCI_DATA_3_FUNCTION
#define SD_SLOT_8BITS_DATA4_PIN            AVR32_MCI_DATA_4_PIN
#define SD_SLOT_8BITS_DATA4_FUNCTION       AVR32_MCI_DATA_4_FUNCTION
#define SD_SLOT_8BITS_DATA5_PIN            AVR32_MCI_DATA_5_PIN
#define SD_SLOT_8BITS_DATA5_FUNCTION       AVR32_MCI_DATA_5_FUNCTION
#define SD_SLOT_8BITS_DATA6_PIN            AVR32_MCI_DATA_6_PIN
#define SD_SLOT_8BITS_DATA6_FUNCTION       AVR32_MCI_DATA_6_FUNCTION
#define SD_SLOT_8BITS_DATA7_PIN            AVR32_MCI_DATA_7_PIN
#define SD_SLOT_8BITS_DATA7_FUNCTION       AVR32_MCI_DATA_7_FUNCTION
#define SD_SLOT_8BITS_CARD_DETECT          AVR32_PIN_PB11
#define SD_SLOT_8BITS_CARD_DETECT_VALUE    0
#define SD_SLOT_8BITS_WRITE_PROTECT        AVR32_PIN_PX57
#define SD_SLOT_8BITS_WRITE_PROTECT_VALUE  1


#define  EMMC_PINS_CFG  { \
			{SD_SLOT_8BITS_CLK_PIN,   SD_SLOT_8BITS_CLK_FUNCTION  }, \
			{SD_SLOT_8BITS_CMD_PIN,   SD_SLOT_8BITS_CMD_FUNCTION  }, \
			{SD_SLOT_8BITS_DATA0_PIN, SD_SLOT_8BITS_DATA0_FUNCTION}  };
/*
			{SD_SLOT_8BITS_DATA1_PIN, SD_SLOT_8BITS_DATA1_FUNCTION}, \
			{SD_SLOT_8BITS_DATA2_PIN, SD_SLOT_8BITS_DATA2_FUNCTION}, \
			{SD_SLOT_8BITS_DATA3_PIN, SD_SLOT_8BITS_DATA3_FUNCTION}, \
			{SD_SLOT_8BITS_DATA4_PIN, SD_SLOT_8BITS_DATA4_FUNCTION}, \
			{SD_SLOT_8BITS_DATA5_PIN, SD_SLOT_8BITS_DATA5_FUNCTION}, \
			{SD_SLOT_8BITS_DATA6_PIN, SD_SLOT_8BITS_DATA6_FUNCTION}, \
			{SD_SLOT_8BITS_DATA7_PIN, SD_SLOT_8BITS_DATA7_FUNCTION}  };
*/

#define EMMC_SPEED	400000
#define EMMC_SLOT	0

// eMMC RESET# line
#define EMMC_NRST	SD_SLOT_8BITS_DATA3_PIN


// 1-bit eMMC interface
#define EMMC_DAT	AVR32_PIN_PA29
#define EMMC_CLK	AVR32_MCI_CLK_0_PIN
#define EMMC_CMD	AVR32_PIN_PA28
#define EMMC_RST	AVR32_PIN_PB00


//! @}

/*! \name Telemetry Port
 */
//! @{
#define TLM_DEFAULT_USART_BAUDRATE      115200
//! @}


/*! \name	Debug Port
 *	Use telemetry port for debugging
 */
//!@{
#define DBG_USART               TLM_USART
#define DBG_USART_RX_PIN        TLM_USART_RX_PIN
#define DBG_USART_RX_FUNCTION   TLM_USART_RX_FUNCTION
#define DBG_USART_TX_PIN        TLM_USART_TX_PIN
#define DBG_USART_TX_FUNCTION   TLM_USART_TX_FUNCTION
#define DBG_USART_BAUDRATE      TLM_DEFAULT_USART_BAUDRATE
//!@}

/*! \name	OCR504 Port
 *	Use for OCR504 connection
 */
//!@{
#define OCR_USART               (&AVR32_USART1)
#define OCR_USART_RX_PIN        AVR32_USART1_RXD_0_0_PIN
#define OCR_USART_RX_FUNCTION   AVR32_USART1_RXD_0_0_FUNCTION
#define OCR_USART_TX_PIN        AVR32_USART1_TXD_0_0_PIN
#define OCR_USART_TX_FUNCTION   AVR32_USART1_TXD_0_0_FUNCTION
#define OCR_PDCA_PID_RX			AVR32_USART1_PDCA_ID_RX		//AVR32_PDCA_PID_USART1_RX
#define OCR_PDCA_PID_TX			AVR32_USART1_PDCA_ID_TX		//AVR32_PDCA_PID_USART1_TX
#define OCR_USART_BAUDRATE      57600
//!@}

/*! \name	MCOMS Port
 *	Use for MCOMS connection
 */
//!@{
#define MCOMS_USART               (&AVR32_USART2)
#define MCOMS_USART_RX_PIN        AVR32_USART2_RXD_0_2_PIN	
#define MCOMS_USART_RX_FUNCTION   AVR32_USART2_RXD_0_2_FUNCTION
#define MCOMS_USART_TX_PIN        AVR32_USART2_TXD_0_2_PIN
#define MCOMS_USART_TX_FUNCTION   AVR32_USART2_TXD_0_2_FUNCTION
#define MCOMS_PDCA_PID_RX		  AVR32_PDCA_PID_USART2_RX	//AVR32_USART2_PDCA_ID_RX   
#define MCOMS_PDCA_PID_TX		  AVR32_PDCA_PID_USART2_TX	//AVR32_USART2_PDCA_ID_TX	
#define MCOMS_USART_BAUDRATE      57600
//!@}





/*! \name HyperNav SPI Control lines
 */
//! @{
// SPI, controller board ready
#define CTRL_RDY					AVR32_PIN_PB05
// SPI, spectrometer board #1 ready
#define SPEC1_RDY__INT2				AVR32_PIN_PA23
// SPI, spectrometer board #1 ready, interrupt line
#define SPEC1_RDY__INT2_LINE		EXT_INT2
// SPI, spectrometer board #2 ready
#define SPEC2_RDY__INT7				AVR32_PIN_PA13
// SPI, spectrometer board #2 ready, interrupt line
#define SPEC1_RDY__INT7_LINE		EXT_INT7


/*! \name System power manager
 */
//! @{
// HW SLEEP# output
#define PWR_SYSTEM_NSLEEP			AVR32_PIN_PB11

// PWR_GOOD monitor
//
// Power monitoring input
#define PWR_PWRGOOD_PIN				AVR32_PIN_PA21
// Power monitoring interrupt line
#define PWR_PWRGOOD_INT_LINE		EXT_INT0
// Power monitoring interrupt line pin function
#define PWR_PWRGOOD_PIN_FUNCTION	AVR32_EIC_EXTINT_0_FUNCTION
// Power monitoring IRQ line
#define PWR_PWRGOOD_IRQ    			AVR32_EIC_IRQ_0
// Interrupt priority
#define PWR_PWRGOOD_PRIO			AVR32_INTC_INT0


// RTC Alarm
//
// RTC alarm interrupt input
#define PWR_RTC_ALARM_PIN			AVR32_PIN_PA22
// RTC alarm interrupt line
#define PWR_RTC_ALARM_INT_LINE		EXT_INT1
// RTC alarm pin function
#define PWR_RTC_ALARM_PIN_FUNCTION	AVR32_EIC_EXTINT_1_FUNCTION
// RTC alarm IRQ line
#define PWR_RTC_ALARM_IRQ			AVR32_EIC_IRQ_1
// Interrupt priority
#define PWR_RTC_ALARM_PRIO			AVR32_INTC_INT0


// Telemetry reception flag
//
// Telemetry reception input
#define PWR_RXFLAG_PIN				AVR32_PIN_PA09
// Telemetry reception line
#define PWR_RXFLAG_INT_LINE			EXT_INT6
// Telemetry reception pin function
#define PWR_RXFLAG_PIN_FUNCTION		AVR32_EIC_EXTINT_6_FUNCTION
// Telemetry reception IRQ line
#define PWR_RXFLAG_IRQ				AVR32_EIC_IRQ_6
// Interrupt priority
#define PWR_RXFLAG_PRIO				AVR32_INTC_INT0


//USB Vbus OK line
//
// USB Vbus OK monitoring input
//#define PWR_VBUS_OK_PIN				AVR32_PIN_PA23
// USB Vbus OK monitoring interrupt line
//#define PWR_VBUS_OK_INT_LINE		EXT_INT2
// USB Vbus OK monitoring interrupt line pin function
//#define PWR_VBUS_OK_PIN_FUNCTION	AVR32_EIC_EXTINT_2_FUNCTION
// USB Vbus OK monitoring IRQ line
//#define PWR_VBUS_OK_IRQ    			AVR32_EIC_IRQ_2
// Interrupt priority
//#define PWR_VBUS_OK_PRIO			AVR32_INTC_INT0



// Relay
//
// Relay set disable
//#define PWR_BAT_RELAY_SET_DISABLE	AVR32_PIN_PA18
// Relay reset
//#define PWR_BAT_RELAY_RESET			AVR32_PIN_PA19
// Relay reset pulse width (ms)
//#define PWR_BAT_RELAY_PULSE_MS		15


//Power loss failure handling
//
// Maximum duration of tolerated power glitch (microseconds)
#define PWR_MAX_GLITCH_US		100
// WDT timeout after a waking up from sleep due to failed shutdown (microseconds)
#define PWR_SHUTDOWN_WDT_TIMEOUT_US	4000000


// Supercap
//#define PWR_SUPERCAP_CHARGE_PIN		AVR32_PIN_PA17

// MCU internal modules switching (for power saving purposes).
//
//#define PWR_DISABLE_USART1	// required for OCR-504 
//#define PWR_DISABLE_USART2	// required for MCOMS
//#define PWR_DISABLE_USART3	// Required for SDI-12 mode
#define PWR_DISABLE_SSC
#define PWR_DISABLE_TWIM0
#define PWR_DISABLE_TWIM1
//#define PWR_DISABLE_ADC		// required by sysmon for voltage measurements
#define PWR_DISABLE_ABDAC
//#define PWR_DISABLE_USBHS		// These modules need to be enabled for USB
//#define PWR_DISABLE_DMACA		//
#define PWR_DISABLE_HRAM
#define PWR_DISABLE_AES
//! @}


/*! \name Spare GPIO
 */
//! @{
#define SPARE_GPIO		AVR32_PIN_PB02
//! @}


/*! \name Pressure sensor port
 */
//! @{
// #define PRES_RXD_PIN	AVR32_PIN_PA05
// #define PRES_TXD_PIN	AVR32_PIN_PA06
// #define PRES_ON_PIN		AVR32_PIN_PA07
// #define PRES_RTS_DE_PIN	AVR32_PIN_PA13
//! @}



/*! \name Aux pump control
 */
//! @{
//#define PUMP_D_FROM_SENSOR_PIN	AVR32_PIN_PX55
//#define PUMP_D_TO_SENSOR_PIN	AVR32_PIN_PX56
//#define PUMP_PWR_ON_PIN			AVR32_PIN_PX44
//#define PUMP_SENS_COMM_ON_PIN	AVR32_PIN_PX51
//#define PUMP_SENS_PWR_ON_PIN	AVR32_PIN_PX45
//! @}

#endif



#endif /* E980030_H_ */
