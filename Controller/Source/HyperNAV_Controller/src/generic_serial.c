/*! \file   generic_serial.c   *********************************************
 *
 *  \brief  Generic interface to a serial port
 *
 *  @author BPlache, Satlantic
 *  @date   2016-02-26
 *
 ***************************************************************************/

# include "generic_serial.h"

#include "compiler.h"
// #include <string.h>
// #include <time.h>
#include "usart.h"
#include "gpio.h"
// #include "cycle_counter.h"
#include "pdmabuffer.h"
#include "avr32rerrno.h"
// #include "syslog.h"
//#include "HyperNAV/E980030.h"
#include "board.h"
#include "io_funcs.controller.h"

# include "FreeRTOS.h"
# include "task.h"

//*****************************************************************************
// Local variables
//*****************************************************************************

# define MAX_PORTS  2
# define OCR_PORT   0
# define MCOMS_PORT 1

static pdmaBufferHandler_t gsp_pdmaBuffer  [MAX_PORTS] = { PDMA_BUFFER_INVALID };
static uint32_t            gsp_fPBA        [MAX_PORTS] = { 0 };
static Bool                gsp_initialized [MAX_PORTS] = { FALSE };
static Bool                gsp_rxEnabled   [MAX_PORTS] = { FALSE };
static Bool                gsp_txEnabled   [MAX_PORTS] = { FALSE };

#define OCR_ON_PIN             AVR32_PIN_PA20
#define MCOMS_ON_PIN           AVR32_PIN_PA19
//#define OCR_TXBUF_LEN          16
//#define OCR_RXBUF_LEN          48
//#define OCR_DISCARDPOS         2
//#define MCOMS_TXBUF_LEN        16
//#define MCOMS_RXBUF_LEN        48
//#define MCOMS_DISCARDPOS       2
#define SENSOR_RE_N            AVR32_PIN_PA07
#define SENSOR_DE              AVR32_PIN_PX51

//! \brief Enable/Disable reception
//! @param enable   If true reception is enabled, if false reception is disabled. Use as needed for power saving.
//! @note Reception starts enabled by default.
static void gsp_rxen( /* generic_serial_port_t gsp, */ Bool enable );


//! \brief Enable/Disable transmission
//! @param enable   If true transmission is enabled, if false transmission is disabled. Use as needed for power saving.
//! @note Reception starts enabled by default.
static void gsp_txen( /* generic_serial_port_t gsp, */ Bool enable );

