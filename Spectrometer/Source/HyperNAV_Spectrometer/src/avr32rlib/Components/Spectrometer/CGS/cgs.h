/*
 * cgs.h
 *
 * Created: 7/4/2016 4:19:30 PM
 *  Author: sfeener
 */ 


#ifndef CGS_H_
#define CGS_H_

#include "compiler.h"
#include <stdint.h>

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define CGS_OK		0
#define CGS_FAIL 	-1

typedef enum {SPEC_A, SPEC_B} spec_num_t;

//*****************************************************************************
// Exported functions
//*****************************************************************************
void CGS_InitGpio( void );
void CGS_Power_On( bool A, bool B );
void CGS_Power_Off( void );

// Initialization must be called first
//
void CGS_Initialize  (bool A, bool B, bool autoClearing );
void CGS_DeInitialize(bool A, bool B);

//void CGS_StartPulse(bool runA, bool runB );

//S16 CGS_StartSampling( spec_num_t spec, F64 inttime_A_ms, F64 inttime_B_ms, int num_clearouts, U32 *specA_timeOfCompletion_ast, U32 *specB_timeOfCompletion_ast );
S16 CGS_StartSampling( spec_num_t spec, F64 inttime_ms, int num_clearouts, int num_readouts, U32 *spec_timeOfCompletion_ast );
uint16_t CGS_Get_A_EOS    (void);
uint16_t CGS_Get_B_EOS    (void);
char*    CGS_Get_A_EOScode(void);
char*    CGS_Get_B_EOScode(void);

//*****************************************************************************
// Past sampling information about the acquired data
//*****************************************************************************

//uint16_t CGS_PixelsRead_A( void );
uint32_t CGS_ClearoutBegun_A   ( void );
uint32_t CGS_IntegrationBegun_A( void );
uint32_t CGS_IntegrationEnded_A( void );
uint32_t CGS_IntegrationTime_A ( void );

//uint16_t CGS_PixelsRead_B( void );
uint32_t CGS_ClearoutBegun_B   ( void );
uint32_t CGS_IntegrationBegun_B( void );
uint32_t CGS_IntegrationEnded_B( void );
uint32_t CGS_IntegrationTime_B ( void );

//*****************************************************************************
// Exported functions
//*****************************************************************************

//void ADC_ADCPowerDown(spec_num_t spec, bool pd);

#endif /* CGS_H_ */
