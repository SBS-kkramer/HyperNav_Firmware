#ifndef RX_H
#define RX_H

#include <stdio.h>
#include "serial.h"

/* prototypes for external functions */
long int Rx(const struct SerialPort *port,FILE *dest);

extern int CrcMode;
extern int BinMode;

#endif /* RX_H */

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/rx.c#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: rx.c,v $
 * Revision 1.2  2005/02/22 21:45:15  swift
 * Fixed a bug caused by changes made to speed up the code.
 *
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.4  2003/08/13 21:52:12  swift
 * Fixed typos in embedded TeX commands.
 *
 * Revision 1.3  2002/10/26 17:14:37  swift
 * Modified logging constraints to reduce the size of the logfile.
 *
 * Revision 1.2  2002/10/08 23:55:50  swift
 * Modifications to use the CD signal of the serial port.
 *
 * Revision 1.1  2002/05/07 22:18:00  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define RxChangeLog "$RCSfile: rx.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include <ctype.h>
#include "xmodem.h"

/* prototypes for local functions */
static int CheckEotPkt(const struct SerialPort *port,struct Packet *pkt,unsigned char PktNum);
static int RxPacket(const struct SerialPort *port,struct Packet *pkt,unsigned char PktNum);
static int RxStartByte(const struct SerialPort *port,struct Packet *pkt);

/* specify 16-bit CRC as the default corruption detection method */
int CrcMode='C';

/* specify the transfer mode (binary or ascii) to be binary by default */
int BinMode=1;

