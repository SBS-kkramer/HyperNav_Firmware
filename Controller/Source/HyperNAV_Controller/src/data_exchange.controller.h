/*! \file data_exchange_controller.h *******************************************************h
 *
 * \brief Data acquisition task
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _DATA_EXCHANGE_CONTROLLER_H_
#define DATA_EXCHANGE_CONTROLLER_H_


# include <compiler.h>
# include "data_exchange_packet.h"

//*****************************************************************************
// Data types
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

void data_exchange_controller_pushPacket( data_exchange_packet_t* packet );

// Allocate the Data acquisition task
Bool data_exchange_controller_createTask(void);

// Resume running the task
Bool data_exchange_controller_resumeTask(void);

// Pause the Data acquisition task
Bool data_exchange_controller_pauseTask(void);

#endif /* DATA_EXCHANGE_CONTROLLER_H_ */
