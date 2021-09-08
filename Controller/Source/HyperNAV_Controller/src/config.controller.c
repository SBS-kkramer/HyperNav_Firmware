/*
 *	ALERT: config.c is auto-generated from CodeTools files.
 *	Do NOT edit.
 *	Read file makeCode.sh for explanations / instructions.
 *
 *  File: config.c
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

# include "config.controller.h"

# include <compiler.h>
# include <ctype.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <errno.h>


# include "flashc.h"
# include "systemtime.h"
# include "onewire.h"
# include "telemetry.h"
# include "files.h"
# include "power.h"
# include "syslog.h"
# include "sysmon.h"
# include "watchdog.h"
# include "usb_drv.h"
# include "usb_standard_request.h"

# include "errorcodes.h"
# include "bbvariables.h"
# include "io_funcs.controller.h"
# include "version.controller.h"
# include "crc.h"
# include "extern.controller.h"
# include "filesystem.h"

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif

# define CFG_BACK_SIZE 512

static S16 cfg_SaveToBackup( char* mem );
static S16 cfg_ReadFromBackup( char* mem );

/*
 *   Maintain same order as Get...() calls in config.h file
 */

typedef struct {
	U8	version;

	//	Build Parameters

	CFG_Sensor_Type	senstype;
	CFG_Sensor_Version	sensvers;
	U16	serialno;
	U32	frwrvers;
	CFG_PCB_Supervisor	pwrsvisr;
	CFG_USB_Switch	usbswtch;
	U32	spcsbdsn;
	U32	spcprtsn;
	U16	frmsbdsn;
	U16	frmprtsn;
	char	admin_pw[8+1];
	char	fctry_pw[8+1];
	CFG_Long_Wait_Flag	waitflag;

	//	Accelerometer Parameters

	F32	accmntng;
	S16	accvertx;
	S16	accverty;
	S16	accvertz;

	//	Compass Parameters

	S16	mag_minx;
	S16	mag_maxx;
	S16	mag_miny;
	S16	mag_maxy;
	S16	mag_minz;
	S16	mag_maxz;

	//	GPS Parameters

	F64	gps_lati;
	F64	gps_long;
	F64	magdecli;

	//	Pressure Parameters

	U32	digiqzsn;
	F64	digiqzu0;
	F64	digiqzy1;
	F64	digiqzy2;
	F64	digiqzy3;
	F64	digiqzc1;
	F64	digiqzc2;
	F64	digiqzc3;
	F64	digiqzd1;
	F64	digiqzd2;
	F64	digiqzt1;
	F64	digiqzt2;
	F64	digiqzt3;
	F64	digiqzt4;
	F64	digiqzt5;
	S16	dqtempdv;
	S16	dqpresdv;
	S16	simdepth;
	S16	simascnt;

	//	Profile Parameters

	F32	puppival;
	F32	puppstrt;
	F32	pmidival;
	F32	pmidstrt;
	F32	plowival;
	F32	plowstrt;

	//	Setup Parameters

	CFG_Configuration_Status	stupstus;
	CFG_Firmware_Upgrade	fwupgrad;

	//	Input_Output Parameters

	CFG_Message_Level	msglevel;
	U8	msgfsize;
	U8	datfsize;
	CFG_Data_File_Needs_Header	datfhead;
	U32	dflstdoy;
	U8	outfrsub;
	CFG_Log_Frames	logframs;
	U32	acqcount;
	U32	cntcount;

	//	Testing Parameters

	CFG_Data_Mode	datamode;

	//	Operational Parameters

	CFG_Operation_Mode	opermode;
	CFG_Operation_Control	operctrl;
	CFG_Operation_State	operstat;
	U32	sysrsttm;
	U32	sysrstct;
	U32	pwrcycnt;
	U16	countdwn;
	U16	apmcmdto;

	//	Acquisition Parameters

	U16	saturcnt;
	U8	numclear;

} cfg_data_struct;

static cfg_data_struct cfg_data;

S16 CFG_VarSaveToUPG ( char* m, U16 location, U8* src, U8 size ) {
	//	User page has 512 (=0x200) bytes. Restrict write to that region.
	if ( location >= 0x200 || location+size > 0x200 ) return CFG_FAIL;
	flashc_memcpy ( (void*)(AVR32_FLASHC_USER_PAGE_ADDRESS+location), (void*)src, (size_t)size, true );
	return CFG_OK;
}

S16 CFG_VarSaveToRTC ( char* m, U16 location, U8* src, U8 size ) {
	//	RTC has 256 (=0x100) bytes. Restrict write to that region.
	if ( location >= 0x100 || location+size > 0x100 ) return -1;
	return BBV_OK == bbv_write (location, src, size ) ? CFG_OK : CFG_FAIL;
}

static S16 cfg_VarSaveToBackup ( char* m, U16 location, U8* src, U8 size ) {
	//	Backup has 512 (=0x200) bytes. Restrict write to that region.
	if ( location >= 0x200 || location+size > 0x200 ) return -1;
	memcpy ( m+location, src, size );
	return CFG_OK;
}

S16 CFG_Save ( bool useBackup ) {

	S16 rv = CFG_OK;

	U32 mVar;
	U8 enum_asU8;

	S16 (*saveToU)( char*, U16, U8*, U8 );
	S16 (*saveToR)( char*, U16, U8*, U8 );
	char* backupMem = 0;

	if ( useBackup ) {
		if ( 0 == (backupMem = pvPortMalloc( CFG_BACK_SIZE ) ) ) {
			return CFG_FAIL;
		}
		saveToU = cfg_VarSaveToBackup;
		saveToR = cfg_VarSaveToBackup;
	} else {
		saveToU = CFG_VarSaveToUPG;
		saveToR = CFG_VarSaveToRTC;
		
	}


	//	Build Parameters

	mVar = (U32)cfg_data.senstype;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SENSTYPE, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.sensvers;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SENSVERS, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.serialno;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SERIALNO, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_FRWRVERS, (U8*)&(cfg_data.frwrvers), 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.pwrsvisr;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PWRSVISR, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.usbswtch;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_USBSWTCH, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}

	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SPCSBDSN, (U8*)&(cfg_data.spcsbdsn), 4))
	{
		rv = CFG_FAIL;
	}

	if  (CFG_OK != saveToU (backupMem, UPG_LOC_SPCPRTSN, (U8*) & (cfg_data.spcprtsn), 4))
	{
		rv = CFG_FAIL;
	}

	mVar = (U32)cfg_data.frmsbdsn;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_FRMSBDSN, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	
	
	mVar = (U32)cfg_data.frmprtsn;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_FRMPRTSN, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}


	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_ADMIN_PW, (U8*)cfg_data.admin_pw, 8 ) ) {
		rv = CFG_FAIL;
	}
	
	
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_FCTRY_PW, (U8*)cfg_data.fctry_pw, 8 ) ) {
		rv = CFG_FAIL;
	}
	
	
	mVar = (U32)cfg_data.waitflag;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_WAITFLAG, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Accelerometer Parameters

	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_ACCMNTNG, (U8*)&(cfg_data.accmntng), 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.accvertx;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_ACCVERTX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.accverty;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_ACCVERTY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.accvertz;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_ACCVERTZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Compass Parameters

	mVar = (U32)cfg_data.mag_minx;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MINX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.mag_maxx;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MAXX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.mag_miny;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MINY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.mag_maxy;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MAXY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.mag_minz;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MINZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.mag_maxz;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAG_MAXZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}

	//	GPS Parameters

	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_GPS_LATI, (U8*)&(cfg_data.gps_lati), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_GPS_LONG, (U8*)&(cfg_data.gps_long), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_MAGDECLI, (U8*)&(cfg_data.magdecli), 8 ) ) {
		rv = CFG_FAIL;
	}

	//	Pressure Parameters

	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZSN, (U8*)&(cfg_data.digiqzsn), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZU0, (U8*)&(cfg_data.digiqzu0), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZY1, (U8*)&(cfg_data.digiqzy1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZY2, (U8*)&(cfg_data.digiqzy2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZY3, (U8*)&(cfg_data.digiqzy3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZC1, (U8*)&(cfg_data.digiqzc1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZC2, (U8*)&(cfg_data.digiqzc2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZC3, (U8*)&(cfg_data.digiqzc3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZD1, (U8*)&(cfg_data.digiqzd1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZD2, (U8*)&(cfg_data.digiqzd2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZT1, (U8*)&(cfg_data.digiqzt1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZT2, (U8*)&(cfg_data.digiqzt2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZT3, (U8*)&(cfg_data.digiqzt3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZT4, (U8*)&(cfg_data.digiqzt4), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DIGIQZT5, (U8*)&(cfg_data.digiqzt5), 8 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.dqtempdv;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DQTEMPDV, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.dqpresdv;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_DQPRESDV, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.simdepth;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SIMDEPTH, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}
	mVar = (U32)cfg_data.simascnt;
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_SIMASCNT, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Profile Parameters

	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PUPPIVAL, (U8*)&(cfg_data.puppival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PUPPSTRT, (U8*)&(cfg_data.puppstrt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PMIDIVAL, (U8*)&(cfg_data.pmidival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PMIDSTRT, (U8*)&(cfg_data.pmidstrt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PLOWIVAL, (U8*)&(cfg_data.plowival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToU ( backupMem, UPG_LOC_PLOWSTRT, (U8*)&(cfg_data.plowstrt), 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Setup Parameters

	enum_asU8 = cfg_data.stupstus;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_STUPSTUS, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	enum_asU8 = cfg_data.fwupgrad;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_FWUPGRAD, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}

	//	Input_Output Parameters

	enum_asU8 = cfg_data.msglevel;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_MSGLEVEL, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_MSGFSIZE, (U8*)&(cfg_data.msgfsize), 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_DATFSIZE, (U8*)&(cfg_data.datfsize), 1 ) ) {
		rv = CFG_FAIL;
	}
	enum_asU8 = cfg_data.datfhead;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_DATFHEAD, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_DFLSTDOY, (U8*)&(cfg_data.dflstdoy), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_OUTFRSUB, (U8*)&(cfg_data.outfrsub), 1 ) ) {
		rv = CFG_FAIL;
	}
	enum_asU8 = cfg_data.logframs;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_LOGFRAMS, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_ACQCOUNT, (U8*)&(cfg_data.acqcount), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_CNTCOUNT, (U8*)&(cfg_data.cntcount), 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Testing Parameters

	enum_asU8 = cfg_data.datamode;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_DATAMODE, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}

	//	Operational Parameters

	enum_asU8 = cfg_data.opermode;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_OPERMODE, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	enum_asU8 = cfg_data.operctrl;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_OPERCTRL, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	enum_asU8 = cfg_data.operstat;
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_OPERSTAT, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_SYSRSTTM, (U8*)&(cfg_data.sysrsttm), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_SYSRSTCT, (U8*)&(cfg_data.sysrstct), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_PWRCYCNT, (U8*)&(cfg_data.pwrcycnt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_COUNTDWN, (U8*)&(cfg_data.countdwn), 2 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_APMCMDTO, (U8*)&(cfg_data.apmcmdto), 2 ) ) {
		rv = CFG_FAIL;
	}

	//	Acquisition Parameters

	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_SATURCNT, (U8*)&(cfg_data.saturcnt), 2 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != saveToR ( backupMem, BBV_LOC_NUMCLEAR, (U8*)&(cfg_data.numclear), 1 ) ) {
		rv = CFG_FAIL;
	}

	if ( useBackup ) {
		if ( CFG_FAIL == cfg_SaveToBackup( backupMem ) ) {
			rv = CFG_FAIL;
		}
		vPortFree ( backupMem );
	}

	return rv;
}

static S16 cfg_VarGetFrmUPG ( char* m, U16 location, U8* dest, U8 size ) {
	//	User page has 512 (=0x200) bytes. Restrict read to that region.
	if ( location >= 0x200 || location+size > 0x200 ) return -1;
	memcpy ( (void*)dest, (void*)(AVR32_FLASHC_USER_PAGE_ADDRESS+location), (size_t)size );
	return CFG_OK;
}

static S16 cfg_VarGetFrmRTC ( char* m, U16 location, U8* dest, U8 size ) {
	//	RTC has 256 (=0x100) bytes. Restrict read to that region.
	if ( location >= 0x100 || location+size > 0x100 ) return -1;
	return BBV_OK == bbv_read ( location, dest, size ) ? CFG_OK : CFG_FAIL;
}

static S16 cfg_VarGetFrmBackup ( char* m, U16 location, U8* dest, U8 size ) {
	//	Backup has 512 (=0x200) bytes. Restrict read to that region.
	if ( location >= 0x200 || location+size > 0x200 ) return -1;
	memcpy ( dest, m+location, size );
	return CFG_OK;
}

S16 CFG_Retrieve ( bool useBackup ) {

	S16 rv = CFG_OK;

	U32 mVar;
	U8 enum_asU8;

	S16 (*getFrmU)( char*, U16, U8*, U8 );
	S16 (*getFrmR)( char*, U16, U8*, U8 );
	char* backupMem = 0;

	if ( useBackup ) {
		if ( 0 == ( backupMem = pvPortMalloc( CFG_BACK_SIZE ) ) ) {
			return CFG_FAIL;
		}
		if ( CFG_FAIL == cfg_ReadFromBackup( backupMem ) ) {
			vPortFree ( backupMem );
			return CFG_FAIL;
		}
		getFrmU = cfg_VarGetFrmBackup;
		getFrmR = cfg_VarGetFrmBackup;
	} else {
		getFrmU = cfg_VarGetFrmUPG;
		getFrmR = cfg_VarGetFrmRTC;
		
	}


	//	Build Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SENSTYPE, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.senstype = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SENSVERS, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.sensvers = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SERIALNO, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.serialno = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_FRWRVERS, (U8*)&(cfg_data.frwrvers), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PWRSVISR, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.pwrsvisr = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_USBSWTCH, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.usbswtch = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SPCSBDSN, (U8*)&(cfg_data.spcsbdsn), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SPCPRTSN, (U8*)&(cfg_data.spcprtsn), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_FRMSBDSN, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.frmsbdsn = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_FRMPRTSN, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.frmprtsn = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_ADMIN_PW, (U8*)cfg_data.admin_pw, 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_FCTRY_PW, (U8*)cfg_data.fctry_pw, 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_WAITFLAG, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.waitflag = mVar;
	}

	//	Accelerometer Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_ACCMNTNG, (U8*)&(cfg_data.accmntng), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_ACCVERTX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.accvertx = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_ACCVERTY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.accverty = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_ACCVERTZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.accvertz = mVar;
	}

	//	Compass Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MINX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_minx = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MAXX, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_maxx = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MINY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_miny = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MAXY, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_maxy = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MINZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_minz = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAG_MAXZ, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.mag_maxz = mVar;
	}

	//	GPS Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_GPS_LATI, (U8*)&(cfg_data.gps_lati), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_GPS_LONG, (U8*)&(cfg_data.gps_long), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_MAGDECLI, (U8*)&(cfg_data.magdecli), 8 ) ) {
		rv = CFG_FAIL;
	}

	//	Pressure Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZSN, (U8*)&(cfg_data.digiqzsn), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZU0, (U8*)&(cfg_data.digiqzu0), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZY1, (U8*)&(cfg_data.digiqzy1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZY2, (U8*)&(cfg_data.digiqzy2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZY3, (U8*)&(cfg_data.digiqzy3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZC1, (U8*)&(cfg_data.digiqzc1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZC2, (U8*)&(cfg_data.digiqzc2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZC3, (U8*)&(cfg_data.digiqzc3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZD1, (U8*)&(cfg_data.digiqzd1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZD2, (U8*)&(cfg_data.digiqzd2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZT1, (U8*)&(cfg_data.digiqzt1), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZT2, (U8*)&(cfg_data.digiqzt2), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZT3, (U8*)&(cfg_data.digiqzt3), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZT4, (U8*)&(cfg_data.digiqzt4), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DIGIQZT5, (U8*)&(cfg_data.digiqzt5), 8 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DQTEMPDV, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.dqtempdv = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_DQPRESDV, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.dqpresdv = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SIMDEPTH, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.simdepth = mVar;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_SIMASCNT, (U8*)&mVar, 4 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.simascnt = mVar;
	}

	//	Profile Parameters

	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PUPPIVAL, (U8*)&(cfg_data.puppival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PUPPSTRT, (U8*)&(cfg_data.puppstrt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PMIDIVAL, (U8*)&(cfg_data.pmidival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PMIDSTRT, (U8*)&(cfg_data.pmidstrt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PLOWIVAL, (U8*)&(cfg_data.plowival), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmU ( backupMem, UPG_LOC_PLOWSTRT, (U8*)&(cfg_data.plowstrt), 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Setup Parameters

	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_STUPSTUS, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.stupstus = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_FWUPGRAD, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.fwupgrad = enum_asU8;
	}

	//	Input_Output Parameters

	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_MSGLEVEL, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.msglevel = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_MSGFSIZE, (U8*)&(cfg_data.msgfsize), 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_DATFSIZE, (U8*)&(cfg_data.datfsize), 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_DATFHEAD, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.datfhead = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_DFLSTDOY, (U8*)&(cfg_data.dflstdoy), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_OUTFRSUB, (U8*)&(cfg_data.outfrsub), 1 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_LOGFRAMS, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.logframs = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_ACQCOUNT, (U8*)&(cfg_data.acqcount), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_CNTCOUNT, (U8*)&(cfg_data.cntcount), 4 ) ) {
		rv = CFG_FAIL;
	}

	//	Testing Parameters

	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_DATAMODE, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.datamode = enum_asU8;
	}

	//	Operational Parameters

	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_OPERMODE, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.opermode = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_OPERCTRL, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.operctrl = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_OPERSTAT, &enum_asU8, 1 ) ) {
		rv = CFG_FAIL;
	} else {
		cfg_data.operstat = enum_asU8;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_SYSRSTTM, (U8*)&(cfg_data.sysrsttm), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_SYSRSTCT, (U8*)&(cfg_data.sysrstct), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_PWRCYCNT, (U8*)&(cfg_data.pwrcycnt), 4 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_COUNTDWN, (U8*)&(cfg_data.countdwn), 2 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_APMCMDTO, (U8*)&(cfg_data.apmcmdto), 2 ) ) {
		rv = CFG_FAIL;
	}

	//	Acquisition Parameters

	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_SATURCNT, (U8*)&(cfg_data.saturcnt), 2 ) ) {
		rv = CFG_FAIL;
	}
	if ( CFG_OK != getFrmR ( backupMem, BBV_LOC_NUMCLEAR, (U8*)&(cfg_data.numclear), 1 ) ) {
		rv = CFG_FAIL;
	}

	if ( useBackup ) {
		vPortFree ( backupMem );
	}

	return rv;
}

