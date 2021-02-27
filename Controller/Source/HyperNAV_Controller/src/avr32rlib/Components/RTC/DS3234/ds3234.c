/*! \file ds3234.c ****************************************************************
 *
 * \brief DS3234 Real Time Clock Device Driver
 *
 * This module implements an API for the DS3234 RTC
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-07-28
 *
 *
 *
 **********************************************************************************/

#include <time.h>
#include "gpio.h"
#include "spi.h"
#include "dbg.h"

#include "ds3234.h"
#include "ds3234_cfg.h"


#ifdef NO_DS3234
#warning "No DS3234 RTC support!"
#endif

//! Address Map =====================================================================

#define WRITE_OFFSET    0x80            //! R/W offset

// Current time
#define DS3234_SECONDS  0x00            //!< Seconds (BCD encoded) 00-59
#define DS3234_MINUTES  0x01            //!< Minutes (BCD encoded) 00-59
#define DS3234_HOUR     0x02            //!< Hours (BCD encoded) 00-23 or 1-12 + AMn/PM
#define DS3234_DAY      0x03            //!< Day (1-7) 1=Sunday
#define DS3234_DATE     0x04            //!< Date (BCD encoded) 01-31
#define DS3234_MONTH    0x05            //!< Month (BCD encoded) 01-12 + Century
#define DS3234_YEAR     0x06            //!< Year (BCD encoded) 00-99

// Alarms
#define DS3234_A1_SECONDS  0x07         //!< Alarm 1 Seconds (BCD encoded) 00-59
#define DS3234_A1_MINUTES  0x08         //!< Alarm 1 Minutes (BCD encoded) 00-59
#define DS3234_A1_HOUR     0x09         //!< Alarm 1 Hours (BCD encoded) 00-23 or 1-12 + AMn/PM
#define DS3234_A1_DAY      0x0A         //!< Alarm 1 Day (1-7) 1=Sunday
#define DS3234_A2_MINUTES  0x08         //!< Alarm 2 Minutes (BCD encoded) 00-59
#define DS3234_A2_HOURS    0x09         //!< Alarm 2  Hours (BCD encoded) 00-23 or 1-12 + AMn/PM
#define DS3234_A2_DAY      0x0A         //!< Alarm 2 Day (1-7) 1=Sunday

// Control register
#define DS3234_CONTROL  0x0E            //!< Control register
#define EOSCn           0x80            //!< Enable oscillator (active-low)
#define BBSQW           0x40            //!< Battery backed square-wave enable
#define CONV            0x20            //!< Convert temperature
#define RS2             0x10            //!< Rate select 2
#define RS1             0x08            //!< Rate select 1
#define INTCN           0x04            //!< Interrupt control
#define A2IE            0x02            //!< Alarm 2 interrupt enable
#define A1IE            0x01            //!< Alarm 1 interrupt enable

// Control/Status
#define DS3234_CSTATUS  0x0F            //!< Control/status register
#define OSF             0x80            //!< Oscillator stop flag
#define BB32KHZ         0x40            //!< Battery backed 32kHz output
#define CRATE1          0x20            //!< Conversion rate 1
#define CRATE0          0x10            //!< Conversion rate 0
#define EN32KHZ         0x08            //!< Enable 32kHz output
#define BSY             0x04            //!< Busy flag
#define A2F             0x02            //!< Alarm 2 flag
#define A1F             0x01            //!< Alarm 1 flag

// Temperature
#define TEMPMSB         0x11            //!< Temperature MSB
#define TEMPLSB         0x12            //!< Temperature MSB
#define TEMPSIGN        0x80            //!< Temperature "sign"

// SRAM
#define SRAMADDR        0x18            //!< SRAM Address
#define SRAMDATA        0x19            //!< SRAM Data

//! Bit masks for register 0x02/0x82 (Hours) 0x09/0x89 (Alarm 1 Hours) 0x0C/0x8C (Alarm 2 Hours)
#define HOUR1224n       0x40            //!< 12/24 hours format flag
#define PM              0x20            //!< AM/PM indicator

