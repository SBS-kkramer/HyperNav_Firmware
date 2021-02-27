/*
 * comms.c
 *
 *  Created on: Dec 7, 2010
 *  Author: scott
 *  Modified on:
 *  2011-10-4, DAS - Added USB functionality
 *	2016-10-17, SF: Porting to HyperNAV spectrometer board.  Removing USB functionality
 */
#include <sys/time.h>
#include <avr32/io.h>
#include "board.h"
#include "gpio.h"
#include "power_clocks_lib.h"
//#include "print_funcs.h"
#include "compiler.h"
#include "delay.h"
#include "intc.h"
#include "usart.h"

#ifdef USBSUPPORT
	#include "uart_usb_lib.h"
	#include "usb_task.h"
#endif	
#include "comms.h"


//*****************************************************************************
// Module configuration
//*****************************************************************************
#define UART_BUF_SIZE	256


//*****************************************************************************
// Local Variables
//*****************************************************************************
volatile int uart_rxd_buf_cnt;
static int *uart_rxd_in_ptr, *uart_rxd_out_ptr;
static int uart_rxd_buffer[UART_BUF_SIZE];
static serialMode_t gIntMode = MODE_RS232;

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
static void usart_int_handler(void);


//*****************************************************************************
// Exported Functions Implementation
//*****************************************************************************

//! \brief Initialize serial interface
void COMMS_init(serialMode_t mode)
{
	gIntMode = mode;

	if(gIntMode == MODE_RS232)
	{
		// Wait for RS-232 IC to stabilize
		delay_ms(300);

		// Initialize on-chip USART
		COMMS_initUSART();

		// Initialize interrupt driven buffer
		Disable_global_interrupt();

		// Initialize pointers
		uart_rxd_in_ptr = uart_rxd_out_ptr = uart_rxd_buffer;
		uart_rxd_buf_cnt = 0;

 		INTC_register_interrupt(&usart_int_handler, MNT_USART_IRQ, AVR32_INTC_INT3);  //AVR32_USART3_IRQ

		// Enable USART Rx interrupt.
		MNT_USART->ier = AVR32_USART_IER_RXRDY_MASK;

		// display address of handler
		//COMMS_printULong((unsigned long)usart_int_handler);

		// Enable all interrupts.
		Enable_global_interrupt();
	}

#ifdef USBSUPPORT
	if(gIntMode == MODE_USB)
	{
		uart_usb_init();
	}
#endif
}


//! \brief Read a single byte from the serial interface if available
int COMMS_getc(void)
{
	int c;

	switch(gIntMode)
	{
		case MODE_RS232:
			if (uart_rxd_buf_cnt > 0)		// if there are bytes available to be read ...
			{
				//COMMS_putchar('|');
				
				Disable_global_interrupt();	// disable interrupts

				uart_rxd_buf_cnt--;			// decrease byte counter
				c = *uart_rxd_out_ptr;		// get character from buffer
				if (++uart_rxd_out_ptr >= uart_rxd_buffer + UART_BUF_SIZE)	// wrap pointer if necessary
					uart_rxd_out_ptr = uart_rxd_buffer;

				Enable_global_interrupt();	// re-enable interrupts
				return c;
			}
			return COMMS_BUFFER_EMPTY;		// buffer is empty

#ifdef USBSUPPORT
		case MODE_USB:
			if (uart_usb_test_hit())          // Something received from the USB ?
				return uart_usb_getchar();
			else
				return COMMS_BUFFER_EMPTY;
#endif

		default: break;
	}

	return COMMS_ERROR;
}


//! \brief Output a single character to the serial interface
int COMMS_putchar(int c)
{
	switch(gIntMode)
	{
	case MODE_RS232:
		if(usart_putchar(MNT_USART, c) != USART_SUCCESS)
			return COMMS_ERROR;
		else
			return COMMS_SUCCESS;
		break;

#ifdef USBSUPPORT
	case MODE_USB:
		if( uart_usb_tx_ready() )
		{
			if(c == '\n') uart_usb_putchar('\r');	// Prepend CR to LF
			uart_usb_putchar(c);
			uart_usb_flush();
			return COMMS_SUCCESS;
		}
		else
			return COMMS_ERROR;
#endif			

	default: break;
	}

	return COMMS_ERROR;
}


