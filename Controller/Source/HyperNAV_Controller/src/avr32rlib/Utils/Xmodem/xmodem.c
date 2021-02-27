/*! \file 	xmodem.c
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

# define OLD_XMDM 0	//	Prepare for future re-written XMODEM code

# include "xmodem.h"
# include <sys/time.h>
# include <string.h>
# include "FreeRTOS.h"
# include "task.h"
# include "systemtime.h"
# include "telemetry.h"
# include "extern.controller.h"
# include "files.h"
# include "watchdog.h"
# include "syslog.h"
#include "dbg.h"

static F64 F64Time(struct timeval* t) {

	return (F64)t->tv_sec + ((F64)t->tv_usec)/1.0e6;
}

# if OLD_XMDM

//	Characters used in the XMODEM protocol with special meaning
//

# define SOH           0x01		// Start of 128 byte Header
# define STX           0x02		// Start of 1K Header
# define EOT           0x04		// End of Transmission
static U8 const cEOT = EOT;		// End of Transmission
# define ACK           0x06		// Acknowledge
static U8 const cACK = ACK;		// Acknowledge
# define NAK           0x15		// Not Acknowledge
static U8 const cNAK = NAK;		// Not Acknowledge
# define CAN           0x18		// Cancel Character
static U8 const cCAN = CAN;		// Cancel Character
# define CRCCHR        'C'		// 'C' indicates CRC extension mode
static U8 const cCRCCHR = CRCCHR;	// 'C' indicates CRC extension mode
# define PAD			26		// XMODEM padding character, <SUB> - 26 dec, 0x1A


//	The XMODEM packet has
//		3 header bytes
//		128 data bytes
//		1 or 2 bytes at the end for either checksum or CRC
//
# define XM_HEAD_SIZE	  3
# define XM_DATA_SIZE	128
# define XM_CRC__SIZE	  2	//	Two characters at end for CRC
# define XM_CHCK_SIZE	  1	//	One character at end for checksum

static volatile U8 xmdbuf[XM_HEAD_SIZE+XM_DATA_SIZE+XM_CRC__SIZE];		// data buffer

# define full 	0xff
# define empty 	0x00

# define bad 	0x00
# define good 	0x01
# define dup 	0x02
# define end 	0x03
# define err 	0x04
# define out 	0x05


static struct {
  volatile U8 *recv_ptr;
  volatile U8 buffer_status;
  volatile bool recv_error;
  volatile bool timed_out;
} gl;

static bool CRCmode = false;	//! use checksum by default because of SUNACom restrictions


// TODO might want to redefine XMDM_USART with the "telemetry usart" definition
#define XMDM_USART   (&AVR32_USART0)
#define XMDM_DEBUG	0


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


/*! \fn 	xmdm_purge(void)
	\brief 	One second delay for sender to empty its transmit buffer
			Any received data is purged.
 */
static void xmdm_purge(void)
{
#if XMDM_DEBUG
	usart_write_line(XMDM_USART, "xmdm_purge\r\n");
#endif

        vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );
	tlm_flushRecv();
}



