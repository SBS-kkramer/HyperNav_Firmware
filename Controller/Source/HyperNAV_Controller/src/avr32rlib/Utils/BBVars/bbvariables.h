/*! \file bbvariables.h**********************************************************
 *
 * \brief Battery backed variables API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-22
 *
  ***************************************************************************/


#ifndef BBVARIABLES_H_
#define BBVARIABLES_H_


#include "compiler.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define 	BBV_OK		0
#define 	BBV_FAIL	-1


//*****************************************************************************
// Exported functions
//*****************************************************************************

/* \brief Read battery-backed byte variable
 * @param 	addr 	Address to read from.
 * @param 	data	OUTPUT: Data read.
 * @param 	size	Number of bytes to read from 'addr'
 * @return BBV_OK: Succesful read, BBV_FAIL: Error while reading
 */
S16 bbv_read(U8 addr, U8* data, U8 size);


/* \brief Write battery-backed byte variable
 * @param 	addr 	Address to write to.
 * @param 	data	Data to write.
 * @param 	size	Number of bytes to write from 'addr'
 * @return BBV_OK: Succesful write, BBV_FAIL: Error while writing
 */
S16 bbv_write(U8 addr, U8* data, U8 size);





#endif /* BBVARIABLES_H_ */
