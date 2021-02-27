/*! \file sysmon.c *************************************************************
 *
 * \brief System monitor driver.
 *
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-24
 *
 * 2016-10-10, SF: Adapting for HyperNAV Controller
 **********************************************************************************/
#include "compiler.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
//#include "ads8344.h"
#include "avr32rerrno.h"
#include "sys/time.h"

#include "sysmon.h"
#include "sysmon_cfg.h"

#ifdef SYSMON_OW_TSENS
#include "onewire.h"
#include "ds18s20.h"
#endif

#include "delay.h"
#include "pm.h"
#include "i2c.h"
#include "sht2x.h"
#include "internal_adc.h"

#include "io_funcs.controller.h"

//*****************************************************************************
// Configuration
//*****************************************************************************

//! @note Hardware dependent definitions should be placed in the corresponding
//! section of the board definition file. This board definition file is embedded
//! 'board.h'.

#ifndef NO_SYSMON		



//*****************************************************************************
// Local Variables
//*****************************************************************************
static Bool sysmonIsOn = FALSE;


#ifdef SYSMON_OW_TSENS

typedef struct OW_MEASUREMENT
{
	U64 ROMID;					// Sensor ROM ID
	struct timeval start;		// Last measurement start time
	Bool	measuring;			// Measurement in progress flag
	F32		lastT;
} measurement_t;

static measurement_t	transformerT;
static measurement_t	spectrometerT;
static measurement_t	housingT;
static Bool initdROMIDs = FALSE;		// Flag to prevent loosing ROM IDs upon system wake-up reinit

#endif // #ifdef SYSMON_OW_TSENS


//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************
static void sysmonOn(void);
// static Bool readADCVolts(U8 channel, F32* volts);
// static Bool readADCCounts(U8 channel, U16* counts);
// static U32 elapsedToNow(struct timeval timeBefore);		// Return elapsed time in milliseconds


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************


/* \brief Initialize system monitor
 * @note This function is to be called only once from inside the system init function
 * @param	fPBA	Peripheral Bus 'A' clock frequency
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
 */
S16 sysmon_init(U32 fPBA)
{

	// Initialize off
	sysmon_off();
	
	// Init I2C for humidity sensor
	I2C_init(fPBA);

#if 0
	#warning "Why are ADS8344 functions still present?  SKF 2016-10-09"
	// Initialize ADC
	if(ads8344_init(fPBA) != ADS8344_OK)
	{
		avr32rerrno = EADC;
		return SYSMON_FAIL;
	}
#else
 	const gpio_map_t ADC_GPIO_MAP = {
		 {VMAIN_SENSE_PIN, VMAIN_SENSE_FUNCTION},
		 {P3V3_SENSE_PIN, P3V3_SENSE_FUNCTION}
 	};
	gpio_enable_module(ADC_GPIO_MAP, sizeof(ADC_GPIO_MAP)/sizeof(ADC_GPIO_MAP[0]));
	
	// ensure module is powered
	pm_enable_module(&AVR32_PM, AVR32_ADC_CLK_PBA);
	
	/* Configure the ADC peripheral module.
	 * Lower the ADC clock to match the ADC characteristics (because we
	 * configured the CPU clock to 12MHz, and the ADC clock characteristics are
	 *  usually lower; cf. the ADC Characteristic section in the datasheet). */
	AVR32_ADC.mr |= 0x1 << AVR32_ADC_MR_PRESCAL_OFFSET;
	adc_configure(&AVR32_ADC);

	/* Enable the ADC channels. */
	adc_enable(&AVR32_ADC, SYSMON_VMAIN_CH);
	adc_enable(&AVR32_ADC, SYSMON_P3V3_CH);
	
#endif

#ifdef SYSMON_OW_TSENS

	//-- Initialize OneWire bus --
	if(ow_init(fPBA) != OW_OK)
	{
		avr32rerrno = EOW;
		return SYSMON_FAIL;
	}

	if(!initdROMIDs)
	{
		housingT.ROMID = 0;
		spectrometerT.ROMID = 0;
		transformerT.ROMID = 0;
		initdROMIDs = TRUE;
	}
	// Flag temperature measurement NOT in progress
	housingT.measuring = FALSE;
	spectrometerT.measuring = FALSE;
	transformerT.measuring = FALSE;

	housingT.lastT = 0;
	spectrometerT.lastT = 0;
	transformerT.lastT = 0;

#endif

	// Initialize humidity sensor

	//	As  a  first  step,  the  sensor  is  powered  up  to  the  chosen
	//	supply  voltage  VDD  (between  2.1V  and  3.6V).  After
	//	power-up,  the  sensor  needs at  most  15ms,  while  SCL  is
	//	high,  for  reaching  idle  state,  i.e.  to  be  ready  accepting
	//	commands from the master (MCU). (Source: Sensirion SHT21 Datasheet)
	I2C_SCLHi();
	delay_ms(15);
	SHT2x_SoftReset();
	delay_ms(15);
	SHT2x_SoftReset();		// Noticed empirically that the first call to the soft reset routine fails if
	// the system is warm booting (reboot command or WDT reset) hence the 2 calls.
	// Also, as a preventative measure will not error out the sysmon init upon humidity sensor
	// failure in order to avoid hanging the system due to this failure only. A centinel
	// value is used instead (255%). - DAS 27-04-14

	return SYSMON_OK;
}



