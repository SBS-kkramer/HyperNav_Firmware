/*! \file hypernav.sys.spectrometer.h *****************************************
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

#define FALSE     0
#define TRUE      1
#define SYS_OK			0
#define SYS_FAIL		-1


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define HNV_SYS_SPEC_OK		0
#define HNV_SYS_SPEC_FAIL	-1


// Reset causes
enum {
	HNV_SYS_SPEC_UNKNOWN_RST=0,
	HNV_SYS_SPEC_WDT_RST,
	HNV_SYS_SPEC_PCYCLE_RST,
	HNV_SYS_SPEC_EXT_RST,
	HNV_SYS_SPEC_BOD_RST,
	HNV_SYS_SPEC_CPUERR_RST
};

//*****************************************************************************
// Exported data types
//*****************************************************************************

// System initialization parameters. This structure is used by 'hnv_sys_spec_init()' to
// obtain parameters to be sent to sub-systems.
typedef struct HNV_SYS_SPEC_PARAM_T
{
	U32 baudrate;					//	Baud rate for USART
} hnv_sys_spec_param_t;

// System initialization info. This structure is returned by 'hnv_sys_spec_init()' to report which sub-systems
// succeeded/failed to initialized. TRUE = Sub-system initialized OK, FALSE = Sub-system failed to initialize.
typedef struct HNV_SYS_SPEC_STT_T
{
	Bool cpu;		// CPU clock initialization result
	Bool time;		// Time sub-system initialization result
	Bool sysmon;	// System monitor initialization result
	Bool tlm;		// Telemetry port initialization result
	Bool spec;		// Spectrometer initialization result

} hnv_sys_spec_status_t;



//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief Initialize HyperNav system.
//! This function initializes the several modules that compose the HyperNav system. It
//! MUST be called before attempting to use any other API function.
//!	@param	initParameter	Input:	Contains arguments to sub-systems.
//! @param	initStatus		Output:	Initialization status. Find here information about which modules did not initialize.
//! @return HNV_SYS_SPEC_OK:Success , HNV_SYS_SPEC_FAIL: Failure, check 'nsyserrno'
S16 hnv_sys_spec_init(hnv_sys_spec_param_t* initParameter, hnv_sys_spec_status_t* initStatus );

//! \brief Initialize USB.
//! This function creates USB tasks for the mass storage interface.
//! @return HNV_SYS_SPEC_OK:Success , HNV_SYS_SPEC_FAIL: Failure, check 'nsyserrno'
S16 hnv_sys_spec_startUSB(void);


//! \brief De-initialize HyperNav system.
//! This function De-initializes the several modules that compose the HyperNav system.
//! It MUST be called before removing power from the main MCU.
//!	@param	keepTLM		INPUT: if true, telemetry interface (RS-232 or USB) is not de-initialized.  Normally only used with "USB sleep"
//! @return HNV_SYS_SPEC_OK:Success , HNV_SYS_SPEC_FAIL: Failure, check 'nsyserrno'
//S16 hnv_sys_spec_deinit(Bool keepTLM);
S16 hnv_sys_spec_deinit(void);


//! \brief Check last reset cause
//! @return 'HNV_SYS_SPEC_WDT_RST' 		- Last reset caused by watchdog timer expiration
//! @return 'HNV_SYS_SPEC_PCYCLE_RST' 	- Last reset caused by power cycling
//! @return 'HNV_SYS_SPEC_EXT_RST' 		- Last reset caused by asserting the external RESET line
//! @return 'HNV_SYS_SPEC_BOD_RST' 		- Last reset caused by internal brown-out detection circuit on 3.3V line
//! @return 'HNV_SYS_SPEC_CPUERR_RST' 	- Last reset caused by CPU error
//! @return 'HNV_SYS_SPEC_UNKNOWN_RST'	- Last reset caused by unknown causes
U8	hnv_sys_spec_resetCause(void);


//! \brief Query HyperNav system driver version
//! @param major	OUTPUT: Major revision number
//! @param minor	OUTPUT: Minor revision number
//! @param patch	OUTPUT: Patch revision number
//! @param variant	OUTPUT: Variant (customization) number
//! @param var_desc	OUTPUT: Variant (customization) description
void hnv_sys_spec_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[]);


//! \brief Initialize SPI0 (for interboard coms)
//! @param fPBA	INPUT: Peripheral Bus A (PBA) frequency in Hz
//void initSPI0(U32 fPBA);

//! \brief Initialize SPI1 (for interboard coms)
//! @param fPBA	INPUT: Peripheral Bus A (PBA) frequency in Hz
//void initSPI1(U32 fPBA);

//! \brief SPI interrupt service routine (ISR)
//__attribute__((__interrupt__)) static void SPI_recv(void);


//! \brief Initialize system time
//! @return SYS_OK:Success , SYS_FAIL: Failure, check 'avr32rerrno'
S16	SYS_InitTime(void);


//! \brief  Reboot system
void sys_reboot(void);


#endif /* _HYPERNAV_SYS_CONTROLLER_H_ */
