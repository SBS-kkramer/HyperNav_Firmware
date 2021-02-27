/*! \file modem.c *************************************************************
 *
 * \brief Modem port and modem driver API
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2010-11-10
 *
 **********************************************************************************/

#include "compiler.h"

# include <string.h>
# include <stdlib.h>
# include <stdio.h>
# include <ctype.h>
# include <time.h>
# include <sys/time.h>

#include "usart.h"
#include "gpio.h"
#include "cycle_counter.h"
#include "pdmabuffer.h"
#include "avr32rerrno.h"
#include "modem.h"
#include "delay.h"

#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#include "semphr.h"
#endif

# define MDM_DBG 0
# if MDM_DBG
# include "telemetry.h"
# endif

# define MDM_LOG 0
# if MDM_LOG
# include <stdio.h>
# include "files.h"
# include "telemetry.h"
# define MDM_LOG_FILE "mdm.log"
static int mdm_log_state = 0; // -1 == RX  +1 == TX
# endif

//****************n************************************************************
// Local types
//*****************************************************************************



//*****************************************************************************
// Local variables
//*****************************************************************************
static pdmaBufferHandler_t mdmBuffer = -1;
static U32 g_fPBA = 0;
static U32 g_baudrate = 0;
static Bool rxEnabled = FALSE;
static Bool txEnabled = FALSE;
static Bool modemInitialized = FALSE;

#ifdef FREERTOS_USED
static xSemaphoreHandle mdmSyncMutex = NULL;
#define MDM_SYNCOBJ_CREATE()    {if(NULL == (mdmSyncMutex = xSemaphoreCreateMutex()))return MDM_FAIL;}
#define MDM_SYNCOBJ_REQUEST()   ( pdTRUE == xSemaphoreTake(mdmSyncMutex, 1000) )
#define MDM_SYNCOBJ_RELEASE()   xSemaphoreGive(mdmSyncMutex)
#else
#define MDM_SYNCOBJ_CREATE()
#define MDM_SYNCOBJ_REQUEST()   (1)
#define MDM_SYNCOBJ_RELEASE()
#endif

/* Motorola's ISU AT Command Reference indicates max command length of 128 bytes */
#define MAXCMDLEN 128

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************



//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! \brief Initialize modem port sub-system
S16 mdm_init(U32 fPBA, U32 baudrate)
{
  if  (!modemInitialized)
  {
    // Save fPBA and baudrate
    g_fPBA = fPBA;
    g_baudrate = baudrate;

    // Initialize USART
    static const gpio_map_t mdmUSARTPinMap =
    {
      {MDM_USART_RX_PIN, MDM_USART_RX_FUNCTION},
      {MDM_USART_TX_PIN, MDM_USART_TX_FUNCTION}
    };

    // Options for USART.
    usart_options_t MDM_USART_OPTIONS =
    {
      .baudrate = baudrate,
      .charlength = 8,
      .paritytype = USART_NO_PARITY,
      .stopbits = USART_1_STOPBIT,
      .channelmode = USART_NORMAL_CHMODE
    };

    // Set up GPIO for MDM_USART, size of the GPIO map is 2 here.
    gpio_enable_module (mdmUSARTPinMap, sizeof(mdmUSARTPinMap) / sizeof(mdmUSARTPinMap[0]));

    // Initialize it in RS232 mode.
    if  (usart_init_rs232 (MDM_USART, &MDM_USART_OPTIONS, fPBA) != USART_SUCCESS)
    {
      avr32rerrno = EUSART;
      return MDM_FAIL;
    }

    // Initialize buffer
    if  ((mdmBuffer = pdma_newBuffer (MDM_TX_BUF_LEN, MDM_PDCA_PID_TX, MDM_RX_BUF_LEN, MDM_PDCA_PID_RX)) == PDMA_BUFFER_INVALID)
    {
      avr32rerrno = EBUFF;
      return MDM_FAIL;
    }

    // Initialize access control mutex
    MDM_SYNCOBJ_CREATE();

    // Enable reception and transmission
    mdm_rxen (TRUE);
    mdm_txen (TRUE);

    // Flag USART modem initialized
    modemInitialized = TRUE;

    // Initialize the GPIO lines
    gpio_enable_gpio_pin (MDM_DSR);
    gpio_enable_gpio_pin (MDM_DTR);
    gpio_enable_gpio_pin (MDM_RTS);
    gpio_enable_gpio_pin (MDM_CTS);
    gpio_enable_gpio_pin (MDM_CD_PIN);

    // We *might* need to use:
    //gpio_enable_pin_pull_up( MDM_DSR );
    
    //  Assert DTR and RTS
    mdm_dtr(1);
    mdm_rts(1);
  }

  // All sub-systems initialized
  return MDM_OK;
}


//! \brief Deinitialize Modem Port (deallocate resources)
void mdm_deinit(void)
{
  if  (modemInitialized)
  {
    // Deallocate Peripheral DMA Transfer buffer
    if  (mdmBuffer != -1)
    {
      pdma_deleteBuffer (mdmBuffer);
      mdmBuffer = -1;
    }

    //  Turn off RS-232 transmitter
    gpio_clr_gpio_pin (MDM_DE);

    modemInitialized = FALSE;
  }
}


//! \brief Send data through modem port
S16 mdm_send (void const* buffer, 
              U16         size,
              U16         flags,
              U16         blocking_timeout /* seconds */
             )
{
  // Sanity check
  if  (!buffer || !size)
  {
    return 0;
  }

  // Check for driver enabled
  if  (!txEnabled)
  {
    avr32rerrno = ERRIO;
    return -1;
  }

  U16 written = 0;

  if  (flags & MDM_USE_CTS  &&  !mdm_get_cts ())
  {
    time_t const t_end = time((time_t*)0) + blocking_timeout;  //   FIXME  -  Replace time() by gettimeofday()

    while  (!mdm_get_cts ()  &&  time( (time_t*)0 ) < t_end)
    {
      vTaskDelay((portTickType)TASK_DELAY_MS(100));
    }

    if  (!mdm_get_cts ())
      return 0;
  }

  // Request access to TX buffer
  if ( MDM_SYNCOBJ_REQUEST() )
  {
    // Non-blocking call
    if  (flags & MDM_NONBLOCK)
    {
      if ( flags & MDM_USE_CTS )
      {
        time_t const t_end = time((time_t*)0) + blocking_timeout;  //   FIXME  -  Replace time() by gettimeofday()

        while ( written < size && time( (time_t*)0 ) < t_end )
        {
          if ( mdm_get_cts() )
          {
            written += pdma_writeBuffer( mdmBuffer, ((U8*)buffer+written), 1 );
          }
        }
      }
      else
      {
        written = pdma_writeBuffer(mdmBuffer, (U8*)buffer, size);
      }
    }
    else
    // Blocking call
    {
      time_t const t_end = time((time_t*)0) + blocking_timeout;  //   FIXME  -  Replace time() by gettimeofday()

      if ( flags & MDM_USE_CTS )
      {
        while ( written < size && time( (time_t*)0 ) < t_end )
        {
          if ( mdm_get_cts() )
          {
             written += pdma_writeBuffer( mdmBuffer, ((U8*)buffer+written), 1 );
          }
          else
          {
               vTaskDelay((portTickType)TASK_DELAY_MS(500));
	        }
        }
      }
      else
      {
        do
        {
          written += pdma_writeBuffer(mdmBuffer, ((U8*)buffer+written), size-written);
        }  while (written < size && time((time_t*)0)<t_end );
      }
    }

    // Release access to TX buffer. Other threads can now access the write buffers
    MDM_SYNCOBJ_RELEASE();
  }

# if MDM_LOG
  if  ( written > 0 )
  {
    fHandler_t fh;

    U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);

    if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) )
    {
      if ( -1 == mdm_log_state )
      {
        f_write ( &fh, "\r\n", 2 );
        mdm_log_state = 0;
      }

      if ( 0 == mdm_log_state )
      {
        struct timeval now;
        gettimeofday( &now, (void*)0 );
        char ts[64];
        snprintf ( ts, 63, "T %6ld\r\n", now.tv_sec-2085978900 );
        f_write ( &fh, ts, strlen(ts) );
        f_write ( &fh, "\t>  ", 4 );
        mdm_log_state = 1;
      }

      int w;
      for ( w=0; w<written; w++ )
      {
        char c = ((char*)buffer)[w];

        if ( c=='\r' )
        {
          f_write ( &fh, "\\r", 2 );
        }
        else if ( c=='\n' )
        {
          f_write ( &fh, "\\n", 2 );
        }
        else
        {
          f_write ( &fh, &c, 1 );
        }
      }

      f_close( &fh );
    }
  }
