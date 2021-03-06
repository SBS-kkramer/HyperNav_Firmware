/*! \file telemetry.c *************************************************************
 *
 * \brief Telemetry port driver API
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2010-11-10
 *
 **********************************************************************************/

#include "telemetry.h"

#include <string.h>

#include "compiler.h"
#include "usart.h"
#include "gpio.h"
#include "cycle_counter.h"
#include "pdmabuffer.h"
#include "avr32rerrno.h"

#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#include "semphr.h"
#endif


//*****************************************************************************
// Local types
//*****************************************************************************



//*****************************************************************************
// Local variables
//*****************************************************************************
static pdmaBufferHandler_t tlmBuffer = -1;
static U32 g_fPBA = 0;
static U32 g_baudrate = 0;
static Bool rxEnabled = FALSE;
static Bool txEnabled = FALSE;
static tlm_t telemetryType = USART_TLM;
static Bool usartTlmInitialized = FALSE;
//static Bool usbTlmInitialized = FALSE;

#ifdef FREERTOS_USED
static xSemaphoreHandle tlmSyncMutex = NULL;
#define TLM_SYNCOBJ_CREATE() 	{if(NULL == (tlmSyncMutex = xSemaphoreCreateMutex()))return TLM_FAIL;}
#define TLM_SYNCOBJ_REQUEST()	{xSemaphoreTake(tlmSyncMutex, portMAX_DELAY);}
#define TLM_SYNCOBJ_RELEASE()	{xSemaphoreGive(tlmSyncMutex);}
#else
#define TLM_SYNCOBJ_CREATE()
#define TLM_SYNCOBJ_REQUEST()
#define TLM_SYNCOBJ_RELEASE()
#endif

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************



//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! \brief Initialize telemetry port sub-system
S16 tlm_init(U32 fPBA, U32 baudrate)
{

	if(telemetryType == USART_TLM && !usartTlmInitialized)
	{
		// Save fPBA and baudrate
		g_fPBA = fPBA;
		g_baudrate = baudrate;

		// Initialize USART
		static const gpio_map_t tlmUSARTPinMap =
		{
				{TLM_USART_RX_PIN, TLM_USART_RX_FUNCTION},
				{TLM_USART_TX_PIN, TLM_USART_TX_FUNCTION}
		};

		// Options for USART.
		usart_options_t TLM_USART_OPTIONS =
		{
				.baudrate = baudrate,
				.charlength = 8,
				.paritytype = USART_NO_PARITY,
				.stopbits = USART_1_STOPBIT,
				.channelmode = USART_NORMAL_CHMODE
		};

		// Set up GPIO for TLM_USART, size of the GPIO map is 2 here.
		gpio_enable_module(tlmUSARTPinMap, sizeof(tlmUSARTPinMap) / sizeof(tlmUSARTPinMap[0]));

		// Initialize it in RS232 mode.
		if(usart_init_rs232(TLM_USART, &TLM_USART_OPTIONS, fPBA) != USART_SUCCESS)
		{
			avr32rerrno = EUSART;
			return TLM_FAIL;
		}

		// Initialize buffer
		if((tlmBuffer = pdma_newBuffer(TLM_TX_BUF_LEN, TLM_PDCA_PID_TX, TLM_RX_BUF_LEN, TLM_PDCA_PID_RX)) == PDMA_BUFFER_INVALID)
		{
			avr32rerrno = EBUFF;
			return TLM_FAIL;
		}

		// Initialize access control mutex
		TLM_SYNCOBJ_CREATE();

		if (MAX3221) {
			// Set control lines
			// MNT_FORCEOFF_N=low, MAX Powered off
			// MNT_FORCEOFF_N=high,MNT_FORCE_ON=high, normal operation with auto-powerdown disabled
			// MNT_FORCEOFF_N=high,MNT_FORCE_ON=low, normal operation with auto-powerdown enabled
			gpio_configure_pin(MNT_FORCE_ON,(GPIO_DIR_OUTPUT | GPIO_INIT_LOW | GPIO_PULL_DOWN));
			gpio_enable_gpio_pin(MNT_FORCE_ON);
			gpio_configure_pin(MNT_FORCEOFF_N,(GPIO_DIR_OUTPUT | GPIO_INIT_HIGH | GPIO_PULL_UP));
			gpio_enable_gpio_pin(MNT_FORCEOFF_N);
		}

		// Enable reception and transmission
		tlm_rxen(TRUE);
		tlm_txen(TRUE);

		// Only relevant for APF builds.
		// In APF mode, the firmware till assert the RTS line when ready to handle input.
		tlm_RTS_assert( false );

		// Flag USART telemetry initialized
		usartTlmInitialized = TRUE;
	}

	// All sub-systems initialized
	  return TLM_OK;
}


