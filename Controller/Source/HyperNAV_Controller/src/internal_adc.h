/*
 * internal_adc.h
 *
 * Created: 10/10/2016 10:46:56 PM
 *  Author: sfeener
 */ 


#ifndef INTERNAL_ADC_H_
#define INTERNAL_ADC_H_

/**
 * \defgroup group_avr32_drivers_adc ADC - Analog to Digital Converter
 *
 * Analog to Digital Converter is able to capture analog signals and transform
 * them
 * into digital format with 10-bit resolution.
 *
 * \{
 */

#include <avr32/io.h>
#include "stdint.h"
#include "compiler.h"

/* if using 8 bits for ADC, define this flag in your compiler options */
/** Max value for ADC resolution */
#ifdef USE_ADC_8_BITS
#  define ADC_MAX_VALUE    0xFF
#else
#  define ADC_MAX_VALUE    0x3FF
#endif

void adc_configure(volatile avr32_adc_t *adc);

void adc_start(volatile avr32_adc_t *adc);

void adc_enable(volatile avr32_adc_t *adc, uint16_t channel);

void adc_disable(volatile avr32_adc_t *adc, uint16_t channel);

bool adc_get_status(volatile avr32_adc_t *adc, uint16_t channel);

bool adc_check_eoc(volatile avr32_adc_t *adc, uint16_t channel);

bool adc_check_ovr(volatile avr32_adc_t *adc, uint16_t channel);

uint32_t adc_get_value(volatile avr32_adc_t *adc,
		uint16_t channel);

uint32_t adc_get_latest_value(volatile avr32_adc_t *adc);

/**
 * \}
 */



#endif /* INTERNAL_ADC_H_ */