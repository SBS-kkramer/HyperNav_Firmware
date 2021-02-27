#ifndef SOCKET_H
#define SOCKET_H

#include "serial.h"

int OpenRudicsPort(const char *hostname);
int CloseRudicsPort(void);

extern struct SerialPort tcp;
extern time_t RefTime;

#endif /* SOCKET_H */
