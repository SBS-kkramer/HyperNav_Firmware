# include "setup.spectrometer.h"

# if 0

# include <compiler.h>
# include <ctype.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>

//# include "files.h"
# include "FreeRTOS.h"
# include "task.h"

# include "hypernav.sys.spectrometer.h"

//# include "onewire.h"
# include "power.spectrometer.h"
//# include "systemtime.h"
//# include "sysmon.h"
//# include "spectrometer.h"
//# include "telemetry.h"
# include "wdt.h"

//# include "errorcodes.h"
# include "extern.spectrometer.h"
# include "config.spectrometer.h"
//# include "filesystem.h"
# include "io_funcs.spectrometer.h"
# include "version.spectrometer.h"

# include "tasks.spectrometer.h"


# if 0
static void cfg_assign_onewire_addresses (void) {

	physical_t ph;
	bool needScan;

	U64 ROM_spec = CFG_Get_T_Spec_One_Wire_Address();
	U64 ROM_hous = CFG_Get_T_Housing_One_Wire_Address();
	U64 ROM_tfrm = CFG_Get_T_Transformer_One_Wire_Address();

	if ( 0 == ROM_spec
	  || 0xFFFFFFFFFFFFFFFFLL == ROM_spec
	  || 0 == ROM_hous
	  || 0xFFFFFFFFFFFFFFFFLL == ROM_hous
	  || 0 == ROM_tfrm
	  || 0xFFFFFFFFFFFFFFFFLL == ROM_tfrm
	  ) {
		needScan = true;
	} else {

		sysmon_setSpectrometerTSensorID ( ROM_spec );
		sysmon_setHousingTSensorID ( ROM_hous );
		sysmon_setTransformerTSensorID ( ROM_tfrm );

		measure_physical( &ph );
                vTaskDelay((portTickType)TASK_DELAY_MS( 750 ));
		measure_physical( &ph );
                vTaskDelay((portTickType)TASK_DELAY_MS( 750 ));
		measure_physical( &ph );

		io_out_string ( "\r\n" );

		io_out_string ( "Intern " );
		io_out_S32 ( "(%08x", (S32)(ROM_hous>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_hous&0xFFFFFFFF));
		if ( ph.tempHousing_valid ) {
			io_out_F32 ( "%.1f C", ph.tempHousing );
		} else {
			io_out_string ( "N/A," );
		}

		io_out_string ( ", spec " );
		io_out_S32 ( "(%08x", (S32)(ROM_spec>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_spec&0xFFFFFFFF));
		if ( ph.tempSpec_valid ) {
			io_out_F32 ( "%.1f C", ph.tempSpec );
		} else {
			io_out_string ( "N/A," );
		}

		io_out_string ( ", transformer " );
		io_out_S32 ( "(%08x", (S32)(ROM_tfrm>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_tfrm&0xFFFFFFFF));
		if ( ph.tempLampOrTfrm_valid ) {
			io_out_F32 ( "%.1f C", ph.tempLampOrTfrm );
		} else {
			io_out_string ( "N/A" );
		}

		char input[4];
		io_out_string ( "\r\nEnter 'Y' to re-scan 1-wire sensors: " );
		io_in_getstring ( input, sizeof(input), 0, 0, true );
		needScan = ( input[0] == 'Y' ) || ( input[0] == 'y' );
	}

	if ( !needScan ) return;

	io_out_string ( "\r\nScan 1-wire addresses: " );

	U64	ROM = 0;

	//	Clear values
	CFG_Set_T_Spec_One_Wire_Address        ( ROM );
	CFG_Set_T_Housing_One_Wire_Address     ( ROM );
	CFG_Set_T_Transformer_One_Wire_Address ( ROM );

	ROM_hous = ROM_spec = ROM_tfrm = ROM;

	char cmd_line[16];

	S32 numDevices = 0;
	bool have_address = ow_first ( &ROM );

	while ( have_address ) {

		if ( 0 == numDevices ) {
			ROM_hous = ROM;
		} else if ( 1 == numDevices ) {
			ROM_spec = ROM;
		} else {
			ROM_tfrm = ROM;
		}

		numDevices++;

		have_address = ow_next ( &ROM );
	}

	io_out_S32( "Found %d devices.\r\n", numDevices );

	do {

		sysmon_setHousingTSensorID      ( ROM_hous );
		sysmon_setSpectrometerTSensorID ( ROM_spec );
		sysmon_setTransformerTSensorID  ( ROM_tfrm );

		if ( ROM_tfrm != CFG_Get_T_Transformer_One_Wire_Address() ) {
			CFG_Set_T_Transformer_One_Wire_Address(ROM_tfrm);
		}

		measure_physical( &ph );
                vTaskDelay((portTickType)TASK_DELAY_MS( 950 ));
		measure_physical( &ph );

		io_out_string ( "Intern " );
		io_out_S32 ( "(%08x", (S32)(ROM_hous>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_hous&0xFFFFFFFF));
		if ( ph.tempHousing_valid ) {
			io_out_F32 ( "%.1f C", ph.tempHousing );
		} else {
			io_out_string ( "N/A," );
		}

		io_out_string ( ", spec " );
		io_out_S32 ( "(%08x", (S32)(ROM_spec>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_spec&0xFFFFFFFF));
		if ( ph.tempSpec_valid ) {
			io_out_F32 ( "%.1f C", ph.tempSpec );
		} else {
			io_out_string ( "N/A," );
		}

		io_out_string ( ", transformer " );
		io_out_S32 ( "(%08x", (S32)(ROM_tfrm>>32));
		io_out_S32 ( " %08x) ", (S32)(ROM_tfrm&0xFFFFFFFF));
		if ( ph.tempLampOrTfrm_valid ) {
			io_out_F32 ( "%.1f C", ph.tempLampOrTfrm );
		} else {
			io_out_string ( "N/A" );
		}

		io_out_string ( "\r\nEnter [q] to quit, two letters to swap> " );
		cmd_line[0] = 0; cmd_line[1] = 0;		// Reset command
		io_in_getstring ( cmd_line, sizeof(cmd_line), 0, 0, true );

		if ( 'q' == cmd_line[0] ) {

			if ( CFG_FAIL == CFG_Set_T_Housing_One_Wire_Address ( ROM_hous ) ) {
				io_out_string ( "Housing ROM NOT saved.\r\n" );
			} else {
				io_out_string ( "Housing ROM saved.\r\n" );
			}

			if ( CFG_FAIL == CFG_Set_T_Spec_One_Wire_Address ( ROM_spec ) ) {
				io_out_string ( "Spectrometer ROM NOT saved.\r\n" );
			} else {
				io_out_string ( "Spectrometer ROM saved.\r\n" );
			}

			if ( CFG_FAIL == CFG_Set_T_Transformer_One_Wire_Address ( ROM_tfrm ) ) {
				io_out_string ( "Transformer ROM NOT saved.\r\n" );
			} else {
				io_out_string ( "Transformer ROM saved.\r\n" );
			}

			return;

		} else {

			if ( 'h' == cmd_line[0] && 's' == cmd_line[1] ) {
				ROM = ROM_spec;
				ROM_spec = ROM_hous;
				ROM_hous = ROM;
			} else if ( 'h' == cmd_line[0] && 't' == cmd_line[1] ) {
				ROM = ROM_tfrm;
				ROM_tfrm = ROM_hous;
				ROM_hous = ROM;
			} else if ( 's' == cmd_line[0] && 't' == cmd_line[1] ) {
				ROM = ROM_tfrm;
				ROM_tfrm = ROM_spec;
				ROM_spec = ROM;
			}

		}

	} while (1);

}
# endif

