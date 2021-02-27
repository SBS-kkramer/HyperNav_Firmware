#ifndef LOGGER_H
#define LOGGER_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/logger.c#1 $
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
 * $Log: logger.c,v $
 * Revision 1.19  2006/04/21 13:37:41  swift
 * Changed copyright attribute.
 *
 * Revision 1.18  2005/12/28 21:31:22  swift
 * Added calls to pet the WatchDog.
 *
 * Revision 1.17  2005/11/11 22:22:05  swift
 * Made the MaxLogSize object persistent.
 *
 * Revision 1.16  2004/12/29 19:46:40  swift
 * Fixed a bug in LogEntry() that used a cc pointer where a char pointer was
 * required.
 *
 * Revision 1.15  2004/12/09 01:20:02  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.14  2004/06/07 21:19:41  swift
 * Modifications to write mission time to flash-based log file.
 *
 * Revision 1.13  2004/03/24 00:13:52  swift
 * Changed the debuglevel to be a persistent variable.
 *
 * Revision 1.12  2003/11/20 18:59:42  swift
 * Added GNU Lesser General Public License to source file.
 *
 * Revision 1.11  2003/07/19 22:41:22  swift
 * Implement a new definition of the most terse logging mode (debuglevel=0).
 *
 * Revision 1.10  2003/07/03 22:52:13  swift
 * Eliminated the need to include 'ds2404.h' as a portability enhancement.
 *
 * Revision 1.9  2003/06/11 18:35:18  swift
 * Modified LogEntry() to use a static character buffer rather
 * than an automatic variable that is created on the program
 * stack.  The purpose is to reduce stack requirements and the
 * probability of stack overflows.
 *
 * Revision 1.8  2003/06/07 20:32:58  swift
 * Modifications to include interval-timer in the log output.
 *
 * Revision 1.7  2003/05/31 21:19:48  swift
 * Tightened up some argument checking in LogEntry() and LogAdd().
 *
 * Revision 1.6  2002/12/31 15:22:08  swift
 * Added some documentation about the debuglevel.
 *
 * Revision 1.5  2002/11/27 00:43:17  swift
 * Fixed a bug caused when logstream is NULL.
 *
 * Revision 1.4  2002/10/21 13:08:09  swift
 * Fixed a formatting bug in LogSize() that reversed the order of the timestamp
 * and function name in the log file.
 *
 * Revision 1.3  2002/10/16 16:10:04  swift
 * Changed debuglevel for log entry of LogOpen() and LogClose() functions.
 *
 * Revision 1.2  2002/10/08 23:39:25  swift
 * Combined header file into source file.  Modifications to accomodate
 * file sizes larger than the 32K limit of int's.
 *
 * Revision 1.1  2002/05/07 22:09:16  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define LoggerChangeLog "$RCSfile: logger.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include <stdio.h>

#ifdef _XA_
   typedef code const char cc;
#else
   typedef const char cc;
   #define persistent
#endif /* _XA_ */

/* external prototypes of functions for the logging module */
long int LogAdd(const char *format,...);
int      LogAttach(FILE *stream);
int      LogClose(void);
long int LogEntry(cc *function_name,cc *format,...);
int      LogOpen(const char *fname,char mode);
int      LogOpenAuto(const char *basepath);
long int LogSize(void);
FILE    *LogStream(void);

/* external declaration for maximum allowed log size */
extern persistent long int MaxLogSize;

/* external declaration of debuglevel */
extern persistent unsigned int debuglevel;

/* declare the default message stream */
extern FILE *persistent logstream;

#endif /* LOGGER_H */

#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "cstring.h"

#ifdef _XA_
   #include <apf9.h>
#else
   #define WatchDog()
#endif /* _XA_ */

/* define the maximum length of the format string */
#define MAXFMTLEN (255)

/* function prototype that retrieves the interval timer */
time_t itimer(void);

/* define the maximum allowed size of the log file (bytes) */
persistent long int MaxLogSize;