//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! \brief Initialize the specified port
int8_t GSP_Init( generic_serial_port_t gsp, uint32_t fPBA, uint32_t baudrate, uint16_t incoming_frame_length, uint16_t outgoing_command_length )
{
  volatile avr32_usart_t* the_usart;
  uint16_t port_index;

  switch ( gsp ) {
    case GSP_OCR:   port_index =   OCR_PORT; the_usart =   OCR_USART; break;
    case GSP_MCOMS: port_index = MCOMS_PORT; the_usart = MCOMS_USART; break;
  default:
    avr32rerrno = EUSART;
    return 1;
  }

  // Save fPBA
  gsp_fPBA [port_index ] = fPBA;

  if ( GSP_OCR == gsp ) {

    // Initialize USART
    static const gpio_map_t OCR_USARTPinMap = {
      {OCR_USART_RX_PIN, OCR_USART_RX_FUNCTION},
      {OCR_USART_TX_PIN, OCR_USART_TX_FUNCTION}
    };

    // Set up GPIO for OCR_USART, size of the GPIO map is 2 here.
    gpio_enable_module ( OCR_USARTPinMap, sizeof(OCR_USARTPinMap) / sizeof(OCR_USARTPinMap[0]) );

    // Configure switch and turn power on to OCR504
    gpio_enable_gpio_pin( OCR_ON_PIN );
    gpio_set_gpio_pin( OCR_ON_PIN );

  } else if ( GSP_MCOMS == gsp ) {
	//io_out_string("confirm\r\n");

    // Initialize USART
    static const gpio_map_t MCOMS_USARTPinMap = {
      {MCOMS_USART_RX_PIN, MCOMS_USART_RX_FUNCTION},
      {MCOMS_USART_TX_PIN, MCOMS_USART_TX_FUNCTION}
    };

    // Set up GPIO for MCOMS_USART, size of the GPIO map is 2 here.
    gpio_enable_module ( MCOMS_USARTPinMap, sizeof(MCOMS_USARTPinMap) / sizeof(MCOMS_USARTPinMap[0]) );

    // Configure switch and turn power on to MCOMS
    gpio_enable_gpio_pin( MCOMS_ON_PIN );
    gpio_set_gpio_pin( MCOMS_ON_PIN );

  }
  
  // Configure control lines for MAX3222
  // Have MAX3222 turned off initially (gets turned on later)
  gpio_enable_gpio_pin( SENSOR_RE_N );
  gpio_set_gpio_pin( SENSOR_RE_N );
  gpio_enable_gpio_pin( SENSOR_DE );
  gpio_clr_gpio_pin( SENSOR_DE );
  
  // Options for USART.  Except baudrate, all ports use the same options.
  usart_options_t USART_OPTIONS = {
  
    .baudrate = baudrate,
    .charlength = 8,
    .paritytype = USART_NO_PARITY,
    .stopbits = USART_1_STOPBIT,
    .channelmode = USART_NORMAL_CHMODE
  };

  // Initialize USART in RS232 mode.
  if ( usart_init_rs232 ( the_usart, &USART_OPTIONS, fPBA ) != USART_SUCCESS ) {

    avr32rerrno = EUSART;
    return 1;
  }

  // Initialize buffer
  if ( GSP_OCR == gsp ) {
   if ( (gsp_pdmaBuffer[port_index] = pdma_newBuffer( outgoing_command_length, OCR_PDCA_PID_TX, 4*incoming_frame_length, OCR_PDCA_PID_RX))
                  == PDMA_BUFFER_INVALID) {

     avr32rerrno = EBUFF;
     return 1;
   }
  } else  if ( GSP_MCOMS == gsp ) {
   if ( (gsp_pdmaBuffer[port_index] = pdma_newBuffer( outgoing_command_length, MCOMS_PDCA_PID_TX, 4*incoming_frame_length, MCOMS_PDCA_PID_RX))
                  == PDMA_BUFFER_INVALID) {

     avr32rerrno = EBUFF;
     return 1;
   }
  }

  // Enable reception and transmission
  gsp_rxen( /* gsp, */ TRUE );
  gsp_txen( /* gsp, */ TRUE );

  // Flag initialized
  gsp_initialized[port_index] = TRUE;

  // This port was initialized
  return 0;
}

int8_t GSP_Deinit( generic_serial_port_t gsp )
{
  uint16_t port_index;

  switch ( gsp ) {
    case GSP_OCR:
      port_index =   OCR_PORT;
      // Turn sensor power off
      gpio_clr_gpio_pin( OCR_ON_PIN );
      // Shut down the USART
      usart_reset( OCR_USART );
      // Disabling the MAX3222 transceiver closes BOTH channels,
      // so check if the other channel is already de-initialized.
      if (gsp_initialized[MCOMS_PORT] == FALSE) {
          gsp_rxen( /* gsp, */ FALSE );
          gsp_txen( /* gsp, */ FALSE );
      }
      break;
    case GSP_MCOMS:
      port_index = MCOMS_PORT;
      // Turn sensor power off
      gpio_clr_gpio_pin( MCOMS_ON_PIN );
      // Shut down the USART
      usart_reset( MCOMS_USART );
      // Disabling the MAX3222 transceiver closes BOTH channels,
      // so check if the other channel is already de-initialized.
      if (gsp_initialized[OCR_PORT] == FALSE) {
          gsp_rxen( /* gsp, */ FALSE );
          gsp_txen( /* gsp, */ FALSE );
      }
      break;
    default:
      avr32rerrno = EUSART;
      return 1;
  }
  
  // Flag de-initialized
  gsp_initialized[port_index] = FALSE;

  // Remove the buffer
  if ( PDMA_OK == pdma_deleteBuffer( gsp_pdmaBuffer[port_index] ) ) {
    gsp_pdmaBuffer[port_index] = PDMA_BUFFER_INVALID;
    // This port was deinitialized
    return 0;
  } else {
    return -1;
  }
}

