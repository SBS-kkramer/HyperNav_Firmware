#ifndef __LIS3DSH__H
#define __LIS3DSH__H

#include <stdint.h>
#include <twim.h>

typedef enum {
	MEMS_SUCCESS			=		0x01,
	MEMS_ERROR				=		0x00
} LIS3DSH_status_t;

typedef enum {
	MEMS_ENABLE				=		0x01,
	MEMS_DISABLE			=		0x00
} LIS3DSH_State_t;

typedef struct {
	int16_t AXIS_X;
	int16_t AXIS_Y;
	int16_t AXIS_Z;
} AxesRaw_t;

typedef enum {
	LIS3DSH_ODR_PWR_DOWN		=		0x00,
	LIS3DSH_ODR_3_125Hz			=		0x01,
	LIS3DSH_ODR_6_25Hz			=		0x02,
	LIS3DSH_ODR_12_5Hz		    =		0x03,
	LIS3DSH_ODR_25Hz		    =		0x04,
	LIS3DSH_ODR_50Hz		    =		0x05,
	LIS3DSH_ODR_100Hz		    =		0x06,
	LIS3DSH_ODR_400Hz		    =		0x07,
	LIS3DSH_ODR_800Hz		    =		0x08,
	LIS3DSH_ODR_1600Hz		    =		0x09
} LIS3DSH_ODR_t;

typedef enum {
	LIS3DSH_POWER_DOWN          =		0x00,
	//LIS3DSH_LOW_POWER 			=		0x01,
	LIS3DSH_NORMAL				=		0x02
} LIS3DSH_Mode_t;

typedef enum {
	LIS3DSH_FULLSCALE_2                   =               0x00,
	LIS3DSH_FULLSCALE_4                   =               0x01,
	LIS3DSH_FULLSCALE_6                   =               0x02,
	LIS3DSH_FULLSCALE_8                   =               0x03,
	LIS3DSH_FULLSCALE_16                  =               0x04
} LIS3DSH_Fullscale_t;

typedef enum {
	LIS3DSH_BLE_LSB			=		0x00,
	LIS3DSH_BLE_MSB			=		0x01
} LIS3DSH_Endianess_t;


typedef enum {
	LIS3DSH_SELF_TEST_DISABLE             =               0x00,
	LIS3DSH_SELF_TEST_0                   =               0x01,		// positive sign
	LIS3DSH_SELF_TEST_1                   =               0x02		// negative sign
} LIS3DSH_SelfTest_t;

typedef enum {
	LIS3DSH_FIFO_BYPASS_MODE              =               0x00,
	LIS3DSH_FIFO_MODE                     =               0x01,
	LIS3DSH_FIFO_STREAM_MODE              =               0x02,
	LIS3DSH_FIFO_TRIGGER_MODE             =               0x03,
	LIS3DSH_FIFO_BYPASS_UNTIL_TRIGGER_THEN_STREAM_MODE =  0x04,
	LIS3DSH_FIFO_BYPASS_UNTIL_TRIGGER_THEN_FIFO_MODE =	  0x07
} LIS3DSH_FifoMode_t;

typedef enum {
	LIS3DSH_X_ENABLE                      =               0x01,
	LIS3DSH_X_DISABLE                     =               0x00,
	LIS3DSH_Y_ENABLE                      =               0x02,
	LIS3DSH_Y_DISABLE                     =               0x00,
	LIS3DSH_Z_ENABLE                      =               0x04,
	LIS3DSH_Z_DISABLE                     =               0x00
} LIS3DSH_AXISenable_t;

/* Exported constants --------------------------------------------------------*/

#ifndef __SHARED__CONSTANTS
#define __SHARED__CONSTANTS

#define MEMS_SET                                        0x01
#define MEMS_RESET                                      0x00

#endif /*__SHARED__CONSTANTS*/

//Register Definitions