/* \brief De-initialize system monitor in preparation for sleep.
 */
void sysmon_deinit(void)
{
	sysmon_off();
}

/* \brief Turn off system monitor.
 * @note The system monitor amplifiers can be turned off for power saving purposes.
 * The system monitor does not need to be explicitly turned back on as it will be activated
 * automatically when calling any of the access functions and will remain in that state
 * until the next call to 'sysmon_off()'.
 */
void sysmon_off(void)
{
	gpio_clr_gpio_pin(SYSMON_ON);
	sysmonIsOn = FALSE;
}










/* \brief Get rail voltages
 * @param vMain	OUTPUT: VMAIN rail voltage
 * @param vP3V3	OUTPUT: P3V3 rail voltage
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getVoltages(F32* vMain, F32* vP3V3)
{
	uint32_t raw1, raw2;
	
	// Sanity check
	if ((vP3V3 == NULL) || (vMain == NULL))
	{
		avr32rerrno = EPARAM;
		return SYSMON_FAIL;
	}
	
	sysmonOn();
	
	/* Start conversions on all enabled channels */
	adc_start(&AVR32_ADC);
	
	raw1 = adc_get_value(&AVR32_ADC, SYSMON_VMAIN_CH);
	raw2 = adc_get_value(&AVR32_ADC, SYSMON_P3V3_CH);
	
	//io_print_U32("raw Vmain = %lu\r\n", raw1);
	//io_print_U32("raw P3V3 = %lu\r\n", raw2);	

	//convert to voltage
	*vMain = 3.3 * ((F32)raw1/1024.0) * ((SYSMON_R1__VMAIN + SYSMON_R2__VMAIN)/SYSMON_R2__VMAIN);
	*vP3V3 = 3.3 * ((F32)raw2/1024.0) * ((SYSMON_R1__P3V3 + SYSMON_R2__P3V3)/SYSMON_R2__P3V3);

	return SYSMON_OK;	
}


/* \brief Get main current sense reading in milliamps
 * @param iMain	OUTPUT: Main supply sensed current (mA)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
/*
S16 sysmon_getMainCurrentSense(F32* iMain)
{
	F32 chVoltage;

	// Sanity check
	if(iMain == NULL)
	{
		avr32rerrno = EPARAM;
		return SYSMON_FAIL;
	}

	// Ensure system monitor amplifiers are ON
	sysmonOn();

	// Read corresponding ADC channel
	if(!readADCVolts(SYSMON_ISENSE_CH, &chVoltage))
	{
		avr32rerrno = EADC;
		return SYSMON_FAIL;
	}
	else
	{
		*iMain = 1000.0 * (chVoltage / (SYSMON_I_SENSE_GM * SYSMON_I_RSENSE * SYSMON_I_ROUT));
		return SYSMON_OK;
	}
}*/



/* \brief Get case relative humidity
 * @param rh	OUTPUT: Relative humidity in %RH units (0.0-100.0%)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
 * @ note Humidity measurement is not temperature compensated, therefore, the accuracy is ±5%RH
 */