void wipe_RTC( U8 value ) {

	int i;

	for ( i=0; i<256; i++ ) {
		bbv_write( i, &value, 1 );
	}
}

void dump_RTC() {

	U32 rtcVal;
	int i;

	io_out_string("RTC Dump:\r\n");

	for ( i=0; i<256; i+=4 ) {
		bbv_read( i, (U8*)&rtcVal, 4 );
		if ( 0 == i%32 ) io_out_string ( "\r\n" );
		if ( 0 == i%4  ) io_out_string ( " " );
		io_dump_X32 ( rtcVal, 0 );
	}

	io_out_string("\r\n\r\n");
}

void wipe_UPG( U8 value ) {

	char mem[AVR32_FLASHC_USER_PAGE_SIZE];

	int i;

	for ( i=64; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) mem[i] = value;

	flashc_memcpy( (void*)AVR32_FLASHC_USER_PAGE_ADDRESS+64, (void*)mem+64, AVR32_FLASHC_USER_PAGE_SIZE-64, true );
}

void dump_UPG() {

	char mem[AVR32_FLASHC_USER_PAGE_SIZE];

	int i;

	for ( i=0; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) mem[i] = 0;

	memcpy( mem, (void*)AVR32_FLASHC_USER_PAGE_ADDRESS, AVR32_FLASHC_USER_PAGE_SIZE );

	io_out_string("UPG Dump:\r\n");

	for ( i=0; i<AVR32_FLASHC_USER_PAGE_SIZE; i++ ) {
		if ( 0 == i%32 ) io_out_string ( "\r\n" );
		if ( 0 == i%4  ) io_out_string ( " " );
		io_dump_X8 ( mem[i], 0 );
	}

	io_out_string("\r\n\r\n");

}


//	Build Parameters


//
//	Configuration Parameter Sensor_Type
//	
//
CFG_Sensor_Type CFG_Get_Sensor_Type(void) {
	return cfg_data.senstype;
}

S16 CFG_Set_Sensor_Type( CFG_Sensor_Type new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SENSTYPE, (void*)&mVar, 4 ) ) {
		cfg_data.senstype = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Sensor_Type() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nSensor_Type [" );
		io_out_string ( CFG_Get_Sensor_Type_AsString() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_Sensor_Type_AsString() );
	} while ( CFG_FAIL == CFG_Set_Sensor_Type_AsString( input ) );
	io_out_string ( "Sensor_Type " );
	io_out_string ( CFG_Get_Sensor_Type_AsString() );
	io_out_string ( "\r\n" );

}
char* CFG_Get_Sensor_Type_AsString(void) {
	switch ( CFG_Get_Sensor_Type() ) {
	case CFG_Sensor_Type_HyperNavRadiometer: return "HyperNavRadiometer";
	case CFG_Sensor_Type_HyperNavTestHead: return "HyperNavTestHead";
	default: return "N/A";
	}
}

S16 CFG_Set_Sensor_Type_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "HyperNavRadiometer" ) ) return CFG_Set_Sensor_Type ( CFG_Sensor_Type_HyperNavRadiometer );
	else if ( 0 == strcasecmp ( new_value, "HyperNavTestHead" ) ) return CFG_Set_Sensor_Type ( CFG_Sensor_Type_HyperNavTestHead );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Sensor_Version
//	
//
CFG_Sensor_Version CFG_Get_Sensor_Version(void) {
	return cfg_data.sensvers;
}

S16 CFG_Set_Sensor_Version( CFG_Sensor_Version new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SENSVERS, (void*)&mVar, 4 ) ) {
		cfg_data.sensvers = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Sensor_Version() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nSensor_Version [" );
		io_out_string ( CFG_Get_Sensor_Version_AsString() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_Sensor_Version_AsString() );
	} while ( CFG_FAIL == CFG_Set_Sensor_Version_AsString( input ) );
	io_out_string ( "Sensor_Version " );
	io_out_string ( CFG_Get_Sensor_Version_AsString() );
	io_out_string ( "\r\n" );

}
char* CFG_Get_Sensor_Version_AsString(void) {
	switch ( CFG_Get_Sensor_Version() ) {
	case CFG_Sensor_Version_V1: return "V1";
	default: return "N/A";
	}
}

S16 CFG_Set_Sensor_Version_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "V1" ) ) return CFG_Set_Sensor_Version ( CFG_Sensor_Version_V1 );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Serial_Number
//	
//
U16 CFG_Get_Serial_Number(void) {
	return cfg_data.serialno;
}

