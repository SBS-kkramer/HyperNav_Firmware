/*	\file	E980031.h
 *
 *	\brief	Satlantic PCB-specific items for E980031 HyperNav Spectrometer Board
 *
 * 
 * Created: 1/17/2016 12:12:47 AM
 *  Author: sfeener
 *
 *	History:
 *		2017-04-17, SF: Updates for E980031B
 */ 


#ifndef E980031_H_
#define E980031_H_

#include "compiler.h"

#if (MCU_BOARD_DN == E980031)
  # ifdef GENERATE_BOARD_MESSAGE
    # warning "FYI >>> Compiling for HyperNAV Spectrometer Board - E980031 <<<"
  # endif
#elif (MCU_BOARD_DN == E980031_DEV_BOARD)
  # ifdef GENERATE_BOARD_MESSAGE
    # error "FYI >>> Compiling for HyperNAV Spectrometer Development Board (STK600) <<<"	
  # endif
#else 
    # error "Invalid MCU_BOARD_DN symbol definition"
#endif

#if (MCU_BOARD_REV==REV_A)
  # ifdef GENERATE_BOARD_MESSAGE
    # warning "FYI >>> Compiling for revision A <<<"
  # endif
	#define PCB_SUPERVISOR_SUPPORTED	1
#elif (MCU_BOARD_REV==REV_B)
	# ifdef GENERATE_BOARD_MESSAGE
		# warning "FYI >>> Compiling for revision B <<<"
	# endif
	#define PCB_SUPERVISOR_SUPPORTED	1	
#else
	#error "Currently unsupported E980030 revision.  Check pin assignments."
#endif


/*! \name Oscillator Definitions
 */
//! @{

#define FOSC32          AVR32_SCIF_OSC32_FREQUENCY				//!< Osc32 frequency: Hz.
#define FOSC32_MODE		SCIF_OSC_MODE_EXT_CLK					//!< Osc32 provided by external clock
#define OSC32_STARTUP   AVR32_SCIF_OSCCTRL32_STARTUP_0_RCOSC	//!< Osc32 startup time: RCOsc periods.
//#define OSC32_STARTUP   AVR32_SCIF_OSCCTRL32_STARTUP_8192_RCOSC //!< Osc32 startup time: RCOsc periods.

#ifdef FOSC0
	#undef FOSC0
	#warning Had to undefine FOSC0
#endif
#ifdef OSC0_STARTUP
	#undef OSC0_STARTUP
	#warning Had to undefine OSC0_STARTUP
#endif

#define FOSC0				14745600                                //!< Osc0 frequency: Hz.
#define MCU_OPERATING_FREQ	58982400								//!< FOSC0 * 4
#define FOSC0_MHZ			14.7456
//#define FAST_PBA_CLK		MCU_OPERATING_FREQ
#define	OSC0_MODE_VALUE	AVR32_SCIF_OSCCTRL_MODE_EXT_CLOCK
#define OSC0_STARTUP    AVR32_SCIF_OSCCTRL0_STARTUP_2048_RCOSC  //!< Osc0 startup time: RCOsc periods.


// only one USART in use, so debug on same port as telemetry
// #define configDBG_USART				(&AVR32_USART2)				
// #define configDBG_USART_RX_PIN		AVR32_USART2_RXD_0_1_PIN			
// #define configDBG_USART_RX_FUNCTION AVR32_USART2_RXD_0_1_FUNCTION	
// #define configDBG_USART_TX_PIN		AVR32_USART2_TXD_0_1_PIN			
// #define configDBG_USART_TX_FUNCTION AVR32_USART2_TXD_0_1_FUNCTION	
// #define configDBG_USART_BAUDRATE	57600		


