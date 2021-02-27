/**
 * \file	main.c
 *
 * \brief Main functions for HyperNAV spectrometer bootloader
 *
 */

 #include <asf.h>
 #include <string.h>
 #include <intc.h>
 #include "SatlanticHardware.h"
 #include "globals.h"
 #include "comms.h" 
 #include "xmodem.h"
 #include "CRC.h"
 #include "boot.h"
 
 
// #include "conf_usb.h"
// #include "sysclk_auto.h"

//#include "power_clocks_lib.h"
//#include "scif_uc3c.h"

#define AST_PSEL_32KHZ_1024HZ		4
#define AST_SHIFT					10		//2^10 = 1024
#define AST_COUNTER_TO_SECONDS(x)	(x>>AST_SHIFT)
//#define AST_SUBSECONDS(x)			(x % (1 << AST_SHIFT))
#define INT_RTC_RES				10
#define RTC_PSEL_32KHZ_1KHZ 	4
#define LINE_TERMINATOR			"\r\n"		

// major.minor.bugfix versioning
#define MAJOR_VERSION 	"R 1"
#define MINOR_VERSION 	"0"
#define BUGFIX_VERSION	"0"
#define HW_VERSION		"HN SPEC"
#define FULL_VERSION 	MAJOR_VERSION "." MINOR_VERSION "." BUGFIX_VERSION "(" HW_VERSION ")"

#define PROMPT					"HNSPECBLDR> "


// these functions are used in boot.S; they are not defined in boot.h because 
// boot.S includes boot.h in order to get various definitions - the assembler doesn't know how to use the C definitions.
// An alternative approach is to move the defines in boot.h to a separate header file.
extern void boot_program(void);
extern void boot_application(long app_start_addr);	

//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
//static Bool initTime(void);
//static void outputErrorCode(char *msg, U16 code);
//static void outputOk(char *msg);
//static U32 readBootKey(void);
//static void flashProgram(void);
//static void menu(void);
//static void banner(void);
//static Bool bootloaderHook();

//*****************************************************************************
// Local Variables
//*****************************************************************************



static wdt_opt_t wdtopt = {
	.dar   = false,								// After a watchdog reset, the WDT will still be enabled.
	.mode  = WDT_BASIC_MODE,						// The WDT is in basic mode, only PSEL time is used.
	.sfv   = false,								// WDT Control Register is not locked.
	.fcd   = false,								// The flash calibration will be redone after a watchdog reset.
	.cssel = WDT_CLOCK_SOURCE_SELECT_RCSYS,       // Select the system RC oscillator (RCSYS) as clock source.
	.us_timeout_period = 3000000					// Timeout Value
};


//! \fn 	CSL_OutputOk(char *msg)
//!	\brief 	Outputs Ok response, with optional message
//!
//!	Example: $Ok 1.0.0
//!
//!	\param	msg 	The message to send
static void outputOk(char *msg)
{
	COMMS_writeLine(LINE_TERMINATOR "$Ok ");		// space intentional - SUNACom wants it

	if (msg != NULL) {	// output message
		COMMS_writeLine(msg);
	}
	COMMS_writeLine(LINE_TERMINATOR );
}


static void outputErrorCode(char *msg, U16 code)
{
	if (msg != NULL) {// output message
		COMMS_writeLine(msg);
	}
	COMMS_writeLine(LINE_TERMINATOR "$Error:");
	COMMS_printULong((unsigned long) code);
	COMMS_writeLine(LINE_TERMINATOR );
}


static U32 readBootKey(void)
{
	U32 key;

	memcpy(&key, (void *)UPV__BOOTMODE_STARTADDR, UPV__BOOTMODE_SIZE);
	return key;
}

static Bool bootloaderHook(void)
{
	int i;
	int c = 0;
	Bool ret = false;


	//Initialize serial interface as RS232 (no fancy interrupt driving)
	COMMS_initUSART();

	// wait up to 3 seconds for a 'b' to access bootloader
	for(i=0; i<30; i++)
	{
		if(USART_SUCCESS == usart_read_char(MNT_USART, &c))
		if(c == 'b')
		{
			ret = true;
			//usart_write_char(MNT_USART, 'b');
			break;
		}
		usart_write_char(MNT_USART, '>');
		delay_ms(100);
	}

	return ret;
}


