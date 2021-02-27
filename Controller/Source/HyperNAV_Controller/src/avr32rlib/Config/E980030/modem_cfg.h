/*! \file modem_cfg.h******************************************************
 *
 * \brief Modem port driver configuration file.
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/

#ifndef MODEM_CFG_H_
#define MODEM_CFG_H_

#include "compiler.h"

/*! \name Modem Port
 */
//! @{

// Modem buffers
#define MDM_RX_BUF_LEN          512   // FIXME // CONFIRM
#define MDM_TX_BUF_LEN          1024  // FIXME // CONFIRM

// Modem USART configuration 
#define MDM_USART               &AVR32_USART3
#define MDM_USART_BAUDRATE      57600 // FIXME // Replace with Iridium Modem baud rate as used/configured by NAVIS

// USART pin mapping
#define MDM_USART_RX_PIN        AVR32_USART3_RXD_0_2_PIN
#define MDM_USART_RX_FUNCTION   AVR32_USART3_RXD_0_2_FUNCTION
#define MDM_USART_TX_PIN        AVR32_USART3_TXD_0_2_PIN
#define MDM_USART_TX_FUNCTION   AVR32_USART3_TXD_0_2_FUNCTION

// PDCA mapping
#define MDM_PDCA_PID_TX         AVR32_PDCA_PID_USART3_TX
#define MDM_PDCA_PID_RX         AVR32_PDCA_PID_USART3_RX

// Receiver enable# pin
#define MDM_RXEN_n              AVR32_PIN_PX52

// Driver enable pin
#define MDM_DE                  AVR32_PIN_PX53

// Navis modem uses DTR and DSR lines (Data Terminal Ready and Data Set Ready)
// We are using lines that would normally be used for USART3 CTS and RTS
// Using them as DTR and DSR lines means they will not be automated
// Bit banging is required
#define MDM_DTR                 AVR32_PIN_PX15
#define MDM_DSR                 AVR32_PIN_PX16

#define MDM_RTS                 AVR32_PIN_PA00
#define MDM_CTS                 AVR32_PIN_PA01

// Carrier Detect
#define MDM_CD_PIN              AVR32_PIN_PX44

//! @}



#endif /* MODEM_CFG_H_ */
