#ifndef SERIAL_H
#define SERIAL_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/serial.c#1 $
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
 * $Log: serial.c,v $
 * Revision 1.11  2006/04/21 13:37:41  swift
 * Changed copyright attribute.
 *
 * Revision 1.10  2006/01/06 18:23:58  swift
 * Added rts(), cts(), dtr(), and dsr() primitives to the SerialPort interface.
 *
 * Revision 1.9  2005/11/11 22:21:25  swift
 * Fixed bugs caused by previous speed-up enhancements.
 *
 * Revision 1.8  2004/12/09 01:20:02  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.7  2004/04/23 23:43:50  swift
 * *** empty log message ***
 *
 * Revision 1.6  2004/02/05 23:48:45  swift
 * Implemented a speed-up to pgets() as suggested by John Dunlap.
 *
 * Revision 1.5  2003/11/20 18:59:42  swift
 * Added GNU Lesser General Public License to source file.
 *
 * Revision 1.4  2003/11/12 22:28:24  swift
 * Added some documentation and changed an initialization in pgetbuf().
 *
 * Revision 1.2  2002/10/08 23:57:15  swift
 * Addition of ioflush() and cd() primitive functions to the serial port
 * structure.
 *
 * Revision 1.1  2002/05/07 22:18:29  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define SerialChangeLog "$RCSfile: serial.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include <time.h>

/*========================================================================*/
/* structure to implement abstraction of serial port communications       */
/*========================================================================*/
/**
   This structure implements an abstraction mechanism for serial ports.  It
   does this by providing low-level primitive serial port IO functionality
   on which functions above the abstraction layer can build.

   int getb(unsigned char *byte):

      This primitive attempts to read one byte from the serial port and then
      return.  A nonblocking IO model is presumed so that the function
      returns whether or not the read attempt was successful.

      \begin{verbatim}
      output:

         byte....When dereferenced, the value (*byte) will contain the byte
                 read from the serial port or else 0x00 if the read attempt
                 failed.

         This function will return a strictly positive value if the read
         attempt was successful.  If the read attempt fails, this function
         will return a zero or negative value.  Except for these criteria,
         the return values and their meaning are undefined and
         implementation dependent.

      \end{verbatim}

   int putb(unsigned char byte):

      This primitive attempts to write one byte to the serial port and then
      return.  A nonblocking IO model is presumed so that the function
      returns whether or not the write attempt was successful.

      \begin{verbatim}
      input:

         byte...The hex value of the byte to write to the serial port.  Any
                value in the closed range [0x00,0xff] is valid and any hex
                value outside this range is invalid.  The primitive will
                check and enforce these criteria.
      
      output:

         This function will return a strictly positive value if the write
         attempt was successful.  If the write attempt fails, this function
         will return a zero or negative value.  Except for these criteria,
         the return values and their meaning are undefined and
         implementation dependent.
      \end{verbatim}

   int iflush(void):

      This primitive attempts to flush the receive buffer of the serial
      port.  This function will return a strictly positive value if the
      flush was successful otherwise a zero or negative value will be
      returned.  Except for these criteria, the return values and their
      meaning are undefined and implementation dependent.

   int ioflush(void):

      This primitive attempts to flush the both the transmit and receive
      buffers of the serial port.  This function will return a strictly
      positive value if the flush was successful otherwise a zero or
      negative value will be returned.  Except for these criteria, the
      return values and their meaning are undefined and implementation
      dependent.

   int oflush(void):

      This primitive attempts to flush the transmit buffer of the serial
      port.  This function will return a strictly positive value if the
      flush was successful otherwise a zero or negative value will be
      returned.  Except for these criteria, the return values and their
      meaning are undefined and implementation dependent.

   int obytes(void):

      This primitive returns the number of bytes remaining in the output
      buffer.  This the number of bytes in the output buffer can be
      determined then this function will return a non-negative number equal
      to the number of bytes remaining to be transmitted.  If the number of
      bytes can't be determined, say because the OS or hardware do not
      support the ability, then this function should return -1.
  
   int cd(void);

      This primitive determines whether or not the carrier detect hardware
      line is asserted.  This primitive should return a strictly positive
      value if the carrier detect is asserted else it should return zero or
      a negative number.
   
   int rts(int state)

      This primitive sets the state of the RTS line according to the 'state'
      argument.  If state is zero then the RTS line should be cleared; if
      nonzero then the RTS line should be asserted.  This primitive should
      return a positive value on success, zero on failure, and a negative
      number if an error condition was encountered.

   int cts(void)

      This primitive returns a positive value if the CTS line is asserted,
      zero if the CTS line is cleared.  A negative return value indicates an
      error condition or else that the CTS signal is undefined.

   int dtr(int state)

      This primitive sets the state of the DTR line according to the 'state'
      argument.  If state is zero then the DTR line should be cleared; if
      nonzero then the RTS line should be asserted.  This primitive should
      return a positive value on success, zero on failure, and a negative
      number if an error condition was encountered.

   int dsr(void)

      This primitive returns a positive value if the DSR line is asserted,
      zero if the DSR line is cleared.  A negative return value indicates an
      error condition or else that the DSR signal is undefined.

   Written by Dana Swift
*/
struct SerialPort
{
      int (*getb)(unsigned char *byte);
      int (*putb)(unsigned char byte);
      int (*iflush)(void);
      int (*ioflush)(void);
      int (*oflush)(void);
      int (*obytes)(void);
      int (*cd)(void);
      int (*rts)(int state);
      int (*cts)(void);
      int (*dtr)(int state);
      int (*dsr)(void);
};

