/*
 * PCBSupervisor.h
 *
 *  Created on: 2011-11-09
 *      Author: Scott
 */

#ifndef PCBSUPERVISOR_H_
#define PCBSUPERVISOR_H_

#include "compiler.h"


// Commands from master
#define CMD_START			(U8)100
#define CMD_DISABLE			(U8)101
#define CMD_READ_REG		(U8)102
#define CMD_WRITE_REG		(U8)103
#define CMD_DEEP_SLEEP		(U8)104

// Commands from slave
#define SLAVE_RESPONSE			(U8)83		//!< 'S' for slave
#define SLAVE_WAIT_START_STATE	(U8)253
#define SLAVE_STANDBY_STATE		(U8)254
//#define SLAVE_CMD_INVALID		(U8)255


// User register defines
#define USER_REGISTER_SIZE		19	//16 user bytes + 3 status (byte 0 is reset source, byte 1 is wake source, byte 2 is firmware rev
// read only register addresses
#define RESET_REGISTER			(U8)0
#define WAKEUP_REGISTER			(U8)1
#define FIRMWARE_REV_REGISTER	(U8)2

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define PCBSUPERVISOR_OK		0
#define PCBSUPERVISOR_FAIL 		-1

#define SPV_INVALIDREAD				-100
#define SPV_INVALIDSLAVERESPONSE	-101
#define SPV_NOT_STANDBY_STATE		-102
#define SPV_BAD_PADDING				-103
#define SPV_BAD_CHECKSUM			-104
#define SPV_WRONG_REGISTER			-105
#define SPV_NOT_WAIT_START_STATE	-106




//*****************************************************************************
// Exported functions
//*****************************************************************************



//! Retrieves a specified User Register byte value
//! @param	reg_addr	INPUT: the register to read
//! @param 	reg_value  	OUTPUT: the retrieved value
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8	supervisorReadUserRegister(U8 reg_addr, U8 *reg_value, S8 *errorval);

//! Sets a specified User Register byte value
//! @param	reg_addr	INPUT: the register to write
//! @param 	reg_value  	INPUT: the value to write
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8	supervisorWriteUserRegister(U8 reg_addr, U8 reg_value, S8 *errorval);

//! Removes power from PCB.  Supervisor waits for power cycle.
//! @return	PCBSUPERVISOR_FAIL: Error, does not return on success (power will be off)
S8 supervisorDisable(S8 *errorval);

//! Removes power from PCB.  Supervisor will reapply power on telemetry activity or RTC alarm
//! @return	PCBSUPERVISOR_FAIL: Error, does not return on success (power will be off)
S8 supervisorDeepSleep(S8 *errorval);

//! Initialize driver and send start command to supervisor
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8 supervisorInitialize(S8 *errorval);


//S16 supervisor_testread (void);



#endif /* PCBSUPERVISOR_H_ */
