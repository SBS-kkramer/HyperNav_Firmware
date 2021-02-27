/*! \file ads8344.c *************************************************************
 *
 * \brief ADS8344 - 16-bit, 8-channel ADC Driver
 *
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-24
 *
 **********************************************************************************/

#include "compiler.h"
#include "dbg.h"
#include "spi.h"
#include "ads8344.h"
#include "ads8344_cfg.h"


// DS8344 Control Byte Definitions
#define ADS8344_START_BIT		0x80
#define ADS8344_SGL				0x04
#define ADS8344_INTERNAL_CLK	0x02
#define ADS8344_EXTERNAL_CLK	0x03


//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
static S16 initClockMode(Bool toExternal);


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

/* \brief Initialize driver
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error initializing driver.
 */
S16 ads8344_init(U32 fPBA)
{

	//!@NOTE To overcome the fact that when using external clock and reading a single channel
	//! at least 25 clocks are needed we will read each channel with three 9-bit transfers resulting
	//! in 27 clocks. In the last 2 clocks the ADC will drive the MISO low (zero filling). These last
	//! 2 bits will be ignored.


	// Set up internal clock mode
	return initClockMode(TRUE);
}

#if 0 // 9-bit transfers version

/* \brief Sample a single channel
 * @param	channel	Channel to sample (0-7)
 * @param	slg		If TRUE sample a single channel, if FALSE sample in differential mode
 * @param	pd		If TRUE puts the converter in low-power mode between conversions.
 * @param	sample	OUTPUT: 16-bit raw count sample value
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error sampling
 */
S16 ads8344_sample(U8 channel, Bool sgl, Bool pd, U16* sample)
{
	U16 dataIO;
	U16 MSB;	// Upper 9 bits of conversion
	U16 LSB;	// Lower 7 bits of conversion + 2 zero-filling bits

	// Channel to mask table (See Table I in ADC datasheet)
	U8 channelMask[8] = {0x00, 0x40, 0x10, 0x50, 0x20, 0x60, 0x30, 0x70};

	if(channel > 7)
		return ADS8344_FAIL;

	// Assemble control byte
	dataIO = 0;
	dataIO |= (ADS8344_START_BIT | ((sgl)?ADS8344_SGL:0) | ((pd)?0:ADS8344_EXTERNAL_CLK) | channelMask[channel]);

	// Shift 1-bit to account for 9-bit transmission. The last bit will be a 0.
	dataIO <<= 1;

	// Select chip
	spi_selectChip(ADS8344_SPI, ADS8344_NPCS);

	// Write Control Byte
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &dataIO);	// Read to prevent an SPI overrun

	// At this point the next incoming bit will be the conversion MSB.
	// Read 18 bits from ADC.
	dataIO = 0;
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &MSB);
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &LSB);
	LSB >>= 2;		// Discard last 2-bits (zero-filling)

	// Unselect chip
	spi_unselectChip(ADS8344_SPI, ADS8344_NPCS);

	if(spi_getStatus(ADS8344_SPI) != SPI_OK)
	{
		spi_reset(ADS8344_SPI);
		return ADS8344_FAIL;
	}
	else
	{
		// Assemble a 16-bit sample from MSB and LSB blocks
		*sample = 0;
		*sample |= MSB;
		*sample <<= 7;
		*sample |= LSB;
		return ADS8344_OK;
	}
}

#else // 8-bit transfers version


/* \brief Sample a single channel
 * @param	channel	Channel to sample (0-7)
 * @param	sgl		If TRUE sample a single channel, if FALSE sample in differential mode
 * @param	pd		If TRUE puts the converter in low-power mode between conversions.
 * @param	sample	OUTPUT: 16-bit raw count sample value
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error sampling
 */
S16 ads8344_sample(U8 channel, Bool sgl, Bool pd, U16* sample)
{

	// Conversion is 16-bit: MSB[7-bits]LSB[8-bits]LLSB[1-bit]

	U16 dataIO;
	U16 MSB;	// Conversion 7-bit MSB
	U16 LSB;	// Conversion 8-bit LSB
	U16 LLSB;	// Conversion 1-bit LSB

	// Channel to mask table (See Table I in ADC datasheet)
	U8 channelMask[8] = {0x00, 0x40, 0x10, 0x50, 0x20, 0x60, 0x30, 0x70};

	if(channel > 7)
		return ADS8344_FAIL;

	// Assemble control byte
	dataIO = 0;
	dataIO |= (ADS8344_START_BIT | ((sgl)?ADS8344_SGL:0) | ((pd)?0:ADS8344_EXTERNAL_CLK) | channelMask[channel]);

	// Select chip
	spi_selectChip(ADS8344_SPI, ADS8344_NPCS);

	// Write Control Byte
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &dataIO);	// Read to prevent an SPI overrun

	// At this point the next incoming bit will be a 0 (See Figure 3 in ADC datasheet)
	spi_write(ADS8344_SPI, 0);
	spi_read(ADS8344_SPI, &MSB);		// Read the leading 0 and the 7 most significative bits
	spi_write(ADS8344_SPI, 0);
	spi_read(ADS8344_SPI, &LSB);
	spi_write(ADS8344_SPI, 0);
	spi_read(ADS8344_SPI, &LLSB);

	// Unselect chip
	spi_unselectChip(ADS8344_SPI, ADS8344_NPCS);

	if(spi_getStatus(ADS8344_SPI) != SPI_OK)
	{
		spi_reset(ADS8344_SPI);
		return ADS8344_FAIL;
	}
	else
	{
		// Assemble a 16-bit sample from MSB and LSB blocks
		*sample = 0;
		*sample |= (MSB << 9) | (LSB << 1);
		*sample |= LLSB >> 7;
		return ADS8344_OK;
	}
}
#endif



/* \brief Convert ADC counts to a volts value using ADC reference voltage
 * @param 	counts	ADS 16-bit raw counts
 * @return	A voltage reading (V).
 */
F32 ads8344_counts2Volts(U16 counts)
{
	return (ADS8344_VREF * (counts/65536.0));
}

//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

//! \brief Initialize clock mode
//! @param toExternal	If TRUE clock mode is initialized to external, else is initialized to internal.
//! @return ADS8344_OK: Success, ADS8344_FAIL: Error
static S16 initClockMode(Bool toExternal)
{
	U16 dataIO;

	// Assemble control byte
	dataIO = 0;
	if(toExternal)
		dataIO |= (ADS8344_START_BIT | ADS8344_SGL | ADS8344_EXTERNAL_CLK);
	else
		dataIO |= (ADS8344_START_BIT | ADS8344_SGL | ADS8344_INTERNAL_CLK);

	// Shift 1-bit to account for 9-bit transmission. The last bit will be a 0.
	dataIO <<= 1;

	// Select chip
	spi_selectChip(ADS8344_SPI, ADS8344_NPCS);

	// Write Control Byte
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &dataIO);	// Read to prevent an SPI overrun

	// Generate 18 additional clock cycles (ignore all incoming data)
	dataIO = 0;
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &dataIO);	// Read to prevent an SPI overrun
	dataIO = 0;
	spi_write(ADS8344_SPI, dataIO);
	spi_read(ADS8344_SPI, &dataIO);	// Read to prevent an SPI overrun

	// Unselect chip
	spi_unselectChip(ADS8344_SPI, ADS8344_NPCS);

	if(spi_getStatus(ADS8344_SPI) != SPI_OK)
	{
		spi_reset(ADS8344_SPI);
		return ADS8344_FAIL;
	}
	else
		return ADS8344_OK;

}




