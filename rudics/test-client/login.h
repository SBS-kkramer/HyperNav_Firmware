#ifndef LOGIN_H
#define LOGIN_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/login.h#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: login.c,v $
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define loginChangeLog "$RCSfile: login.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include "serial.h"

/* function prototypes for external functions */
int login(const struct SerialPort *port, const char *user, const char *pwd);
int logout(const struct SerialPort *port);

#endif /* LOGIN_H */
