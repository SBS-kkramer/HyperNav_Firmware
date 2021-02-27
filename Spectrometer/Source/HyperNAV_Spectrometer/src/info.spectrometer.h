/*
 * info.h
 *
 *  Created on: Dec 14, 2010
 *      Author: plache
 */

#ifndef _INFO_SPECTROMETER_H_
#define _INFO_SPECTROMETER_H_

# include "compiler.h"

/*
 *!	\brief	Write sensor identifying information
 */
void Info_SensorID ( void );

/*
 *!	\brief	Write sensor state information
 */
bool Info_SensorState ( void );

/*
 *!	\brief	Write sensor device state
 */
bool Info_SensorDevices ( void );

#endif /* INFO_SPECTROMETER_H_ */