/*
S16 sysmon_getHumidity(F32* rh)
{
	F32 chVoltage, sensorVout, humidity;

	// Sanity check
	if(rh == NULL)
	{
		avr32rerrno = EPARAM;
		return SYSMON_FAIL;
	}

	// Ensure system monitor amplifiers are ON
	sysmonOn();

	// Read corresponding ADC channel
	if(!readADCVolts(SYSMON_HUMIDITY_CH, &chVoltage))
	{
		avr32rerrno = EADC;
		return SYSMON_FAIL;
	}
	else
	{
		// Obtain sensor output
		sensorVout = chVoltage * ((SYSMON_R1__HUMIDITY + SYSMON_R2__HUMIDITY)/SYSMON_R2__HUMIDITY);

		// Convert to relative humidity using 20C pre-compensation (See Jira 2008-401-63)
		//
		// rh = ((Vout - d)/a) * K(20) + O(20) where 'd' and 'a' come from a second order temperature
		// compensation formula: Vout = (a+bT+cT^2) RH + (d+eT+fT^2), and K(20) and O(20) is obtained from
		// compensation curves. See Jira 2008-401-63 for curves and an '.m' file to calculate K(20) and O(20).
		//
		humidity = ((sensorVout - 0.9237)/0.0305) * 0.98578 + 2.1086;

		// The error introduced due to not compensating for temperature is small enough not to be a concern.
		// However it could result in overestimating a high humidity or underestimating a low humidity,
		// so clamping is required.
		humidity = (humidity > 100.0)?100.0:humidity;
		humidity = (humidity < 0)?0:humidity;
		*rh = humidity;

		return SYSMON_OK;
	}
}*/

/* \brief Get case relative humidity from a digital sensor (Sensirion SHT2X)
 * @param rh	OUTPUT: Relative humidity in %RH units (0.0-100.0%)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 */
S16 sysmon_getHumiditySHT2X(F32* rh)
{
	U16 rawHumidity;

	// Initialize return value in case the call fails
	*rh = BAD_HUMIDITY;

	if(!SHT2x_MeasureHM(HUMIDITY, &rawHumidity))
	{
		*rh = SHT2x_CalcRH(rawHumidity);
		return SYSMON_OK;
	}
	else
		return SYSMON_FAIL;
}

/* \brief Get case temperature from a digital sensor (Sensirion SHT2X)
 * @param rh	OUTPUT: Temperature in C
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 */
S16 sysmon_getTemperature(F32* rh)
{
	U16 temperature;

	if(!SHT2x_MeasureHM(TEMP, &temperature))
	{
		*rh = SHT2x_CalcTemperatureC(temperature);
		return SYSMON_OK;
	}
	else
		return SYSMON_FAIL;
}



//=============================================================================
// One wire temperature sensors support
//=============================================================================
#ifdef SYSMON_OW_TSENS


/* \brief Search for 1-Wire sensors on the bus.
 * @note Use this function to support the sensor identification procedure at calibration time.
 * @param	maxSensors	The maximum number of sensors to look for
 * @param	nSensors	OUTPUT: Number of sensors found
 * @param	ROMIDs		OUTPUT: ROM ids of the sensors found (caller must allocate enough space)
 * @return  SYSMON_OK: Success, SYSMON_FAIL: Error while searching for sensors, check 'avr32rerrno'
 */
S16 sysmon_searchOWSensors(U16 maxSensors, U16* nSensors, U64* ROMIDs)
{
	U16 i = 0;
	Bool foundSensor;

	//Sanity check
	if(maxSensors < 1 || nSensors == NULL || ROMIDs == NULL)
	{
		avr32rerrno = EPARAM;
		return SYSMON_FAIL;
	}

	// Initialize nSensors
	*nSensors = 0;


	// Get the first sensor ROM ID
	if(!ow_first(ROMIDs))
		return SYSMON_OK;

	// Get the remaining ROMIDs
	*nSensors = 1;
	for(i=1; i<maxSensors; i++)
	{
		foundSensor = ow_next(ROMIDs+i);
		if(foundSensor)
			(*nSensors)++;
		else
			break;
	}

	return SYSMON_OK;
}