static void banner(void)
{
	COMMS_writeLine(LINE_TERMINATOR LINE_TERMINATOR	"HyperNAV Serial Bootloader" LINE_TERMINATOR );
	COMMS_writeLine("Version " FULL_VERSION LINE_TERMINATOR LINE_TERMINATOR);
	COMMS_writeLine(PROMPT);

}


static void flashProgram(void)
{
	U16 upload = CEC_UNKNOWN_ERROR;

	upload = XMDM_ReceiveApp();

	switch (upload) {
		case CEC_NOT_APPLICATION_FILE:
		outputErrorCode( LINE_TERMINATOR "Invalid firmware file", CEC_NOT_APPLICATION_FILE);
		break;

		case CEC_RECEIVED_OK:
		outputOk("Upload successful");
		break;

		case CEC_TIMEOUT:
		outputErrorCode("Timeout", CEC_TIMEOUT);
		break;

		case CEC_TOO_MANY_ERRORS:
		outputErrorCode( LINE_TERMINATOR "Too many errors", CEC_TOO_MANY_ERRORS);
		break;

		case CEC_APP_TOO_LARGE:
		outputErrorCode( LINE_TERMINATOR "Application file too large", CEC_APP_TOO_LARGE);
		break;

		case CEC_UPLOAD_CRC_ERROR:
		outputErrorCode( LINE_TERMINATOR "Calculated CRC-32 does not match expected value", CEC_UPLOAD_CRC_ERROR);
		break;

		case CEC_BURN_CRC_ERROR:
		outputErrorCode( LINE_TERMINATOR "Saved application file CRC-32 does not match expected value", CEC_BURN_CRC_ERROR);
		break;

		case CEC_UNKNOWN_ERROR:
		default:
		outputErrorCode( LINE_TERMINATOR "Unknown error", CEC_UNKNOWN_ERROR);
		break;
	}
}


