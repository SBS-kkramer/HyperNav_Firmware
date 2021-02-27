/*! \file PDMABuffer.c *************************************************************
 *
 * \brief Peripheral Direct Memory Access Buffers
 *
 * This module implements a PDMA buffer pair 'class'. A PDMA buffer-pair is a generic
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
 *@todo Add functions documentation!!!
 **********************************************************************************/


#include <avr32/io.h>
#include "PDMABuffer.h"
#include "compiler.h"
#include "pdca.h"
#include "stdlib.h"
#include "intc.h"

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif


//! NOTE: PDMA_BUFFER_MAX*2 cannot exceed the number of available channels
#define PDMA_BUFFER_MAX 4

#define BUFF2TXCHAN(buff)       (buff*2)
#define BUFF2RXCHAN(buff)       (buff*2+1)


//*****************************************************************************
// Data structures
//*****************************************************************************
typedef struct PDMATXBUFF
{
  U8* buffer;           //!< Data buffer
  U16 len;              //!< Data buffer length
  U16 freeSpace;        //!< Available space in buffer
  U16 lastTransferLen;  //!< Amount of bytes transferred to the peripheral in the last DMA transfer
  U16 writeP;           //!< Write pointer: index from buffer beginning. Points to the next user write.

}pdmaTxBuff_t;


typedef struct PDMARXBUFF
{
  U8* buffer;           //!< Data buffer
  U16 len;              //!< Data buffer length
  U16 readP;            //!< Read pointer: an index from buffer beggining. Points to next user read.
}pdmaRxBuff_t;


typedef struct PDMABUFFERP
{

  pdmaTxBuff_t* TX;     //!< Transmission buffer
  pdmaRxBuff_t* RX;     //!< Reception buffer

}pdmaBuff_t;



//*****************************************************************************
// Module Globals
//*****************************************************************************


//! Buffer is in use flag
static Bool bufferInUse[PDMA_BUFFER_MAX];
//! Buffers
static pdmaBuff_t* buffers[PDMA_BUFFER_MAX];
//! Initialized flag
static Bool initialized = FALSE;



//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************

static U16 readBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen, Bool peek);

static pdmaBuff_t* allocateBufferSpace(U16 txLen, U16 rxLen);
static void setIRQ(__int_handler txHandler, __int_handler rxHandler, U16 txIRQ, U16 rxIRQ);

// Interrupt handlers
static void pdma_txHandler(void);
static void pdma_rxHandler(void);

// Reload PDMA controller
static U16 setNewTxTransfer(pdmaBufferHandler_t pdmaBuffer);


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************


//! new
//	returns a new buffer or PDMA_BUFFER_INVALID on failure.
pdmaBufferHandler_t pdma_newBuffer(U16 txLen, U16 txPid, U16 rxLen, U16 rxPid)
{
  S8 i;
  pdmaBuff_t* newBuffer;
  pdca_channel_options_t pdcaOpt;

  // If first call, initialize
  if(!initialized)
    {
    for(i=0; i<PDMA_BUFFER_MAX; i++)
      {
      bufferInUse[i] = FALSE;
      buffers[i] = NULL;
      }
    initialized = TRUE;
    }

  // Search for available buffer
  for(i=0; i<PDMA_BUFFER_MAX; i++)
    {
    if(!bufferInUse[i])
      break;
    }

  // If no buffers available abort
  if(i >= PDMA_BUFFER_MAX)
    {
    //DEBUG("ERROR - No PDMA buffers available\n");
    return PDMA_BUFFER_INVALID;
    }

  // Allocate and initialize buffer
  newBuffer = allocateBufferSpace(txLen, rxLen);
  if(newBuffer == NULL)
    {
    //DEBUG("ERROR - Could not allocate buffer\n");
    return PDMA_BUFFER_INVALID;
    }

  // Initialize PDCA channels
    //TX
  pdcaOpt.addr = (void*)newBuffer->TX->buffer;
  pdcaOpt.pid = txPid;
  pdcaOpt.size = 0;
  pdcaOpt.r_addr = NULL;
  pdcaOpt.r_size = 0;
  pdcaOpt.transfer_size = PDCA_TRANSFER_SIZE_BYTE;
  pdca_init_channel(BUFF2TXCHAN(i), &pdcaOpt);

    //RX
  pdcaOpt.addr = (void*)newBuffer->RX->buffer;
  pdcaOpt.pid = rxPid;
  pdcaOpt.size = 0;
  pdcaOpt.r_addr = NULL;
  pdcaOpt.r_size = 0;
  pdcaOpt.transfer_size = PDCA_TRANSFER_SIZE_BYTE;
  pdca_init_channel(BUFF2RXCHAN(i), &pdcaOpt);

  // Register interrupt handler
  setIRQ((__int_handler) &pdma_txHandler, (__int_handler) &pdma_rxHandler, AVR32_PDCA_IRQ_0 + BUFF2TXCHAN(i), AVR32_PDCA_IRQ_0 + BUFF2RXCHAN(i));

  // Flag as 'in use'
  bufferInUse[i] = TRUE;
  // Save pointer in global array
  buffers[i] = newBuffer;

  // Enable now the transfers
  pdca_enable(BUFF2TXCHAN(i));
  pdca_enable(BUFF2RXCHAN(i));


  // << Interrupt enabling MUST be at the end after the data structures are properly configured!! >>
  pdca_enable_interrupt_transfer_complete(BUFF2TXCHAN(i));
  pdca_enable_interrupt_transfer_complete(BUFF2RXCHAN(i));

  return i;
}