#define LIS3DSH_INFO1					0x0D			// Read only information register
#define LIS3DSH_INFO2					0x0E			// Read only information register 2
#define LIS3DSH_WHO_AM_I				0x0F			// device identification register
#define LIS3DSH_OUT_T					0x0C			// Temperature output register. 1LSB/deg - 8-bit resolution. Two's complement
#define LIS3DSH_OFF_X					0x10			// Offset correction X-axis register,signed value
#define LIS3DSH_OFF_Y					0x11			// Offset correction Y-axis register,signed value
#define LIS3DSH_OFF_Z					0x12			// Offset correction Z-axis register,signed value
#define LIS3DSH_CS_X					0x13			// Constant shift signed value X-axis register - state machine only.
#define LIS3DSH_CS_Y					0x14			// Constant shift signed value Y-axis register - state machine only.
#define LIS3DSH_CS_Z					0x15			// Constant shift signed value Z-axis register - state machine only.
#define LIS3DSH_LC_L					0x16
#define LIS3DSH_LC_H					0x17			// 16-bit long-counter register for interrupt state machine programs timing.
#define LIS3DSH_VFC_1					0x1B			// Vector coefficient register 1 for DIff filter
#define LIS3DSH_VFC_2					0x1C			// Vector coefficient register 2 for DIff filter
#define LIS3DSH_VFC_3					0x1D			// Vector coefficient register 3 for FSM2 filter  (is this a typo in the datasheet?)
#define LIS3DSH_VFC_4					0x1E			// Vector coefficient register 4 for DIff filter
#define LIS3DSH_THRS3					0x1F			// Threshold value register
#define LIS3DSH_OUT_X_L					0x28
#define LIS3DSH_OUT_X_H					0x29			// X-axis output register
#define LIS3DSH_OUT_Y_L					0x2A
#define LIS3DSH_OUT_Y_H					0x2B			// Y-axis output register
#define LIS3DSH_OUT_Z_L					0x2C
#define LIS3DSH_OUT_Z_H					0x2D			// Z-axis output register
#define LIS3DSH_ST1_1					0x40			// State machine 1 code register 1
#define LIS3DSH_ST2_1					0x41			// State machine 1 code register 2
#define LIS3DSH_ST3_1					0x42			// State machine 1 code register 3
#define LIS3DSH_ST4_1					0x43			// State machine 1 code register 4
#define LIS3DSH_ST5_1					0x44			// State machine 1 code register 5
#define LIS3DSH_ST6_1					0x45			// State machine 1 code register 6
#define LIS3DSH_ST7_1					0x46			// State machine 1 code register 7
#define LIS3DSH_ST8_1					0x47			// State machine 1 code register 8
#define LIS3DSH_ST9_1					0x48			// State machine 1 code register 9
#define LIS3DSH_ST10_1					0x49			// State machine 1 code register 10
#define LIS3DSH_ST11_1					0x4A			// State machine 1 code register 11
#define LIS3DSH_ST12_1					0x4B			// State machine 1 code register 12
#define LIS3DSH_ST13_1					0x4C			// State machine 1 code register 13
#define LIS3DSH_ST14_1					0x4D			// State machine 1 code register 14
#define LIS3DSH_ST15_1					0x4E			// State machine 1 code register 15
#define LIS3DSH_ST16_1					0x4F			// State machine 1 code register 16
#define LIS3DSH_TIM4_1					0X50			// 8-bit general timer (unsigned) for SM1 operation timing
#define LIS3DSH_TIM3_1					0x51			// 8-bit general timer (unsigned) for SM1 operation timing
#define LIS3DSH_TIM2_1_L				0x52			// 
#define LIS3DSH_TIM2_1_H				0x53			// 16-bit general timer (unsigned value) for SM1 operation timing.
#define LIS3DSH_TIM1_1_L				0x54			//
#define LIS3DSH_TIM1_1_H				0x55			// 16-bit general timer (unsigned value) for SM1 operation timing.
#define LIS3DSH_THRS2_1					0x56			// Threshold value for SM1 operation.
#define LIS3DSH_THRS1_1					0x57			// Threshold value for SM1 operation.
#define LIS3DSH_PR1						0x5C			// PR1 - Program and reset pointer for SM1 motion detection operation.
#define LIS3DSH_TC1_L					0x5D
#define LIS3DSH_TC1_H					0x5E			// 16-bit general timer (unsigned output value) for SM1 operation timing.
#define LIS3DSH_PEAK1					0x19			// Peak detection value register for SM1 operation
#define LIS3DSH_ST1_2					0x60			// State machine 2 code register 1
#define LIS3DSH_ST2_2					0x61			// State machine 2 code register 2
#define LIS3DSH_ST3_2					0x62			// State machine 2 code register 3
#define LIS3DSH_ST4_2					0x63			// State machine 2 code register 4
#define LIS3DSH_ST5_2					0x64			// State machine 2 code register 5
#define LIS3DSH_ST6_2					0x65			// State machine 2 code register 6
#define LIS3DSH_ST7_2					0x66			// State machine 2 code register 7
#define LIS3DSH_ST8_2					0x67			// State machine 2 code register 8
#define LIS3DSH_ST9_2					0x68			// State machine 2 code register 9
#define LIS3DSH_ST10_2					0x69			// State machine 2 code register 10
#define LIS3DSH_ST11_2					0x6A			// State machine 2 code register 11
#define LIS3DSH_ST12_2					0x6B			// State machine 2 code register 12
#define LIS3DSH_ST13_2					0x6C			// State machine 2 code register 13
#define LIS3DSH_ST14_2					0x6D			// State machine 2 code register 14
#define LIS3DSH_ST15_2					0x6E			// State machine 2 code register 15
#define	LIS3DSH_ST16_2					0x6F			// State machine 2 code register 16
#define LIS3DSH_TIM4_2					0x70			// 8-bit general timer (unsigned value) for SM2 operation timing	
#define LIS3DSH_TIM3_2					0x71			// 8-bit general timer (unsigned value) for SM2 operation timing			
#define LIS3DSH_TIM2_2_L				0x72			// 16-bit general timer (unsigned value) for SM2 operation timing	
#define LIS3DSH_TIM2_2_H				0x73
#define LIS3DSH_TIM1_2_L				0x74			// 16-bit general timer (unsigned value) for SM2 operation timing
#define LIS3DSH_TIM1_2_H				0x75
#define LIS3DSH_THRS2_2					0x76			// Threshold signed value for SM2 operation
#define LIS3DSH_THRS1_2					0x77			// Threshold signed value for SM2 operation
#define LIS3DSH_PR2						0x7C			// Program and reset pointer for SM2 motion detection operation
#define LIS3DSH_TC2_L					0x7D
#define LIS3DSH_TC2_H					0x7E			// 16-bit general timer (unsigned output value) for SM2 operation timing.
#define LIS3DSH_PEAK2					0x1A			// Peak detection value register for SM2 operation
#define LIS3DSH_DES2					0x78			// Decimation counter value register for SM2 operation



//CONTROL REGISTER 1  (SM1 control register)
#define LIS3DSH_CTRL_REG1				0x21
#define LIS3DSH_HYST2_1					BIT(7)			// Hysteresis unsigned value to be added or subtracted from threshold value in SM1
#define LIS3DSH_HYST1_1					BIT(6)			// 
#define LIS3DSH_HYST0_1					BIT(5)			// 
#define LIS3DSH_CTRL_REG1_BIT4			BIT(4)			// Unused
#define LIS3DSH_SM1_PIN					BIT(3)			// 0=SM1 interrupt routed to INT1, 1=SM1 interrupt routed to INT2 pin
#define LIS3DSH_CTRL_REG1_BIT2			BIT(2)			// Unused
#define LIS3DSH_CTRL_REG1_BIT1			BIT(1)			// unused
#define LIS3DSH_SM1_EN					BIT(0)			// 0=SM1 disabled, 1=SM1 enabled  Default value=0


//CONTROL REGISTER 3
#define LIS3DSH_CTRL_REG3				0x23
#define LIS3DSH_I1_DR_EN				BIT(7)			// DRDY signal enable to INT1. Default value:0 
#define LIS3DSH_IEA						BIT(6)			// Interrupt signal polarity. Default value:0
#define LIS3DSH_IEL				        BIT(5)			// Interrupt signal latching. Default value:0
#define LIS3DSH_INT2_EN					BIT(4)			// Interrupt 2 enable/disable. Default value:0
#define LIS3DSH_INT1_EN					BIT(3)			// Interrupt 1 enable/disable. Default Value:0
#define LIS3DSH_VFILT					BIT(2)			// Vector filter enable/disable. Default value:0
#define LIS3DSH_CTRLREG3_BIT1			BIT(1)			// unused
#define LIS3DSH_STRT					BIT(0)			// Soft reset bit

