/*! \file ds3234.h ****************************************************************
 *
 * \brief DS3234 Real Time Clock Device Driver API
 *
 * This module implements an API for the DS3234 RTC
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-07-28
 *
 *
 * @todo Extend alarm support for different alarm rates and alarm 2.
 *
 **********************************************************************************/
#ifndef DS3234_RTC_H_
#define DS3234_RTC_H_

#include <compiler.h>

//! Returned constants==============================================================
#define DS3234_OK          0
#define DS3234_FAIL        -1

//! Exported functions =============================================================


//! Initialises the RTC module. This function must be called before any other in the module.
//! @param fPBA PBA frequency
//! @return     0:Success -1: Fail
S8 ds3234_init(U32 fPBA);


//! Sets the RTC time and date given a UNIX timestamp (seconds from the epoch)
//! @param time  Seconds from the epoch (January 1, 1970 00:00:00 UTC/GMT)
//! @return     0:Success -1: Fail
S8 ds3234_setTime(U32 time);


//! Gets the RTC time and date given a UNIX timestamp (seconds from the epoch)
//! @param time  OUTPUT: Seconds from the epoch (January 1, 1970 00:00:00 UTC/GMT)
//! @return     0:Success -1: Fail
S8 ds3234_getTime(U32 *time);


//! Sets the RTC time and date given a human understandable format
//! @param year         Year
//! @param month        Month, 1-12
//! @param date         Date, 01-31 (has to be consistent with month)
//! @param hour         Hour, 00-23
//! @param minutes      Minutes, 00-59
//! @param seconds      Seconds, 00-59
//! @return     0:Success -1: Fail
S8 ds3234_setTime2(U16 year, U8 month, U8 date, U8 hour, U8 minutes, U8 seconds);



//! Gets the RTC time and date in a human understandable format
//! @param year         OUTPUT: Year, 1900-2099
//! @param month        OUTPUT: Month, 1-12
//! @param date         OUTPUT: Date, 01-31 (has to be consistent with month)
//! @param day          OUTPUT: Day of week, 0:Sunday
//! @param hour         OUTPUT: Hour, 00-23
//! @param minutes      OUTPUT: Minutes, 00-59
//! @param seconds      OUTPUT: Seconds, 00-59
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_getTime2(U16 *year, U8 *month, U8 *date, U8 *day, U8 *hour, U8 *minutes, U8 *seconds);


//! Sets an alarm at the specified time.
//! @param date         Date, 01-31 (has to be consistent with month)
//! @param hour         Hour, 00-23
//! @param minutes      Minutes, 00-59
//! @param seconds      Seconds, 00-59
//! @return     0:Success -1: Fail
//! @note The alarm must be enabled by calling ds3234_enableAlarm() to assert the INTn pin at the alarm time.
S8 ds3234_setAlarm(U8 date, U8 hour, U8 minutes, U8 seconds);


//! Enables the alarm. The INTn pin will be asserted when the clock time equals the alarm setting.
//! @return     0:Success -1: Fail
S8 ds3234_enableAlarm(void);


//! Disables the alarm. The INTn pin will not be asserted when the clock time equals the alarm setting.
//! @return     0:Success -1: Fail
S8 ds3234_disableAlarm(void);


//! Acknolwleges the alarm. If INTn pin is asserted due to an alarm condition, calling to this function will
//! cause the RTC to de-assert INTn.
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_ackAlarm(void);

//! Gets the temperature in degrees Celsius
//! @param temperature  OUTPUT: Temperature in degrees Celsius
//! @return     0:Success -1: Fail
S8 ds3234_getTemperature(F32 *temperature);


//! Write a byte in the internal SRAM
//! @param address      Address 0-255
//! @param data         Data to store in SRAM
//! @return     0:Success -1: Fail
S8 ds3234_writeSRAMData(U8 address, U8 data);


//! Read a byte from the internal SRAM
//! @param address      Address 0-255
//! @param data         OUTPUT: Data to read from SRAM
//! @return     0:Success -1: Fail
S8 ds3234_readSRAMData(U8 address, U8 *data);



#endif /* DS3234_RTC_H_ */
