/*!	\file	AD7677.h
 *
 *	\brief	AD7677 16-bit 1MSPS ADC driver
 *
 * Created: 7/2/2016 11:45:29 PM
 *  Author: sfeener
 */ 


#ifndef AD7677_H_
#define AD7677_H_

#include "compiler.h"
#include "components.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define AD7677_OK		0
#define AD7677_FAIL 	-1


//*****************************************************************************
// Exported functions
//*****************************************************************************


/* \brief Initialize driver
 * @return AD7677_OK: Success, AD7677_FAIL: Error initializing driver.
 */
S16 ad7677_InitGpio(void);

/* \brief Start operation
 * @return AD7677_OK: Success, AD7677_FAIL: Error initializing driver.
 */
S16 ad7677_Start( component_selection_t which );

/* \brief Stop operation
 * @return AD7677_OK: Success, AD7677_FAIL: Error initializing driver.
 */
S16 ad7677_StopBoth(void);

# if 0
/* \brief Convert ADC counts to a volts value using ADC reference voltage
 * @param 	counts	ADC 16-bit raw counts
 * @return	A voltage reading (V).
 */
F32 ad7677_counts2Volts(U16 counts);
# endif

#endif /* AD7677_H_ */