// CONTROL REGISTER 4
#define LIS3DSH_CTRL_REG4				0x20
#define LIS3DSH_ODR3					BIT(7)			// Output data rate and power mode selection.
#define LIS3DSH_ODR2					BIT(6)			// Output data rate and power mode selection.
#define LIS3DSH_ODR1				    BIT(5)			// Output data rate and power mode selection.
#define LIS3DSH_ODR0					BIT(4)			// Output data rate and power mode selection.
#define LIS3DSH_BDU						BIT(3)			// Block data update. Default value:0
#define LIS3DSH_ZEN						BIT(2)			// Z axis enable. Default value:1
#define LIS3DSH_YEN						BIT(1)			// Y axis enable. Default value:1
#define LIS3DSH_XEN						BIT(0)			// X axis enable. Default value:1

// CONTROL REGISTER 5
#define LIS3DSH_CTRL_REG5				0x24
#define LIS3DSH_BW2						BIT(7)			// Anti-aliasing filter bandwidth.
#define LIS3DSH_BW1						BIT(6)			// Anti-aliasing filter bandwidth.
#define LIS3DSH_FSCALE2				    BIT(5)			// Full-scale selection.
#define LIS3DSH_FSCALE1					BIT(4)			// Full-scale selection.
#define LIS3DSH_FSCALE0					BIT(3)			// Full-scale selection.
#define LIS3DSH_ST2						BIT(2)			// Self-test enable.
#define LIS3DSH_ST1						BIT(1)			// Self-test enable.
#define LIS3DSH_SIM						BIT(0)			// SPI serial interface mode selection.  Default value: 0

// CONTROL REGISTER 6
#define LIS3DSH_CTRL_REG6				0x25
#define LIS3DSH_BOOT					BIT(7)			// Force reboot, cleared as soon as the reboot is finished. Active high.
#define LIS3DSH_FIFO_EN					BIT(6)			// FIFO enable. Default value 0.
#define LIS3DSH_WTM_EN				    BIT(5)			// Enable FIFO Watermark level use. Default value 0.
#define LIS3DSH_ADD_INC					BIT(4)			// Register address automatically incremented during a multiple byte access with a serial interface (I2C or SPI).
#define LIS3DSH_P1_EMPTY				BIT(3)			// Enable FIFO Empty indication on int1. Default value 0.
#define LIS3DSH_P1_WTM					BIT(2)			// FIFO Watermark interrupt on int1. Default value 0.
#define LIS3DSH_P1_OVERRUN				BIT(1)			// FIFO overrun interrupt on int1. Default value 0.
#define LIS3DSH_P2_BOOT					BIT(0)			// BOOT interrupt on int2. Default value 0.

// 
#define LIS3DSH_STATUS_REG				0x27
#define LIS3DSH_ZYXOR                    BIT(7)			// X, Y, and Z axis data overrun. Default value: 0
#define LIS3DSH_ZOR                      BIT(6)			// Z axis data overrun. Default value: 0
#define LIS3DSH_YOR                      BIT(5)			// Y axis data overrun. Default value: 0
#define LIS3DSH_XOR                      BIT(4)			// X axis data overrun. Default value: 0
#define LIS3DSH_ZYXDA                    BIT(3)			// X, Y, and Z axis new data available. Default value: 0
#define LIS3DSH_ZDA                      BIT(2)			// Z axis new data available. Default value: 0
#define LIS3DSH_YDA                      BIT(1)			// Y axis new data available. Default value: 0
#define LIS3DSH_XDA                      BIT(0)			// X axis new data available. Default value: 0

// STAT register (interrupt status - interrupt synchronization register)
#define LIS3DSH_STAT_REG					0x18
#define LIS3DSH_LONG						BIT(7)			// 0=no interrupt, 1=long counter (LC) interrupt flag common for both SM
#define LIS3DSH_SYNCW                    BIT(6)			// Synchronization for external Host Controller interrupt based on output data
#define LIS3DSH_SYNC1                    BIT(5)			// 0=SM1 running normally, 1=SM1 stopped and await restart request from SM2
#define LIS3DSH_SYNC2                    BIT(4)			// 0=SM2 running normally, 1=SM2 stopped and await restart request from SM1
#define LIS3DSH_INT_SM1                  BIT(3)			// SM1 - Interrupt Selection - 1=SM1 interrupt enable; 0: SM1 interrupt disable
#define LIS3DSH_INT_SM2                  BIT(2)			// SM2 - Interrupt Selection - 1=SM2 interrupt enable; 0: SM2 interrupt disable
#define LIS3DSH_DOR                      BIT(1)			// Data overrun indicates not read data from output register when next data samples measure start
#define LIS3DSH_DRDY                     BIT(0)			// data ready from output register
 
// FIFO_CTRL register (FIFO control register)
#define LIS3DSH_FIFO_CTRL				0x2E
#define LIS3DSH_FMODE2					BIT(7)			// FIFO Mode Selection
#define LIS3DSH_FMODE1                  BIT(6)			// FIFO Mode Selection
#define LIS3DSH_FMODE0                  BIT(5)			// FIFO Mode Selection
#define LIS3DSH_WTMP4                   BIT(4)			// FIFO Watermark pointer; FIFO deep if the Watermark is enabled
#define LIS3DSH_WTMP3					BIT(3)			// FIFO Watermark pointer; FIFO deep if the Watermark is enabled
#define LIS3DSH_WTMP2					BIT(2)			// FIFO Watermark pointer; FIFO deep if the Watermark is enabled
#define LIS3DSH_WTMP1					BIT(1)			// FIFO Watermark pointer; FIFO deep if the Watermark is enabled
#define LIS3DSH_WTMP0					BIT(0)			// FIFO Watermark pointer; FIFO deep if the Watermark is enabled

