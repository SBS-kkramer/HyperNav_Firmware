/*
 * flash_memory.h
 *
 * The spectrometer board Flash is an S25FL512S 512 Mbit / 64 MByte.
 * The sector size is 256 KBytes.
 * The page size is 512 Bytes.
 *
 * Created: 9/19/2017 3:30:06 PM
 *  Author: bplache
 */ 


#ifndef FLASH_MEMORY_H_
#define FLASH_MEMORY_H_

# include "spectrometer_data.h"

//  Use to initialize at the beginning,
//  to e.g., continue offloading.
//  Also use to clear memory.
//
int FlashMemory_Init   ( void );
int FlashMemory_IsEmpty( void );
int FlashMemory_nOfFrames( void );
int FlashMemory_AddFrame     ( Spectrometer_Data_t* frame );
int FlashMemory_RetrieveFrame( Spectrometer_Data_t* frame );

int FlashMemory_Test( char* option, char* value );

#endif /* FLASH_MEMORY_H_ */
