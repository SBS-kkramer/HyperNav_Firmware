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
#include "board.h"

/*! \name Telemetry (Management) Port
 */
//! @{

// Interface chip
#define MAX3221                 TRUE

// Telemetry buffers
#define TLM_RX_BUF_LEN			256
#define TLM_TX_BUF_LEN			256

// Telemetry USART configuration 
#define TLM_USART               (&AVR32_USART3)
#define TLM_USART_BAUDRATE      115200
#define TLM_USART_CLOCK			sysclk_get_pba_hz()		// USART3 in PBA clock domain

//USART pin mapping
#define TLM_USART_RX_PIN        MNT_RX
#define TLM_USART_RX_FUNCTION   AVR32_USART3_RXD_2_FUNCTION
#define TLM_USART_TX_PIN        MNT_TX
#define TLM_USART_TX_FUNCTION   AVR32_USART3_TXD_2_FUNCTION

// PDCA mapping
#define TLM_PDCA_PID_TX         AVR32_USART3_PDCA_ID_TX
#define TLM_PDCA_PID_RX         AVR32_USART3_PDCA_ID_RX

// Receiver enable# pin
#define TLM_RXEN_n				MNT_RXEN_N

// Control lines
#define MNT_FORCE_ON			MNT_FORCEON
#define MNT_FORCE_OFF_N			MNT_FORCEOFF_N

// USB Telemetry configuration
#define TLM_USB_COMM		DISABLED

//! @}

#endif /* TELEMETRY_CFG_H_ */