// FIFO_SRC register (FIFO SRC control register)
#define LIS3DSH_FIFO_SRC				0x2F
#define LIS3DSH_WTM						BIT(7)			// Watermark status
#define LIS3DSH_OVRN_FIFO               BIT(6)			// Overrun bit status. 0=FIFO is not completely filled; 1=FIFO is completely filled
#define LIS3DSH_EMPTY	                BIT(5)			// FIFO empty bit.
#define LIS3DSH_FSS4					BIT(4)			// FIFO stored data level
#define LIS3DSH_FSS3					BIT(3)			// FIFO stored data level
#define LIS3DSH_FSS2					BIT(2)			// FIFO stored data level
#define LIS3DSH_FSS1					BIT(1)			// FIFO stored data level
#define LIS3DSH_FSS0					BIT(0)			// FIFO stored data level


// MASK_1B register (Axis and sign mask (swap) for SM1 motion detection operation)
#define LIS3DSH_MASK1_B					0x59
#define LIS3DSH_MASK1_B_P_X				BIT(7)			// 0=X + disabled, 1=X + enabled
#define LIS3DSH_MASK1_B_N_X				BIT(6)			// 0=X - disabled, 1=X – enabled
#define LIS3DSH_MASK1_B_P_Y				BIT(5)			// 0=Y+ disabled, 1=Y + enabled
#define LIS3DSH_MASK1_B_N_Y				BIT(4)			// 0=Y- disabled, 1=Y – enabled
#define LIS3DSH_MASK1_B_P_Z				BIT(3)			// 0=Z + disabled, 1=Z + enabled
#define LIS3DSH_MASK1_B_N_Z				BIT(2)			// 0=Z - disabled, 1=Z – enabled
#define LIS3DSH_MASK1_B_P_V				BIT(1)			// 0=V + disabled, 1=V + enabled
#define LIS3DSH_MASK1_B_N_V				BIT(0)			// 0=V - disabled, 1=V – enabled

// MASK_1A register (Axis and sign mask (default) for SM1 motion detection operation)
#define LIS3DSH_MASK1_A					0x5A
#define LIS3DSH_MASK1_A_P_X				BIT(7)			// 0=X + disabled, 1=X + enabled
#define LIS3DSH_MASK1_A_N_X				BIT(6)			// 0=X - disabled, 1=X – enabled
#define LIS3DSH_MASK1_A_P_Y				BIT(5)			// 0=Y+ disabled, 1=Y + enabled
#define LIS3DSH_MASK1_A_N_Y				BIT(4)			// 0=Y- disabled, 1=Y – enabled
#define LIS3DSH_MASK1_A_P_Z				BIT(3)			// 0=Z + disabled, 1=Z + enabled
#define LIS3DSH_MASK1_A_N_Z				BIT(2)			// 0=Z - disabled, 1=Z – enabled
#define LIS3DSH_MASK1_A_P_V				BIT(1)			// 0=V + disabled, 1=V + enabled
#define LIS3DSH_MASK1_A_N_V				BIT(0)			// 0=V - disabled, 1=V – enabled

// SETT1 Setting of threshold, peak detection and flags for SM1 motion detection operation
#define LIS3DSH_SETT1					0x5B
#define LIS3DSH_SETT1_P_DET				BIT(7)			// SM1 peak detection
#define LIS3DSH_SETT1_THR3_SA			BIT(6)			// 
#define LIS3DSH_SETT1_ABS				BIT(5)			// 
#define LIS3DSH_SETT1_THR3_MA			BIT(2)			// 
#define LIS3DSH_SETT1_R_TAM				BIT(1)			// Next condition validation flag.
#define LIS3DSH_SETT1_SITR				BIT(0)			// 

// OUTS1 Output flags on axis for interrupt SM1 management
#define LIS3DSH_OUTS1					0x5F
#define LIS3DSH_OUTS1_P_X				BIT(7)			// 0=X + no show, 1=X + show
#define LIS3DSH_OUTS1_N_X				BIT(6)			// 0=X - no show, 1=X – show
#define LIS3DSH_OUTS1_P_Y				BIT(5)			// 0=Y+ no show, 1=Y + show
#define LIS3DSH_OUTS1_N_Y				BIT(4)			// 0=Y- no show, 1=Y – show
#define LIS3DSH_OUTS1_P_Z				BIT(3)			// 0=Z + no show, 1=Z + show
#define LIS3DSH_OUTS1_N_Z				BIT(2)			// 0=Z - no show, 1=Z – show
#define LIS3DSH_OUTS1_P_V				BIT(1)			// 0=V + no show, 1=V + show
#define LIS3DSH_OUTS1_N_V				BIT(0)			// 0=V - no show, 1=V – show

// CTRL_REG2
#define LIS3DSH_CTRL_REG2				0x22
#define LIS3DSH_HYST2_2					BIT(7)			// Hysteresis unsigned value to be added or subtracted from threshold value in SM2
#define LIS3DSH_HYST1_2					BIT(6)			//
#define LIS3DSH_HYST0_2					BIT(5)			//
#define LIS3DSH_CTRL_REG2_BIT4			BIT(4)			// Unused
#define LIS3DSH_SM2_PIN					BIT(3)			// 0=SM2 interrupt routed to INT1, 1=SM2 interrupt routed to INT2 pin
#define LIS3DSH_CTRL_REG2_BIT2			BIT(2)			// Unused
#define LIS3DSH_CTRL_REG2_BIT1			BIT(1)			// unused
#define LIS3DSH_SM2_EN					BIT(0)			// 0=SM2 disabled, 1=SM2 enabled  Default value=0

// MASK_2B register (Axis and sign mask (swap) for SM2 motion detection operation)
#define LIS3DSH_MASK2_B					0x79
#define LIS3DSH_MASK2_B_P_X				BIT(7)			// 0=X + disabled, 1=X + enabled
#define LIS3DSH_MASK2_B_N_X				BIT(6)			// 0=X - disabled, 1=X – enabled
#define LIS3DSH_MASK2_B_P_Y				BIT(5)			// 0=Y+ disabled, 1=Y + enabled
#define LIS3DSH_MASK2_B_N_Y				BIT(4)			// 0=Y- disabled, 1=Y – enabled
#define LIS3DSH_MASK2_B_P_Z				BIT(3)			// 0=Z + disabled, 1=Z + enabled
#define LIS3DSH_MASK2_B_N_Z				BIT(2)			// 0=Z - disabled, 1=Z – enabled
#define LIS3DSH_MASK2_B_P_V				BIT(1)			// 0=V + disabled, 1=V + enabled
#define LIS3DSH_MASK2_B_N_V				BIT(0)			// 0=V - disabled, 1=V – enabled