//! Bit masks for register 0x05/0x85 (Month)
#define CENTURY         0x80            //!< Century flag

//! Bit masks for register 0x0A/0x8A (Alarm 1 Day/Date) 0x0D/0x8D (Alarm 2 Day/Date)
#define ALARM_DAY       0x0F            //!< Alarm day BCD 01-07 1=Sunday
#define ALARM_DATE      0x3F            //!< Alarm date BCD 01-31
#define DAYDATEn        0x40            //!< Alarm on day or date indicator
#define ALARM_MASK      0x80            //!< Alarm mask (to set the alarm rate)



//! Module private functions ========================================================
static S8 readRTCByte(U8 address, U8 *data);
static S8 writeRTCByte(U8 address, U8 data);
static U8 dec2BCD(U8 number);
static U8 BCD2dec(U8 BCDnumber);


//! Exported functions =============================================================


//! Initializes the RTC module. This function must be called before any other in the module.
//! @return     RTC_OK:Success RTC_FAIL:Fail
S8 ds3234_init(U32 fPBA)
{

#ifdef NO_DS3234

	DEBUG("WARNING - No external RTC on board");
	return DS3234_OK;

#else
		// Disable 32KHz output (to reduce power consumption)
		//readRTCByte(DS3234_CSTATUS, &CStatusReg);
		//Clr_bits(CStatusReg,EN32KHZ);
		// writeRTCByte(DS3234_CSTATUS, CStatusReg);

		// Power RTC from external source
#ifdef DS3234_ONn_PIN
		gpio_clr_gpio_pin(DS3234_ONn_PIN);
#endif

		return DS3234_OK;

#endif	// NO_DS3234
}


//! Sets the RTC time and date given a UNIX timestamp (seconds from the epoch)
//! @param time  Seconds from the epoch (January 1, 1970 00:00 UTC/GMT)
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_setTime(U32 time)
{
  U16 year;
  U8 month, date, hour, minutes, seconds;
  struct tm *bdTime;    // Broken-down time

  // Convert UNIX timestamp to broken-down time and convert to BCD
  bdTime = localtime((const time_t*) &time);
  seconds = dec2BCD(bdTime->tm_sec);
  minutes = dec2BCD(bdTime->tm_min);
  hour = dec2BCD(bdTime->tm_hour);
  date = dec2BCD(bdTime->tm_mday);
  month = dec2BCD(bdTime->tm_mon + 1);
  year = bdTime->tm_year + 1900;
  // Specify century
  if(year <= 1999)
  {
    Clr_bits(month,CENTURY);
    // Offset and convert year to BCD
    year = dec2BCD(year-1900);
  }
  else
  {
    Set_bits(month,CENTURY);
    // Offset and convert year to BCD
    year = dec2BCD(year-2000);
  }

  // Send to RTC
  writeRTCByte(DS3234_SECONDS, seconds);
  writeRTCByte(DS3234_MINUTES, minutes);
  writeRTCByte(DS3234_HOUR, hour);
  writeRTCByte(DS3234_DAY, bdTime->tm_wday + 1);
  writeRTCByte(DS3234_DATE, date);
  writeRTCByte(DS3234_MONTH, month);
  writeRTCByte(DS3234_YEAR, year);

  return DS3234_OK;
}


