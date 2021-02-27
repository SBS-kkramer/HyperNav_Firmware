/*
 * PCBSupervisor.c
 *
 *  Created on: 2011-11-09
 *      Author: Scott
 */


#include "compiler.h"
#include "dbg.h"
#include "spi.h"
#include "PCBSupervisor.h"
#include "PCBSupervisor_cfg.h"
#include "FreeRTOS.h"
#include "task.h"
#include <gpio.h>

#define CS_ASSERT_DELAY		100		// ms between selecting supervisor and writing on SPI interface
#define CS_DEASSERT_DELAY	1		// ms between finishing SPI comms and deasserting cs line
#define DELAY_BETWEEN_BYTES	1		// ms delay between sending bytes to PCB supervisor
#define SPV_ACTIVITY_DELAY	10

#define ALLOW_ABORT			1

//! Retrieves a specified User Register byte value
//! @param	reg_addr	INPUT: the register to read
//! @param 	reg_value  	OUTPUT: the retrieved value
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8	supervisorReadUserRegister(U8 reg_addr, U8 *reg_value, S8 *errorval)
{
	U16 dataIO;
	S8 ret = PCBSUPERVISOR_OK;

	*errorval = PCBSUPERVISOR_OK;

	// check for valid register
	if (reg_addr >= USER_REGISTER_SIZE) {
		*errorval = SPV_INVALIDREAD;
		return PCBSUPERVISOR_FAIL;
	}

	// select PCB Supervisor
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);

	// delay for supervisor to wake up
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_ASSERT_DELAY ) );


	do {
		// send command packet..
		spi_write(SUPERVISOR_SPI, CMD_READ_REG);	// first byte - command byte
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_RESPONSE) {				// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_INVALIDSLAVERESPONSE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, reg_addr);		// second byte - register address
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_STANDBY_STATE) {		// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_NOT_STANDBY_STATE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// third byte - padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != 0) {							// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_PADDING;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, (U8)(0-(CMD_READ_REG+reg_addr+0)));		// 4th byte - check sum
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != (U8)(0-(SLAVE_RESPONSE+SLAVE_STANDBY_STATE+0))) {
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_CHECKSUM;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		// now send 4 more bytes to retrieve the register value
		spi_write(SUPERVISOR_SPI, 0);				// padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != SLAVE_RESPONSE) {				// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_INVALIDSLAVERESPONSE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != reg_addr) {					// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_WRONG_REGISTER;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// padding - get the register value
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		*reg_value = (U8)dataIO;
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != (U8)(0-(SLAVE_RESPONSE+reg_addr+*reg_value))) {
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_CHECKSUM;
#if ALLOW_ABORT
			break;
#endif
		}

	} while (0);

	// short delay to let operations complete
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_DEASSERT_DELAY ) );

	// deselect supervisor
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);


	return ret;
}

//! Sets a specified User Register byte value
//! @param	reg_addr	INPUT: the register to write
//! @param 	reg_value  	INPUT: the value to write
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8	supervisorWriteUserRegister(U8 reg_addr, U8 reg_value, S8 *errorval)
{
	U16 dataIO;
	S8 ret = PCBSUPERVISOR_OK;

	*errorval = PCBSUPERVISOR_OK;

	// check for valid writeable register
	if ((reg_addr == RESET_REGISTER) ||
		(reg_addr == WAKEUP_REGISTER) ||
		(reg_addr == FIRMWARE_REV_REGISTER) ||
		(reg_addr >= USER_REGISTER_SIZE)) {

			*errorval = SPV_WRONG_REGISTER;
			return PCBSUPERVISOR_FAIL;
	}

	// select PCB Supervisor
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);


	// delay for supervisor to wake up
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_ASSERT_DELAY ) );

	do {
		// send command packet..
		spi_write(SUPERVISOR_SPI, CMD_WRITE_REG);	// first byte - command byte
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_RESPONSE) {				// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_INVALIDSLAVERESPONSE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, reg_addr);		// second byte - register address
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_STANDBY_STATE) {		// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_NOT_STANDBY_STATE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, reg_value);		// third byte - the value
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != 0) {							// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_PADDING;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, (U8)(0-(CMD_WRITE_REG+reg_addr+reg_value)));		// 4th byte - check sum
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != (U8)(0-(SLAVE_RESPONSE+SLAVE_STANDBY_STATE+0))) {
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_CHECKSUM;
#if ALLOW_ABORT
			break;
#endif
		}
	} while (0);

	// short delay to let operations complete
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_DEASSERT_DELAY ) );

	// deselect supervisor
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);

	return ret;
}

static S8 supervisorPowerOff(U8 command, S8 *errorval)
{
	U16 dataIO;
	S8 ret = PCBSUPERVISOR_OK;

	// select PCB Supervisor
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);


	// delay for supervisor to wake up
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_ASSERT_DELAY ) );

	do {
		// send command packet..
		spi_write(SUPERVISOR_SPI, command);			// first byte - command byte
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_RESPONSE) {				// validate byte
			ret = PCBSUPERVISOR_FAIL;
		*errorval = SPV_INVALIDSLAVERESPONSE;
#if ALLOW_ABORT
		break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// second byte - padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != SLAVE_STANDBY_STATE) {		// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_NOT_STANDBY_STATE;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// third byte - padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if (dataIO != 0) {							// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_PADDING;
#if ALLOW_ABORT
			break;
#endif
		}
        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, (U8)(0-(command+0+0)));		// 4th byte - check sum
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
		if ((U8)dataIO != (U8)(0-(SLAVE_RESPONSE+SLAVE_STANDBY_STATE+0))) {
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_CHECKSUM;
#if ALLOW_ABORT
			break;
#endif
		}
	} while (0);

	// short delay to let operations complete
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_DEASSERT_DELAY ) );

	// deselect supervisor
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);


	//if (ret == PCBSUPERVISOR_OK)
	//	while (1);		// wait here for power cycle; want to remain in "high power" state to drain caps faster

	return ret;
}

