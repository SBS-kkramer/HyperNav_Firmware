#ifndef CSTRING_H
#define CSTRING_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/cstring.h#1 $
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
 * $Log: cstring.c,v $
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
