/*! \file confSpectrometer.h
 *	Spectrometer PCB configuration
 *
 * Created: 7/21/2015 4:11:02 PM
 *  Author: Scott
 */ 


#ifndef CONFSPECTROMETER_H_
#define CONFSPECTROMETER_H_

#include "compiler.h"

#ifdef __AVR32_ABI_COMPILER__ // Automatically defined when compiling for AVR32, not when assembling.
#  include "led.h"
#endif  // __AVR32_ABI_COMPILER__


/*! \name Oscillator Definitions
 */
//! @{


#define FOSC32          AVR32_SCIF_OSC32_FREQUENCY              //!< Osc32 frequency: Hz.
//#define OSC32_STARTUP   AVR32_SCIF_OSCCTRL32_STARTUP_0_RCOSC //!< Osc32 startup time: RCOsc periods.
#define OSC32_STARTUP   AVR32_SCIF_OSCCTRL32_STARTUP_8192_RCOSC //!< Osc32 startup time: RCOsc periods.
#define FOSC32_MODE		SCIF_OSC_MODE_EXT_CLK					//!< Osc32 provided by external clock


// If using the STK600 dev board, ensure the oscillator is programmed to this value
#define FOSC0           14745600                                //!< Osc0 frequency: Hz.
#define OSC0_STARTUP    AVR32_SCIF_OSCCTRL0_STARTUP_2048_RCOSC  //!< Osc0 startup time: RCOsc periods.

// Osc1 crystal is not mounted by default. Set the following definitions to the
// appropriate values if a custom Osc1 crystal is mounted on your board.
// #define FOSC1           12000000                              //!< Osc1 frequency: Hz.
// #define OSC1_STARTUP    AVR32_SCIF_OSCCTRL1_STARTUP_2048_RCOSC  //!< Osc1 startup time: RCOsc periods.

//! @}

//#define BOARD_OSC0_HZ           16000000
//#define BOARD_OSC0_STARTUP_US   2000
//#define BOARD_OSC0_IS_XTAL      true
//#define BOARD_OSC32_HZ          32768
//#define BOARD_OSC32_STARTUP_US  71000
//#define BOARD_OSC32_IS_XTAL     true





#define serialPORT_USART              (&AVR32_USART2)
#define serialPORT_USART_RX_PIN       AVR32_USART2_RXD_0_1_PIN
#define serialPORT_USART_RX_FUNCTION  AVR32_USART2_RXD_0_1_FUNCTION
#define serialPORT_USART_TX_PIN       AVR32_USART2_TXD_0_1_PIN
#define serialPORT_USART_TX_FUNCTION  AVR32_USART2_TXD_0_1_FUNCTION
#define serialPORT_USART_IRQ          AVR32_USART2_IRQ
#define serialPORT_USART_BAUDRATE     57600
// if testing with the STK600 board, connect STK600.PORTJ.PJ4 to STK600.RS232 SPARE.TXD
// and connect STK600.PORTA.PJ5 to STK600.RS232 SPARE.RXD
// CHECK


/*
#define serialPORT_USART				(&AVR32_USART1)
#define serialPORT_USART_RX_PIN			AVR32_USART1_RXD_0_2_PIN
#define serialPORT_USART_RX_FUNCTION	AVR32_USART1_RXD_0_2_FUNCTION
#define serialPORT_USART_TX_PIN			AVR32_USART1_TXD_0_2_PIN
#define serialPORT_USART_TX_FUNCTION	AVR32_USART1_TXD_0_2_FUNCTION
#define serialPORT_USART_IRQ			AVR32_USART1_IRQ
#define serialPORT_USART_BAUDRATE		57600
#define	TLM_nRXEN						AVR32_PIN_PA20
#define TLM_nFORCEOFF					AVR32_PIN_PA18
#define TLM_nRXINVALID					AVR32_PIN_PA07
#define TLM_FORCEON						AVR32_PIN_PB05
// if testing with the STK600 board, connect STK600.PORTA.PA4 to STK600.RS232 SPARE.RXD
// and connect STK600.PORTA.PA5 to STK600.RS232 SPARE.TXD
*/


// only one USART in use, so debug on same port as telemetry
#define configDBG_USART				serialPORT_USART				//(&AVR32_USART1)
#define configDBG_USART_RX_PIN		serialPORT_USART_RX_PIN			//AVR32_USART1_RXD_0_2_PIN
#define configDBG_USART_RX_FUNCTION serialPORT_USART_RX_FUNCTION	//AVR32_USART1_RXD_0_2_FUNCTION
#define configDBG_USART_TX_PIN		serialPORT_USART_TX_PIN			//AVR32_USART1_TXD_0_2_PIN
#define configDBG_USART_TX_FUNCTION serialPORT_USART_TX_FUNCTION	//AVR32_USART1_TXD_0_2_FUNCTION
#define configDBG_USART_BAUDRATE	serialPORT_USART_BAUDRATE		//57600











