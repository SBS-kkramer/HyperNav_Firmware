/*! \file sht2x.c *************************************************************
 *
 * \brief Sensirion SHT2X humidity sensor driver
 *
 *
 *      @author      Diego, Satlantic LP. (Based on code provided by manufacturer)
 *      @date        2014-04-25
 *
 *
 *	2016-10-09, SF: Porting to HyperNAV Controller
 **********************************************************************************/

#include "i2c.h"
#include "delay.h"
#include "SHT2x.h"


//  CRC
static  const U16 POLYNOMIAL = 0x131;  //P(x)=x^8+x^5+x^4+1 = 100110001


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************


U8 SHT2x_CheckCrc(U8 data[], U8 nbrOfBytes, U8 checksum)
{
  U8 crc = 0;
  U8 byteCtr;
  U8 bit;

  //calculates 8-Bit checksum with given polynomial
  for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr)
  { crc ^= (data[byteCtr]);
    for (bit = 8; bit > 0; --bit)
    { if (crc & 0x80) crc = (crc << 1) ^ POLYNOMIAL;
      else crc = (crc << 1);
    }
  }
  if (crc != checksum)
	  return TRUE;
  else
	  return FALSE;
}


U8 SHT2x_ReadUserRegister(U8 *pRegisterValue)
{
  U8 checksum;   //variable for checksum byte
  U8 error=0;    //variable for error code

  I2C_start();
  error |= !I2C_write(I2C_ADR_W, ACK);
  error |= !I2C_write(USER_REG_R, ACK);
  I2C_start();
  error |= !I2C_write(I2C_ADR_R, ACK);
  *pRegisterValue = I2C_read(ACK);
  checksum=I2C_read(NACK);
  error |= SHT2x_CheckCrc (pRegisterValue,1,checksum);
  I2C_stop();

  return error;
}

//===========================================================================
U8 SHT2x_WriteUserRegister(U8 *pRegisterValue)
//===========================================================================
{
  U8 error=0;   //variable for error code

  I2C_start();
  error |= !I2C_write(I2C_ADR_W, ACK);
  error |= !I2C_write(USER_REG_W, ACK);
  error |= !I2C_write(*pRegisterValue, ACK);
  I2C_stop();

  return error;
}

//===========================================================================
U8 SHT2x_MeasureHM(etSHT2xMeasureType eSHT2xMeasureType, U16 *pMeasurand)
//===========================================================================
{
  U8  checksum;   //checksum
  U8  data[2];    //data array for checksum verification
  U8  error=0;    //error variable
  U16 i;          //counting variable

  //-- write I2C sensor address and command --
  I2C_start();
  error |= !I2C_write(I2C_ADR_W, ACK); // I2C Adr
  switch(eSHT2xMeasureType)
  {
	case HUMIDITY: error |= !I2C_write(TRIG_RH_MEASUREMENT_HM, ACK); break;
    case TEMP    : error |= !I2C_write(TRIG_T_MEASUREMENT_HM, ACK);  break;
    default: break;
  }
  //-- wait until hold master is released --
  I2C_start();
  error |= !I2C_write(I2C_ADR_R, ACK);
  I2C_SCLHi();                  // set SCL I/O port as Hi-Z
  for(i=0; i<20; i++)          // wait until master hold is released or
  {
	delay_ms(5);    			// a timeout (~100ms) is reached
    if (I2C_SCL()) break;		// if sensor releases SCL line then means it has finished conversion
  }
  //-- check for timeout --
  if(i>=20)
    error = TRUE;
  else
  {
	  //-- read two data bytes and one checksum byte -- (NOTE: AVR32 is big endian)
	  ((U8*)pMeasurand)[0] = data[0] = I2C_read(ACK);	// MSB
	  ((U8*)pMeasurand)[1] = data[1] = I2C_read(ACK);	// LSB
	  checksum=I2C_read(NACK);

	  //-- verify checksum --
	  error |= SHT2x_CheckCrc (data,2,checksum);
  }

  I2C_stop();

  return error;
}