# endif

  return written;
}


//! \brief Receive data via modem
S16 mdm_recv(void* buffer, U16 size, U16 flags)
{
  // Sanity check
  if ( !buffer || !size )
  {
    return 0;
  }

  // Check for receiver enabled
  if  (!rxEnabled)
  {
    avr32rerrno = ERRIO;
    return MDM_FAIL;
  }

  U16 read = 0;

  // Non-blocking call
  if  (flags & MDM_NONBLOCK)
  {
    if  (flags & MDM_PEEK)
      read = pdma_peekBuffer(mdmBuffer, (U8*)buffer, size);
    else
      read = pdma_readBuffer(mdmBuffer, (U8*)buffer, size);
  }
  else
  // Blocking call
  {
    do
    {
      if  (flags & MDM_PEEK)
        read += pdma_peekBuffer(mdmBuffer, ((U8*)buffer+read), size-read);
      else
        read += pdma_readBuffer(mdmBuffer, ((U8*)buffer+read), size-read);

    } while(read < size);
  }

# if MDM_LOG
  if  ( !(flags & MDM_PEEK) && read > 0 )
  {
    fHandler_t fh;

    U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);

    if  ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) )
    {
      if ( 1 == mdm_log_state )
      {
        f_write ( &fh, "\r\n", 2 );
        mdm_log_state = 0;
      }

      if ( 0 == mdm_log_state )
      {
        struct timeval now;
        gettimeofday( &now, (void*)0 );
        char ts[64];
        snprintf ( ts, 63, "T %6ld\r\n", now.tv_sec-2085978900 );
        f_write ( &fh, ts, strlen(ts) );
        f_write ( &fh, "\t < ", 4 );
        mdm_log_state = -1;
      }

      int r;
      for  (r = 0;  r < read;  r++)
      {
        char c = ((char*)buffer)[r];

        if ( c=='\r' )
        {
          f_write ( &fh, "\\r", 2 );
        }
        else if ( c=='\n' )
        {
          f_write ( &fh, "\\n", 2 );
        }
        else
        {
          f_write ( &fh, &c, 1 );
        }
      }

      f_close( &fh );
    }
  }
# endif

  return read;
}



void mdm_log (int action)
{
# if MDM_LOG
  fHandler_t fh;
  
  if  (FILE_OK == f_open( MDM_LOG_FILE, O_RDONLY, &fh ))
  {
    int read = 0;
    do
    {
      char block[32];
      read = f_read (&fh, block, 32);
      tlm_send (block, 32, 0);
    } while  (read == 32);
    
    f_close (&fh);
  }
  
  tlm_send ( "\r\n", 2, 0 );
  
  if ( 1==action )
  {
    f_delete ( MDM_LOG_FILE );
  }
# endif
}


//! \brief Flush modem reception buffer.
//! This function discards all unread data.
void mdm_flushRecv (void)
{
  pdma_flushReadBuffer (mdmBuffer);
}


//! \brief Enable/Disable reception
void mdm_rxen(Bool enable)
{
#if ( (BOARD==USER_BOARD)  &&  ((MCU_BOARD_DN==E980030) || (MCU_BOARD_DN==E980030_DEV_BOARD)) )

  if  (enable)
  {
    gpio_clr_gpio_pin (MDM_RXEN_n);
  }
  else
  {
    gpio_set_gpio_pin (MDM_RXEN_n);
  }

//#elif BOARD==EVK1104
#else
    #error "Unhandled hardware variant. Cannot build binary"
#endif

  // Flag state
  rxEnabled = enable;
}



//! \brief Enable/Disable transmission
void  mdm_txen (Bool enable)
{
#if ( (BOARD==USER_BOARD) && ((MCU_BOARD_DN==E980030)||(MCU_BOARD_DN==E980030_DEV_BOARD)) )

  if  (enable)
  {
    gpio_set_gpio_pin (MDM_DE);
    // 100uS delay after coming back from shutdown (see Maxim's MAX3222 datasheet)
    cpu_delay_us (100, g_fPBA);
  }
  else
  {
    gpio_clr_gpio_pin (MDM_DE);
  }

#else
    #error "Unhandled hardware variant. Cannot build binary"
#endif

  // Flag state
  txEnabled = enable;
}


//! \brief Dynamically set modem port baudrate
S16 mdm_setBaudrate (U32 baudrate)
{
  // Options for USART.
  usart_options_t MDM_USART_OPTIONS =
  {
    .baudrate = baudrate,
    .charlength = 8,
    .paritytype = USART_NO_PARITY,
    .stopbits = USART_1_STOPBIT,
    .channelmode = USART_NORMAL_CHMODE
  };

  // Verify l_fPBA initialized
  if  (g_fPBA == 0)
  {
    avr32rerrno = EUSART;
    return MDM_FAIL;
  }

  // Re-initialize usart in RS232 mode with new baudrate
  if  (usart_init_rs232 (MDM_USART, &MDM_USART_OPTIONS, g_fPBA) != USART_SUCCESS)
  {
    avr32rerrno = EUSART;
    return MDM_FAIL;
  }

  // Update baudrate
  g_baudrate = baudrate;

  return MDM_OK;
}



//! \brief Detect soft break, ignore all other inputs
Bool mdm_softBreak(char softBreakChar)
{
  char c=0;

  while  (mdm_recv (&c, 1, MDM_NONBLOCK) == 1)
  {
    if  (c==softBreakChar)    // If soft break received
    {
      mdm_flushRecv ();    // Flush all other data in buffer including repeated soft break chars
      return TRUE;
    }
  }

  return FALSE;  // Soft break not received
}