//! delete
S16 pdma_deleteBuffer(pdmaBufferHandler_t buffer)
{

	if((buffer >= 0) && (buffer < PDMA_BUFFER_MAX))
	{
		// Disable PDCA transfers
		pdca_disable(BUFF2TXCHAN(buffer));
		pdca_disable(BUFF2RXCHAN(buffer));

		// Disable interrupts on transfer complete
		pdca_disable_interrupt_transfer_complete(BUFF2TXCHAN(buffer));
		pdca_disable_interrupt_transfer_complete(BUFF2RXCHAN(buffer));

		// Deallocate buffers
		vPortFree(buffers[buffer]->TX->buffer);
		vPortFree(buffers[buffer]->RX->buffer);
		vPortFree(buffers[buffer]->TX);
		vPortFree(buffers[buffer]->RX);
		vPortFree(buffers[buffer]);

		buffers[buffer]= NULL;
		bufferInUse[buffer] = FALSE;

		return PDMA_OK;
	}
	else
		return PDMA_FAIL;
}


//! write
U16 pdma_writeBuffer(pdmaBufferHandler_t buffer, U8* data, U16 dataLen)
{
  pdmaTxBuff_t* txBuffer;
  U16 fs;
  U16 written;
  U16 toWrite;

  txBuffer = buffers[buffer]->TX;
  fs = txBuffer->freeSpace;

  // Calculate how many we can write (never fill all buffer space, otherwise we cannot distinguish between full and empty) => use fs-1
  toWrite = Min(fs-1, dataLen);	// fs is always >= 1 by design

  // Write data in TX buffer
  for(written = 0; written < toWrite; written++)
    {
    txBuffer->buffer[txBuffer->writeP] = data[written];
    txBuffer->writeP++;
    txBuffer->writeP %= txBuffer->len;
    }

  if(written > 0)
    {
    // <<<<< Critical Section begins >>>>>>
    pdca_disable_interrupt_transfer_complete(BUFF2TXCHAN(buffer));

    // Update free space
    txBuffer->freeSpace -= written;

    pdca_enable_interrupt_transfer_complete(BUFF2TXCHAN(buffer));
    // <<<<< Critical Section ends   >>>>>>
    }

  return written;
}


//! read
U16 pdma_readBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen)
{
  return  readBuffer(buffer, data, readLen, FALSE);
}


//! Flush read buffer
void pdma_flushReadBuffer(pdmaBufferHandler_t buffer)
{
    pdmaRxBuff_t* rxBuffer;
    U16 buffLen;
    unsigned long MAR;          // MAR (memory pointer)
    U16 mar;                    // MAR offset from buffer base address
    volatile avr32_pdca_channel_t *pdca_channel;
    U16 rPTemp;                 // Temporary storage of read pointer

    rxBuffer = buffers[buffer]->RX;
    rPTemp = rxBuffer->readP;
    buffLen = rxBuffer->len;

    // Where is MAR pointing?
    pdca_channel = pdca_get_handler(BUFF2RXCHAN(buffer));
    MAR = pdca_channel->mar;
    mar = (MAR - (unsigned long)(rxBuffer->buffer)) % buffLen;

    // "Empty the buffer"
    rxBuffer->readP = mar;

    // Re-enable interrupts on producer
    pdca_enable_interrupt_transfer_complete(BUFF2RXCHAN(buffer));
}


//! peek read
U16 pdma_peekBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen)
{
  return  readBuffer(buffer, data, readLen, TRUE);
}


//*****************************************************************************
// Interrupt handlers
//*****************************************************************************

