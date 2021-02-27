/*! \file generic_serial.h**********************************************************
 *
 * \brief Generic interface to a serial port
 *
 * @author      BPlache, Satlantic
 * @date        2016-02-26
 *
  ***************************************************************************/

#ifndef _GENERIC_SERIAL_H_
#define _GENERIC_SERIAL_H_

#include "compiler.h"
#include <stdint.h>

//*****************************************************************************
// Constants
//*****************************************************************************

// Tx-Rx flag options
#define GSP_NONBLOCK 0x0001
#define GSP_PEEK     0x0020

typedef enum {
  GSP_MCOMS,
  GSP_OCR
} generic_serial_port_t;

//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief           Initialize generic serial port I/O for a specified port
//! @note            This function must be called once before using the driver
//! @param gsp       The port to be initialized
//! @param fPBA      PBA clock frequency
//! @param baudrate  Baudrate
//! @param incoming_frame_length    data in a single incoming frame, so the GSP can setup a buffer
//! @param outgoing_command_length  max length of an outgoing command, for buffer setup
//! @return          0: Initialized Ok;  1: Error occurred (check 'nsyserrno').
int8_t GSP_Init( generic_serial_port_t gsp, uint32_t fPBA, uint32_t baudrate, uint16_t incoming_frame_length, uint16_t outgoing_command_length );


//! \brief           Initialize generic serial port I/O for a specified port
//! @note            This function must be called once before using the driver
//! @param gsp       The port to be initialized
//! @return          0: Deinitialized Ok;  1: Error occurred (check 'nsyserrno').
int8_t GSP_Deinit( generic_serial_port_t gsp );


//! \brief           Send data through serial port
//! @param gsp       The port to send to
//! @param buffer    Data to be sent
//! @param size      Number of bytes to send
//! @param flags     Send options flags:
//!                  GSP_NONBLOCK  - Do not block. Function will return right after writing to the buffer.
//!                                i.e., will not wait until data is sent.
//! @return Number of bytes sent, or -1 if an error occurred (check 'nsyserrno').
int32_t GSP_Send( generic_serial_port_t gsp, void const* buffer, uint16_t size, uint16_t flags);


//! \brief           Receive data through serial port
//! @param gsp       The port to receive from
//! @param buffer    Array where to store received data
//! @param size      Number of bytes to read
//! @param flags     Receive options flags:
//!                  GSP_NONBLOCK  - Do not block. Function will read from port buffer and return immediately, even if
//!                  less than 'size' bytes were read. If this flag is not specified, call will block until 'size' bytes
//!                  are received.
//!                  GSP_PEEK - Read data from port without removing it from the port buffer. If used, the next read will
//!                  return the same data.
//! @return Number of bytes read, or -1 if an error occurred (check 'nsyserrno').
int32_t GSP_Recv( generic_serial_port_t gsp, void /*const*/* buffer, uint16_t size, uint16_t flags );


//! \brief Flush CTD port reception buffer.
//! This function discards all unread data.
void gsp_flushRecv(void);


#endif /* _GENERIC_SERIAL_H_ */
