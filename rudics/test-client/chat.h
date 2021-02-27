#ifndef CHAT_H
#define CHAT_H

#include <time.h>
#include "serial.h"

/* function prototype */
int chat(const struct SerialPort *port, const char *cmd, const char *expect, time_t sec, const char *trm);

#endif /* CHAT_H */