// Not used then not ported
#if 0
//===========================================================================
U8 SHT2x_MeasurePoll(etSHT2xMeasureType eSHT2xMeasureType, nt16 *pMeasurand)
//===========================================================================
{
  U8  checksum;   //checksum
  U8  data[2];    //data array for checksum verification
  U8  error=0;    //error variable
  U16 i=0;        //counting variable

  //-- write I2C sensor address and command --
  I2C_start();
  error |= !I2C_write (I2C_ADR_W); // I2C Adr
  switch(eSHT2xMeasureType)
  { case HUMIDITY: error |= !I2C_write (TRIG_RH_MEASUREMENT_POLL); break;
    case TEMP    : error |= !I2C_write (TRIG_T_MEASUREMENT_POLL);  break;
    default: assert(0);
  }
  //-- poll every 10ms for measurement ready. Timeout after 20 retries (200ms)--
  do
  { I2C_start();
    DelayMicroSeconds(10000);  //delay 10ms
    if(i++ >= 20) break;
  } while(I2c_WriteByte (I2C_ADR_R) == ACK_ERROR);
  if (i>=20) error |= TIME_OUT_ERROR;

  //-- read two data bytes and one checksum byte --
  pMeasurand->s16.u8H = data[0] = I2C_read(ACK);
  pMeasurand->s16.u8L = data[1] = I2C_read(ACK);
  checksum=I2C_read(NACK);

  //-- verify checksum --
  error |= SHT2x_CheckCrc (data,2,checksum);
  I2C_stop();

  return error;
}
#endif

//===========================================================================
U8 SHT2x_SoftReset()
//===========================================================================
{
  U8  error=0;           //error variable

  I2C_start();
  error |= !I2C_write(I2C_ADR_W, ACK); // I2C Adr
  error |= !I2C_write(SOFT_RESET, ACK);// Command
  I2C_stop();

  delay_ms(15); 		// wait till sensor has restarted

  return error;
}

//==============================================================================
F32 SHT2x_CalcRH(U16 u16sRH)
//==============================================================================
{
  F32 humidityRH;              // variable for result

  u16sRH &= ~0x0003;          // clear bits [1..0] (status bits)

  //-- calculate relative humidity [%RH] --
  humidityRH = -6.0 + 125.0/65536 * (F32)u16sRH; // RH= -6 + 125 * SRH/2^16

  return humidityRH;
}

//==============================================================================
F32 SHT2x_CalcTemperatureC(U16 u16sT)
//==============================================================================
{
  F32 temperatureC;           // variable for result

  u16sT &= ~0x0003;           // clear bits [1..0] (status bits)

  //-- calculate temperature [°C] --
  temperatureC= -46.85 + 175.72/65536 *(F32)u16sT; //T= -46.85 + 175.72 * ST/2^16

  return temperatureC;
}

//==============================================================================
U8 SHT2x_GetSerialNumber(U8 u8SerialNumber[])
//==============================================================================
{
  U8  error=0;                          //error variable

  //Read from memory location 1
  I2C_start();
  error |= !I2C_write (I2C_ADR_W, ACK); //I2C address
  error |= !I2C_write (0xFA, ACK);      //Command for readout on-chip memory
  error |= !I2C_write (0x0F, ACK);      //on-chip memory address
  I2C_start();
  error |= !I2C_write (I2C_ADR_R, ACK); //I2C address
  u8SerialNumber[5] = I2C_read(ACK);	//Read SNB_3
  I2C_read(ACK);                     	//Read CRC SNB_3 (CRC is not analyzed)
  u8SerialNumber[4] = I2C_read(ACK); 	//Read SNB_2
  I2C_read(ACK);                     	//Read CRC SNB_2 (CRC is not analyzed)
  u8SerialNumber[3] = I2C_read(ACK); 	//Read SNB_1
  I2C_read(ACK);                     	//Read CRC SNB_1 (CRC is not analyzed)
  u8SerialNumber[2] = I2C_read(ACK); 	//Read SNB_0
  I2C_read(NACK);                    	//Read CRC SNB_0 (CRC is not analyzed)
  I2C_stop();

  //Read from memory location 2
  I2C_start();
  error |= !I2C_write (I2C_ADR_W, ACK); //I2C address
  error |= !I2C_write (0xFC, ACK);      //Command for readout on-chip memory
  error |= !I2C_write (0xC9, ACK);      //on-chip memory address
  I2C_start();
  error |= !I2C_write (I2C_ADR_R, ACK); //I2C address
  u8SerialNumber[1] = I2C_read(ACK); 	//Read SNC_1
  u8SerialNumber[0] = I2C_read(ACK); 	//Read SNC_0
  I2C_read(ACK);                    	//Read CRC SNC0/1 (CRC is not analyzed)
  u8SerialNumber[7] = I2C_read(ACK); 	//Read SNA_1
  u8SerialNumber[6] = I2C_read(ACK);	//Read SNA_0
  I2C_read(NACK);                  		//Read CRC SNA0/1 (CRC is not analyzed)
  I2C_stop();

  return error;
}