int mdm_get_dtr ()
{
  return 1-gpio_get_pin_value ( MDM_DTR );
}



int mdm_get_dsr()
{
  return 1-gpio_get_pin_value ( MDM_DSR );
}



int mdm_carrier_detect ()
{
  return 1-gpio_get_pin_value ( MDM_CD_PIN );
}



void mdm_dtr ( int assert )
{
  if  ( assert )
  {
    gpio_clr_gpio_pin (MDM_DTR);
  }
  else
  {
    gpio_set_gpio_pin (MDM_DTR);
  }
  vTaskDelay ((portTickType)TASK_DELAY_MS(250));
}



int mdm_get_cts ()
{
  return  1 - gpio_get_pin_value ( MDM_CTS );
}



void mdm_rts( int assert )
{
  if  ( assert )
  {
   gpio_clr_gpio_pin (MDM_RTS);
  }
  else
  {
    gpio_set_gpio_pin (MDM_RTS);
  }
  vTaskDelay ((portTickType)TASK_DELAY_MS(250));
}



void mdm_dtr_toggle_off_on() {
    mdm_dtr(0);
    mdm_dtr(1);
}



int mdm_wait_for_dsr ()
{

  //
  //  Toggle DTR (Data Terminal Ready) line,
  //  and expect modem to assert DSR (Data Set Ready) line.
  //
  //  Give it a few tries.
  //
  //  Normally, the modem should respond to an assertion of DTR
  //  by asserting DSR within a few 100 milliseconds.
  //
  //  Modem behaviour at deasserting DTR is controlled via
  //  the "AT &D2" configuration command (see mdm_configure()).
  //

  if  ( mdm_get_dsr() )
    return 1;

  int nTry = 5;

  do
  {
    mdm_dtr_toggle_off_on();

    int nWait = 5*(10-nTry);

    while ( !mdm_get_dsr() && --nWait>0 )
    {
      vTaskDelay ((portTickType)TASK_DELAY_MS (50));
    }

  } while (!mdm_get_dsr () && --nTry > 0);

  return mdm_get_dsr ();
}



///////////////   Below is code pulled from NAVIS firmware, significantly stripped down
//

/**
   This function transmits a command string to the modem and verifies
   the expected response.  The command string should not include the
   termination character (\r), as this function transmits the termination
   character after the command string is transmitted.  The command string may
   include wait-characters ('~') to make the processing pause as needed.
   The command string is processed each byte in turn.  Each time a
   wait-character is encountered, processing is halted for one wait-period
   (delay_sec) and then processing is resumed.

      \begin{verbatim}
      input:

         command....The command string to transmit.
                    A non-existing or empty command implies
                    that the function listens only for the
                    expect response.

         expect.....The expected response to the command string.
                    A non-existing or empty expected implies
                    that the function sends only the command.

         delay_sec..The number of seconds this function will attempt to
                    match the prompt-string.
                    If delay_sec is less than 2 it will be forced to be 2.

         trm........A termination string transmitted immediately after the
                    command string.  For example, if the termination string
                    is "\r\n" then the command string will be transmitted
                    first and followed immediately by the termination
                    string.  If trm=NULL or trm="" then no termination
                    string is transmitted.

      output:

         This function returns a positive number if the exchange was
         successful.  Zero is returned if the exchange failed.  A negative
         number is returned if the function parameters were determined to be
         ill-defined.
      \end{verbatim}

   written by Dana Swift, adapted to hypernav by BP
*/

int mdm_chat( char const* command, char const* expect, time_t delay_sec, char const* trm ) {

# if MDM_LOG
    {
        fHandler_t fh;
        U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);
        if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) ) {

            f_write ( &fh, "\r\nCHAT  ", 8 );

            int w;

            for ( w=0; w<strlen(command); w++ ) {
              char c = ((char*)command)[w];

              if ( c=='\r' ) {
                f_write ( &fh, "\\r", 2 );
              } else if ( c=='\n' ) {
                f_write ( &fh, "\\n", 2 );
              } else {
                  f_write ( &fh, &c, 1 );
              }
            }
            for ( w=0; w<strlen(trm); w++ ) {
              char c = ((char*)trm)[w];

              if ( c=='\r' ) {
                f_write ( &fh, "\\r", 2 );
              } else if ( c=='\n' ) {
                f_write ( &fh, "\\n", 2 );
              } else {
                  f_write ( &fh, &c, 1 );
              }
            }
            f_write ( &fh, "  --expect--  ", 14 );

            for ( w=0; w<strlen(expect); w++ ) {
              char c = ((char*)expect)[w];

              if ( c=='\r' ) {
                f_write ( &fh, "\\r", 2 );
              } else if ( c=='\n' ) {
                f_write ( &fh, "\\n", 2 );
              } else {
                  f_write ( &fh, &c, 1 );
              }
            }
            f_write ( &fh, "\r\n", 2 );

            f_close( &fh );
        }
    }
# endif

  /* define the logging signature */
//static cc FuncName[] = "chat()";

  int status = -1;

  /* flush the input buffer prior to sending the command string */
  mdm_flushRecv();

//mdm_dtr_toggle_off_on();

  /* transmit the command to the modem */
  if ( command && *command ) {

    /* define the wait-character the wait period */
    unsigned char const wait_char = '~';
    int const wait_period_ms = 1000;  /*  milliseconds  */

    /* compute the length of the command string */
    int const len=strlen(command);

    int i;
    for (status=0, i=0; i<len; i++) {

      /* check if the current byte is the wait-character */
      if( command[i] == wait_char ) {

        vTaskDelay( (portTickType)TASK_DELAY_MS( wait_period_ms ) );

      /*  Send all other characters as is  */
      } else if ( 1 != mdm_send ( command+i, 1, MDM_NONBLOCK, 0 ) ) {

# if MDM_LOG
        fHandler_t fh;
        U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);
        if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) ) {

            f_write ( &fh, "ERROR ", 6 );

            if ( command[i]=='\r' ) {
                f_write ( &fh, "\\r", 2 );
            } else if ( command[i]=='\n' ) {
                f_write ( &fh, "\\n", 2 );
            } else {
                f_write ( &fh, command+i, 1 );
            }
            f_write ( &fh, "\r\n", 2 );
            f_close( &fh );
        }
# endif
        /* create the message */
      //static cc format[]="Attempt to send command string (%s) failed.\n";

        /* log the message */
      //LogEntry(FuncName,format,command);

        goto Err;
      }
    }
  }

  /* transmit the (optional) command termination to the modem */
  if( trm && *trm ) {

    /* compute the length of the termination string */
    int const len=strlen(trm);

    /* transmit the command termination to the modem */
    int i;
    for (i=0; i<len; i++) {

      if ( 1 != mdm_send ( trm+i, 1, MDM_NONBLOCK, 0 ) ) {

        /* create the message */
      //static cc format[]= "Attempt to send termination string (%s) failed.\n";

        /* log the message */
      //LogEntry(FuncName,format,trm);

# if MDM_LOG
        fHandler_t fh;
        U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);
        if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) ) {

            f_write ( &fh, "ERROR ", 6 );

            if ( trm[i]=='\r' ) {
                f_write ( &fh, "\\r", 2 );
            } else if ( trm[i]=='\n' ) {
                f_write ( &fh, "\\n", 2 );
            } else {
                f_write ( &fh, trm+i, 1 );
            }
            f_write ( &fh, "\r\n", 2 );
            f_close( &fh );
        }
