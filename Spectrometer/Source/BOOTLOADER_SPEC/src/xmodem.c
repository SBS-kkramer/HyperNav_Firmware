/*
 * xmodem.c
 *
 *  Created on: Dec 5, 2010
 *      Author: Scott Feener
 *  Modified on:
 *  2011-10-4, DAS - Removed explicit dependency on USART. Redirected to new COMMS module.
 *	2016-10-24, SF: Porting to HyperNAV spectrometer board
 */

#include <avr32/io.h>
#include <sys/time.h>
#include "intc.h"
#include "compiler.h"
#include "gpio.h"
//#include "pm.h"
#include "board.h"
#include "delay.h"
//#include "rtc.h"
#include "string.h"
#include "globals.h"
#include "flashc.h"
#include "crc.h"
#include "comms.h"
#include "wdt.h"
#include "xmodem.h"


#define XMDM_DEBUG	0
#define ALLOW_PRG_BURN	1		// skip burning program to flash if 0


#define full 	0xff
#define empty 	0x00

#define bad 	0x00
#define good 	0x01
#define dup 	0x02
#define end 	0x03
#define err 	0x04
#define out 	0x05
#define not_application_file	0x06

volatile U8 xmdbuf[133];		// data buffer

volatile static bool rtc_int_fired = false;

struct {
  volatile unsigned char *recv_ptr;
  volatile unsigned char buffer_status;
  volatile bool recv_error;
  volatile bool timed_out;
} gl;

static bool CRCmode = false;	// use checksum by default because of SUNACom restrictions

/*
 * from http://support.atmel.no/bin/customer?=&action=viewKbEntry&id=323

	Question
	How to include a binary file as a table into a project?
	Answer
	Here are the steps to follow:

	1. Open a Cygwin shell and type:
	od -A n -t x1 file.bin | sed -e 's/[0-9a-fA-F]\+/0x\U\0,/g' > file.h

	2. Declare the table:
	static const unsigned char table[] =
	{
		#include "file.h"
	};

	2010-12-08, SF: I had to remove the \U from example and delete the final comma from the output file
 */
// Firmware file to must begin with this packet
static const U8 FIRMWARE_PRELUDE_PACKET[128] =
{
	#include "prelude.h"
};

/*
F64 F64Time(struct timeval t)
{
	return (F64)t.tv_sec + ((F64)t.tv_usec)/1.0e6;
}
*/

void XMDM_SetCRCMode(bool on)
{
	if (on)
		CRCmode = true;
	else
		CRCmode = false;

}

bool XMDM_GetCRCMode(void)
{
	return CRCmode;
}

static void xmdm_ReceiveChar(int c)
{
	volatile U8 *local_ptr;

	local_ptr = gl.recv_ptr;

	// should check for errors (framing, overrun, etc) here
	/*
	 * e.g. if (some_error)
	 * 	gl.recv_error = TRUE;   		// will NAK sender in XMDM_Respond
	 */

	*local_ptr++ = (U8)c;

	// always read a character otherwise another interrupt could get generated
	// read status register before reading data register

	switch (xmdbuf[0])           // determine if buffer full
	{
		case (SOH) :
			if (CRCmode) {	// use 16-bit CRC?
				if (local_ptr == (&xmdbuf[132] + 1))
				{
					gl.buffer_status = full;
					local_ptr = &xmdbuf[0];
				}
			}

			else { // normal XMODEM checksum - buffer is 1-byte smaller

				if (local_ptr == (&xmdbuf[131]+1))
				{
					gl.buffer_status = full;
					local_ptr = &xmdbuf[0];
				}
			}
			break;

		case (EOT) :
			gl.buffer_status = full;
			local_ptr = &xmdbuf[0];
			break;

		default :
			gl.buffer_status = full;    // first char unknown
			local_ptr = &xmdbuf[0];
			break;
	}
	gl.recv_ptr = local_ptr;            // restore global pointer

}


/*! \fn 	xmdm_purge(void)
	\brief 	One second delay for sender to empty its transmit buffer
			Any received data is purged.
 */
