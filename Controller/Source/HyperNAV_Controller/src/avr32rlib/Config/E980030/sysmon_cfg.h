/*! \file sysmon_cfg.h**********************************************************
 *
 * \brief System Monitor Driver configuration file.
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-24
 *
 * 2016-10-10, SF: Adapting for HyperNAV Controller, E980030
  ***************************************************************************/

#ifndef SYSMON_CFG_H_
#define SYSMON_CFG_H_

#include "compiler.h"

/*! \name System monitor configuration
 */
//! @{

// Enable One-Wire sensors
//#define SYSMON_OW_TSENS	

// General enable port
#define SYSMON_ON	AVR32_PIN_PB01

// Channel specific information --
//
// Note that some channels have voltage-divider resistor networks.
// The relation is: VINxxx = (R1xxx + R2xxx)*Vmeasured/R2xxx
// (R2xxx is the resistance to ground).

// Lamp temperature sensor gain (uA/K)
//#define SYSMON_AD590LH_GAIN    1

// AD590LH gain resistor (Ohm)
//#define	SYSMON_AD590LH_R		2490.0

// Main supply current sense gain (A/V)
//#define SYSMON_I_SENSE_GM		0.01
// Sensing resistor (Ohm)
//#define SYSMON_I_RSENSE			0.01
// Current sense Rout (Ohm)
//#define SYSMON_I_ROUT			20000.0

// VMAIN divider
#define SYSMON_R1__VMAIN		100.0e3
#define SYSMON_R2__VMAIN	 	10.0e3

// P3V3 divider
#define SYSMON_R1__P3V3		100.0e3
#define SYSMON_R2__P3V3	 	100.0e3


// ADC channel wiring
#define SYSMON_VMAIN_CH			6
#define VMAIN_SENSE_PIN			AVR32_ADC_AD_6_PIN
#define VMAIN_SENSE_FUNCTION	AVR32_ADC_AD_6_FUNCTION

#define	SYSMON_P3V3_CH			7
#define P3V3_SENSE_PIN			AVR32_ADC_AD_7_PIN
#define P3V3_SENSE_FUNCTION		AVR32_ADC_AD_7_FUNCTION



//! @}


#endif /* SYSMON_CFG_H_ */