//! Sets the RTC time and date in a human understandable format
//! @param year         Year
//! @param month        Month, 1-12
//! @param date         Date, 01-31 (has to be consistent with month)
//! @param hour         Hour, 00-23
//! @param minutes      Minutes, 00-59
//! @param seconds      Seconds, 00-59
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_setTime2(U16 year, U8 month, U8 date, U8 hour, U8 minutes, U8 seconds)
{
  struct tm bdTime;

  bdTime.tm_year = year-1900;
  bdTime.tm_mon = month-1;
  bdTime.tm_mday = date;
  bdTime.tm_hour = hour;
  bdTime.tm_min = minutes;
  bdTime.tm_sec = seconds;

  // Normalize (and figure out the day of the week)
  if(mktime(&bdTime)== -1)
    return DS3234_FAIL;

  // Convert to BCD format
  seconds = dec2BCD(seconds);
  minutes = dec2BCD(minutes);
  hour = dec2BCD(hour);
  date = dec2BCD(date);
  month = dec2BCD(month);

  // Specify century, offset and convert year to BCD
  if(year <= 1999)
    {
    Clr_bits(month,CENTURY);
    year = dec2BCD(year-1900);
    }
  else
    {
    Set_bits(month,CENTURY);
    year = dec2BCD(year-2000);
    }

  // Send to RTC
  writeRTCByte(DS3234_SECONDS, seconds);
  writeRTCByte(DS3234_MINUTES, minutes);
  writeRTCByte(DS3234_HOUR, hour);
  writeRTCByte(DS3234_DAY, bdTime.tm_wday + 1);
  writeRTCByte(DS3234_DATE, date);
  writeRTCByte(DS3234_MONTH, month);
  writeRTCByte(DS3234_YEAR, year);

  return DS3234_OK;
}


//! Gets the RTC time and date as a UNIX timestamp (seconds from the epoch)
//! @param time  OUTPUT: Seconds from the epoch (January 1, 1970 00:00 UTC/GMT)
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_getTime(U32 *time)
{
  U16 year;
  U8 month, date, day, hour, minutes, seconds;
  struct tm bdTime;

  // Get time from RTC
  ds3234_getTime2(&year, &month, &date, &day, &hour, &minutes, &seconds);

  // Convert to UNIX time
  bdTime.tm_sec = seconds;
  bdTime.tm_min = minutes;
  bdTime.tm_hour = hour;
  bdTime.tm_mday = date;
  bdTime.tm_mon  = month-1;
  bdTime.tm_year = year - 1900;
  *time = mktime(&bdTime);

  if((time_t)*time == (time_t)-1)	//	Bad type cast: (*time) has incorrect type.
      return DS3234_FAIL;

  return DS3234_OK;
}


//! Gets the RTC time and date in a human understandable format
//! @param year         OUTPUT: Year, 1900-2099
//! @param month        OUTPUT: Month, 1-12
//! @param date         OUTPUT: Date, 01-31 (has to be consistent with month)
//! @param day          OUTPUT: Day of week, 0:Sunday
//! @param hour         OUTPUT: Hour, 00-23
//! @param minutes      OUTPUT: Minutes, 00-59
//! @param seconds      OUTPUT: Seconds, 00-59
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_getTime2(U16 *year, U8 *month, U8 *date, U8 *day, U8 *hour, U8 *minutes, U8 *seconds)
{
  Bool century;
  Bool format12Hr;
  Bool isPM;
  U8 year8b;  // 8-bit year

  // Read RTC
  readRTCByte(DS3234_SECONDS, seconds);
  readRTCByte(DS3234_MINUTES, minutes);
  readRTCByte(DS3234_HOUR, hour);
  readRTCByte(DS3234_DATE, date);
  readRTCByte(DS3234_DAY, day);
  readRTCByte(DS3234_MONTH, month);
  readRTCByte(DS3234_YEAR, &year8b);

  // Read flags
  century = Tst_bits(*month, CENTURY);
  format12Hr = Tst_bits(*hour,HOUR1224n);
  isPM = Tst_bits(*hour, PM);

  // Clear flags before conversion
  Clr_bits(*month, CENTURY);
  Clr_bits(*hour, HOUR1224n);
  if(format12Hr)Clr_bits(*hour, PM);

  // Convert BCD to decimal
  *seconds = BCD2dec(*seconds);
  *minutes = BCD2dec(*minutes);
  *hour = (format12Hr && isPM)? BCD2dec(*hour) + 12 : BCD2dec(*hour);
  *date = BCD2dec(*date);
  (*day)--;
  *month = BCD2dec(*month);
  *year = (century)? BCD2dec(year8b) + 2000:BCD2dec(year8b) + 1900;

  return DS3234_OK;
}