//! Removes power from PCB.  Supervisor waits for power cycle.
//! @return	PCBSUPERVISOR_FAIL: Error, does not return on success (power will be off)
S8 supervisorDisable(S8 *errorval)
{
	return supervisorPowerOff(CMD_DISABLE, errorval);
}

//! Removes power from PCB.  Supervisor will reapply power on telemetry activity or RTC alarm
//! @return	PCBSUPERVISOR_FAIL: Error, does not return on success (power will be off)
S8 supervisorDeepSleep(S8 *errorval)
{
	return supervisorPowerOff(CMD_DEEP_SLEEP, errorval);
}

/*
S16 supervisor_testread (void)
{
	U16 dataIO;

	gpio_set_gpio_pin(SUPERVISOR_CSn);	// ensure supervisor is deselected
	// give supervisor time to reset if necessary
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

	//test
	spi_selectChip(&AVR32_SPI0, 2);
	spi_write(&AVR32_SPI0, 0);		// first byte - command byte
	spi_read(&AVR32_SPI0, &dataIO);			// Read byte
	spi_unselectChip(&AVR32_SPI0, 2);
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

	// select PCB Supervisor
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);

        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

	spi_write(SUPERVISOR_SPI, 128);		// first byte - command byte
	spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte

	// short delay to let operations complete
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_DEASSERT_DELAY ) );
	// deselect supervisor
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);


	return dataIO;
}
*/


//! Initialize driver and send start command to supervisor
//! @return PCBSUPERVISOR_OK: Success, PCBSUPERVISOR_FAIL: Error
S8 supervisorInitialize(S8 *errorval)
{
	//U16 dataIO;
	S8 ret = PCBSUPERVISOR_OK;

	*errorval = PCBSUPERVISOR_OK;

#if 0
	gpio_set_gpio_pin(SUPERVISOR_CSn);	// ensure supervisor is deselected
	// give supervisor time to reset if necessary
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

/*	Not required - PCB supervisor now waits for SCK=0 before enabling SPI, but requires slow-ish SCK
	// dummy byte to get SPI SCK in proper mode 0 default state
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	gpio_set_gpio_pin(SUPERVISOR_CSn);	// ensure supervisor is deselected
	spi_write(SUPERVISOR_SPI, 0);		// command byte
	spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	// give supervisor time to reset if necessary
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );
*/
	// give required 200ms "break" signal
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	//gpio_clr_gpio_pin(SUPERVISOR_CSn);
        vTaskDelay( (portTickType)TASK_DELAY_MS( 210 ) );
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	//gpio_set_gpio_pin(SUPERVISOR_CSn);

	// give supervisor time to reset if necessary
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

	// select PCB Supervisor
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	//gpio_clr_gpio_pin(SUPERVISOR_CSn);


	// delay for supervisor to wake up
        // vTaskDelay( (portTickType)TASK_DELAY_MS( CS_ASSERT_DELAY ) );

	// give supervisor time to reset if necessary
        vTaskDelay( (portTickType)TASK_DELAY_MS( 10 ) );

	do {
		// send command packet..
		spi_write(SUPERVISOR_SPI, CMD_START);		// first byte - command byte
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte

		if (dataIO != SLAVE_RESPONSE) {				// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_INVALIDSLAVERESPONSE;
#if ALLOW_ABORT
			break;
#endif
		}

        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// second byte - padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte

		if (dataIO != SLAVE_WAIT_START_STATE) {		// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_NOT_WAIT_START_STATE;
#if ALLOW_ABORT
			break;
#endif
		}

        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, 0);				// third byte - padding
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte

		if (dataIO != 0) {							// validate byte
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_PADDING;
#if ALLOW_ABORT
			break;
#endif
		}

        	vTaskDelay( (portTickType)TASK_DELAY_MS( DELAY_BETWEEN_BYTES ) );

		spi_write(SUPERVISOR_SPI, (U8)(0-(CMD_START+0+0)));		// 4th byte - check sum
		spi_read(SUPERVISOR_SPI, &dataIO);			// Read byte

		if ((U8)dataIO != (U8)(0-(SLAVE_RESPONSE+SLAVE_WAIT_START_STATE+0))) {
			ret = PCBSUPERVISOR_FAIL;
			*errorval = SPV_BAD_CHECKSUM;
#if ALLOW_ABORT
			break;
#endif
		}

	} while (0);

	// short delay to let operations complete
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_DEASSERT_DELAY ) );

	// deselect supervisor
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
	//gpio_set_gpio_pin(SUPERVISOR_CSn);

#else
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
        vTaskDelay( (portTickType)TASK_DELAY_MS( SPV_ACTIVITY_DELAY ) );
	spi_selectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
        vTaskDelay( (portTickType)TASK_DELAY_MS( CS_ASSERT_DELAY ) );
	spi_unselectChip(SUPERVISOR_SPI, SUPERVISOR_NPCS);
        vTaskDelay( (portTickType)TASK_DELAY_MS( SPV_ACTIVITY_DELAY ) ); // give supervisor time to reset if necessary

#endif

	return ret;
}
