/*! \file ads8344.h**********************************************************
 *
 * \brief ADS8344 - 16-bit, 8-channel ADC Driver
 *
 *
 * @author      Diego, Satlantic Inc.
 * @date        2010-11-24
 *
  ***************************************************************************/

#ifndef ADS8344_H_
#define ADS8344_H_

#include "compiler.h"

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define ADS8344_OK		0
#define ADS8344_FAIL 	-1


//*****************************************************************************
// Exported functions
//*****************************************************************************

/* \brief Initialize driver
 * @param fPBA	Peripheral Block 'A' clock frequency
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error initializing driver.
 */
S16 ads8344_init(U32 fPBA);


/* \brief Sample a single channel
 * @param	channel	Channel to sample (0-7)
 * @param	slg		If TRUE sample a single channel, if FALSE sample in differential mode
 * @param	pd		If TRUE puts the converter in low-power mode between conversions.
 * @param	sample	OUTPUT: 16-bit raw count sample value
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error sampling
 */
S16 ads8344_sample(U8 channel, Bool sgl, Bool pd, U16* sample);



/* \brief Convert ADC counts to a volts value using ADC reference voltage
 * @param 	counts	ADS 16-bit raw counts
 * @return	A voltage reading (V).
 */
F32 ads8344_counts2Volts(U16 counts);


#endif /* ADS8344_H_ */