//! TX transference complete handler
__attribute__((__interrupt__))
static void pdma_txHandler(void)
{
  U8 pdmaBuffer;
  U8 pdcaIRR;
  U8 pdcaIRQMask = 0x01;
  pdmaTxBuff_t* txBuffer;


  // IRQ Map PDCAn => IRQn
  // Even 'n' PDCA channels correspond to TX buffers
  // Odd  'n' PDCA channels correspond to RX buffers

  // Identify interrupting channel
  pdcaIRR = 0x000000FF & AVR32_INTC.irr[AVR32_PDCA_IRQ_0/AVR32_INTC_MAX_NUM_IRQS_PER_GRP];
  for(pdmaBuffer=0; pdmaBuffer<PDMA_BUFFER_MAX; pdmaBuffer++)
    {
    if(pdcaIRR & pdcaIRQMask)
      break;
    pdcaIRQMask<<=2;
    }

#ifdef DEBUG_PRN
  if(pdmaBuffer >= PDMA_BUFFER_MAX)
    {
    DEBUG("WARNING - Did not find interrupting channel\n");
    return;
    }
#endif

  txBuffer = buffers[pdmaBuffer]->TX;

  // A transfer was completed: compute free buffer space
  txBuffer->freeSpace += txBuffer->lastTransferLen;

  // Reset last transfer length (otherwise fs will be incremented multiple times per transfer)
  txBuffer->lastTransferLen = 0;

  // If the buffer is empty then disable interrupt because the triggering cause
  // (transfer count == 0) will persist
  if(txBuffer->freeSpace == txBuffer->len )
    {
    pdca_disable_interrupt_transfer_complete(BUFF2TXCHAN(pdmaBuffer));
    }
  else
    { //Else set up a new transfer and save the transfer length to be used when it is finished to
      // update the free space
    txBuffer->lastTransferLen = setNewTxTransfer(pdmaBuffer);

    }
  return;
}


//! RX transference complete handler
__attribute__((__interrupt__))
static void pdma_rxHandler(void)
{
  U8 pdmaBuffer;
  U16 mar;                    // Offset of the MAR pointer wrt buffer origin
  unsigned long MAR;          // MAR (memory pointer)
  volatile avr32_pdca_channel_t *pdca_channel;
  U8 pdcaIRR;
  U8 pdcaIRQMask = 0x02;
  U16 tCnt;                   // Next transfer count
  U16 rP;                     // Read pointer
  pdmaRxBuff_t* rxBuffer;

  // IRQ Map PDCAn => IRQn
  // Even 'n' PDCA channels correspond to TX buffers
  // Odd  'n' PDCA channels correspond to RX buffers

  // Identify interrupting channel
  pdcaIRR = 0x000000FF & AVR32_INTC.irr[AVR32_PDCA_IRQ_0/AVR32_INTC_MAX_NUM_IRQS_PER_GRP];
  for(pdmaBuffer=0; pdmaBuffer<4; pdmaBuffer++)
    {
    if(pdcaIRR & pdcaIRQMask)
      break;
    pdcaIRQMask<<=2;
    }

#ifdef DEBUG_PRN
  if(pdmaBuffer >= PDMA_BUFFER_MAX)
    {
    DEBUG("WARNING - Did not find interrupting channel\n");
    return;
    }
#endif

  rxBuffer = buffers[pdmaBuffer]->RX;

  // Where is MAR pointing?
  pdca_channel = pdca_get_handler(BUFF2RXCHAN(pdmaBuffer));
  MAR = pdca_channel->mar;
  mar = (MAR - (unsigned long)(rxBuffer->buffer))%rxBuffer->len;

  // Set up a new transfer (make 'mar' "catch" rP)
  rP = rxBuffer->readP;
  if(mar < rP )
   tCnt  = (rP-mar-1);
  else
   tCnt = rxBuffer->len - mar - ((rP==0)?1:0);

  //! @todo Maybe we want to overwrite and signal a buffer overrun instead of blocking reads....
  //!
  // If tCnt is 0, then the buffer is full, put the producer to sleep until space is available
  if(tCnt == 0)
    {
    //OPTION 1 - Disable reception
    //pdca_disable_interrupt_transfer_complete(BUFF2RXCHAN(pdmaBuffer));

    //OPTION 2 - overwrite 1 byte in the buffer
    rxBuffer->readP++;
    rxBuffer->readP%= rxBuffer->len;
    pdca_load_channel(BUFF2RXCHAN(pdmaBuffer), rxBuffer->buffer + mar, 1);
    }
  else
    {
    // Load PDCA
    pdca_load_channel(BUFF2RXCHAN(pdmaBuffer), rxBuffer->buffer + mar, tCnt);
    }
}


//*****************************************************************************
// Private Functions Implementations
//*****************************************************************************