static void xmdm_purge(void)
{
	int c;

#if XMDM_DEBUG
	COMMS_writeLine("xmdm_purge\r\n");
#endif

	delay_ms(1000);

	while ((c = COMMS_getc()) != COMMS_BUFFER_EMPTY); 	// flush any received chars
}

/*! \fn		xmdm_sendc(void)
	\brief	Initializes communication with sender.

	Aborts after 10 tries.
	Sends 'C' when using 16-bit CRC, NAK when using checksum

	\return False 	if no response after 10 tries.
 */
static bool xmdm_sendc(void)
{
	int c;
	t_cpu_time timer;
	S8 retries = 10;

	// enable entry into while loops
	gl.buffer_status = empty;
	gl.timed_out = false;
	gl.recv_error = false; // checked in validate_packet for framing or overruns

	cpu_set_timeout( cpu_ms_2_cy(3000, FOSC0), &timer ); // timeout in 3000 ms		

	// send character 'C' until we get a packet from the sender
	while ((!gl.buffer_status) && (retries-- > 0))
	{
		if (CRCmode) {
			COMMS_putchar(CRCCHR); // signal transmitter to use CRC mode ... 128 byte packets
		}
		else {
			COMMS_putchar(NAK);	// use checksum mode - e.g. SUNACom
			#if XMDM_DEBUG
			COMMS_writeLine("NAK\r\n");
			#endif
		}

		while (!gl.timed_out && !gl.buffer_status) {
			wdt_clear();	// Clear watchdog

			// if character available, populate buffer
			if (((c = COMMS_getc()) != COMMS_BUFFER_EMPTY)) {
				xmdm_ReceiveChar(c);
			}


			if( cpu_is_timeout(&timer) ) {
				cpu_stop_timeout(&timer);
				gl.timed_out = true;
			}
		}

		if (gl.timed_out) {
			gl.timed_out = false;
			cpu_set_timeout( cpu_ms_2_cy(3000, FOSC0), &timer ); // restart timeout for 3000 ms
		}
	}

	if (retries > 0)
		return true;

	return false;
}

/*! \fn 	xmdm_ReceiveWait(void)
	\brief 	Waits for complete packet or timeout
 */
static void xmdm_ReceiveWait(void)
{
	int c;
	t_cpu_time timer;

	gl.timed_out = false;

	// start a simple timer with a 1-second timeout value
	cpu_set_timeout( cpu_ms_2_cy(1000, FOSC0), &timer ); // timeout in 1000 ms

	// wait for packet, error, or timeout
	while (!gl.timed_out && !gl.buffer_status) {

		// if character available, populate buffer
		if (((c = COMMS_getc()) != COMMS_BUFFER_EMPTY)) {
			xmdm_ReceiveChar(c);
		}

		// check for timeout
		if( cpu_is_timeout(&timer) ) {
			cpu_stop_timeout(&timer);
			gl.timed_out = true;
		}
	}
	cpu_stop_timeout(&timer);	// probably not necessary
}

/*! \fn 	XMDM_CalculateCRC(unsigned char *ptr, int count)
	\brief 	Generates 16-bit CRC or 8-bit checksum
	\param	ptr 	Pointer to data
	\param 	count 	Number of bytes to perform calculation on
 */
static int xmdm_CalculateCRC(U8 *ptr, int count)
{
    int crc;
    char i;

	if (CRCmode) {
		crc = 0;
		while (--count >= 0)
		{
			crc = crc ^ (int) *ptr++ << 8;
			i = 8;
			do
			{
				if (crc & 0x8000)
					crc = crc << 1 ^ 0x1021;
				else
					crc = crc << 1;
			} while(--i);
		}

		// is there an AVR32-GCC CRC function?
	}

	else {	// calculate checksum - return as int, mask later
		crc = 0;
		while (--count >= 0) {
			crc += *ptr++;
		}
	}

	return crc;
}

/*! \fn 	xmdm_ValidatePacket(U8 *bufptr, U8 *packet_number)
	\brief 	Validates received data packet
	\param 	bufptr 			Pointer to receive buffer
	\param 	packet_number	Last good packet number
 */
