/*! \file aux_data_acquisition.h *******************************************************h
 *
 * \brief Auxiliary data acquisition task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _AUX_DATA_ACQUISITION_H_
#define _AUX_DATA_ACQUISITION_H_

# include <compiler.h>

# include "data_exchange_packet.h"

//*****************************************************************************
// Data types
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

void aux_data_acquisition_pushPacket( data_exchange_packet_t* packet );

// Allocate the Data acquisition task
Bool aux_data_acquisition_createTask(void);

// Resume running the task
Bool aux_data_acquisition_resumeTask(void);

// Pause the Data acquisition task
Bool aux_data_acquisition_pauseTask(void);

#endif /* _AUX_DATA_ACQUISITION_H_ */
