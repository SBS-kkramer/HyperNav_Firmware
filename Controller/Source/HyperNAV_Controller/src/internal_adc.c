/*
 * internal_adc.c
 *
 * Created: 10/10/2016 10:47:55 PM
 *  Author: sfeener
 */ 

// these functions based on ASF adc.c, .h
// having problems adding new ASF functionality




#include <avr32/io.h>
#include "compiler.h"
#include "internal_adc.h"

/** \brief Configure ADC. Mandatory to call.
 * If not called, ADC channels will have side effects
 *
 * \param *adc Base address of the ADC
 */
void adc_configure(volatile avr32_adc_t *adc)
{
	Assert( adc != NULL );

#ifdef USE_ADC_8_BITS
	adc->mr |= 1 << AVR32_ADC_LOWRES_OFFSET;
#endif

	/* Set Sample/Hold time to max so that the ADC capacitor should be
	 * loaded entirely */
	adc->mr |= 0xF << AVR32_ADC_SHTIM_OFFSET;

	/* Set Startup to max so that the ADC capacitor should be loaded
	 * entirely */
	adc->mr |= 0x1F << AVR32_ADC_STARTUP_OFFSET;
}

/** \brief Start analog to digital conversion
 * \param *adc Base address of the ADC
 */
void adc_start(volatile avr32_adc_t *adc)
{
	Assert( adc != NULL );

	/* start conversion */
	adc->cr = AVR32_ADC_START_MASK;
}

/** \brief Enable channel
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to enable (0 to 7)
 */
void adc_enable(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB ); /* check if channel exist
	                                              **/

	/* enable channel */
	adc->cher = (1 << channel);
}

/** \brief Disable channel
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to disable (0 to 7)
 */
void adc_disable(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB ); /* check if channel exist
	                                              **/

	if (adc_get_status(adc, channel) == true) {
		/* disable channel */
		adc->chdr |= (1 << channel);
	}
}

/** \brief Get channel 0 to 7 status
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to handle (0 to 7)
 * \return bool      true if channel is enabled
 *                   false if channel is disabled
 */
bool adc_get_status(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB ); /* check if channel exist
	                                              **/

	return ((adc->chsr & (1 << channel)) ? true : false);
}

/** \brief Check channel conversion status
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to check (0 to 7)
 * \return bool      true if conversion not running
 *                   false if conversion running
 */
bool adc_check_eoc(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB ); /* check if channel exist
	                                              **/

	/* get SR register : EOC bit for channel */
	return ((adc->sr & (1 << channel)) ? true : false);
}

/** \brief Check channel conversion overrun error
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to check (0 to 7)
 * \return bool      FAIL if an error occurred
 *                   PASS if no error occurred
 */
bool adc_check_ovr(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB ); /* check if channel exist
	                                              **/

	/* get SR register : OVR bit for channel */
	return ((adc->sr & (1 << (channel + 8))) ? FAIL : PASS);
}

/** \brief Get channel value
 *
 * \param *adc Base address of the ADC
 * \param  channel   channel to handle (0 to 7)
 * \return The value acquired (unsigned long)
 */
uint32_t adc_get_value(volatile avr32_adc_t *adc, uint16_t channel)
{
	Assert( adc != NULL );
	Assert( channel <= AVR32_ADC_CHANNELS_MSB );

	/* wait for end of conversion */
	while (adc_check_eoc(adc, channel) != true) {
	}

	return *((uint32_t *)((&(adc->cdr0)) + channel));
}

/** \brief Wait for the next converted data and return its value
 *
 * \param *adc Base address of the ADC
 * \return The latest converted value (unsigned long)
 */
uint32_t adc_get_latest_value(volatile avr32_adc_t *adc)
{
	Assert( adc != NULL );

	/* Wait for the data ready flag */
	while ((adc->sr & AVR32_ADC_DRDY_MASK) != AVR32_ADC_DRDY_MASK) {
	}

	return adc->lcdr;
}