#warning "temporarily remapped configDBG_USART to MNT_USART"
#define configDBG_USART				(&AVR32_USART3)
#define configDBG_USART_RX_PIN        MNT_RX
#define configDBG_USART_RX_FUNCTION   AVR32_USART3_RXD_2_FUNCTION
#define configDBG_USART_TX_PIN        MNT_TX
#define configDBG_USART_TX_FUNCTION   AVR32_USART3_TXD_2_FUNCTION
#define configDBG_USART_BAUDRATE		115200
//#define MNT_USART_IRQ			AVR32_USART3_IRQ

#define MNT_USART				(&AVR32_USART3)
#define MNT_USART_RX_PIN        MNT_RX
#define MNT_USART_RX_FUNCTION   AVR32_USART3_RXD_2_FUNCTION
#define MNT_USART_TX_PIN        MNT_TX
#define MNT_USART_TX_FUNCTION   AVR32_USART3_TXD_2_FUNCTION
#define MNT_USART_BAUDRATE		115200
#define MNT_USART_IRQ			AVR32_USART3_IRQ

// For bootloader use:
// DEBUG UART
#  define DBG_USART               MNT_USART
#  define DBG_USART_RX_PIN        MNT_USART_RX_PIN
#  define DBG_USART_RX_FUNCTION   MNT_USART_RX_FUNCTION
#  define DBG_USART_TX_PIN        MNT_USART_TX_PIN
#  define DBG_USART_TX_FUNCTION   MNT_USART_TX_FUNCTION
#  define DBG_USART_BAUDRATE      MNT_USART_BAUDRATE


//! @{
//! \name SPI Slave Bus 0
// SPI for controller-spectrometer board communications.
// Master mode only for testing board on its own.  Normally operated as slave.

// Memory location
#define	SPI_SLAVE_0				(&AVR32_SPI0)
// Testing for SPI interrupt
#define AVR32_SPI0_IER_ADDRESS  0xFFFD1814
#define AVR32_SPI0_IER          *(unsigned long *)AVR32_SPI0_IER_ADDRESS

// Pin settings
#define SPI_SLAVE_0_PINS_CFG	{\
		{AVR32_SPI0_SCK_2_PIN, AVR32_SPI0_SCK_2_FUNCTION}, \
		{AVR32_SPI0_MISO_2_PIN, AVR32_SPI0_MISO_2_FUNCTION}, \
		{AVR32_SPI0_MOSI_2_PIN, AVR32_SPI0_MOSI_2_FUNCTION}, \
		{AVR32_SPI0_NPCS_0_1_PIN, AVR32_SPI0_NPCS_0_1_FUNCTION}, \
	}

// Peripheral selection settings
#define SPI_SLAVE_0_DECODE_MODE		0	//	No decoding
#define SPI_SLAVE_0_PBA_CS_DELAY    0   //  No delay in peripheral bus A (PBA) periods between chip selects
#define SPI_SLAVE_0_VARIABLE_PS     0   //  Target slave is selected in transfer register for every character to transmit

// Options for CS0
//-------------------------------------------------
// 4MHz
// 8 bits per transfer
// 10-bit delay between slave select and clock
// No delay between transfers
// Stay active after last transfer
// SPI Mode 0 - CPOL=0, CPHA=1 (setup-falling ,latch-rising & clock normally low)
// Disable mode fault detection
#define SPI_SLAVE_0_OPTIONS_CS0	{\
			.reg          = 0,		\
			.baudrate     = 400000,\
			.bits         = 8,		\
			.spck_delay   = 10,     \
			.trans_delay  = 0,      \
			.stay_act     = 1,      \
			.spi_mode     = 0,      \
			.modfdis      = 1       \
		}

//! @{
//! \name SPI Master Bus 1
// SPI for Flash-memory communications.

// Memory location
#define	SPI_MASTER_1				(&AVR32_SPI1)

