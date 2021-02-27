/*! \file sysmon.h**********************************************************
 *
 * \brief System Monitor Driver API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-24
 *
  ***************************************************************************/

#ifndef SYSMON_H_
#define SYSMON_H_

#include "compiler.h"
#include "sysmon_cfg.h"


//*****************************************************************************
// Returned constants
//*****************************************************************************
#define SYSMON_BUSY		1
#define SYSMON_OK		0
#define SYSMON_FAIL		-1


// Sentinel value for humidity measurement failure
#define BAD_HUMIDITY	255

//*****************************************************************************
// Exported functions
//*****************************************************************************

/* \brief Initialize system monitor
 * @note This function is to be called only once from inside the system init function.
 * @param	fPBA	Peripheral Bus 'A' clock frequency
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 */
S16 sysmon_init(U32 fPBA);


/* \brief De-initialize system monitor in preparation for sleep.
 */
void sysmon_deinit(void);


/* \brief Turn off system monitor.
 * @note The system monitor amplifiers can be turned off for power saving purposes.
 * The system monitor does not need to be explicitly turned back on as it will be activated
 * automatically when calling any of the access functions and will remain in that state
 * until the next call to 'sysmon_off()'.
 */
void sysmon_off(void);


/* \brief Get rail voltages
 * @param vMain	OUTPUT: VMAIN rail voltage
 * @param vP3V3	OUTPUT: P3V3 rail voltage
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'avr32rerrno'.
  */
S16 sysmon_getVoltages(F32* vMain, F32* vP3V3);


/* \brief Get main current sense reading in milliamps
 * @param iMain	OUTPUT: Main supply sensed current (mA)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
  */
//S16 sysmon_getMainCurrentSense(F32* iMain);


/* \brief Get case relative humidity
 * @param rh	OUTPUT: Relative humidity in %RH units (0.0-100.0%)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 * @ note Humidity measurement is not temperature compensated, therefore, the accuracy is ±5%RH
 */
//S16 sysmon_getHumidity(F32* rh);

/* \brief Get case relative humidity from a digital sensor (Sensirion SHT2X)
 * @param rh	OUTPUT: Relative humidity in %RH units (0.0-100.0%)
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 */
S16 sysmon_getHumiditySHT2X(F32* rh);


/* \brief Get case temperature from a digital sensor (Sensirion SHT2X)
 * @param rh	OUTPUT: Temperature in C
 * @return	SYSMON_OK: Success,	SYSMON_FAIL: Failure, check 'nsyserrno'.
 */
S16 sysmon_getTemperature(F32* rh);


//=============================================================================
// One wire temperature sensors support
//=============================================================================
#ifdef SYSMON_OW_TSENS


/* \brief Search for 1-Wire sensors on the bus.
 * @note Use this function to support the sensor identification procedure at calibration time.
 * @param	maxSensors	The maximum number of sensors to look for
 * @param	nSensors	OUTPUT: Number of sensors found
 * @param	ROMIDs		OUTPUT: ROM ids of the sensors found (caller must allocate enough space)
 * @return  SYSMON_OK: Success, SYSMON_FAIL: Error while searching for sensors, check 'nsyserrno'
 */
S16 sysmon_searchOWSensors(U16 maxSensors, U16* nSensors, U64* ROMIDs);



/* \brief Set the ROM ID corresponding to the spectrometer temperature sensor
 * @param	ROMID	Spectrometer temperature sensor ROM id.
 */
void sysmon_setTransformerTSensorID(U64 ROMID);



/* \brief Initiate/query an spectrometer temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	sT	OUTPUT: Last spectrometer temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'nsyserrno'
 */
S16 sysmon_getTransformerTemp(F32* tT);



/* \brief Set the ROM ID corresponding to the spectrometer temperature sensor
 * @param	ROMID	Spectrometer temperature sensor ROM id.
 */
void sysmon_setSpectrometerTSensorID(U64 ROMID);



/* \brief Initiate/query an spectrometer temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	sT	OUTPUT: Last spectrometer temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'nsyserrno'
 */
S16 sysmon_getSpectrometerTemp(F32* sT);



/* \brief Set the ROM ID corresponding to the housing temperature sensor
 * @param	ROMID	Housing temperature sensor ROM id.
 */
void sysmon_setHousingTSensorID(U64 ROMID);



/* \brief Initiate/query a housing temperature measurement
 * A temperature measurement could take up to 720ms depending on the temperature.
 * This function sends a 'take measurement' command and returns control to the caller immediately (non-blocking).
 * User must call this function again to check  for the result of the last measurement.
 * @param	hT	OUTPUT: Last housing temperature measurement result (in degrees Celsius).
 * @return SYSMON_OK: Temperature measurement finished. SYSMON_BUSY: Temperature measurement in progress,
 * check again later. SYSMON_FAIL: An error occurred, check 'nsyserrno'
 */
S16 sysmon_getHousingTemp(F32* hT);


#endif


#endif /* SYSMON_H_ */
