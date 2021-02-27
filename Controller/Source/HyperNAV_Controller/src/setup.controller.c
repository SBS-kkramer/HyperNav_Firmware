# include "setup.controller.h"

# include <compiler.h>
# include <ctype.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>

# include "files.h"

# include "hypernav.sys.controller.h"

# include "onewire.h"
# include "power.h"
# include "systemtime.h"
# include "syslog.h"
# include "sysmon.h"
# include "telemetry.h"
# include "watchdog.h"

# include "errorcodes.h"
# include "extern.controller.h"
# include "config.controller.h"
# include "filesystem.h"
# include "io_funcs.controller.h"
# include "version.controller.h"
# include "wavelength.h"

# include "tasks.controller.h"


static void cfg_assign_onewire_addresses () {

# if 0
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
        vTaskDelay( (portTickType)TASK_DELAY_MS( 750 ) );
		measure_physical( &ph );
        vTaskDelay( (portTickType)TASK_DELAY_MS( 750 ) );
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
        vTaskDelay( (portTickType)TASK_DELAY_MS( 950 ) );
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
# endif

}

# if 0
S16 setup_system() {

	gHNV_SetupCmdCtrlTask_Status = TASK_IN_SETUP;

	bool all_ok = true;
	bool verbose = false;

	//	Access_Mode_t const access_mode = Access_Factory;

	//	During setup, write to TLM and nothing to FILE.
	syslog_setVerbosity ( SYSLOG_DEBUG );
	syslog_enableOut    ( SYSOUT_TLM );
	syslog_disableOut   ( SYSOUT_FILE );
	syslog_out ( SYSLOG_NOTICE, "system_setup", "Entering system setup." );

	char input[64];

	bool factorySetup = false;
	bool initialSetup = false;

	//	Determine if this is a firmware upgrade.
	//	If the upgrade was done over a firmware >= 2.2.10,
	//	that firmware would have set CFG_Firmware_Upgrade_Yes.
	//	If the upgrade was done over a firmware <= 2.2.9,
	//	assume that the factory password had been set,
	//	i.e., is no longer at 0xFF or 0x00 (
	bool this_is_a_firmware_upgrade =
			( CFG_Get_Firmware_Upgrade() == CFG_Firmware_Upgrade_Yes );

	if ( !this_is_a_firmware_upgrade ) {

		if ( CFG_Configuration_Status_Done == CFG_Get_Configuration_Status() ) {
			if ( 0xFF != CFG_Get_Factory_Password()[0] && 0 != CFG_Get_Factory_Password()[0] ) {
				this_is_a_firmware_upgrade = true;
			}
		}
	}

	if ( 0 /* this_is_a_firmware_upgrade */ ) {

		//	Firmware Version & related code is introduced with FW 2.2.10

		U32 previous_firmware_version = CFG_Get_Firmware_Version();

		if ( previous_firmware_version == 0xFFFFFFFF ) previous_firmware_version = 0;

		//	Make sure to do this upgrade handling only once per upgrade
		CFG_Set_Firmware_Upgrade( CFG_Firmware_Upgrade_No );

	} else {

		//	Prevent hung system if this function is entered erroneously.

		U16 wait = ( CFG_Long_Wait_Flag_Wait == CFG_Get_Long_Wait_Flag() )
			 	? 60 : 15;

		io_out_string (	"\r\nSetup." );

		char dummy;
		tlm_flushRecv();
		while ( 0 == tlm_recv ( &dummy, 1, TLM_PEEK | TLM_NONBLOCK ) && wait > 0 ) {
			io_out_string (	"\r\n\tHit key to enter." );
            vTaskDelay( (portTickType)TASK_DELAY_MS( 950 ) );
			wait--;
		}

		io_out_string ( "\r\n" );
		io_out_string ( "\r\n" );

		if ( 0 == wait ) {
			tlm_flushRecv();
			syslog_out ( SYSLOG_NOTICE, "system_setup", "Timeout. Exiting setup." );
			return SETUP_OK;
		} else {
			tlm_recv ( &dummy, 1, TLM_NONBLOCK );
			tlm_flushRecv();
			switch ( dummy ) {
			case 'T':
# ifdef COMPILE_APITEST
				io_out_string ( "Entering API Test\r\n" );
				api_test( true, CFG_Get_Baud_Rate_AsValue());
# endif
			case '$':
				command_controller_loop( PWR_UNKNOWN_WAKEUP );
			case 'S':
			case 's':
				return SETUP_OK;
			case 'v':
			case 'V':
				verbose = true;
			}
		}

		//	Check if eMMC is formatted.
		//	If not, optionally format now.

		U64 avail;
		U64 total;

		bool did_format = false;
		DWORD const plist[] = {100, 0, 0, 0};
		BYTE work[_MAX_SS];
		FATFS Fatfs;

		if ( FILE_OK != f_space ( EMMC_DRIVE_CHAR, &avail, &total ) ) {
			io_out_string ( "eMMC card may be unformatted. Format now? [n]: " );
			io_in_getstring ( input, sizeof(input), 0, 0 );
			if ( ( input[0] == 'Y' ) || ( input[0] == 'y' ) ) {
				BYTE const drive_num = EMMC_DRIVE_CHAR-'0';
				io_out_string ( "Formatting..." );
				FATFs_f_fdisk(drive_num, plist, work);	// Here 'drive_num' is physical drive number
				FATFs_f_mount(drive_num, &Fatfs);		// Here 'drive_num'is logical drive number
				if(FATFs_f_mkfs(drive_num, 0, 0) != FR_OK) {// Here 'drive_num'is logical drive number
					io_out_string( "Error!\r\n" );
				} else {
					did_format = true;
					io_out_string ( "done.\r\n" );
				}
			}
		}

		if ( did_format ) {
			command_controller_reboot();
		}

		//	File system

		if ( FSYS_FAIL == FSYS_Setup() ) {
			syslog_out ( SYSLOG_ERROR, "system_setup", "File system setup failed." );
			all_ok = false;
		}

		//	Set System Clock Time

		time_ext2sys();
		time_t curr_sys_time = time ( (time_t*)0 );
		io_out_string ( "System time: " );
		io_out_string ( str_time_formatted ( curr_sys_time, DATE_TIME_IO_FORMAT ) );
		io_out_string ( "\r\n" );
		bool input_ok = false;
		do {
			io_out_string ( "Enter time to modify: " );
			io_in_getstring ( input, sizeof(input), 0, 0 );

			if ( input[0] ) {
				struct tm tt;
				if ( strptime ( input, DATE_TIME_IO_FORMAT, &tt ) ) {
					struct timeval tv;
					tv.tv_sec = mktime ( &tt );
					tv.tv_usec = 0;
					settimeofday ( &tv, 0 );
					time_sys2ext();
					input_ok = true;
				} else {
					io_out_string ( "Entered time '" );
					io_out_string ( input );
					io_out_string ( "' not accepted.\r\n" );
				}
			} else {
				input_ok = true;
			}
		} while ( !input_ok );

		curr_sys_time = time ( (time_t*)0 );
		io_out_string ( "System time: " );
		io_out_string ( str_time_formatted ( curr_sys_time, DATE_TIME_IO_FORMAT ) );
		io_out_string ( "\r\n" );

		//	The check on the next line depends on the bootloader script.
		//	The bootloader script sets the complete userpage to 0xFF.
		//	If the Factory Password begins with 0xFF, assume that
		//	this is the very first setup.
		//	Then, have operator confirm to wipe RTC
		if ( 0xFF == CFG_Get_Factory_Password()[0] || 0 == CFG_Get_Factory_Password()[0]
		  || CFG_Configuration_Status_Required == CFG_Get_Configuration_Status() ) {

			initialSetup = true;

			io_out_string ( "\r\nEnter 'Y' to init RTC memory: " );
			io_in_getstring ( input, sizeof(input), 0, 0 );

			if ( ( input[0] == 'Y' ) || ( input[0] == 'y' ) ) {

				U8 const FF = 0xFF;
				wipe_RTC( FF );
				factorySetup = true;
			}
		}

		if ( factorySetup ) {

			//  Assign default build and sane values

			CFG_Set_Admin_Password( "HyperNav" );
			CFG_Setup_Factory_Password();

			//	Sensor generics

			CFG_Set_Sensor_Type( CFG_Sensor_Type_HyperNavRadiometer );
			CFG_Set_Sensor_Version( CFG_Sensor_Version_V1 );
			CFG_Setup_Serial_Number();

			//	Optional Features

			CFG_Set_PCB_Supervisor( CFG_PCB_Supervisor_Available );
			CFG_Set_USB_Switch( CFG_USB_Switch_Available );

			//	Force all RTC residing configuration values to their default values
			CFG_OutOfRangeCounter( false, true );
			CFG_Save ( false );

		}

	}

	CFG_Set_Firmware_Version ( 256L*256L*atoi(HNV_FW_VERSION_MAJOR)
		                     +      256L*atoi(HNV_FW_VERSION_MINOR)
		                     +           atoi(HNV_CTRL_FW_VERSION_PATCH) );

	bool need = false;

	if ( initialSetup ) {

		//	1-Wire
		cfg_assign_onewire_addresses();

		//	Spectrometer Serial Numbers and wavelengths coefficients
		//
		CFG_Setup_Spec_Starboard_Serial_Number();
		CFG_Setup_Spec_Port_Serial_Number();

		//	Upload wavelength coefficient files

		S16 side;

		for ( side=0; side<2; side++ ) {

		  U32 ssn = 0;
		  need = false;

		  if ( 0 == side ) {
			ssn = CFG_Get_Spec_Starboard_Serial_Number();
		  } else {
			ssn = CFG_Get_Spec_Port_Serial_Number();
		  }

		  if ( WL_OK == wl_Load( side ) ) {

			need = ( ssn && ssn != wl_GetSN(side) );

			if ( ssn && !need ) {
				io_out_string ( "Spectrometer " );
				io_dump_U32 ( ssn, " wavelength for channels 10 and 2057: " );
				io_out_F64 ( "%.2f ",    wl_wlenOfCell(side,  10) );
				io_out_F64 ( "%.2f\r\n", wl_wlenOfCell(side,2057) );
				io_out_string ( "\r\nEnter 'Y' to re-upload wavelenght coefficient file: " );
				io_in_getstring ( input, sizeof(input), 0, 0 );
				need = ( input[0] == 'Y' ) || ( input[0] == 'y' );
			}
		  } else {
			need = true;
		  }

		  if ( need ) {
			io_out_string ( "\r\nHit return when ready to upload wavelenght coefficient file for " );
			io_dump_U32 ( ssn, " ..." );
			io_in_getstring ( input, sizeof(input), 0, 0 );
			if ( CEC_Ok == FSYS_RcvWlCoefFile(side) ) {
				io_out_string ( "Upload ok.\r\n" );
				io_out_string ( "Spectrometer " );
				io_dump_U32 ( ssn, " wavelength for channels 10 and 2057: " );
				io_out_F64 ( "%.2f ",    wl_wlenOfCell(side,  10) );
				io_out_F64 ( "%.2f\r\n", wl_wlenOfCell(side,2057) );

				if ( 0 == side && CFG_Get_Spec_Starboard_Serial_Number() != wl_GetSN(side) ) {
					io_out_string ( "Mismatching Startboard wavelength coefficient file.\r\n" );
					all_ok = false;
				}
				if ( 1 == side && CFG_Get_Spec_Port_Serial_Number() != wl_GetSN(side) ) {
					io_out_string ( "Mismatching Port wavelength coefficient file.\r\n" );
					all_ok = false;
				}
			} else {
				io_out_string ( "Upload failed.\r\n" );
				all_ok = false;
			}
		  }
		}
	}

	//	Set CFG to sane values
	//	Print All

	S16 nOOR = CFG_OutOfRangeCounter( false, false );

	if ( nOOR > 0 ) {

		if ( this_is_a_firmware_upgrade ) {

			CFG_OutOfRangeCounter( true, false );
			CFG_Save( false );
			nOOR = CFG_OutOfRangeCounter( false, false );

		} else {

			CFG_Print();
			io_out_S32( "CFG has %ld out-of-range values.\r\n", (S32)nOOR );

			tlm_flushRecv();
			io_out_string ( "Enter 'Y' to create default operational configuration: " );
			io_in_getstring ( input, sizeof(input), 0, 0 );

			if ( ( input[0] == 'Y' ) || ( input[0] == 'y' ) ) {

				CFG_OutOfRangeCounter( true, false );
				CFG_Save( false );

				CFG_Print();

				nOOR = CFG_OutOfRangeCounter( false, false );

				io_out_S32( "CFG has %ld out-of-range values.\r\n", (S32)nOOR );
			}

			if ( nOOR > 0 ) {
				all_ok = false;
			}
		}
	}

	if ( initialSetup ) {

		io_out_string ( "\r\nEnter 'Y' to test low power sleep: " );
		io_in_getstring ( input, sizeof(input), 0, 0 );
		if ( ( input[0] == 'Y' ) || ( input[0] == 'y' ) ) {

			S16 i;
			for ( i=0; i<2; i++ ) {

				time_t Now = time ( (time_t*)0 );
				//	Get time_t value for this day at 00:00:00
				struct tm* tmTmp = gmtime ( &Now );

				struct timeval Wakeup;
				Wakeup.tv_sec = mktime ( tmTmp ) + 30;
				Wakeup.tv_usec = 0;

				tmTmp = gmtime ( &Wakeup.tv_sec );

				hnv_sys_ctrl_status_t reinitStatus;
				hnv_sys_ctrl_param_t reinitParameter;
				reinitParameter.tlm_baudrate = TLM_DEFAULT_USART_BAUDRATE;

				strftime(glob_tmp_str, TMP_STR_LEN,
					"Sleep until %Y/%m/%d %H:%M:%S\r\n", tmTmp);
				io_out_string ( glob_tmp_str );

				// go to low power sleep..
				wakeup_t reason;
				if ( PWR_FAIL == pwr_lpSleep(&Wakeup, &reason, &reinitParameter, &reinitStatus) ) {
					//	This happens if nsys_init() inside pwr_lpSleep() fails.
					io_out_string ( "Failed to initialize after sleep." );
				}

				// Awake!
				time_t Woke_Time = time( (time_t*)0 );
				tmTmp = gmtime ( &Woke_Time );
				strftime(glob_tmp_str, TMP_STR_LEN-32, "Awake at %Y/%m/%d %H:%M:%S from ", tmTmp);

				switch(reason)
				{
				case PWR_TELEMETRY_WAKEUP: 	strcat(glob_tmp_str,"Telemetry\r\n"); break;
				case PWR_EXTRTC_WAKEUP:		strcat(glob_tmp_str,"RTC\r\n"); break;
				default: 					strcat(glob_tmp_str,"Unknown\r\n"); break;
				}

				io_out_string ( glob_tmp_str );
			}
		}
	}

# if 0
	CFG_Set_Burnin_Hours( 0 );
# endif

	//	Save config

	if ( all_ok ) {
		CFG_Set_Configuration_Status( CFG_Configuration_Status_Done );
		CFG_Set_Long_Wait_Flag ( CFG_Long_Wait_Flag_Skip );
		if ( CFG_FAIL == CFG_Save( true ) ) {
			io_out_string ( "CFG Backup failed.\r\n" );
			if ( !this_is_a_firmware_upgrade ) {
				all_ok = false;
			}
		}
	}

	if ( all_ok ) {
		io_out_string ( "Setup done.\r\n" );
	} else {
		//	setup must be repeated.
		CFG_Set_Configuration_Status( CFG_Configuration_Status_Required );
		io_out_string ( "Setup failed.\r\n" );
	}

	//	Make sure to not get stuck
	watchdog_enable ( CFG_Get_Watchdog_Duration() );
	if ( false /* CFG_Watchdog_Off == CFG_Get_Watchdog() */ ) {
		watchdog_disable ();
	}

	//	Allow fine tuning of setting TODO
	//  command_controller_loop();

	return all_ok ? SETUP_OK : SETUP_FAILED;
}
# endif