//! \brief Send data through port
int32_t GSP_Send( generic_serial_port_t gsp, void const* buffer, uint16_t size, uint16_t flags )
{
  if ( NULL == buffer || 0 == size ) {       // Sanity check
    return 0;
  }

  uint16_t port_index;

  switch ( gsp ) {
  case GSP_OCR:   port_index =   OCR_PORT; break;
  case GSP_MCOMS: port_index = MCOMS_PORT; break;
  default:
    avr32rerrno = EUSART;
    return -1;
  }

  if ( !gsp_txEnabled[port_index] ) {           // Check for port enabled

    avr32rerrno = ERRIO;
    return -1;
  }

  pdmaBufferHandler_t pdmaBuffer = gsp_pdmaBuffer[port_index];

  if(flags & GSP_NONBLOCK) {   // Non-blocking call

    return pdma_writeBuffer ( pdmaBuffer, (uint8_t*)buffer, size );

  } else {                   // Blocking call
                             // also implements slow send by pausing between characters
    uint16_t written = 0;

    do {
      vTaskDelay( (portTickType)TASK_DELAY_MS(10) );
      written += pdma_writeBuffer ( pdmaBuffer, ((uint8_t*)buffer+written), 1 );
    } while(written < size);
  
    return written;
  }
}


//! \brief Receive data through port
int32_t GSP_Recv( generic_serial_port_t gsp, void /*const*/* buffer, uint16_t size, uint16_t flags )
{
  uint16_t port_index;

  switch ( gsp ) {
  case GSP_OCR:   port_index =   OCR_PORT; break;
  case GSP_MCOMS: port_index = MCOMS_PORT; break;
  default:
    avr32rerrno = EUSART;
    return -1;
  }

  if(buffer == NULL) {      // Sanity check

    avr32rerrno = EPARAM;
    return -1;
  }

  if ( !gsp_rxEnabled[port_index] ) {          // Check for receiver enabled

    avr32rerrno = ERRIO;
    return -1;
  }

  pdmaBufferHandler_t pdmaBuffer = gsp_pdmaBuffer[port_index];
  
  if(flags & GSP_NONBLOCK) {  // Non-blocking call

    if(flags & GSP_PEEK) {
      return pdma_peekBuffer( pdmaBuffer, (uint8_t*)buffer, size);
	} else {
      return pdma_readBuffer( pdmaBuffer, (uint8_t*)buffer, size);
	}

  } else {                  // Blocking call

    uint16_t read = 0;

	//io_out_string("blocking\r\n");
    do {
      if(flags & GSP_PEEK)
        read += pdma_peekBuffer( pdmaBuffer, ((uint8_t*)buffer+read), size-read);
      else
        read += pdma_readBuffer( pdmaBuffer, ((uint8_t*)buffer+read), size-read);

    } while ( read < size );

    return read;
  }
}


//! \brief Flush port reception buffer.
//! This function discards all unread data.
void GSP_FlushRecv( generic_serial_port_t gsp )
{
  switch ( gsp ) {
  case GSP_OCR:   pdma_flushReadBuffer(gsp_pdmaBuffer[  OCR_PORT]); break;
  case GSP_MCOMS: pdma_flushReadBuffer(gsp_pdmaBuffer[MCOMS_PORT]); break;
  }
}



//! \brief Enable/Disable reception
// Note that the OCR and MCOMS channels cannot be controlled independently
static void gsp_rxen( /* generic_serial_port_t gsp, */ Bool enable )
{
    if(enable)
        gpio_clr_gpio_pin(SENSOR_RE_N);
    else
        gpio_set_gpio_pin(SENSOR_RE_N);

    // Flag state
    gsp_rxEnabled[OCR_PORT] = enable;
    gsp_rxEnabled[MCOMS_PORT] = enable;

    return;
}


//! \brief Enable/Disable transmission
// Note that the OCR and MCOMS channels cannot be controlled independently
static void gsp_txen( /* generic_serial_port_t gsp, */ Bool enable )
{
    if(enable) {
        gpio_set_gpio_pin(SENSOR_DE);
        // 100uS delay after coming back from shutdown (see Maxim's MAX3222 datasheet)
        // We will wait 1 ms.
        vTaskDelay( (portTickType)TASK_DELAY_MS(1) );
    } else
        gpio_clr_gpio_pin(SENSOR_DE);

    // Flag state
    gsp_txEnabled[OCR_PORT] = enable;
    gsp_txEnabled[MCOMS_PORT] = enable;

    return;

}