// Pin settings
#define SPI_MASTER_1_PINS_CFG	{\
	{AVR32_SPI1_SCK_PIN, AVR32_SPI1_SCK_FUNCTION}, \
	{AVR32_SPI1_MISO_PIN, AVR32_SPI1_MISO_FUNCTION}, \
	{AVR32_SPI1_MOSI_PIN, AVR32_SPI1_MOSI_FUNCTION}, \
	{AVR32_SPI1_NPCS_0_1_PIN, AVR32_SPI1_NPCS_0_1_FUNCTION}, \
}

// Options for CS0
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
	.trans_delay  = 0,      \
	.stay_act     = 1,      \
	.spi_mode     = 0,      \
	.modfdis      = 1       \
}

// Add more configs if needed
//! @}


#if (MCU_BOARD_REV==REV_A)
	//! @{
	//! \name TWI (I2C) Master 0
	// TWI through LSM303 multiplexer to Ecompass, tilts, spec temperature.

//	 #define AVR32_TWIM0_ADDRESS                0xFFFF3800
//	 #define AVR32_TWIM0                        (*((volatile avr32_twim_t*)AVR32_TWIM0_ADDRESS))
//	 #define AVR32_TWIM0_IRQ                    800
//	 #define AVR32_TWIM0_PDCA_ID_RX             6
//	 #define AVR32_TWIM0_PDCA_ID_TX             17

	 //#define AVR32_TWIMS0_TWCK_PIN              AVR32_PIN_PC03		// Pin 67, I2C Clock
//	 #define AVR32_TWIMS0_TWCK_FUNCTION         0
	 //#define AVR32_TWIMS0_TWD_PIN               AVR32_PIN_PC02		// Pin 66, I2C Data
//	 #define AVR32_TWIMS0_TWD_FUNCTION          0
	#define TWI_MUX_PORT	&AVR32_TWIM0
#else	
	#define TWI_MUX_PORT	&AVR32_TWIM1
#endif
 

// Add more configs if needed
//! @}


