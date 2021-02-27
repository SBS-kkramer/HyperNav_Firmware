/*!	\file	AD7677.c
 *	\brief	AD7677 - 16-bit, 1 MSPS ADC driver
 * Created: 7/2/2016 11:46:00 PM
 *  Author: sfeener
 */ 

# include "gpio.h"

#include "compiler.h"
#include "board.h"
//#include "dbg.h"
//#include "spi.h"
#include "AD7677.h"
#include "AD7677_cfg.h"


/* \brief Initialize driver
 * @return ADS8344_OK: Success, ADS8344_FAIL: Error initializing driver.
 */
S16 ad7677_InitGpio() {

# if (MCU_BOARD_REV==REV_A) 
  gpio_configure_pin(ADC_MUX_A0, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// mapped to CGS_SPEC_SELECT - 0 for SPEC A, 1 for B
  gpio_configure_pin(ADC_MUX_A1, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);		// .. but force ADC input mux to gnd
	
  gpio_configure_pin(ADC_RESET, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);		// Hold ADC in reset
  gpio_configure_pin(ADC_PD, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);			// Keep ADC in power down mode
  gpio_configure_pin(ADC_BUSY_INT0, GPIO_DIR_INPUT);					// ADC busy interrupt pin as input
  gpio_configure_pin(ADC_WARP, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// don't use warp speed by default
  gpio_configure_pin(ADC_IMPULSE, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// don't use impulse mode by default - it may start another conversion unexpectedly
  // reference turned off earlier
  gpio_set_pin_low(ADC_RESET);											// take ADC Out of reset
# else
  //  Shared between A & B
  //
  gpio_configure_pin(ADC_RESET, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);		// Hold ADCs in reset

  gpio_configure_pin(ADC_A_PD, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);		// Keep ADC A in power down mode
  gpio_configure_pin(ADC_B_PD, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);		// Keep ADC B in power down mode

  //  Shared between A & B
  //
  gpio_configure_pin(ADC_WARP, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// don't use warp speed by default
  gpio_configure_pin(ADC_IMPULSE, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);		// don't use impulse mode by default - it may start another conversion unexpectedly
# endif

  return AD7677_OK;
}

/* \brief Start operation
 * @return AD7677_OK: Success, AD7677_FAIL: Error initializing driver.
 */
S16 ad7677_Start( component_selection_t which ) {
  //  Take BOTH ADCs out of reset
  //
  gpio_set_pin_low(ADC_RESET);
  
  switch ( which ) {
  case component_A: gpio_set_pin_low  ( ADC_A_PD ); break;
  case component_B: gpio_set_pin_low  ( ADC_B_PD ); break;
  default:          //  Code design should make it impossible to get here.
                    //  If this is executed, a coding error has occurred.
                    //  Currently, ignoring.
                    return AD7677_FAIL;
  }

  return AD7677_OK;
}

/* \brief Stop operation
 * @return AD7677_OK: Success, AD7677_FAIL: Error initializing driver.
 */
S16 ad7677_StopBoth() {

  //  Power down both ADCs
  //	
  gpio_set_pin_high ( ADC_A_PD );
  gpio_set_pin_high ( ADC_B_PD );

  //  Bring ADCs into reset
  //
  gpio_set_pin_high(ADC_RESET);

  return AD7677_OK;
}


# if 0
/* \brief Convert ADC counts to a volts value using ADC reference voltage
 * @param 	counts	ADC 16-bit raw counts
 * @return	A voltage reading (V).
 */
F32 ad7677_counts2Volts(U16 counts)
{
	return (AD7677_VREF * (counts/65536.0));
}
# endif
