/*! \file telemetry.h**********************************************************
 *
 * \brief Telemetry port driver API
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/

#ifndef TELEMETRY_H_
#define TELEMETRY_H_

#include "compiler.h"
#include "telemetry_cfg.h"

//*****************************************************************************
// Constants
//*****************************************************************************
#define TLM_OK		0
#define TLM_FAIL	-1

// Tx-Rx options (for flags)
#define TLM_NONBLOCK 0x0001
#define TLM_PEEK     0x0020


//*****************************************************************************
// Exported data types
//*****************************************************************************
typedef enum {USART_TLM, USB_TLM} tlm_t;

//*****************************************************************************
// Exported functions
//*****************************************************************************

/*! \brief Initialize the telemetry driver.
 *! @note This function must be called once before using the telemetry driver
 *! @param	fPBA	PBA clock frequency
 *! @param	baudrate Baudrate
 */
S16 tlm_init(U32 fPBA, U32 baudrate);



 //! \brief Deinitialize Telemetry Port (deallocate resources)
 void tlm_deinit(void);



//! \brief Send data through telemetry port
//! @param buffer	Data to be sent
//! @param size		Number of bytes to send
//! @param flags	Send options flags:
//!					TLM_NONBLOCK	- Do not block. Function will return right after writing to the buffer.
//!								  i.e., will not wait until data is sent.
//! @return Number of bytes sent, or -1 if an error ocurred (check 'nsyserrno').
S16 tlm_send(void const* buffer, U16 size, U16 flags);


//! \brief Send a message through the telemetry
//! @param msg	Null terminated string message
S16 tlm_msg(const char* msg);


//! \brief Receive data through telemetry port
//! @param buffer	Array where to store received data
//! @param size		Number of bytes to read
//! @param flags	Receive options flags:
//!					TLM_NONBLOCK	- Do not block. Function will read from port buffer and return immediately, even if
//!					less than 'size' bytes were read. If this flag is not specified, call will block until 'size' bytes
//!					are received.
//!					TML_PEEK - Read data from port without removing it from the port buffer. If used, the next read will
//!					return the same data.
//! @return Number of bytes read, or -1 if an error ocurred (check 'nsyserrno').
S16 tlm_recv(void* buffer, U16 size, U16 flags);



//! \brief Flush telemetry port reception buffer.
//! This function discards all unread data.
void tlm_flushRecv(void);



//! \brief Assert/Deassert RTS line
//! @param assert	If true RTS is asserted, if false RTS is deasserted.
void tlm_RTS_assert(Bool assert);



//! \brief Enable/Disable reception
//! @param enable	If true reception is enabled, if false reception is disabled. Use as needed for power saving.
//! @note Reception starts enabled by default.
void tlm_rxen(Bool enable);



//! \brief Enable/Disable transmission
//! @param enable	If true transmission is enabled, if false transmission is disabled. Use as needed for power saving.
//! @note Transmission starts enabled by default.
void tlm_txen(Bool enable);



//! \brief Dynamically set telemetry port baudrate
//! @param baudrate Baudrate
//! @return TLM_OK: Baudrate changed successfully. TLM_FAIL: An error ocurred (check 'nsyserrno').
S16 tlm_setBaudrate(U32 baudrate);



//! \brief Detect soft break, ignore all other inputs
//! @param	softBreakChar	Soft break character
//! @return TRUE: Soft break detected, FALSE: Soft break not detected
Bool tlm_softBreak(char softBreakChar);


//! \brief Get telemetry mode
tlm_t tlm_getMode(void);


#if TLM_USB_COMM == ENABLED

//! \brief Switch to USB telemetry
//! @return TLM_OK: Success, TLM_FAIL: Error
S16 tlm_switchToUSBTLM(void);


//! \brief Switch to serial telemetry
//! @return TLM_OK: Success, TLM_FAIL: Error
S16 tlm_switchToSerialTLM(void);


#endif


#endif /* TELEMETRY_H_ */