// consistent across revisions
#define	P16V_ON			AVR32_PIN_PA00		//!< 16V Supply enable
#define SPEC_A_ON		AVR32_PIN_PA01		//!< Spectrometer A enable
#define SPEC_B_ON		AVR32_PIN_PA02		//!< Spectrometer B enable
#define ADC_IMPULSE		AVR32_PIN_PA03		//!< ADC impulse controlA
#define ADC_RESET		AVR32_PIN_PA06		//!< ADC reset control
#define ADC_WARP		AVR32_PIN_PA07		//!< ADC warp speed control
#define CT_RXEN_N		AVR32_PIN_PA10		//!< CT sensor RS-232 receive enable (active low)
#define CT_ON			AVR32_PIN_PA11		//!< CT sensor power on
#define CS_SPEC_N		AVR32_PIN_PA12		//!< Spectrometer chip select (for SPI comms) input
#define CS_FLASH_N		AVR32_PIN_PA14		//!< Flash memory chip select
#define FORWARD_A		AVR32_PIN_PA20		//!< Shutter A forward control
#define REVERSE_A		AVR32_PIN_PA21		//!< Shutter A reverse control
#define FORWARD_B		AVR32_PIN_PA22		//!< Shutter B forward control
#define REVERSE_B		AVR32_PIN_PA23		//!< Shutter B reverse control
#define PWR_GOOD		AVR32_PIN_PA28		//!< Power good signal
#define XIN32			AVR32_PIN_PB00		//!< 32 kHz input signal
#define TEN_N			AVR32_PIN_PB01		//!< Paroscientific pressure sensor temperature signal buffer enable
#define FLASH_RESET_N	AVR32_PIN_PB02		//!< Flash memory reset control
#define FLASH_PRT_N		AVR32_PIN_PB03		//!< Flash memory protection control
#define SPI1_MOSI		AVR32_PIN_PB04		//!< SPI MOSI for Flash
#define SPI1_MISO		AVR32_PIN_PB05		//!< SPI MISO for Flash
#define SPI1_SCK		AVR32_PIN_PB06		//!< SPI clock for Flash
#define PEN_N			AVR32_PIN_PB07		//!< Paroscientific pressure sensor pressure signal buffer enable
#define SPEC_RDY		AVR32_PIN_PB08		//!< Spectrometer ready output
#define CTRL_RDY		AVR32_PIN_PB09		//!< Controller board ready input
#define SPEC_MOSI		AVR32_PIN_PB10		//!< SPI MOSI for inter-board coms
#define SPEC_MISO		AVR32_PIN_PB11      //!< SPI MISO for inter-board coms
#define	SPEC_SCK		AVR32_PIN_PB12		//!< SPI clock for inter-board coms
#define PRES_DE			AVR32_PIN_PB14		//!< Pressure sensor RS-485 driver enable (Keller)
#define PRES_RE_N		AVR32_PIN_PB15		//!< Pressure sensor RS-485 receive enable (Keller)
#define PRES_RX			AVR32_PIN_PB16		//!< Pressure sensor RS-485 receiver
#define PRES_TX			AVR32_PIN_PB17		//!< Pressure sensor RS-485 transmitter
#define CMP_INT4		AVR32_PIN_PB18		//!< Compass interrupt
#define SPEC_CLK		AVR32_PIN_PB19		//!< Spectrometer clock
#define MNT_FORCEON		AVR32_PIN_PB23		//!< maintenance port RS232 forceon
#define MNT_FORCEOFF_N	AVR32_PIN_PB24		//!< maintenance port RS232 forceoff
#define MNT_RXEN_N		AVR32_PIN_PB25		//!< maintenance port RS232 receiver enable
#define MNT_RXINVALID_N	AVR32_PIN_PB26		//!< maintenance port RS232 Rx invalid
#define P_T_FREQ_CLK	AVR32_PIN_PB27		//!< Paroscientific pressure sensor frequency input - clock pin
#define T_FREQ			AVR32_PIN_PB28		//!< Paroscientific pressure sensor temperature frequency signal
#define P_T_FREQ_A0		AVR32_PIN_PB29		//!< Paroscientific pressure sensor frequency input - A0
#define I2C_MUX_RESET_N	AVR32_PIN_PC00		//!< I2C-mux reset
#define CMP_DRDY		AVR32_PIN_PC06		//!< Compass data ready
#define MDM_RX_EN_N		AVR32_PIN_PC11		//!< modem receive enable
#define MDM_TX_EN		AVR32_PIN_PC12		//!< modem transmit enable
#define USART0_RTS		AVR32_PIN_PC13
#define MDM_DTR			USART0_RTS			//!< modem DTR
#define USART0_CTS		AVR32_PIN_PC14
#define MDM_DSR			USART0_CTS			//!< modem DSR
#define MDM_RX			AVR32_PIN_PC15		//!< modem receive (from modem)
#define MDM_TX			AVR32_PIN_PC16		//!< modem transmit (to modem)
#define MNT_TX			AVR32_PIN_PC17		//!< maintenance port transmit
#define MNT_RX			AVR32_PIN_PC18		//!< maintenance port receive

#define AVR32_EBI_DATA_0	AVR32_PIN_PC19	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_1	AVR32_PIN_PC20	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_2	AVR32_PIN_PC21	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_3	AVR32_PIN_PC22	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_4	AVR32_PIN_PC23	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_5	AVR32_PIN_PC24	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_6	AVR32_PIN_PC25	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_7	AVR32_PIN_PC26	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_8	AVR32_PIN_PC27	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_9	AVR32_PIN_PC28	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_10	AVR32_PIN_PC29	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_11	AVR32_PIN_PC30	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_12	AVR32_PIN_PC31	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_13	AVR32_PIN_PD00	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_14	AVR32_PIN_PD01	//!< External Bus Interface (EBI) data
#define AVR32_EBI_DATA_15	AVR32_PIN_PD02	//!< External Bus Interface (EBI) data

