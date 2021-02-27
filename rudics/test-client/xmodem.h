#ifndef XMODEM_H
#define XMODEM_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/xmodem.h#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: xmodem.c,v $
 * Revision 1.2  2005/02/22 21:43:39  swift
 * Shorted packet timeout.
 *
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.5  2003/08/13 21:52:12  swift
 * Fixed typos in embedded TeX commands.
 *
 * Revision 1.4  2002/12/31 16:58:29  swift
 * Eliminated the reference to the unistd.h header file.
 *
 * Revision 1.3  2002/10/16 13:04:17  swift
 * Modified definition of CanPkt to change status classification
 * from success to fail.
 *
 * Revision 1.2  2002/10/09 00:02:26  swift
 * Minor modifications and clean-up.
 *
 * Revision 1.1  2002/05/07 22:19:45  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define XmodemChangeLog "$RCSfile: xmodem.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include <string.h>
#include "logger.h"
#include "serial.h"

#define MAXBUFSIZE 1024
#define NUL 0x00
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define BS  0x08
#define NAK 0x15
#define CAN 0x18
#define PAD 0x1a

/*------------------------------------------------------------------------*/
/* structure to contain an xmodem packet                                  */
/*------------------------------------------------------------------------*/
/**
   This structure is an abstraction of packets used in the xmodem protocol.
   This structure allows for implementation of four variants of the xmodem
   protocol:

      \begin{verbatim}
      1. Xmodem with checksum and 128 byte blocks.
      2. Xmodem with checksum and 1024 byte blocks.
      3. Xmodem with 16-bit CRC and 128 byte blocks.
      4. Xmodem with 16-bit CRC and 1024 byte blocks.
      \end{verbatim}
   
   The send and receive modules of the xmodem session negotiate which of
   these four variants will be used.  This implementation is based on 3
   sources of information about the xmodem protocol.  The primarily source
   is a document entitled:

                          \begin{verbatim}
                XMODEM/YMODEM PROTOCOL REFERENCE
            A Compendium of documents describing the
                       XMODEM and YMODEM
                     File Transfer Protocols
                          \end{verbatim}

   which was formatted on 10/14/88 and edited by Chuck Forsberg.  No other
   citation is available, as I downloaded it after doing a web search.  I
   also derived useful information from an article in the Sep90 edition of
   Embedded Systems Journal written by Jonathan Ward, entitled:

                         \begin{verbatim}
                XMODEM, XMODEM-1K, and XMODEM/CRC
                         \end{verbatim}

   Finally, I used information contained in the 2nd edition of the book:

                         \begin{verbatim}
             C Programmer's Guide to Serial Communications
                         \end{verbatim}

   by Joe Campbell (Sams Publishing, ISBN 0-672-30286-1).  Chapter 4 of this
   reference is the most relevant.

   written by Dana Swift
*/
struct Packet
{
   unsigned char StartByte;
   unsigned char PktNum;
   unsigned char PktNum1C;
   unsigned char data[MAXBUFSIZE];
   unsigned char crc1;
   unsigned char crc2;
};

#define SessionTimeOut -8
#define FalseEot       -7
#define CanPkt         -6
#define WrongPkt       -5
#define PrevPkt        -4
#define ShortPkt       -3
#define BadPkt         -2
#define NullArg        -1
#define NoPkt           0
#define OkPkt           1
#define EotPkt          2

/* external declaration of the number of packet-retry attempts before aborting the transfer */
extern const int NRetry;

/* external declaration of the constant timeout periods (sec) */
extern const int ByteTimeout, PacketTimeout, EotTimeout;

/* external declaration of the adjustable timeout periods (sec) */
extern long int MaxSessionTime;

/* global function prototypes */
int abort_transfer(const struct SerialPort *port);
int BufSize(struct Packet *pkt);
int CheckCanPkt(const struct SerialPort *port);
int checksum(struct Packet *pkt, unsigned char *crc1, unsigned char *crc2);
int crc16bit(struct Packet *pkt, unsigned char *crc1, unsigned char *crc2);
int LogPacket(struct Packet *pkt);

#endif /* XMODEM_H */
