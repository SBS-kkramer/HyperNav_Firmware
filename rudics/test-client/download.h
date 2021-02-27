#ifndef DOWNLOAD_H
#define DOWNLOAD_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/download.h#1 $
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
 * $Log: download.c,v $
 * Revision 1.5  2006/04/21 13:45:42  swift
 * Changed copyright attribute.
 *
 * Revision 1.4  2005/06/27 15:15:37  swift
 * Added an additional bit of modem control if the download failed.
 *
 * Revision 1.3  2005/06/14 19:02:09  swift
 * Changed the return value of the download manager.
 *
 * Revision 1.2  2005/02/22 21:04:39  swift
 * Remove the temporary mission config file.
 *
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define downloadChangeLog "$RCSfile: download.c,v $ $Revision: #1 $ $Date: 2016/01/20 $"
 
#include "serial.h"

int RxConfig(const struct SerialPort *modem);

#endif /* DOWNLOAD_H */
