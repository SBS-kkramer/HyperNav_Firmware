/*! \file onewire.h**********************************************************
 *
 * \brief 1-Wire bus API.
 *
 * Adapted from MAXIM's AN3684 - "How to Use the DS2482 I²C 1-Wire® Master"
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-19
 *
  ***************************************************************************/
#ifndef ONEWIRE_H_
#define ONEWIRE_H_

#include "compiler.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define	OW_BUSY	1
#define	OW_OK	0
#define	OW_FAIL	-1


//*****************************************************************************
// Exported functions
//*****************************************************************************


/* \brief Initialize One-Wire bus access
 * @note This function MUST be called once before calling any other of this module
 * @param	fCPU	CPU frequency in Hz
 * @return OW_OK: Success	OW_FAIL: Failed to initialize
 */
S16 ow_init(U32 fCPU);

//!\brief Reset all of the devices on the 1-Wire Net and return the result.
//!	@param	PPD	OUTPUT:	If TRUE presence pulse(s)were detected, else presence pulse(s)were not detected.
//! @return OW_OK:  Success
//! @return OW_FAIL: Failure while trying to reset the 1-Wire bus
//! @return	OW_BUSY: 1-Wire bus is busy, cannot reset, try again later.
S16 ow_reset(Bool* PPD);


/*! \brief Find the 'first' device on the 1-Wire network
 * @param	ROMno	OUTPUT: ROM number of the device found (if found).
 * @return	TRUE: Device found. Check 'ROMno' for ROM number.
 * @return 	FALSE: No device present.
 */
Bool ow_first(U64* ROMno);


/*! Find the 'next' devices on the 1-Wire network
 * @param	ROMno	OUTPUT: ROM number of the device found (if found).
 * @return	TRUE: Device found. Check 'ROMno' for ROM number.
 * @return	FALSE: Device not found, end of search
 */
Bool ow_next(U64* ROMno);


/* \brief Send 1 bit of communication to the 1-Wire Net.
 * @param sendbit  1 bit value to send
 * @return OW_OK:Sucess.
 * @return OW_FAIL: Failure.
 * @return OW_BUSY: OneWire bus is busy, try again later.
 */
S16 ow_writeBit(Bool sendbit);



/*! \brief Reads 1 bit of communication from the 1-Wire Net
* @param bit  OUTPUT: Received bit
* @return OW_OK:Sucess.
* @return OW_FAIL: Failure.
* @return OW_BUSY: OneWire bus is busy, try again later.
*/
S16 ow_readBit(Bool* bit);



//! \brief Send 8 bits of communication to the 1-Wire Net
//! @param sendbyte	Byte to send
//!	@return OW_OK: Byte sent sucessfully
//!	@return OW_BUSY: OneWire bus is busy cannot send, try again later.
//! @return OW_FAIL: Error while sending byte
S16 ow_writeByte(U8 sendbyte);



//! \brief Read a byte from the 1-Wire network
//! @param rcv	OUTPUT: received byte
//!	@return OW_OK: Byte received sucessfully
//!	@return OW_BUSY: OneWire bus is busy cannot receive, try again later.
//! @return OW_FAIL: Error while receiving byte
S16 ow_readByte(U8* rcv);



//! \brief Send 8 bits of communication to the 1-Wire Net and enables 'Strong Pull-Up (SPU)'
//! @note This function must be used to send the last byte that will put a 1-Wire slave
//! into a state where it will need extra power from the bus. The strong pull up condition will
//! persist until the next bit/byte transfer, or until explicitly disabled through 'ow_disableSPU()'
//! @param sendbyte	Byte to send
//!	@return OW_OK: Byte sent sucessfully
//!	@return OW_BUSY: OneWire bus is busy cannot send, try again later.
//! @return OW_FAIL: Error while sending byte
S16 ow_writeByteSPU(U8 sendbyte);



//! \brief Disable Srong Pull-Up
//!	@return OW_OK: SPU doisabled
//! @return OW_FAIL: Error while disabling
S16 ow_disableSPU(void);


#endif /* ONEWIRE_H_ */
