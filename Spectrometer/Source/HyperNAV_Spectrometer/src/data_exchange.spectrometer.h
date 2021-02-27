/*! \file data_exchange_spectrometer.h *******************************************************h
 *
 * \brief Data exchange spectrometer task
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _DATA_EXCHANGE_SPECTROMETER_H_
#define _DATA_EXCHANGE_SPECTROMETER_H_

# include <compiler.h>
# include "data_exchange_packet.h"

//*****************************************************************************
// Data types
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************



void data_exchange_spectrometer_pushPacket       ( data_exchange_packet_t* packet );

// Allocate the Data acquisition task
Bool data_exchange_spectrometer_createTask(void);

// Resume running the task
Bool data_exchange_spectrometer_resumeTask(void);

// Pause the Data acquisition task
Bool data_exchange_spectrometer_pauseTask(void);

#endif /* _DATA_EXCHANGE_SPECTROMETER_H_ */