# endif
        goto Err;

      } else {
      //if (debuglevel>=3 || (debugbits&CHAT_H)) {
      //  if (trm[i]=='\r') LogAdd("\\r");
      //  else if (trm[i]=='\n') LogAdd("\\n");
      //  else if (isprint(trm[i])) LogAdd("%c",trm[i]);
      //  else LogAdd("[0x%02x]",trm[i]);
      //}
      }
    }
  }

  /* terminate the last logentry */
//if (debuglevel>=3 || (debugbits&CHAT_H)) LogAdd("\n");

  /* seek the expect string in the modem response */
  if (expect && *expect) {

    int nRX = 0;

    /* work around a time descretization problem */
    if (delay_sec<=1) delay_sec=2;

    /* get the reference time and set the current time */
    time_t       Tnow = time((time_t*)0);  //   FIXME  -  Change time functions to gettimeofday()
    time_t const Tend = Tnow + delay_sec;

    /* compute the length of the prompt string */
    int const len=strlen(expect);

    /* define the index of expected string */
    int i=0;

    /* make a log entry */
  //if (debuglevel>=3 || (debugbits&CHAT_H)) {
  //  static cc msg[]="Received: "; LogEntry(FuncName,msg);
  //}

    int n=0;

    do {

      unsigned char byte;
      /* read the next byte from the modem */
      if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {

        mdm_recv ( &byte, 1, MDM_NONBLOCK );
        nRX++;

# if MDM_DBG
        tlm_send ( "\r\nmdm_chat< ", 12, 0 );
        if ( byte == '\r' ) {
          tlm_send ( "CR", 2, 0 );
        } else if ( byte == '\n' ) {
          tlm_send ( "LF", 2, 0 );
        } else {
          tlm_send ( &byte, 1, 0 );
        }
        tlm_send ( " >\r\n", 4, 0 );
# endif

        /* write the current byte to the logstream */
      //if (debuglevel>=3 || (debugbits&CHAT_H)) {
      //  if (byte=='\r') LogAdd("\\r");
      //  else if (byte=='\n') LogAdd("\\n");
      //  else if (isprint(byte)) LogAdd("%c",byte);
      //  else LogAdd("[0x%02x]",byte);
      //}

        /*  check if the current byte matches the expected byte from the prompt */
        if( toupper(byte)==toupper(expect[i]) ) {
          i++;
        } else {
          //  Start over
          i=0;
        }

        /*  the expect-string has been found if the index (i) matches its length */
        if (i>=len) {
          status=1;
          break;
        }

        /* periodically update the current time;
         * otherwise, might get stuck in the do{}while loop if there is constant byte receiving */
        if ( n > 25 ) {
          Tnow = time((time_t*)0);  //  FIXME  -  Change time functions to gettimeofday()
          n=0;
        } else {
          n++;
        }

      } else {

        vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );

        /* get the current time */
        Tnow = time((time_t*)0);  //  FIXME  -  Change time functions to gettimeofday()
      }

    /* check the termination conditions */
    } while( Tnow < Tend );   //  FIXME  -  Change time functions to gettimeofday()

    /* terminate the last logentry */
  //if (debuglevel>=3 || (debugbits&CHAT_H)) LogAdd("\n");

    /* report a failure or success (in DEBUG mode only) */
    if (status<=0) {

      /* create the message */
    //static cc format[]="Expected string [%s] not received.\n";

      /* log the message */
    //LogEntry(FuncName,format,expect);

    /* report a successful chat session */
    } else {
    //if (debuglevel>=3 || (debugbits&CHAT_H)) {
    //  /* create the message */
    //  static cc format[]="Expected response [%s] received.\n";
    //  /* log the message */
    //  LogEntry(FuncName,format,expect);
    //}
    }

# if MDM_LOG
    {
        fHandler_t fh;
        U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);
        if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) ) {

            char rt[32];
            snprintf ( rt, sizeof(rt)-1, "\r\nCHAT RX %d\r\n", nRX );
            f_write ( &fh, rt, strlen(rt) );

            f_close( &fh );
        }
    }
# endif


  } else {
    // No response was required.
    // All is ok.
    status=1;
  }

  Err: //  Jump here if an unrecoverable error is encountered

  return status;
}

/*------------------------------------------------------------------------*/
/* function to read a termination specified string from the serial port   */
/*------------------------------------------------------------------------*/
/**
   This function reads (and stores in a buffer) bytes from the serial port
   until one of three termination criteria are satisfied.

      \begin{verbatim}
      1) A specified termination string is read.  For example, if the
         termination string is "\r\n" then once this string is read from the
         serial port the function returns the string read up to that
         point.  The termination string itself is discarded.

      2) A specified maximum number of bytes are read.  This criteria
         prevents buffer overflow.

      3) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if insufficient data are
         available after a specified number of seconds.
      \end{verbatim}

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.getb, buf, and trm are initialized with non-NULL values and
   that the maximum buffer size is strictly positive.

      \begin{verbatim}
      input:

         size....The maximum number of bytes that will be read from the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
j                 function will attempt to read bytes from the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>0.

         trm.....The termination string.  For example, if the termination
                 string is "\r\n" then once this string is read from the
                 serial port the function returns the string read up to that
                 point.  The termination string is discarded.

      output:

         buf.....The buffer into which the bytes that are read from the
                 serial port will be stored.  This buffer must be at least
                 (size+1) bytes.  Although the termination string (trm) is
                 not returned, the buffer must be large enough to contain
                 all bytes read including the termination string.

         This function returns the number of bytes read from the serial port
         including the termination string (if a termination string was
         read).

      \end{verbatim}

   Written by Dana Swift
*/
static int mdm_pgets(char *buf, int size, time_t sec, const char *trm)
{
   if (!buf || size<=0 || sec<0 ) {
      return -1;
   }

   /* enforce some termination string */
   if (!trm) trm="\n";

   /* compute the length of the termination string */
   int trmlen=strlen(trm);

   /* flag that is set to 1 when termination string found */
   int trm_found=0;

   /* record the current time */
   time_t To=0,T=To;

   /* initialize the byte counter and the buffer */
   int n=0;
   buf[0]=0;


   ////    FIXME  --  There could be a bug in this code !!!
   //                 if size is not large enough to hold
   //                 the return plus termination, the
   //                 return value could be 'n' before the
   //                 termination sequence arrived.
   do {

      /* attempt to read the next byte from modem port */
      unsigned char byte;
      if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {

         mdm_recv ( &byte, 1, MDM_NONBLOCK );

         /* increment the byte count and re-terminate the buffer */
         buf[n] = byte;
         n++;
         buf[n] = 0;

         /* check for the line terminator string */
         if (n>=trmlen && !strcmp(buf+n-trmlen,trm)) {

            /* remove the termination string from the buffer */
            trm_found=1;
            buf[n-trmlen]=0;
         }
      } else {
         vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );
         /* recompute time-criteria for loop termination */
         T=time(NULL); if (!To) {To=T;}
         if (!(T>=0 && To>=0 && sec>=0 && difftime(T,To)<=sec)) break;
      }

   } while (n<size && !trm_found); /* check termination criteria */

   return n;
}

