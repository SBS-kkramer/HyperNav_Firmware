/*
 * twi_mux.h
 *
 * Created: 28/07/2016 11:23:54 AM
 *  Author: rvandommelen
 
 * 2017-02-27, SKF: Updates for tilt, temperature sensors
 * 2017-04-19, SKF: Changes for E980031B
 */ 


#ifndef TWI_MUX_H_
#define TWI_MUX_H_

#if (MCU_BOARD_REV==REV_A)
//#define MUX_PORT	&AVR32_TWIM0
#endif

// Datasheet for TCA9546A multiplexer www.ti.com/lit/ds/symlink/tca9546a.pdf
// 7-bit address is 1 1 1 0 A2=0 A1=0 A0=0
// For HyperNav A2=A1=A0 are tied to ground.
#define TWI_MUX_ADDRESS	0x70    // = bit-wise 01110000

// The four channels of the multiplexer are coded using bit masks.
// In principle, multiple channels can be used concurrently.
//
// In HyperNav, only single channels are communicated over at a time.
// However, multiple devices can be powered up concurrently.
// 
#define TWI_MUX_HEADTILTS	0x01
#define TWI_MUX_ECOMPASS	0x02
#define TWI_MUX_SPEC_TEMP_A 0x04
#define TWI_MUX_SPEC_TEMP_B 0x08

# define TWI_MUX_TENBIT false

int twi_mux_start(void);  //  This function is very similar to twi_master_setup() as defined in twi_master.h
int twi_mux_test(int n, int delay);

#endif /* TWI_MUX_H_ */