/*  
  Define and initialize the debuglevel.  Despite that the debuglevel is not
  used by any function in the logger module, it is defined in this module
  because it is used to control the verbosity of logging operations.  The
  intention is that logging can be made more terse by reducing the
  debuglevel such that debuglevel=0 produces the fewest log entries and
  debuglevel=5 produces the maximum number of log entries.  Here is an
  example of its typical use:

     if (debuglevel>=3) {LogEntry("foo()","Situation normal: FUBAR\n");}

  The default is debuglevel=2.  
*/
persistent unsigned int debuglevel;

/* define the default message stream */
FILE *persistent logstream;

/* local declarations of function prototypes */
int snprintf(char *str, size_t size, const char *format, ...);

/*------------------------------------------------------------------------*/
/* Function to print a message to the log file.                           */
/*------------------------------------------------------------------------*/
/**
   This function appends a non-timestamped message to the log file.  The
   arguments of this function match the arguments to the printf() function.

   The return value of this function is the number of bytes written to the
   logstream.
*/
long int LogAdd(const char *format,...)
{
   long int len=0;
       
   /* pet the watch dog */
   WatchDog();

   /* a debuglevel of zero suppresses all logging */
   if (debuglevel>0)
   {
      /* validate the format */
      if (!format || !(*format))
      {
         /* define the error message */
         const char *msg = "Missing format specification in LogAdd().\n";
      
         /* warn of the missing format specification */
         if (stderr) fprintf(stderr,msg);

         /* make a logentry of the missing format specification */
         if (logstream && logstream!=stderr && LogSize()<MaxLogSize) {fprintf(logstream,msg);}

         /* indicate failure */
         len=-1;
      }

      else
      {
         va_list arg_pointer;
      
         /* write the logentry to stderr */
         if (stderr)
         {
            /* get the format argument from the argument list */
            va_start(arg_pointer,format);
            
            /* print the message */
            vfprintf(stderr,format,arg_pointer);
         
            /* clean up call */
            va_end(arg_pointer);

            /* flush the output */
            fflush(stderr);
         }

         /* write the logentry to the logstream */
         if (logstream && logstream!=stderr && LogSize()<MaxLogSize)
         {
            /* get the format argument from the argument list */
            va_start(arg_pointer,format);
         
            /* print the message */
            len=vfprintf(logstream,format,arg_pointer);
         
            /* clean up call */
            va_end(arg_pointer);
         
            /* flush the output */
            fflush(logstream);
         }
      }
   }
   
   return len;
}

