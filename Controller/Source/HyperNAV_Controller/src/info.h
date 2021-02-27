/*
 * info.h
 *
 *  Created on: Dec 14, 2010
 *      Author: plache
 */

#ifndef INFO_H_
#define INFO_H_

# include "compiler.h"
# include "files.h"

/*
 *!	\brief	Write sensor identifying information to either file or usart
 *! @param	write_to_file	flag to indicate if output to usart or file
 *! @param	fh				if write_to_file is true, file to be written to.
 *!							otherwise, ignore fh.
 */
void Info_SensorID ( fHandler_t* fh, char* prefix );

/*
 *!	\brief	Write sensor state information to either file or usart
 *! @param	write_to_file	flag to indicate if output to usart or file
 *! @param	fh				if write_to_file is true, file to be written to.
 *!							otherwise, ignore fh.
 */
bool Info_SensorState ( fHandler_t* fh, char* prefix );

/*
 *!	\brief	Write sensor device state to either file or usart
 *! @param	write_to_file	flag to indicate if output to usart or file
 *! @param	fh				if write_to_file is true, file to be written to.
 *!							otherwise, ignore fh.
 */
bool Info_SensorDevices ( fHandler_t* fh, char* prefix );

/*
 *!	\brief	Write processing parameters to either file or usart
 *! @param	write_to_file	flag to indicate if output to usart or file
 *! @param	fh				if write_to_file is true, file to be written to.
 *!							otherwise, ignore fh.
 */
void Info_ProcessingParameters ( fHandler_t* fh, char* prefix );

//void OutputHeaderInfo ( bool write_to_file, fHandler_t* fh );

//void DisplayInitMsg ( void );

#endif /* INFO_H_ */
