/*! \file ds18s20.c **********************************************************
 *
 * \brief DS18S20 1-Wire digital thermometer driver
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-25
 *
  ***************************************************************************/

#include "compiler.h"
#include "onewire.h"
#include "avr32rerrno.h"
#include "ds18s20.h"


/*
 * Define DS18S20 command set
 */
#define CONVERT_TEMP		0x44		// initiates temperature conversion
#define READ_SCRATCHPAD		0xBE		// reads the entire scratchpad
#define WRITE_SCRATCHPAD	0x4E		// writes data into scratchpad bytes 2 and 3
#define COPY_SCRATCHPAD		0x48		// copies Th and Tl data from scratchpad to EEPROM
#define RECALL_EE			0xB8		// copies Th and Tl data from EEPROM to Scratchpad
#define READ_POWER_SUPPLY	0xB4		// signals power supply mode to the master
#define MATCH_ROM			0x55		// match ROM ID command

/*
 * DS18S20 scratchpad map
 */
enum{T_LSB=0, T_MSB, TH, TL, RES0, RES1, COUNT_REMAIN, COUNT_PER_C, CRC};


//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
static F32 calcHiResTemp(U8* scratchPad);


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

/* \brief Start a temperature conversion on a sensor
 * @param	ROMID	ROM ID of the sensor that should start the conversion
 * @return DS18S20_OK: Success	DS18S20_FAIL: An error occurred
 */
S16 ds18s20_start(U64 ROMID)
{
	U8 i;
	Bool presencePulse;
	S16 ret;

	ret = ow_reset(&presencePulse) ;		// initialize bus

	if(ret != OW_OK || !presencePulse)
	{
		avr32rerrno = EOW;
		return DS18S20_FAIL;
	}

	ow_writeByte(MATCH_ROM);				// match ROM ID command
    for(i = 0; i <= 7; i++)		        	// send ROM ID contents
    	ow_writeByte(((U8*)&ROMID)[i]);

    ow_writeByteSPU(CONVERT_TEMP);     		// start temperature conversion (enable strong pull-up)

    return DS18S20_OK;
}



/* \brief Finish a temperature conversion on a sensor, i.e., retrieve conversion result
 * @param	ROMID	ROM ID of the sensor to be read from
 * @param	Temp	OUTPUT: Temperature in degrees Celsius
 * @note This function must be called at least 'DS18S20_TCONV_MS' after a conversion start
 * to obtain reliable data.
 * @return DS18S20_OK: Success	DS18S20_FAIL: An error occurred
 */
S16 ds18s20_finish(U64 ROMID, F32* T)
{
	U8 i;
	Bool presencePulse;
	S16 ret;
	U8 scratchPad[9];

	ret = ow_reset(&presencePulse) ;		// initialize bus

	if(ret != OW_OK || !presencePulse)
	{
		avr32rerrno = EOW;
		return DS18S20_FAIL;
	}

	*T = -100;								// initialize temperature to a riduculous value
	ow_writeByte(MATCH_ROM);				// match ROM ID command
	for(i = 0; i < 8; i++)		        	// send ROM ID contents
		ow_writeByte(((U8*)&ROMID)[i]);

	ow_writeByte(READ_SCRATCHPAD); 			// issue read scratchpad command
	for(i = 0; i < 9; i++)					// read 9 bytes from scratchpad
		if(ow_readByte(scratchPad+i) != OW_OK)
			break;

	// Check that read was successful
	if(i < 9)
	{
		ow_reset(&presencePulse);
		avr32rerrno = EOW;
		return DS18S20_FAIL;
	}

	ow_reset(&presencePulse);				// initialize bus
	*T = calcHiResTemp(scratchPad);			// calculate temperature from data in scratchpad

	return DS18S20_OK;
}



//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************
static F32 calcHiResTemp(U8* scratchPad)
{
	S16 tempLSB = scratchPad[T_LSB];
	U8 tempMSB = scratchPad[T_MSB];
	U8 countsPerDegreeC = scratchPad[COUNT_PER_C];
	U8 countsRemain = scratchPad[COUNT_REMAIN];

	// Read two's complement formatted temperature
	tempLSB >>= 1;	// Truncate temperature bit 0
	F32 tempRead = ((tempMSB)?-128:0)+ tempLSB;

	// Calculate higher res temperature (See DS18S20 datasheet, Section: "Operation-Measuring Temperature")
	return (tempRead - 0.25 + (countsPerDegreeC - countsRemain)/(F32)countsPerDegreeC);
}