// MASK_2A register (Axis and sign mask (default) for SM2 motion detection operation)
#define LIS3DSH_MASK2_A					0x7A
#define LIS3DSH_MASK2_A_P_X				BIT(7)			// 0=X + disabled, 1=X + enabled
#define LIS3DSH_MASK2_A_N_X				BIT(6)			// 0=X - disabled, 1=X – enabled
#define LIS3DSH_MASK2_A_P_Y				BIT(5)			// 0=Y+ disabled, 1=Y + enabled
#define LIS3DSH_MASK2_A_N_Y				BIT(4)			// 0=Y- disabled, 1=Y – enabled
#define LIS3DSH_MASK2_A_P_Z				BIT(3)			// 0=Z + disabled, 1=Z + enabled
#define LIS3DSH_MASK2_A_N_Z				BIT(2)			// 0=Z - disabled, 1=Z – enabled
#define LIS3DSH_MASK2_A_P_V				BIT(1)			// 0=V + disabled, 1=V + enabled
#define LIS3DSH_MASK2_A_N_V				BIT(0)			// 0=V - disabled, 1=V – enabled

// SETT2 Setting of threshold, peak detection and flags for SM2 motion detection operation
#define LIS3DSH_SETT2					0x7B
#define LIS3DSH_SETT2_P_DET				BIT(7)			// SM1 peak detection
#define LIS3DSH_SETT2_THR3_SA			BIT(6)			//
#define LIS3DSH_SETT2_ABS				BIT(5)			//
#define LIS3DSH_SETT2_THR3_MA			BIT(2)			//
#define LIS3DSH_SETT2_R_TAM				BIT(1)			// Next condition validation flag.
#define LIS3DSH_SETT2_SITR				BIT(0)			//

// OUTS2 Output flags on axis for interrupt SM2 management
#define LIS3DSH_OUTS2					0x7F
#define LIS3DSH_OUTS2_P_X				BIT(7)			// 0=X + no show, 1=X + show
#define LIS3DSH_OUTS2_N_X				BIT(6)			// 0=X - no show, 1=X – show
#define LIS3DSH_OUTS2_P_Y				BIT(5)			// 0=Y+ no show, 1=Y + show
#define LIS3DSH_OUTS2_N_Y				BIT(4)			// 0=Y- no show, 1=Y – show
#define LIS3DSH_OUTS2_P_Z				BIT(3)			// 0=Z + no show, 1=Z + show
#define LIS3DSH_OUTS2_N_Z				BIT(2)			// 0=Z - no show, 1=Z – show
#define LIS3DSH_OUTS2_P_V				BIT(1)			// 0=V + no show, 1=V + show
#define LIS3DSH_OUTS2_N_V				BIT(0)			// 0=V - no show, 1=V – show



/* Exported macro ------------------------------------------------------------*/

#ifndef __SHARED__MACROS

#define __SHARED__MACROS
#define ValBit(VAR,Place)         (VAR & (1<<Place))
#define BIT(x) ( (x) )

#endif /*__SHARED__MACROS*/


uint8_t				LIS3DSH_ReadReg(uint8_t twiaddr, uint8_t Reg, uint8_t* Data);
uint8_t				LIS3DSH_WriteReg(uint8_t twiaddr, uint8_t Reg, uint8_t Data);
LIS3DSH_status_t	LIS3DSH_FIFOFull(uint8_t twiaddr, uint8_t* Data);
LIS3DSH_status_t	LIS3DSH_GetWHO_AM_I(uint8_t twiaddr, uint8_t* val);
LIS3DSH_status_t	LIS3DSH_SetODR(uint8_t twiaddr, LIS3DSH_ODR_t ov);
LIS3DSH_status_t	LIS3DSH_GetTemp(uint8_t twiaddr, int8_t* buff);
LIS3DSH_status_t	LIS3DSH_GetStatusReg(uint8_t twiaddr, uint8_t* val);
LIS3DSH_status_t	LIS3DSH_GetAccAxesRaw(uint8_t twiaddr, AxesRaw_t* buff);
LIS3DSH_status_t	LIS3DSH_SoftReset(uint8_t twiaddr);		// does this work?
LIS3DSH_status_t	LIS3DSH_SetBDU(uint8_t twiaddr, bool bdu);
LIS3DSH_status_t	LIS3DSH_DataAvailable(uint8_t twiaddr, uint8_t* Data);		


#define LIS3DSH_TWI_PORT	&AVR32_TWIM0




#if 0		// LIS3HH example library - not the same part!!!
//these could change accordingly with the architecture

#ifndef __ARCHDEP__TYPES
#define __ARCHDEP__TYPES

typedef unsigned char u8_t;
typedef unsigned short int u16_t;
typedef short int i16_t;
typedef signed char i8_t;

#endif /*__ARCHDEP__TYPES*/

typedef u8_t LIS3DH_IntPinConf_t;
typedef u8_t LIS3DH_Axis_t;
typedef u8_t LIS3DH_Int1Conf_t;


//define structure
#ifndef __SHARED__TYPES
#define __SHARED__TYPES

//typedef enum {
//  MEMS_SUCCESS				=		0x01,
//  MEMS_ERROR				=		0x00	
//} status_t;

/*
typedef enum {
  MEMS_ENABLE				=		0x01,
  MEMS_DISABLE				=		0x00	
} State_t;
*/
/*
typedef struct {
  i16_t AXIS_X;
  i16_t AXIS_Y;
  i16_t AXIS_Z;
} AxesRaw_t;
*/
#endif /*__SHARED__TYPES*/