//! Sets an alarm at the specified time.
//! @param date         Date, 01-31 (has to be consistent with month)
//! @param hour         Hour, 00-23
//! @param minutes      Minutes, 00-59
//! @param seconds      Seconds, 00-59
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
//! @note The alarm must be enabled by calling ds3234_enableAlarm() to assert the INTn pin at the alarm time.
S8 ds3234_setAlarm(U8 date, U8 hour, U8 minutes, U8 seconds)
{
  // Convert to BCD
  seconds = dec2BCD(seconds);
  minutes = dec2BCD(minutes);
  hour = dec2BCD(hour);
  date = dec2BCD(date);

  // Write to RTC
  writeRTCByte(DS3234_A1_SECONDS, seconds);
  writeRTCByte(DS3234_A1_MINUTES, minutes);
  writeRTCByte(DS3234_A1_HOUR, hour);
  writeRTCByte(DS3234_A1_DAY, date);

   return DS3234_OK;
}


//! Enables the alarm. The INTn pin will be asserted when the clock time equals the alarm setting.
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_enableAlarm()
{
  U8 controlReg;

  // Set A1IE bit in control register
  readRTCByte(DS3234_CONTROL, &controlReg);
  Set_bits(controlReg, A1IE);
  writeRTCByte(DS3234_CONTROL, controlReg);

  return DS3234_OK;
}


//! Disables the alarm. The INTn pin will not be asserted when the clock time equals the alarm setting.
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_disableAlarm()
{
  U8 controlReg;

  // Clear A1IE bit in control register
  readRTCByte(DS3234_CONTROL, &controlReg);
  Clr_bits(controlReg, A1IE);
  writeRTCByte(DS3234_CONTROL, controlReg);

  return DS3234_OK;
}


//! Acknowledges the alarm. If INTn pin is asserted due to an alarm condition, calling to this function will
//! cause the RTC to de-assert INTn.
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_ackAlarm(void)
{
	// Control/Status register
	U8 CSRegister;

	// Clear A1F bit (Alarm 1 flag) in the control/status register
	readRTCByte(DS3234_CSTATUS, &CSRegister);
	Clr_bits(CSRegister, A1F);
	writeRTCByte(DS3234_CSTATUS, CSRegister);

	return DS3234_OK;
}


//! Gets the temperature in degrees Celsius
//! @param temperature  OUTPUT: Temperature in degrees Celsius
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_getTemperature(F32 *temperature)
{
  U8 tempMSB, tempLSB;
  Bool isNegative = FALSE;
  S16 integerTemp;

  readRTCByte(TEMPMSB, &tempMSB);
  readRTCByte(TEMPLSB, &tempLSB);

  // Account for 10-bit 2's complement format
  if(Tst_bits(tempMSB, TEMPSIGN))
    {
    isNegative = TRUE;
    Clr_bits(tempMSB, TEMPSIGN);
    }

  integerTemp = ((U16)tempMSB)<<2;
  integerTemp += tempLSB >> 6;

  if(isNegative)
    integerTemp -= 512;

  *temperature = integerTemp * 0.25;

  return DS3234_OK;

}


//! Write a byte in the internal SRAM
//! @param address      Address 0-255
//! @param data         Data to store in SRAM
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_writeSRAMData(U8 address, U8 data)
{
  // Point to destination address
  if(writeRTCByte(SRAMADDR, address) != DS3234_OK)
    return DS3234_FAIL;

  // Write in data register
  if(writeRTCByte(SRAMDATA, data) != DS3234_OK)
    return DS3234_FAIL;

  return DS3234_OK;
}