#define AVR32_EBI_ADDR_0	AVR32_PIN_PD03	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_1	AVR32_PIN_PD04	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_2	AVR32_PIN_PD05	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_3	AVR32_PIN_PD06	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_4	AVR32_PIN_PD07	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_5	AVR32_PIN_PD08	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_6	AVR32_PIN_PD09	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_7	AVR32_PIN_PD10	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_8	AVR32_PIN_PD11	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_9	AVR32_PIN_PD12	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_10	AVR32_PIN_PD14	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_11	AVR32_PIN_PD15	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_12	AVR32_PIN_PD16	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_13	AVR32_PIN_PD17	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_14	AVR32_PIN_PD18	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_15	AVR32_PIN_PD19	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_16	AVR32_PIN_PD20	//!< External Bus Interface (EBI) address
#define AVR32_EBI_ADDR_17	AVR32_PIN_PD21	//!< External Bus Interface (EBI) address
#if (MCU_BOARD_REV!=REV_A)
	#define AVR32_EBI_ADDR_18	AVR32_PIN_PD22	//!< External Bus Interface (EBI) address
	#define AVR32_EBI_ADDR_19	AVR32_PIN_PD23	//!< External Bus Interface (EBI) address
#endif	
#define AVR32_EBI_NWE1		AVR32_PIN_PD24  //!< EBI #(Write enable 1)
#define AVR32_EBI_NWE0		AVR32_PIN_PD25  //!< EBI #(Write enable 0)
#define AVR32_EBI_NRD		AVR32_PIN_PD26  //!< EBI #(Read enable)
#define AVR32_EBI_NCS_1		AVR32_PIN_PD27  //!< EBI #(Chip select 1)
#define AVR32_EBI_NCS_2		AVR32_PIN_PD28  //!< EBI #(Chip select 2)
#if (MCU_BOARD_REV!=REV_A)
	#define AVR32_EBI_NCS_3	AVR32_PIN_PC07	//!< EBI #(Chip select 3)
#endif
#define CD_N			AVR32_PIN_PD29		//!< modem carrier detect


// board-revision-specific implementations
#if (MCU_BOARD_REV==REV_A)
	#define ADC_PD			AVR32_PIN_PA04		//!< ADC power down control
	#define ADC_REF_ON		AVR32_PIN_PA05		//!< ADC reference enable
	#define ADC_MUX_A0		AVR32_PIN_PA08		//!< ADC mux A0 control
	#define ADC_MUX_A1		AVR32_PIN_PA09		//!< ADC mux A1 control
	#define SHTR_PWR_ON		AVR32_PIN_PA19		//!< Shutter power on
	#define ADC_BUSY_INT0	AVR32_PIN_PA25		//!< ADC busy signal
	#define SPEC_EOS_N_INT1	AVR32_PIN_PA26		//!< Spectrometer End-of-Scan 
	#define ADC_CNVST_N_INT2 AVR32_PIN_PA27		//!< ADC conversion start
	#define PRES_ON			AVR32_PIN_PB13		//!< Pressure sensor power enable
	#define SPEC_START		AVR32_PIN_PB20		//!< Spectrometer start signal
	#define GPIO1			AVR32_PIN_PB21		//!< spare GPIO
	#define RLY_EN			AVR32_PIN_PB22		//!< relay enable (spare)
	#define RLY_RST_EN		AVR32_PIN_PC10		//!< modem relay reset enable
	// AVR32_PIN_PD13 is not used
	
