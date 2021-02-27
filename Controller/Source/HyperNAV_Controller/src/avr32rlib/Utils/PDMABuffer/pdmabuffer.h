/*! \file PDMABuffer.h *************************************************************
 *
 * \brief Peripheral Direct Memory Access Buffers
 *
 * This module implements a PDMA buffer-pair 'class'. A PDMA buffer-pair is a generic
 * transmission / reception circular buffer-pair that can be tied to a peripheral of choice for
 * DMA writes and reads.
 *
 * Glossary
 * - \b PDC     Peripheral DMA Controller
 * list.
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-08-07
 *
 **********************************************************************************/

#ifndef PDMABUFFER_H_
#define PDMABUFFER_H_

#include "compiler.h"


#define PDMA_OK         0
#define PDMA_FAIL       -1

#define PDMA_BUFFER_INVALID -1


//! PDMA buffer handler
typedef S8 pdmaBufferHandler_t;



//! new
//	returns a new buffer or PDMA_BUFFER_INVALID on failure.
pdmaBufferHandler_t pdma_newBuffer(U16 txLen, U16 txPid, U16 rxLen, U16 rxPid);

//! delete
//	returns PDMA_OK or PDMA_FAIL
S16 pdma_deleteBuffer(pdmaBufferHandler_t buffer);


//! write
//	returns number of bytes written
U16 pdma_writeBuffer(pdmaBufferHandler_t buffer, U8* data, U16 dataLen);


//! read
//	returns number of bytes read
U16 pdma_readBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen);


//! Flush read buffer
void pdma_flushReadBuffer(pdmaBufferHandler_t buffer);


//! peek read
//	return number of bytes read
U16 pdma_peekBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen);


#endif /* PDMABUFFER_H_ */