S16 CFG_Set_Serial_Number( U16 new_value ) {
	if ( SERIALNO_MIN <= new_value && new_value <= SERIALNO_MAX ) {
		U32 mVar = new_value;
		if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SERIALNO, (void*)&mVar, 4 ) ) {
			cfg_data.serialno = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Serial_Number() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nSerial_Number [" );
		io_out_S32 ( "%ld]: ", (S32)CFG_Get_Serial_Number() );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) snprintf ( input, sizeof(input), "%hu", CFG_Get_Serial_Number() );
	} while ( CFG_FAIL == CFG_Set_Serial_Number_AsString( input ) );
	io_out_string ( "Serial_Number " );
	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Serial_Number() );

}
S16 CFG_Set_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_Serial_Number ( (U16)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Firmware_Version
//	Is Patch + 256* Minor + 256*256 * Major
//
U32 CFG_Get_Firmware_Version(void) {
	return cfg_data.frwrvers;
}

S16 CFG_Set_Firmware_Version( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_FRWRVERS, (void*)&new_value, 4 ) ) {
		cfg_data.frwrvers = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter PCB_Supervisor
//	
//
CFG_PCB_Supervisor CFG_Get_PCB_Supervisor(void) {
	return cfg_data.pwrsvisr;
}

S16 CFG_Set_PCB_Supervisor( CFG_PCB_Supervisor new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PWRSVISR, (void*)&mVar, 4 ) ) {
		cfg_data.pwrsvisr = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_PCB_Supervisor() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nPCB_Supervisor [" );
		io_out_string ( CFG_Get_PCB_Supervisor_AsString() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_PCB_Supervisor_AsString() );
	} while ( CFG_FAIL == CFG_Set_PCB_Supervisor_AsString( input ) );
	io_out_string ( "PCB_Supervisor " );
	io_out_string ( CFG_Get_PCB_Supervisor_AsString() );
	io_out_string ( "\r\n" );

}
char* CFG_Get_PCB_Supervisor_AsString(void) {
	switch ( CFG_Get_PCB_Supervisor() ) {
	case CFG_PCB_Supervisor_Available: return "Available";
	case CFG_PCB_Supervisor_Missing: return "Missing";
	default: return "N/A";
	}
}

S16 CFG_Set_PCB_Supervisor_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Available" ) ) return CFG_Set_PCB_Supervisor ( CFG_PCB_Supervisor_Available );
	else if ( 0 == strcasecmp ( new_value, "Missing" ) ) return CFG_Set_PCB_Supervisor ( CFG_PCB_Supervisor_Missing );
	else return CFG_FAIL;
}


//
//	Configuration Parameter USB_Switch
//	
//
CFG_USB_Switch CFG_Get_USB_Switch(void) {
	return cfg_data.usbswtch;
}

S16 CFG_Set_USB_Switch( CFG_USB_Switch new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_USBSWTCH, (void*)&mVar, 4 ) ) {
		cfg_data.usbswtch = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_USB_Switch() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nUSB_Switch [" );
		io_out_string ( CFG_Get_USB_Switch_AsString() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_USB_Switch_AsString() );
	} while ( CFG_FAIL == CFG_Set_USB_Switch_AsString( input ) );
	io_out_string ( "USB_Switch " );
	io_out_string ( CFG_Get_USB_Switch_AsString() );
	io_out_string ( "\r\n" );

}
char* CFG_Get_USB_Switch_AsString(void) {
	switch ( CFG_Get_USB_Switch() ) {
	case CFG_USB_Switch_Available: return "Available";
	case CFG_USB_Switch_Missing: return "Missing";
	default: return "N/A";
	}
}

S16 CFG_Set_USB_Switch_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Available" ) ) return CFG_Set_USB_Switch ( CFG_USB_Switch_Available );
	else if ( 0 == strcasecmp ( new_value, "Missing" ) ) return CFG_Set_USB_Switch ( CFG_USB_Switch_Missing );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Spec_Starboard_Serial_Number
//	
//
U32 CFG_Get_Spec_Starboard_Serial_Number(void) {
	return cfg_data.spcsbdsn;
}

S16 CFG_Set_Spec_Starboard_Serial_Number( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SPCSBDSN, (void*)&new_value, 4 ) ) {
		cfg_data.spcsbdsn = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Spec_Starboard_Serial_Number() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nSpec_Starboard_Serial_Number [" );
		io_out_S32 ( "%ld]: ", (S32)CFG_Get_Spec_Starboard_Serial_Number() );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) snprintf ( input, sizeof(input), "%lu", CFG_Get_Spec_Starboard_Serial_Number() );
	} while ( CFG_FAIL == CFG_Set_Spec_Starboard_Serial_Number_AsString( input ) );
	io_out_string ( "Spec_Starboard_Serial_Number " );
	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Spec_Starboard_Serial_Number() );

}
S16 CFG_Set_Spec_Starboard_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Spec_Starboard_Serial_Number ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Spec_Port_Serial_Number
//	
//
U32 CFG_Get_Spec_Port_Serial_Number(void)
{
	return cfg_data.spcprtsn;
}

S16 CFG_Set_Spec_Port_Serial_Number( U32 new_value )
{
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SPCPRTSN, (void*)&new_value, 4 ) )
	{
		cfg_data.spcprtsn = new_value;
		return CFG_OK;
	}
	else
	{
		return CFG_FAIL;
	}
}

void CFG_Setup_Spec_Port_Serial_Number()
{

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nSpec_Port_Serial_Number [" );
		io_out_S32 ( "%ld]: ", (S32)CFG_Get_Spec_Port_Serial_Number() );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] )
			snprintf ( input, sizeof(input), "%lu", CFG_Get_Spec_Port_Serial_Number() );
	}
	  while ( CFG_FAIL == CFG_Set_Spec_Port_Serial_Number_AsString( input ) );

	io_out_string ( "Spec_Port_Serial_Number " );
	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Spec_Port_Serial_Number() );

}
S16 CFG_Set_Spec_Port_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Spec_Port_Serial_Number ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Frame_Starboard_Serial_Number
//	
//
U16 CFG_Get_Frame_Starboard_Serial_Number(void) {
	return cfg_data.frmsbdsn;
}

S16 CFG_Set_Frame_Starboard_Serial_Number( U16 new_value ) {
	if ( new_value <= FRMSBDSN_MAX ) {
		U32 mVar = new_value;
		if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_FRMSBDSN, (void*)&mVar, 4 ) ) {
			cfg_data.frmsbdsn = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Frame_Starboard_Serial_Number() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nFrame_Starboard_Serial_Number [" );
		io_out_S32 ( "%ld]: ", (S32)CFG_Get_Frame_Starboard_Serial_Number() );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) snprintf ( input, sizeof(input), "%hu", CFG_Get_Frame_Starboard_Serial_Number() );
	} while ( CFG_FAIL == CFG_Set_Frame_Starboard_Serial_Number_AsString( input ) );
	io_out_string ( "Frame_Starboard_Serial_Number " );
	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Frame_Starboard_Serial_Number() );

}
S16 CFG_Set_Frame_Starboard_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_Frame_Starboard_Serial_Number ( (U16)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Frame_Port_Serial_Number
//	
//
U16 CFG_Get_Frame_Port_Serial_Number(void) {
	return cfg_data.frmprtsn;
}

S16 CFG_Set_Frame_Port_Serial_Number( U16 new_value ) {
	if ( new_value <= FRMPRTSN_MAX ) {
		U32 mVar = new_value;
		if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_FRMPRTSN, (void*)&mVar, 4 ) ) {
			cfg_data.frmprtsn = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Frame_Port_Serial_Number() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nFrame_Port_Serial_Number [" );
		io_out_S32 ( "%ld]: ", (S32)CFG_Get_Frame_Port_Serial_Number() );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) snprintf ( input, sizeof(input), "%hu", CFG_Get_Frame_Port_Serial_Number() );
	} while ( CFG_FAIL == CFG_Set_Frame_Port_Serial_Number_AsString( input ) );
	io_out_string ( "Frame_Port_Serial_Number " );
	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Frame_Port_Serial_Number() );

}
S16 CFG_Set_Frame_Port_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_Frame_Port_Serial_Number ( (U16)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Admin_Password
//	
//
char* CFG_Get_Admin_Password(void) {
	return cfg_data.admin_pw;
}

S16 CFG_Set_Admin_Password( char* new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG ( 0, UPG_LOC_ADMIN_PW, (U8*)new_value, 8 ) ) {
		strncpy ( cfg_data.admin_pw, new_value, 8 );
		cfg_data.admin_pw[8] = '\0';
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Admin_Password() {

	char input[64];


	if(!isprint(CFG_Get_Admin_Password()[0]))
		CFG_Set_Admin_Password(".");

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nAdmin_Password [" );
		io_out_string ( CFG_Get_Admin_Password() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_Admin_Password() );
	} while ( CFG_FAIL == CFG_Set_Admin_Password( input ) );
	io_out_string ( "Admin_Password " );
		io_out_string ( CFG_Get_Admin_Password() );
	io_out_string ( "\r\n" );

}

//
//	Configuration Parameter Factory_Password
//	
//
char* CFG_Get_Factory_Password(void) {
	return cfg_data.fctry_pw;
}

S16 CFG_Set_Factory_Password( char* new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG ( 0, UPG_LOC_FCTRY_PW, (U8*)new_value, 8 ) ) {
		strncpy ( cfg_data.fctry_pw, new_value, 8 );
		cfg_data.fctry_pw[8] = '\0';
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Factory_Password() {

	char input[64];


	if(!isprint(CFG_Get_Factory_Password()[0]))
		CFG_Set_Factory_Password(".");

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nFactory_Password [" );
		io_out_string ( CFG_Get_Factory_Password() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_Factory_Password() );
	} while ( CFG_FAIL == CFG_Set_Factory_Password( input ) );
	io_out_string ( "Factory_Password " );
		io_out_string ( CFG_Get_Factory_Password() );
	io_out_string ( "\r\n" );

}

//
//	Configuration Parameter Long_Wait_Flag
//	
//
CFG_Long_Wait_Flag CFG_Get_Long_Wait_Flag(void) {
	return cfg_data.waitflag;
}

S16 CFG_Set_Long_Wait_Flag( CFG_Long_Wait_Flag new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_WAITFLAG, (void*)&mVar, 4 ) ) {
		cfg_data.waitflag = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//	Accelerometer Parameters


//
//	Configuration Parameter Accelerometer_Mounting
//	
//
F32 CFG_Get_Accelerometer_Mounting(void) {
	return cfg_data.accmntng;
}

S16 CFG_Set_Accelerometer_Mounting( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_ACCMNTNG, (void*)&new_value, 4 ) ) {
		cfg_data.accmntng = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Accelerometer_Mounting_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Accelerometer_Mounting ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Accelerometer_Vertical_x
//	
//
S16 CFG_Get_Accelerometer_Vertical_x(void) {
	return cfg_data.accvertx;
}

S16 CFG_Set_Accelerometer_Vertical_x( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_ACCVERTX, (void*)&mVar, 4 ) ) {
		cfg_data.accvertx = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Accelerometer_Vertical_x_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Accelerometer_Vertical_x ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Accelerometer_Vertical_y
//	
//
S16 CFG_Get_Accelerometer_Vertical_y(void) {
	return cfg_data.accverty;
}

S16 CFG_Set_Accelerometer_Vertical_y( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_ACCVERTY, (void*)&mVar, 4 ) ) {
		cfg_data.accverty = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Accelerometer_Vertical_y_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Accelerometer_Vertical_y ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Accelerometer_Vertical_z
//	
//
S16 CFG_Get_Accelerometer_Vertical_z(void) {
	return cfg_data.accvertz;
}

S16 CFG_Set_Accelerometer_Vertical_z( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_ACCVERTZ, (void*)&mVar, 4 ) ) {
		cfg_data.accvertz = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Accelerometer_Vertical_z_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Accelerometer_Vertical_z ( new_num );
	else
		return CFG_FAIL;
}


//	Compass Parameters


//
//	Configuration Parameter Magnetometer_Min_x
//	
//
S16 CFG_Get_Magnetometer_Min_x(void) {
	return cfg_data.mag_minx;
}

S16 CFG_Set_Magnetometer_Min_x( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MINX, (void*)&mVar, 4 ) ) {
		cfg_data.mag_minx = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Min_x_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Min_x ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetometer_Max_x
//	
//
S16 CFG_Get_Magnetometer_Max_x(void) {
	return cfg_data.mag_maxx;
}

S16 CFG_Set_Magnetometer_Max_x( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MAXX, (void*)&mVar, 4 ) ) {
		cfg_data.mag_maxx = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Max_x_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Max_x ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetometer_Min_y
//	
//
S16 CFG_Get_Magnetometer_Min_y(void) {
	return cfg_data.mag_miny;
}

S16 CFG_Set_Magnetometer_Min_y( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MINY, (void*)&mVar, 4 ) ) {
		cfg_data.mag_miny = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Min_y_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Min_y ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetometer_Max_y
//	
//
S16 CFG_Get_Magnetometer_Max_y(void) {
	return cfg_data.mag_maxy;
}

S16 CFG_Set_Magnetometer_Max_y( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MAXY, (void*)&mVar, 4 ) ) {
		cfg_data.mag_maxy = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Max_y_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Max_y ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetometer_Min_z
//	
//
S16 CFG_Get_Magnetometer_Min_z(void) {
	return cfg_data.mag_minz;
}

S16 CFG_Set_Magnetometer_Min_z( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MINZ, (void*)&mVar, 4 ) ) {
		cfg_data.mag_minz = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Min_z_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Min_z ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetometer_Max_z
//	
//
S16 CFG_Get_Magnetometer_Max_z(void) {
	return cfg_data.mag_maxz;
}

S16 CFG_Set_Magnetometer_Max_z( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAG_MAXZ, (void*)&mVar, 4 ) ) {
		cfg_data.mag_maxz = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetometer_Max_z_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Magnetometer_Max_z ( new_num );
	else
		return CFG_FAIL;
}


//	GPS Parameters


//
//	Configuration Parameter Latitude
//	
//
F64 CFG_Get_Latitude(void) {
	return cfg_data.gps_lati;
}

S16 CFG_Set_Latitude( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_GPS_LATI, (void*)&new_value, 8 ) ) {
		cfg_data.gps_lati = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Latitude_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Latitude ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Longitude
//	
//
F64 CFG_Get_Longitude(void) {
	return cfg_data.gps_long;
}

S16 CFG_Set_Longitude( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_GPS_LONG, (void*)&new_value, 8 ) ) {
		cfg_data.gps_long = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Longitude_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Longitude ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Magnetic_Declination
//	
//
F64 CFG_Get_Magnetic_Declination(void) {
	return cfg_data.magdecli;
}

S16 CFG_Set_Magnetic_Declination( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_MAGDECLI, (void*)&new_value, 8 ) ) {
		cfg_data.magdecli = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Magnetic_Declination_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Magnetic_Declination ( new_num );
	else
		return CFG_FAIL;
}


