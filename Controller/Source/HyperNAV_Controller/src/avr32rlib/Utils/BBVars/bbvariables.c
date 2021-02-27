/*! \file bbvariables.c *************************************************************
 *
 * \brief Battery-backed variables support.
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2010-11-22
 *
 **********************************************************************************/

#include "compiler.h"
#include "ds3234.h"
#include "bbvariables.h"
#include "avr32rerrno.h"

//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

/* \brief Read battery-backed byte variable
 */
S16 bbv_read(U8 addr, U8* data, U8 size)
{
	U8 i;
	S16 ret = BBV_OK;

	for(i=0; i < size; i++)
	{
		if(ds3234_readSRAMData(addr+i, data+i) != DS3234_OK)
		{
			avr32rerrno = EBBV;
			ret = BBV_FAIL;
			break;
		}
	}

	return ret;
}


/* \brief Write battery-backed byte variable
 */
S16 bbv_write(U8 addr, U8* data, U8 size)
{
	U8 i;
	S16 ret = BBV_OK;

	for(i=0; i < size; i++)
	{
		if(ds3234_writeSRAMData(addr+i, data[i]) != DS3234_OK)
		{
			avr32rerrno = EBBV;
			ret = BBV_FAIL;
			break;
		}
	}

	return ret;
}

