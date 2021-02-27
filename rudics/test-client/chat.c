#ifndef CHAT_H
#define CHAT_H

#include <time.h>
#include "serial.h"

/* function prototype */
int chat(const struct SerialPort *port, const char *cmd, const char *expect, time_t sec, const char *trm);

#endif /* CHAT_H */

#include <string.h>
#include <ctype.h>
#include "logger.h"

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/chat.c#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* Copyright University of Washington
 *  
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or (at
 * your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: chat.c,v $
 * Revision 1.11  2006/04/21 13:45:40  swift
 * Changed copyright attribute.
 *
 * Revision 1.10  2005/07/07 15:07:15  swift
 * Fixed a bug caused by 1-second time descretization.
 *
 * Revision 1.9  2004/12/29 23:06:50  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.8  2004/04/23 23:43:50  swift
 * *** empty log message ***
 *
 * Revision 1.7  2003/12/20 19:46:24  swift
 * Fixed a bug in the return value in the case where there was a zero-length
 * expect-string.
 *
 * Revision 1.6  2003/11/20 18:59:42  swift
 * Added GNU Lesser General Public License to source file.
 *
 * Revision 1.5  2003/11/12 18:40:04  swift
 * Eliminated automatic termination of the command string.
 *
 * Revision 1.4  2003/07/05 21:22:11  swift
 * Changed logging protocols.
 *
 * Revision 1.3  2002/12/31 16:58:29  swift
 * Eliminated the reference to the unistd.h header file.
 *
 * Revision 1.2  2002/10/08 23:28:22  swift
 * Fixed a type-mismatch bug in a LogEntry() call.  Combined header file
 * into source file.
 *
 * Revision 1.1  2002/05/07 22:13:47  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define chatChangeLog "$RCSfile: chat.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

/* function prototypes */
unsigned int sleep(unsigned int seconds);

/*------------------------------------------------------------------------*/
/* function to negotiate commands                                         */
/*------------------------------------------------------------------------*/
/**
   This function transmits a command string to the serial port and verifies
   and expected response.  The command string should not include that
   termination character (\r), as this function transmits the termination
   character after the command string is transmitted.  The command string
   may include wait-characters (~) to make the processing pause as needed.
   The command string is processed each byte in turn.  Each time a
   wait-character is encountered, processing is halted for one wait-period
   (1 sec) and then processing is resumed.
   
      \begin{verbatim}
      input:

         port.......A structure that contains pointers to machine dependent
                    primitive IO functions.  See the comment section of the
                    SerialPort structure for details.  The function checks
                    to be sure this pointer is not NULL.

         cmd........The command string to transmit.

         expect.....The expected response to the command string.

         sec........The number of seconds this function will attempt to
                    match the prompt-string.

         trm........A termination string transmitted immediately after the
                    command string.  For example, if the termination string
                    is "\r\n" then the command string will be transmitted
                    first and followed immediately by the termination
                    string.  If trm=NULL or trm="" then no termination
                    string is transmitted.

      output:

         This function returns a positive number if the exchange was
         successful.  Zero is returned if the exchange failed.  A negative
         number is returned if the function parameters were determined to be
         ill-defined. 
      \end{verbatim}

   written by Dana Swift
*/
int chat(const struct SerialPort *port, const char *cmd,
         const char *expect, time_t sec, const char *trm)
{
   /* define the logging signature */
   static cc FuncName[] = "chat()";

   int status = -1;
  
   /* verify the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   /* verify the cmd */
   else if (!cmd)
   {
      /* create the message */
      static cc msg[]="NULL pointer to the command string.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the expect string */
   else if (!expect) 
   {
      /* create the message */
      static cc msg[]="NULL pointer to the expect string.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the serial port's putb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL putb() function for serial port.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the serial port's getb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL getb() function for serial port.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the timeout period */
   else if (sec<=0)
   {
      /* create the message */
      static cc format[]="Invalid time-out period: %ld\n";
      
      /* log the message */
      LogEntry(FuncName,format,sec);
   }

   /* flush the IO buffers prior to sending the command string */
   else if (pflushio(port)<=0)
   {
      /* create the message */
      static cc msg[]="Attempt to flush IO buffers failed.";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   else
   {
      /* define the wait-character */
      const unsigned char wait_char = '~';

      /* define the wait-period (sec) per wait-character */
      const time_t wait_period = 1;

      /* compute the length of the command string */
      int i,len=strlen(cmd);

      /* work around a time descretization problem */
      if (sec==1) sec=2;
      
      /* make a log entry */
      if (debuglevel>=3) {static cc msg[]="Sent: "; LogEntry(FuncName,msg);}
 
      /* transmit the command to the serial port */
      for (status=0, i=0; i<len; i++)
      {
         /* check if the current byte is the wait-character */
         if (cmd[i]==wait_char) {sleep(wait_period);}

         else if (pputb(port,cmd[i],1)<=0)
         {
            /* create the message */
            static cc format[]="Attempt to send command string (%s) failed.\n";

            /* log the message */
            LogEntry(FuncName,format,cmd);

            goto Err;
         }
         else if (debuglevel>=3)
         {
            if (cmd[i]=='\r') LogAdd("\\r");
            else if (cmd[i]=='\n') LogAdd("\\n");
            else if (isprint(cmd[i])) LogAdd("%c",cmd[i]);
            else LogAdd("[0x%02x]",cmd[i]);
         }
      }

      /* transmit the (optional) command termination to the serial port */
      if (trm && trm[0]) 
      {
         /* compute the length of the termination string */
         len=strlen(trm);
         
         /* transmit the command termination to the serial port */
         for (i=0; i<len; i++)
         {
            if (pputb(port,trm[i],1)<=0)
            {
               /* create the message */
               static cc format[]= "Attempt to send termination string (%s) failed.\n";
               
               /* log the message */
               LogEntry(FuncName,format,trm);

               goto Err;
            }
            else if (debuglevel>=3)
            {
               if (trm[i]=='\r') LogAdd("\\r");
               else if (trm[i]=='\n') LogAdd("\\n");
               else if (isprint(trm[i])) LogAdd("%c",trm[i]);
               else LogAdd("[0x%02x]",trm[i]);
            }
         }
      }
      
      /* terminate the last logentry */
      if (debuglevel>=3) LogAdd("\n");
      
      /* seek the expect string in the modem response */
      if (*expect)
      {
         unsigned char byte;

         /* get the reference time */
         time_t To=time(NULL),T=To;
         
         /* compute the length of the prompt string */
         int len=strlen(expect);
      
         /* define the index of prompt string */
         i=0;

         /* make a log entry */
         if (debuglevel>=3) {static cc msg[]="Received: "; LogEntry(FuncName,msg);}
         
         do 
         {
            /* read the next byte from the serial port */
            if (port->getb(&byte)>0)
            {
               /* write the current byte to the logstream */
               if (debuglevel>=3)
               {
                  if (byte=='\r') LogAdd("\\r");
                  else if (byte=='\n') LogAdd("\\n");
                  else if (isprint(byte)) LogAdd("%c",byte);
                  else LogAdd("[0x%02x]",byte);
               }
               
               /* check if the current byte matches the expected byte from the prompt */
               if (byte==expect[i]) {i++;} else i=0;

               /* the expect-string has been found if the index (i) matches its length */
               if (i>=len) {status=1; break;}
            }
            
            /* get the current time */
            else T=time(NULL);
         }

         /* check the termination conditions */
         while (T>=0 && To>=0 && difftime(T,To)<sec);

         /* terminate the last logentry */
         if (debuglevel>=3) LogAdd("\n");
         
         /* write the response string if the prompt was found */
         if (status<=0)
         {
            /* create the message */
            static cc format[]="Expected string [%s] not received.\n";
      
            /* log the message */
            LogEntry(FuncName,format,expect);
         }
         
         /* report a successful chat session */
         else if (debuglevel>=3)
         {
            /* create the message */
            static cc format[]="Expected response [%s] received.\n";
      
            /* log the message */
            LogEntry(FuncName,format,expect);
         }
      }
      else status=1;
   }

   Err: /* collection point for errors */

   return status;
}