/*------------------------------------------------------------------------*/
/* function to attach the log stream to a user-supplied stream            */
/*------------------------------------------------------------------------*/
/**
   This function attaches the logstream to a user-suppled stream.

      \begin{verbatim}
      input:

         stream....The user-supplied stream to which the logstream will be
                   attached.
      \end{verbatim}

   This function returns one if the logstream was successfully attached to
   the user-supplied stream, zero otherwise.
*/
int LogAttach(FILE *stream) 
{
   /* define logentry signature */
   static cc FuncName[] = "LogAttach()";
   
   int status=0;

   if (stream)
   {
      /* validate the stream before attaching to it */
      if (!ferror(stream))
      {
         /* create the message */
         static cc msg[]="Enabling logging functions.\n";

         /* log the message */
         LogEntry(FuncName,msg);
         
         /* attach the log stream */
         logstream=stream; status=1;
      }
 
      /* make a log entry */
      else
      {
         /* create the message */
         static cc msg[]="Attempt to attach log stream failed.\n";

         /* log the message */
         LogEntry(FuncName,msg);
      }
   }
   else
   {
      /* create the message */
      static cc msg[]="Attaching to a NULL stream is not allowed.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to close the log file.                                        */
/*------------------------------------------------------------------------*/
/**
   This function closes the logstream. The return value of this function is
   one if the logstream was successfully closed, zero otherwise.
*/
int LogClose(void)
{
   /* define logentry signature */
   static cc FuncName[] = "LogClose()";

   int status=1;
   
   if (logstream)
   {
      /* don't close if log is either stderr or stdout */
      if (logstream!=stdout && logstream!=stderr) 
      {
         /* Make a log entry before closing the log file */
         if (debuglevel>=3)
         {
            /* create the message */
            static cc msg[]="Disabling logging functions.\n";

            /* log the message */
            LogEntry(FuncName,msg);
         }
         
         /* close the current log file */
         if (fclose(logstream))
         {
            /* create the message */
            static cc msg[]="Attempt to close log file failed.\n";

            /* make a log entry */
            LogEntry(FuncName,msg);

            /* indicate failed attempt to close current log file */
            status=0; 
         }
      }
   }

   /* reattach the default message stream */
   logstream=NULL;
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to open the log file for storing run-time messages.           */
/*------------------------------------------------------------------------*/
/**
   This function is designed to open a file into which the logstream will be
   written.

      \begin{verbatim}
      input:

         fname....The name of the file into which the logstream will be
                  written.

         mode.....The mode used to open the file.  Possible choices are 'w'
                  or 'a'.  The 'w' mode opens a file and discards any
                  contents if the file previously existed.  The 'a' mode
                  appends to a file if it already exists, otherwise it opens
                  the file for writing.
       \end{verbatim}

    This function returns one if the file was successfully opened, zero otherwise.
*/
int LogOpen(const char *fname,char mode)
{
   /* define logentry signature */
   static cc FuncName[] = "LogOpen()";

   int status=-1;
   
   if (fname)
   {
      /* initialize the mode string */
      const char *modestr=(mode=='w')?"w+":"a+";

      /* check for an existing log file to close */
      if (logstream) {LogClose();}
      
      /* open the log file */
      if (!(logstream=fopen(fname,modestr)))
      {
         /* create the message */
         static cc format[]="Unable to open \"%s\" using mode \"%s\".\n";
         
         /* make a log entry about the failed attempt to open the log file */
         LogEntry(FuncName,format,fname,modestr);

         /* open failed */
         status=0;
      }
      else status=1;
      
      /* make a log entry */
      if (debuglevel>=3)
      {
         /* create the message */
         static cc msg[]="Enabling logging functions.\n";
         
         /* make the log entry */
         LogEntry(FuncName,msg);
      }
   }
   else
   {
      /* create the message */
      static cc msg[]="Pathname for log file required.\n";
         
      /* make the log entry */
      LogEntry(FuncName,msg);
   }
      
   return status;
}

/*------------------------------------------------------------------------*/
/* function to open an automatic logfile                                  */
/*------------------------------------------------------------------------*/
/**
   This function opens a logfile using an automatically generated logfile
   name.  The logfile name will have the form:
      \$(basepath).\$(month)\$(day)-\$(hour):\$(minute)
   where the basepath is user-specified and the current date is
   automatically determined.

      \begin{verbatim}
      input:
         basepath....The basepath specifies the logfile name except for an
                     extension based on the current date and time.  If
                     basepath is NULL then the basepath is automatically
                     selected as well.
      \end{verbatim}

   This function returns one if the automatic logfile was successfully
   opened, zero otherwise.
*/
int LogOpenAuto(const char *basepath)
{
   unsigned int len=0;
   time_t t=0;
   int status=0;
   
   /* set the maximum length for the logfile name */
   #define MAXLEN 1023

   /* make room for the logfile name */
   char fname[MAXLEN+1];
         
   /* validate the directory */
   if (!basepath || strlen(basepath)>(MAXLEN-20)) basepath="./msglog";

   /* create the base-path */
   sprintf(fname,"%s.",basepath);

   /* compute the length of the logfile directory string */
   len = strlen(fname);

   /* get the current local time */
   t=time(NULL); 
         
   /* create the logfile name */
   strftime(fname+len,MAXLEN-len,"%b%d-%H:%M:%S",gmtime(&t));

   /* open the logfile */
   status=LogOpen(fname,'w');

   return status;
   
   #undef MAXLEN
}
  
/*------------------------------------------------------------------------*/
/* function to append a timestamped entry to the logstream                */
/*------------------------------------------------------------------------*/
/**
   This function appends a timestamped entry to the logstream.

      \begin{verbatim}
      input:
         function_name...The name of the function from which the logentry is
                         made.  This aids greatly in interpreting the
                         logfile afterwards.

         format..........The format and the remainder of the arguments match
                         exactly those that would be used in the printf()
                         function.
      \end{verbatim}

   This function returns the number of bytes written to the logstream.
*/
long int LogEntry(cc *function_name,cc *format,...)
{
   long int len=0;
     
   /* pet the watch dog */
   WatchDog();

   /* a debuglevel of zero suppresses all logging */
   if (debuglevel>0)
   {
      /* validate the format */
      if (!format || !(*format))
      {
         /* define logentry signature */
         const char *FuncName = "LogEntry()";

         /* define the error message */
         const char *msg = "%-20s Missing format specification.";
      
         /* warn of the missing format specification */
         if (stderr) fprintf(stderr,msg,FuncName);

         /* make a logentry of the missing format specification */
         if (logstream && logstream!=stderr && LogSize()<MaxLogSize) {fprintf(logstream,msg,FuncName);}

         /* indicate failure */
         len=-1;
      }

      else
      {
         va_list arg_pointer;
         char fmt[MAXFMTLEN], func[21], buf[32];

         /* get the current calendar time and the interval-timer */
         time_t t=time(NULL), itime=itimer(); 

         /* check for interval-timer value that exceeds 7 characters */
         if (itime<-999999L || itime>9999999L) itime=(time_t)(-1); 
         
         /* copy the format from code ROM */
         cstrncpy(fmt,format,MAXFMTLEN);

         /* copy the function name from code ROM */
         if (function_name) cstrncpy(func,function_name,sizeof(func));
         else snprintf(func,sizeof(func),"<NULL>");

         /* write the date and time */
         strftime(buf,sizeof(buf),"%b %d %Y %H:%M:%S",gmtime(&t));
   
         /* write the logentry to stderr */
         if (stderr)
         {
            /* print the date and time */
            fprintf(stderr,"(%s, %7ld sec) ",buf,itime);

            /* print the function identifier */
            fprintf(stderr,"%-20s ",func);
         
            /* get the format argument from the argument list */
            va_start(arg_pointer,format);
         
            /* print the message */
            vfprintf(stderr,fmt,arg_pointer);
         
            /* clean up call */
            va_end(arg_pointer);

            /* flush the output */
            fflush(stderr);
         }

         /* write a copy of the logentry to the logstream */
         if (logstream && logstream!=stderr && LogSize()<MaxLogSize)
         {
            /* print the date and time */
            len=fprintf(logstream,"(%s, %7ld sec) ",buf,itime);
         
            /* print the function identifier */
            len+=fprintf(logstream,"%-20s ",func);
         
            /* get the format argument from the argument list */
            va_start(arg_pointer,format);
         
            /* print the message */
            len+=vfprintf(logstream,fmt,arg_pointer);
         
            /* clean up call */
            va_end(arg_pointer);
         
            /* flush the output */
            fflush(logstream);
         }
      }
   }
 
   return len;
}

/*------------------------------------------------------------------------*/
/* function to return the current size of the log file                    */
/*------------------------------------------------------------------------*/
/**
   This function determines the size (in bytes) of the log stream if the log
   stream represents a file.  
*/
long int LogSize(void)
{
   long int size=-1;
   time_t t=0;
   char buf[128];
   persistent static int WarnTooBig;
      
   /* validate the file name */
   if (logstream && logstream!=stderr && logstream!=stdout)
   {
      if (fseek(logstream,0,SEEK_END)>=0)
      {
         /* initialize the one-time switch */
         if ((size=ftell(logstream))>=0 && size<256) WarnTooBig=0;
         
         /* position the file pointer to the end of the stream */
         if (size>=0 && !WarnTooBig && MaxLogSize>0 && size>MaxLogSize*0.95) 
         {
            /* get the date & time */
            t=time(NULL); strftime(buf,127,"%b %d %Y %H:%M:%S",gmtime(&t));

            /* print the function identifier */
            size+=fprintf(logstream,"\n\n(%s) %-20s Log file is %ld bytes.  " 
                          "Limit is %ld bytes.\n\n",buf,"LogSize()",size,MaxLogSize); 

            /* flush the output */
            fflush(logstream);

            /* set the one-time switch */
            WarnTooBig=1;
         }
      }
   }
   
   return size;
}

/*------------------------------------------------------------------------*/
/* function to return the logstream FILE pointer                          */
/*------------------------------------------------------------------------*/
FILE *LogStream(void)
{
   return logstream;
}
