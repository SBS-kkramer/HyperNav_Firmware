/*! \file stream_log.h *******************************************************h
 *
 * \brief Streaming log task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _STREAM_OUTPUT_H_
#define _STREAM_OUTPUT_H_

# if defined(OPERATION_AUTONOMOUS)

# include <compiler.h>

# include "data_exchange_packet.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

// --- No configuration ---

//*****************************************************************************
// Exported functions
//*****************************************************************************

void stream_log_pushPacket( data_exchange_packet_t* packet );

// Allocate the stream log task
Bool stream_log_createTask(void);

// Resume running the task
Bool stream_log_resumeTask(void);

// Pause the stream log task
Bool stream_log_pauseTask(void);

# if 0
// Get sync errors count
void  stream_output_getSyncErrorCnt(U16* p1, U16* p2, U16* p3);

// Reset sync errors count
void stream_output_resetSyncErrorCnt(void);

// Get frame count
void stream_output_getFrameCnt(U32* p1, U32* p2, U32* p3);

// Reset frame count
void stream_output_resetFrameCnt(void);
# endif

# endif

#endif /* _STREAM_OUTPUT_H_ */
