/*
 * command.spectrometer.h
 */

#ifndef _COMMAND_SPECTROMETER_H_
#define _COMMAND_SPECTROMETER_H_

# include <compiler.h>

# include "data_exchange_packet.h"

typedef enum {

	Access_User = 0,
	Access_Admin,
	Access_Factory

} Access_Mode_t;


void command_spectrometer_reboot (void);

void commander_spectrometer_pushPacket ( data_exchange_packet_t* packet );
Bool command_spectrometer_createTask(void);

#endif /* _COMMAND_SPECTROMETER_H_ */