static U8 xmdm_ValidatePacket(U8 *bufptr, U8 *packet_number)
{
	U8 packet;
	int crc;

	packet = bad;

	if (!gl.timed_out) {

		if (!gl.recv_error) {

			if (bufptr[0] == SOH) { // valid start

				if (bufptr[1] == ((*packet_number+1) & 0xff)) { // sequential block number ?
					if ((bufptr[1] + bufptr[2]) == 0xff) { // block number and block number checksum are ok?
						crc = xmdm_CalculateCRC(&bufptr[3],128);      // compute CRC and validate it
						// crc will either contain 16-bit CRC or a checksum, depending on useCRC argument
						if (CRCmode) {
							if ((bufptr[131] == (U8)(crc >> 8)) && (bufptr[132] == (U8)(crc)))
							{
								*packet_number = *packet_number + 1; // good packet ... ok to increment
								packet = good;
							}// bad CRC, packet stays 'bad'
						}
						else {	// checksum only
							if (bufptr[131] == (U8)(crc)) {
								*packet_number = *packet_number + 1; // good packet ... ok to increment
								packet = good;
							}	// bad CRC, packet stays 'bad'
						}
					}// bad block number checksum, packet stays 'bad'
				}// bad block number or same block number, packet stays 'bad'

				else if (bufptr[1] == ((*packet_number) & 0xff))
				{ 					// same block number ... ack got glitched
					packet = dup;	// packet is duplicate, don't inc packet number
				}
			}

			else if (bufptr[0] == EOT)  // check for the end
				packet = end;

			else packet = bad;  // byte zero unrecognised
								// statement not required, included for clarity
		}
		else packet = err; // UART Framing or overrun error
	}
	else packet = out; // receive timeout error


	return (packet); // one of: good, dup, end, bad, err, out
}

/*! \fn  	xmdm_Respond(U8 packet)
	\brief	Responds to sender with ACK or NAK depending on packet status
	\param	packet 	status of last packet
 */
static void xmdm_Respond(U8 packet)
{
    // clear buffer flag here ... when acking or nacking sender may respond
    // very quickly.
    gl.buffer_status = empty;
    gl.recv_error = false; 		// framing and over run detection

    if ((packet == good) || (packet == dup) || (packet == end))
    {
    	// transmit ACK - ensure loaded in Tx buffer
    	while (COMMS_putchar(ACK) != COMMS_SUCCESS);
    }
    else
    {
    	xmdm_purge(); 				// let transmitter empty its buffer
		// transmit NAK (error) - ensure loaded in Tx buffer
		while (COMMS_putchar(NAK) != COMMS_SUCCESS);
    }
}

/*!	\fn 	XMDM_ReceiveApp(void)
 * 	\brief	Uploads application firmware from PC to MCU
 */
