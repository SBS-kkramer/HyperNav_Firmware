/*! \file i2c_cfg.h**********************************************************
 *
 * \brief I2C Master GPIO bit-banging driver configuration file
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2011-1-28
 *
 *
 * 2016-10-09, SF: Added to HyperNAV Controller
  ***************************************************************************/


#ifndef I2C_CFG_H_
#define I2C_CFG_H_


#include "compiler.h"


/*! \name GPIO-I2C
 */
//! @{

// Pin mapping
//
#define I2C_SDA_PIN			AVR32_PIN_PA25
#define I2C_SCL_PIN			AVR32_PIN_PA26

// Timing constraints (in microseconds)
//
// Bus-free
#define I2C_TBUF	2
// Hold-time (repeated) start
#define I2C_THDSTA	1
// Set-up time for repeated start
#define I2C_TSUSTA	1
// Min clock low time
#define I2C_TLOW	2
// Min clock high time
#define I2C_THIGH	1
// Data hold time
#define I2C_THDDAT	1
// Set-up time for stop
#define I2C_TSUSTO	1
//! @}



#endif /* I2C_CFG_H_ */