#else	// REV_B
	#define ADC_A_PD		AVR32_PIN_PA04		//!< ADC A power down control == FIFO A write enable
	#define ADC_B_PD		AVR32_PIN_PA05		//!< ADC B power down control == FIFO B write enable
	#define FIFO_A_EMPTYN	AVR32_PIN_PA08		//!< FIFO A empty, active low
	#define FIFO_A_FULLN	AVR32_PIN_PA09		//!< FIFO A full, active low
	#define FIFO_A_HFN		AVR32_PIN_PA13		//!< FIFO A half full, active low
	#define FIFO_B_EMPTYN	AVR32_PIN_PA15		//!< FIFO B empty, active low
	#define FIFO_B_FULLN	AVR32_PIN_PA16		//!< FIFO B full, active low
	#define FIFO_B_RESETN	AVR32_PIN_PA19		//!< FIFO B reset, active low
	#define FIFO_B_HFN		AVR32_PIN_PA24		//!< FIFO B half full, active low
	#define SPEC_A_EOS_N	AVR32_PIN_PA26		//!< Spectrometer A End-of-Scan 
	#define SPEC_B_EOS_N	AVR32_PIN_PA27		//!< Spectrometer B End-of-Scan
	#define PWM_CLK			AVR32_PIN_PB13		//!< optional spec clock generation
	#define SPEC_START_A	AVR32_PIN_PB20		//!< Spectrometer A start signal
	#define SPEC_START_B	AVR32_PIN_PB21		//!< Spectrometer B start signal
	#define PRES_ON			AVR32_PIN_PB22		//!< Pressure sensor power enable
	#define FIFO_A_RESETN	AVR32_PIN_PC01		//!< FIFO A reset, active low
	#define T_FREQ2			AVR32_PIN_PC02		//!< 2nd pin for Paroscientific pressure sensor temperature frequency signal
	#define CT_TXEN			AVR32_PIN_PC03		//!< 
	#define TWIS1_TWD		AVR32_PIN_PC04		//!< TWI port 1 data pin
	#define TWIS1_TWCK		AVR32_PIN_PC05		//!< TWI port 1 clock pin
	//EBI_NCS3 is on AVR32_PIN_PC07		
	#define CT_TXD			AVR32_PIN_PC08		//!< CT transmit
	#define CT_RXD			AVR32_PIN_PC09		//!< CT receiver
	#define GAIN_A_X2		AVR32_PIN_PC10		//!< Spectrometer A gain x2
	#define GAIN_B_X2		AVR32_PIN_PD13		//!< Spectrometer B gain x2
#endif	







// #define PRES_TC							(&AVR32_TC1)
// #define PRES_FREQ_INPUT_PIN				P_T_FREQ_A0
// #define PRES_FREQ_INPUT_FUNCTION		AVR32_TC1_A0_1_FUNCTION
// #define PRES_FREQ_CHANNEL_ID			0
// 
// #define TEMP_FREQ_INPUT_PIN				T_FREQ
// #define TEMP_FREQ_INPUT_FUNCTION		AVR32_TC1_B0_1_FUNCTION
// #define TEMP_FREQ_CHANNEL_ID			1


#define P_T_TC							(&AVR32_TC1)
#define P_T_INPUT_PIN					P_T_FREQ_A0
// #define P_T_INPUT_FUNCTION				AVR32_TC1_A0_1_FUNCTION
// #define P_T_INPUT_CHANNEL_ID			0	

#define P_T_FREQ_CLK_PIN				P_T_FREQ_CLK
#define P_T_FREQ_CLK_FUNCTION			AVR32_TC1_CLK0_1_FUNCTION
#define P_T_FREQ_COUNTER_CHANNEL_ID		0	//1

#define T_FREQ_CLK_PIN					T_FREQ2
#define T_FREQ_CLK_FUNCTION				AVR32_TC1_CLK1_1_FUNCTION
#define T_FREQ_COUNTER_CHANNEL_ID		1


// #define P_T_TIMER_CHANNEL_ID			2
// #define P_T_TIMER_PIN					AVR32_TC1_A2_1_PIN			//PC01 - currently unused
// #define P_T_TIMER_FUNCTION				AVR32_TC1_A2_1_FUNCTION
// #define P_T_TIMER_INT					AVR32_TC1_IRQ2