//	Pressure Parameters


//
//	Configuration Parameter Digiquartz_Serial_Number
//	
//
U32 CFG_Get_Digiquartz_Serial_Number(void) {
	return cfg_data.digiqzsn;
}

S16 CFG_Set_Digiquartz_Serial_Number( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZSN, (void*)&new_value, 4 ) ) {
		cfg_data.digiqzsn = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Serial_Number_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Serial_Number ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_U0
//	
//
F64 CFG_Get_Digiquartz_U0(void) {
	return cfg_data.digiqzu0;
}

S16 CFG_Set_Digiquartz_U0( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZU0, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzu0 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_U0_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_U0 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_Y1
//	
//
F64 CFG_Get_Digiquartz_Y1(void) {
	return cfg_data.digiqzy1;
}

S16 CFG_Set_Digiquartz_Y1( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZY1, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzy1 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Y1_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Y1 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_Y2
//	
//
F64 CFG_Get_Digiquartz_Y2(void) {
	return cfg_data.digiqzy2;
}

S16 CFG_Set_Digiquartz_Y2( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZY2, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzy2 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Y2_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Y2 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_Y3
//	
//
F64 CFG_Get_Digiquartz_Y3(void) {
	return cfg_data.digiqzy3;
}

S16 CFG_Set_Digiquartz_Y3( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZY3, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzy3 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Y3_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Y3 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_C1
//	
//
F64 CFG_Get_Digiquartz_C1(void) {
	return cfg_data.digiqzc1;
}

S16 CFG_Set_Digiquartz_C1( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZC1, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzc1 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_C1_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_C1 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_C2
//	
//
F64 CFG_Get_Digiquartz_C2(void) {
	return cfg_data.digiqzc2;
}

S16 CFG_Set_Digiquartz_C2( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZC2, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzc2 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_C2_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_C2 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_C3
//	
//
F64 CFG_Get_Digiquartz_C3(void) {
	return cfg_data.digiqzc3;
}

S16 CFG_Set_Digiquartz_C3( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZC3, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzc3 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_C3_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_C3 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_D1
//	
//
F64 CFG_Get_Digiquartz_D1(void) {
	return cfg_data.digiqzd1;
}

S16 CFG_Set_Digiquartz_D1( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZD1, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzd1 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_D1_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_D1 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_D2
//	
//
F64 CFG_Get_Digiquartz_D2(void) {
	return cfg_data.digiqzd2;
}

S16 CFG_Set_Digiquartz_D2( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZD2, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzd2 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_D2_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_D2 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_T1
//	
//
F64 CFG_Get_Digiquartz_T1(void) {
	return cfg_data.digiqzt1;
}

S16 CFG_Set_Digiquartz_T1( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZT1, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzt1 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_T1_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_T1 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_T2
//	
//
F64 CFG_Get_Digiquartz_T2(void) {
	return cfg_data.digiqzt2;
}

S16 CFG_Set_Digiquartz_T2( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZT2, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzt2 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_T2_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_T2 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_T3
//	
//
F64 CFG_Get_Digiquartz_T3(void) {
	return cfg_data.digiqzt3;
}

S16 CFG_Set_Digiquartz_T3( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZT3, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzt3 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_T3_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_T3 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_T4
//	
//
F64 CFG_Get_Digiquartz_T4(void) {
	return cfg_data.digiqzt4;
}

S16 CFG_Set_Digiquartz_T4( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZT4, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzt4 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_T4_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_T4 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_T5
//	
//
F64 CFG_Get_Digiquartz_T5(void) {
	return cfg_data.digiqzt5;
}

S16 CFG_Set_Digiquartz_T5( F64 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DIGIQZT5, (void*)&new_value, 8 ) ) {
		cfg_data.digiqzt5 = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_T5_AsString( char* new_value ) {
	errno = 0;
	F64 new_num = strtod(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_T5 ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_Temperature_Divisor
//	
//
S16 CFG_Get_Digiquartz_Temperature_Divisor(void) {
	return cfg_data.dqtempdv;
}

S16 CFG_Set_Digiquartz_Temperature_Divisor( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DQTEMPDV, (void*)&mVar, 4 ) ) {
		cfg_data.dqtempdv = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Temperature_Divisor_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Temperature_Divisor ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Digiquartz_Pressure_Divisor
//	
//
S16 CFG_Get_Digiquartz_Pressure_Divisor(void) {
	return cfg_data.dqpresdv;
}

S16 CFG_Set_Digiquartz_Pressure_Divisor( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_DQPRESDV, (void*)&mVar, 4 ) ) {
		cfg_data.dqpresdv = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Digiquartz_Pressure_Divisor_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Digiquartz_Pressure_Divisor ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Simulated_Start_Depth
//	
//
S16 CFG_Get_Simulated_Start_Depth(void) {
	return cfg_data.simdepth;
}

S16 CFG_Set_Simulated_Start_Depth( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SIMDEPTH, (void*)&mVar, 4 ) ) {
		cfg_data.simdepth = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Simulated_Start_Depth_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Simulated_Start_Depth ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Sumulated_Ascent_Rate
//	
//
S16 CFG_Get_Sumulated_Ascent_Rate(void) {
	return cfg_data.simascnt;
}

S16 CFG_Set_Sumulated_Ascent_Rate( S16 new_value ) {
	U32 mVar = new_value;
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_SIMASCNT, (void*)&mVar, 4 ) ) {
		cfg_data.simascnt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Sumulated_Ascent_Rate_AsString( char* new_value ) {
	errno = 0;
	S16 new_num = strtol(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Sumulated_Ascent_Rate ( new_num );
	else
		return CFG_FAIL;
}


//	Profile Parameters


//
//	Configuration Parameter Profile_Upper_Interval
//	
//
F32 CFG_Get_Profile_Upper_Interval(void) {
	return cfg_data.puppival;
}

S16 CFG_Set_Profile_Upper_Interval( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PUPPIVAL, (void*)&new_value, 4 ) ) {
		cfg_data.puppival = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Upper_Interval_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Upper_Interval ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Profile_Upper_Start
//	
//
F32 CFG_Get_Profile_Upper_Start(void) {
	return cfg_data.puppstrt;
}

S16 CFG_Set_Profile_Upper_Start( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PUPPSTRT, (void*)&new_value, 4 ) ) {
		cfg_data.puppstrt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Upper_Start_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Upper_Start ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Profile_Middle_Interval
//	
//
F32 CFG_Get_Profile_Middle_Interval(void) {
	return cfg_data.pmidival;
}

S16 CFG_Set_Profile_Middle_Interval( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PMIDIVAL, (void*)&new_value, 4 ) ) {
		cfg_data.pmidival = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Middle_Interval_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Middle_Interval ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Profile_Middle_Start
//	
//
F32 CFG_Get_Profile_Middle_Start(void) {
	return cfg_data.pmidstrt;
}

S16 CFG_Set_Profile_Middle_Start( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PMIDSTRT, (void*)&new_value, 4 ) ) {
		cfg_data.pmidstrt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Middle_Start_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Middle_Start ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Profile_Lower_Interval
//	
//
F32 CFG_Get_Profile_Lower_Interval(void) {
	return cfg_data.plowival;
}

S16 CFG_Set_Profile_Lower_Interval( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PLOWIVAL, (void*)&new_value, 4 ) ) {
		cfg_data.plowival = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Lower_Interval_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Lower_Interval ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Profile_Lower_Start
//	
//
F32 CFG_Get_Profile_Lower_Start(void) {
	return cfg_data.plowstrt;
}

S16 CFG_Set_Profile_Lower_Start( F32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToUPG( 0, UPG_LOC_PLOWSTRT, (void*)&new_value, 4 ) ) {
		cfg_data.plowstrt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Profile_Lower_Start_AsString( char* new_value ) {
	errno = 0;
	F32 new_num = strtof(new_value, (char**)0 );
	if ( 0 == errno )
		return CFG_Set_Profile_Lower_Start ( new_num );
	else
		return CFG_FAIL;
}


//	Setup Parameters


//
//	Configuration Parameter Configuration_Status
//	
//
CFG_Configuration_Status CFG_Get_Configuration_Status(void) {
	return cfg_data.stupstus;
}

S16 CFG_Set_Configuration_Status( CFG_Configuration_Status new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_STUPSTUS, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.stupstus = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

char* CFG_Get_Configuration_Status_AsString(void) {
	switch ( CFG_Get_Configuration_Status() ) {
	case CFG_Configuration_Status_Required: return "Required";
	case CFG_Configuration_Status_Done: return "Done";
	default: return "N/A";
	}
}

S16 CFG_Set_Configuration_Status_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Required" ) ) return CFG_Set_Configuration_Status ( CFG_Configuration_Status_Required );
	else if ( 0 == strcasecmp ( new_value, "Done" ) ) return CFG_Set_Configuration_Status ( CFG_Configuration_Status_Done );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Firmware_Upgrade
//	
//
CFG_Firmware_Upgrade CFG_Get_Firmware_Upgrade(void) {
	return cfg_data.fwupgrad;
}

S16 CFG_Set_Firmware_Upgrade( CFG_Firmware_Upgrade new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_FWUPGRAD, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.fwupgrad = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//	Input_Output Parameters


//
//	Configuration Parameter Message_Level
//	
//
CFG_Message_Level CFG_Get_Message_Level(void) {
	return cfg_data.msglevel;
}

S16 CFG_Set_Message_Level( CFG_Message_Level new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_MSGLEVEL, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.msglevel = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

char* CFG_Get_Message_Level_AsString(void) {
	switch ( CFG_Get_Message_Level() ) {
	case CFG_Message_Level_Error: return "Error";
	case CFG_Message_Level_Warn: return "Warn";
	case CFG_Message_Level_Info: return "Info";
	case CFG_Message_Level_Debug: return "Debug";
	case CFG_Message_Level_Trace: return "Trace";
	default: return "N/A";
	}
}

S16 CFG_Set_Message_Level_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Error" ) ) return CFG_Set_Message_Level ( CFG_Message_Level_Error );
	else if ( 0 == strcasecmp ( new_value, "Warn" ) ) return CFG_Set_Message_Level ( CFG_Message_Level_Warn );
	else if ( 0 == strcasecmp ( new_value, "Info" ) ) return CFG_Set_Message_Level ( CFG_Message_Level_Info );
	else if ( 0 == strcasecmp ( new_value, "Debug" ) ) return CFG_Set_Message_Level ( CFG_Message_Level_Debug );
	else if ( 0 == strcasecmp ( new_value, "Trace" ) ) return CFG_Set_Message_Level ( CFG_Message_Level_Trace );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Message_File_Size
//	
//
U8 CFG_Get_Message_File_Size(void) {
	return cfg_data.msgfsize;
}

S16 CFG_Set_Message_File_Size( U8 new_value ) {
	if ( new_value <= MSGFSIZE_MAX ) {
		if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_MSGFSIZE, (U8*)&new_value, 1 ) ) {
			cfg_data.msgfsize = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Message_File_Size_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x100 )
		return CFG_Set_Message_File_Size ( (U8)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Data_File_Size
//	
//
U8 CFG_Get_Data_File_Size(void) {
	return cfg_data.datfsize;
}

S16 CFG_Set_Data_File_Size( U8 new_value ) {
	if ( DATFSIZE_MIN <= new_value && new_value <= DATFSIZE_MAX ) {
		if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_DATFSIZE, (U8*)&new_value, 1 ) ) {
			cfg_data.datfsize = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Data_File_Size_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x100 )
		return CFG_Set_Data_File_Size ( (U8)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Data_File_Needs_Header
//	
//
CFG_Data_File_Needs_Header CFG_Get_Data_File_Needs_Header(void) {
	return cfg_data.datfhead;
}

S16 CFG_Set_Data_File_Needs_Header( CFG_Data_File_Needs_Header new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_DATFHEAD, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.datfhead = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter Data_File_LastDayOfYear
//	
//
U32 CFG_Get_Data_File_LastDayOfYear(void) {
	return cfg_data.dflstdoy;
}

S16 CFG_Set_Data_File_LastDayOfYear( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_DFLSTDOY, (U8*)&new_value, 4 ) ) {
		cfg_data.dflstdoy = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter Output_Frame_Subsampling
//	
//
U8 CFG_Get_Output_Frame_Subsampling(void) {
	return cfg_data.outfrsub;
}

S16 CFG_Set_Output_Frame_Subsampling( U8 new_value ) {
	if ( new_value <= OUTFRSUB_MAX ) {
		if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_OUTFRSUB, (U8*)&new_value, 1 ) ) {
			cfg_data.outfrsub = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Output_Frame_Subsampling_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x100 )
		return CFG_Set_Output_Frame_Subsampling ( (U8)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Log_Frames
//	
//
CFG_Log_Frames CFG_Get_Log_Frames(void) {
	return cfg_data.logframs;
}

S16 CFG_Set_Log_Frames( CFG_Log_Frames new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_LOGFRAMS, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.logframs = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

char* CFG_Get_Log_Frames_AsString(void) {
	switch ( CFG_Get_Log_Frames() ) {
	case CFG_Log_Frames_Yes: return "Yes";
	case CFG_Log_Frames_No: return "No";
	default: return "N/A";
	}
}

S16 CFG_Set_Log_Frames_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Yes" ) ) return CFG_Set_Log_Frames ( CFG_Log_Frames_Yes );
	else if ( 0 == strcasecmp ( new_value, "No" ) ) return CFG_Set_Log_Frames ( CFG_Log_Frames_No );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Acquisition_Counter
//	
//
U32 CFG_Get_Acquisition_Counter(void) {
	return cfg_data.acqcount;
}

S16 CFG_Set_Acquisition_Counter( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_ACQCOUNT, (U8*)&new_value, 4 ) ) {
		cfg_data.acqcount = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Acquisition_Counter_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Acquisition_Counter ( new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Continuous_Counter
//	
//
U32 CFG_Get_Continuous_Counter(void) {
	return cfg_data.cntcount;
}

S16 CFG_Set_Continuous_Counter( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_CNTCOUNT, (U8*)&new_value, 4 ) ) {
		cfg_data.cntcount = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Continuous_Counter_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno )
		return CFG_Set_Continuous_Counter ( new_num );
	else
		return CFG_FAIL;
}


//	Testing Parameters


//
//	Configuration Parameter Data_Mode
//	
//
CFG_Data_Mode CFG_Get_Data_Mode(void) {
	return cfg_data.datamode;
}

S16 CFG_Set_Data_Mode( CFG_Data_Mode new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_DATAMODE, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.datamode = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

void CFG_Setup_Data_Mode() {

	char input[64];

	do {
		tlm_flushRecv();
		io_out_string ( "\r\nData_Mode [" );
		io_out_string ( CFG_Get_Data_Mode_AsString() );
		io_out_string( "]: " );
		io_in_getstring( input, sizeof(input), 0, 0 );
		if ( !input[0] ) strcpy ( input, CFG_Get_Data_Mode_AsString() );
	} while ( CFG_FAIL == CFG_Set_Data_Mode_AsString( input ) );
	io_out_string ( "Data_Mode " );
	io_out_string ( CFG_Get_Data_Mode_AsString() );
	io_out_string ( "\r\n" );

}
char* CFG_Get_Data_Mode_AsString(void) {
	switch ( CFG_Get_Data_Mode() ) {
	case CFG_Data_Mode_Real: return "Real";
	case CFG_Data_Mode_Fake: return "Fake";
	default: return "N/A";
	}
}

S16 CFG_Set_Data_Mode_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Real" ) ) return CFG_Set_Data_Mode ( CFG_Data_Mode_Real );
	else if ( 0 == strcasecmp ( new_value, "Fake" ) ) return CFG_Set_Data_Mode ( CFG_Data_Mode_Fake );
	else return CFG_FAIL;
}


//	Operational Parameters

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
//
//	Configuration Parameter Operation_Mode
//	Startup; replace FixedTime by Burnin command?
//
CFG_Operation_Mode CFG_Get_Operation_Mode(void) {
	return cfg_data.opermode;
}

S16 CFG_Set_Operation_Mode( CFG_Operation_Mode new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_OPERMODE, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.opermode = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

char* CFG_Get_Operation_Mode_AsString(void) {
	switch ( CFG_Get_Operation_Mode() ) {
	case CFG_Operation_Mode_Continuous: return "Continuous";
	case CFG_Operation_Mode_APM: return "APM";
	default: return "N/A";
	}
}

S16 CFG_Set_Operation_Mode_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Continuous" ) ) return CFG_Set_Operation_Mode ( CFG_Operation_Mode_Continuous );
	else if ( 0 == strcasecmp ( new_value, "APM" ) ) return CFG_Set_Operation_Mode ( CFG_Operation_Mode_APM );
	else return CFG_FAIL;
}
# endif

//
//	Configuration Parameter Operation_Control
//	
//
CFG_Operation_Control CFG_Get_Operation_Control(void) {
	return cfg_data.operctrl;
}

S16 CFG_Set_Operation_Control( CFG_Operation_Control new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_OPERCTRL, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.operctrl = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

char* CFG_Get_Operation_Control_AsString(void) {
	switch ( CFG_Get_Operation_Control() ) {
	case CFG_Operation_Control_Duration: return "Duration";
	case CFG_Operation_Control_Samples: return "Samples";
	default: return "N/A";
	}
}

S16 CFG_Set_Operation_Control_AsString( char* new_value ) {
	if ( 0 == strcasecmp ( new_value, "Duration" ) ) return CFG_Set_Operation_Control ( CFG_Operation_Control_Duration );
	else if ( 0 == strcasecmp ( new_value, "Samples" ) ) return CFG_Set_Operation_Control ( CFG_Operation_Control_Samples );
	else return CFG_FAIL;
}


//
//	Configuration Parameter Operation_State
//	
//
CFG_Operation_State CFG_Get_Operation_State(void) {
	return cfg_data.operstat;
}

S16 CFG_Set_Operation_State( CFG_Operation_State new_value ) {
	U8 new_value_asU8 = new_value;
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_OPERSTAT, (U8*)&new_value_asU8, 1 ) ) {
		cfg_data.operstat = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter System_Reset_Time
//	Time when current execution started
//
U32 CFG_Get_System_Reset_Time(void) {
	return cfg_data.sysrsttm;
}

S16 CFG_Set_System_Reset_Time( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_SYSRSTTM, (U8*)&new_value, 4 ) ) {
		cfg_data.sysrsttm = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter System_Reset_Counter
//	
//
U32 CFG_Get_System_Reset_Counter(void) {
	return cfg_data.sysrstct;
}

S16 CFG_Set_System_Reset_Counter( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_SYSRSTCT, (U8*)&new_value, 4 ) ) {
		cfg_data.sysrstct = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter Power_Cycle_Counter
//	
//
U32 CFG_Get_Power_Cycle_Counter(void) {
	return cfg_data.pwrcycnt;
}

S16 CFG_Set_Power_Cycle_Counter( U32 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_PWRCYCNT, (U8*)&new_value, 4 ) ) {
		cfg_data.pwrcycnt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}


//
//	Configuration Parameter Countdown
//	most Operation Modes
//
U16 CFG_Get_Countdown(void) {
	return cfg_data.countdwn;
}

S16 CFG_Set_Countdown( U16 new_value ) {
	if ( COUNTDWN_MIN <= new_value && new_value <= COUNTDWN_MAX ) {
		if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_COUNTDWN, (U8*)&new_value, 2 ) ) {
			cfg_data.countdwn = new_value;
			return CFG_OK;
		} else {
			return CFG_FAIL;
		}
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Countdown_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_Countdown ( (U16)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter APM_Command_Timeout
//	0 means no timeout
//
U16 CFG_Get_APM_Command_Timeout(void) {
	return cfg_data.apmcmdto;
}

S16 CFG_Set_APM_Command_Timeout( U16 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_APMCMDTO, (U8*)&new_value, 2 ) ) {
		cfg_data.apmcmdto = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_APM_Command_Timeout_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_APM_Command_Timeout ( (U16)new_num );
	else
		return CFG_FAIL;
}


//	Acquisition Parameters


//
//	Configuration Parameter Saturation_Counts
//	
//
U16 CFG_Get_Saturation_Counts(void) {
	return cfg_data.saturcnt;
}

S16 CFG_Set_Saturation_Counts( U16 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_SATURCNT, (U8*)&new_value, 2 ) ) {
		cfg_data.saturcnt = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Saturation_Counts_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x10000 )
		return CFG_Set_Saturation_Counts ( (U16)new_num );
	else
		return CFG_FAIL;
}


//
//	Configuration Parameter Number_of_Clearouts
//	
//
U8 CFG_Get_Number_of_Clearouts(void) {
	return cfg_data.numclear;
}

S16 CFG_Set_Number_of_Clearouts( U8 new_value ) {
	if ( CFG_OK == CFG_VarSaveToRTC ( 0, BBV_LOC_NUMCLEAR, (U8*)&new_value, 1 ) ) {
		cfg_data.numclear = new_value;
		return CFG_OK;
	} else {
		return CFG_FAIL;
	}
}

S16 CFG_Set_Number_of_Clearouts_AsString( char* new_value ) {
	errno = 0;
	U32 new_num = strtoul(new_value, (char**)0, 0 );
	if ( 0 == errno && new_num < 0x100 )
		return CFG_Set_Number_of_Clearouts ( (U8)new_num );
	else
		return CFG_FAIL;
}

S16 CFG_CmdGet ( char* option, char* result, S16 r_max_len )
{
	S16 const r_max_len_m1 = r_max_len-1;

	S16 cec = CEC_Ok;

	if ( !result || r_max_len < 16 ) {
		return CEC_Failed;
	}

	result[0] = 0;


	//	Build Parameters

	if ( 0 == strcasecmp ( option, "SENSTYPE" ) )
	{
		strncpy ( result, CFG_Get_Sensor_Type_AsString(), r_max_len_m1 );
	}
	else if ( 0 == strcasecmp ( option, "SENSVERS" ) )
	{
		strncpy ( result, CFG_Get_Sensor_Version_AsString(), r_max_len_m1 );
	}
	else if ( 0 == strcasecmp ( option, "SERIALNO" ) )
	{
		snprintf ( result, r_max_len, "%hu", CFG_Get_Serial_Number() );
	}
	else if ( 0 == strcasecmp ( option, "PWRSVISR" ) )
	{
		strncpy ( result, CFG_Get_PCB_Supervisor_AsString(), r_max_len_m1 );
	}
	else if ( 0 == strcasecmp ( option, "USBSWTCH" ) )
	{
		strncpy ( result, CFG_Get_USB_Switch_AsString(), r_max_len_m1 );
	}
	else if ( 0 == strcasecmp ( option, "SPCSBDSN" ) )
	{
		snprintf ( result, r_max_len, "%lu", CFG_Get_Spec_Starboard_Serial_Number() );
	}
	else if ( 0 == strcasecmp ( option, "SPCPRTSN" ) )
	{
		snprintf ( result, r_max_len, "%lu", CFG_Get_Spec_Port_Serial_Number() );
	}
	else if ( 0 == strcasecmp ( option, "FRMSBDSN" ) )
	{
		snprintf ( result, r_max_len, "%hu", CFG_Get_Frame_Starboard_Serial_Number() );
	}
	else if ( 0 == strcasecmp ( option, "FRMPRTSN" ) )
	{
		snprintf ( result, r_max_len, "%hu", CFG_Get_Frame_Port_Serial_Number() );
	}
	else if ( 0 == strcasecmp ( option, "ADMIN_PW" ) )
	{
		strncpy ( result, CFG_Get_Admin_Password(), r_max_len_m1 );
	}
	else if ( 0 == strcasecmp ( option, "FCTRY_PW" ) )
	{
		strncpy ( result, CFG_Get_Factory_Password(), r_max_len_m1 );

	//	Accelerometer Parameters

	}
	else if ( 0 == strcasecmp ( option, "ACCMNTNG" ) )
	{
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Accelerometer_Mounting() ) );
	}
	else if ( 0 == strcasecmp ( option, "ACCVERTX" ) )
	{
		snprintf ( result, r_max_len, "%hd", CFG_Get_Accelerometer_Vertical_x() );
	}
	else if ( 0 == strcasecmp ( option, "ACCVERTY" ) )
	{
		snprintf ( result, r_max_len, "%hd", CFG_Get_Accelerometer_Vertical_y() );
	}
	else if ( 0 == strcasecmp ( option, "ACCVERTZ" ) )
	{
		snprintf ( result, r_max_len, "%hd", CFG_Get_Accelerometer_Vertical_z() );

	//	Compass Parameters

	} else if ( 0 == strcasecmp ( option, "MAG_MINX" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Min_x() );
	} else if ( 0 == strcasecmp ( option, "MAG_MAXX" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Max_x() );
	} else if ( 0 == strcasecmp ( option, "MAG_MINY" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Min_y() );
	} else if ( 0 == strcasecmp ( option, "MAG_MAXY" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Max_y() );
	} else if ( 0 == strcasecmp ( option, "MAG_MINZ" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Min_z() );
	} else if ( 0 == strcasecmp ( option, "MAG_MAXZ" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Magnetometer_Max_z() );

	//	GPS Parameters

	} else if ( 0 == strcasecmp ( option, "GPS_LATI" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Latitude() ) );
	} else if ( 0 == strcasecmp ( option, "GPS_LONG" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Longitude() ) );
	} else if ( 0 == strcasecmp ( option, "MAGDECLI" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Magnetic_Declination() ) );

	//	Pressure Parameters

	} else if ( 0 == strcasecmp ( option, "DIGIQZSN" ) ) {
		snprintf ( result, r_max_len, "%lu", CFG_Get_Digiquartz_Serial_Number() );
	} else if ( 0 == strcasecmp ( option, "DIGIQZU0" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_U0() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZY1" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_Y1() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZY2" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_Y2() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZY3" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_Y3() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZC1" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_C1() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZC2" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_C2() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZC3" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_C3() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZD1" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_D1() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZD2" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_D2() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZT1" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_T1() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZT2" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_T2() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZT3" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_T3() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZT4" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_T4() ) );
	} else if ( 0 == strcasecmp ( option, "DIGIQZT5" ) ) {
		snprintf ( result, r_max_len, "%lf", printAbleF64 ( CFG_Get_Digiquartz_T5() ) );
	} else if ( 0 == strcasecmp ( option, "DQTEMPDV" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Digiquartz_Temperature_Divisor() );
	} else if ( 0 == strcasecmp ( option, "DQPRESDV" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Digiquartz_Pressure_Divisor() );
	} else if ( 0 == strcasecmp ( option, "SIMDEPTH" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Simulated_Start_Depth() );
	} else if ( 0 == strcasecmp ( option, "SIMASCNT" ) ) {
		snprintf ( result, r_max_len, "%hd", CFG_Get_Sumulated_Ascent_Rate() );

	//	Profile Parameters

	} else if ( 0 == strcasecmp ( option, "PUPPIVAL" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Upper_Interval() ) );
	} else if ( 0 == strcasecmp ( option, "PUPPSTRT" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Upper_Start() ) );
	} else if ( 0 == strcasecmp ( option, "PMIDIVAL" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Middle_Interval() ) );
	} else if ( 0 == strcasecmp ( option, "PMIDSTRT" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Middle_Start() ) );
	} else if ( 0 == strcasecmp ( option, "PLOWIVAL" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Lower_Interval() ) );
	} else if ( 0 == strcasecmp ( option, "PLOWSTRT" ) ) {
		snprintf ( result, r_max_len, "%f", printAbleF32 ( CFG_Get_Profile_Lower_Start() ) );

	//	Setup Parameters

	} else if ( 0 == strcasecmp ( option, "STUPSTUS" ) ) {
		strncpy ( result, CFG_Get_Configuration_Status_AsString(), r_max_len_m1 );

	//	Input_Output Parameters

	} else if ( 0 == strcasecmp ( option, "MSGLEVEL" ) ) {
		strncpy ( result, CFG_Get_Message_Level_AsString(), r_max_len_m1 );
	} else if ( 0 == strcasecmp ( option, "MSGFSIZE" ) ) {
		snprintf ( result, r_max_len, "%hhu", CFG_Get_Message_File_Size() );
	} else if ( 0 == strcasecmp ( option, "DATFSIZE" ) ) {
		snprintf ( result, r_max_len, "%hhu", CFG_Get_Data_File_Size() );
	} else if ( 0 == strcasecmp ( option, "OUTFRSUB" ) ) {
		snprintf ( result, r_max_len, "%hhu", CFG_Get_Output_Frame_Subsampling() );
	} else if ( 0 == strcasecmp ( option, "LOGFRAMS" ) ) {
		strncpy ( result, CFG_Get_Log_Frames_AsString(), r_max_len_m1 );
	} else if ( 0 == strcasecmp ( option, "ACQCOUNT" ) ) {
		snprintf ( result, r_max_len, "%lu", CFG_Get_Acquisition_Counter() );
	} else if ( 0 == strcasecmp ( option, "CNTCOUNT" ) ) {
		snprintf ( result, r_max_len, "%lu", CFG_Get_Continuous_Counter() );

	//	Testing Parameters

	} else if ( 0 == strcasecmp ( option, "DATAMODE" ) ) {
		strncpy ( result, CFG_Get_Data_Mode_AsString(), r_max_len_m1 );

	//	Operational Parameters
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
	} else if ( 0 == strcasecmp ( option, "OPERMODE" ) ) {
		strncpy ( result, CFG_Get_Operation_Mode_AsString(), r_max_len_m1 );
# endif
	} else if ( 0 == strcasecmp ( option, "OPERCTRL" ) ) {
		strncpy ( result, CFG_Get_Operation_Control_AsString(), r_max_len_m1 );
	} else if ( 0 == strcasecmp ( option, "COUNTDWN" ) ) {
		snprintf ( result, r_max_len, "%hu", CFG_Get_Countdown() );
	} else if ( 0 == strcasecmp ( option, "APMCMDTO" ) ) {
		snprintf ( result, r_max_len, "%hu", CFG_Get_APM_Command_Timeout() );

	//	Acquisition Parameters

	} else if ( 0 == strcasecmp ( option, "SATURCNT" ) ) {
		snprintf ( result, r_max_len, "%hu", CFG_Get_Saturation_Counts() );
	} else if ( 0 == strcasecmp ( option, "NUMCLEAR" ) ) {
		snprintf ( result, r_max_len, "%hhu", CFG_Get_Number_of_Clearouts() );
	} else if ( 0 == strcasecmp ( option, "FirmwareVersion" ) ) {
		strncpy ( result, HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_CTRL_FW_VERSION_PATCH, r_max_len_m1 );
	} else if ( 0 == strcasecmp ( option, "ExtPower" ) ) {
		F32 v, p3v3;
		if ( SYSMON_OK == sysmon_getVoltages(&v, &p3v3) ) {
			cec = CEC_Ok;
			if(v >= 6)
				strncpy ( result, "On", r_max_len_m1 );
			else
				strncpy ( result, "Off", r_max_len_m1 );
		} else {
			cec = CEC_Failed;
		}
	} else if ( 0 == strcasecmp ( option, "USBCom" ) ) {
		if ( Is_usb_vbus_high() && Is_device_enumerated() ) {
			strncpy ( result, "On", r_max_len_m1 );
		} else {
			strncpy ( result, "Off", r_max_len_m1 );
		}
		cec = CEC_Ok;
	} else if ( 0 == strcasecmp ( option, "PkgCRC" ) ) {
		cec = FSYS_CmdCRC ( "PKG", 0, result, r_max_len );
	} else if ( 0 == strcasecmp ( option, "Clock" ) ) {
		strncpy ( result, str_time_formatted ( time( (time_t*)0 ), DATE_TIME_IO_FORMAT ), r_max_len_m1 );
		cec = CEC_Ok;
	} else if ( 0 == strncasecmp ( option, "Disk", 4 ) ) {
		U64 total;
		U64 avail;
		if ( FILE_OK != f_space ( EMMC_DRIVE_CHAR, &avail, &total ) ) {
			cec = CEC_Failed;
		} else {
			if ( 0 == strcasecmp ( "Total", option+4 ) ) {
				snprintf ( result, r_max_len, "%llu", total );
				cec = CEC_Ok;
			} else if ( 0 == strcasecmp ( "Free", option+4 ) ) {
				snprintf ( result, r_max_len, "%llu", avail );
				cec = CEC_Ok;
			} else {
				cec = CEC_Failed;
			}
		}
	} else if ( 0 == strcasecmp ( option, "cfg" ) ) {
		CFG_Print();
	} else {
		cec = CEC_ConfigVariableUnknown;
	}

	return cec;
}

S16 CFG_CmdSet ( char* option, char* value, Access_Mode_t access_mode ) {

	if ( 0 == option || 0 == *option ) return CEC_Failed;
	if ( 0 == value  || 0 == *value  ) return CEC_Failed;

	S16 cec = CEC_Ok;


	//	Build Parameters

	if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SENSTYPE" ) ) {
		if ( CFG_FAIL == CFG_Set_Sensor_Type_AsString ( value ) ) cec = CEC_Sensor_Type_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SENSVERS" ) ) {
		if ( CFG_FAIL == CFG_Set_Sensor_Version_AsString ( value ) ) cec = CEC_Sensor_Version_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SERIALNO" ) ) {
		if ( CFG_FAIL == CFG_Set_Serial_Number_AsString ( value ) ) cec = CEC_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "PWRSVISR" ) ) {
		if ( CFG_FAIL == CFG_Set_PCB_Supervisor_AsString ( value ) ) cec = CEC_PCB_Supervisor_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "USBSWTCH" ) ) {
		if ( CFG_FAIL == CFG_Set_USB_Switch_AsString ( value ) ) cec = CEC_USB_Switch_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SPCSBDSN" ) ) {
		if ( CFG_FAIL == CFG_Set_Spec_Starboard_Serial_Number_AsString ( value ) ) cec = CEC_Spec_Starboard_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SPCPRTSN" ) ) {
		if ( CFG_FAIL == CFG_Set_Spec_Port_Serial_Number_AsString ( value ) ) cec = CEC_Spec_Port_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "FRMSBDSN" ) ) {
		if ( CFG_FAIL == CFG_Set_Frame_Starboard_Serial_Number_AsString ( value ) ) cec = CEC_Frame_Starboard_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "FRMPRTSN" ) ) {
		if ( CFG_FAIL == CFG_Set_Frame_Port_Serial_Number_AsString ( value ) ) cec = CEC_Frame_Port_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "ADMIN_PW" ) ) {
		if ( CFG_FAIL == CFG_Set_Admin_Password ( value ) ) cec = CEC_Admin_Password_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "FCTRY_PW" ) ) {
		if ( CFG_FAIL == CFG_Set_Factory_Password ( value ) ) cec = CEC_Factory_Password_Invalid;


	//	Accelerometer Parameters

	} else if ( 0 == strcasecmp ( option, "ACCMNTNG" ) ) {
		if ( CFG_FAIL == CFG_Set_Accelerometer_Mounting_AsString ( value ) ) cec = CEC_Accelerometer_Mounting_Invalid;

	} else if ( 0 == strcasecmp ( option, "ACCVERTX" ) ) {
		if ( CFG_FAIL == CFG_Set_Accelerometer_Vertical_x_AsString ( value ) ) cec = CEC_Accelerometer_Vertical_x_Invalid;

	} else if ( 0 == strcasecmp ( option, "ACCVERTY" ) ) {
		if ( CFG_FAIL == CFG_Set_Accelerometer_Vertical_y_AsString ( value ) ) cec = CEC_Accelerometer_Vertical_y_Invalid;

	} else if ( 0 == strcasecmp ( option, "ACCVERTZ" ) ) {
		if ( CFG_FAIL == CFG_Set_Accelerometer_Vertical_z_AsString ( value ) ) cec = CEC_Accelerometer_Vertical_z_Invalid;


	//	Compass Parameters

	} else if ( 0 == strcasecmp ( option, "MAG_MINX" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Min_x_AsString ( value ) ) cec = CEC_Magnetometer_Min_x_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAG_MAXX" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Max_x_AsString ( value ) ) cec = CEC_Magnetometer_Max_x_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAG_MINY" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Min_y_AsString ( value ) ) cec = CEC_Magnetometer_Min_y_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAG_MAXY" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Max_y_AsString ( value ) ) cec = CEC_Magnetometer_Max_y_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAG_MINZ" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Min_z_AsString ( value ) ) cec = CEC_Magnetometer_Min_z_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAG_MAXZ" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetometer_Max_z_AsString ( value ) ) cec = CEC_Magnetometer_Max_z_Invalid;


	//	GPS Parameters

	} else if ( 0 == strcasecmp ( option, "GPS_LATI" ) ) {
		if ( CFG_FAIL == CFG_Set_Latitude_AsString ( value ) ) cec = CEC_Latitude_Invalid;

	} else if ( 0 == strcasecmp ( option, "GPS_LONG" ) ) {
		if ( CFG_FAIL == CFG_Set_Longitude_AsString ( value ) ) cec = CEC_Longitude_Invalid;

	} else if ( 0 == strcasecmp ( option, "MAGDECLI" ) ) {
		if ( CFG_FAIL == CFG_Set_Magnetic_Declination_AsString ( value ) ) cec = CEC_Magnetic_Declination_Invalid;


	//	Pressure Parameters

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZSN" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Serial_Number_AsString ( value ) ) cec = CEC_Digiquartz_Serial_Number_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZU0" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_U0_AsString ( value ) ) cec = CEC_Digiquartz_U0_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZY1" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Y1_AsString ( value ) ) cec = CEC_Digiquartz_Y1_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZY2" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Y2_AsString ( value ) ) cec = CEC_Digiquartz_Y2_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZY3" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Y3_AsString ( value ) ) cec = CEC_Digiquartz_Y3_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZC1" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_C1_AsString ( value ) ) cec = CEC_Digiquartz_C1_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZC2" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_C2_AsString ( value ) ) cec = CEC_Digiquartz_C2_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZC3" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_C3_AsString ( value ) ) cec = CEC_Digiquartz_C3_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZD1" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_D1_AsString ( value ) ) cec = CEC_Digiquartz_D1_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZD2" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_D2_AsString ( value ) ) cec = CEC_Digiquartz_D2_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZT1" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_T1_AsString ( value ) ) cec = CEC_Digiquartz_T1_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZT2" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_T2_AsString ( value ) ) cec = CEC_Digiquartz_T2_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZT3" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_T3_AsString ( value ) ) cec = CEC_Digiquartz_T3_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZT4" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_T4_AsString ( value ) ) cec = CEC_Digiquartz_T4_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DIGIQZT5" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_T5_AsString ( value ) ) cec = CEC_Digiquartz_T5_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DQTEMPDV" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Temperature_Divisor_AsString ( value ) ) cec = CEC_Digiquartz_Temperature_Divisor_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "DQPRESDV" ) ) {
		if ( CFG_FAIL == CFG_Set_Digiquartz_Pressure_Divisor_AsString ( value ) ) cec = CEC_Digiquartz_Pressure_Divisor_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SIMDEPTH" ) ) {
		if ( CFG_FAIL == CFG_Set_Simulated_Start_Depth_AsString ( value ) ) cec = CEC_Simulated_Start_Depth_Invalid;

	} else if ((access_mode>=Access_Factory) && 0 == strcasecmp ( option, "SIMASCNT" ) ) {
		if ( CFG_FAIL == CFG_Set_Sumulated_Ascent_Rate_AsString ( value ) ) cec = CEC_Sumulated_Ascent_Rate_Invalid;


	//	Profile Parameters

	} else if ( 0 == strcasecmp ( option, "PUPPIVAL" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Upper_Interval_AsString ( value ) ) cec = CEC_Profile_Upper_Interval_Invalid;

	} else if ( 0 == strcasecmp ( option, "PUPPSTRT" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Upper_Start_AsString ( value ) ) cec = CEC_Profile_Upper_Start_Invalid;

	} else if ( 0 == strcasecmp ( option, "PMIDIVAL" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Middle_Interval_AsString ( value ) ) cec = CEC_Profile_Middle_Interval_Invalid;

	} else if ( 0 == strcasecmp ( option, "PMIDSTRT" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Middle_Start_AsString ( value ) ) cec = CEC_Profile_Middle_Start_Invalid;

	} else if ( 0 == strcasecmp ( option, "PLOWIVAL" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Lower_Interval_AsString ( value ) ) cec = CEC_Profile_Lower_Interval_Invalid;

	} else if ( 0 == strcasecmp ( option, "PLOWSTRT" ) ) {
		if ( CFG_FAIL == CFG_Set_Profile_Lower_Start_AsString ( value ) ) cec = CEC_Profile_Lower_Start_Invalid;


	//	Setup Parameters

	} else if ((access_mode>=Access_Admin) && 0 == strcasecmp ( option, "STUPSTUS" ) ) {
		if ( CFG_FAIL == CFG_Set_Configuration_Status_AsString ( value ) ) cec = CEC_Configuration_Status_Invalid;


	//	Input_Output Parameters

	} else if ( 0 == strcasecmp ( option, "MSGLEVEL" ) ) {
		if ( CFG_FAIL == CFG_Set_Message_Level_AsString ( value ) ) cec = CEC_Message_Level_Invalid;

	} else if ( 0 == strcasecmp ( option, "MSGFSIZE" ) ) {
		if ( CFG_FAIL == CFG_Set_Message_File_Size_AsString ( value ) ) cec = CEC_Message_File_Size_Invalid;

	} else if ( 0 == strcasecmp ( option, "DATFSIZE" ) ) {
		if ( CFG_FAIL == CFG_Set_Data_File_Size_AsString ( value ) ) cec = CEC_Data_File_Size_Invalid;

	} else if ( 0 == strcasecmp ( option, "OUTFRSUB" ) ) {
		if ( CFG_FAIL == CFG_Set_Output_Frame_Subsampling_AsString ( value ) ) cec = CEC_Output_Frame_Subsampling_Invalid;

	} else if ( 0 == strcasecmp ( option, "LOGFRAMS" ) ) {
		if ( CFG_FAIL == CFG_Set_Log_Frames_AsString ( value ) ) cec = CEC_Log_Frames_Invalid;

	} else if ( 0 == strcasecmp ( option, "ACQCOUNT" ) ) {
		if ( CFG_FAIL == CFG_Set_Acquisition_Counter_AsString ( value ) ) cec = CEC_Acquisition_Counter_Invalid;

	} else if ( 0 == strcasecmp ( option, "CNTCOUNT" ) ) {
		if ( CFG_FAIL == CFG_Set_Continuous_Counter_AsString ( value ) ) cec = CEC_Continuous_Counter_Invalid;


	//	Testing Parameters

	} else if ((access_mode>=Access_Admin) && 0 == strcasecmp ( option, "DATAMODE" ) ) {
		if ( CFG_FAIL == CFG_Set_Data_Mode_AsString ( value ) ) cec = CEC_Data_Mode_Invalid;


	//	Operational Parameters

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
	} else if ( 0 == strcasecmp ( option, "OPERMODE" ) ) {
		if ( CFG_FAIL == CFG_Set_Operation_Mode_AsString ( value ) ) cec = CEC_Operation_Mode_Invalid;
# endif

	} else if ( 0 == strcasecmp ( option, "OPERCTRL" ) ) {
		if ( CFG_FAIL == CFG_Set_Operation_Control_AsString ( value ) ) cec = CEC_Operation_Control_Invalid;

	} else if ( 0 == strcasecmp ( option, "COUNTDWN" ) ) {
		if ( CFG_FAIL == CFG_Set_Countdown_AsString ( value ) ) cec = CEC_Countdown_Invalid;

	} else if ( 0 == strcasecmp ( option, "APMCMDTO" ) ) {
		if ( CFG_FAIL == CFG_Set_APM_Command_Timeout_AsString ( value ) ) cec = CEC_APM_Command_Timeout_Invalid;


	//	Acquisition Parameters

	} else if ( 0 == strcasecmp ( option, "SATURCNT" ) ) {
		if ( CFG_FAIL == CFG_Set_Saturation_Counts_AsString ( value ) ) cec = CEC_Saturation_Counts_Invalid;

	} else if ( 0 == strcasecmp ( option, "NUMCLEAR" ) ) {
		if ( CFG_FAIL == CFG_Set_Number_of_Clearouts_AsString ( value ) ) cec = CEC_Number_of_Clearouts_Invalid;

	} else if ( 0 == strcasecmp ( option, "Clock" ) ) {
		struct tm tt;
		if ( strptime ( value, DATE_TIME_IO_FORMAT, &tt ) ) {
			struct timeval tv;
			tv.tv_sec = mktime ( &tt );
			tv.tv_usec = 0;
			settimeofday ( &tv, 0 );
			time_sys2ext();
			cec = CEC_Ok;
		} else {
			cec = CEC_Failed;
		}
	} else {
		cec = CEC_ConfigVariableUnknown;
	}

	return cec;
}