/*
typedef enum {  
  LIS3DH_ODR_1Hz		        =		0x01,		
  LIS3DH_ODR_10Hz                      =		0x02,
  LIS3DH_ODR_25Hz		        =		0x03,
  LIS3DH_ODR_50Hz		        =		0x04,
  LIS3DH_ODR_100Hz		        =		0x05,	
  LIS3DH_ODR_200Hz		        =		0x06,
  LIS3DH_ODR_400Hz		        =		0x07,
  LIS3DH_ODR_1620Hz_LP		        =		0x08,
  LIS3DH_ODR_1344Hz_NP_5367HZ_LP       =		0x09	
} LIS3DH_ODR_t;
*/
/*
typedef enum {
  LIS3DH_POWER_DOWN                    =		0x00,
  LIS3DH_LOW_POWER 			=		0x01,
  LIS3DH_NORMAL			=		0x02
} LIS3DH_Mode_t;
*/

typedef enum {
  LIS3DH_HPM_NORMAL_MODE_RES           =               0x00,
  LIS3DH_HPM_REF_SIGNAL                =               0x01,
  LIS3DH_HPM_NORMAL_MODE               =               0x02,
  LIS3DH_HPM_AUTORESET_INT             =               0x03
} LIS3DH_HPFMode_t;

typedef enum {
  LIS3DH_HPFCF_0                       =               0x00,
  LIS3DH_HPFCF_1                       =               0x01,
  LIS3DH_HPFCF_2                       = 		0x02,
  LIS3DH_HPFCF_3                       =               0x03
} LIS3DH_HPFCutOffFreq_t;

typedef struct {
  u16_t AUX_1;
  u16_t AUX_2;
  u16_t AUX_3;
} LIS3DH_Aux123Raw_t;








typedef enum {
  LIS3DH_TRIG_INT1                     =		0x00,
  LIS3DH_TRIG_INT2 			=		0x01
} LIS3DH_TrigInt_t;

typedef enum {
  LIS3DH_SPI_4_WIRE                    =               0x00,
  LIS3DH_SPI_3_WIRE                    =               0x01
} LIS3DH_SPIMode_t;


typedef enum {
  LIS3DH_INT1_6D_4D_DISABLE            =               0x00,
  LIS3DH_INT1_6D_ENABLE                =               0x01,
  LIS3DH_INT1_4D_ENABLE                =               0x02 
} LIS3DH_INT_6D_4D_t;

typedef enum {
  LIS3DH_UP_SX                         =               0x44,
  LIS3DH_UP_DX                         =               0x42,
  LIS3DH_DW_SX                         =               0x41,
  LIS3DH_DW_DX                         =               0x48,
  LIS3DH_TOP                           =               0x60,
  LIS3DH_BOTTOM                        =               0x50
} LIS3DH_POSITION_6D_t;

typedef enum {
  LIS3DH_INT_MODE_OR                   =               0x00,
  LIS3DH_INT_MODE_6D_MOVEMENT          =               0x01,
  LIS3DH_INT_MODE_AND                  =               0x02,
  LIS3DH_INT_MODE_6D_POSITION          =               0x03  
} LIS3DH_Int1Mode_t;


//interrupt click response
//  b7 = don't care   b6 = IA  b5 = DClick  b4 = Sclick  b3 = Sign  
//  b2 = z      b1 = y     b0 = x
typedef enum {
LIS3DH_DCLICK_Z_P                      =               0x24,
LIS3DH_DCLICK_Z_N                      =               0x2C,
LIS3DH_SCLICK_Z_P                      =               0x14,
LIS3DH_SCLICK_Z_N                      =               0x1C,
LIS3DH_DCLICK_Y_P                      =               0x22,
LIS3DH_DCLICK_Y_N                      =               0x2A,
LIS3DH_SCLICK_Y_P                      =               0x12,
LIS3DH_SCLICK_Y_N			=		0x1A,
LIS3DH_DCLICK_X_P                      =               0x21,
LIS3DH_DCLICK_X_N                      =               0x29,
LIS3DH_SCLICK_X_P                      =               0x11,
LIS3DH_SCLICK_X_N                      =               0x19,
LIS3DH_NO_CLICK                        =               0x00
} LIS3DH_Click_Response; 

//TODO: start from here and manage the shared macros etc before this






// CONTROL REGISTER 1
#define LIS3DH_CTRL_REG1				0x20
#define LIS3DH_ODR_BIT				        BIT(4)
#define LIS3DH_LPEN					BIT(3)
#define LIS3DH_ZEN					BIT(2)
#define LIS3DH_YEN					BIT(1)
#define LIS3DH_XEN					BIT(0)

//CONTROL REGISTER 2
#define LIS3DH_CTRL_REG2				0x21
#define LIS3DH_HPM     				BIT(6)
#define LIS3DH_HPCF					BIT(4)
#define LIS3DH_FDS					BIT(3)
#define LIS3DH_HPCLICK					BIT(2)
#define LIS3DH_HPIS2					BIT(1)
#define LIS3DH_HPIS1					BIT(0)



//CONTROL REGISTER 6
#define LIS3DH_CTRL_REG6				0x25
#define LIS3DH_I2_CLICK				BIT(7)
#define LIS3DH_I2_INT1					BIT(6)
#define LIS3DH_I2_BOOT         			BIT(4)
#define LIS3DH_H_LACTIVE				BIT(1)

//TEMPERATURE CONFIG REGISTER
#define LIS3DH_TEMP_CFG_REG				0x1F
#define LIS3DH_ADC_PD				        BIT(7)
#define LIS3DH_TEMP_EN					BIT(6)



//REFERENCE/DATA_CAPTURE
#define LIS3DH_REFERENCE_REG		                0x26
#define LIS3DH_REF		                	BIT(0)



//STATUS_REG_AUX
#define LIS3DH_STATUS_AUX				0x07

//INTERRUPT 1 CONFIGURATION
#define LIS3DH_INT1_CFG				0x30
#define LIS3DH_ANDOR                                   BIT(7)
#define LIS3DH_INT_6D                                  BIT(6)
#define LIS3DH_ZHIE                                    BIT(5)
#define LIS3DH_ZLIE                                    BIT(4)
#define LIS3DH_YHIE                                    BIT(3)
#define LIS3DH_YLIE                                    BIT(2)
#define LIS3DH_XHIE                                    BIT(1)
#define LIS3DH_XLIE                                    BIT(0)

//FIFO CONTROL REGISTER
#define LIS3DH_FIFO_CTRL_REG                           0x2E
#define LIS3DH_FM                                      BIT(6)
#define LIS3DH_TR                                      BIT(5)
#define LIS3DH_FTH                                     BIT(0)

