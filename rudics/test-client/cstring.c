#ifndef CSTRING_H
#define CSTRING_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/cstring.c#1 $
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
 * $Log: cstring.c,v $
 * Revision 1.3  2006/04/21 13:37:39  swift
 * Changed copyright attribute.
 *
 * Revision 1.2  2005/02/27 01:31:39  swift
 * Inserted a prototype for vnsprintf() rather than including <snprintf.h>.
 *
 * Revision 1.1  2004/12/09 01:20:44  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * \end{verbatim}
*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define cstringChangeLog "$RCSfile: cstring.c,v $ $Revision: #1 $ $Date: 2016/01/20 $"

#include <stddef.h>

/* function prototypes */
char *cstrncpy(char *dest, /*code*/ const char *src, size_t n);
int   csnprintf(char *str, size_t size, /*code*/ const char *format, ...);

#endif /* CSTRING_H */

#include <stdarg.h>

int vsnprintf(char *str, size_t size, const char *format, va_list ap);


/*------------------------------------------------------------------------*/
/* function to copy a string from code (ROM) memory                       */
/*------------------------------------------------------------------------*/
/**
   This function reproduces the functionality of the C standard library
   function strncpy() except that it allows its second argument to be in
   code (ROM) memory.  It copies at most n bytes of the string pointed to by
   src (including the terminating NULL character) to the array pointed to by
   dest.  The destination string will be NULL terminated and must be large
   enough to receive at least n bytes.
*/
char *cstrncpy(char *dest, /*code*/ const char *src, size_t n)
{
   size_t i;
   char *ptr=dest;

   /* copy the string */
   for (i=1; i<n && (*src); i++) {*dest++ = *src++;}

   /* terminate the string */
   *dest=0;

   return ptr;
}

/*------------------------------------------------------------------------*/
/* function to print to a string buffer                                   */
/*------------------------------------------------------------------------*/
/**
   This function performs the same services as sprintf() except that it
   writes no more than 'size' bytes (including the terminating NULL) in
   order to protect against writing beyond the end of the character buffer.

      \begin{verbatim}
      input:
         size.....The size of the character buffer 'str' into which the
                  formatted output will be written.  Nothing will be written
                  beyond 'str[size-1]'.
         format...The conversion specifier for formatting output.  See
                  printf() documentation for details.

      output:
         
         str......The character buffer into which the formatted output is
                  written.

         This function returns the number of characters that would have been
         written to the string buffer if the string buffer were sufficiently
         large.  This is in accordance with the C99 standard.
      \end{verbatim}
*/
int csnprintf(char *str, size_t size, /*code*/ const char *format, ...)
{
   int n=0; char fmt[256];
   va_list arg_pointer;
   cstrncpy(fmt,format,sizeof(fmt));
   va_start(arg_pointer,format);
   n=vsnprintf(str, size, fmt, arg_pointer);
   va_end(arg_pointer);
   return n;
}
