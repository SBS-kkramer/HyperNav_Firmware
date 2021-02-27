/*
 * orientation.c
 *
 * Created: 1/24/2016 12:53:09 AM
 *  Author: sfeener
 *
 *	2017-02-27, SF: Updates for system integration.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <compiler.h>

#include "twim.h"
#include "portmacro.h"
#include "orientation.h"
#include "twi_mux.h"

# include "io_funcs.spectrometer.h"

# include <FreeRTOS.h>
# include <task.h>


static const uint8_t       MUX_CHANNEL_HEADTILTS[] = {TWI_MUX_HEADTILTS};
static const uint32_t SIZE_MUX_CHANNEL_HEADTILTS   = 1;

static Bool PORT_ACC_ON = false;
static Bool STARBOARD_ACC_ON = false;



int8_t ORIENT_StartAccelerometer(uint8_t twiaddr, LIS3DSH_ODR_t samplerate)
{
	
#ifdef USEFIFO	
	// Start the specified accelerometer at the requested sample rate.
	// Enable FIFO buffer (32 samples deep), stop when complete

	
	// Set up the TWI mux to transmit on the Tilt channels
	if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_HEADTILTS, SIZE_MUX_CHANNEL_HEADTILTS, TWI_MUX_ADDRESS, FALSE) ) {
		return ORIENT_FAIL;
	}
	
	uint8_t val;

	// Clear FIFO by enabling bypass mode - other bits of register can be set to 0 (more efficient then read, change bit, then write)
	//if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_CTRL_REG6, (uint8_t)(1<<LIS3DSH_FIFO_EN))) 
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_CTRL_REG6, 0x42)) 
		return ORIENT_FAIL;
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_CTRL_REG6, &val);
	portDBG_TRACE("CTRL_REG_6 = %#X", val);	
		
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_FIFO_CTRL, 0))
		return ORIENT_FAIL;
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_FIFO_CTRL, &val);
	portDBG_TRACE("FIFO_CTRL = %#X", val);	
		
	// Enable FIFO mode
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_FIFO_CTRL, (uint8_t)(1<<LIS3DSH_FMODE0)))
		return ORIENT_FAIL;
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_FIFO_CTRL, &val);
	portDBG_TRACE("FIFO_CTRL = %#X", val);
		
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_CTRL_REG6, &val);
	portDBG_TRACE("CTRL_REG_6 = %#X", val);	
		
	// accelerometer is now ready to start - set rate
	if (LIS3DSH_SetODR(twiaddr, samplerate) != MEMS_SUCCESS)
		return ORIENT_FAIL;
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_CTRL_REG4, &val);
	portDBG_TRACE("CTRL_REG_4 = %#X", val);
	
	// accelerometer now running, time to complete is ~ 32/samplerate
#else	//FIFO bypassed

io_out_string("ORI StopAcc\r\n");
	// stop accelerometer  (performs mux select)
	if (ORIENT_StopAccelerometer(twiaddr) == ORIENT_FAIL)
		return ORIENT_FAIL;
	
io_out_string("ORI AntiAliasing\r\n");
	// Set anti-aliasing analog filter to 50Hz, fullscale to 2G, disable selftest
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_CTRL_REG5, 0xC0))
		return ORIENT_FAIL;
	
io_out_string("ORI FIFO\r\n");
	// enable FIFO
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_CTRL_REG6, (uint8_t)(1<<LIS3DSH_FIFO_EN)))
		return ORIENT_FAIL;
	
io_out_string("ORI ByPass\r\n");
	// set bypass mode
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_FIFO_CTRL, 0))
		return ORIENT_FAIL;
	
io_out_string("ORI XYZ, BDU\r\n");
	// enable XYX axes, enable BDU, set rate
	if (!LIS3DSH_WriteReg(twiaddr, LIS3DSH_CTRL_REG4, 0x0F))
		return ORIENT_FAIL;
	if (LIS3DSH_SetODR(twiaddr, samplerate) == MEMS_ERROR)
		return ORIENT_FAIL;

#endif

	if (twiaddr == PORT_ACCELEROMETER_ADDRESS)
		PORT_ACC_ON = true;

	if (twiaddr == STARBOARD_ACCELEROMETER_ADDRESS)
		STARBOARD_ACC_ON = true;	

	return ORIENT_OK;	
}

int8_t ORIENT_StopAccelerometer(uint8_t twiaddr)
{

	// Set up the TWI mux to transmit on the Tilt channels
	if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_HEADTILTS, SIZE_MUX_CHANNEL_HEADTILTS, TWI_MUX_ADDRESS, false) ) {
		return ORIENT_FAIL;
	}
	
  io_out_string ( "MUXed\r\n" );
  vTaskDelay((portTickType)TASK_DELAY_MS(1000));

	if (LIS3DSH_SetODR(twiaddr, LIS3DSH_ODR_PWR_DOWN) != MEMS_SUCCESS)
		return ORIENT_FAIL;
	
  io_out_string ( "ODRed\r\n" );
  vTaskDelay((portTickType)TASK_DELAY_MS(1000));

	
	if (twiaddr == PORT_ACCELEROMETER_ADDRESS)
		PORT_ACC_ON = false;
	if (twiaddr == STARBOARD_ACCELEROMETER_ADDRESS)
		STARBOARD_ACC_ON = false;
		
	return ORIENT_OK;	
}

int8_t ORIENT_GetAccelerometer(uint8_t twiaddr, AxesRaw_t* raw)
{
	// Set up the TWI mux to transmit on the Tilt channels
	if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_HEADTILTS, SIZE_MUX_CHANNEL_HEADTILTS, TWI_MUX_ADDRESS, false) ) {
		return ORIENT_FAIL;
	}
	
	switch (twiaddr) {
		case PORT_ACCELEROMETER_ADDRESS:
			if (!PORT_ACC_ON)	return ORIENT_FAIL;
			break;
		case STARBOARD_ACCELEROMETER_ADDRESS:
			if (!STARBOARD_ACC_ON)	return ORIENT_FAIL;
			break;
		default:
			return ORIENT_FAIL;
	}
		
	uint8_t val;
	
	if (LIS3DSH_DataAvailable(twiaddr, &val) == MEMS_ERROR)
		return ORIENT_NOT_READY;
		
	if (LIS3DSH_GetAccAxesRaw(twiaddr, raw) == MEMS_ERROR)
		return ORIENT_FAIL;
	
	return ORIENT_OK;
}


int8_t ORIENT_TestAccelerometer(uint8_t twiaddr,  LIS3DSH_ODR_t samplerate)
{
	int ii;
	for ( ii=0; ii<2; ii++ ) {
#define SIZE_MUX_CHANNEL 1
uint8_t MUX_CHANNEL;

MUX_CHANNEL = TWI_MUX_ECOMPASS;

if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
  io_out_string ( "notMXed 2\r\n" );
  return ORIENT_FAIL;
}
io_out_string ( "MXed 2\r\n" );
vTaskDelay((portTickType)TASK_DELAY_MS(1000));

MUX_CHANNEL = TWI_MUX_SPEC_TEMP_A;

if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
  io_out_string ( "notMXed 4\r\n" );
  return ORIENT_FAIL;
}
io_out_string ( "MXed 4\r\n" );
vTaskDelay((portTickType)TASK_DELAY_MS(1000));

MUX_CHANNEL = TWI_MUX_SPEC_TEMP_B;

if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
  io_out_string ( "notMXed 8\r\n" );
  return ORIENT_FAIL;
}
io_out_string ( "MXed 8\r\n" );
vTaskDelay((portTickType)TASK_DELAY_MS(1000));

MUX_CHANNEL = TWI_MUX_HEADTILTS;

if ( STATUS_OK != twim_write(TWI_MUX_PORT, &MUX_CHANNEL, SIZE_MUX_CHANNEL, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
  io_out_string ( "notMXed 1\r\n" );
  return ORIENT_FAIL;
}
io_out_string ( "MXed 1\r\n" );
vTaskDelay((portTickType)TASK_DELAY_MS(1000));
	}

	int i;
	AxesRaw_t raw;
	float pitch, roll;
	
	// Set up the TWI mux to transmit on the Tilt channels
	if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_HEADTILTS, SIZE_MUX_CHANNEL_HEADTILTS, TWI_MUX_ADDRESS, false) ) {
		return ORIENT_FAIL;
	}

  io_out_string ( "MUXed\r\n" );
  vTaskDelay((portTickType)TASK_DELAY_MS(1000));
	
	if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_HEADTILTS, SIZE_MUX_CHANNEL_HEADTILTS, TWI_MUX_ADDRESS, false) ) {
		return ORIENT_FAIL;
	}

  io_out_string ( "MUXed\r\n" );
  vTaskDelay((portTickType)TASK_DELAY_MS(1000));
	
	uint8_t val;

#ifdef USEFIFO	// attempt to use FIFO
  io_out_string ( "USEFIFO\r\n" );
	uint8_t tst_bit = 0;
	uint8_t val;
	
	
	// start accelerometer
	if (ORIENT_StartAccelerometer(twiaddr, samplerate) == ORIENT_FAIL)
		return ORIENT_FAIL;
	
	// wait for completion
	do {
		if (LIS3DSH_FIFOFull(twiaddr, &tst_bit) == MEMS_ERROR)
			break;
		portDBG_TRACE("waiting for FIFO on %#X, rate=%#X", twiaddr, samplerate);
	} while (tst_bit == 0);

	// buffers should now be full.  
	
	
	// display results?
	portDBG_TRACE("X\tY\tZ");
	for (i=0; i<32; i++) {
		if (LIS3DSH_GetAccAxesRaw(twiaddr, &raw) == MEMS_ERROR) {
			return ORIENT_FAIL;
		}
		if (LIS3DSH_ReadReg(twiaddr, LIS3DSH_FIFO_SRC, &val) == MEMS_ERROR)
			return ORIENT_FAIL;
			
		portDBG_TRACE("%d\t%d\t%d\t%#X", raw.AXIS_X, raw.AXIS_Y,raw.AXIS_Z, val);
	}
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_FIFO_CTRL, &val);
	portDBG_TRACE("FIFO_CTRL: %#X", val);
	LIS3DSH_WriteReg(twiaddr, LIS3DSH_FIFO_CTRL, 0);		//enable bypass mode
	LIS3DSH_ReadReg(twiaddr, LIS3DSH_FIFO_CTRL, &val);
	portDBG_TRACE("FIFO_CTRL: %#X", val);

	//ORIENT_StopAccelerometer(twiaddr);
	
	// try soft reset
	//LIS3DSH_SoftReset(twiaddr);
	}
#else // FIFO in bypass mode
		
  io_out_string ( "BYPASS FIFO\r\n" );
	// start accelerometer
	ORIENT_StartAccelerometer(twiaddr, samplerate);		
		
	// now, test
	portDBG_TRACE("\t\tX\tY\tZ");
#if 0	
	for (i = 0; i<100; i++) {
		// wait for new data
		val = 0;
		do {
			LIS3DSH_NewDataAvailable(twiaddr, &val);
		} while (val == 0);
		LIS3DSH_GetAccAxesRaw(twiaddr, &raw);
		
		ORIENT_CalculatePitchAndRoll(twiaddr, raw, &pitch, &roll);
		portDBG_TRACE("Sample %d:\t%d\t%d\t%d\tPitch: %.2f\tRoll: %.2f", i+1, raw.AXIS_X, raw.AXIS_Y, raw.AXIS_Z, pitch, roll);
	}
#else	
	i=0;
	int16_t try=100;
	do {
		//portDBG_TRACE("ret = %d", ret);	
		switch ( ORIENT_GetAccelerometer(twiaddr, &raw) ) {
			case ORIENT_NOT_READY:
				try--;
				break;
			case ORIENT_FAIL:
				ORIENT_StopAccelerometer(twiaddr);
				return ORIENT_FAIL;
				break;
			case ORIENT_OK:
				i++;
				try=100;
				ORIENT_CalculatePitchAndRoll(twiaddr, raw, &pitch, &roll);
				portDBG_TRACE("Sample %d:\t%d\t%d\t%d\tPitch: %.2f\tRoll: %.2f", i+1, raw.AXIS_X, raw.AXIS_Y, raw.AXIS_Z, pitch, roll);
				{
char xyzpr[64]; snprintf ( xyzpr, 63, "%4d\t%d\t%d\t%d\t%.2f\t%.2f\r\n", i, raw.AXIS_X, raw.AXIS_Y, raw.AXIS_Z, pitch, roll );
io_out_string ( xyzpr );
				}
				break;	
			default:
				break;	
		}
	} while (i<100 && try>0);
#endif	
	// stop accelerometer
	ORIENT_StopAccelerometer(twiaddr);	
	
#endif		

	return ORIENT_OK;
}

#if 0	
		// start accelerometers
		if (sysInit.portAcc)	ORIENT_StartAccelerometer(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_3_125Hz);	// LIS3DSH_ODR_100Hz);
		
		if (sysInit.strbrdAcc)	ORIENT_StartAccelerometer(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_3_125Hz);	//LIS3DSH_ODR_100Hz);
		// wait - 32 sample buffer @ 100 Hz = 0.32s
		uint8_t tst_bit ;
		if (sysInit.portAcc) {
			tst_bit = 0;
			do {
				if (LIS3DSH_FIFOFull(PORT_ACCELEROMETER_ADDRESS, &tst_bit) == MEMS_ERROR)
				break;
			} while (tst_bit == 0);
		}
		if (sysInit.strbrdAcc) {
			tst_bit = 0;
			do {
				if (LIS3DSH_FIFOFull(STARBOARD_ACCELEROMETER_ADDRESS, &tst_bit) == MEMS_ERROR)
				break;
			} while (tst_bit == 0);
		}
		// buffers should now be full
		
		
		// readout
		int i;
		AxesRaw_t rawP, rawS;
		portDBG_TRACE("Xp\tYp\tZp\tXs\tYs\tZs");
		for (i=0; i<32; i++) {
			if (sysInit.portAcc) {
				LIS3DSH_GetAccAxesRaw(PORT_ACCELEROMETER_ADDRESS, &rawP);
			}
			if (sysInit.strbrdAcc) {
				LIS3DSH_GetAccAxesRaw(STARBOARD_ACCELEROMETER_ADDRESS, &rawS);
			}
			portDBG_TRACE("%d\t%d\t%d\t%d\t%d\t%d", (sysInit.portAcc)?(rawP.AXIS_X):(100000), (sysInit.portAcc)?(rawP.AXIS_Y):(100000), \
			(sysInit.portAcc)?(rawP.AXIS_Z):(100000), (sysInit.strbrdAcc)?(rawS.AXIS_X):(100000), \
			(sysInit.strbrdAcc)?(rawS.AXIS_Y):(100000), (sysInit.strbrdAcc)?(rawS.AXIS_Z):(100000));
		}
#endif


int8_t ORIENT_CalculatePitchAndRoll(uint8_t twiaddr, AxesRaw_t rawTilts, F32 *calcPitch, F32 *calcRoll)
{
	// default sensitivities are 0.06 mg/digit!!  from datasheet, may need to be calibrated
	#define PORT_SENSITIVITY_X		0.00006;				//0.001 = 1 mg/count
	#define PORT_SENSITIVITY_Y		0.00006;
	#define PORT_SENSITIVITY_Z		0.00006;
	#define STARBOARD_SENSITIVITY_X	0.00006;
	#define STARBOARD_SENSITIVITY_Y	0.00006;
	#define STARBOARD_SENSITIVITY_Z	0.00006;
	#define PORT_PITCH_OFFSET		0.0;
	#define PORT_ROLL_OFFSET		0.0;
	#define STARBOARD_PITCH_OFFSET	0.0;
	#define STARBOARD_ROLL_OFFSET	0.0;
	#warning Accelerometers may need calibration - need storage and retrieval functions for these values
	
	float  x, y, z, off_p, off_r;
	
	if (twiaddr == PORT_ACCELEROMETER_ADDRESS) {
		x = PORT_SENSITIVITY_X;
		y = PORT_SENSITIVITY_Y;
		z = PORT_SENSITIVITY_Z;
		off_p = PORT_PITCH_OFFSET;
		off_r = PORT_ROLL_OFFSET;
	}
	
	else if (twiaddr == STARBOARD_ACCELEROMETER_ADDRESS) {
		x = STARBOARD_SENSITIVITY_X;
		y = STARBOARD_SENSITIVITY_Y;
		z = STARBOARD_SENSITIVITY_Z;
		off_p = STARBOARD_PITCH_OFFSET;
		off_r = STARBOARD_ROLL_OFFSET;
	}
	
	else
		return ORIENT_FAIL;
	
	*calcPitch = atan2( (y*rawTilts.AXIS_Y), sqrt(pow(x*rawTilts.AXIS_X,2) + pow(z*rawTilts.AXIS_Z,2))  ) * 180.0/M_PI;
	*calcRoll = atan2( (x*rawTilts.AXIS_X) , sqrt(pow(y*rawTilts.AXIS_Y,2) + pow(z*rawTilts.AXIS_Z,2))  ) * 180.0/M_PI;

	// correct for small offsets
	*calcPitch -= off_p;
	*calcRoll -= off_r;
	
	return ORIENT_OK;
}



#if 0   // old testing code
#if 1

LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG3, &val);
portDBG_TRACE("Port CTRL_REG3:0x%X", val);

//LIS3DSH_SoftReset(PORT_ACCELEROMETER_ADDRESS);
//portDBG_TRACE("Port soft reset");
//cpu_delay_ms(20, FOSC0);

LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_INFO1, &val);
portDBG_TRACE("Port INFO1:0x%X", val);
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_INFO2, &val);
portDBG_TRACE("Port INFO2:0x%X", val);
//for (i = 0; i < 3; i++) {
//	LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG3, &val);
//	portDBG_TRACE("Port CTRL_REG3:0x%X", val);
//}
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Port CTRL_REG4:0x%X", val);
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG5, &val);
portDBG_TRACE("Port CTRL_REG5:0x%X", val);
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG6, &val);
portDBG_TRACE("Port CTRL_REG6:0x%X", val);
LIS3DSH_GetStatusReg(PORT_ACCELEROMETER_ADDRESS, &val) ;
portDBG_TRACE("Port status:0x%X", val);
#endif
#if 0	// probe PORT accelerometer address
if (twim_probe(LIS3DSH_TWI_PORT, PORT_ACCELEROMETER_ADDRESS) == STATUS_OK) {
	portDBG_TRACE("Port acc found");
}
else {
	portDBG_TRACE("Port acc not found");
}
#endif

#if 1	// Check WHO_AM_I register for PORT accelerometer
if (LIS3DSH_GetWHO_AM_I(PORT_ACCELEROMETER_ADDRESS, &val) == MEMS_SUCCESS) {
	//portDBG_TRACE("WhoAmI=0x%X", val);		// should return 0x3F
	if (val == 0x3F) {
		portDBG_TRACE("Port tilt detected");
	}
	else {
		portDBG_TRACE("Port tilt not detected");
	}
}
val = 0;
#endif

#if 1
LIS3DSH_GetStatusReg(PORT_ACCELEROMETER_ADDRESS, &val) ;
portDBG_TRACE("Port status:0x%X", val);

LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Port CTRL_REG4:0x%X", val);
LIS3DSH_SetBDU(PORT_ACCELEROMETER_ADDRESS, true);
portDBG_TRACE("Port BDU set");
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Port CTRL_REG4:0x%X", val);

LIS3DSH_SetODR(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_100Hz);
portDBG_TRACE("ODR set");
LIS3DSH_ReadReg(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Port CTRL_REG4:0x%X", val);

for (i=0; i<3; i++)		{
	LIS3DSH_GetTemp(PORT_ACCELEROMETER_ADDRESS, &temperature);
	portDBG_TRACE("Port t:%d", (int16_t)temperature);
}

LIS3DSH_GetStatusReg(PORT_ACCELEROMETER_ADDRESS, &val) ;
portDBG_TRACE("Port status:0x%X", val);

for (i=0; i<10; i++) {
	if ( LIS3DSH_GetAccAxesRaw(PORT_ACCELEROMETER_ADDRESS, &raw) == MEMS_SUCCESS ) {
		portDBG_TRACE("%d %d %d", raw.AXIS_X, raw.AXIS_Y, raw.AXIS_Z);
	}
	else {
		portDBG_TRACE("Error reading acc")	;
	}
}

#endif
#if 0
LIS3DSH_GetStatusReg(STARBOARD_ACCELEROMETER_ADDRESS, &val) ;
portDBG_TRACE("Strbrd status:0x%X", val);

LIS3DSH_ReadReg(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Strbrd CTRL_REG4:0x%X", val);
LIS3DSH_SetBDU(STARBOARD_ACCELEROMETER_ADDRESS, true);
portDBG_TRACE("Strbrd BDU set", val);
LIS3DSH_ReadReg(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Strbrd CTRL_REG4:0x%X", val);

LIS3DSH_SetODR(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_100Hz);
portDBG_TRACE("ODR set");
LIS3DSH_ReadReg(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Strbrd CTRL_REG4:0x%X", val);

for (i=0; i<3; i++)		{
	LIS3DSH_GetTemp(STARBOARD_ACCELEROMETER_ADDRESS, &temperature);
	portDBG_TRACE("Strbrd t:%d", (int16_t)temperature);
}

LIS3DSH_GetStatusReg(STARBOARD_ACCELEROMETER_ADDRESS, &val) ;
portDBG_TRACE("Strbrd status:0x%X", val);


LIS3DSH_GetAccAxesRaw(STARBOARD_ACCELEROMETER_ADDRESS, &raw);
portDBG_TRACE("%d %d %d", raw.AXIS_X, raw.AXIS_Y, raw.AXIS_Z);


#endif
#if 0
LIS3DSH_SoftReset(STARBOARD_ACCELEROMETER_ADDRESS);
portDBG_TRACE("Strbrd soft reset");
#endif
#if 0	// probe STARBOARD accelerometer address
if (twim_probe(LIS3DSH_TWI_PORT, STARBOARD_ACCELEROMETER_ADDRESS) == STATUS_OK) {
	portDBG_TRACE("Starboard acc found");
}
else {
	portDBG_TRACE("Starboard acc not found");
}
#endif
#if 0
if (LIS3DSH_GetWHO_AM_I(STARBOARD_ACCELEROMETER_ADDRESS, &val) == MEMS_SUCCESS) {
	//portDBG_TRACE("WhoAmI=0x%X", val);		// should return 0x3F
	if (val == 0x3F) {
		portDBG_TRACE("Starboard tilt detected");
	}
	else {
		portDBG_TRACE("Starboard tilt not detected");
	}
}
#endif
#if 0
LIS3DSH_ReadReg(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Strbrd ODR:0x%X", val);
LIS3DSH_SetODR(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_100Hz);
LIS3DSH_ReadReg(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_CTRL_REG4, &val);
portDBG_TRACE("Strbrd ODR:0x%X", val);
LIS3DSH_GetTemp(STARBOARD_ACCELEROMETER_ADDRESS, &temperature);
portDBG_TRACE("Strbrd t:%d", (int16_t)temperature);
#endif





#endif

