/*! \file telemetry_cfg.h******************************************************
 *
 * \brief Telemetry port driver configuration file.
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/

#ifndef TELEMETRY_CFG_H_
#define TELEMETRY_CFG_H_

#include "compiler.h"

/*! \name Telemetry Port
 */
//! @{

// Telemetry buffers
#define TLM_RX_BUF_LEN			256
#define TLM_TX_BUF_LEN			256


// Telemetry USART configuration 
#define TLM_USART               (&AVR32_USART0)
#define TLM_USART_BAUDRATE      57600

//USART pin mapping
#define TLM_USART_RX_PIN        AVR32_USART0_RXD_0_0_PIN
#define TLM_USART_RX_FUNCTION   AVR32_USART0_RXD_0_0_FUNCTION
#define TLM_USART_TX_PIN        AVR32_USART0_TXD_0_0_PIN
#define TLM_USART_TX_FUNCTION   AVR32_USART0_TXD_0_0_FUNCTION

// PDCA mapping
#define TLM_PDCA_PID_TX         AVR32_PDCA_PID_USART0_TX
#define TLM_PDCA_PID_RX         AVR32_PDCA_PID_USART0_RX

// Receiver enable# pin
#define TLM_RXEN_n				AVR32_PIN_PA16

// Driver enable pin
#define TLM_DE					AVR32_PIN_PA15

// NOTE: We are going to use the USART CTS as an RTS output
#define TLM_USART_RTS_PIN		AVR32_USART0_CTS_0_0_PIN

//! @}



#endif /* TELEMETRY_CFG_H_ */