#define LED_COUNT   8
#define LED0_GPIO   AVR32_PIN_PB08  // STK600.PORTF.PF0 -> STK600.LEDS.LED0



#if 0
/*! \name SDRAM Definitions
 */
//! @{

//! Part header file of used SDRAM(s).
#define SDRAM_PART_HDR  "mt48lc16m16a2tg7e/mt48lc16m16a2tg7e.h"

//! Data bus width to use the SDRAM(s) with (16 or 32 bits; always 16 bits on
//! UC3).
#define SDRAM_DBW       16
//! @}
//! Number of LEDs.
#define LED_COUNT   1//	8

/*! \name GPIO Connections of LEDs. To use these defines, connect the STK600 PORTA 
 * PB08-to-PB15 connectors to respectively the LEDs LED0-to-LED3 connectors.
 * Note that when the JTAG is active, it uses pins PA00-to-PA03 of the UC3A. This
 * is why we didn't use pins PA00-to-PA03 for the LEDs LED0-to-LED3.
 */
//! @{
#define LED0_GPIO   AVR32_PIN_PB08  // STK600.PORTF.PF0 -> STK600.LEDS.LED0
#define LED1_GPIO   AVR32_PIN_PB09  // STK600.PORTF.PF1 -> STK600.LEDS.LED1
#define LED2_GPIO   AVR32_PIN_PB10  // STK600.PORTF.PF2 -> STK600.LEDS.LED2
#define LED3_GPIO   AVR32_PIN_PB11  // STK600.PORTF.PF3 -> STK600.LEDS.LED3
#define LED4_GPIO   AVR32_PIN_PB12  // STK600.PORTF.PF4 -> STK600.LEDS.LED4
#define LED5_GPIO   AVR32_PIN_PB13  // STK600.PORTF.PF5 -> STK600.LEDS.LED5
#define LED6_GPIO   AVR32_PIN_PB14  // STK600.PORTF.PF6 -> STK600.LEDS.LED6
#define LED7_GPIO   AVR32_PIN_PB15  // STK600.PORTF.PF7 -> STK600.LEDS.LED7
//! @}

/*! \name GPIO Connections of Push Buttons. To use these defines, connect the
 * STK600 PORTH connector to the SWITCHES connector.
 */
//! @{
#define GPIO_PUSH_BUTTON_SW0            AVR32_PIN_PB24  // Connect STK600.PORTH.PH0 to STK600.SWITCHES.SW0
#define GPIO_PUSH_BUTTON_SW0_PRESSED    0
#define GPIO_PUSH_BUTTON_SW1            AVR32_PIN_PB25  // Connect STK600.PORTH.PH1 to STK600.SWITCHES.SW1
#define GPIO_PUSH_BUTTON_SW1_PRESSED    0
#define GPIO_PUSH_BUTTON_SW2            AVR32_PIN_PB26  // Connect STK600.PORTH.PH2 to STK600.SWITCHES.SW2
#define GPIO_PUSH_BUTTON_SW2_PRESSED    0
#define GPIO_PUSH_BUTTON_SW3            AVR32_PIN_PB27  // Connect STK600.PORTH.PH3 to STK600.SWITCHES.SW3
#define GPIO_PUSH_BUTTON_SW3_PRESSED    0
#define GPIO_PUSH_BUTTON_SW4            AVR32_PIN_PB28  // Connect STK600.PORTH.PH4 to STK600.SWITCHES.SW4
#define GPIO_PUSH_BUTTON_SW4_PRESSED    0
#define GPIO_PUSH_BUTTON_SW5            AVR32_PIN_PB29  // Connect STK600.PORTH.PH5 to STK600.SWITCHES.SW5
#define GPIO_PUSH_BUTTON_SW5_PRESSED    0
#define GPIO_PUSH_BUTTON_SW6            AVR32_PIN_PB30  // Connect STK600.PORTH.PH6 to STK600.SWITCHES.SW6
#define GPIO_PUSH_BUTTON_SW6_PRESSED    0
#define GPIO_PUSH_BUTTON_SW7            AVR32_PIN_PB31  // Connect STK600.PORTH.PH7 to STK600.SWITCHES.SW7
#define GPIO_PUSH_BUTTON_SW7_PRESSED    0
//! @}


/*! \name SPI Connections of the AT45DBX Data Flash Memory
 */
