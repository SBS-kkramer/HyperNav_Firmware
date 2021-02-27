#ifndef EXPECT_H
#define EXPECT_H

#include <time.h>
#include "serial.h"

/* function prototypes */
int expect(const struct SerialPort *port, const char *prompt, const char *response, time_t sec, const char *trm);

#endif /* EXPECT_H */

#include <string.h>
#include <ctype.h>
#include "logger.h"

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/expect.c#1 $
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
 * $Log: expect.c,v $
 * Revision 1.9  2006/04/21 13:45:40  swift
 * Changed copyright attribute.
 *
 * Revision 1.8  2004/12/29 23:06:50  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.7  2004/04/23 23:43:50  swift
 * *** empty log message ***
 *
 * Revision 1.6  2003/12/24 17:13:19  swift
 * Fixed a minor logging bug that failed to take into account the debuglevel.
 *
 * Revision 1.5  2003/11/20 18:59:42  swift
 * Added GNU Lesser General Public License to source file.
 *
 * Revision 1.4  2003/11/12 22:21:55  swift
 * Eliminated automatic termination of the command string.
 *
 * Revision 1.3  2002/10/08 23:30:17  swift
 * Fixed a simple typo in the comment section.
 *
 * Revision 1.2  2002/10/08 23:29:19  swift
 * Fixed a type-mismatch bug in a LogEntry() call.  Combined header file
 * into source file.
 *
 * Revision 1.1  2002/05/07 22:14:14  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define expectChangeLog "$RCSfile: expect.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

/*------------------------------------------------------------------------*/
/* function to respond to expected prompts                                */
/*------------------------------------------------------------------------*/
/**
   This function reads from a serial port until it receives an expected
   prompt and then it replies with a specified response.  It was designed to
   allow software to log into a computer.  If the expected prompt is not
   received then this function times out and returns.

      \begin{verbatim}
      input:

         port.......A structure that contains pointers to machine dependent
                    primitive IO functions.  See the comment section of the
                    SerialPort structure for details.  The function checks
                    to be sure this pointer is not NULL.

         prompt.....Bytes are read from the serial port until this
                    prompt-string is detected.

         response...Once the prompt-string is read, this response-string is
                    transmitted to the serial port.

         sec........The number of seconds this function will attempt to
                    match the prompt-string.

         trm........A termination string transmitted immediately after the
                    response string.  For example, if the termination string
                    is "\r\n" then the response string will be transmitted
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
int expect(const struct SerialPort *port, const char *prompt,
           const char *response, time_t sec, const char *trm)
{
   /* define the logging signature */
   static cc FuncName[] = "expect()";
   
   int status = -1;
  
   /* verify the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the prompt */
   else if (!prompt)
   {
      /* create the message */
      static cc msg[]="NULL pointer to the prompt.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify that the prompt is not the empty string */
   else if (!(*prompt))
   {
      /* create the message */
      static cc msg[]="Empty prompt string.\n";
      
      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* verify the response */
   else if (!response)
   {
      /* create the message */
      static cc msg[]="NULL pointer to the response.\n";
      
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
   
   else
   {
      unsigned char byte;

      /* get the reference time */
      time_t To=time(NULL),T=To;
         
      /* compute the length of the prompt string */
      int len=strlen(prompt);
      
      /* define the index of prompt string */
      int i=0;
      
      /* re-initialize the return value */
      status=0;

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
            if (byte==prompt[i]) {i++;} else i=0;

            /* prompt string has been found if the index (i) matches its length */
            if (i>=len) {status=1; break;}
         }
         
         /* get the current time */
         else T=time(NULL);
      }

      /* check the termination conditions */
      while (T>=0 && To>=0 && difftime(T,To)<sec);
      
      /* write the response string if the prompt was found */
      if (status<=0)
      {
            /* create the message */
            static cc format[]="Attempt to read prompt string [%s] failed.\n";

            /* terminate the prompt string */
            if (debuglevel>=3) LogAdd("\n");
         
            /* log the message */
            LogEntry(FuncName,format,prompt);
      }
      else 
      {
         /* validate the termination string */
         if (!trm) trm="";
         
         /* write the response string to the serial port */
         if (!response[0]) status=1;
         else status = (pputs(port,response,sec,trm)>0) ? 1 : 0;
         
         /* echo the response to the log stream */
         if (debuglevel>=3)
         {
            int len=strlen(response);
            for (i=0; i<len; i++) 
            {
               char byte = response[i];
               if (byte=='\r') LogAdd("\\r");
               else if (byte=='\n') LogAdd("\\n");
               else if (isprint(byte)) LogAdd("%c",byte);
               else LogAdd("[0x%02x]",byte);
            }
         
            LogAdd("\n");
         }
      }
   }

   return status;
}
