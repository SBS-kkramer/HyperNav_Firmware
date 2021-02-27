#ifndef UPLOAD_H
#define UPLOAD_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/upload.c#1 $
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
 * $Log: upload.c,v $
 * Revision 1.3  2006/04/21 13:45:42  swift
 * Changed copyright attribute.
 *
 * Revision 1.2  2005/02/22 21:37:24  swift
 * Eliminate writes to the profile file during telemetry retries.
 *
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define uploadChangeLog "$RCSfile: upload.c,v $ $Revision: #1 $ $Date: 2016/01/20 $"

#include "serial.h"

/* function prototypes */
int UpLoadFile(const struct SerialPort *modem, const char *localpath, const char *hostpath);

#endif /* UPLOAD_H */

#include "logger.h"
#include <unistd.h>
#include <stdio.h>
#include "tx.h"
#include "chat.h"
#include <string.h>

#ifdef _XA_
   #include <apf9.h>
   #include <apf9icom.h>
#else
   #define WatchDog()
   #define StackOk() 1
#endif /* _XA_ */

/* define the command to download via xmodem */
static const char *rxcmd = "rx -c ";

int snprintf(char *str, size_t size, const char *format, ...);

/*------------------------------------------------------------------------*/
/* function to upload a file from the Iridium float to the remote host    */
/*------------------------------------------------------------------------*/
/**
   This function uploads a data file from the Iridium float to the remote
   host.
     
      \begin{verbatim}
      input:
         modem.......A structure that contains pointers to machine dependent
                     primitive IO functions.  See the comment section of the
                     SerialPort structure for details.  The function checks
                     to be sure this pointer is not NULL.

         localpath...The filename on the CompactFlash to be transferred to
                     the remote host.
 
         hostpath....The filename on the remote host where the file will be
                     stored. 
                     
      output:
         This function returns a positive value if successful, zero if it
         fails, and a negative value if exceptions were detected in the
         function's arguments.
      \end{verbatim}
*/
int UpLoadFile(const struct SerialPort *modem, const char *localpath, const char *hostpath)
{
   /* function name for log entries */
   static cc FuncName[] = "UpLoadFile()";

   /* initialize return value */
   int status=-1;
            
   /* pet the watch dog */
   WatchDog(); 

   /* validate the port */
   if (!modem)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }
   
   /* validate the hostpath */
   else if (!hostpath)
   {
      /* create the message */
      static cc msg[]="NULL hostpath.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the localpath */
   else if (!localpath)
   {
      /* create the message */
      static cc msg[]="NULL localpath.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }
     
   /* check if carrier-dectect enabled and CD line not asserted */
   else if (modem->cd && !modem->cd())
   {
      /* create the message */
      static cc msg[]="No carrier detected.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);

      /* indicate failure */
      status=0;
   }

   /* attempt to upload the file */
   else
   {
      /* define a temporary buffer to hold filenames and commands */
      char cmd[64];
       
      /* define the command timeout period */
      const time_t timeout=30;
      
      /* define a FILE pointer for uploading the data file */
      FILE *source=NULL;

      /* initialize the return value */
      status=0;
      
      /* pet the watch dog */
      WatchDog(); 
      
      /* open the file to transfer */
      if ((source=fopen(localpath,"r"))!=0)
      {
         /* create the host's command to upload the config file */
         snprintf(cmd,sizeof(cmd)-1,"%s %s",rxcmd,hostpath);

         /* execute the command on the host */
         if ((status=chat(modem,cmd,cmd,timeout,"\n"))>0)
         {
            /* log the upload attempt */
            if (debuglevel>=2)
            {
               /* create the message */
               static cc format[]="Uploading \"%s\" to host as \"%s\".\n";

               /* make the logentry */
               LogEntry(FuncName,format,localpath,hostpath);
            }
            
            /* upload the data file to the host */
            if (Tx(modem,source)<=0)
            {
               /* create the message  and make the logentry*/
               static cc msg[]="Upload failed.\n"; LogEntry(FuncName,msg);

               /* indicate failure */
               status=0;
            }
            
            /* make a log entry that the upload was successful */
            else
            {
               /* indicate success */
               status=1;

               if (debuglevel>=2)
               {
                  /* create the message */
                  static cc msg[]="Upload successful.\n";

                  /* make the logentry */
                  LogEntry(FuncName,msg);
               }
            }
         }

         /* make a log entry that the chat session failed */
         else
         {
            /* create the message */
            static cc format[]="Failed attempt to execute \"%s\" on the host.\n";

            /* make the logentry */
            LogEntry(FuncName,format,cmd);
         }
         
         /* close the temporary file */
         fclose(source);
      }

      /* make a log entry that the file can't be opened for reading */
      else
      {
         /* create the message */
         static cc format[]="Unable to open \"%s\" for reading.\n";

         /* make the logentry */
         LogEntry(FuncName,format,localpath);
      }
   }
   
   return status;
}