//! @{
#define AT45DBX_SPI                 (&AVR32_SPI1)
#define AT45DBX_SPI_NPCS            1
#define AT45DBX_SPI_SCK_PIN         AVR32_SPI1_SCK_0_1_PIN
#define AT45DBX_SPI_SCK_FUNCTION    AVR32_SPI1_SCK_0_1_FUNCTION
#define AT45DBX_SPI_MISO_PIN        AVR32_SPI1_MISO_0_1_PIN
#define AT45DBX_SPI_MISO_FUNCTION   AVR32_SPI1_MISO_0_1_FUNCTION
#define AT45DBX_SPI_MOSI_PIN        AVR32_SPI1_MOSI_0_1_PIN
#define AT45DBX_SPI_MOSI_FUNCTION   AVR32_SPI1_MOSI_0_1_FUNCTION
#define AT45DBX_SPI_NPCS0_PIN       AVR32_SPI1_NPCS_1_2_PIN
#define AT45DBX_SPI_NPCS0_FUNCTION  AVR32_SPI1_NPCS_1_2_FUNCTION
//! @}

/*! \name ET024006DHU TFT display
 */
//! @{

#define ET024006DHU_TE_PIN              AVR32_PIN_PD19
#define ET024006DHU_RESET_PIN           AVR32_PIN_PD16
#define ET024006DHU_BL_PIN              AVR32_TC0_B0_0_2_PIN
#define ET024006DHU_BL_FUNCTION         AVR32_TC0_B0_0_2_FUNCTION
#define ET024006DHU_DNC_PIN             AVR32_EBI_ADDR_22_PIN
#define ET024006DHU_DNC_FUNCTION        AVR32_EBI_ADDR_22_FUNCTION
#define ET024006DHU_EBI_NCS_PIN         AVR32_EBI_NCS_PIN
#define ET024006DHU_EBI_NCS_FUNCTION    AVR32_EBI_NCS_FUNCTION

//! @}
/*! \name Optional SPI connection to the TFT
 */
//! @{

#define ET024006DHU_SPI                  (&AVR32_SPI1)
#define ET024006DHU_SPI_NPCS             1
#define ET024006DHU_SPI_SCK_PIN          AVR32_SPI1_SCK_0_1_PIN
#define ET024006DHU_SPI_SCK_FUNCTION     AVR32_SPI1_SCK_0_1_FUNCTION
#define ET024006DHU_SPI_MISO_PIN         AVR32_SPI1_MISO_0_1_PIN
#define ET024006DHU_SPI_MISO_FUNCTION    AVR32_SPI1_MISO_0_1_FUNCTION
#define ET024006DHU_SPI_MOSI_PIN         AVR32_SPI1_MOSI_0_1_PIN
#define ET024006DHU_SPI_MOSI_FUNCTION    AVR32_SPI1_MOSI_0_1_FUNCTION
#define ET024006DHU_SPI_NPCS_PIN         AVR32_SPI1_NPCS_2_2_PIN
#define ET024006DHU_SPI_NPCS_FUNCTION    AVR32_SPI1_NPCS_2_2_FUNCTION

//! @}

/*! \name LCD Connections of the ET024006DHU display
 */
//! @{
#define ET024006DHU_SMC_USE_NCS           0
#define ET024006DHU_SMC_COMPONENT_CS      "smc_et024006dhu.h"

#define ET024006DHU_EBI_DATA_0    AVR32_EBI_DATA_0
#define ET024006DHU_EBI_DATA_1    AVR32_EBI_DATA_1
#define ET024006DHU_EBI_DATA_2    AVR32_EBI_DATA_2
#define ET024006DHU_EBI_DATA_3    AVR32_EBI_DATA_3
#define ET024006DHU_EBI_DATA_4    AVR32_EBI_DATA_4
#define ET024006DHU_EBI_DATA_5    AVR32_EBI_DATA_5
#define ET024006DHU_EBI_DATA_6    AVR32_EBI_DATA_6
#define ET024006DHU_EBI_DATA_7    AVR32_EBI_DATA_7
#define ET024006DHU_EBI_DATA_8    AVR32_EBI_DATA_8
#define ET024006DHU_EBI_DATA_9    AVR32_EBI_DATA_9
#define ET024006DHU_EBI_DATA_10   AVR32_EBI_DATA_10
#define ET024006DHU_EBI_DATA_11   AVR32_EBI_DATA_11
#define ET024006DHU_EBI_DATA_12   AVR32_EBI_DATA_12
#define ET024006DHU_EBI_DATA_13   AVR32_EBI_DATA_13
#define ET024006DHU_EBI_DATA_14   AVR32_EBI_DATA_14
#define ET024006DHU_EBI_DATA_15   AVR32_EBI_DATA_15

#define ET024006DHU_EBI_ADDR_21   AVR32_EBI_ADDR_22

#define ET024006DHU_EBI_NWE       AVR32_EBI_NWE0
#define ET024006DHU_EBI_NRD       AVR32_EBI_NRD
#define ET024006DHU_EBI_NCS       AVR32_EBI_NCS_0
//! @}

#endif

#endif /* CONFSPECTROMETER_H_ */