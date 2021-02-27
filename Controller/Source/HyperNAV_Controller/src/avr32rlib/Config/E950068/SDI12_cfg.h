/*
 * SDI12_cfg.h
 *
 *  Created on: 2011-08-15
 *      Author: Scott
 */

#ifndef SDI12_CFG_H_
#define SDI12_CFG_H_

#include "compiler.h"

/*! \name Telemetry Port
 */
//! @{
#if 1	// original
// SDI-12 USART configuration
#define SDI12_USART				(&AVR32_USART3)
#define SDI12_USART_BAUDRATE	1200

//SDI-12 USART pin mapping

#define SDI12_USART_RX_PIN        	AVR32_USART3_RXD_0_2_PIN
#define SDI12_USART_RX_FUNCTION   	AVR32_USART3_RXD_0_2_FUNCTION
#define SDI12_USART_TX_PIN        	AVR32_USART3_TXD_0_2_PIN
#define SDI12_USART_TX_FUNCTION		AVR32_USART3_TXD_0_2_FUNCTION

/* Note that although SDI-12 is a half-duplex interface, the USART cannot be used
 * in RS-485 mode to control the RTS (tx enable) pin automatically, as indepedent
 * control is required (marking state, for example)
 */
// SDI-12 transmitter enable pin
#define SDI12_TX_EN				AVR32_PIN_PX15

#else // use USART 1
	#define SDI12_USART					(&AVR32_USART1)
	#define SDI12_USART_BAUDRATE		1200
	#define SDI12_USART_RX_PIN        	AVR32_USART1_RXD_0_0_PIN
	#define SDI12_USART_RX_FUNCTION   	AVR32_USART1_RXD_0_0_FUNCTION
	#define SDI12_USART_TX_PIN        	AVR32_USART1_TXD_0_0_PIN
	#define SDI12_USART_TX_FUNCTION		AVR32_USART1_TXD_0_0_FUNCTION
	#define SDI12_TX_EN					AVR32_PIN_PA13
#endif


// Timer setup
#define SDI12_TIMER     	(&AVR32_TC1)
#define SDI12_TIMER_CH		0
#define SDI12_TIMER_PRIO	AVR32_INTC_INT2
#define SDI12_TIMER_IRQ		AVR32_TC1_IRQ0



//! @}


#endif /* SDI12_CFG_H_ */