/* function prototypes */
int pflushio(const struct SerialPort *port);
int pflushrx(const struct SerialPort *port);
int pflushtx(const struct SerialPort *port);
int pgetb(const struct SerialPort *port, unsigned char *byte, time_t sec);
int pgetbuf(const struct SerialPort *port, void *buf, int size, time_t sec);
int pgets(const struct SerialPort *port, char *buf, int size, time_t sec, const char *trm);
int pputb(const struct SerialPort *port, unsigned char byte, time_t sec);
int pputbuf(const struct SerialPort *port, const void *buf, int size, time_t sec);
int pputs(const struct SerialPort *port, const char *buf, time_t sec, const char *trm);

#endif /* SERIAL_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "logger.h"

/*------------------------------------------------------------------------*/
/* function to flush the IO buffers of a serial port                      */
/*------------------------------------------------------------------------*/
/**
   This function flushes the receive and transmit buffers of a serial port.  

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.
      
      output:

         This function returns a positive value if the Rx and Tx buffers were
         successfully flushed.  A zero or negative value indicates that the
         attempt to flush the buffers failed.
     \end{verbatim}
      
   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.iflush, port.oflush are initialized with non-NULL values.
   
   Written by Dana Swift
*/
int pflushio(const struct SerialPort *port)
{
   /* define the logging signature */
   static cc FuncName[] = "pflushio()";
   
   int status=-1;
   
   /* validate the SerialPort object */
   if (!port) 
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's iiflush() function */
   else if (!port->ioflush)
   {
      /* create the message */
      static cc msg[]="NULL ioflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's iflush() function */
   else if (!port->iflush)
   {
      /* create the message */
      static cc msg[]="NULL iflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's oflush() function */
   else if (!port->oflush)
   {
      /* create the message */
      static cc msg[]="NULL oflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* attempt to use the ioflush() function */
   else if ((status=port->ioflush())<=0)
   {
      /* flush the IO buffers of the serial port */
      int s1=port->iflush(), s2=port->oflush();

      /* determine if both operations were successful */
      status = (s1>0 && s2>0) ? 1 : 0;
   }

   return status;
}

/*------------------------------------------------------------------------*/
/* function to flush the Rx buffer of a serial port                       */
/*------------------------------------------------------------------------*/
/**
   This function flushes the receive buffer of a serial port.  

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.
      
      output:

         This function returns a positive value if the Rx buffer was
         successfully flushed.  A zero or negative value indicates that the
         attempt to flush the Rx buffer failed.
     \end{verbatim}
      
   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.iflush are initialized with non-NULL values.
   
   Written by Dana Swift
*/
int pflushrx(const struct SerialPort *port)
{
   /* define the logging signature */
   static cc FuncName[] = "pflushrx()";
   
   int status=-1;
   
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's iflush() function */
   else if (!port->iflush)
   {
      /* create the message */
      static cc msg[]="NULL iflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* flush the Rx buffer of the serial port */
   else {status=port->iflush();}

   return status;
}

/*------------------------------------------------------------------------*/
/* function to flush the Tx buffer of a serial port                       */
/*------------------------------------------------------------------------*/
/**
   This function flushes the transmit buffer of a serial port.  

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.
      
      output:

         This function returns a positive value if the Tx buffer was
         successfully flushed.  A zero or negative value indicates that the
         attempt to flush the Tx buffer failed.
     \end{verbatim}
      
   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.oflush are initialized with non-NULL values.
   
   Written by Dana Swift
*/
int pflushtx(const struct SerialPort *port)
{
   /* define the logging signature */
   static cc FuncName[] = "pflushtx()";
   
   int status=-1;
   
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
 
   /* validate the serial port's iflush() function */
   else if (!port->oflush)
   {
      /* create the message */
      static cc msg[]="NULL oflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
 
   /* flush the Tx buffer of the serial port */
   else {status=port->oflush();}

   return status;
}

/*------------------------------------------------------------------------*/
/* function to get a byte from the serial port                            */
/*------------------------------------------------------------------------*/
/**
   This function is designed to extract a single byte from a serial port or
   else time-out and return after a specified number of seconds.  A time-out
   period measured in milliseconds would have been preferred but the ANSI C
   library does not provide for timing with better resolution than 1 second.
   As portability was a major design criteria, a time-out period measured in
   seconds was accepted.  

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         sec.....This function will return immediately upon reading a byte
                 from the serial port.  If no byte is available after the
                 specified number of seconds then this function will return
                 without reading a byte.  Due to the limited (ie., 1 sec)
                 resolution of the time() function, the actuall timeout
                 period will be somewhere in the semiclosed interval
                 [sec,sec+1) if sec>0. If sec<=0 then this function will
                 attempt to read a byte from the serial port but return
                 immediately if the attempt fails.
      
      output:

         byte....This is where the byte is stored after it is read from the
                 serial port.  The function checks to be sure this storage
                 location pointer is not NULL.

         This function returns a positive value if a byte was successfully
         read from the serial port.  A zero or negative value indicates that
         the read attempt failed.
      \end{verbatim}
      
   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.getb, and byte are initialized with non-NULL values.
   
   Written by Dana Swift
*/
int pgetb(const struct SerialPort *port, unsigned char *byte, time_t sec)
{
   /* define the logging signature */
   static cc FuncName[] = "pgetb()";
   
   /* initialize the return value of this function */
   int status=-1;

   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pgetb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL pgetb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
 
   /* validate the byte storage */
   else if (!byte)
   {
      /* create the message */
      static cc msg[]="NULL pointer to byte.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* get one byte from the serial port */
   else
   {
      /* record the current time */
      time_t To=0,T=To;

      /* initialize the byte */
      *byte=0;
      
      /* use serial port primitive to get a byte from the serial port */
      do {status=port->getb(byte); if (status<=0) {T=time(NULL); if (!To) {To=T;}}}

      /* terminate the attempt if successful or else if timeout period has expired */
      while (status<=0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec);
   }

   return status;
}

/*------------------------------------------------------------------------*/
/* function to read a string from the serial port                         */
/*------------------------------------------------------------------------*/
/**
   This function reads (and stores in a buffer) bytes from the serial port
   until one of two termination criteria are satisfied.

      \begin{verbatim}
      1) A specified maximum number of bytes are read.  This criteria
         prevents buffer overflow.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if insufficient data are
         available after a specified number of seconds.
      \end{verbatim}

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.getb, and buf are initialized with non-NULL values and that
   the maximum buffer size is strictly positive.

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         size....The maximum number of bytes that will be read from the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to read bytes from the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>0.
         
      output:

         buf.....The buffer into which the bytes that are read from the
                 serial port will be stored.  This buffer must be at least
                 (size+1) bytes.

         This function returns the number of bytes in the buffer on exit from
         this function.
         
      \end{verbatim}
  
   Written by Dana Swift
*/
int pgetbuf(const struct SerialPort *port, void *buf, int size, time_t sec)
{
   /* define the logging signature */
   static cc FuncName[] = "pgetbuf()";
   
   /* initialize the number of bytes stored in buffer */
   int n=-1;
   
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's getb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL getb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
 
   /* validate the byte storage */
   else if (!buf) 
   {
      /* create the message */
      static cc msg[]="NULL pointer to buffer.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate that the buffer size as strictly positive */
   else if (size<=0)
   {
      /* create the message */
      static cc msg[]="Invalid buffer size.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the timeout period as non-negative */
   else if (sec<0)
   {
      /* create the message */
      static cc format[]="Time-out period [%ld sec] not allowed - must be non-negative.\n";

      /* log the message */
      LogEntry(FuncName,format,sec);
   }

   /* read the buffer from the serial port */
   else
   {
      /* record the current time */
      time_t To=0,T=To;

      /* effect a change of type and initialize the buffer with 0x00 */
      unsigned char *s=buf; *s=0;
      
      /* initialize the byte counter and the buffer */
      n=0; s[0]=0;
      
      do
      {
         /* attempt to read the next byte from the serial port */
         if (port->getb(s+n)>0) {n++; s[n]=0;}

         else
         {
            /* get the current time */
            T=time(NULL); if (!To) {To=T;}

            /* check time-related exit criteria */
            if (!(T>=0 && To>=0 && sec>=0 && difftime(T,To)<=sec)) break;
         }
      }

      /* check termination criteria */
      while (n<size);
   } 
   
   return n;
}

/*------------------------------------------------------------------------*/
/* function to read a string from the serial port                         */
/*------------------------------------------------------------------------*/
/**
   This function reads (and stores in a buffer) bytes from the serial port
   until one of three termination criteria are satisfied.

      \begin{verbatim}
      1) A specified termination string is read.  For example, if the
         termination string is "\r\n" then once this string is read from the
         serial port the function returns the string read up to that
         point.  The termination string itself is discarded.

      2) A specified maximum number of bytes are read.  This criteria
         prevents buffer overflow.

      3) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if insufficient data are
         available after a specified number of seconds.
      \end{verbatim}

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.getb, buf, and trm are initialized with non-NULL values and
   that the maximum buffer size is strictly positive.

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         size....The maximum number of bytes that will be read from the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
j                 function will attempt to read bytes from the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>0.

         trm.....The termination string.  For example, if the termination
                 string is "\r\n" then once this string is read from the
                 serial port the function returns the string read up to that
                 point.  The termination string is discarded.
         
      output:

         buf.....The buffer into which the bytes that are read from the
                 serial port will be stored.  This buffer must be at least
                 (size+1) bytes.  Although the termination string (trm) is
                 not returned, the buffer must be large enough to contain
                 all bytes read including the termination string.

         This function returns the number of bytes read from the serial port
         including the termination string (if a termination string was
         read).
         
      \end{verbatim}
  
   Written by Dana Swift
*/
int pgets(const struct SerialPort *port, char *buf, int size, time_t sec, const char *trm)
{
   /* define the logging signature */
   static cc FuncName[] = "pgets()";
   
   /* initialize the number of bytes stored in buffer */
   int n=-1;

   /* validate the termination string */
   if (!trm) trm="\n";
   
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's getb() function */
   else if (!port->getb)
   {
      /* create the message */
      static cc msg[]="NULL getb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   /* validate the byte storage */
   else if (!buf)
   {
      /* create the message */
      static cc msg[]="NULL pointer to buffer.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate that the buffer size as strictly positive */
   else if (size<=0)
   {
      /* create the message */
      static cc msg[]="Invalid buffer size.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the timeout period as non-negative */
   else if (sec<0) 
   {
      /* create the message */
      static cc format[]="Time-out period [%ld sec] not allowed - must be non-negative.\n";

      /* log the message */
      LogEntry(FuncName,format,sec);
   }

   /* read the buffer from the serial port */
   else
   {
      /* compute the length of the termination string */
      int trmlen=strlen(trm);

      /* boolean flag that is asserted when termination string found */
      int trm_found=0;

      /* record the current time */
      time_t To=0,T=To;
      
      /* initialize the byte counter and the buffer */
      n=0; buf[0]=0;

      do
      {
         /* attempt to read the next byte from the serial port */
         if (port->getb((unsigned char *)(buf+n))>0)
         {
            /* increment the byte count and re-terminate the buffer */
            n++; buf[n]=0;

            /* check for the line terminator string */
            if (n>=trmlen && !strcmp(buf+n-trmlen,trm))
            {
               /* remove the termination string from the buffer */
               trm_found=1; buf[n-trmlen]=0;
            }
         }
         
         /* recompute time-criteria for loop termination */
         else
         {
            T=time(NULL); if (!To) {To=T;}
            if (!(T>=0 && To>=0 && sec>=0 && difftime(T,To)<=sec)) break;
         } 
      }

      /* check termination criteria */
      while (n<size && !trm_found);
   } 
   
   return n;
}

/*------------------------------------------------------------------------*/
/* function to put a byte to the serial port                              */
/*------------------------------------------------------------------------*/
/**
   This function is designed to extract a single byte from a serial port or
   else time-out and return after a specified number of seconds.  A time-out
   period measured in milliseconds would have been preferred but the ANSI C
   library does not provide for timing with better resolution than 1 second.
   As portability was a major design criteria, a time-out period measured in
   seconds was accepted.  

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         byte....The byte to be written to the serial port.
                 
         sec.....This function will return immediately upon writing a byte
                 from the serial port.  If no byte can be written after the
                 specified number of seconds then this function will return
                 without writing the byte.  Due to the limited (ie., 1 sec)
                 resolution of the time() function, the actuall timeout
                 period will be somewhere in the semiclosed interval
                 [sec,sec+1) if sec>0.  If sec<=0 then this function will
                 attempt to write a byte to the serial port but return
                 immediately if the attempt fails.
      
      output:

         This function returns a positive value if a byte was successfully
         written to the serial port.  A zero or negative value indicates that
         the write attempt failed.
      \end{verbatim}
      
   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.getb, and byte are initialized with non-NULL values.
   
   Written by Dana Swift
*/
int pputb(const struct SerialPort *port,unsigned char byte, time_t sec)
{
   /* define the logging signature */
   static cc FuncName[] = "pputb()";
   
   /* initialize the return value of this function */
   int status=-1;

   /* validate the SerialPort object */
   if (!port) 
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pputb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL pputb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's obytes() function */
   else if (!port->obytes)
   {
      /* create the message */
      static cc msg[]="NULL obytes() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's oflush() function */
   else if (!port->oflush)
   {
      /* create the message */
      static cc msg[]="NULL oflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* get one byte from the serial port */
   else
   {
      /* record the current time */
      time_t To=0,T=To;

      /* use serial port primitive to put a byte to the serial port */
      do {status=port->putb(byte); if (status<=0) {T=time(NULL); if (!To) {To=T;}}}

      /* terminate the attempt if successful or else if timeout period has expired */
      while (status<=0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec);
       
      /* check to see if the output buffer needs to be drained and monitored */
      if (sec>0 && status>0 && port->obytes()>0)
      {
         do {T=time(NULL); if (!To) {To=T;}}

         /* terminate the attempt if successful or else if timeout period has expired */
         while (port->obytes()>0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec);

         /* if any bytes remain in the output buffer then flush them */
         if (port->obytes()>0) {status=0; port->oflush();}
      }
   }

   return status;
}

/*------------------------------------------------------------------------*/
/* function to write a buffer to the serial port                          */
/*------------------------------------------------------------------------*/
/**
   This function writes bytes to the serial port until one of two
   termination criteria are satisfied.

      \begin{verbatim}
      1) The specified maximum number of bytes are written.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if the serial port is not
         writable for whatever reason.
      \end{verbatim}

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.putb, and buf are initialized with non-NULL values and that
   the buffer size and time-out period are non-negative.

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         buf.....The buffer containing the bytes to write to the serial port.

         size....The maximum number of bytes that will be written to the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to write bytes to the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>=0.
         
      output:

         This function returns the number of bytes written to the serial port.
         
      \end{verbatim}
  
   Written by Dana Swift
*/
int pputbuf(const struct SerialPort *port, const void *buf, int size, time_t sec)
{
   /* define the logging signature */
   static cc FuncName[] = "pputbuf()";
   
   /* initialize the number of bytes stored in buffer */
   int n=-1;
   
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's putb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL putb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's obytes() function */
   else if (!port->obytes)
   {
      /* create the message */
      static cc msg[]="NULL obytes() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's oflush() function */
   else if (!port->oflush)
   {
      /* create the message */
      static cc msg[]="NULL oflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   /* validate the buffer pointer */
   else if (!buf)
   {
      /* create the message */
      static cc msg[]="NULL pointer to buffer.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
 
   /* validate that the buffer size as strictly positive */
   else if (size<0) 
   {
      /* create the message */
      static cc msg[]="Invalid buffer size.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* make sure the time-out period is non-negative */
   else if (sec<0)
   {
      /* create the message */
      static cc msg[]="Time-out period must be non-negative.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* write the buffer to the serial port */
   else
   {
      /* effect a change of type */
      const unsigned char *s=buf;

      /* record the current time */
      time_t To=0,T=To;

      for (n=0; n<size;)
      {
         /* attempt to write the next byte from the serial port */
         if (port->putb(s[n])>0) {n++;}
         
         else
         {
            /* get the current time */
            T=time(NULL); if (!To) {To=T;}

            /* check termination criteria */
            if (T<0 || To<0 || difftime(T,To)>sec) break;
         }
      }
      
      /* check to see if the output buffer needs to be drained and monitored */
      if (sec>0 && port->obytes()>0)
      {
         /* zero bytes in the output buffer indicates success */
         do {T=time(NULL); if (!To) {To=T;}}

         /* terminate the attempt if successful or else if timeout period has expired */
         while (port->obytes()>0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec);

         /* flush any remaining bytes from the output buffer */
         if (port->obytes()>0) {n-=port->obytes(); port->oflush(); if (n<0) n=0;}
      }
   } 
   
   return n;
}

/*------------------------------------------------------------------------*/
/* function to write a string to the serial port                          */
/*------------------------------------------------------------------------*/
/**
   This function writes bytes to the serial port until one of two
   termination criteria are satisfied.

      \begin{verbatim}
      1) The whole buffer plus termination string are written to the serial
         port.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if the serial port is not
         writable for whatever reason.
      \end{verbatim}

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.putb, buf, and trm are initialized with non-NULL values and that
   the time-out period is non-negative.

      \begin{verbatim}
      input:

         port....A structure that contains pointers to machine dependent
                 primitive IO functions.  See the comment section of the
                 SerialPort structure for details.  The function checks to
                 be sure this pointer is not NULL.

         buf.....The buffer containing the NULL terminated string to write
                 to the serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to write bytes to the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>=0.

         trm.....The string terminator to write to the serial port.  The
                 string terminator is initialized to "\r\n" if a NULL
                 pointer is passed to this function.
                 
      output:

         This function returns the number of bytes written to the serial
         port (including the terminator string).
         
      \end{verbatim}
  
   Written by Dana Swift
*/
int pputs(const struct SerialPort *port, const char *buf, time_t sec, const char *trm)
{
   /* define the logging signature */
   static cc FuncName[] = "pputs()";
   
   int n=-1;
   
   /* validate the string terminator */
   if (!trm) trm="\r\n";
      
   /* validate the SerialPort object */
   if (!port)
   {
      /* create the message */
      static cc msg[]="NULL serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's putb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL putb() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   /* validate the serial port's obytes() function */
   else if (!port->obytes)
   {
      /* create the message */
      static cc msg[]="NULL obytes() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }
   
   /* validate the serial port's oflush() function */
   else if (!port->oflush)
   {
      /* create the message */
      static cc msg[]="NULL oflush() function for serial port.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* validate the string pointer */
   else if (!buf)
   {
      /* create the message */
      static cc msg[]="NULL pointer to buffer.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* make sure the time-out period is non-negative */
   else if (sec<0)
   {
      /* create the message */
      static cc msg[]="Time-out period must be non-negative.\n";

      /* log the message */
      LogEntry(FuncName,msg);
   }

   /* write the string to the serial port */
   else
   {
      /* compute the length of the buffer and the termination string */
      int k, buflen=strlen(buf), trmlen=strlen(trm);

      /* record the current time */
      time_t To=0,T=To;

      for (n=0, k=0; n<buflen || k<trmlen;)
      {
         /* attempt to write the next byte of the buffer to the serial port */
         if (n<buflen && port->putb((unsigned char)buf[n])>0) {n++;}

         /* attempt to write the next byte of the termination string to the serial port */
         else if (k<trmlen && port->putb((unsigned char)trm[k])>0) {k++; n++;}
            
         else
         {
            /* get the current time */
            T=time(NULL); if (!To) {To=T;}

            /* check termination criteria */
            if (T<0 || To<0 || difftime(T,To)>sec) break;
         }
      }
      
      /* check to see if the output buffer needs to be drained and monitored */
      if (sec>0 && port->obytes()>0)
      {
         /* zero bytes in the output buffer indicates success */
         do {T=time(NULL); if (!To) {To=T;}}

         /* terminate the attempt if successful or else if timeout period has expired */
         while (port->obytes()>0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec);

         /* flush any remaining bytes from the output buffer */
         if (port->obytes()>0) {n-=port->obytes(); port->oflush(); if (n<0) n=0;}
      }
   }
   
   return n;
}
