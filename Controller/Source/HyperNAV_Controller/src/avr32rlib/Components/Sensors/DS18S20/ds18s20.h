/*! \file ds18s20.h**********************************************************
 *
 * \brief DS18S20 1-Wire digital thermometer driver API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-25
 *
  ***************************************************************************/
#ifndef DS18S20_H_
#define DS18S20_H_

#include "compiler.h"

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define DS18S20_OK		0
#define DS18S20_FAIL	-1


//*****************************************************************************
// Sensor data
//*****************************************************************************
#define DS18S20_TCONV_MS	750


//*****************************************************************************
// Exported functions
//*****************************************************************************

/* \brief Start a temperature conversion on a sensor
 * @param	ROMID	ROM ID of the sensor that should start the conversion
 * @return DS18S20_OK: Success	DS18S20_FAIL: An error occurred
 */
S16 ds18s20_start(U64 ROMID);


/* \brief Finish a temperature conversion on a sensor, i.e., retrieve conversion result
 * @param	ROMID	ROM ID of the sensor to be read from
 * @param	Temp	OUTPUT: Temperature in degrees Celsius
 * @note This function must be called at least 'DS18S20_TCONV_MS' after a conversion start
 * to obtain reliable data.
 * @return DS18S20_OK: Success	DS18S20_FAIL: An error occurred
 */
S16 ds18s20_finish(U64 ROMID, F32* T);


#endif /* DS18S20_H_ */
