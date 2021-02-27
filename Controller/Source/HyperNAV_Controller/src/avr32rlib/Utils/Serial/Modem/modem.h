/*! \file modem.h**********************************************************
 *
 * \brief Modem port driver API
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/

#ifndef MODEM_H_
#define MODEM_H_

#include "compiler.h"
#include "modem_cfg.h"
#include "time.h"

//*****************************************************************************
// Constants
//*****************************************************************************
#define MDM_OK      0
#define MDM_FAIL    -1

// Tx-Rx options (for flags)
#define MDM_NONBLOCK  0x0001
#define MDM_PEEK      0x0020
#define MDM_USE_CTS   0x0100

//*****************************************************************************
// Exported data types
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

/*! \brief Initialize the modem driver.
 *! @note This function must be called once before using the modem driver
 *! @param  fPBA    PBA clock frequency
 *! @param  baudrate Baudrate
 */
S16 mdm_init(U32 fPBA, U32 baudrate);

//! \brief Deinitialize Modem Port (deallocate resources)
void mdm_deinit(void);

//! \brief Send data through modem port
//! @param buffer   Data to be sent
//! @param size     Number of bytes to send
//! @param flags    Send options flags:
//!                 O_NONBLOCK  - Do not block. Function will return right after writing to the buffer.
//!                               i.e., will not wait until data is sent.
//! @return Number of bytes sent, or -1 if an error ocurred (check 'nsyserrno').
S16 mdm_send(void const* buffer, U16 size, U16 flags, U16 blocking_timeout /* ms */);


//! \brief Send a message through the modem
//! @param msg  Null terminated string message
S16 mdm_msg(const char* msg);


//! \brief Receive data through modem port
//! @param buffer   Array where to store received data
//! @param size     Number of bytes to read
//! @param flags    Receive options flags:
//!                 O_NONBLOCK  - Do not block. Function will read from port buffer and return immediately, even if
//!                 less than 'size' bytes were read. If this flag is not specified, call will block until 'size' bytes
//!                 are received.
//!                 O_PEEK - Read data from port without removing it from the port buffer. If used, the next read will
//!                 return the same data.
//! @return Number of bytes read, or -1 if an error ocurred (check 'nsyserrno').
S16 mdm_recv(void* buffer, U16 size, U16 flags);


//! \brief Flush modem port reception buffer.
//! This function discards all unread data.
void mdm_flushRecv(void);


//! \brief Enable/Disable reception
//! @param enable   If true reception is enabled, if false reception is disabled. Use as needed for power saving.
//! @note Reception starts enabled by default.
void mdm_rxen(Bool enable);


//! \brief Enable/Disable transmission
//! @param enable   If true transmission is enabled, if false transmission is disabled. Use as needed for power saving.
//! @note Transmission starts enabled by default.
void mdm_txen(Bool enable);


//! \brief Dynamically set modem port baudrate
//! @param baudrate Baudrate
//! @return MDM_OK: Baudrate changed successfully. MDM_FAIL: An error ocurred (check 'nsyserrno').
S16 mdm_setBaudrate(U32 baudrate);


//! \brief Detect soft break, ignore all other inputs
//! @param  softBreakChar   Soft break character
//! @return TRUE: Soft break detected, FALSE: Soft break not detected
Bool mdm_softBreak(char softBreakChar);

int mdm_carrier_detect();
int mdm_get_dsr();
int mdm_get_dtr();
void mdm_dtr( int assert );
void mdm_dtr_toggle_off_on();
int mdm_wait_for_dsr();

int mdm_get_cts();
void mdm_rts( int assert );

int mdm_chat( char const* command, char const* expect, time_t delay_sec, char const* trm );

int mdm_getAtOk();
int mdm_isIridium();
int mdm_configure();
int mdm_isRegistered();
int mdm_getFwRev( char* FwRev, size_t size );
int mdm_getIccid( char* iccid, size_t size );
int mdm_getImei ( char* imei,  size_t size );
int mdm_getModel( char* model, size_t size );
int mdm_getSignalStrength( int measurements, int* min, int* avg, int* max );
int mdm_getLCC();

/* prototypes for external functions */
int mdm_connect(const char *dialstring, int sec);
int mdm_login( int try_duration /* seconds */, char* username, char* password );
int mdm_communicate ( int duration, int toSend, int* bytesSent );
int mdm_logout();
int mdm_escape();
int mdm_hangup();

void mdm_log( int action );

#endif /* MODEM_H_ */
