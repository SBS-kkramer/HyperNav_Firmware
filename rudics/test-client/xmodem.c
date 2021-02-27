#ifndef XMODEM_H
#define XMODEM_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/xmodem.c#1 $
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

#include <assert.h>
#include <ctype.h>

/* local function prototypes */
unsigned int Crc16Bit(const unsigned char *msg, unsigned int len); 
unsigned int sleep(unsigned int seconds); 

/* define the number of packet-retry attempts before aborting the transfer */
const int NRetry = 10;

/* define the constant timeout periods (sec) */
const int ByteTimeout=2, PacketTimeout=10, EotTimeout=60;

/* define the adjustable timeout periods (sec) */
long int MaxSessionTime=0;

/*------------------------------------------------------------------------*/
/* function to transmit a request to abort the xmodem session             */
/*------------------------------------------------------------------------*/
/*
   This function transmits a request to abort the xmodem session.
   Cancelling an xmodem session is not particularly well defined by the
   xmodem protocol.  Different implementations of the xmodem protocol
   implement such requests in many different ways or not at all.  This
   implementation matches Chuck Forsberg's by transmitting 8 CAN characters
   followed by 8 BS characters.

      \begin{verbatim}
      output:
 
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.

         On success, this returns a positive value otherwise it returns with
         zero or a negative value according to the following key.  See xmodem.h for
         the values of the following tokens.

            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The attempt to transmit the packet failed.
            OkPkt:    The packet was successfully transmitted.
            
      \end{verbatim}

   written by Dana Swift
*/
int abort_transfer(const struct SerialPort *port)
{
   /* define the logging signature */
   static cc FuncName[] = "abort_transfer()";

   /* initialize the function's return value */
   int status=NullArg;
  
   /* validate the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pputb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL pputb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      int i;
      
      /* send 8 CANs like Forsberg does */
      for (status=OkPkt, i=0; status>0 && i<8; i++) status=port->putb(CAN);
   
      /* send 8 BSs like Forsberg does */
      for (i=0; status>0 && i<8; i++) status=port->putb(BS);

      /* pause before continuing */
      sleep(10);
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to return the size of the data-buffer of an xmodem packet     */
/*------------------------------------------------------------------------*/
/*
   This function returns the size of the data buffer of an xmodem packet.

      \begin{verbatim}
      input:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.
                     
      output:

         This function returns the size of the data buffer.
            
      \end{verbatim}
      
   written by Dana Swift
*/
int BufSize(struct Packet *pkt)
{
   /* define the logging signature */
   static cc FuncName[] = "BufSize()";

   /* initialize the return value */
   int bufsize=0;

   /* validate the packet */
   if (!pkt) 
   {
      /* create the message */
      static cc msg[]="NULL Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* select the size of the data buffer based on the packet type */
   else switch (pkt->StartByte)
   {
      /* NULL packet has zero buffer size */
      case NUL: {bufsize=0;  break;}

      /* 128-byte packet */
      case SOH: {bufsize=128; break;}

      /* 1024 byte packet */
      case STX: {bufsize=1024; break;}

      /* end-of-transmission packet has zero data bytes */
      case EOT: {bufsize=0;  break;}

      /* undefined packet has zero buffer size */
      default:
      {
         /* create the message */
         static cc format[]="Undefined start-byte: 0x%02x\n";

         /* make the logentry */
         LogEntry(FuncName,format,pkt->StartByte);
      }
   }
   
   return bufsize;
}

/*------------------------------------------------------------------------*/
/* compute the 8-bit checksum of an xmodem packet and test for corruption */
/*------------------------------------------------------------------------*/
/*
   This function computes and/or initializes the 8-bit checksum of a packet.  

      \begin{verbatim}
      input:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

      output:

         crc1.....If this pointer is non-NULL then crc1 is initialized with
                  the checksum of the data buffer.

         crc2.....If this pointer is non-NULL then crc2 is initialized with
                  zero.  
         
         This function returns a positive number if the computed checksum
         matches the checksum stored in the packet.  If the computed
         checksum does not match the packet-checksum then zero is returned.
         A negative return value indicates a NULL pointer to the packet.
                  
      \end{verbatim}

   written by Dana Swift
*/
int checksum(struct Packet *pkt,unsigned char *crc1,unsigned char *crc2)
{
   /* define the logging signature */
   static cc FuncName[] = "checksum()";

   /* initialize the function's return value */
   int status=NullArg;

   /* check for a NULL pointer to the packet */
   if (!pkt) 
   {
      /* create the message */
      static cc msg[]="Null Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else if (BufSize(pkt)>0)
   {
      unsigned char checksum;

      /* get the buffer size */
      int i,bufsize=BufSize(pkt);
      
      /* compute the checksum of the data buffer */
      for (checksum=0,i=0; i<bufsize; i++)
      {
         checksum += pkt->data[i];
      }

      /* determine if the computed checksum matches the packet's checksum */
      status = (checksum==pkt->crc1) ? 1 : 0; 

      /* initialize the checksum value */
      if (crc1) *crc1=checksum;

      /* initialize the unused CRC byte to zero */
      if (crc2) *crc2=0;
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* compute the 16-bit CRC of an xmodem packet and test for corruption     */
/*------------------------------------------------------------------------*/
/*
   This function computes and/or initializes the 16-bit CRC of a packet.  

      \begin{verbatim}
      input:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

      output:

         crc1.....If this pointer is non-NULL then crc1 is initialized with
                  the MSB of the 16-bit CRC of the data buffer.

         crc2.....If this pointer is non-NULL then crc2 is initialized with
                  the LSB of the 16-bit CRC of the data buffer.
         
         This function returns a positive number if the computed CRC matches
         the CRC stored in the packet.  If the computed CRC does not match
         the packet-CRC then zero is returned.  A negative return value
         indicates a NULL pointer to the packet.
                  
      \end{verbatim}

   written by Dana Swift
*/
int crc16bit(struct Packet *pkt,unsigned char *crc1,unsigned char *crc2)
{
   /* define the logging signature */
   static cc FuncName[] = "crc16bit()";

   int status=NullArg;

   /* check for a NULL pointer to the packet */
   if (!pkt) 
   {
      /* create the message */
      static cc msg[]="Null Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else if (BufSize(pkt)>0)
   {
      /* compute the 16-bit CRC of the data packet */
      unsigned int crc = Crc16Bit(pkt->data,BufSize(pkt));

      /* break the 16-bit CRC into its MSB and LSB */
      unsigned char msb=crc/256, lsb=crc%256;

      /* determine if the computed CRC matches the packet's CRC */
      status = (pkt->crc1==msb && pkt->crc2==lsb) ? 1 : 0;
      
      /* initialize the msb of the 16-bit CRC */
      if (crc1) *crc1 = msb;

      /* initialize the lsb of the 16-bit CRC */
      if (crc2) *crc2 = lsb;
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to write the current packet to the log stream                 */
/*------------------------------------------------------------------------*/
/*
   This function writes the current xmodem packet to the log stream.
   
      \begin{verbatim}
      input:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

      output:

         On success, this function returns the number of bytes written to
         the log stream.  On failure, this function returns a negative number.

     \end{verbatim}

   written by Dana Swift
*/
int LogPacket(struct Packet *pkt)
{
   /* define the logging signature */
   static cc FuncName[] = "LogPacket()";

   int n=NullArg;

   /* check for a NULL pointer to the packet */
   if (!pkt)
   {
      /* create the message */
      static cc msg[]="Null Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      /* initialize the buffersize */
      int i,bufsize=BufSize(pkt); 

      /* create the message */
      static cc format[]="0x%02x 0x%02x 0x%02x  [";

      /* initialize the function's return value */
      n=0; 

      /* write the start-byte and packet numbers to the log stream */
      n+=LogEntry(FuncName,format,pkt->StartByte,pkt->PktNum,pkt->PktNum1C);

      /* write the data buffer to the log stream */
      for (i=0; i<bufsize && debuglevel>=5; i++)
      {
         /* write printable bytes as characters */
         if (isprint(pkt->data[i])) n+=LogAdd("%c",pkt->data[i]);

         /* write nonprintable bytes in hex */
         else n+=LogAdd("[0x%02x]",pkt->data[i]);
      }

      /* write the CRC bytes */
      n+=LogAdd("]   0x%02x 0x%02x\n", pkt->crc1, pkt->crc2); 
   }
   
   return n;
}

/*------------------------------------------------------------------------*/
/* function to detect false CAN packet                                    */
/*------------------------------------------------------------------------*/
/*
   This function determines whether a CAN packet is an actual request to
   abort the session or if corruption caused a false CAN to be received.

      \begin{verbatim} 
      input: 
 
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.

      output:

         This function returns a positive value if an actual CAN is
         detected.  Otherwise, a zero or negative value is returned.  The
         actual return is one of the following tokens.
         
            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The time-out period expired without receiving a packet.
            CanPkt:   A request to cancel the current session was detected.

      \end{verbatim}
      
   written by Dana Swift
*/
int CheckCanPkt(const struct SerialPort *port)
{
   /* define the logging signature */
   static cc FuncName[] = "CheckCanPkt()";

   int status=NullArg;
 
   /* validate the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      unsigned char byte;
      int i,ncan;
      
      for (ncan=0, i=1; i<MAXBUFSIZE+5; i++)
      {
         if (pgetb(port,&byte,ByteTimeout)<=0) break;
         if (byte==CAN) ncan++;
      }

      /* initialize the function's return value */
      status = (ncan<3) ? NoPkt : CanPkt;
   }
   
   return status;
}
