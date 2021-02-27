/*
 * twi_mux.c
 * 
 * This API is for the TCA9564APWR 4-channel multiplexer.
 *
 * Created: 28/07/2016 11:23:10 AM
 *  Author: rvandommelen
 *  
 */ 

#include "twi_mux.h"
#include "twim.h"
#include "sysclk.h"
//#include "io_funcs.spectrometer.h"
#include "board.h"
#include "gpio.h"

#if (MCU_BOARD_REV==REV_A)
static const gpio_map_t TWIM_GPIO_MAP =
{
	{AVR32_TWIMS0_TWD_PIN, AVR32_TWIMS0_TWD_FUNCTION},
	{AVR32_TWIMS0_TWCK_PIN, AVR32_TWIMS0_TWCK_FUNCTION}
};
#else
static const gpio_map_t TWIM_GPIO_MAP =
{
	{AVR32_TWIMS1_TWD_PIN, AVR32_TWIMS1_TWD_FUNCTION},
	{AVR32_TWIMS1_TWCK_PIN, AVR32_TWIMS1_TWCK_FUNCTION}
};
#endif

//! \brief Constants to define the sent and received pattern
//#define  PATTERN_TEST_LENGTH    (sizeof(test_pattern)/sizeof(U8))
//const uint8_t test_pattern[] =  {0x27};


int twi_mux_start() {
	// A couple helpful sites
	// https://engineering.purdue.edu/ece477/Archive/2012/Spring/S12-Grp03/hot_links/imu_gpst.c
	// http://simplemachines.it/martin/mizar32/1.6.0-AT32UC3/DRIVERS/TWI/MASTER_EXAMPLE/DOC/html/a00010.html#l00123 (N/A as of 2017-05-16, BPlache)

	int8_t status;
	
	const twim_options_t TWI_MUX_OPTIONS = {
		.pba_hz = sysclk_get_pba_hz(),	//FOSC0,
		.speed = TWI_STD_MODE_SPEED,
		.chip = TWI_MUX_ADDRESS,		// Need to change for more general operation?  I don't know how to use this. Ronnie.
		.smbus = false,
	};
	
	// Configure and set the ~RESET line of the I2C mux
#if (MCU_BOARD_REV==REV_A)		
	gpio_configure_pin(I2C_MUX_RESET_N,(GPIO_DIR_OUTPUT | GPIO_INIT_HIGH | GPIO_PULL_UP));
	gpio_enable_gpio_pin(I2C_MUX_RESET_N);		// is this line needed?
#else
	gpio_configure_pin(I2C_MUX_RESET_N, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_set_pin_low(I2C_MUX_RESET_N);  // reset - only 6ns req.
	gpio_set_pin_high(I2C_MUX_RESET_N);
#endif

	// Configure TWI clock and data pins
	gpio_enable_module(TWIM_GPIO_MAP, sizeof(TWIM_GPIO_MAP) / sizeof(TWIM_GPIO_MAP[0]));

#if (MCU_BOARD_REV==REV_A)	
	// Initialize the TWI
	status  = twi_master_init( &AVR32_TWIM0, &TWI_MUX_OPTIONS);
#else
	status = twi_master_init(TWI_MUX_PORT, &TWI_MUX_OPTIONS);
	//status = 0;
#endif	

	return status;
}


////  Add the following include statements only here,
//    because they are just for testing & debugging.

# include "io_funcs.spectrometer.h"
# include <FreeRTOS.h>
# include <task.h>

int twi_mux_test(int n, int delay) {

  int ii;
  for ( ii=0; ii<n; ii++ ) {

    const uint32_t SIZE_MUX_CHANNEL = 1;
    uint8_t MUX_CHANNEL;

    MUX_CHANNEL = TWI_MUX_ECOMPASS;

    io_out_string ( "TWI-MUX --+- " );
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
      io_out_string ( "failed\r\n" );
      return (int)MUX_CHANNEL;
    }
    io_out_string ( "ok\r\n" );
    vTaskDelay((portTickType)TASK_DELAY_MS(delay));

    MUX_CHANNEL = TWI_MUX_SPEC_TEMP_A;

    io_out_string ( "TWI-MUX -+-- " );
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
      io_out_string ( "failed\r\n" );
      return (int)MUX_CHANNEL;
    }
    io_out_string ( "ok\r\n" );
    vTaskDelay((portTickType)TASK_DELAY_MS(delay));

    MUX_CHANNEL = TWI_MUX_SPEC_TEMP_B;

    io_out_string ( "TWI-MUX +--- " );
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
      io_out_string ( "failed\r\n" );
      return (int)MUX_CHANNEL;
    }
    io_out_string ( "ok\r\n" );
    vTaskDelay((portTickType)TASK_DELAY_MS(delay));

    MUX_CHANNEL = TWI_MUX_HEADTILTS;

    io_out_string ( "TWI-MUX ---+ " );
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
      io_out_string ( "failed\r\n" );
      return (int)MUX_CHANNEL;
    }
    io_out_string ( "ok\r\n" );
    vTaskDelay((portTickType)TASK_DELAY_MS(delay));
  }

  return 0;
}


