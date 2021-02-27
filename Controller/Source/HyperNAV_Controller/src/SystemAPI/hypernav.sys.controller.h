/*! \file hypernav.sys.controller.h *****************************************
 *
 * \brief HyperNAV system - Controller - control
 *
 *
 * @author      BP, Satlantic
 * @date        2015-08-19
 *
  ***************************************************************************/

#ifndef _HYPERNAV_SYS_CONTROLLER_H_
#define _HYPERNAV_SYS_CONTROLLER_H_

#include <compiler.h>


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define HNV_SYS_CTRL_OK		0
#define HNV_SYS_CTRL_FAIL	-1


// Reset causes
enum {
	HNV_SYS_CTRL_UNKNOWN_RST=0,
	HNV_SYS_CTRL_WDT_RST,
	HNV_SYS_CTRL_PCYCLE_RST,
	HNV_SYS_CTRL_EXT_RST,
	HNV_SYS_CTRL_BOD_RST,
	HNV_SYS_CTRL_CPUERR_RST
};

//*****************************************************************************
// Exported data types
//*****************************************************************************

// System initialization parameters. This structure is used by 'hnv_sys_ctrl_init()' to
// obtain parameters to be sent to sub-systems.
typedef struct HNV_SYS_CTRL_PARAM_T
{
	U32 tlm_baudrate;					//	Baud rate for telemetry usart
	U32 mdm_baudrate;					//	Baud rate for iridium modem
} hnv_sys_ctrl_param_t;

// System initialization info. This structure is returned by 'hnv_sys_ctrl_init()' to report which sub-systems
// succeded/failed to initialized. TRUE = Sub-system initialized OK, FALSE = Sub-system failed to initialize.
typedef struct HNV_SYS_CTRL_STT_T
{
	Bool cpu;		// CPU clock initialization result
	Bool pwr;		// Power sub-system initialization result
	Bool time;		// Time sub-system initialization result
	Bool sysmon;	// System monitor initialization result
	Bool tlm;		// Telemetry port initialization result
	Bool mdm;		// Modem port initialization result
	Bool file;		// File system initialization result

} hnv_sys_ctrl_status_t;



//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Initializes the PCB on power up or reset to a safe, predictable state
//! This function should be called very early.
void hnv_sys_ctrl_board_init(void);

//! \brief Reset spectrometer board
void hnv_sys_spec_board_reset(void);

//! \brief Start spectrometer board
//! Must be called because initially, controller board keeps spectrometer board in reset state.
void hnv_sys_spec_board_start(void);

//! \brief Stop spectrometer board
//! Call when resetting self or going to bootloader.
void hnv_sys_spec_board_stop(void);

//! \brief Initialize HyperNav system.
//! This function initializes the several modules that compose the HyperNav system.
//! It **must** be called before attempting to use any other API function.
//!	@param	initParameter	Input:	Contains arguments to sub-systems.
//! @param	initStatus		Output:	Initialization status. Find here information about which modules did not initialize.
//! @return HNV_SYS_CTRL_OK:Success , HNV_SYS_CTRL_FAIL: Failure, check 'nsyserrno'
S16 hnv_sys_ctrl_init(hnv_sys_ctrl_param_t* initParameter, hnv_sys_ctrl_status_t* initStatus );

//! \brief Initialize USB.
//! This function creates USB tasks for the mass storage interface.
//! @return HNV_SYS_CTRL_OK:Success , HNV_SYS_CTRL_FAIL: Failure, check 'nsyserrno'
S16 hnv_sys_ctrl_startUSB();


//! \brief De-initialize HyperNav system.
//! This function De-initializes the several modules that compose the HyperNav system.
//! It MUST be called before removing power from the main MCU.
//!	@param	keepTLM		INPUT: if true, telemetry interface (RS-232 or USB) is not de-initialized.  Normally only used with "USB sleep"
//! @return HNV_SYS_CTRL_OK:Success , HNV_SYS_CTRL_FAIL: Failure, check 'nsyserrno'
//S16 hnv_sys_ctrl_deinit(Bool keepTLM);
S16 hnv_sys_ctrl_deinit(void);


//! \brief Check last reset cause
//! @return 'HNV_SYS_CTRL_WDT_RST' 		- Last reset caused by watchdog timer expiration
//! @return 'HNV_SYS_CTRL_PCYCLE_RST' 	- Last reset caused by power cycling
//! @return 'HNV_SYS_CTRL_EXT_RST' 		- Last reset caused by asserting the external RESET line
//! @return 'HNV_SYS_CTRL_BOD_RST' 		- Last reset caused by internal brown-out detection circuit on 3.3V line
//! @return 'HNV_SYS_CTRL_CPUERR_RST' 	- Last reset caused by CPU error
//! @return	'HNV_SYS_CTRL_UNKNOWN_RST'	- Last reset caused by unknown causes
U8	hnv_sys_ctrl_resetCause(void);


//! \brief Query HyperNav system driver version
//! @param major	OUTPUT: Major revision number
//! @param minor	OUTPUT: Minor revision number
//! @param patch	OUTPUT: Patch revision number
//! @param variant	OUTPUT: Variant (customization) number
//! @param var_desc	OUTPUT: Variant (customization) description
void hnv_sys_ctrl_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[]);

//  Temporary here, so can be called from command.controller.c
void initSPI0(U32 fPBA);
void initSPI1(U32 fPBA);

#endif /* _HYPERNAV_SYS_CONTROLLER_H_ */
