/*! \file 	xmodem.h
 	\brief 	XMODEM transmit/receive utility header file

	Receive routines adapted from Atmel's APPNOTE AVR350: XmodemCRC Receive
	Utility for AVR for use with the AT32UC3A3256 MCU (originally on AT90USB1287).

	Send routines written from protocol description in the
	XMODEM/YMODEM PROTOCOL REFERENCE (edited by Chuck Forsberg),the
	AVR350 appnote, and online references (e.g. wikipedia).

	Created on: Dec 5, 2010
	Author: Scott Feener

	History:


	Copyright (C) 2007 - 2010 Satlantic Inc. All rights reserved.
 */

#ifndef XMODEM_H_
#define XMODEM_H_

#define SOH 	01			// Start of Header
#define EOT 	04			// End of Transmission
#define ACK 	06			// Acknowledge
#define NAK 	21			// Not Acknowledge
#define CRCCHR	'C'			// 'C' character to indicate CRC extension mode
#define PAD		26			// XMODEM padding character, <SUB> - 26 dec, 0x1A

#define XMODEM_USE_CRC		true
#define XMODEM_USE_CHECKSUM	false
void XMDM_SetCRCMode(bool on);
bool XMDM_GetCRCMode(void);


#if 0
typedef enum {
	NOT_APPLICATION_FILE = 0,
	RECEIVED_OK,
	TIMEOUT,
	TOO_MANY_ERRORS,
	APP_TOO_LARGE,
	UPLOAD_CRC_ERROR,
	BURN_CRC_ERROR,
	UNKNOWN_ERROR

} APP_FILE_ERROR;
#endif


U16 XMDM_ReceiveApp(void);







#endif /* XMODEM_H_ */
