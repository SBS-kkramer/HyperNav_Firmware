#ifndef TX_H
#define TX_H

#include <stdio.h>
#include "serial.h"

/* prototypes for external functions */
long int Tx(const struct SerialPort *port,FILE *source);

#endif /* TX_H */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/tx.c#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: tx.c,v $
 * Revision 1.2  2005/02/22 21:45:15  swift
 * Fixed a bug caused by changes made to speed up the code.
 *
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.5  2003/08/13 21:52:12  swift
 * Fixed typos in embedded TeX commands.
 *
 * Revision 1.4  2002/10/26 17:09:15  swift
 * Modified logging constraints to reduce the size of the logfile.
 *
 * Revision 1.3  2002/10/11 13:54:37  swift
 * Added a check for loss of carrier to the default case of the switch statement
 * that manages packet receiver status.
 *
 * Revision 1.2  2002/10/09 00:01:17  swift
 * Modifications to status tracking.
 *
 * Revision 1.1  2002/05/07 22:19:30  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define TxChangeLog "$RCSfile: tx.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include <assert.h>
#include <ctype.h>
#include "xmodem.h"
#include "pkt.h"

/* local function prototypes */
static int GetReceiverResponse(const struct SerialPort *port, time_t timeout);
static int TxPacket(const struct SerialPort *port, struct Packet *pkt);
static int NegotiateCrcMode(const struct SerialPort *port,time_t timeout);

static int CrcMode='C';

