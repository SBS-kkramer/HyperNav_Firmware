/*
 * max6633.c
 *
 * Created: 5/19/2017 12:40:23 PM
 *  Author: bplache
 */

# include <max6633.h>
# include <twi_mux.h>
# include <twim.h>

# include <FreeRTOS.h>
# include <task.h>
# include "io_funcs.spectrometer.h"
//# include <asf.h>

# define MAX6633_TWI_PORT TWI_MUX_PORT

//
//  This documentation is based on http://pdfserv.maximintegrated.com/en/ds/MAX6633-MAX6635.pdf
//  which covers MAX6633, MAX6634, MAX6635.
//
//  The 7-bit MAX6633 TWI address is determined
//  by the four address pins:
//  Address = [ 1 0 0 A3 A2 A1 A0 ]
//  Where
//    Ai = 0 is connected to ground
//    Ai = 1 if connected to Vcc

/* In HyperNav Spectrometers A0,A1,A2 is connected to Vcc, A3 is connected to ground.
 Which makes the Address for MAX6633 = 0x47 [0 1 0 0  0 1 1 1] */

# define MAX6633_TWI_ADDRESS 0x47  

//  The MAX6633 has 3 registers: Pointer, Temperature, Configuration
//
//  Pointer 0x00 -> Points to Temperature  (16 bit)
//          0x01 -> Points to Configuration (8 bit)
# define MAX6633_POINT_TEMPERATURE   0x00
# define MAX6633_POINT_CONFIGURATION 0x01
//
//  Temperature (16 bit):  [ MSB Bit12 ... Bit1 T_max T_high T_low ]
//    MSB = first sign bit
//    Value in two's complement format;  1 LSB = 0.0625 C = 1/16 C
//    T_... = Flag Bits (not implemented)
 
static inline int16_t max6633_tempValue_from_tempRegister ( int16_t t_register ) {
    return t_register>>=3;
}
//
//  Configuration (8 bit) register:
//    MSB
//    D7 == 0
//    D6 == 0
//    D5: 0 normal SMBus / 1 = full I2C compatibility
//    D4 == 0 (N/A in MAX6633?)
//    D3 == 0 (N/A in MAX6633?)
//    D2 == 0 (N/A in MAX6633?)
//    D1: 0 comparator mode / 1 interrupt mode
//    D0: 0 operate / 1 shutdown
//    LSB
//

int max6633_measure( uint8_t twi_mux_channel, float* T_C ) {

    // Set up the TWI mux to transmit on the required twi_mux_channel
    //
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, &twi_mux_channel, 1, TWI_MUX_ADDRESS, false) ) 
	{
        return MAX6633_ERR_SETUP_MUX;
    }

    //  Memory for bytes sent to / received from MAX6633
    //  We are using by default configuration so we don't need to configure manually
    uint8_t twi_io[2];
    
    //  Send command byte + configuration byte 
	// If we need to configure manually then we need to send these both bytes, otherwise just the command byte.
  
    twi_io[0] = 0x00; // This is the default MAX6633 configuration

  if ( STATUS_OK != twim_write( TWI_MUX_PORT, twi_io, 1, MAX6633_TWI_ADDRESS, false ) )
	 {
		 return MAX6633_ERR_WRITE_CFG;
     } 

    //  Send command byte, As we are not configuring manually, these will be same as above because we are just sending command byte.

 /*   twi_io[0] = MAX6633_POINT_TEMPERATURE;

    if ( STATUS_OK != twim_write( MAX6633_TWI_PORT, twi_io, 1, MAX6633_TWI_ADDRESS, false ) ) {
        return MAX6633_ERR_WRITE_TEM;
    }
*/
 
    //  Receive temperature register
    //
    twi_io[0] = 0x00;
    twi_io[1] = 0x00;

    if ( STATUS_OK != twim_read( TWI_MUX_PORT, twi_io, 2, MAX6633_TWI_ADDRESS, false ) ) {
        return MAX6633_ERR_READ_TEM;
    }

    //  Convert 2 bytes to a single 2-byte variable
    uint16_t temp_register = twi_io[0];
    temp_register <<= 8;
    temp_register |= twi_io[1];

    int16_t temp_16th_deg = max6633_tempValue_from_tempRegister( (int16_t)temp_register );

    *T_C  = temp_16th_deg;
    *T_C /= 16.0;

    return MAX6633_OK;
}