// Spectrometer clock configuration details
#define SPEC_TC							(&AVR32_TC0)
#define SPEC_CLK_TC_CHANNEL_ID			0
#define SPEC_CLK_TC_CHANNEL_PIN			AVR32_TC0_A0_0_0_PIN
#define SPEC_CLK_TC_CHANNEL_FUNCTION	AVR32_TC0_A0_0_0_FUNCTION
// Note that TC0_A0_0_0 pin is pin 58 (PB19 / GPIO 51) on AT32UC3C0512C QFP144.


// Spectrometer A interval timer - unused pin PD30
#define SPEC_A_INTERVAL_TC_CHANNEL_ID			1		// use TC channel 1
#if 1
#define SPEC_A_INTERVAL_TC_CHANNEL_PIN			AVR32_TC0_A1_2_PIN
#define SPEC_A_INTERVAL_TC_CHANNEL_FUNCTION		AVR32_TC0_A1_2_FUNCTION
#else	// use PB22 (RLY_EN) temporarily - easier to measure 
#warning NEED TO CHANGE INTERVAL TIMER OUTPUT PIN BACK AFTER TESTING!!!
#define SPEC_A_INTERVAL_TC_CHANNEL_PIN			AVR32_TC0_A1_PIN
#define SPEC_A_INTERVAL_TC_CHANNEL_FUNCTION		AVR32_TC0_A1_FUNCTION
#endif

// Spectrometer B interval timer
#define SPEC_B_INTERVAL_TC_CHANNEL_ID			2		// use TC channel 2
#define SPEC_B_INTERVAL_TC_CHANNEL_PIN			AVR32_TC0_A2_2_PIN
#define SPEC_B_INTERVAL_TC_CHANNEL_FUNCTION		AVR32_TC0_A2_2_FUNCTION

#if (MCU_BOARD_REV==REV_A)
// Spectrometer /EOS interrupt line
#define SPEC_EOS_INT_LINE						AVR32_EIC_INT1			
#define SPEC_EOS_PIN_FUNCTION					AVR32_EIC_EXTINT_1_2_FUNCTION
#define SPEC_EOS_IRQ							AVR32_EIC_IRQ_1
#define SPEC_EOS_PRIO							AVR32_INTC_INTLEVEL_INT3	//high priority interrupt
#else	//rev B
// Spectrometer /EOS interrupt lines
#define SPEC_A_EOS_INT_LINE						AVR32_EIC_INT1
#define SPEC_A_EOS_PIN_FUNCTION					AVR32_EIC_EXTINT_1_2_FUNCTION		// is this correct?  shouldn't it be _1_1?
#define SPEC_A_EOS_IRQ							AVR32_EIC_IRQ_1
#define SPEC_A_EOS_PRIO							AVR32_INTC_INTLEVEL_INT3	//high priority interrupt

#define SPEC_B_EOS_INT_LINE						AVR32_EIC_INT2
#define SPEC_B_EOS_PIN_FUNCTION					AVR32_EIC_EXTINT_2_1_FUNCTION		// is this correct?
#define SPEC_B_EOS_IRQ							AVR32_EIC_IRQ_2
#define SPEC_B_EOS_PRIO							AVR32_INTC_INTLEVEL_INT3	//high priority interrupt
#endif


// ADC Conversion Start (CNVST) interrupt line - maps to spectrometer TRIG
#define ADC_CNVST_INT_LINE						AVR32_EIC_INT2
#define ADC_CNVST_PIN_FUNCTION					AVR32_EIC_EXTINT_2_FUNCTION
#define ADC_CNVST_IRQ							AVR32_EIC_IRQ_2
#define ADC_CNVST_PRIO							AVR32_INTC_INTLEVEL_INT3	//high priority interrupt

// ADC BUSY interrupt line
#define ADC_BUSY_INT_LINE						AVR32_EIC_NMI
#define ADC_BUSY_PIN_FUNCTION					AVR32_EIC_EXTINT_0_2_FUNCTION