# if 0
S16 setup_system() {

	gHNV_SetupCmdSpecTask_Status = TASK_IN_SETUP;

	bool all_ok = true;
//	bool verbose = false;

	io_out_string ( "Entering system setup." );

	char input[64];

	bool factorySetup = false;
	bool initialSetup = false;

    {

		//	Prevent hung system if this function is entered erroneously.
# if 0
		U16 wait = 60;

		io_out_string (	"\r\nSetup." );

		char dummy;
		tlm_flushRecv();
		while ( 0 == tlm_recv ( &dummy, 1, O_PEEK | O_NONBLOCK ) && wait > 0 ) {
			io_out_string (	"\r\n\tHit key to enter." );
                	vTaskDelay((portTickType)TASK_DELAY_MS( 975 ));
			wait--;
		}

		io_out_string ( "\r\n" );
		io_out_string ( "\r\n" );

		if ( 0 == wait ) {
			tlm_flushRecv();
			io_out_string ( "Timeout. Exiting setup." );
			return SETUP_OK;
		} else {
			tlm_recv ( &dummy, 1, O_NONBLOCK );
			tlm_flushRecv();
			switch ( dummy ) {
			case 'T':
# ifdef COMPILE_APITEST
				io_out_string ( "Entering API Test\r\n" );
				api_test( true, CFG_Get_Baud_Rate_AsValue());
# endif
			case '$':
				CMD_Shell( 120 );
				CMD_Reboot();
			case 'S':
			case 's':
				return SETUP_OK;
			case 'v':
			case 'V':
				verbose = true;
			}
		}
# endif

		//	The check on the next line depends on the bootloader script.
		//	The bootloader script sets the complete userpage to 0xFF.
		//	If the Admin Password begins with 0xFF, assume that
		//	this is the very first setup.
		//	Then, have operator confirm to wipe RTC
		if ( 0xFF == CFG_Get_Admin_Password()[0] || 0 == CFG_Get_Admin_Password()[0] ) {

			initialSetup = true;
		}

	}

	CFG_Set_Firmware_Version ( 256L*256L*atoi(HNV_SPEC_FW_VERSION_MAJOR)
		                     +      256L*atoi(HNV_SPEC_FW_VERSION_MINOR)
		                     +           atoi(HNV_SPEC_FW_VERSION_PATCH) );

//	bool need = false;

	if ( initialSetup ) {

		//	1-Wire
		cfg_assign_onewire_addresses();

	}

	//	Save config

	if ( all_ok ) {
		if ( CFG_FAIL == CFG_Save() ) {
			io_out_string ( "CFG Backup failed.\r\n" );
			all_ok = false;
		}
	}

	if ( all_ok ) {
		io_out_string ( "Setup done.\r\n" );
	} else {
		//	setup must be repeated.
		io_out_string ( "Setup failed.\r\n" );
	}

# if 0
	//	Allow fine tuning of setting
	if ( !this_is_a_firmware_upgrade ) {
		CMD_Shell( 7*24*3600L );
	} else {
		CMD_Shell( 15L );
	}

	CMD_Reboot();
# endif
	return SETUP_OK;
}

# endif

# endif