//! \brief Deinitialize Telemetry Port (deallocate resources)
void tlm_deinit(void)
{
	if(telemetryType == USART_TLM && usartTlmInitialized)
	{
		// Deallocate Peripheral DMA Transfer buffer
		if(tlmBuffer != -1)
		{
			pdma_deleteBuffer(tlmBuffer);
			tlmBuffer = -1;
		}

		// 	Turn off RS-232 transmitter
		// MAX3221 has auto power down
		//gpio_clr_gpio_pin(TLM_DE);

		usartTlmInitialized = FALSE;
	}

}


//! \brief Send data through telemetry port
S16 tlm_send(void const* buffer, U16 size, U16 flags)
{
	U16 written = 0;

	// Sanity check
	if(buffer == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	if(telemetryType == USART_TLM)
	{
		// Check for driver enabled
		if(!txEnabled)
		{
			avr32rerrno = ERRIO;
			return -1;
		}

		// Request access to TX buffer
		TLM_SYNCOBJ_REQUEST();

		// Non-blocking call
		if(flags & TLM_NONBLOCK)
		{
			written = pdma_writeBuffer(tlmBuffer, (U8*)buffer, size);
		}
		else
		// Blocking call
		{
			do
			{
				written += pdma_writeBuffer(tlmBuffer, ((U8*)buffer+written), size-written);
			}while(written < size);
		}

		// Release access to TX buffer. Other threads can now access the write buffers
		TLM_SYNCOBJ_RELEASE();
	}

	return written;
}


//! \brief Send a message through the telemetry
S16 tlm_msg(const char* msg)
{
 if (!msg) return 0;	//	Check since strlen() does not accept 0 pointers.
 return tlm_send((U8*) msg, strlen(msg),0);
}


//! \brief Receive data through telemetry port
S16 tlm_recv(void* buffer, U16 size, U16 flags)
{
	U16 read = 0;

	// Sanity check
	if(buffer == NULL)
	{
		avr32rerrno = EPARAM;
		return TLM_FAIL;
	}

	if(telemetryType == USART_TLM)
	{
		// Check for receiver enabled
		if(!rxEnabled)
		{
			avr32rerrno = ERRIO;
			return TLM_FAIL;
		}

		// Non-blocking call
		if(flags & TLM_NONBLOCK)
		{
			if(flags & TLM_PEEK)
				read = pdma_peekBuffer(tlmBuffer, (U8*)buffer, size);
			else
				read = pdma_readBuffer(tlmBuffer, (U8*)buffer, size);
		}
		else
		// Blocking call
		{
			do
			{
				if(flags & TLM_PEEK)
					read += pdma_peekBuffer(tlmBuffer, ((U8*)buffer+read), size-read);
				else
					read += pdma_readBuffer(tlmBuffer, ((U8*)buffer+read), size-read);

			}while(read < size);
		}
	}

	return read;
}


//! \brief Flush telemetry port reception buffer.
//! This function discards all unread data.
void tlm_flushRecv(void)
{
	pdma_flushReadBuffer(tlmBuffer);
}


//! \brief Assert/Deassert RTS line
//! @param assert	If true RTS is asserted, if false RTS is deasserted.
void tlm_RTS_assert(Bool assert) {
	
	// MAX 3221 does not have RTS line - Ronnie
	/*if ( assert ) {
		gpio_clr_gpio_pin ( TLM_USART_RTS_PIN );
	} else {
		gpio_set_gpio_pin ( TLM_USART_RTS_PIN );
	}*/
}

//! \brief Enable/Disable reception
void tlm_rxen(Bool enable)
{
#if ( (BOARD==USER_BOARD) && ((MCU_BOARD_DN==E980030)||(MCU_BOARD_DN==E980030_DEV_BOARD)||(MCU_BOARD_DN==E980031)) )

	if(enable)
		gpio_clr_gpio_pin(TLM_RXEN_n);
	else
		gpio_set_gpio_pin(TLM_RXEN_n);

//#elif BOARD==EVK1104
#else
	#error "Unhandled hardware variant. Cannot build binary"
#endif

	// Flag state
	rxEnabled = enable;
}


//! \brief Enable/Disable transmission
void tlm_txen(Bool enable)
{
// MAX 3221 has autopower down. Ronnie
/*#if ( (BOARD==USER_BOARD) && ((MCU_BOARD_DN==E980030)||(MCU_BOARD_DN==E980030_DEV_BOARD)||(MCU_BOARD_DN==E980031)) )

	if(enable)
	{
		gpio_set_gpio_pin(TLM_DE);
		// 100uS delay after coming back from shutdown (see Maxim's MAX3222 datasheet)
		cpu_delay_us(100, g_fPBA);
	}
	else
		gpio_clr_gpio_pin(TLM_DE);

#else
	#error "Unhandled hardware variant. Cannot build binary"
#endif
*/
	// Flag state
	txEnabled = enable;
}


//! \brief Dynamically set telemetry port baudrate
S16 tlm_setBaudrate(U32 baudrate)
{
	// Options for USART.
	usart_options_t TLM_USART_OPTIONS =
	{
			.baudrate = baudrate,
			.charlength = 8,
			.paritytype = USART_NO_PARITY,
			.stopbits = USART_1_STOPBIT,
			.channelmode = USART_NORMAL_CHMODE
	};

	// Verify l_fPBA initialized
	if(g_fPBA == 0)
	{
		avr32rerrno = EUSART;
		return TLM_FAIL;
	}

	// Re-initialize usart in RS232 mode with new baudrate
	if(usart_init_rs232(TLM_USART, &TLM_USART_OPTIONS, g_fPBA) != USART_SUCCESS)
	{
		avr32rerrno = EUSART;
		return TLM_FAIL;
	}

	// Update baudrate
	g_baudrate = baudrate;

	return TLM_OK;
}


//! \brief Detect soft break, ignore all other inputs
Bool tlm_softBreak(char softBreakChar)
{
	char c=0;

	if(telemetryType == USART_TLM)
	{
		while(tlm_recv(&c, 1, TLM_NONBLOCK) == 1)
		{
			if(c==softBreakChar)	// If soft break received
			{
				tlm_flushRecv();	// Flush all other data in buffer including repeated soft break chars
				return TRUE;
			}
		}
	}

	return FALSE;				// Soft break not received
}


//! \brief Get telemetry mode
tlm_t tlm_getMode(void)
{
	return telemetryType;
}


#if TLM_USB_COMM == ENABLED

//! \brief Switch to USB telemetry
//! @return TLM_OK: Success, TLM_FAIL: Error
S16 tlm_switchToUSBTLM(void)
{
	if(telemetryType != USB_TLM)
	{
		telemetryType = USB_TLM;

		// Initialize USB telemetry if not initialized before
		return tlm_init(g_fPBA, g_baudrate);
	}
	else
		return TLM_OK;
}


//! \brief Switch to serial telemetry
//! @return TLM_OK: Success, TLM_FAIL: Error
S16 tlm_switchToSerialTLM(void)
{
	if(telemetryType != USART_TLM)
	{
		telemetryType = USART_TLM;

		// Initialize USART telemetry if not initialized before
		return tlm_init(g_fPBA, g_baudrate);
	}
	else
		return TLM_OK;
}


#endif


//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************