//
//  Generic modem IO function:
//  Send a command (sender must ensure it is '\r' terminated!
//  If key provided, scan modem output lines for key,
//  and copy the content of the line past the key to response.
//  For each line, allow rx_timeout seconds.
//  Expect OK as final modem output.
//  Abort on ERROR from modem.
//
//  Return 1 if key found and OK found
//
static int mdm_getResponseString(
                char const* command,
                char const* key,
                char const* failed,
                int needOK,
                int rx_timeout,
                char response[],
                size_t response_length ) {

  if ( !command || !mdm_get_dsr() ) {
    return -1;
  }

  /* flush the IO port buffers */
  mdm_flushRecv();

  int  const cLen = strlen(command);
  time_t const  send_timeout = 2;

  //
  //  Transmit the command to the modem.
  //
  if ( cLen != mdm_send( command, cLen, 0, send_timeout ) ) {
    return -1;
  }

# if MDM_DBG
  tlm_send( command, cLen, 0 );
  tlm_send( "\r\n", 2, 0 );
# endif

  //
  //  The expected response is:
  //
  //    [Echo Back]   <command>\r
  //    [CRLF]        \r\n
  //    [Response]    <key><response>\r\n
  //    [Other]       <some string>\r\n
  //    [CRLF]        \r\n
  //    [OK]          OK\r\n
  //

  char buf[64];
  int needKey   = ( key && key[0] );
  int gotKey    = 0;
  int gotOK     = 0;
  int gotFailed = 0;

  while ( mdm_get_dsr()
       && (  ( needOK  && !gotOK )
          || ( needKey && !gotKey ) )
       && !gotFailed
       && mdm_pgets(buf,sizeof(buf),rx_timeout,"\r\n")>0) {

# if MDM_DBG
    tlm_send( "<< ", 3, 0 );
    tlm_send( buf, strlen(buf), 0 );
    tlm_send( " >>\r\n", 5, 0 );
# endif

    if ( key && key[0] ) {
      //
      // Scan received line for key,
      // then return the matched string
      //
      char* p;

      if ((p=strstr(buf,key))) {

        gotKey = 1;

        if ( response && response_length>0 ) {
          strncpy( response, p+strlen(key), response_length );
        }

# if MDM_DBG
        tlm_send( "\r\nKEY: ", 7, 0 );
        tlm_send( buf, strlen(buf), 0 );
# endif
      }
    }

    if (strstr(buf,"OK")) {
# if MDM_DBG
        tlm_send( "\r\n OK", 5, 0 );
# endif
        gotOK = 1;
    }

    if ( failed && failed[0] ) {
      if (strstr(buf, failed )) {
          gotFailed = 1;
# if MDM_DBG
          tlm_send( "\r\nFAILED: ", 10, 0 );
          tlm_send( failed, strlen(failed), 0 );
# endif
      }
    }
  }

  /* flush the IO port buffers */
  mdm_flushRecv();

  int rv = ( !needOK || gotOK ) && ( !needKey || gotKey );

# if MDM_LOG
  if ( 1 ) {
        fHandler_t fh;
        U16 const flag = f_exists( MDM_LOG_FILE ) ? (O_WRONLY|O_APPEND) : (O_WRONLY|O_CREAT);
        if ( FILE_OK == f_open( MDM_LOG_FILE, flag, &fh ) ) {

            char rt[64];
            snprintf ( rt, sizeof(rt)-1, "\r\nRsp=%d gotOK %d needOK %d gotKey %d needKey %d\r\n", rv, gotOK, needOK, gotKey, needKey );
            f_write ( &fh, rt, strlen(rt) );

            f_close( &fh );
        }
  }
# endif

  return rv;
}


/*------------------------------------------------------------------------*/
/* function to initialize modem using AT command string                   */
/*------------------------------------------------------------------------*/
/**
   This function initializes the modem with a specified command string.
   Motorola's ISU AT Command Reference (SSP-ISU-CPSW-USER-005 Version 1.3)
   was used as the guide document.

      \begin{verbatim}
      input:

      output:

         This function returns a positive number if the exchange was
         successful.  Zero is returned if the exchange failed.  A negative
         number is returned if the function parameters were determined to be
         ill-defined.

      \end{verbatim}

   written by Dana Swift, adapted to hypernav by BP
*/
static int modem_initialize()
{
  int const rx_timeout = 5;

  //
  //  restore the modem's factory configuration
  //
  if ( mdm_getResponseString( "AT&F0\r",
                              0, "ERROR",
                              1, rx_timeout,
                              0, 0 ) <= 0 ) {
    return -1;
  }

  /* Specify command string */
  /* Note: Command must not exceed MAXCMDLEN characters  */
  /*
   *  &C1   DCD Option   = 1  ==>  DCD indicates connection status.  (Carrier Detect?)
   *  &D2   DTR Option   = 2  ==>  If DTR transitions from ON to OFF
   *                               during either in-call command mode or in-call data mode,
   *                               the call will drop to on-hook command mode (default).
   *  &K0   Flow Control = 0  ==>  Disable flow control.
   *  &K3   Flow Control = 3  ==>  Enable RTS/CTS flow control.
   * [&Q5]  Sync/Async   = 5  ==>  Asynchronous operation with error correction (factory default?)
   *  &R1   RTS/CTS           --   Not used by modem, compatibility command.
   *  &S1   DSR Override = 1  ==>  Same as 0, DSR always active. ???
   * [&X0]  Select Sync Clock ==>  Not used by modem, compatibility command.
   *
   *   E0   Echo         = 1  ==>  Characters are NOT echoed to the DTE
   *  [L2]  Loudspeaker  = 2  ==>  Compatibility command.
   *  [M1]  Speaker ctrl.     ==>  Compatibility command.
   *   Q0   Quiet Mode   = 0  ==>  ISU responses are sent to the DTE.
   *   S0=0 Auto-Answer  = 0  ==>  Disable auto-answer.
   *   S7   Comm. std.   =45       Compatibility command
   *   S10  Carrier loss time =100 Compatibility command
   *   V1   Verbose Mode = 1  ==>  Textual responses (default).
   *  [W0]  Error Correct  0  ==>  Upon connection, the ISU reports the DTE speed (default).
   *   X4   Extended Codes 4  ==>  OK, CONNECT, RING, NO CARRIER, NO ANSWER, ERROR,
   *                               CONNECT <speed>,
   *                               NO DIALTONE,
   *                               BUSY,
   *                               CARRIER <speed>, PROTOCOL:, COMPRESSION:
   *  [Y0]  Long Space Disconnect  Compatibility command.
   */
  const char *cmd = "AT &C1 &D2 &K3 &R1 &S1 E0 Q0 S0=0 "            "V1 X4" "\r";
  //
  // Navis 1 uses this configuration
  // const char *cmd = "AT &C1 &D2 &K0 &R1 &S1 E1 Q0 S0=0 S7=45 S10=100 V1 X4";
  //
  if ( mdm_getResponseString( cmd,
                              0, "ERROR",
                              1, rx_timeout,
                              0, 0 ) <= 0 ) {
    return -2;
  }

  //
  //  This command selects the Bearer Service Type to be 4800 baud on the
  //  remote computer.  Dana Swift found this command to be necessary in order for
  //  logins to be successful.  Motorola's ISU AT Command Reference
  //  (SSP-ISU-CPSW-USER-005 Version 1.3) was used as the guide document.
  //
  //  The command "AT+CBST?" returns the BSTs supported by the modem.
  //
  if ( mdm_getResponseString( "AT+CBST=6,0,1\r",
                              0, "ERROR",
                              1, rx_timeout,
                              0, 0 ) <= 0 ) {
    return -3;
  }

  if ( mdm_getResponseString( "AT&V\r",
                              0, "OK",
                              1, rx_timeout,
                              0, 0 ) <= 0 ) {
    return -3;
  }

  return 1;
}

