/*! \file profile_manager.h *******************************************************h
 *
 * \brief profile manager task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _PROFILE_MANAGER_H_
#define _PROFILE_MANAGER_H_

# if defined(OPERATION_NAVIS)

# include <compiler.h>

# include "data_exchange_packet.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

// --- No configuration ---

//*****************************************************************************
// Exported functions
//*****************************************************************************

void profile_manager_pushPacket( data_exchange_packet_t* packet );

// for testing
// int16_t profile_fake( uint16_t profile_id );

// Allocate the profile manager task
Bool profile_manager_createTask(void);

// Resume running the task
Bool profile_manager_resumeTask(void);

// Pause the profile manager task
Bool profile_manager_pauseTask(void);

char const* profile_manager_LogDirName ();

# endif

#endif /* _PROFILE_MANAGER_H_ */
