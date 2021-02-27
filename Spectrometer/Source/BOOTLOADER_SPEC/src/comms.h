/*
 * comms.h
 *
 *  Created on: Dec 7, 2010
 *  Author: scott
 *  Modified on:
 *  2011-10-4, DAS - Added USB functionality
 *	2016-10-17, SF: Porting to HyperNAV spectrometer board.  Removing USB support.
 */

#ifndef COMMS_H_
#define COMMS_H_

// Bootloader serial interface modes (RS-232 or USB Virtual COM)
typedef enum {MODE_RS232, MODE_USB} serialMode_t;


#define COMMS_SUCCESS		1
#define COMMS_ERROR			0
#define COMMS_BUFFER_EMPTY	-1



//! \brief Initialize serial interface
//! @par mode	Interface mode (RS232 or USB)
void COMMS_init(serialMode_t mode);


//! \brief Read a single byte from the serial interface if available
int COMMS_getc(void);


//! \brief Output a single character to the serial interface
int COMMS_putchar(int c);


//! \brief Output a message through serial interface
void COMMS_writeLine(char* line);


//! \brief Print an unsigned long to the serial interface
void COMMS_printULong(unsigned long ul);


//! \brief Print a hex char to the serial interface
void COMMS_printHexChar(char c);

//! \brief Init usart (no interrupts)
void COMMS_initUSART(void);

#endif /* COMMS_H_ */
