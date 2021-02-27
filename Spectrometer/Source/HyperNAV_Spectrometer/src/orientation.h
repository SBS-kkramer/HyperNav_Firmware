/*
 * orientation.h
 *
 * Created: 1/24/2016 12:46:29 AM
 *  Author: sfeener
 */ 


#ifndef ORIENTATION_H_
#define ORIENTATION_H_

#include <stdint.h>
#include "lis3dsh.h"

//#define USEFIFO 1

#define ORIENT_OK			0
#define ORIENT_FAIL			-1
#define ORIENT_NOT_READY	-2

int8_t ORIENT_StartAccelerometer(uint8_t twiaddr, LIS3DSH_ODR_t samplerate);
int8_t ORIENT_StopAccelerometer(uint8_t twiaddr);
int8_t ORIENT_GetAccelerometer(uint8_t twiaddr, AxesRaw_t* raw);		// reading faster than the sample rate will result in multiple reads of the same sample
int8_t ORIENT_TestAccelerometer(uint8_t twiaddr,  LIS3DSH_ODR_t samplerate);

int8_t ORIENT_CalculatePitchAndRoll(uint8_t twiaddr, AxesRaw_t rawTilts, F32 *calcPitch, F32 *calcRoll);


#endif /* ORIENTATION_H_ */