/*------------------------------------------------------------------------*/
/* function to confirm modem attention                                        */
/*------------------------------------------------------------------------*/
/*
   On success (the modem responds "OK" to "AT"), this function returns 1.
   On failure (the modem fails to respond to "AT"), it returns zero.
*/
int mdm_getAtOk()
{
  int const rx_timeout = 2;
  return mdm_getResponseString( "AT\r",
                                0, "ERROR",
                                1, rx_timeout,
                                0, 0 );
}

int mdm_isIridium() {

//  Old interface - confirm if working - not in documentation!
//
//if ( mdm_chat("AT I4","IRIDIUM",10,"\r") <= 0 ) {

//mdm_dtr_toggle_off_on();

  if ( mdm_chat("AT+CGMI","IRIDIUM",10,"\r") <= 0 ) {
    return 0;  //  Is not Iridium
  } else {
    return 1;  //  Is Iridium
  }
}

/*------------------------------------------------------------------------*/
/* function to configure the modem                                        */
/*------------------------------------------------------------------------*/
int mdm_configure()
{
  if ( 1 != mdm_getAtOk() ) {
    return 0;
  }

  /* initialize the return value */
  int status=1;

//mdm_dtr_toggle_off_on();

  /* initialize the modem configuration */
  if (modem_initialize()>0) {

    /* set the baud rate to be 19200 (fixed) */
    if (mdm_chat("AT +IPR=6,0","OK",3,"\r")>0) {

      //if (debuglevel>=2) {
      //  LogEntry(FuncName,"Modem configured for fixed (19200) baud rate.\n");
      //}

      /* store the configuration in the modem's NVRAM */
      if (mdm_chat("AT &W0 &Y0","OK",3,"\r")<=0) {
        status=-3;
      }
    } else {
      status=-2;
    }
  } else {
    status=-1;
  }

  return status;
}

/*------------------------------------------------------------------------*/
/* function to query the Modem for its firmware revision                  */
/*------------------------------------------------------------------------*/
/**
   This function queries the modem for its firmware.

      \begin{verbatim}
      input:

         size....The size of the 'FwRev' buffer used to store the firmware
                 revision.  This buffer must be at least 16 bytes long.

      output:

         FeRev...The firmware revision of the modem is stored in this
                 buffer.

         This function returns a positive value on success and a zero on
         failure.  A negative negative value indicates an invalid argument.
     \end{verbatim}
*/
int mdm_getFwRev(char *FwRev, size_t size)
{
  int rx_timeout = 10;
  return mdm_getResponseString( "AT+CGMR\r",
                                "Call Processor Version: ", "ERROR",
                                1, rx_timeout,
                                FwRev, size );
}

/*------------------------------------------------------------------------*/
/* function to register the modem with the iridium system                 */
/*------------------------------------------------------------------------*/
/**
   This function attempts to register the modem with the iridium system.
   On success, this function returns 1.
   If the registration attempt failed, it returns 0.
*/
int mdm_isRegistered()
{
  /* registration status is unknown*/
  int registered=0;

  char values[16];
  int rx_timeout = 60;

  //
  //  The expected command/response exchange is:
  //
  //    [Echo Back]   AT+CREG?\r
  //    [CRLF]        \r\n
  //    [Response]    +CREG:<value>,<value>\r\n
  //    [CRLF]        \r\n
  //    [OK]          OK\r\n
  //

  if ( mdm_getResponseString( "AT+CREG?\r",
                              "+CREG:", "ERROR",
                              1, rx_timeout,
                              values, sizeof(values) ) > 0 ) {

    char* p = strchr( values,',');

    if ( p ) {

      int s = atoi(++p);

      /* There are two values indicating network registration:
       *   1 == home network
       *   5 == roaming
       * Either value is acceptable.
       */
      if ( s==1 || s==5 ) {
        registered=1;
      }
    }
  }

  return registered;
}

/*------------------------------------------------------------------------*/
/* function to query the iridium modem for signal strength                */
/*------------------------------------------------------------------------*/
/**
   This function queries the Iridium modem for signal strength.  A time-out
   feature (2 minutes) is implemented to prevent this function waiting
   indefinitely for the modem to respond.

      \begin{verbatim}
      input:

      output:

         This function returns the average of up to three responses to the
         +CSQ command.  A two second delay follows each response before the
         next +CSQ command is sent to the modem.
         On success, this function returns a non-negative number
         representing the average number of decibars (0-50)
         that would be visible on the LCD screen of an Iridium phone.
         A negative return value indicates an exception was encountered.
      \end{verbatim}
*/
static int mdm_getSingleSignalStrength()
{
  /* initialize the return value */
  int strength = -1;

  char value[8];
  int rx_timeout = 60;

  //
  //  The expected command/response exchange is:
  //
  //    [Echo Back]   AT+CSQ\r
  //    [CRLF]        \r\n
  //    [Response]    +CSQ:<value>\r\n
  //    [CRLF]        \r\n
  //    [OK]          OK\r\n
  //

  if ( mdm_getResponseString( "AT+CSQ\r",
                              "+CSQ:", "ERROR",
                              1, rx_timeout,
                              value, sizeof(value) ) > 0 ) {

    strength = atoi(value);
  }

  return strength;
}