static S16 sysmon_get1WireT ( measurement_t* mt, F32* T ) {

	// Sanity check
	if (T == NULL)
	{
		avr32rerrno = EPARAM;
		return SYSMON_FAIL;
	}

	// Check for sensor availability
	if(mt->ROMID == 0)
	{
		avr32rerrno = ETSENSOR;
		return SYSMON_FAIL;
	}

	// If we are measuring check that enough time has passed, and if so is the case finish the measurement
	// (read the result from the sensor)
	if(mt->measuring)
	{
		if(elapsedToNow(mt->start) >= DS18S20_TCONV_MS)
		{
			mt->measuring = FALSE;
			if(ds18s20_finish(mt->ROMID, T) != DS18S20_OK)
			{
				avr32rerrno = ETSENSOR;
				return SYSMON_FAIL;
			}
			else
			{
				if(ds18s20_start(mt->ROMID) != DS18S20_OK)
				{
					avr32rerrno = ETSENSOR;
					return SYSMON_FAIL;
				}
				else
				{
					mt->lastT = *T;
					gettimeofday(&(mt->start),NULL);
					mt->measuring = TRUE;
					return SYSMON_OK;
				}
			}
		}
		// Not enough time has passed, return
		else {
			*T = mt->lastT;
			return SYSMON_OK;
		}
	}
	// Else, we are not measuring, so this means start a new measurement
	else
	{
		if(ds18s20_start(mt->ROMID) != DS18S20_OK)
		{
			avr32rerrno = ETSENSOR;
			return SYSMON_FAIL;
		}
		else
		{
			gettimeofday(&(mt->start),NULL);
			mt->measuring = TRUE;
			return SYSMON_BUSY;
		}
	}
}

/* \brief Set the ROM ID corresponding to the spectrometer temperature sensor
 * @param	ROMID	Spectrometer temperature sensor ROM id.
 */
void sysmon_setTransformerTSensorID(U64 ROMID)
{
	transformerT.ROMID = ROMID;
	transformerT.measuring = false;
	transformerT.lastT = 0;
}



/* \brief Initiate/query an spectrometer temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	sT	OUTPUT: Last spectrometer temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'avr32rerrno'
 */
S16 sysmon_getTransformerTemp(F32* tT)
{
	return sysmon_get1WireT( &transformerT, tT);
}



/* \brief Set the ROM ID corresponding to the spectrometer temperature sensor
 * @param	ROMID	Spectrometer temperature sensor ROM id.
 */
void sysmon_setSpectrometerTSensorID(U64 ROMID)
{
	spectrometerT.ROMID = ROMID;
	spectrometerT.measuring = false;
	spectrometerT.lastT = 0;
}



/* \brief Initiate/query an spectrometer temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	sT	OUTPUT: Last spectrometer temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'avr32rerrno'
 */
S16 sysmon_getSpectrometerTemp(F32* sT)
{
	return sysmon_get1WireT( &spectrometerT, sT);
}



/* \brief Set the ROM ID corresponding to the housing temperature sensor
 * @param	ROMID	Housing temperature sensor ROM id.
 */
void sysmon_setHousingTSensorID(U64 ROMID)
{
	housingT.ROMID = ROMID;
	housingT.measuring = false;
	housingT.lastT = 0;

}



/* \brief Initiate/query a housing temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	hT	OUTPUT: Last housing temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'avr32rerrno'
 */
S16 sysmon_getHousingTemp(F32* hT)
{
	return sysmon_get1WireT( &housingT, hT);
}


#endif


//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

// Turn system monitor amplifiers on if they are off
static void sysmonOn(void)
{
	if(!sysmonIsOn)
	{
		gpio_set_gpio_pin(SYSMON_ON);
                vTaskDelay( (portTickType)TASK_DELAY_MS( 1 ) );
		sysmonIsOn = TRUE;
	}
}





// static Bool readADCVolts(U8 channel, F32* volts)
// {
// 	U16 counts;
// 
// #if 0
// 	if(ads8344_sample(channel, TRUE, TRUE, &counts) != ADS8344_OK)
// 		return FALSE;
// 	else
// 	{
// 		*volts = ads8344_counts2Volts(counts);
// 		return TRUE;
// 	}
// #else
// 	#warning "need onboard ADC measurement"
// 	return FALSE;
// #endif	
// }
// 
// 
// static Bool readADCCounts(U8 channel, U16* counts)
// {
// 
// #if 0
// 	if(ads8344_sample(channel, TRUE, TRUE, counts) != ADS8344_OK)
// 		return FALSE;
// 	else
// 		return TRUE;
// 		
// #else
// 	#warning "need onboard ADC measurement"
// 	return FALSE;
// #endif		
// 
// }

# if 0
// Return elapsed time in milliseconds
static U32 elapsedToNow(struct timeval before)
{
	struct timeval now;

	gettimeofday(&now,NULL);

	return (now.tv_sec - before.tv_sec)*1000 + (now.tv_usec - before.tv_usec)/1000;
}
# endif


#else	// #ifndef NO_SYSMON
#error "No System Monitor support!"

//*****************************************************************************
//	Stubs for development systems
//*****************************************************************************

/* \brief Initialize system monitor
 * @note This function is to be called only once from inside the system init function.
 * @param	fPBA	Peripheral Bus 'A' clock frequency
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
 */