U16 XMDM_ReceiveApp(void)
{
	#define MAX_ERRORS	20
	U8 errors = 0;
	U16 ret = CEC_UNKNOWN_ERROR;
	unsigned char packet = 0;    // status flag
	unsigned char packet_number; // represents 'last successfully received packet'
	int index;
	const U8 *ptr;
	U32 flash_addr = APP_START_ADDRESS, check_flash_addr;
	U8 block[128];
	bool prelude_packet_received = false;		// packet number rolls over after 255!
	bool crc_packet_received = false;			// packet containing crc value
	U32 crc32 = 0xffffffff;
	U32 flash_crc32 = 0xffffffff;
	U8 flash_char;
	U32 user_page_crc32;

	//U32 i;

	union {
		U8 crc_byte[4];
		U32 crc_value;
	} crc_from_packet;

	crc_from_packet.crc_value = (U32)0;		// avoid compiler warning

#if 0	// display expected prelude packet
	COMMS_writeLine("prelude: ");
	for (index = 3; index <=130; index++) {
		COMMS_printHexChar(FIRMWARE_PRELUDE_PACKET[index-3]);
		COMMS_writeLine(" ");
	}
	COMMS_writeLine("\r\n");

	COMMS_writeLine("\r\n");
	ptr = FIRMWARE_PRELUDE_PACKET;
	for (index = 0; index <128; index++) {
		COMMS_printHexChar(*ptr++);
		COMMS_writeLine(" ");
	}
	COMMS_writeLine("\r\n");
return ret;
#endif

	
	// this displays the expected value, 2147500032 = 0x80004000
	//COMMS_writeLine("\r\nFlash start address: ");	COMMS_printULong(APP_START_ADDRESS); COMMS_writeLine("\r\n");

#if XMDM_DEBUG
	COMMS_writeLine("Preparing to receive program file...\r\n");
#endif
	xmdm_purge();				// let sender empty it's transmit buffer

#if XMDM_DEBUG
	COMMS_writeLine("Purge complete\r\n");
#endif

	packet_number = 0x00;       // initialize to first xmodem packet number - 1
	gl.recv_ptr = xmdbuf;       // point to recv buffer

	if (!xmdm_sendc()) {		// uses checksum mode or crc mode depending on CRCmode
		return CEC_TIMEOUT;
	}

	while ((packet != end) && (errors < MAX_ERRORS) && (packet != not_application_file)) {
		wdt_clear();		// Clear Watchdog
		xmdm_ReceiveWait();		// wait for error or buffer full
		packet = xmdm_ValidatePacket((U8*)xmdbuf, &packet_number);  // validate the packet

		// check for application file prelude packet
		if ((packet_number == 1) && (packet == good) && (!prelude_packet_received)) {
			ptr = FIRMWARE_PRELUDE_PACKET;		// assign pointer to comparison packet

			for (index = 3; index <= 130; index++) {
				if (xmdbuf[index] != *ptr) {
					packet = not_application_file;
					packet_number--;		// decrement packet number
#if 0  // Display failed comparison
					while (1) {
						COMMS_writeLine("invalid file ");
						COMMS_writeLine("xmdbuf: ");
						COMMS_printHexChar(xmdbuf[index]);
						COMMS_writeLine(" prelude: ");
						COMMS_printHexChar(*ptr);
						COMMS_writeLine("\r\n");
						delay_ms(1000);
					}
#endif
					break;
				}
				else {
					//COMMS_printHexChar(*ptr);
					ptr++;
				}
			}
		}

		//retrieve CRC value for program file from second packet
		if ((packet_number == 2) && (packet == good) && (!crc_packet_received)) {
			crc_from_packet.crc_byte[0] = xmdbuf[3];
			crc_from_packet.crc_byte[1] = xmdbuf[4];
			crc_from_packet.crc_byte[2] = xmdbuf[5];
			crc_from_packet.crc_byte[3] = xmdbuf[6];
		}

		gl.recv_ptr = xmdbuf;     // re-initialize buffer pointer before acknowledging

		switch(packet)
		{
			case good:
				// data handler here

				// don't do anything with first packet - it's not part of the application
				if ((packet_number == 1) && (!prelude_packet_received)) {
					prelude_packet_received = true;
					break;
				}

				// don't do anything with crc packet - it's not part of the application
				if ((packet_number == 2) && (!crc_packet_received)) {
					crc_packet_received = true;
					break;
				}

				// copy buffer to block and calculate running CRC
				for (index = 3; index <= 130; index++) {
					block[index-3] = xmdbuf[index];
					crc32 = CRC_updateCRC32(crc32, xmdbuf[index]);
				}

				// burn to flash
#if ALLOW_PRG_BURN			
				#warning HERE IS THE CULPRIT	
				//Disable_global_interrupt();
				//flashc_memcpy((void *)flash_addr, block, 128, true);	//false);
			//	flashc_memcpy((void *)flash_addr, &block[0], 1, true);	//false);
				//Enable_global_interrupt();
				// same problem: http://www.avrfreaks.net/forum/uc3c-flashcmemcpy-issue
				
				//for (i=0; i<128;i++) {
				//	flashc_memcpy((void *)flash_addr, &block[i], 1, true);
				//	flash_addr++;
				//}
#endif				
				
				if (flashc_is_lock_error() || flashc_is_programming_error()) {
					
					//while (1)	COMMS_writeLine("burn error");
					return CEC_BURN_ERROR;
				}

				flash_addr += 128;		// increment address by packet size

				// check to make sure another packet will fit in flash...
				if ((flash_addr + 128) >= (APP_START_ADDRESS + AVR32_FLASH_SIZE)) {
					ret = CEC_APP_TOO_LARGE;
				}
				break;

			case dup:
				// a counter for duplicate packets could be added here, to enable a
				// for example, exit gracefully if too many consecutive duplicates,
				// otherwise do nothing, we will just ack this

				//while (1)	COMMS_writeLine("duplicate");
				
				break;

			case end:
				// do nothing, we will exit

				ret = CEC_RECEIVED_OK;
				break;

			case not_application_file:
				// terminate and return false (unsuccessful)
				ret = CEC_NOT_APPLICATION_FILE;
				break;

			default:
				// bad, timeout or error -
				// if required, insert an error handler of some description,
				// for example, exit gracefully if too many errors

				errors++;
				if (errors < MAX_ERRORS)
					ret = CEC_TOO_MANY_ERRORS;
				else
					ret = CEC_UNKNOWN_ERROR;
				//return ret;  // while does using this cause the program not to RUN?????
				break;
		}

		xmdm_Respond(packet);                  // ack or nak

	}	// end of file transmission

	// when failing returns 32007, unknown error
	COMMS_writeLine("ret:   ");	COMMS_printULong(ret); COMMS_writeLine("\r\n");
	

	crc32 ^= 0xffffffff;	// finalize crc with one's complement

	// compare calculated crc value with expected value
	if (crc32 != crc_from_packet.crc_value) {
		// set invalid flag
		flashc_memset8((void *)UPV__VALIDAPP_STARTADDR, (U8)APP_INVALID, 1, true);
		return CEC_UPLOAD_CRC_ERROR;
	}

	// read flash memory back and ensure crc agrees  (byte order matters!)
	CRC_reset();
	for (check_flash_addr = APP_START_ADDRESS; check_flash_addr < flash_addr; check_flash_addr++) {
		memcpy(&flash_char, (void *)check_flash_addr, 1);
		flash_crc32 = CRC_updateCRC32(flash_crc32, flash_char);

		//print_char(BTLDR_USART, (int)flash_char);
	}
	flash_crc32 ^= 0xffffffff;	// finalize crc with one's complement

	// save burned crc value to user page, even if it's not correct
#if ALLOW_PRG_BURN		
	flashc_memcpy((void *)UPV__APPCRC32_STARTADDR, &flash_crc32, UPV__APPCRC32_SIZE, true);
#endif
	// save end address
#if ALLOW_PRG_BURN		
	flashc_memcpy((void *)UPV__APPSPACE_STARTADDR, &flash_addr, UPV__APPSPACE_SIZE, true);
#endif

#if 1
	delay_ms(100);
	COMMS_writeLine("\r\nCRC from sfw file: ");	COMMS_printULong(crc_from_packet.crc_value); COMMS_writeLine("\r\n");
	COMMS_writeLine("CRC from upload:   ");	COMMS_printULong(flash_crc32); COMMS_writeLine("\r\n");
#endif
	// compare burned crc value with expected value
	if (flash_crc32 != crc_from_packet.crc_value) {
		// set invalid flag
#if ALLOW_PRG_BURN	
		flashc_memset8((void *)UPV__VALIDAPP_STARTADDR, (U8)APP_INVALID, 1, true);
#endif
		return CEC_BURN_CRC_ERROR;
	}

	// if we are here, it should be a good application file
#if ALLOW_PRG_BURN	
	flashc_memset8((void *)UPV__VALIDAPP_STARTADDR, (U8)APP_VALID, 1, true);
#endif

#if 1
	while (1) {

		// display crc received from packet
		COMMS_writeLine("Packet crc32: ");
		COMMS_printULong(crc_from_packet.crc_value);

		// display crc processed from received data
		COMMS_writeLine("\tReceived crc32: ");
		COMMS_printULong(crc32);

		// display crc processed from flash memory
		COMMS_writeLine("\tFlash crc32: ");
		COMMS_printULong(flash_crc32);

		// display saved crc
		COMMS_writeLine("\tUser page crc32: ");
		memcpy(&user_page_crc32, (void *)UPV__APPCRC32_STARTADDR, UPV__APPCRC32_SIZE);
		COMMS_printULong(user_page_crc32);

		COMMS_writeLine("\r\n");

		delay_ms(1000);
	}
#endif

	return ret;
}