int mdm_getSignalStrength( int measurements, int* min, int* avg, int* max )
{
  if ( min ) *min =  99;
  if ( avg ) *avg =   0;
  if ( max ) *max = -99;

  int count = 0;
  int sum   = 0;

  int i;
  for ( i=0; i<measurements; i++ ) {

    int strength = mdm_getSingleSignalStrength();

    if ( strength >= 0 ) {

      strength *= 10;   //  Rescale for better averaging of small integer values

      sum   += strength;
      count += 1;

      if ( min && *min > strength ) *min = strength;
      if ( max && *max < strength ) *max = strength;
    }
  }

  if ( count ) {
    if ( avg ) *avg = sum/count;
  }

  return count;
}

/*------------------------------------------------------------------------*/
/* function to connect to a host computer                                 */
/*------------------------------------------------------------------------*/
/**
   This function attempts to establish a modem-to-modem connection with the
   remote host computer.  Multiple attempts are made to establish a
   connection up to a maximum number of retries.

      \begin{verbatim}
      input:

         dialstring...The modem command that generates the tones to connect
                      to the remote computer.  Example: ATDT2065436697.

         sec..........The number of seconds allowed to make the connection
                      before this function abandons the connection attempt.

      output:

         This function returns a positive number (equal to the number of
         tries) if the exchange was successful.  A negative number (whose
         absolute value is equal to the number of tries) is returned if a
         connection could not be established.

      \end{verbatim}

*/
int mdm_connect( const char *dialstring, int sec)
{
  /* validate the dialstring */
  if (!dialstring || !dialstring[0]) {
    return 0;
  }

  return mdm_getResponseString( dialstring,
                                "CONNECT", "NO_CARRIER",
                                0, sec,
                                0, 0 );
}

int mdm_login( int try_duration, char* username, char* password ) {

  //
  //  This function is called after mdm_connect() has
  //  successfully (i.e., "CONNECT" string was sent) returned.
  //
  //  The next expected event is for the rudics server to
  //  send its 'banner' message, which ends with "login:"
  //  Thus, this function checks input until it receives that sub-string.
  //
  //  After receiving the "login:" prompt,
  //  this function sends the username string.
  //
  //  In resonse to the username being sent,
  //  the rudics server sends the "Password:" prompt.
  //
  //  This function then sends the password string.
  //
  //  Then, the rudics server should be at the shell prompt.
  //  Normally, the shell prompt is terminated by "> ".
  //
  //  Currently, the function returns 1 of the shell prompt "> " is seen.
  //  Otherwise, it returns 0.
  //
  //  TODO  ==  Recognize a more general prompt???
  //

  char* needed_match = "login:";
  int    count_match = strlen(needed_match);
  int     have_match = 0;

  struct timeval now;
  gettimeofday( &now, (void*)0 );

  struct timeval give_up;
  gettimeofday( &give_up, (void*)0 );
  give_up.tv_sec += try_duration;  //  give up try_duration seconds from now

  do {

    char byte;

    if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {
      mdm_recv ( &byte, 1, MDM_NONBLOCK );

      if ( byte == needed_match[have_match] ) { have_match ++ ; }
      else                                    { have_match = 0; }

    } else {

      vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );
      gettimeofday( &now, (void*)0 );
    }

  } while ( mdm_carrier_detect() && have_match < count_match && now.tv_sec < give_up.tv_sec );

  if ( have_match < count_match ) return -1;

  time_t const  send_timeout = 2;
  int const ulen = strlen(username);

  if ( ulen != mdm_send( username, ulen, 0, send_timeout ) ) {
    return -2;
  }

  give_up.tv_sec += 10;

  needed_match = "Password:";
   count_match = strlen(needed_match);
    have_match = 0;

  gettimeofday( &now, (void*)0 );

  do {

    char byte;

    if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {
      mdm_recv ( &byte, 1, MDM_NONBLOCK );

      if ( byte == needed_match[have_match] ) { have_match ++ ; }
      else                                    { have_match = 0; }

    } else {

      vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );
      gettimeofday( &now, (void*)0 );
    }

  } while ( mdm_carrier_detect() && have_match < count_match && now.tv_sec < give_up.tv_sec );

  if ( have_match < count_match ) return -3;

  int const plen = strlen(password);

  if ( plen != mdm_send( password, plen, 0, send_timeout ) ) {
    return -4;
  }

  give_up.tv_sec += 10;

  needed_match = "> ";
   count_match = strlen(needed_match);

    have_match = 0;

  gettimeofday( &now, (void*)0 );

  do {

    char byte;

    if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {
        mdm_recv ( &byte, 1, MDM_NONBLOCK );

        if ( byte == needed_match[have_match] ) { have_match ++ ; }
        else                                    { have_match = 0; }

    } else {

        vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );
        gettimeofday( &now, (void*)0 );
    }

  } while ( mdm_carrier_detect() && have_match < count_match && now.tv_sec < give_up.tv_sec );

  if ( have_match < count_match ) return -5;

  return 1;
}

int mdm_communicate ( int duration, int numSend, int* bytesSent )
{
  //
  //  TODO: Based on have/have not RTS/CTS handshake,
  //        Rewrite the timing!!!
  //

  *bytesSent = 0;

  char input[64];
  int nInput = 0;

  struct timeval zero;
  gettimeofday (&zero, (void*)0);

  struct timeval now;
  gettimeofday (&now, (void*)0);

  struct timeval give_up;
  gettimeofday (&give_up, (void*)0);
  give_up.tv_sec += duration;  //  give up try_duration seconds from now

  char const* command = "brx\r\r\n";
  int const cmdlen = strlen(command);
  time_t const  send_timeout = 2;

  if  (cmdlen != mdm_send( command, cmdlen, 0, send_timeout ) )
  {
    return -1;
  }

  *bytesSent += cmdlen;

  //  Allow remote program to start
  vTaskDelay ((portTickType)TASK_DELAY_MS (1000));

  char brst[64+1];

  int const hnv = 9001;
  int const prf = 17041;

  int packetNum = 0;

  do
  {
    int burstNum;

    for  ( burstNum = 0; burstNum < 32 && now.tv_sec < give_up.tv_sec;  burstNum++ )
    {
      while  (!mdm_get_cts () && now.tv_sec < give_up.tv_sec )
      {
        int const sinceStart = now.tv_sec - zero.tv_sec;
        snprintf (brst, 64, "[%4d.%2d] Wait for CTS (%d / %d = %d)\r\n", packetNum, burstNum, *bytesSent, sinceStart, (*bytesSent)/sinceStart );
        tlm_send (brst, strlen(brst), 0 );
        vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );
        gettimeofday( &now, (void*)0 );
      }

      if ( mdm_get_cts() )
      {
        snprintf ( brst, 32, "BRST" "%04d" "%05d" "%04d" "%03d" "0032" "  fv", hnv, prf, packetNum, burstNum );

        if ( 32 != mdm_send( brst, 32, 0, send_timeout ) )
        {
          return -1;
        }

        *bytesSent += 32;
      }

      gettimeofday( &now, (void*)0 );
    }

    char byte;

    while ( mdm_carrier_detect() && mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {

      mdm_recv ( &byte, 1, MDM_NONBLOCK );
      input[nInput] = byte;
      nInput++;

      if ( nInput == 64 )
      {
        nInput = 0; //  Wipe input
        char const* const ERR = "Input wiped\r\n";
        tlm_send ( ERR, strlen(ERR), 0 );
      }
      else
      {
        if ( byte == '\n' )
        {
          tlm_send ( input, nInput-1, 0 );
          char* CRLF = "\r\n";
          tlm_send ( CRLF, 2, 0 );
          nInput = 0;
        }
      }
    }

    gettimeofday( &now, (void*)0 );

    packetNum++;
  }
  while ( mdm_carrier_detect() && now.tv_sec < give_up.tv_sec && packetNum<numSend );

  // BRST 0000   21033  0008 004 4096   06E5744D

  char* last = "BRST" "0000" "00000" "0000" "ZZZZZZZ" "FFFFFFFF";

  snprintf ( brst, 32, "BRST" "%04d" "%05d" "ZZZZ" "ZZZ" "ZZZZ" "$$$$$$$$", hnv, prf );

  if ( 32 != mdm_send( last, 32, 0, send_timeout ) ) {
    return -1;
  }

  *bytesSent += 32;

  //  Allow remote program to terminate
  vTaskDelay( (portTickType)TASK_DELAY_MS( 1000 ) );

  give_up.tv_sec = now.tv_sec + 5;

  do
  {
    char byte;
    if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) )
    {
      mdm_recv ( &byte, 1, MDM_NONBLOCK );
      input[nInput] = byte;
      nInput++;

      if ( nInput == 64 )
      {
        nInput = 0; //  Wipe input
        char const* const ERR = "Input wiped\r\n";
        tlm_send ( ERR, strlen(ERR), 0 );
      }
      else
      {
        if ( byte == '\n' )
        {
          tlm_send ( input, nInput-1, 0 );
          char* CRLF = "\r\n";
          tlm_send ( CRLF, 2, 0 );
          nInput = 0;
        }
      }

      give_up.tv_sec = now.tv_sec + 5;
    }

    gettimeofday( &now, (void*)0 );

  } while ( mdm_carrier_detect() && now.tv_sec < give_up.tv_sec );

  if ( packetNum==numSend ) return 1;  //  Success
  if ( now.tv_sec >= give_up.tv_sec ) return 0;
  return -1;
}

