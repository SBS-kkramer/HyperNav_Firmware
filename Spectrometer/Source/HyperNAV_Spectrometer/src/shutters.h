/*
 * shutters.h
 *
 * Created: 05/08/2016 3:47:58 PM
 *  Author: rvandommelen
 */ 


#ifndef SHUTTERS_H_
#define SHUTTERS_H_

void shutters_InitGpio(void);

void shutters_power_on( void );
void shutters_power_off( void );
void shutters_open( void );
void shutters_close( void );
void shutter_A_open( void );
void shutter_A_close( void );
void shutter_B_open( void );
void shutter_B_close( void );


#endif /* SHUTTERS_H_ */