void CFG_Print () {

	io_out_string ( "\r\n" );


	//	Build Parameters
	io_out_string ( "SENSTYPE " ); 	io_out_string ( CFG_Get_Sensor_Type_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "SENSVERS " ); 	io_out_string ( CFG_Get_Sensor_Version_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "SERIALNO " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Serial_Number() );
	io_out_string ( "PWRSVISR " ); 	io_out_string ( CFG_Get_PCB_Supervisor_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "USBSWTCH " ); 	io_out_string ( CFG_Get_USB_Switch_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "SPCSBDSN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Spec_Starboard_Serial_Number() );
	io_out_string ( "SPCPRTSN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Spec_Port_Serial_Number() );
	io_out_string ( "FRMSBDSN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Frame_Starboard_Serial_Number() );
	io_out_string ( "FRMPRTSN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Frame_Port_Serial_Number() );

	//	Accelerometer Parameters
	io_out_string ( "ACCMNTNG " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Accelerometer_Mounting() );
	io_out_string ( "ACCVERTX " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Accelerometer_Vertical_x() );
	io_out_string ( "ACCVERTY " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Accelerometer_Vertical_y() );
	io_out_string ( "ACCVERTZ " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Accelerometer_Vertical_z() );

	//	Compass Parameters
	io_out_string ( "MAG_MINX " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Min_x() );
	io_out_string ( "MAG_MAXX " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Max_x() );
	io_out_string ( "MAG_MINY " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Min_y() );
	io_out_string ( "MAG_MAXY " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Max_y() );
	io_out_string ( "MAG_MINZ " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Min_z() );
	io_out_string ( "MAG_MAXZ " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Magnetometer_Max_z() );

	//	GPS Parameters
	io_out_string ( "GPS_LATI " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Latitude() );
	io_out_string ( "GPS_LONG " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Longitude() );
	io_out_string ( "MAGDECLI " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Magnetic_Declination() );

	//	Pressure Parameters
	io_out_string ( "DIGIQZSN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Digiquartz_Serial_Number() );
	io_out_string ( "DIGIQZU0 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_U0() );
	io_out_string ( "DIGIQZY1 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_Y1() );
	io_out_string ( "DIGIQZY2 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_Y2() );
	io_out_string ( "DIGIQZY3 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_Y3() );
	io_out_string ( "DIGIQZC1 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_C1() );
	io_out_string ( "DIGIQZC2 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_C2() );
	io_out_string ( "DIGIQZC3 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_C3() );
	io_out_string ( "DIGIQZD1 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_D1() );
	io_out_string ( "DIGIQZD2 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_D2() );
	io_out_string ( "DIGIQZT1 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_T1() );
	io_out_string ( "DIGIQZT2 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_T2() );
	io_out_string ( "DIGIQZT3 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_T3() );
	io_out_string ( "DIGIQZT4 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_T4() );
	io_out_string ( "DIGIQZT5 " ); 	io_out_F64 ( "%lf\r\n", CFG_Get_Digiquartz_T5() );
	io_out_string ( "DQTEMPDV " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Digiquartz_Temperature_Divisor() );
	io_out_string ( "DQPRESDV " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Digiquartz_Pressure_Divisor() );
	io_out_string ( "SIMDEPTH " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Simulated_Start_Depth() );
	io_out_string ( "SIMASCNT " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Sumulated_Ascent_Rate() );

	//	Profile Parameters
	io_out_string ( "PUPPIVAL " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Upper_Interval() );
	io_out_string ( "PUPPSTRT " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Upper_Start() );
	io_out_string ( "PMIDIVAL " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Middle_Interval() );
	io_out_string ( "PMIDSTRT " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Middle_Start() );
	io_out_string ( "PLOWIVAL " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Lower_Interval() );
	io_out_string ( "PLOWSTRT " ); 	io_out_F32 ( "%f\r\n", CFG_Get_Profile_Lower_Start() );

	//	Setup Parameters
	io_out_string ( "STUPSTUS " ); 	io_out_string ( CFG_Get_Configuration_Status_AsString() ); io_out_string ( "\r\n" );

	//	Input_Output Parameters
	io_out_string ( "MSGLEVEL " ); 	io_out_string ( CFG_Get_Message_Level_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "MSGFSIZE " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Message_File_Size() );
	io_out_string ( "DATFSIZE " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Data_File_Size() );
	io_out_string ( "OUTFRSUB " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Output_Frame_Subsampling() );
	io_out_string ( "LOGFRAMS " ); 	io_out_string ( CFG_Get_Log_Frames_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "ACQCOUNT " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Acquisition_Counter() );
	io_out_string ( "CNTCOUNT " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Continuous_Counter() );

	//	Testing Parameters
	io_out_string ( "DATAMODE " ); 	io_out_string ( CFG_Get_Data_Mode_AsString() ); io_out_string ( "\r\n" );

	//	Operational Parameters
# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
	io_out_string ( "OPERMODE " ); 	io_out_string ( CFG_Get_Operation_Mode_AsString() ); io_out_string ( "\r\n" );
# endif
	io_out_string ( "OPERCTRL " ); 	io_out_string ( CFG_Get_Operation_Control_AsString() ); io_out_string ( "\r\n" );
	io_out_string ( "COUNTDWN " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Countdown() );
	io_out_string ( "APMCMDTO " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_APM_Command_Timeout() );

	//	Acquisition Parameters
	io_out_string ( "SATURCNT " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Saturation_Counts() );
	io_out_string ( "NUMCLEAR " ); 	io_out_S32 ( "%ld\r\n", (S32)CFG_Get_Number_of_Clearouts() );
}

S16 CFG_OutOfRangeCounter ( bool correctOOR, bool setRTCToDefault ) {

	S16 cnt = 0;

	if ( CFG_Sensor_Type_HyperNavRadiometer > cfg_data.senstype || cfg_data.senstype > CFG_Sensor_Type_HyperNavTestHead ) {
		cnt++;
	}
	if ( CFG_Sensor_Version_V1 > cfg_data.sensvers || cfg_data.sensvers > CFG_Sensor_Version_V1 ) {
		cnt++;
	}
	if ( SERIALNO_MIN > cfg_data.serialno || cfg_data.serialno > SERIALNO_MAX ) {
		cnt++;
	}
	if ( CFG_PCB_Supervisor_Available > cfg_data.pwrsvisr || cfg_data.pwrsvisr > CFG_PCB_Supervisor_Missing ) {
		cnt++;
	}
	if ( CFG_USB_Switch_Available > cfg_data.usbswtch || cfg_data.usbswtch > CFG_USB_Switch_Missing ) {
		cnt++;
	}
	if ( cfg_data.frmsbdsn > FRMSBDSN_MAX ) {
		cnt++;
	}
	if ( cfg_data.frmprtsn > FRMPRTSN_MAX ) {
		cnt++;
	}
	if ( CFG_Configuration_Status_Required > cfg_data.stupstus || cfg_data.stupstus > CFG_Configuration_Status_Done ) {
		cnt++;
		if ( correctOOR ) cfg_data.stupstus = STUPSTUS_DEF;
	}
	if ( setRTCToDefault ) cfg_data.stupstus = STUPSTUS_DEF;

	if ( CFG_Message_Level_Error > cfg_data.msglevel || cfg_data.msglevel > CFG_Message_Level_Trace ) {
		cnt++;
		if ( correctOOR ) cfg_data.msglevel = MSGLEVEL_DEF;
	}
	if ( setRTCToDefault ) cfg_data.msglevel = MSGLEVEL_DEF;

	if ( cfg_data.msgfsize > MSGFSIZE_MAX ) {
		cnt++;
		if ( correctOOR ) cfg_data.msgfsize = MSGFSIZE_DEF;
	}
	if ( setRTCToDefault ) cfg_data.msgfsize = MSGFSIZE_DEF;

	if ( DATFSIZE_MIN > cfg_data.datfsize || cfg_data.datfsize > DATFSIZE_MAX ) {
		cnt++;
		if ( correctOOR ) cfg_data.datfsize = DATFSIZE_DEF;
	}
	if ( setRTCToDefault ) cfg_data.datfsize = DATFSIZE_DEF;

	if ( cfg_data.outfrsub > OUTFRSUB_MAX ) {
		cnt++;
		if ( correctOOR ) cfg_data.outfrsub = OUTFRSUB_DEF;
	}
	if ( setRTCToDefault ) cfg_data.outfrsub = OUTFRSUB_DEF;

	if ( CFG_Log_Frames_Yes > cfg_data.logframs || cfg_data.logframs > CFG_Log_Frames_No ) {
		cnt++;
		if ( correctOOR ) cfg_data.logframs = LOGFRAMS_DEF;
	}
	if ( setRTCToDefault ) cfg_data.logframs = LOGFRAMS_DEF;

	if ( setRTCToDefault ) cfg_data.acqcount = ACQCOUNT_DEF;

	if ( setRTCToDefault ) cfg_data.cntcount = CNTCOUNT_DEF;

	if ( CFG_Data_Mode_Real > cfg_data.datamode || cfg_data.datamode > CFG_Data_Mode_Fake ) {
		cnt++;
		if ( correctOOR ) cfg_data.datamode = DATAMODE_DEF;
	}
	if ( setRTCToDefault ) cfg_data.datamode = DATAMODE_DEF;

# if defined(OPERATION_AUTONOMOUS) && defined(OPERATION_NAVIS)
	if ( CFG_Operation_Mode_Continuous > cfg_data.opermode || cfg_data.opermode > CFG_Operation_Mode_APM ) {
		cnt++;
		if ( correctOOR ) cfg_data.opermode = OPERMODE_DEF;
	}
# endif

	if ( setRTCToDefault ) cfg_data.opermode = OPERMODE_DEF;

	if ( CFG_Operation_Control_Duration > cfg_data.operctrl || cfg_data.operctrl > CFG_Operation_Control_Samples ) {
		cnt++;
		if ( correctOOR ) cfg_data.operctrl = OPERCTRL_DEF;
	}
	if ( setRTCToDefault ) cfg_data.operctrl = OPERCTRL_DEF;

	if ( COUNTDWN_MIN > cfg_data.countdwn || cfg_data.countdwn > COUNTDWN_MAX ) {
		cnt++;
		if ( correctOOR ) cfg_data.countdwn = COUNTDWN_DEF;
	}
	if ( setRTCToDefault ) cfg_data.countdwn = COUNTDWN_DEF;

	if ( setRTCToDefault ) cfg_data.apmcmdto = APMCMDTO_DEF;

	if ( setRTCToDefault ) cfg_data.saturcnt = SATURCNT_DEF;

	if ( setRTCToDefault ) cfg_data.numclear = NUMCLEAR_DEF;


	return cnt;
}


/*
 *	Backup functions
 */

static char const* cfg_MakeBackupFileName () {

	return EMMC_DRIVE "config.bck";
}

static S16 cfg_SaveToBackup( char* backupMemory ) {

	char const* bfn = cfg_MakeBackupFileName();

	bool have_to_write = true;

	//	Compare content in backup file to backupMemory.
	//	If identical, no need to write to file.
	//	Do via CRC of file vs. CRC of backupMemory.
	//	TODO: Confirm this is ok to do.

	U16 crc_of_file = 0;
	if ( CEC_Ok == FSYS_getCRCOfFile( bfn, &crc_of_file ) ) {
	
		if ( crc_of_file == fCrc16BitN ( (unsigned char*)backupMemory, CFG_BACK_SIZE ) ) {
			have_to_write = false;
		}
	}

	if ( have_to_write ) {

		f_delete ( bfn );

		fHandler_t fh;
		if ( FILE_FAIL == f_open( bfn, O_WRONLY|O_CREAT, &fh ) ) {
			return CFG_FAIL;
		}

		if ( CFG_BACK_SIZE != f_write ( &fh, backupMemory, CFG_BACK_SIZE ) ) {
			f_close ( &fh );
			f_delete ( bfn );
			return CFG_FAIL;
		}

		f_close ( &fh );
	}

	return CFG_OK;
}

static S16 cfg_ReadFromBackup( char* backupMemory ) {

	char const* bfn = cfg_MakeBackupFileName();

	fHandler_t fh;
	if ( FILE_FAIL == f_open( bfn, O_RDONLY, &fh ) ) {
		return CFG_FAIL;
	}

	if ( CFG_BACK_SIZE != f_read ( &fh, backupMemory, CFG_BACK_SIZE ) ) {
		f_close ( &fh );
		f_delete ( bfn );
		return CFG_FAIL;
	}

	f_close ( &fh );
	return CFG_OK;
}

S16 CFG_WipeBackup() {

	char const* bfn = cfg_MakeBackupFileName();

	return FILE_OK == f_delete ( bfn )
	     ? CFG_OK
	     : CFG_FAIL;
}