int mdm_communicate_0 ( int duration, int dummy ) {

  struct timeval now;
  gettimeofday( &now, (void*)0 );

  struct timeval give_up;
  gettimeofday( &give_up, (void*)0 );
  give_up.tv_sec += duration;  //  give up try_duration seconds from now

  do {

    char const* command = "date | tee -a DATES\r";
    int const cmdlen = strlen(command);
    time_t const  send_timeout = 2;

    if ( cmdlen != mdm_send( command, cmdlen, 0, send_timeout ) ) {
      return 0;
    }

    char const* needed_match = "> ";
    int  const   count_match = strlen(needed_match);
    int           have_match = 0;

    do {

      char byte;

      if ( mdm_recv( &byte, 1, MDM_PEEK | MDM_NONBLOCK ) ) {
        mdm_recv ( &byte, 1, MDM_NONBLOCK );

        if ( byte == needed_match[have_match] ) { have_match ++ ; }
        else                                    { have_match = 0; }

      } else {

        vTaskDelay( (portTickType)TASK_DELAY_MS( 100 ) );
        gettimeofday( &now, (void*)0 );
      }

    } while ( mdm_carrier_detect() && have_match < count_match && now.tv_sec < give_up.tv_sec );

    vTaskDelay( (portTickType)TASK_DELAY_MS( 2500 ) );
    gettimeofday( &now, (void*)0 );

  } while ( mdm_carrier_detect() && now.tv_sec < give_up.tv_sec );

  return now.tv_sec >= give_up.tv_sec;
}

int mdm_communicate_1 ( int duration, int bytes ) {

  struct timeval now;
  gettimeofday( &now, (void*)0 );

  struct timeval give_up;
  gettimeofday( &give_up, (void*)0 );
  give_up.tv_sec += duration;  //  give up try_duration seconds from now

  char* command = "rezive BLKS\r";
  int   cmdlen = strlen(command);
  time_t const  send_timeout = 2;

  if ( cmdlen != mdm_send( command, cmdlen, 0, send_timeout ) ) {
    return 0;
  }

  unsigned char block[33];
  int i;
  for ( i=0; i<26; i++ ) block[i] = 'A'+i;
  for ( i=27; i<31; i++ ) block[i] = '2'+(i-27);
  block[31] = '\n';
  block[32] = 0;

  int const nBlocks = 1 + bytes/33;
  int sent = 0;

  int b;
  for ( b=0; b<nBlocks && mdm_carrier_detect(); b++ ) {
    sent += mdm_send( block, 32, 0, send_timeout );
    vTaskDelay( (portTickType)TASK_DELAY_MS( 50 ) );
  }

  block[0] = 255;
  sent += mdm_send( block, 1, 0, send_timeout );

  return sent;
}

int mdm_logout() {

  int const rx_timeout = 10;
  return mdm_getResponseString( "\r\nexit\r",
                                "logout", "NO_CARRIER",
                                0, rx_timeout,
                                0, 0 );
}

/*------------------------------------------------------------------------*/
/* function to hang-up the modem to break the connection with the host    */
/*------------------------------------------------------------------------*/
/**
   This function attempts to hang-up the modem in order to break the
   connection with the host.  Multiple attempts are made to hang-up the
   modem up to a maximum number of retries.

      \begin{verbatim}
      input:

      output:

         This function returns a positive number if the hang-up was
         confirmed.  Zero is returned if the confirmation failed.

      \end{verbatim}

   written by Dana Swift, adapted to hypernav by BP
*/
int mdm_escape() {

  vTaskDelay( (portTickType)TASK_DELAY_MS( 1100 ) );

  int const rx_timeout = 5;
  if ( mdm_getResponseString( "+++",
                              0, 0,
                              1, rx_timeout,
                              0, 0 ) ) {
    return 1;
  }

  //
  //  If the modem was already in command mode,
  //  there will be no "OK" response to the "+++" string.
  //

  //
  //  Clean up input:
  //
  char const LF = '\r';
  mdm_send ( &LF, 1, 0, 1 );

  //
  //  Confirm that the modem is in command mode.
  //
  return mdm_getAtOk();
}


int mdm_hangup()
{
  int rx_timeout = 10;
  return mdm_getResponseString( "ATH\r",
                                0, 0,
                                1, rx_timeout,
                                0, 0 );
}

int mdm_getLCC()
{
  /* initialize the return value */
  int LCC = -1;

  char value[8];
  int rx_timeout = 60;

  //
  //  The expected command/response exchange is:
  //
  //    [Echo Back]   AT+CLCC\r
  //    [CRLF]        \r\n
  //    [Response]    +CLCC:<value>\r\n
  //    [CRLF]        \r\n
  //    [OK]          OK\r\n
  //

  if ( mdm_getResponseString( "AT+CLCC\r",
                              "+CLCC:", "ERROR",
                              1, rx_timeout,
                              value, sizeof(value) ) > 0 ) {

    LCC = atoi(value);
  }

  return LCC;
}