static void menu(void)
{
	int command;
	U32 boot_mode, crc, calc_crc, addr, end_addr, key, testvalue, location;
	static const char HEX_DIGITS[16] = "0123456789ABCDEF";
	char tmp[9];
	int i;
	U8 flash_char, validity;

	// Read command
	command = COMMS_getc();

	// If no command was received exit (don't block)
	if(command != COMMS_BUFFER_EMPTY)
	{
		
		COMMS_putchar(command);		// Display received char

		switch (command) {
			case '?':
			COMMS_writeLine(LINE_TERMINATOR "Available Commands:" LINE_TERMINATOR
			"A - Run Application Program on startup" LINE_TERMINATOR
			"B - Run Bootloader on startup" LINE_TERMINATOR
			"C - Return program CRC" LINE_TERMINATOR
			"K - Return Bootloader key" LINE_TERMINATOR
			"R - Return Bootloader revision" LINE_TERMINATOR
			"S - Start program immediately" LINE_TERMINATOR
			"V - Validate program" LINE_TERMINATOR
			"W - Write Program" LINE_TERMINATOR
			"? - Display help" LINE_TERMINATOR
			);
			outputOk(NULL);
			break;

			case 'r':
			case 'R':
				outputOk(FULL_VERSION);
				break;

			case 'w':
			case 'W':		// write application  memory
				flashProgram();
				break;

			case 'k':
			case 'K':		// Return bootloader key value
				memcpy(&key, (void *)UPV__BOOTMODE_STARTADDR, UPV__BOOTMODE_SIZE);

				// Convert the number to an ASCII hexadecimal representation.
				tmp[8] = '\0';
				for (i = 7; i >= 0; i--) {
					tmp[i] = HEX_DIGITS[key & 0xF];
					key >>= 4;
				}

				outputOk(tmp);
				//COMMS_writeLine("KEY" LINE_TERMINATOR );
				break;

			case 'c':
			case 'C':		// Return CRC of application file in memory
				memcpy(&crc, (void *)UPV__APPCRC32_STARTADDR, UPV__APPCRC32_SIZE);

				// Convert the number to an ASCII hexadecimal representation.
				tmp[8] = '\0';
				for (i = 7; i >= 0; i--) {
					tmp[i] = HEX_DIGITS[crc & 0xF];
					crc >>= 4;
				}

				outputOk(tmp);
				//COMMS_writeLine("CRC" LINE_TERMINATOR );
				break;

			case 'a':
			case 'A':		// accept application file - run app on startup
				memcpy(&validity, (void *)UPV__VALIDAPP_STARTADDR, UPV__VALIDAPP_SIZE);
				if (validity != (U8)APP_VALID) {
					outputErrorCode( LINE_TERMINATOR "Application file flagged as invalid", CEC_INVALID_APP_FLAG);
					break;
				}
				boot_mode = (U32)BOOT_APP;
				flashc_memcpy((void *)UPV__BOOTMODE_STARTADDR, &boot_mode, UPV__BOOTMODE_SIZE, true);
				outputOk("Run Application on startup");
				break;

			case 'b':
			case 'B':		// discard (reject) application file - run bootloader on startup
				boot_mode = (U32)BOOT_LOADER;
				flashc_memcpy((void *)UPV__BOOTMODE_STARTADDR, &boot_mode, UPV__BOOTMODE_SIZE, true);
				outputOk("Run Bootloader on startup");
				break;

			case 'v':
			case 'V':		// verify validity flag and that application matches stored CRC
				memcpy(&validity, (void *)UPV__VALIDAPP_STARTADDR, UPV__VALIDAPP_SIZE);
				if (validity != (U8)APP_VALID) {
					outputErrorCode( LINE_TERMINATOR "Application file flagged as invalid", CEC_INVALID_APP_FLAG);
					break;
				}
				memcpy(&crc, (void *)UPV__APPCRC32_STARTADDR, UPV__APPCRC32_SIZE);
				memcpy(&end_addr, (void *)UPV__APPSPACE_STARTADDR, UPV__APPSPACE_SIZE);

				// read flash memory back and ensure crc agrees  (byte order matters!)
				CRC_reset();
				calc_crc = 0xffffffff;
				for (addr = APP_START_ADDRESS; addr < end_addr; addr++) {
					memcpy(&flash_char, (void *)addr, 1);
					calc_crc = CRC_updateCRC32(calc_crc, flash_char);
				}
				calc_crc ^= 0xffffffff;	// finalize crc with one's complement

				if (crc == calc_crc) {
					outputOk("Application matches stored CRC-32");
				}
				else {
					// set invalid flag
					flashc_memset8((void *)UPV__VALIDAPP_STARTADDR, (U8)APP_INVALID, 1, true);
					outputErrorCode( LINE_TERMINATOR "Stored CRC-32 does not match Application File", CEC_CRC_NOT_MATCHED);
				}
				break;

			case 's':
			case 'S':		// start application
				memcpy(&validity, (void *)UPV__VALIDAPP_STARTADDR, UPV__VALIDAPP_SIZE);
				if (validity != (U8)APP_VALID) {
					outputErrorCode( LINE_TERMINATOR "Application file flagged as invalid", CEC_INVALID_APP_FLAG);
					break;
				}
				boot_mode = (U32)BOOT_APP;
				flashc_memcpy((void *)UPV__BOOTMODE_STARTADDR, &boot_mode, UPV__BOOTMODE_SIZE, true);
				//wdt_enable(1000000);
				wdtopt.us_timeout_period = 1000000;
				wdt_enable(&wdtopt);
				while(1);
			break;
			
			case 't':
				location = APP_START_ADDRESS;
				memcpy(&testvalue, (void *)location, 4);
				
				COMMS_writeLine("\r\nflash size (bytes): "); COMMS_printULong(flashc_get_flash_size());COMMS_writeLine(LINE_TERMINATOR);
				
				for (i=0; i<AVR32_FLASHC_REGIONS; i++) {
					COMMS_writeLine("Region "); COMMS_printULong((U32)i); COMMS_writeLine(flashc_is_region_locked(i)?"locked":"unlocked" LINE_TERMINATOR);
				}				
				
				//flash_api_lock_all_regions(false);		// this is the key to getting it working!
// 				
// 				for (i=0; i<AVR32_FLASHC_REGIONS; i++) {
// 					COMMS_writeLine("Region "); COMMS_printULong((U32)i); COMMS_writeLine(flashc_is_region_locked(i)?"locked":"unlocked" LINE_TERMINATOR);
// 				}
				
				COMMS_writeLine("\r\nCurrent U32 value at "); COMMS_printULong(location); COMMS_writeLine(" is "); COMMS_printULong(testvalue);COMMS_writeLine(LINE_TERMINATOR);
				testvalue = 987654321;	
				flashc_memcpy((void *)location, &testvalue, 4, true);
				//flashc_memset32((void *)location, testvalue, 4, true);
				if (flashc_is_lock_error())
					COMMS_writeLine("Lock error" LINE_TERMINATOR);
				if (flashc_is_programming_error())
					COMMS_writeLine("Prog error" LINE_TERMINATOR);	
					
				
				testvalue = 0;
				memcpy(&testvalue, (void *)location, 4);
				COMMS_writeLine("New U32 value at "); COMMS_printULong(location); COMMS_writeLine(" is "); COMMS_printULong(testvalue);COMMS_writeLine(LINE_TERMINATOR);
				break;

			default:
				COMMS_writeLine(LINE_TERMINATOR );
				break;

		} // end - switch(command)

		// Display prompt
		COMMS_writeLine(LINE_TERMINATOR PROMPT);

	}// end - if(command != COMMS_BUFFER_EMPTY)
}

