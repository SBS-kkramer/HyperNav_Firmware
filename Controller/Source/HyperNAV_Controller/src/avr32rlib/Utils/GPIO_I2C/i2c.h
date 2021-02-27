/*! \file i2c.h**********************************************************
 *
 * \brief I2C Master GPIO bit-banging driver API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-19
 *
  ***************************************************************************/

#ifndef I2C_H_
#define I2C_H_

#include "compiler.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

//! For byte read ack flag 
//! @note Do not change!
#define ACK		1
#define NACK	0

// R/W# flag
#define I2C_WRITE	0x00
#define I2C_READ	0x01

//*****************************************************************************
// Exported functions
//*****************************************************************************

/* \brief Initialize I2C driver.
 */
void I2C_init(U32 f_CPU);


/* \brief Generate a start condition
 */
void I2C_start(void);


/* \brief Generate a repeated start condition
 */
void I2C_rep_start(void);


/* \brief Put SCL line to Hi (Hi-Z)
 */
void I2C_SCLHi(void);


/* \brief Return state of SCL line
 */
Bool I2C_SCL(void);


/* \brief Write a byte to I2C bus
 * @param data	Byte to be sent
 * @param expectack A expect ack flag. If true will generate an extra clock for slave ACK bit.
 * @return TRUE: Transmission was acknowledged FALSE: Transmission was not acknwoledged
 */
Bool I2C_write(U8 data, Bool expectack);


/* \brief Read a byte from I2C bus
 * @param ack If true an ACK pulse is sent after byte is read. If false a NACK is sent after byte is read.
 * @return Byte read
 */
U8 I2C_read(Bool ack);


/* \brief Generate a stop condition
 */
void I2C_stop(void);





#endif /* I2C_H_ */
