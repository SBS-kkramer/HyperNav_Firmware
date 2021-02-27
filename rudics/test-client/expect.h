#ifndef EXPECT_H
#define EXPECT_H

#include <time.h>
#include "serial.h"

/* function prototypes */
int expect(const struct SerialPort *port, const char *prompt, const char *response, time_t sec, const char *trm);

#endif /* EXPECT_H */
