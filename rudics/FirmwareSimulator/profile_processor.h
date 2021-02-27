# ifndef _PROFILE_PROCESSOR_H_
# define _PROFILE_PROCESSOR_H_

# include <stdint.h>

# include "data_exchange_packet.h"

//  Some task/thread may push packets to profile_processor
//
//  Return 0  ok
//         1  queue full, try again later
//        -1  argument error
//
int16_t profile_processor_sendPacket( data_exchange_packet_t* packet );

//  Some task/thread will create/pause/resume the profile_processor
//
void profile_processor_pauseTask ( void );
void profile_processor_resumeTask( void );

//  Start the profile_processor task/thread
//
//  Return 0  ok, done
//         1  already running
//        -1  not created
//
int16_t profile_processor_createTask( void );

# endif
