/*! \file data_acquisition.h *******************************************************h
 *
 * \brief Data acquisition task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _DATA_ACQUISITION_H_
#define _DATA_ACQUISITION_H_

# include <compiler.h>

# include "data_exchange_packet.h"

//*****************************************************************************
// Data types
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

//void data_acquisition_start();
//void data_acquisition_stop();
//void data_acquisition_darkFrame();

void data_acquisition_pushPacket( data_exchange_packet_t* packet );

// Allocate the Data acquisition task
Bool data_acquisition_createTask(void);

// Resume running the task
Bool data_acquisition_resumeTask(void);

// Pause the Data acquisition task
Bool data_acquisition_pauseTask(void);

# if 0
// Get sync errors count
void  data_acquisition_getSyncErrorCnt(U16* p1, U16* p2, U16* p3);

// Reset sync errors count
void data_acquisition_resetSyncErrorCnt(void);

// Get frame count
void data_acquisition_getFrameCnt(U32* p1, U32* p2, U32* p3);

// Reset frame count
void data_acquisition_resetFrameCnt(void);
# endif

//  Interface for data consumer
//Bool data_acquisition_getAvailableFrame( HyperNav_Spectrometer_Data_t* data_frame );

#endif /* _DATA_ACQUISITION_H_ */