//! \brief Allocate buffer
static pdmaBuff_t* allocateBufferSpace(U16 txLen, U16 rxLen)
{
  pdmaBuff_t* newBuffer = NULL;

  // Allocate structures
  // Bail out block
  do
    {

    newBuffer = pvPortMalloc(sizeof(pdmaBuff_t));

    if(newBuffer == NULL)
      {
      //DEBUG("ERROR - malloc failed!\n");
      break;
      }

    newBuffer->TX = pvPortMalloc(sizeof(pdmaTxBuff_t));
    newBuffer->RX = pvPortMalloc(sizeof(pdmaRxBuff_t));

    if(newBuffer->TX == NULL || newBuffer->RX == NULL)
      {
      vPortFree(newBuffer->TX);
      vPortFree(newBuffer->RX);
      vPortFree(newBuffer);
      newBuffer = NULL;
      //DEBUG("ERROR - malloc failed!");
      break;
      }

    newBuffer->TX->buffer = pvPortMalloc(txLen);
    newBuffer->RX->buffer = pvPortMalloc(rxLen);

    if(newBuffer->TX->buffer == NULL || newBuffer->RX->buffer == NULL)
          {
          vPortFree(newBuffer->TX->buffer);
          vPortFree(newBuffer->RX->buffer);
          vPortFree(newBuffer->TX);
          vPortFree(newBuffer->RX);
          vPortFree(newBuffer);
          newBuffer = NULL;
          //DEBUG("ERROR - malloc failed!");
          break;
          }

    newBuffer->TX->len = txLen;
    newBuffer->TX->freeSpace = txLen;
    newBuffer->TX->lastTransferLen = 0;
    newBuffer->TX->writeP = 0;
    newBuffer->RX->len = rxLen;
    newBuffer->RX->readP = 0;

    }
  while(0);

  return newBuffer;
}


//! \brief Init interrupt controller and register pdca_int_handler interrupt.
 //! @todo interrupt priority as parameter
static void setIRQ(__int_handler txHandler, __int_handler rxHandler, U16 txIRQ, U16 rxIRQ)
{
  Disable_global_interrupt();

  INTC_register_interrupt(txHandler, txIRQ, AVR32_INTC_INT2);
  INTC_register_interrupt(rxHandler, rxIRQ, AVR32_INTC_INT2);

  Enable_global_interrupt();
}


//! Set a new DMA transfer in a circular buffer fashion
static U16 setNewTxTransfer(pdmaBufferHandler_t pdmaBuffer)
{
  U16 marOff;       // PDCA MAR (offset from buffer beginning)
  U8* mar;
  pdmaTxBuff_t* txBuffer = buffers[pdmaBuffer]->TX;
  U16 bufferLen = txBuffer->len;
  U16 fillCnt;      // Bytes available for transfer
  U16 transferLen;

  // Calculate MAR offset
  mar = (U8*)pdca_get_handler(BUFF2TXCHAN(pdmaBuffer))->mar;
  marOff = mar - txBuffer->buffer;

  // Wrap MAR
  marOff %= bufferLen;
  mar = txBuffer->buffer + marOff;

  // Calculate bytes available for transfer
  fillCnt = (txBuffer->writeP - marOff + bufferLen)%bufferLen;

  // Calculate how many of those we can transfer
  if((marOff + fillCnt) >= bufferLen)
    transferLen = fillCnt - (marOff+fillCnt)%bufferLen;
  else
    transferLen = fillCnt;

  // load PDCA
  pdca_load_channel(BUFF2TXCHAN(pdmaBuffer), mar, transferLen);

  return transferLen;
}


//! Read buffer with peek option
static U16 readBuffer(pdmaBufferHandler_t buffer, U8* data, U16 readLen, Bool peek)
{

  pdmaRxBuff_t* rxBuffer;
  U16 buffLen;
  unsigned long MAR;          // MAR (memory pointer)
  U16 mar;                    // MAR offset from buffer base address
  U16 fillCnt;
  volatile avr32_pdca_channel_t *pdca_channel;
  U16 toRead;
  U16 read;
  U16 rPTemp;                 // Temporary storage of read pointer

  rxBuffer = buffers[buffer]->RX;
  rPTemp = rxBuffer->readP;
  buffLen = rxBuffer->len;

  // Where is MAR pointing?
  pdca_channel = pdca_get_handler(BUFF2RXCHAN(buffer));
  MAR = pdca_channel->mar;
  mar = (MAR - (unsigned long)(rxBuffer->buffer)) % buffLen;

  // Calculate valid data in buffer
  fillCnt = (mar - rPTemp + buffLen) % buffLen;

  // Read data to user buffer
  toRead = Min(readLen, fillCnt);
  for(read = 0; read < toRead; read++)
    {
    data[read] = rxBuffer->buffer[rPTemp];
    rPTemp++;
    rPTemp %= buffLen;
    }

  // If this is not a buffer peek and read something, update the read pointer and re-enable producer
  if(!peek && read > 0)
    {
    rxBuffer->readP = rPTemp;
    pdca_enable_interrupt_transfer_complete(BUFF2RXCHAN(buffer));
    }

  return read;
}