/*========================================================================*/
/* function to transmit a file via the xmodem protocol                    */
/*========================================================================*/
/**
   This function is designed to transmit a file via the xmodem protocol.
   Actually, four variants of the xmodem protocol are implemented:

      \begin{verbatim}
      1. Xmodem with checksum and 128 byte blocks.
      2. Xmodem with checksum and 1024 byte blocks.
      3. Xmodem with 16-bit CRC and 128 byte blocks.
      4. Xmodem with 16-bit CRC and 1024 byte blocks.
      \end{verbatim}
   
   The send and receive modules of the xmodem session negotiate which of
   these four variants will be used.  The file will be read from a file and
   written to a serial port.  This implementation is based on 3 sources of
   information about the xmodem protocol.  The primarily source is a
   document entitled:

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

      \begin{verbatim}
      input:
        
         source...The output stream from which the xmodem packets are read.
                
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.
                  
       output:

         On success, this function returns the number of bytes written to
         the serial port.  If the transfer fails or is terminated
         abnormally, this function returns -1.

      \end{verbatim}

   written by Dana Swift
*/
long int Tx(const struct SerialPort *port,FILE *source)
{
   /* define the logging signature */
   static cc FuncName[] = "Tx()";

   long int status=0, bytes_transmitted=-1;
  
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

   /* validate the FILE pointer */
   else if (source && !ferror(source) && !feof(source)) 
   {
      struct Packet pkt;
      long int n,bufsize,NPkt,pktbytes;
      time_t T,To=time(NULL),Tstart=To;
      float dT,bps;

      /* flush the Rx buffer */
      if (pflushrx(port)<=0)
      {
         /* create the message */
         static cc msg[]="Attempt to flush Rx buffer failed.\n";

         /* make the logentry */
         LogEntry(FuncName,msg);
      }
      
      switch ((status=NegotiateCrcMode(port,EotTimeout)))
      {
         /* CRC negotiation was successful */
         case OkPkt: 
         {
            /* create the message */
            static cc format[]="CRC negotiation successful. [%s]\n";

            /* make the logentry */
            LogEntry(FuncName,format,(CrcMode=='C')?"16-bit CRC":"checksum");
            
            break;
         }

         /* request to cancel the session was detected */
         case CanPkt:
         {
            /* create the message */
            static cc msg[]="Receiver cancelled transfer - aborting transfer.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);

            goto abort;
         }

         /* CRC negotiation failed */
         default:
         {
            /* create the message */
            static cc msg[]="CRC negotiation failed - aborting transfer.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);

            goto abort;
         }
      }

      /* initialize some statistics metrics */
      NPkt=0,pktbytes=0,bytes_transmitted=0; Tstart=time(NULL);
      
      /* packetize and transmit the stream */
      for (n=1,pkt.PktNum=1; n>0 && !feof(source); pkt.PktNum++)
      {
         /* make sure that the total session time stays acceptable */
         if (MaxSessionTime>0 && difftime(time(NULL),To)>MaxSessionTime)
         {
            /* create the message */
            static cc format[]="Transfer session has exceeded allowed "
               "time (%ld seconds) - truncating transfer.\n";

            /* make the logentry */
            LogEntry(FuncName,format,MaxSessionTime);

            /* indicate failure */
            status=SessionTimeOut; break;
         }

         /* reinitialize fault status */
         else status=NullArg;
         
         /* initialize the packet type */
         pkt.StartByte=PktType();
            
         /* compute the 1's complement of the packet number */
         pkt.PktNum1C = ~pkt.PktNum;

         /* get the buffer size */
         bufsize=BufSize(&pkt);
         
         /* read the next data packet from the source stream */
         if ((n=fread(pkt.data,1,bufsize,source))<bufsize && n>0)
         {
            /* pad the incomplete data packet with the PAD character */
            if (!ferror(source)) {memset(pkt.data+n,PAD,bufsize-n);}

            /* log the stream error */
            else
            {
               /* create the message */
               static cc msg[]="Stream error.\n";

               /* make the logentry */
               LogEntry(FuncName,msg);

               goto abort;
            }
         }

         /* check for errors on the source stream */
         else if (ferror(source))
         {
            /* create the message */
            static cc msg[]="Error reading from source stream - aborting transfer.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);
            
            goto abort;
         }
         
         if (n>0)
         {
            /* transmit the packet and get the receiver response */            
            switch ((status=TxPacket(port,&pkt)))
            {
               /* accumlate the bytes and packets transmitted */ 
               case OkPkt: {bytes_transmitted+=n; pktbytes+=bufsize; NPkt++; break;}

               /* receiver requests to cancel the transfer */
               case CanPkt:
               {
                  /* create the message */
                  static cc msg[]="Receiver cancelled transfer - aborting transfer.\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);

                  goto abort;
               }

               /* transmit failed */
               default:
               {
                  /* create the message */
                  static cc msg[]="Packet transmission failed - aborting transfer.\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);

                  goto abort;
               }
            }
         }
         else if (debuglevel>=3)
         {
            /* create the message */
            static cc msg[]="No bytes read from source stream.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);
         }
      }

      /* check for zero-length file */
      if (bytes_transmitted<=0) 
      {
         /* create the message */
         static cc msg[]="No bytes transferred - aborting.\n";

         /* make the logentry */
         LogEntry(FuncName,msg);

         /* indicate failure */
         status=NoPkt; goto abort;
      }
      
      /* notify the receiver of EOT - use regular packet timeout */
      if (port->putb(EOT)>0 && GetReceiverResponse(port,PacketTimeout)!=ACK)
      {
         /* try a second notification - use EOT timeout */
         if (port->putb(EOT)>0 && GetReceiverResponse(port,EotTimeout)!=ACK)
         {
            /* create the message */
            static cc msg[]="Receiver failed to acknowledge EOT.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);
         }
      }

      /* get the current time */
      T=time(NULL);

      /* compute the total transfer time */
      dT=(T>0 && Tstart>0) ? difftime(T,Tstart) : 0;

      /* compute the net transfer rate */
      bps = (dT>0) ? pktbytes/dT : 0;
      
      /* create the message */
      {static cc format[]="Transmission completed successfully "
            "[%ld packets, %ld bytes, %0.0f sec, %0.1f bps]\n";

      /* write the transfer statistics to the output */
      LogEntry(FuncName,format,NPkt,bytes_transmitted,dT,bps);}
      
   }
   
   /* invalid FILE pointer - make a log entry */
   else
   {
      /* create the message */
      static cc msg[]="Source stream not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }
   
   return bytes_transmitted;

   /* catch the abort request */
   abort: abort_transfer(port);
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to negotiate the corruption-detection mode                    */
/*------------------------------------------------------------------------*/
/*
   This function is designed to negotiate with the receiver in order to
   determine whether a 16-bit CRC or an 8-bit checksum is used to detect
   corrupt packets.  Receipt of a 'C' from the serial port indicates that a
   16-bit CRC will be used.  Receipt of NAK selects for a checksum to be
   used.

      \begin{verbatim}
      input:
 
         port......A structure that contains pointers to machine dependent
                   primitive IO functions.  See the comment section of the
                   SerialPort structure for details.  The function checks
                   to be sure this pointer is not NULL.

         timeout...The maximum amount of time (seconds) that this function
                   will attempt to negotiate before terminating.
         
      output:

         This return value of this function indicates the status of the
         negotiation according to the following key.  See xmodem.h for the
         values of the following tokens.  

            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The time-out period expired without receiving a packet.
            OkPkt:    The packet was successfully sent.
            CanPkt:   A request to cancel the current session was detected.

      \end{verbatim}

   written by Dana Swift
*/
static int NegotiateCrcMode(const struct SerialPort *port,time_t timeout)
{
   /* define the logging signature */
   static cc FuncName[] = "NegotiateCrcMode()";

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

   /* validate the serial port's pgetb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL pgetb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      /* xmodem protocol calls for termination after receiving too many bad bytes */
      const int MaxBadBytes=1029;

      /* initialize the time */
      time_t T,dT=0,To=time(NULL);
      unsigned char byte;
      int i,j;

      /* read bytes from the serial port and check termination conditions */
      for (status=NoPkt, j=0, i=0; status<=0 && j<MaxBadBytes && i<MaxBadBytes && dT<timeout; j++)
      {
         /* read the next byte from the serial port */
         if (pgetb(port,&byte,ByteTimeout)>0)
         {
            switch (byte)
            {
               /* 16-bit CRC mode */
               case 'C': {CrcMode='C'; status=OkPkt; break;}

               /* 8-bit checksum mode */
               case NAK: {CrcMode=NAK; status=OkPkt; break;}

               /* request to cancel session */
               case CAN:
               {
                  if (CheckCanPkt(port)==CanPkt) {status=CanPkt;} else {i++;}
                  break;
               }

               /* count and discard bad bytes */
               default:
               {
                  if (debuglevel>=3)
                  {
                     if (!i)
                     {
                        /* create the message */
                        static cc msg[]="Discarding ";

                        /* make the logentry */
                        LogEntry(FuncName,msg);
                     }
                     
                     if (byte=='\r') LogAdd("\\r");
                     else if (byte=='\n') LogAdd("\\n");
                     else if (isprint(byte)) LogAdd("%c",byte);
                     else LogAdd("[0x%02x]",byte);
                  }
                  
                  i++;
               }
            }
         }
         
         /* check if timeout has expired */
         T=time(NULL); dT=(T>0 && To>0) ? (time_t)difftime(T,To) : 0;
      }

      /* terminate the last logentry */
      if (debuglevel>=3 && i>0) LogAdd("\n");
   }
   
   /* log the function's return value */
   if (debuglevel>=3)
   {
      /* create the message */
      static cc format[]="Exit status: %d\n";

      /* make the logentry */
      LogEntry(FuncName,format,status);
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to read the receiver's response from the serial port          */
/*------------------------------------------------------------------------*/
/*
   This function reads the receiver's response from the serial port.  

      \begin{verbatim}
      input:
 
         port......A structure that contains pointers to machine dependent
                   primitive IO functions.  See the comment section of the
                   SerialPort structure for details.  The function checks
                   to be sure this pointer is not NULL.

         timeout...The maximum amount of time (seconds) that this function
                   will wait before returning.
         
      output:

         On success, this function returns the ascii value of the byte read
         from the serial port.  On failure, the function returns a negative
         number according to the following key.  See xmodem.h for the
         values of the following tokens.  

            NullArg:  A NULL pointer was detected in one of the function parameters.

      \end{verbatim}

   written by Dana Swift
*/
static int GetReceiverResponse(const struct SerialPort *port, time_t timeout)
{  
   /* define the logging signature */
   static cc FuncName[] = "GetReceiverResponse()";

   /* initialize the function's return value */
   int response=NullArg;
  
   /* validate the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pgetb() function */
   else if (!port->getb) 
   {
      /* create the message */
      static cc msg[]="NULL pgetb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      unsigned char byte;

      /* read a byte from the port */
      int status = pgetb(port,&byte,timeout);

      /* determine the return value of this function */
      response = (status>0) ? byte : NullArg;
   }

   /* check logging constraints */
   if (debuglevel>=3 || (debuglevel>=2 && response!=ACK))
   {
      /* create the message */
      static cc format[]="Return value: %d\n";

      /* log the function's return value */      
      LogEntry(FuncName,format,response);
   }
   
   return response;
}

/*------------------------------------------------------------------------*/
/* function to send a single packet out the serial port                   */
/*------------------------------------------------------------------------*/
/*
   This function sends a single packet using the xmodem protocol.

      \begin{verbatim}
      input:

         pkt......A structure where the packet is stored.  See the comment
                  section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.
         
      output:

         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.

         This return value of this function indicates the status of the sent
         packet according to the following key.  See xmodem.h for the values
         of the following tokens.  Non-positive values indicate that the
         packet should be retried.

            WrongPkt: The received packet does not the expected packet number.
            ShortPkt: The length of the packet was shorter than expected.
            BadPkt:   Corruption was detected in the packet.
            NullArg:  A NULL pointer was detected in one of the function
                         parameters or no response received from receiver.
            NoPkt:    The time-out period expired without receiving a packet.
            OkPkt:    The packet was successfully sent.
            CanPkt:   A request to cancel the current session was detected.
            
      \end{verbatim}
      
   written by Dana Swift
*/
static int TxPacket(const struct SerialPort *port,struct Packet *pkt)
{
   /* define the logging signature */
   static cc FuncName[] = "TxPacket()";

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

   /* validate the serial port's pgetb() function */
   else if (!port->getb) 
   {
      /* create the message */
      static cc msg[]="NULL pgetb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pputb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL pgetb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      int n=0;
      
      /* compute the 16-bit CRC of the packet data */
      if (CrcMode=='C' && crc16bit(pkt,&pkt->crc1,&pkt->crc2)<0)
      {
         /* create the message */
         static cc msg[]="Attempt to initialize packet CRC failed.\n";

         /* make the logentry */
         LogEntry(FuncName,msg);
      }

      /* compute the 8-bit CRC of the packet data */
      else if (CrcMode==NAK && checksum(pkt,&pkt->crc1,&pkt->crc2)<0)
      {
         /* create the message */
         static cc msg[]="Attempt to initialize packet checksum failed.\n";

         /* make the logentry */
         LogEntry(FuncName,msg);
      }

      else
      {
         int response;
         
         /* loop over each retry attempt */
         for (n=0; status<=0 && n<NRetry; n++)
         {
            /* flush IO-queues before transmitting the packet */
            if (pflushio(port)<=0)
            {
               /* create the message */
               static cc msg[]="Attempt to flush IO buffers failed.\n";

               /* make the logentry */
               LogEntry(FuncName,msg);
            }

            /* enter the packet in the log */
            if (debuglevel>=4 || (debuglevel>=2 && n>0)) LogPacket(pkt);
            
            /* write the start-byte to the serial port */
            if (pputb(port,pkt->StartByte,ByteTimeout)<=0) {status=ShortPkt;}

            /* write the packet number to the serial port */
            else if (pputb(port,pkt->PktNum,ByteTimeout)<=0) {status=ShortPkt;}

            /* write the 1's complement of the packet number to the serial port */
            else if (pputb(port,pkt->PktNum1C,ByteTimeout)<=0) {status=ShortPkt;}

            /* write the data packet to the serial port */
            else if (pputbuf(port,pkt->data,BufSize(pkt),PacketTimeout)!=BufSize(pkt)) {status=ShortPkt;}

            /* write the 1st crc byte */
            else if (pputb(port,pkt->crc1,ByteTimeout)<=0) {status=ShortPkt;}

            /* if 16-bit crc mode, write the 2nd crc byte */
            else if (CrcMode=='C' && pputb(port,pkt->crc2,ByteTimeout)<=0) {status=ShortPkt;}
            
            /* make a log entry if the packet transmission was terminated abnormally */
            if (status==ShortPkt)
            {
               /* create the message */
               static cc format[]="Packet [0x%02x] was truncated during send operations.\n";

               /* make the logentry */
               LogEntry(FuncName,format,pkt->PktNum);
            }
            
            /* get the packet response from the sender */
            switch ((response=GetReceiverResponse(port,EotTimeout)))
            {
               /* packet was ACK'd */
               case ACK: {status=OkPkt; break;}

               /* No response was detected from receiver */
               case NullArg:
               {
                  /* check if carrier-dectect enabled and CD line not asserted */
                  if (port->cd && !port->cd())
                  {
                     /* create the message */
                     static cc msg[]="No carrier detected.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                   
                     /* indicate failure */
                     status=NoPkt; goto abort;
                  }

                  /* create the message */
                  {static cc format[]="Timeout - resending packet. [PktNum=0x%02x]\n";

                  /* log the timeout and try again */
                  LogEntry(FuncName,format,pkt->PktNum);}

                  /* define the error code */
                  status=NoPkt; break;
               }

               /* CAN received - verify it */
               case CAN: 
               {
                  /* check for spurious CAN packet */
                  if (CheckCanPkt(port)!=CanPkt)
                  {
                     /* create the message */
                     static cc format[]="False CAN detected - resending packet. [PktNum=0x%02x]\n";

                     /* make the logentry */
                     LogEntry(FuncName,format,pkt->PktNum);

                     /* define the error code */
                     status=BadPkt;
                  }

                  /* define the error code */
                  else {status=CanPkt;}

                  break;
               }

               /* packet was NAK'd */
               case NAK:
               {
                  /* create the message */
                  static cc format[]="NAK received - resending packet. [PktNum=0x%02x]\n";

                  /* make the logentry */
                  LogEntry(FuncName,format,pkt->PktNum);

                  /* define the error code */
                  status=BadPkt; break;
               }

               /* garbled response */
               default:
               {
                  /* define the error code */
                  status=WrongPkt;
                  
                  /* check if carrier-dectect enabled and CD line not asserted */
                  if (port->cd && !port->cd())
                  {
                     /* create the message */
                     static cc msg[]="No carrier detected.\n";

                     /* log the carrier drop and bail out */
                     LogEntry(FuncName,msg);

                     goto abort;
                  }

                  /* log that an invalid packet was received and then retry */
                  else
                  {
                     /* create the message */
                     static cc format[]="Invalid response [0x%02x] - resending packet. [PktNum=0x%02x]\n";

                     /* make the logentry */
                     LogEntry(FuncName,format,response,pkt->PktNum);
                  }
               }
            }

            /* monitor the general quality of the connection */
            response=PktMonitor((status==OkPkt || status==CanPkt) ? OkPkt : BadPkt);

            /* check for verbose logging */
            if (debuglevel>=4 || (debuglevel>=2 && status<OkPkt))
            {
               int i,mask;

               /* create the message */
               static cc msg[]="History of packet transmission failures: [";

               /* start a log entry of the history of packet errors */
               LogEntry(FuncName,msg);

               /* log the status of the past 8 transmission attempts */
               for (mask=0x80,i=0; i<8; i++,mask>>=1) {LogAdd("%d",(response&mask)?1:0);} 

               /* terminate the log entry */
               LogAdd("]  (0:xmit-ok, 1:xmit-failed).\n");
            }
         }
      }
   }

   /* collection point for aborted transfers */
   abort: 

   /* make a log entry of the exist status of the packet transfer */
   if (debuglevel>=3 || (debuglevel>=2 && status<OkPkt))
   {
      /* create the message */
      static cc format[]="Exit status: %d\n";
      
      /* make the logentry */
      LogEntry(FuncName,format,status);
   }
   
   return status;
}