//! \brief Output a message through serial interface
void COMMS_writeLine(char* line)
{
	switch(gIntMode)
	{
	case MODE_RS232:
		usart_write_line(MNT_USART, line);
		return;

#ifdef USBSUPPORT
	case MODE_USB:
		  while (*line != '\0')
		    uart_usb_putchar(*line++);
		  uart_usb_flush();
		  return;
#endif

	default: break;
	}
}


//! \brief Print an unsigned long to the serial interface
void COMMS_printULong(unsigned long ul)
{
	char tmp[11];
	int i = sizeof(tmp) - 1;

	// Convert the given number to an ASCII decimal representation.
	tmp[i] = '\0';
	do
	{
		tmp[--i] = '0' + ul % 10;
		ul /= 10;
	} while (ul);

	// Transmit the resulting string
	COMMS_writeLine(tmp + i);
}


//! \brief Print a hex char to the serial interface
void COMMS_printHexChar(char c)
{
	 char tmp[3];
	 int i;
	 static const char HEX_DIGITS[16] = "0123456789ABCDEF";

	 // Convert the given number to an ASCII hexadecimal representation.
	 tmp[2] = '\0';
	 for (i = 1; i >= 0; i--)
	 {
		 tmp[i] = HEX_DIGITS[c & 0xF];
		 c >>= 4;
	 }

	 // Transmit the resulting string
	 COMMS_writeLine(tmp);
}



//! \brief Initialize on-chip USART module
void COMMS_initUSART(void)
{
	static const gpio_map_t USART_GPIO_MAP =
	{
			{MNT_USART_RX_PIN, MNT_USART_RX_FUNCTION},
			{MNT_USART_TX_PIN, MNT_USART_TX_FUNCTION}
	};

	// USART options.
	static const usart_options_t USART_OPTIONS =
	{
			.baudrate     = MNT_USART_BAUDRATE,
			.charlength   = 8,
			.paritytype   = USART_NO_PARITY,
			.stopbits     = USART_1_STOPBIT,
			.channelmode  = USART_NORMAL_CHMODE
	};

	// Assign GPIO to USART.
	gpio_enable_module(USART_GPIO_MAP, sizeof(USART_GPIO_MAP) / sizeof(USART_GPIO_MAP[0]));

	// Initialize USART in RS232 mode.
	usart_init_rs232(MNT_USART, &USART_OPTIONS, FOSC0);

}

//*****************************************************************************
// Local Functions Implementation
//*****************************************************************************



// Note - this function must be referenced in the custom linker script!!!
__attribute__((__interrupt__))
static void usart_int_handler(void)
{
	int c;
	int readRet;
	
	//COMMS_putchar('!');

	// In the code line below, the interrupt priority level does not need to be
	// explicitly masked as it is already because we are within the interrupt
	// handler.
	// The USART Rx interrupt flag is cleared by side effect when reading the
	// received character.
	// Waiting until the interrupt has actually been cleared is here useless as
	// the call to usart_write_char will take enough time for this before the
	// interrupt handler is leaved and the interrupt priority level is unmasked by
	// the CPU.
	readRet = usart_read_char(MNT_USART, &c);

	// Clear usart error
	if(readRet == USART_RX_ERROR)
	{
		// Reset the status bits and return
		//(MNT_USART)->CR.rststa = 1;
		usart_reset_status(MNT_USART);
		return;
	}

	// Print the received character to USART.
	// It is a simple echo, so there will be no translation of '\r' to "\r\n". The
	// connected terminal has to be configured accordingly to send '\n' after
	// '\r'.
	//usart_write_char(BTLDR_USART, c);


	*uart_rxd_in_ptr = c;		// get character
	uart_rxd_buf_cnt++;			// increment character count

	if(++uart_rxd_in_ptr >= uart_rxd_buffer + UART_BUF_SIZE) {	// wrap pointer if necessary
		uart_rxd_in_ptr = uart_rxd_buffer;

	}
	
}




