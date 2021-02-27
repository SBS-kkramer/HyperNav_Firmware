/*
 * cgs_cfg.h
 *
 * Created: 7/4/2016 4:20:31 PM
 *  Author: sfeener
 */ 


#ifndef CGS_CFG_H_
#define CGS_CFG_H_

#include "board.h"	// required for hardware-specific details

// Spectrometer clock generation  
#define CGS_TC						SPEC_TC
#define CGS_CLK_TC_CHANNEL_ID		SPEC_CLK_TC_CHANNEL_ID
#define CGS_CLK_TC_CHANNEL_PIN		SPEC_CLK_TC_CHANNEL_PIN
#define CGS_CLK_TC_CHANNEL_FUNCTION	SPEC_CLK_TC_CHANNEL_FUNCTION

#define CGS_SPEC_SELECT				ADC_MUX_A0
#define CGS_MUX_GND_SELECT			ADC_MUX_A1

#define CGS_CLK_CLOCK_SOURCE		TC_CLOCK_SOURCE_TC2           // Internal source clock 2, connected to fPBC / 2.
// assuming PBC clock is at crystal freq of 14.7456 MHz,
// the following define specifies the spectrometer clock speed.
//#define CGS_CLK_RC_VALUE				0	// a slow clock (overflow)
  #define CGS_CLK_RC_VALUE				1	// 3.6864 MHz
//#define CGS_CLK_RC_VALUE				2	// 1.8432 MHz
//#define CGS_CLK_RC_VALUE				5	// ~737.28 kHz
//#define CGS_CLK_RC_VALUE				10	// ~368.64 kHz
//#define CGS_CLK_RC_VALUE				25	// ~147.5 kHz 
//#define CGS_CLK_RC_VALUE				50	// ~73.73 kHz 



// Sample interval timer - time between start pulses
#define CGS_SPEC_A_INTERVAL_TC_CHANNEL_ID			SPEC_A_INTERVAL_TC_CHANNEL_ID
#define CGS_SPEC_A_INTERVAL_TC_CHANNEL_PIN			SPEC_A_INTERVAL_TC_CHANNEL_PIN
#define CGS_SPEC_A_INTERVAL_TC_CHANNEL_FUNCTION		SPEC_A_INTERVAL_TC_CHANNEL_FUNCTION
#define CGS_SPEC_B_INTERVAL_TC_CHANNEL_ID			SPEC_B_INTERVAL_TC_CHANNEL_ID
#define CGS_SPEC_B_INTERVAL_TC_CHANNEL_PIN			SPEC_B_INTERVAL_TC_CHANNEL_PIN
#define CGS_SPEC_B_INTERVAL_TC_CHANNEL_FUNCTION		SPEC_B_INTERVAL_TC_CHANNEL_FUNCTION
#define CGS_INTERVAL_CLOCK_SOURCE					TC_CLOCK_SOURCE_TC1	// Internal source clock 1, connected to 32 kHz clock (32768 Hz).
#define CGS_INTERVAL_CLOCK_FREQUENCY_HZ             32768  //  MUST modify this when modifying CGS_INTERVAL_CLOCK_SOURCE !!!
#define CGS_INTERVAL_MIN_RC_VALUE					327    //    327 ticks =    9.97 ms;    327.68 =   10 ms
#define CGS_INTERVAL_MAX_RC_VALUE					65535  //  65535 ticks = 1999.97 ms;  65503.23 = 1999 ms

#if (MCU_BOARD_REV==REV_A)
// Spectrometer /EOS signal
#define CGS_EOS_PIN									SPEC_EOS_N_INT1
#define CGS_EOS_INT_LINE							SPEC_EOS_INT_LINE
#define CGS_EOS_PIN_FUNCTION						SPEC_EOS_PIN_FUNCTION
#define CGS_EOS_PRIORITY							SPEC_EOS_PRIO
#define	CGS_EOS_IRQ									SPEC_EOS_IRQ
#else
// /EOS signals from both spectrometers
#define CGS_SPEC_A_EOS_PIN							SPEC_A_EOS_N
#define CGS_SPEC_A_EOS_INT_LINE						SPEC_A_EOS_INT_LINE
#define CGS_SPEC_A_EOS_PIN_FUNCTION					SPEC_A_EOS_PIN_FUNCTION
#define CGS_SPEC_A_EOS_PRIORITY						SPEC_A_EOS_IRQ
#define	CGS_SPEC_A_EOS_IRQ							SPEC_A_EOS_IRQ

#define CGS_SPEC_B_EOS_PIN							SPEC_B_EOS_N
#define CGS_SPEC_B_EOS_INT_LINE						SPEC_B_EOS_INT_LINE
#define CGS_SPEC_B_EOS_PIN_FUNCTION					SPEC_B_EOS_PIN_FUNCTION
#define CGS_SPEC_B_EOS_PRIORITY						SPEC_B_EOS_IRQ
#define	CGS_SPEC_B_EOS_IRQ							SPEC_B_EOS_IRQ
#endif

// Spectrometer TRIG signal
#define CGS_TRIG_PIN								ADC_CNVST_N_INT2
#define	CGS_TRIG_INT_LINE							ADC_CNVST_INT_LINE						
#define	CGS_TRIG_PIN_FUNCTION						ADC_CNVST_PIN_FUNCTION					
#define	CGS_TRIG_PRIORITY							ADC_CNVST_PRIO								
#define	CGS_TRIG_IRQ								ADC_CNVST_IRQ								


#endif /* CGS_CFG_H_ */