void xmdm_ReceiveChar ( U8 c )
{
	volatile U8 *local_ptr;

	local_ptr = gl.recv_ptr;

	//TODO should check for errors (framing, overrun, etc) here
	/*
	 * e.g. if (some_error)
	 * 	gl.recv_error = TRUE;   		// will NAK sender in XMDM_Respond
	 */

	*local_ptr++ = c;

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


/*! \fn		xmdm_sendc(void)
	\brief	Initializes communication with sender.

	Aborts after 10 tries.
	Sends 'C' when using 16-bit CRC, NAK when using checksum

	\return False 	if no response after 10 tries.
 */
static bool xmdm_sendc(void)
{
	struct timeval t1, t2;
	S8 retries = 10;

	// enable entry into while loops
	gl.buffer_status = empty;
	gl.timed_out = false;
	gl.recv_error = false; // checked in validate_packet for framing or overruns

	gettimeofday(&t1, NULL);

	// send character 'C' until we get a packet from the sender
	while ((!gl.buffer_status) && (retries-- > 0))
	{
		if ( CRCmode ) {
			tlm_send ( &cCRCCHR, 1, TLM_NONBLOCK);
		} else {
			tlm_send ( &cNAK, 1, TLM_NONBLOCK);
		}

		while ( !gl.timed_out && !gl.buffer_status )
		{
#ifdef FREERTOS_USED
			// Prevent owning the CPU (allow idle task to run to clear the WDT)
            vTaskDelay( (portTickType)TASK_DELAY_MS( 5 ) );
#endif

			// If character available, read from usart and populate buffer
			U8 c;
			if ( tlm_recv( &c, 1, TLM_PEEK | TLM_NONBLOCK) ) {
	    		tlm_recv( &c, 1, TLM_NONBLOCK );
				xmdm_ReceiveChar(c);
			}

			gettimeofday(&t2, NULL);
			if (F64Time(&t2) > (F64Time(&t1) + 3.0)) {
				gl.timed_out = true;
			}
		}

		if (gl.timed_out) {
			gl.timed_out = false;
			gettimeofday(&t1, NULL);
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
	struct timeval t1, t2;
	int read = 0;

    gl.timed_out = false;

    gettimeofday(&t1, NULL);

    // wait for packet, error, or timeout
    while (!gl.timed_out && !gl.buffer_status) {

    	// if character available, read from usart and populate buffer
    	U8 c;
    	read = 0;

    	read = tlm_recv( &c, 1, TML_NONBLOCK );
		if(read == 1)
			xmdm_ReceiveChar(c);

    	gettimeofday(&t2, NULL);
		if (F64Time(&t2) > (F64Time(&t1) + 1.0)) {
			gl.timed_out = true;
		}
	}
}

/*! \fn 	XMDM_CalculateCRC(U8 *ptr, int count, bool useCRC)
	\brief 	Generates 16-bit CRC or 8-bit checksum
	\param	ptr 	Pointer to data
	\param 	count 	Number of bytes to perform calculation on
	\param  useCRC  Use crc mode if true, checksum otherwise
 */
static int xmdm_CalculateCRC(U8 *ptr, int count, bool useCRC)
{
    int crc;
    char i;

	if (useCRC) {
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

	if (!gl.timed_out)
	{
		if (!gl.recv_error)
		{
			DEBUG("--------- Will receive packet #%d ----------",*packet_number+1);

			if (bufptr[0] == SOH) // valid start?
			{
				DEBUG("Valid start - CPN=%d",bufptr[1]);
				if (bufptr[1] == ((*packet_number+1) & 0xff)) // sequential block number ?
				{
					if ((bufptr[1] + bufptr[2]) == 0xff)	// block number and block number checksum are ok?
					{
						crc = xmdm_CalculateCRC(&bufptr[3],128, CRCmode);      // compute CRC and validate it
						// crc will either contain 16-bit CRC or a checksum, depending on useCRC argument
						if (CRCmode)
						{
							if ((bufptr[131] == (U8)(crc >> 8)) && (bufptr[132] == (U8)(crc)))
							{
								*packet_number = *packet_number + 1; // good packet ... ok to increment
								packet = good;
							}// bad CRC, packet stays 'bad'
							else
							{
								DEBUG("CRC Error");
								packet = bad;
							}
						}
						else
						{	// checksum only
							if (bufptr[131] == (U8)(crc))
							{
								*packet_number = *packet_number + 1; // good packet ... ok to increment
								packet = good;
							}	// bad CSUM, packet stays 'bad'
							else
							{
								DEBUG("Checksum Error");
								packet = bad;
							}
						}
					}
					else	// bad block number checksum, packet stays 'bad'
					{
						DEBUG("Bad block # complement");
					}
				}
				else	// bad block number or same block number, packet stays 'bad'
				{
					DEBUG("Non sequential block # - CPN=%d",bufptr[1]);

					if (bufptr[1] == ((*packet_number) & 0xff))
					{ 					// same block number ... ack got glitched
						DEBUG("Duplicate block # - CPN=%d",bufptr[1]);
						packet = dup;	// packet is duplicate, don't inc packet number
					}
				}
			}
			else // It is not start
			{
				if (bufptr[0] == EOT)  // check for the end
					packet = end;
				else // byte zero unrecognised
				{
					DEBUG("Byte zero not SOH nor EOT - LGP=%d",*packet_number);
					packet = bad;
				}
			}
		}
		else // UART Framing or overrun error
		{
			DEBUG("RX Error");
			packet = err;
		}
	}
	else // receive timeout error
	{
		DEBUG("RX Timeout");
		packet = out;
	}

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

    if ( packet == good || packet == dup || packet == end ) {
    	// transmit ACK - call blocking to ensure loaded in Tx buffer
    	tlm_send ( &cACK, 1, 0 );
    	DEBUG("ACK");
    }
    else
    {
    	xmdm_purge(); 				// let transmitter empty its buffer
		// transmit NAK (error) - call blocking to ensure loaded in Tx buffer
    	tlm_send ( &cNAK, 1, 0 );
    	DEBUG("NACK");
    }
}


S16 XMDM_recv_from_usart ( char const* filename, bool useCRC ) {

	#define MAX_ERRORS	20
	U8 errors = 0;

	//  Receive packets
	char xm_packet [ XM_DATA_SIZE ];
	S16 ret = XMDM_FAIL;
	U8 packet_number; // represents 'last successfully received packet'
	int index;

	if ( f_exists( filename ) ) {
		return XMDM_FAIL;
    }

	fHandler_t fh;
	if ( FILE_OK != f_open ( filename, O_WRONLY | O_CREAT, &fh ) ) {
		return XMDM_FAIL;
	}

	// delay to allow transmitter to (hopefully) finish any current transmissions
	xmdm_purge();
	
	packet_number = 0x00;       // initialize to first xmodem packet number - 1
	gl.recv_ptr = xmdbuf;       // point to recv buffer

	if (!xmdm_sendc()) {		// uses checksum mode or crc mode depending on CRCmode
		f_close ( &fh );
		f_delete ( filename );
		return XMDM_TIMEOUT;
	}

	U8 packet = 0;

	while ( packet != end && errors < MAX_ERRORS ) {	// get remainder of file

		// Clear WDT
		watchdog_clear();

		xmdm_ReceiveWait();		// wait for error or buffer full
		packet = xmdm_ValidatePacket((U8*)xmdbuf, &packet_number);  // validate the packet
		gl.recv_ptr = xmdbuf;     // re-initialize buffer pointer before acknowledging
		
		switch (packet) {
			case good:
				// data handler here
				/* packet structure: 
					Byte 1 (index 0) = Start of Header
					Byte 2 = packet number
					Byte 3 = packet number checksum (byte 2+3 = 0xff)
					Bytes 4 - 131 = Packet Data
					Bytes 132 - 133 = CRC
				*/	

				// copy buffer to block and calculate running CRC
				for (index = 3; index <= 130; index++) {
					xm_packet[index-3] = xmdbuf[index];
				}

				// save packet
				if ( XM_DATA_SIZE != f_write ( &fh, xm_packet, XM_DATA_SIZE ) ) {

					f_close ( &fh );
					f_delete ( filename );
					return XMDM_FAIL;
				}			
				break;

			case dup:
				// a counter for duplicate packets could be added here, to enable a
				// for example, exit gracefully if too many consecutive duplicates,
				// otherwise do nothing, we will just ack this
				break;

			case end:
				// do nothing, we will exit

				ret = XMDM_OK;
				break;

			default:
				// bad, timeout or error -
				// if required, insert an error handler of some description,
				// for example, exit gracefully if too many errors

				errors++;
				if (errors > MAX_ERRORS)
					ret = XMDM_TOO_MANY_ERRORS;
				else
					ret = XMDM_FAIL;

				DEBUG("Packet error! (%d)",errors);
				break;
		}

		xmdm_Respond(packet);	// ack or nak
		
	}// end of file transmission

	f_close ( &fh );

	if (ret != XMDM_OK) {
		f_delete ( filename );
	}

	return ret;

}

S16 XMDM_send_to_usart ( char const* filename, bool use1k ) {
	#define TRANSFER_START_TIMEOUT_SECONDS	20.0
	#define ACK_TIMEOUT_SECONDS	6.0

	U8 c;
	bool sendCRC = false;
	struct timeval t1;
	F64 timeout;
	U16 pkt_num = 1;	// note first packet is 1, but rolls over to 0
	char xm_packet [ XM_DATA_SIZE ];
	S32	p;

	// Check to ensure file exists
	if ( !f_exists( filename ) ) {
		return XMDM_FAIL;
	}

	// open file for reading
	fHandler_t fh;
	if ( FILE_OK != f_open ( filename, O_RDONLY, &fh ) ) {
		return XMDM_FAIL;
	}

	// get size of file, calculate number of packets
	S32 const fileSize = f_getSize ( &fh );
	S32 const numPackets = 1 + ( fileSize-1 ) / XM_DATA_SIZE;
	S32 const finalPacketSize = fileSize - XM_DATA_SIZE * ( numPackets - 1);

	// wait, with timeout, for receiver to start download process
	bool wait_for_receiver = true;
	gettimeofday(&t1, NULL);			// get current time
	// calculate startup timeout
	timeout = F64Time(&t1) + TRANSFER_START_TIMEOUT_SECONDS;
	do {
		// wait for C (CRC) or NAK (checksum) from receiver
		while (timeout >= F64Time(&t1))
		{
			// Clear WDT
			watchdog_clear();

			gettimeofday(&t1, NULL);	// update current time
			if (tlm_recv(&c, 1, TLM_PEEK | TLM_NONBLOCK)) {	// check for received char
				tlm_recv( &c, 1, TLM_NONBLOCK );			// retrieve char
				break;
			}
			// Don't own the CPU while waiting for incoming char
            vTaskDelay( (portTickType)TASK_DELAY_MS( 50 ) );
		}

		// check for timeout - if so,close file and exit
		if (F64Time(&t1) > timeout) {
			f_close ( &fh );
			return XMDM_TIMEOUT;
		}

		if (c == CRCCHR) {
			sendCRC = true;
			wait_for_receiver = false;
		}

		else if ( c == NAK) {
			sendCRC = false;
			wait_for_receiver = false;
		}

		else { // not a valid start character
			continue;
		}


	} while (wait_for_receiver);

	// receiver has successfully initiated transfer, so we can send the file...

	U8 index;
	for ( p=0; p<numPackets; p++ ) {

		// Clear WDT
		watchdog_clear();

		S16 const this_packet_size = (p<numPackets-1)
				? XM_DATA_SIZE
				: finalPacketSize;
		if ( this_packet_size != f_read ( &fh, xm_packet, this_packet_size ) ) {

			f_close ( &fh );
			return XMDM_FAIL;
		}

		//  The following loop executes only for the final packet.
		if ( finalPacketSize < XM_DATA_SIZE ) {
			S16 byte;
			for ( byte=this_packet_size; byte<XM_DATA_SIZE; byte++ ) {
				//xm_packet[byte] = 0;
				xm_packet[byte] = PAD;	// use typical xmodem padding char
			}
		}

		//  xm_sent_packet ()
		index = 0;
		xmdbuf[index++] = SOH;		// Start of header
		xmdbuf[index++] = pkt_num;	// packet number
		xmdbuf[index++] = ~pkt_num;	// 1's complement of pkt_num, or 0xff-pkt_num

		// copy data to output packet
		U8 j;
		int crc;
		for (j = 0; j <XM_DATA_SIZE; j++) {
			xmdbuf[index++] = xm_packet[j];		// copy byte
		}

		// compute CRC or checksum
		crc = xmdm_CalculateCRC((U8 *)xm_packet,XM_DATA_SIZE, sendCRC);


		if (sendCRC) {	// CRC?
			xmdbuf[index++] = (crc>>8);	// MSB of crc
			xmdbuf[index] = crc;	// LSB of crc - don't update index (last byte)

		}
		else {	// checksum
			xmdbuf[index] = (U8)crc;	// checksum - don't update index (last byte)
		}

		wait_for_receiver = true;	// reset wait flag
		do {
			tlm_send((const char *)xmdbuf, index+1, TLM_NONBLOCK);	// send packet

			// wait for ACK/NAK/timeout
			gettimeofday(&t1, NULL);			// get current time
			timeout = F64Time(&t1) + ACK_TIMEOUT_SECONDS;	// determine timeout value

			while (timeout >= F64Time(&t1)) {
				gettimeofday(&t1, NULL);	// update current time
				if (tlm_recv(&c, 1, TLM_PEEK | TLM_NONBLOCK)) {	// check for received char
					tlm_recv( &c, 1, TLM_NONBLOCK );			// retrieve char
					break;
				}
				// yield to other tasks
                vTaskDelay( (portTickType)TASK_DELAY_MS( 5 ) );
			}

			// check for timeout - if so,close file and exit
			if (F64Time(&t1) > timeout) {
				f_close ( &fh );
				return XMDM_TIMEOUT;
			}

			if (c == ACK) {				// received ACK?
				wait_for_receiver = false;	// yes, send next packet
			}

			else if (c == NAK)	 {	// received NAK?
				continue;			// resend packet
			}

			else {				// timeout
				continue;		// resend packet
			}

		} while (wait_for_receiver);

		pkt_num++;				// increment packet
	}

	f_close ( &fh );

	// All packets have been sent.
	// Send EOT
	wait_for_receiver = false;
	char eot_char = EOT;
	do {
		tlm_send(&eot_char, 1, 0);

		// wait for ACK
		// wait for ACK/NAK/timeout
		gettimeofday(&t1, NULL);			// get current time
		timeout = F64Time(&t1) + ACK_TIMEOUT_SECONDS;	// determine timeout value

		while (timeout >= F64Time(&t1)) {
			gettimeofday(&t1, NULL);	// update current time
			if (tlm_recv(&c, 1, TLM_PEEK | TLM_NONBLOCK)) {	// check for received char
				tlm_recv( &c, 1, TLM_NONBLOCK );			// retrieve char
				break;
			}
			// yield to other tasks
            vTaskDelay( (portTickType)TASK_DELAY_MS( 5 ) );
		}

		// check for timeout - if so,close file and exit
		if (F64Time(&t1) > timeout) {
			return XMDM_TIMEOUT;
		}


		if (c == ACK) {				// received ACK?
			wait_for_receiver = true;
		}

	} while (!wait_for_receiver);

	return XMDM_OK;
}

# else

# define SOH           0x01		// Start of 128 byte Header
# define STX           0x02		// Start of 1K Header
# define EOT           0x04		// End of Transmission
static U8 const cEOT = EOT;		// End of Transmission
# define ACK           0x06		// Acknowledge
static U8 const cACK = ACK;		// Acknowledge
# define NAK           0x15		// Not Acknowledge
static U8 const cNAK = NAK;		// Not Acknowledge
# define CAN           0x18		// Cancel Character
//static U8 const cCAN = CAN;		// Cancel Character
static U8 const cCAN3[] = "\x18\x18\x18";	//	Send multiple CAN to terminate transfer
# define CRCCHR        'C'		// 'C' indicates CRC extension mode
static U8 const cCRCCHR = CRCCHR;	// 'C' indicates CRC extension mode
# define PAD			26		// XMODEM padding character, <SUB> - 26 dec, 0x1A


#define DLY_1S 1000
#define MAXRESEND 5

static int xmdm_ClcCRC(U8 const* ptr, S16 count )
{
    char i;

	int crc = 0;
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

	return crc;
}

static int verifyBuffer(S16 crc, U8 const* buf, S16 sz)
{
	if (crc) {
		U16 crc = xmdm_ClcCRC(buf, sz);
		U16 tcrc = (buf[sz]<<8)+buf[sz+1];
		if (crc == tcrc)
			return 1;
	}
	else {
		S16 i;
		U8 cks = 0;
		for (i = 0; i < sz; ++i) {
			cks += buf[i];
		}
		if (cks == buf[sz])
		return 1;
	}

	return 0;
}

static int readBytes( U8* c, S16 nBytesToRead, U16 tout_ms ) {

	S16 nRead = 0;

	if ( nBytesToRead <= 0 ) return nRead;

	struct timeval t1, t2;
	gettimeofday(&t1, NULL);

	do {
		S16 thisRead = tlm_recv( c, nBytesToRead-nRead, TLM_NONBLOCK);

		if ( thisRead ) {

			nRead += thisRead;
			if ( nRead >= nBytesToRead ) {
				return nRead;
			}
			gettimeofday(&t1, NULL);
			c += thisRead;
		}

		gettimeofday(&t2, NULL);

	} while ( F64Time(&t2) < F64Time(&t1)+tout_ms/1000.0 );

	return nRead;
}

static int readByte( U8* c, U16 tout_ms ) {

# if 1
	return readBytes( c, 1, tout_ms );
# else
	struct timeval t1, t2;
	gettimeofday(&t1, NULL);
	F64 const ft1 = F64Time(&t1);

	do {
		if ( tlm_recv( c, 1, TLM_PEEK | TLM_NONBLOCK) ) {
    		tlm_recv( c, 1, TLM_NONBLOCK );
			return 1;
		}

		gettimeofday(&t2, NULL);

	} while ( F64Time(&t2) < ft1+tout_ms/1000.0 );

	return 0;
# endif

}


static void flushinput(void)
{
	U8 dummy;
	while ( readByte( &dummy, 100 ) )
		watchdog_clear();
}

# define XMHEAD 3
# define XMDATA 1024
# define XMCRC  2	// Or 1 for checksum
# define XMBUFF (XMHEAD+XMDATA+XMCRC+1)	//	== 1030
static U8 xbuff[XMBUFF]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */

S16 XMDM_recv_from_usart ( char const* filename, bool useCRC ) {

	//DBG	syslog_setOut ( SYSLOG_TLM, false );

	if ( f_exists( filename ) ) {
		return XMDM_FAIL;
    }

	fHandler_t fh;
	if ( FILE_OK != f_open ( filename, O_WRONLY | O_CREAT, &fh ) ) {
		return XMDM_FAIL;
	}

    vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );

	S16 bufsz,    crc = ( useCRC ? 1 : 0 );
	char firstRequest = ( useCRC ? CRCCHR : NAK );
	U8 packetno = 1;
	U8 prevPackno = 0;
    U8 c;
	S16 sendAttemt = 0;

	for(;;) {
		S16 retry;
		for( retry = 0; retry < 30; ++retry) {	//	read first character with a 1 second timeout
			watchdog_clear();
			//	Xmodem is receiver driven => Trigger Sender Action by sending
			//	NAK (checksum XMODEM) or 'C' (CRC XMODEM):
			//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR P%02x T%02d %S_%02x", packetno, retry, firstRequest );
			//	After the first packet, ACK (or NAK) is sent
			//	immediately after receiving the packet.
			if (firstRequest) tlm_send ( &firstRequest, 1, 0 );
			//	Check Sender Response
			if ( readByte( &c, DLY_1S ) ) {
				//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR R_%02x", c );
				switch (c) {

				case SOH:	//	First character of packet indicates to use 128-byte Xmodem
					bufsz = 128;
					goto start_recv;

				case STX:	//	First character of packet indicates to use 1K-byte Xmodem
					bufsz = 1024;
					goto start_recv;

				case EOT:	//	Sender sent end-of-text. All packets transferred.
					flushinput();
    				tlm_send ( &cACK, 1, 0 );
					f_close ( &fh );
					return XMDM_OK; /* normal end */

				case CAN:
					//	Check for second CAN, to confirm this is indeed an abort
					if ( readByte( &c, DLY_1S ) && CAN==c ) {
						flushinput();
    					tlm_send ( &cACK, 1, 0 );
						f_close ( &fh );
						f_delete ( filename );
						return XMDM_CAN; /* canceled by remote */
					}
					//	No second CAN, ignore first CAN & retry
					break;

				default:
					flushinput();
    				tlm_send ( &cNAK, 1, 0 );
					//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR NAK" );
					break;
				}
			}

		}

		//	Sender may not support CRC, try CheckSum instead.
		//	This executes ONLY while requesting the first packet,
		//	and only if the 15 retrie have been attemped.
		//	This is NON-STANDARD!
		if (firstRequest == CRCCHR) { firstRequest = NAK; crc = 0; continue; }

		//	Getting here if getting nothing.
		flushinput();
		//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR 0 CAN CAN CAN" );
    	tlm_send ( cCAN3, 3, 0 );
		f_close ( &fh );
		f_delete ( filename );
        vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
		return XMDM_TIMEOUT;

	start_recv:

		watchdog_clear();

# ifdef STEPWISE
		U8 *p = xbuff;
		S16 gotBytes = 0;
		S16 i;
		*p++ = c;	//	c was the first character received
		for( i=0;  i < (XMHEAD-1)+bufsz+(crc?2:1); i++ ) {
			if( !readByte( &c, DLY_1S ) ) goto reject;
			*p++ = c;
			gotBytes ++;
		}
# else
		xbuff[0] = c;
		S16 const needBytes = (XMHEAD-1)+bufsz+(crc?2:1);
		S16 const gotBytes = readBytes( xbuff+1, needBytes, DLY_1S );
		if ( needBytes != gotBytes ) goto reject;
# endif

		//	Make sure that the header is well formatted
		//	and the packet number is as expected.
		//	Also, accept a re-send of the previous packet,
		//	which happens if the most recent ACK was not
		//	received at the other side.
		//	However, if the just received packet has the
		//	previous packet number, do not write it to file,
		//	but send another ACK to prompt the sender to
		//	transmit the next packet.
		//	Also, make sure to prevent an infinite loop
		//	by decrementing the sendAttemt counter.

		//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR rx P%02x %02x %d %d %d", xbuff[1], 0xFF & (U8)(~xbuff[2]), crc, verifyBuffer(0,xbuff+3,bufsz), verifyBuffer(1,xbuff+3,bufsz) );

		if ( xbuff[1] == (0xFF & (U8)(~xbuff[2]))
		  && ( xbuff[1] == packetno || xbuff[1] == prevPackno )
		  && verifyBuffer(crc, xbuff+XMHEAD, bufsz) ) {

			if (xbuff[1] == packetno)	{
				if ( bufsz != f_write ( &fh, xbuff+XMHEAD, bufsz ) ) {
					f_close ( &fh );
					f_delete ( filename );
					//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR 1 CAN CAN CAN" );
    				tlm_send ( cCAN3, 3, 0 );
					flushinput();
                    vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
					return XMDM_FAIL; /* no sync */
				}
				prevPackno = packetno;
				packetno++;
				sendAttemt = 0;
			} else {
				//	This was a resent of the previous packet.
				//	Silently accept, and will send an ACK,
				//	unless this has happend more than MAXRESEND times.
			}

			//	The following contition can only happen
			//	if the same packet has been sent MAXRESEND,
			//	and no ACK was received in response.
			if (sendAttemt++ >= MAXRESEND) {
				f_close ( &fh );
				f_delete ( filename );
				flushinput();
				//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR 2 CAN CAN CAN" );
    			tlm_send ( cCAN3, 3, 0 );
                vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
				return XMDM_TIMEOUT; /* abort after MAXRESEND attempts */
			}

			firstRequest = 0;
			//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR ACK" );
    		tlm_send ( &cACK, 1, 0 );
			continue;
		}


	reject:

		flushinput();
		if (sendAttemt++ >= MAXRESEND) {
			f_close ( &fh );
			f_delete ( filename );
			//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR 3 CAN CAN CAN" );
    		tlm_send ( cCAN3, 3, 0 );
            vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
			return XMDM_TIMEOUT; /* abort after MAXRESEND attempts */
		}
		//DBG	SYSLOG ( SYSLOG_NOTICE, "XMR NAK %d", gotBytes );
    	tlm_send ( &cNAK, 1, 0 );
	}
}

S16 XMDM_send_to_usart ( char const* filename, bool use1k ) {

	//DBG	syslog_setOut ( SYSLOG_TLM, false );

	// Check to ensure file exists
	if ( !f_exists( filename ) ) {
		return XMDM_FAIL;
	}

	// open file for reading
	fHandler_t fh;
	if ( FILE_OK != f_open ( filename, O_RDONLY, &fh ) ) {
		return XMDM_FAIL;
	}

    vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );

	S32 const fileSize = f_getSize ( &fh );

	S16 bufsz, crc = -1;
	U8 packetno = 1;
	S16 i;
	S32 len = 0;
	S32 need;
	U8 c;
	S16 retry;

	for(;;) {
		for( retry = 0; retry < 30; ++retry) {

			watchdog_clear();

			//SYSLOG ( SYSLOG_NOTICE, "XMS P%03d T%02d", packetno, retry );
			if ( readByte( &c, DLY_1S ) ) {
				//SYSLOG ( SYSLOG_NOTICE, "XMS R_%02x", c );
				switch (c) {
				case CRCCHR:
					crc = 1;
					goto start_send;
				case NAK:
					crc = 0;
					goto start_send;
				case CAN:
					if ( readByte( &c, DLY_1S ) && CAN == c ) {
    					tlm_send ( &cACK, 1, 0 );
						flushinput();
						f_close ( &fh );
						return XMDM_CAN; /* canceled by receiver */
					}
					break;
				default:
					break;
				}
			}
		}
		//SYSLOG ( SYSLOG_NOTICE, "XMS 0 CAN CAN CAN" );
    	tlm_send ( cCAN3, 3, 0 );
		flushinput();
		f_close ( &fh );
        vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
		return XMDM_TIMEOUT; /* no sync */

		for(;;) {
		start_send:
			if ( use1k ) {
				xbuff[0] = STX; bufsz = 1024;
			} else {
				xbuff[0] = SOH; bufsz = 128;
			}
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;
			need = fileSize - len;
			if (need > bufsz) need = bufsz;
			//SYSLOG ( SYSLOG_NOTICE, "XMS P F%05d L%05d C%04d B%04d", fileSize, len, need, bufsz );
			if (need > 0) {
				memset (xbuff+XMHEAD, PAD, bufsz);
				if ( need != f_read ( &fh, xbuff+XMHEAD, need ) ) {
					//SYSLOG ( SYSLOG_NOTICE, "XMS 1 CAN CAN CAN" );
    				tlm_send ( cCAN3, 3, 0 );
					flushinput();
					f_close ( &fh );
                    vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
					return XMDM_FAIL; /* no sync */
				}
				if (crc) {
					U16 crc16 = xmdm_ClcCRC(xbuff+XMHEAD, bufsz);
					xbuff[XMHEAD+bufsz  ] = (crc16>>8) & 0xFF;
					xbuff[XMHEAD+bufsz+1] = (crc16   ) & 0xFF;
				} else {
					U8 ccks = 0;
					for (i = XMHEAD; i < XMHEAD+bufsz; ++i) {
						ccks += xbuff[i];
					}
					xbuff[XMHEAD+bufsz] = ccks;
				}
				for (retry = 0; retry < MAXRESEND; ++retry) {
					watchdog_clear();
					//SYSLOG ( SYSLOG_NOTICE, "XMS P%03d S%05d", packetno, bufsz+4+(crc?1:0) );
    				tlm_send ( xbuff, XMHEAD+bufsz+(crc?2:1), 0 );
					if ( readByte( &c, DLY_1S*2 ) ) {
						//SYSLOG ( SYSLOG_NOTICE, "XMS R_%02x", c );
						switch (c) {
						case ACK:
							packetno++;
							len += need;
							goto start_send;
						case CAN:
							if ( readByte( &c, DLY_1S ) && CAN == c ) {
    							tlm_send ( &cACK, 1, 0 );
								flushinput();
								f_close ( &fh );
								return XMDM_CAN; /* canceled by receiver */
							}
							break;
						case NAK:	//	Receiver requested a resend
						default:
							break;
						}
					}
				}
				//SYSLOG ( SYSLOG_NOTICE, "XMS 2 CAN CAN CAN" );
    			tlm_send ( cCAN3, 3, 0 );
				flushinput();
				f_close ( &fh );
                vTaskDelay( (portTickType)TASK_DELAY_MS( 3000 ) );
				return XMDM_TIMEOUT; /* send error */
			} else {	//	Sending complete, send End-Of-Text character
				for (retry = 0; retry < 10; ++retry) {
					watchdog_clear();
					//SYSLOG ( SYSLOG_NOTICE, "XMS P%03d EOT", packetno );
    				tlm_send ( &cEOT, 1, 0 );
					if ( readByte( &c, (DLY_1S)*2 ) && ACK == c ) break;
				}
				flushinput();
				f_close ( &fh );
				return (c == ACK)?XMDM_OK:XMDM_FAIL;
			}
		}
	}
}

# endif


