/*! \file 	xmodem.h
 	\brief 	XMODEM transmit/receive utility header file

	Receive routines adapted from Atmel's APPNOTE AVR350: XmodemCRC Receive
	Utility for AVR for use with the AT32UC3A3256 MCU (originally on AT90USB1287).

	Send routines written from protocol description in the
	XMODEM/YMODEM PROTOCOL REFERENCE (edited by Chuck Forsberg),the
	AVR350 appnote, and online references (e.g. wikipedia).

	Created on: Dec 21, 2010
	Author: Burkhard Plache and Scott Feener

	History:
		2010-12-21, SF: Adding implementation details
		2011-01-31, DAS: Integrated to avr32rlib

	Copyright (C) 2007 - 2010 Satlantic Inc. All rights reserved.
 */ 

#ifndef XMODEM_H_
#define XMODEM_H_

# include "compiler.h"

# define XMDM_OK				 0
# define XMDM_FAIL				-1
# define XMDM_TIMEOUT			-2
# define XMDM_TOO_MANY_ERRORS	-3
# define XMDM_CAN				-4

//!	\brief	Send the specified file via xmodem over usart
//!
//!	@param	filename	source of data to transfer
//!	@param	use1K		Sends 128 byte packets,
//!						1K packets not implemented!
//!						Final packet will be <PAD> = decimal 26 padded.
//!						Note: CRC mode determined by receiver
//!
//!	@return	XMDM_OK		on success
//!			XMDM_FAIL	if use1K == true
//!						if file does not exist
//!						if cannot read from file
//!						if transfer failed
S16 XMDM_send_to_usart ( char const* filename, bool use1k );

//!	\brief	Receive a file via xmodem over usart, and save at specified location.
//!
//!	@param	filename	destination where to put the file
//!	@param	rxCRCmode	CRC mode for this transfer
//!						Note: Packet size (128 byte or 1K) determined by sender
//!
//!	@return	XMDM_OK		on success
//!			XMDM_FAIL	if file exists already (will not overwrite)
//!						if cannot write to file
//!						if transfer failed
S16 XMDM_recv_from_usart ( char const* filename, bool useCRC );


#endif /* XMODEM_H_ */
