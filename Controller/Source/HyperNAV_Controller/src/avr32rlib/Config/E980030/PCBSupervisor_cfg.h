/*! \file PCBSupervisor_cfg.h**********************************************************
 *
 * \brief PCB Supervisor microcontroller Driver configuration file.
 *
 *
 * @author      Scott Feener, Satlantic Inc.
 * @date        2011-11-09
 *
  ***************************************************************************/
#ifndef PCBSUPERVISOR_CFG_H_
#define PCBSUPERVISOR_CFG_H_

#include <board.h>
#include "compiler.h"

#ifndef PCB_SUPERVISOR_SUPPORTED
#error "PCB_SUPERVISOR_SUPPORTED not defined"
#endif

// SPI Master
#define	SUPERVISOR_SPI		(&AVR32_SPI0)
// SPI chipselect#
#define	SUPERVISOR_NPCS	7
// Although the supervisor is assigned to CS7, CS7 is not actually used
// due to it being selected by default through the use of a 3:8 decoder,
// which causes board issues - a separate control line is required
#define SUPERVISOR_CSn		AVR32_PIN_PB02		// "SPAREGPIO" on schematic
//AVR32_PIN_PX49
//#warning "revert above when done testing"

//*****************************************************************************
//	User Register addresses
//*****************************************************************************
#define SPV_RESET_SOURCE_REG	0		//!< supervisor reset source register address
#define SPV_WAKEUP_SOURCE_REG	1		//!< supervisor wakeup source
#define SPV_FIRMWARE_VERSION	2		//!< supervisor firmware version (1-255)
#define SPV_CHECK_REG			3		//!<

#define SPV_CHECK_REG_CKVAL		0xAA
#define SPV_CHECK_REG_RSTVAL	0xFF

#define SPV_USER_REGISTER_SIZE	19		//!< size of user register;

//*****************************************************************************
//	Supervisor reset sources
//		The contents of the SPV_RESET_SOURCE_REG will define the
//		PCB Supervisor reset source
//*****************************************************************************
#define SPV_POWERON_RESET		0		//!< Power-on reset source
#define SPV_EXTERNAL_RESET		1		//!< External reset source
#define SPV_WATCHDOG_RESET		2		//!< Watchdog reset source
#define SPV_BROWNOUT_RESET		3		//!< Brownout reset source
#define SPV_UNKNOWN_RESET		100		//!< Unknown reset source
#define SPV_NO_RESET			255		//!< Default regiater value

//*****************************************************************************
//	Supervisor wakeup sources
//		The contents of the SPV_WAKEUP_SOURCE_REG will define the
//		PCB Supervisor wake from Deep Sleep source
//*****************************************************************************
#define SPV_WOKE_FROM_RTC			0		//!< Supervisor woke from real time clock alarm
#define SPV_WOKE_FROM_RS232			1		//!< Supervisor woke from RS-232 telemetry activity
#define SPV_WOKE_FROM_OC			2		//!< Supervisor woke from open-collector telemetry activity
#define SPV_WOKE_FROM_USB_INSERTION	3		//!< Supervisor woke from USB power detection
#define SPV_WOKE_FROM_USB_REMOVAL 	4		//!< Supervisor woke from USB power removal; should not happen
#define SPV_WOKE_FROM_UNKNOWN 		100		//!< Supervisor woke from unknown source
#define SPV_NO_WAKE_CONDITION		255		//!< Supervisor no-wake condition/default register value

#endif /* PCBSUPERVISOR_CFG_H_ */