//CONTROL REG3 bit mask
#define LIS3DH_CLICK_ON_PIN_INT1_ENABLE                0x80
#define LIS3DH_CLICK_ON_PIN_INT1_DISABLE               0x00
#define LIS3DH_I1_INT1_ON_PIN_INT1_ENABLE              0x40
#define LIS3DH_I1_INT1_ON_PIN_INT1_DISABLE             0x00
#define LIS3DH_I1_INT2_ON_PIN_INT1_ENABLE              0x20
#define LIS3DH_I1_INT2_ON_PIN_INT1_DISABLE             0x00
#define LIS3DH_I1_DRDY1_ON_INT1_ENABLE                 0x10
#define LIS3DH_I1_DRDY1_ON_INT1_DISABLE                0x00
#define LIS3DH_I1_DRDY2_ON_INT1_ENABLE                 0x08
#define LIS3DH_I1_DRDY2_ON_INT1_DISABLE                0x00
#define LIS3DH_WTM_ON_INT1_ENABLE                      0x04
#define LIS3DH_WTM_ON_INT1_DISABLE                     0x00
#define LIS3DH_INT1_OVERRUN_ENABLE                     0x02
#define LIS3DH_INT1_OVERRUN_DISABLE                    0x00

//CONTROL REG6 bit mask
#define LIS3DH_CLICK_ON_PIN_INT2_ENABLE                0x80
#define LIS3DH_CLICK_ON_PIN_INT2_DISABLE               0x00
#define LIS3DH_I2_INT1_ON_PIN_INT2_ENABLE              0x40
#define LIS3DH_I2_INT1_ON_PIN_INT2_DISABLE             0x00
#define LIS3DH_I2_INT2_ON_PIN_INT2_ENABLE              0x20
#define LIS3DH_I2_INT2_ON_PIN_INT2_DISABLE             0x00
#define LIS3DH_I2_BOOT_ON_INT2_ENABLE                  0x10
#define LIS3DH_I2_BOOT_ON_INT2_DISABLE                 0x00
#define LIS3DH_INT_ACTIVE_HIGH                         0x00
#define LIS3DH_INT_ACTIVE_LOW                          0x02

//INT1_CFG bit mask
#define LIS3DH_INT1_AND                                0x80
#define LIS3DH_INT1_OR                                 0x00
#define LIS3DH_INT1_ZHIE_ENABLE                        0x20
#define LIS3DH_INT1_ZHIE_DISABLE                       0x00
#define LIS3DH_INT1_ZLIE_ENABLE                        0x10
#define LIS3DH_INT1_ZLIE_DISABLE                       0x00
#define LIS3DH_INT1_YHIE_ENABLE                        0x08
#define LIS3DH_INT1_YHIE_DISABLE                       0x00
#define LIS3DH_INT1_YLIE_ENABLE                        0x04
#define LIS3DH_INT1_YLIE_DISABLE                       0x00
#define LIS3DH_INT1_XHIE_ENABLE                        0x02
#define LIS3DH_INT1_XHIE_DISABLE                       0x00
#define LIS3DH_INT1_XLIE_ENABLE                        0x01
#define LIS3DH_INT1_XLIE_DISABLE                       0x00

//INT1_SRC bit mask
#define LIS3DH_INT1_SRC_IA                             0x40
#define LIS3DH_INT1_SRC_ZH                             0x20
#define LIS3DH_INT1_SRC_ZL                             0x10
#define LIS3DH_INT1_SRC_YH                             0x08
#define LIS3DH_INT1_SRC_YL                             0x04
#define LIS3DH_INT1_SRC_XH                             0x02
#define LIS3DH_INT1_SRC_XL                             0x01

//INT1 REGISTERS
#define LIS3DH_INT1_THS                                0x32
#define LIS3DH_INT1_DURATION                           0x33

//INTERRUPT 1 SOURCE REGISTER
#define LIS3DH_INT1_SRC				0x31

//FIFO Source Register bit Mask
#define LIS3DH_FIFO_SRC_WTM                            0x80
#define LIS3DH_FIFO_SRC_OVRUN                          0x40
#define LIS3DH_FIFO_SRC_EMPTY                          0x20
  
//INTERRUPT CLICK REGISTER
#define LIS3DH_CLICK_CFG				0x38
//INTERRUPT CLICK CONFIGURATION bit mask
#define LIS3DH_ZD_ENABLE                               0x20
#define LIS3DH_ZD_DISABLE                              0x00
#define LIS3DH_ZS_ENABLE                               0x10
#define LIS3DH_ZS_DISABLE                              0x00
#define LIS3DH_YD_ENABLE                               0x08
#define LIS3DH_YD_DISABLE                              0x00
#define LIS3DH_YS_ENABLE                               0x04
#define LIS3DH_YS_DISABLE                              0x00
#define LIS3DH_XD_ENABLE                               0x02
#define LIS3DH_XD_DISABLE                              0x00
#define LIS3DH_XS_ENABLE                               0x01
#define LIS3DH_XS_DISABLE                              0x00

//INTERRUPT CLICK SOURCE REGISTER
#define LIS3DH_CLICK_SRC                               0x39
//INTERRUPT CLICK SOURCE REGISTER bit mask
#define LIS3DH_IA                                      0x40
#define LIS3DH_DCLICK                                  0x20
#define LIS3DH_SCLICK                                  0x10
#define LIS3DH_CLICK_SIGN                              0x08
#define LIS3DH_CLICK_Z                                 0x04
#define LIS3DH_CLICK_Y                                 0x02
#define LIS3DH_CLICK_X                                 0x01

//Click-click Register
#define LIS3DH_CLICK_THS                               0x3A
#define LIS3DH_TIME_LIMIT                              0x3B
#define LIS3DH_TIME_LATENCY                            0x3C
#define LIS3DH_TIME_WINDOW                             0x3D



//AUX REGISTER
#define LIS3DH_OUT_1_L					0x08
#define LIS3DH_OUT_1_H					0x09
#define LIS3DH_OUT_2_L					0x0A
#define LIS3DH_OUT_2_H					0x0B
#define LIS3DH_OUT_3_L					0x0C
#define LIS3DH_OUT_3_H					0x0D

