/*! \file SPECtasks.c
 *
 *	\brief	Spectrometer tasks configuration and handling
 *
 * Created: 7/26/2015 9:01:58 PM
 *  Author: Scott
 */ 

#include "FreeRTOS.h"
#include "task.h"

#include "SPECtasks.h"

//*****************************************************************************
// Exported handlers
//*****************************************************************************

// Main Spectrometer Task
xTaskHandle gMainSPECTaskHandler = NULL;