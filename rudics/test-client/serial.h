#ifndef SERIAL_H
#define SERIAL_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/serial.h#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/* Copyright (C) 2003 Dana Swift
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