//STATUS REGISTER bit mask
#define LIS3DH_STATUS_REG_ZYXOR                        0x80    // 1	:	new data set has over written the previous one
							// 0	:	no overrun has occurred (default)	
#define LIS3DH_STATUS_REG_ZOR                          0x40    // 0	:	no overrun has occurred (default)
							// 1	:	new Z-axis data has over written the previous one
#define LIS3DH_STATUS_REG_YOR                          0x20    // 0	:	no overrun has occurred (default)
							// 1	:	new Y-axis data has over written the previous one
#define LIS3DH_STATUS_REG_XOR                          0x10    // 0	:	no overrun has occurred (default)
							// 1	:	new X-axis data has over written the previous one
#define LIS3DH_STATUS_REG_ZYXDA                        0x08    // 0	:	a new set of data is not yet avvious one
                                                        // 1	:	a new set of data is available 
#define LIS3DH_STATUS_REG_ZDA                          0x04    // 0	:	a new data for the Z-Axis is not availvious one
                                                        // 1	:	a new data for the Z-Axis is available
#define LIS3DH_STATUS_REG_YDA                          0x02    // 0	:	a new data for the Y-Axis is not available
                                                        // 1	:	a new data for the Y-Axis is available
#define LIS3DH_STATUS_REG_XDA                          0x01    // 0	:	a new data for the X-Axis is not available

#define LIS3DH_DATAREADY_BIT                           LIS3DH_STATUS_REG_ZYXDA


//STATUS AUX REGISTER bit mask
#define LIS3DH_STATUS_AUX_321OR                         0x80
#define LIS3DH_STATUS_AUX_3OR                           0x40
#define LIS3DH_STATUS_AUX_2OR                           0x20
#define LIS3DH_STATUS_AUX_1OR                           0x10
#define LIS3DH_STATUS_AUX_321DA                         0x08
#define LIS3DH_STATUS_AUX_3DA                           0x04
#define LIS3DH_STATUS_AUX_2DA                           0x02
#define LIS3DH_STATUS_AUX_1DA                           0x01

#define LIS3DH_MEMS_I2C_ADDRESS			        0x33

//FIFO REGISTERS
#define LIS3DH_FIFO_CTRL_REG			        0x2E
#define LIS3DH_FIFO_SRC_REG			        0x2F




/* Exported functions --------------------------------------------------------*/
//Sensor Configuration Functions
status_t LIS3DH_SetODR(LIS3DH_ODR_t ov);
status_t LIS3DH_SetMode(LIS3DH_Mode_t md);
status_t LIS3DH_SetAxis(LIS3DH_Axis_t axis);
status_t LIS3DH_SetFullScale(LIS3DH_Fullscale_t fs);
status_t LIS3DH_SetBDU(State_t bdu);
status_t LIS3DH_SetBLE(LIS3DH_Endianess_t ble);
status_t LIS3DH_SetSelfTest(LIS3DH_SelfTest_t st);
status_t LIS3DH_SetTemperature(State_t state);
status_t LIS3DH_SetADCAux(State_t state);

//Filtering Functions
status_t LIS3DH_HPFClickEnable(State_t hpfe);
status_t LIS3DH_HPFAOI1Enable(State_t hpfe);
status_t LIS3DH_HPFAOI2Enable(State_t hpfe);
status_t LIS3DH_SetHPFMode(LIS3DH_HPFMode_t hpf);
status_t LIS3DH_SetHPFCutOFF(LIS3DH_HPFCutOffFreq_t hpf);
status_t LIS3DH_SetFilterDataSel(State_t state);

//Interrupt Functions
status_t LIS3DH_SetInt1Pin(LIS3DH_IntPinConf_t pinConf);
status_t LIS3DH_SetInt2Pin(LIS3DH_IntPinConf_t pinConf);
status_t LIS3DH_Int1LatchEnable(State_t latch);
status_t LIS3DH_ResetInt1Latch(void);
status_t LIS3DH_SetIntConfiguration(LIS3DH_Int1Conf_t ic);
status_t LIS3DH_SetInt1Threshold(u8_t ths);
status_t LIS3DH_SetInt1Duration(LIS3DH_Int1Conf_t id);
status_t LIS3DH_SetIntMode(LIS3DH_Int1Mode_t ic);
status_t LIS3DH_SetClickCFG(u8_t status);
status_t LIS3DH_SetInt6D4DConfiguration(LIS3DH_INT_6D_4D_t ic);
status_t LIS3DH_GetInt1Src(u8_t* val);
status_t LIS3DH_GetInt1SrcBit(u8_t statusBIT, u8_t* val);

//FIFO Functions
status_t LIS3DH_FIFOModeEnable(LIS3DH_FifoMode_t fm);
status_t LIS3DH_SetWaterMark(u8_t wtm);
status_t LIS3DH_SetTriggerInt(LIS3DH_TrigInt_t tr);
status_t LIS3DH_GetFifoSourceReg(u8_t* val);
status_t LIS3DH_GetFifoSourceBit(u8_t statusBIT, u8_t* val);
status_t LIS3DH_GetFifoSourceFSS(u8_t* val);

//Other Reading Functions
status_t LIS3DH_GetStatusReg(u8_t* val);
status_t LIS3DH_GetStatusBit(u8_t statusBIT, u8_t* val);
status_t LIS3DH_GetStatusAUXBit(u8_t statusBIT, u8_t* val);
status_t LIS3DH_GetStatusAUX(u8_t* val);
status_t LIS3DH_GetAccAxesRaw(AxesRaw_t* buff);
status_t LIS3DH_GetAuxRaw(LIS3DH_Aux123Raw_t* buff);
status_t LIS3DH_GetClickResponse(u8_t* val);
status_t LIS3DH_GetTempRaw(i8_t* val);

status_t LIS3DH_Get6DPosition(u8_t* val);

//Generic
// i.e. u8_t LIS3DH_ReadReg(u8_t Reg, u8_t* Data);
// i.e. u8_t LIS3DH_WriteReg(u8_t Reg, u8_t Data);
#endif

#endif /* __LIS3DSH__H */

/******************* (C) COPYRIGHT 2012 STMicroelectronics *****END OF FILE****/