//*****************************************************************************
// Exported Functions Implementation
//*****************************************************************************

/*! \brief Main function. Execution starts here.
 */
int main(void)
{	
	U32 key;
	
	serialMode_t serialMode = MODE_RS232;
	
	// board init?

	//-- Initialize MCU clock--
	sysclk_init();		// should use OCS0 - see conf_clock.h
	
	//	The firmware uses the watchdog to drop into this bootloader.
	//  Reconfigure with bootloader suitable timeout
	wdt_disable();
	wdt_enable(&wdtopt);
	
	// Initialize delay module with CPU clock freq
	delay_init(FOSC0);
	
	//set the wait states of flash read accesses
//	#warning THIS SEEMS TO BE A PROBLEM - but shouldn't be need as it is done during clock init'
flashc_set_flash_waitstate_and_readmode(FOSC0);


flashc_lock_all_regions(false);		// this is the key to getting it working! ?
	
	// Enable telemetry UART RS-232 interface for communication
	// Set control lines
	// MNT_FORCEOFF_N=low, MAX Powered off
	// MNT_FORCEOFF_N=high,MNT_FORCE_ON=high, normal operation with auto-powerdown disabled
	// MNT_FORCEOFF_N=high,MNT_FORCE_ON=low, normal operation with auto-powerdown enabled
	gpio_clr_gpio_pin(MNT_FORCEON);
	gpio_set_gpio_pin(MNT_FORCEOFF_N);
	gpio_clr_gpio_pin(MNT_RXEN_N);
	delay_ms(5);						// short delay to allow transceiver to stabilize
	
	// Read boot key from user page
	key = readBootKey();
	COMMS_init(serialMode);  COMMS_writeLine("\r\nKey: "); COMMS_printULong(key); COMMS_writeLine("\r\n");
	
	// if boot key is set to run application - skip bootloader
	if(key == BOOT_APP)	boot_application(APP_START_ADDRESS);	// Start application

	// if key is set to run bootloader - then run the bootloader
	if(key == BOOT_LOADER)  goto start_btldr;
	
	// if key not set, or corrupted:
	
	// Corrupted key fallback (see Jira 2010-401-602)
	//-----------------------------------------------
	// The bootloader will try to heal the key by attempting to set it to BOOT_APP and
	// then booting the application. The user can later set the key to BOOT_LOADER and reboot to
	// get the bootloader started by "normal" code flow. In the rare event in which the boot
	// key cannot be healed neither by the bootloader nor by the user by attempting it to set
	// to BOOT_LOADER, the user will have to connect to the device through the serial port and
	// force the start by sending 'b' during the timeout period. If the bootloader does not
	// receive the 'b' on time, it will re-attempt to heal the key and will boot the application.
	if(bootloaderHook())
	{
		key = BOOT_LOADER;	// Heal key
		flashc_memcpy((void *)UPV__BOOTMODE_STARTADDR, &key, UPV__BOOTMODE_SIZE, true);
		goto start_btldr;
	}
	
	else
	{
		key = BOOT_APP;		// Heal key
		flashc_memcpy((void *)UPV__BOOTMODE_STARTADDR, &key, UPV__BOOTMODE_SIZE, true);
		boot_application(APP_START_ADDRESS);	// Start application
	}
	
	// Valid BOOT_LOADER key or bootloader start confirmed by user input -> Start bootloader
	
// START BOOTLOADER =======================================================
start_btldr:
	
	//Initialize serial interface
	COMMS_init(serialMode);

	COMMS_writeLine("\r\nClock: "); COMMS_printULong(sysclk_get_main_hz()); COMMS_writeLine(" Hz" LINE_TERMINATOR);
	
	// Display startup banner right away if in RS-232 mode else wait until connection is made (not implemented yet)
	banner();
	
	while (1) {
		wdt_clear();
		
		// Process menu task
		menu();
	}
}


 
 