/*========================================================================*/
/* function to receive a file via the xmodem protocol                     */
/*========================================================================*/
/**
   This function is designed to receive a file via the xmodem protocol.
   Actually, four variants of the xmodem protocol are implemented:

      \begin{verbatim}
      1. Xmodem with checksum and 128 byte blocks.
      2. Xmodem with checksum and 1024 byte blocks.
      3. Xmodem with 16-bit CRC and 128 byte blocks.
      4. Xmodem with 16-bit CRC and 1024 byte blocks.
      \end{verbatim}
   
   The send and receive modules of the xmodem session negotiate which of
   these four variants will be used.  The file will be read from the serial
   port and written to a file.  This implementation is based on 3 sources of
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
 
         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.
                 
      output:
        
         dest.....The output stream to which the xmodem packets are written.

         On success, this function returns the number of bytes read from the
         serial port.  If the transfer fails or is terminated abnormally,
         this function returns -1.

      \end{verbatim}

   written by Dana Swift
*/
long int Rx(const struct SerialPort *port,FILE *dest)
{
   /* define the logging signature */
   static cc FuncName[] = "Rx()";

   long int bytes_read=-1;
  
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
      static cc msg[]="NULL pputb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the FILE pointer */
   else if (!dest || ferror(dest))
   {
      /* create the message */
      static cc msg[]="Output stream not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      struct Packet pkt;
      unsigned char PktNum;
      unsigned long int NPkt,pktbytes;
      unsigned int try;
      time_t To=time(NULL),Tstart=To;
      int pkt_status;

      /* start with clean IO buffers */
      if (pflushio(port)<=0)
      {
         /* create the message */
         static cc msg[]="Attempt to flush IO buffers failed.\n";

         /* make the logentry */
         LogEntry(FuncName,msg);
      }
      
      /* initiate the transfer with the START byte */
      if (port->putb(CrcMode)>0)
      {
         if (debuglevel>=1)
         {
            /* create the message */
            static cc format[]="Initiating transfer. [0x%02x]\n";

            /* make the logentry */
            LogEntry(FuncName,format,CrcMode);
         }
      }
      else
      {
         /* create the message */
         static cc format[]="Attempt to initiate transfer failed. [0x%02x]\n";

         /* make the logentry */
         LogEntry(FuncName,format,CrcMode);
      }
      
      /* loop through each packet */
      for (pktbytes=0,bytes_read=0,NPkt=0,PktNum=1; ;PktNum++,NPkt++)
      {
         /* make sure that the total session time stays acceptable */
         if (MaxSessionTime>0 && difftime(time(NULL),To)>MaxSessionTime)
         {
            /* create the message */
            static cc format[]="Transfer session has exceeded allowed "
               "time (%ld seconds) - aborting.\n";
            
            /* make the logentry */
            LogEntry(FuncName,format,MaxSessionTime);
            
            goto abort;
         }
         
         /* loop through each retry attempt */
         for (pkt_status=NoPkt, try=1; pkt_status<=NoPkt && try<=NRetry; try++)
         {
            /* initialize the packet data */
            memset(pkt.data,0,MAXBUFSIZE);

            /* initialize other members of the Packet structure */
            pkt.StartByte=pkt.PktNum=pkt.PktNum1C=pkt.crc1=pkt.crc2=0;

            /* attempt to read the next packet from the serial port */
            pkt_status=RxPacket(port,&pkt,PktNum);

            /* use the packet status to determine what to do next */
            switch (pkt_status)
            {
               /* A false Eot was received */
               case FalseEot:
               {
                  if (debuglevel>=2)
                  {
                     /* create the message */
                     static cc msg[]="False EOT packet detected - ignoring.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
                  
                  /* request resend */
                  if (port->putb(NAK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
    
                  break;
               }
               
               /* Fatal sequence error detected */
               case WrongPkt: 
               {
                  /* create the message */
                  static cc msg[]="Packet sync error - aborting transfer.\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);

                  goto abort;
               }
 
               /* The non-corrupt copy of the previous packet was received */
               case PrevPkt: 
               {
                  if (debuglevel>=2)
                  {
                     /* create the message */
                     static cc msg[]="Previous packet received again - resyncing.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
                  
                  /* resync by ACKing the packet */
                  if (port->putb(ACK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }
               
               /* A truncated packet was received */
               case ShortPkt: 
               {
                  if (debuglevel>=2)
                  {
                     /* create the message */
                     static cc msg[]="Truncated packet received - retrying.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
                  
                 /* request resend */
                  if (port->putb(NAK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }
               
               /* A corrupt packet was received */
               case BadPkt: 
               {
                  if (debuglevel>=2)
                  {
                     /* create the message */
                     static cc msg[]="Corrupt packet received - retrying.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
                  
                  /* request resend */
                  if (port->putb(NAK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }
               
               /* One of RxPacket's function arguments was NULL */
               case NullArg: 
               {
                  /* create the message */
                  static cc msg[]="Bad function parameter found - aborting transfer .\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);

                  goto abort;
               }

               /* No packet was received */
               case NoPkt: 
               {
                  /* check if carrier-dectect enabled and CD line not asserted */
                  if (port->cd && !port->cd())
                  {
                     /* create the message */
                     static cc msg[]="No carrier detected.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);

                     goto abort;
                  }
                  
                  /* check to see if xfer-init byte should be resent */
                  if (PktNum==1)
                  {
                     /* mark the new transfer start time */
                     Tstart=time(NULL);
                     
                     /* switch from 16-bit CRC to checksum after a few tries */
                     if (try>3)
                     {
                        if (debuglevel>=2)
                        {
                           /* create the message */
                           static cc msg[]="Switching from 16-bit CRC to checksum.\n";

                           /* make the logentry */
                           LogEntry(FuncName,msg);
                        }
                        
                        CrcMode=NAK;
                     }
                     
                     /* re-initiate the transfer with the START byte */
                     if (port->putb(CrcMode)>0)
                     {
                        if (debuglevel>=2)
                        {
                           /* create the message */
                           static cc format[]="Re-initiating transfer. [0x%02x]\n";

                           /* make the logentry */
                           LogEntry(FuncName,format,CrcMode);
                        }
                     }
                     else
                     {
                        /* create the message */
                        static cc format[]="Attempt to re-initiate transfer failed. [0x%02x]\n";

                        /* make the logentry */
                        LogEntry(FuncName,format,CrcMode);
                     }
                  }
          
                  /* missing packet - request resend */
                  else if (port->putb(NAK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }
                  else if (debuglevel>=2)
                  {
                     /* create the message */
                     static cc msg[]="Packet timeout - retrying.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }

               /* A good packet was received */
               case OkPkt:
               {
                  /* get the size of the packet */
                  int i,bufsize=BufSize(&pkt);
                  
                  /* check if binary mode is used */
                  if (BinMode)
                  {
                     /* write the data to the destination file */
                     if (fwrite(pkt.data,1,bufsize,dest)!=bufsize)
                     {
                        /* create the message */
                        static cc msg[]="Write to FILE object failed - aborting transfer.\n";

                        /* make the logentry */
                        LogEntry(FuncName,msg);

                        goto abort;
                     }
                  }

                  /* ascii mode used - write packet to destination file */
                  else for (i=0; i<bufsize; i++)
                  {
                     /* truncate the packet after a PAD byte */
                     if (pkt.data[i]==PAD)
                     {
                        if (debuglevel>=2)
                        {
                           /* create the message */
                           static cc format[]="Pad character [0x%02x] found "
                              "in ascii mode - truncating packet.\n";

                           /* make the logentry */
                           LogEntry(FuncName,format,PAD);
                        }
                        
                        break;
                     }

                     /* discard carriage returns */
                     else if (pkt.data[i]=='\r') continue;

                     /* write line feeds to the output stream */
                     else if (pkt.data[i]=='\n') {fputc(pkt.data[i],dest);}
                     
                     /* write printable bytes to the output stream */
                     else if (isprint(pkt.data[i])) {fputc(pkt.data[i],dest);}

                     /* identify hex-values of non-printable bytes */
                     else {fprintf(dest,"[0x%02x]",pkt.data[i]);}
                  }
                     
                  /* ACK the good packet */
                  if (port->putb(ACK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }

               /* end-of-transmission was received */
               case EotPkt:
               {
                  /* get the current time */
                  time_t T=time(NULL);

                  /* compute the total transfer time */
                  float dT=(T>0 && Tstart>0) ? difftime(T,Tstart) : 0;

                  /* compute the net transfer rate */
                  float bps = (dT>0) ? pktbytes/dT : 0;

                  /* write the transfer statistics to the output */
                  if (debuglevel>=1)
                  {
                     /* create the message */
                     static cc format[]="Received EOT - transfer complete. "
                        "[%ld packets, %ld bytes, %0.0f sec, %0.1f bps]\n";
                     
                     /* make the logentry */
                     LogEntry(FuncName,format,NPkt,bytes_read,dT,bps);
                  }
                  
                  /* ACK the EOT packet */
                  if (port->putb(ACK)<=0)
                  {
                     /* create the message */
                     static cc msg[]="Attempted ACK/NAK response failed.\n";

                     /* make the logentry */
                     LogEntry(FuncName,msg);
                  }

                  break;
               }
               
               /* The sender sent a request to cancel the transfer */
               case CanPkt:
               {
                  /* create the message */
                  static cc msg[]="Sender cancelled transfer - aborting.\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);

                  goto abort;
               }
            }
             
            /* maintain a log of diagnostics */
            if (debuglevel>=4 || (debuglevel>=2 && pkt_status<OkPkt)) LogPacket(&pkt);
         }

         /* check if packet transfer failed */
         if (pkt_status<OkPkt)
         {
            /* create the message */
            static cc format[]="Attempt to read packet %ld failed.  Transfer aborted.\n";

            /* make the logentry */
            LogEntry(FuncName,format,NPkt+1);
            goto abort;
         }

         /* terminate normally after EOT received */
         else if (pkt_status==EotPkt) {break;}

         /* accumulate the number of bytes read */
         else {bytes_read+=BufSize(&pkt); pktbytes+=BufSize(&pkt);}      
      }
   }

   return bytes_read;
   
   /* catch the abort request */
   abort: abort_transfer(port);
   
   return -1;
}

/*------------------------------------------------------------------------*/
/* function to receive a single xmodem packet from the serial port        */
/*------------------------------------------------------------------------*/
/*
   This function receives a single packet using the xmodem protocol.

      \begin{verbatim}
      input:
 
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.

         PktNum...The expected packet number as defined by the xmodem
                  protocol.  The packet that is received is checked against
                  the expected packet number in order to guard against
                  corruption and maintain synchrony between the send and
                  receive components of the transfer.
         
      output:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

         This return value of this function indicates the status of the
         received packet according to the following key.  See xmodem.h for
         the values of the following tokens.  Non-positive values indicate
         that the packet should be retried.

            FalseEot: A false end-of-transmission (EOT) packet was detected.
            WrongPkt: The received packet does not the expected packet number.
            PrevPkt:  Another copy of the previous packet was received.
            ShortPkt: The length of the packet was shorter than expected.
            BadPkt:   Corruption was detected in the packet.
            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The time-out period expired without receiving a packet.
            OkPkt:    The packet was successfully received.
            EotPkt:   An end-of-transmission (EOT) packet was detected.
            CanPkt:   A request to cancel the current session was detected.
            
      \end{verbatim}
      
   written by Dana Swift
*/
static int RxPacket(const struct SerialPort *port,struct Packet *pkt,unsigned char PktNum)
{
   /* define the logging signature */
   static cc FuncName[] = "RxPacket()";

   /* initialize the function's return value */
   int status=NullArg;

   /* validate the serial port */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the packet */
   else if (!pkt)
   {
      /* create the message */
      static cc msg[]="NULL Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* seek the start-byte of the next packet */
   else if ((status=RxStartByte(port,pkt))>0 && status!=EotPkt && status!=CanPkt)
   {
      /* get the buffer size */
      int i,bufsize=BufSize(pkt);
      
      /* read the packet number from the serial port */
      if (pgetb(port,&pkt->PktNum,ByteTimeout)<=0) {status=ShortPkt;}

      /* read the 1's complement of the packet number from the serial port */
      else if (pgetb(port,&pkt->PktNum1C,ByteTimeout)<=0) {status=ShortPkt;}
      
      /* read the packet from the serial port */
      else for (i=0; status>0 && i<bufsize; i++)
      {
         if (pgetb(port,pkt->data+i,ByteTimeout)<=0) {status=ShortPkt;}
      }

      if (status>0)
      {
         /* read the first crc byte from the serial port */
         if (pgetb(port,&pkt->crc1,ByteTimeout)<=0) {status=ShortPkt;}

         /* if 16-bit crc, read 2nd crc byte from the packet number from the serial port */
         else if (CrcMode=='C' && pgetb(port,&pkt->crc2,ByteTimeout)<=0) {status=ShortPkt;}
      
         /* if 16-bit crc, check for data corruption via the CRC */
         else if (CrcMode=='C' && crc16bit(pkt,NULL,NULL)<=0) {status=BadPkt;}
      
         /* if checksum mode, check for data corruption via the checksum */
         else if (CrcMode==NAK && checksum(pkt,NULL,NULL)<=0) {status=BadPkt;}

         /* verify that the packet numbers are 1's complements of each other */
         else if (pkt->PktNum1C != (unsigned char)(~pkt->PktNum)) {status=BadPkt;}

         /* check to see if the previous packet was received */
         else if (pkt->PktNum == PktNum-1) {status=PrevPkt;} 
        
         /* check to see if the expected packet number was received */
         else if (pkt->PktNum != PktNum) {status=WrongPkt;}
      }
   }
   
   /* if EOT then analyze the stream for a possible false EOT */
   else if (status==EotPkt && CheckEotPkt(port,pkt,PktNum)==FalseEot) {status=FalseEot;}
   
   /* if CAN then analyze the stream for a possible false CAN */
   else if (status==CanPkt && CheckCanPkt(port)!=CanPkt) {status=BadPkt;}

   return status;
}

/*------------------------------------------------------------------------*/
/* function to detect a false EOT packet                                  */
/*------------------------------------------------------------------------*/
/*
   This function determines whether an EOT packet is an actual request to
   terminate the session or if corruption caused a false EOT to be received.

      \begin{verbatim}
      input:
 
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.
         
         PktNum...The expected packet number as defined by the xmodem
                  protocol.  The packet that is received is checked against
                  the expected packet number in order to guard against
                  corruption and maintain synchrony between the send and
                  receive components of the transfer.

      output:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

         This function returns a positive value if an actual EOT is
         detected.  Otherwise, a zero or negative value is returned.  The
         actual return is one of the following tokens.
         
            FalseEot: A false end-of-transmission (EOT) packet was detected.
            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The time-out period expired without receiving a packet.
            EotPkt:   An end-of-transmission (EOT) packet was detected.
 
      \end{verbatim}
      
   written by Dana Swift
*/
static int CheckEotPkt(const struct SerialPort *port,struct Packet *pkt,unsigned char PktNum)
{
   /* define the logging signature */
   static cc FuncName[] = "CheckEotPkt()";

   int status=NullArg;
 
   /* validate the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the packet */
   if (!pkt)
   {
      /* create the message */
      static cc msg[]="NULL Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* make sure the packet claims to indicate EOT */
   if (pkt->StartByte!=EOT)
   {
      /* create the message */
      static cc msg[]="Packet doesn't indicate EOT.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   else
   {
      unsigned char byte;
      int i,pktnum=-1, pktnum1c=-1;

      for (i=1; i<MAXBUFSIZE+5; i++)
      {
         if (pgetb(port,&byte,ByteTimeout)<=0) break;
         else if (i==1) pktnum=byte;
         else if (i==2) pktnum1c=byte;
      }

      /* initialize the function's return value */
      status=NoPkt;
                               
      /* EOT indicated if the start-byte is EOT */
      if (i==1 && pkt->StartByte==EOT) status=EotPkt;

      /* false EOT is indicated if the packet number matches the expected one */
      else if (pktnum!=-1 && PktNum==pktnum) {status=FalseEot;}

      /* false EOT is indicated if the packet number matches the expected one */
      else if (pktnum1c!=-1 && (unsigned char)(~PktNum)==pktnum1c) {status=FalseEot;}

      /* false EOT is indicated if the packet numbers match each other */
      else if (pktnum!=-1 && pktnum!=-1 && (unsigned char)(~pktnum)==pktnum1c) {status=FalseEot;}

      /* false EOT indicated if packet length matches SOH or STX packet (with CRC) */
      else if (CrcMode=='C' && (i==133 || i==1029)) {status=FalseEot;}

      /* false EOT indicated if packet length matches SOH or STX packet (with checksum) */
      else if (CrcMode==NAK && (i==132 || i==1028)) {status=FalseEot;}

      /* EOT indicated if the start-byte is EOT */
      else if (pkt->StartByte==EOT) status=EotPkt;
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to find the start-byte of the next xmodem packet              */
/*------------------------------------------------------------------------*/
/*
   This function detects the start of the next packet in the xmodem transfer
   session.  It reads bytes from the serial port and discards them until a
   start-byte of a valid xmodem packet is detected.
   
      \begin{verbatim}
      input:
 
         port.....A structure that contains pointers to machine dependent
                  primitive IO functions.  See the comment section of the
                  SerialPort structure for details.  The function checks to
                  be sure this pointer is not NULL.

      output:

         pkt......A structure where the received packet is stored.  See the
                  comment section of the Packet structure for details.  This
                  function checks to be sure this pointer is not NULL.

         This function returns a positive value if the start-byte of a
         packet was detected.  Otherwise, a zero or negative value is
         returned.  The actual return is one of the following tokens.
         
            NullArg:  A NULL pointer was detected in one of the function parameters.
            NoPkt:    The time-out period expired without receiving a packet.
            OkPkt:    The start-byte of the packet was successfully received.
            EotPkt:   An end-of-transmission (EOT) packet was detected.
            CanPkt:   A request to cancel the current session was detected.

      \end{verbatim}
      
   written by Dana Swift
*/
static int RxStartByte(const struct SerialPort *port,struct Packet *pkt)
{
   /* function name for log entries */
   static cc FuncName[] = "RxStartByte()";
   
   /* initialize the function's return value */
   int status=NullArg;

   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's getb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL getb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the packet */
   else if (!pkt)
   {
      /* create the message */
      static cc msg[]="NULL Packet pointer.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }
   
   else
   {
      unsigned char byte;
      unsigned int sync_err=0;

      /* initialize the return value of this function */
      status=NoPkt;

      /* initialize the start-byte element of the Packet structure */
      pkt->StartByte=0;

      /* count the number of sync-errors before detecting the next packet */
      for (sync_err=0; pgetb(port,&byte,PacketTimeout)>0; sync_err++)
      {
         /* check for an EOT packet */
         if (byte==EOT) {pkt->StartByte=EOT; status=EotPkt; break;}

         /* check for the start of a 128-byte or 1024-byte xmodem packet */
         else if (byte==SOH || byte==STX) {pkt->StartByte=byte; status=OkPkt; break;}

         /* check for a request to cancel the transmission */
         else if (byte==CAN) {pkt->StartByte=byte; status=CanPkt; break;}

         /* check if log entry of sync error should be made */
         else if (debuglevel>=5)
         {
            /* create log entry */
            if (!sync_err)
            {
               /* create the message */
               static cc msg[]="Sync error [";

               /* make the logentry */
               LogEntry(FuncName,msg);
            }
            
            /* add to log entry */
            if (byte>=' ' && byte<='~') LogAdd("%c",byte);
            else LogAdd("[0x%02x]",byte);
         }
      }

      /* terminate the log entry of the sync errors */
      if (sync_err>0 && debuglevel>=5) LogAdd("]\n");
      
      /* log the number of sync-errors before the next packet was detected */
      if (sync_err>0 && debuglevel>=2)
      {
         /* create the message */
         static cc format[]="Sync errors encountered: %u\n";

         /* make the logentry */
         LogEntry(FuncName,format,sync_err); 
      }
   }
   
   return status;
}
