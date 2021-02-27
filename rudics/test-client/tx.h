#ifndef TX_H
#define TX_H

#include <stdio.h>
#include "serial.h"

/* prototypes for external functions */
long int Tx(const struct SerialPort *port,FILE *source);

#endif /* TX_H */