//! Read a byte from the internal SRAM
//! @param address      Address 0-255
//! @param data         OUTPUT: Data to read from SRAM
//! @return     DS3234_OK:Success DS3234_FAIL:Fail
S8 ds3234_readSRAMData(U8 address, U8 *data)
{
  // Point to destination address
   if(writeRTCByte(SRAMADDR, address) != DS3234_OK)
     return DS3234_FAIL;

   // Write in data register
   if(readRTCByte(SRAMDATA, data) != DS3234_OK)
     return DS3234_FAIL;

   return DS3234_OK;
}


//! Module private functions ========================================================
static S8 readRTCByte(U8 address, U8 *data)
{

#ifdef NO_DS3234

  *data = 0;
  return DS3234_OK;

#else

  U16 dataIO;

  // Assemble 16-bit stream
  dataIO = address;
  dataIO <<= 8;
  dataIO += *data;

#ifndef FREERTOS_USED
  // Disable interrupts to prevent an SPI preemption abort data transfer
  // (better way to do it would be using mutexes)
  Disable_global_interrupt();
#endif

  // Select chip
  spi_selectChip(DS3234_SPI, DS3234_SPI_NPCS);

  // Write-read
#if (DS3234_SPI_BITS == 16)
  spi_write(DS3234_SPI,dataIO);
  spi_read(DS3234_SPI, &dataIO);
  *data = 0x00FF & dataIO;
#elif (DS3234_SPI_BITS == 8)
  spi_write(DS3234_SPI,address);
  spi_read(DS3234_SPI, &dataIO);	// Read first 8-bits to prevent overrun (invalid data coming from RTC)
  spi_write(DS3234_SPI,0);			// Generate 8 clocks to get data
  spi_read(DS3234_SPI, &dataIO);	// Read valid data coming from RTC
  *data = 0x00FF & dataIO;
#else
#error "Invalid DS3234_SPI_BITS setting"
#endif

  // Unselect chip
  spi_unselectChip(DS3234_SPI, DS3234_SPI_NPCS);

#ifndef FREERTOS_USED
  // Re-enable global interrupts
  Enable_global_interrupt();
#endif

  return DS3234_OK;

#endif	// NO_DS3234
}


static S8 writeRTCByte(U8 address, U8 data)
{

#ifdef NO_DS3234

  return DS3234_OK;

#else

  U16 dataIO;

#if (DS3234_SPI_BITS == 16)
  // Assemble 16-bit stream for 16-bit transfers
  dataIO = address + WRITE_OFFSET;
  dataIO <<= 8;
  dataIO += data;
#endif

#ifndef FREERTOS_USED
  // Disable interrupts to prevent an SPI preemption abort data transfer
  // (better way to do it would be using mutexes)
  Disable_global_interrupt();
#endif

  // Select chip
  spi_selectChip(DS3234_SPI, DS3234_SPI_NPCS);

  // Write-read
#if (DS3234_SPI_BITS == 16)
  spi_write(DS3234_SPI,dataIO);
  spi_read(DS3234_SPI, &dataIO);	// Read to prevent an overrun
#elif (DS3234_SPI_BITS == 8)
  spi_write(DS3234_SPI,address + WRITE_OFFSET);
  spi_read(DS3234_SPI, &dataIO);	// Read to prevent an overrun
  spi_write(DS3234_SPI,data);
  spi_read(DS3234_SPI, &dataIO);	// Read to prevent an overrun
#else
#error "Invalid DS3234_SPI_BITS setting"
#endif

  // Unselect chip
  spi_unselectChip(DS3234_SPI, DS3234_SPI_NPCS);

  // Re-enable global interrupts
  Enable_global_interrupt();

  return DS3234_OK;

#endif	// NO_DS3234
}


static U8 dec2BCD(U8 number)
{
 U8 units;

 units = number % 10;
 number /= 10;
 number <<= 4;
 number |= units;

 return number;
}


static U8 BCD2dec(U8 BCDnumber)
{
  return ((BCDnumber & 0xF0)>>4)*10 + (BCDnumber & 0x0F);
}