#ifndef PKT_H
#define PKT_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/pkt.h#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: pkt.c,v $
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.2  2002/10/08 23:52:36  swift
 * Combined header file into source file.
 *
 * Revision 1.1  2002/05/07 22:16:38  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define PktChangeLog "$RCSfile: pkt.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

unsigned char PktMonitor(int status);
unsigned char PktType(void);
void Pkt1k(void);
void Pkt128b(void);

#endif /* PKT_H */