//#define TWI_SPEED	50000						//!< TWI data transfer rate - up to 400 kbits/s supported
#define PORT_ACCELEROMETER_ADDRESS		29		//!< Port accelerometer I2C (TWI) address		
#define STARBOARD_ACCELEROMETER_ADDRESS	30		//!< Starboard accelerometer I2C (TWI) address

//////////////////////////////////////////////////
//
// User page variables and memory map
//
//////////////////////////////////////////////////
#define UPV__OFFSET		0	// offset from user page start address


// Bootloader Error codes
#define 	CEC_NOT_APPLICATION_FILE	32000
#define		CEC_RECEIVED_OK				32001
#define 	CEC_TIMEOUT					32002
#define 	CEC_TOO_MANY_ERRORS			32003
#define		CEC_APP_TOO_LARGE			32004
#define		CEC_UPLOAD_CRC_ERROR		32005
#define		CEC_BURN_CRC_ERROR			32006
#define		CEC_UNKNOWN_ERROR			32007
#define 	CEC_CRC_NOT_MATCHED			32008
#define 	CEC_INVALID_APP_FLAG		32009
#define		CEC_BURN_ERROR				32010


// Boot Mode variable
//	Define boot-up keys
#define BOOT_LOADER		0xA5A5BEEF		//!< bootloader startup key
#define BOOT_APP		1234567890		//!< application startup key
#define UPV__BOOTMODE_STARTADDR			(AVR32_FLASHC_USER_PAGE_ADDRESS + UPV__OFFSET)
#define UPV__BOOTMODE_SIZE				4
#define UPV__BOOTMODE_ENDADDR			(UPV__BOOTMODE_STARTADDR + UPV__BOOTMODE_SIZE)

// Application (program) CRC value
#define UPV__APPCRC32_STARTADDR			UPV__BOOTMODE_ENDADDR
#define UPV__APPCRC32_SIZE				4
#if UPV__APPCRC32_SIZE != 4
#error CRC32 must be size of unsigned long (U32)
#endif
#define UPV__APPCRC32_ENDADDR			(UPV__APPCRC32_STARTADDR + UPV__APPCRC32_SIZE)

// Application (program) end address
#define UPV__APPSPACE_STARTADDR			UPV__APPCRC32_ENDADDR
#define UPV__APPSPACE_SIZE				4
#define UPV_APPSPACE_ENDADDR			(UPV__APPSPACE_STARTADDR + UPV__APPSPACE_SIZE)

// Valid application flag
#define APP_VALID						1
#define APP_INVALID						0
#define UPV__VALIDAPP_STARTADDR			UPV_APPSPACE_ENDADDR
#define UPV__VALIDAPP_SIZE				1
#define	UPV__VALIDAPP_ENDADDR			(UPV__VALIDAPP_STARTADDR + UPV__VALIDAPP_SIZE)

#if (UPV__VALIDAPP_ENDADDR > (AVR32_FLASHC_USER_PAGE_ADDRESS + AVR32_FLASHC_USER_PAGE_SIZE))
#error User Page Variables exceed User Page Size!
#endif
//////////////////////////////////////////////////


// Start address of the flashed application
# define BTLDR_64K			0x10000
# define BTLDR_32K			0x8000
# define BTLDR_16K			0x4000
# define BTLDR_8K			0x2000



// Application firmware:
# define APP_START_OFFSET  	BTLDR_16K			// note you may (will) have to update the trampoline_xx.h file as well!
# define APP_START_ADDRESS  (AVR32_FLASH_ADDRESS + APP_START_OFFSET)


//  Copied from now deleted confSpectrometer.h

#define BOARD_OSC0_HZ           FOSC0	
#define BOARD_OSC0_STARTUP_US   2000
#define BOARD_OSC0_IS_XTAL      true

#define BOARD_OSC32_HZ          32768
#define BOARD_OSC32_STARTUP_US  71000
#define BOARD_OSC32_IS_XTAL     true



#endif /* E980031_H_ */