S16 sysmon_init(U32 fPBA)
{
	return SYSMON_OK;
}


/* \brief Turn off system monitor.
 * @note The system monitor amplifiers can be turned off for power saving purposes.
 * The system monitor does not need to be explicitly turned back on as it will be activated
 * automatically when calling any of the access functions and will remain in that state
 * until the next call to 'sysmon_off()'.
 */
void sysmon_off(void)
{
	return;
}


/* \brief Get main supply voltage
 * @param vMain	OUTPUT: Main supply voltage
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getMainSupply(F32* vMain)
{
	*vMain = 14;
	return SYSMON_OK;
}


/* \brief Get 12V lamp power supply voltage
 * @param vLamp	OUTPUT: Lamp power supply voltage
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_get12VSupply(F32* vLamp)
{
	*vLamp = 12;
	return SYSMON_OK;
}


/* \brief Get 5V rail voltage
 * @param v5V	OUTPUT: 5V rail voltage
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_get5VSupply(F32* v5V)
{
	*v5V = 5;
	return SYSMON_OK;
}



/* \brief Get main current sense reading in milliamps
 * @param iMain	OUTPUT: Main supply sensed current (mA)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getMainCurrentSense(F32* iMain)
{
	*iMain = 123;
	return SYSMON_OK;
}


/* \brief Get case relative humidity
 * @param rh	OUTPUT: Relative humidity in %RH units (0.0-100.0%)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
 * @ note Humidity measurement is not temperature compensated, therefore, the accuracy is ±5%RH
 */
S16 sysmon_getHumidity(F32* rh)
{
	*rh = 12.3;
	return SYSMON_OK;
}


/* \brief Get lamp temperature
 * @param 	lampT	OUTPUT: Lamp temperature in degrees Celsius.
 * @param	nAvg	Average size (1 for no averaging).
 * @param	delayms	Delay in millisecods between averaged measurements.
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getLampTemp(F32* lampT, U8 nAvg, U16 delayms)
{
        vTaskDelay( (portTickType)TASK_DELAY_MS( nAvg ? ((nAvg-1)*delayms) : delayms ) );
	*lampT = 67.8;
	return SYSMON_OK;
}


/* \brief Get reference channel counts
  * @param 	refCounts	OUTPUT: Reference channel counts
  * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getREFCounts(U16* refCounts)
{
	*refCounts = 5678;
	return SYSMON_OK;
}


#ifdef SYSMON_OW_TSENS
//=============================================================================
// One wire temperature sensors support
//=============================================================================

/* \brief Search for 1-Wire sensors on the bus.
 * @note Use this function to support the sensor identification procedure at calibration time.
 * @param	maxSensors	The maximum number of sensors to look for
 * @param	nSensors	OUTPUT: Number of sensors found
 * @param	ROMIDs		OUTPUT: ROM ids of the sensors found (caller must allocate enough space)
 * @return  SYSMON_OK: Success, SYSMON_FAIL: Error while searching for sensors, check 'avr32rerrno'
 */
S16 sysmon_searchOWSensors(U16 maxSensors, U16* nSensors, U64* ROMIDs)
{
	int i;

	for(i=0; i<2 && i<maxSensors; i++)
		ROMIDs[i] = i;

	*nSensors = (i<maxSensors)?maxSensors:2;

	return SYSMON_OK;
}



/* \brief Set the ROM ID corresponding to the spectrometer temperature sensor
 * @param	ROMID	Spectrometer temperature sensor ROM id.
 */
void sysmon_setSpectrometerTSensorID(U64 ROMID)
{
	return;
}



/* \brief Initiate/query an spectrometer temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	sT	OUTPUT: Last spectrometer temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'avr32rerrno'
 */
S16 sysmon_getSpectrometerTemp(F32* sT)
{
	*sT = 45;
	return SYSMON_OK;
}



/* \brief Set the ROM ID corresponding to the housing temperature sensor
 * @param	ROMID	Housing temperature sensor ROM id.
 */
void sysmon_setHousingTSensorID(U64 ROMID)
{
	return;
}



/* \brief Initiate/query a housing temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	hT	OUTPUT: Last housing temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'avr32rerrno'
 */
S16 sysmon_getHousingTemp(F32* hT)
{
	*hT = 23;
	return SYSMON_OK;
}


#endif // #ifdef SYSMON_OW_TSENS
//=============================================================================


#endif // #ifndef NO_SYSMON




