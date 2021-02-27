/*
 * command.controller.h
 */

#ifndef _COMMAND_CONTROLLER_H_
#define _COMMAND_CONTROLLER_H_

# include <compiler.h>

# include "data_exchange_packet.h"
# include "power.h"

typedef enum {

	Access_User = 0,
	Access_Admin,
	Access_Factory

} Access_Mode_t;

void command_controller_reboot ();
void command_controller_loop( wakeup_t wakeup_reason );

void commander_controller_pushPacket( data_exchange_packet_t* packet );
Bool command_controller_createTask(void);

S16 DialString( char dial[], int len );

#endif /* _COMMAND_CONTROLLER_H_ */
