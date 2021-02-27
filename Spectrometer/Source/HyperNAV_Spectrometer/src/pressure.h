/*!	\file	pressure.h
 *	\brief	Driver for Paroscientific Digiquartz pressure sensor
 *
 *	Usage notes  (REV A):
 *		Initialize driver (call PRES_Initialize());
 *		Start temperature measurement (PRES_StartMeasurement(TEMP_FREQ))
 *		Delay for temperature measurement (max time dependent on external divisor)
 *		Retrieve temperature period (PRES_GetPerioduSecs(&sensor, &period)
 *		Start pressure measurement (PRES_StartMeasurement(PRES_FREQ))
 *		Delay for pressure measurement (max time dependent on external divisor)
 *		Retrieve pressure period (PRES_GetPerioduSecs(&sensor, &period)
 *		Calculate pressure (dBAR = PRES_CalculatePressure_dBar(t_period, p_period, &TdegC, &Ppsia))
 *	Usage notes  (REV B):  
 *		Pressure and Temperature measurements can be made concurrently and with 
 *		different "integration" times, limited by 16-bit counter and external divider
 *
 *		Initialize driver (call PRES_Initialize());
 *		Start temperature measurement (PRES_StartMeasurement(TEMP_FREQ))
 *		Start pressure measurement (PRES_StartMeasurement(PRES_FREQ))
 *		Delay for measurements (max time dependent on external divisors)
 *		Retrieve temperature and pressure period (PRES_GetPerioduSecs(&sensor, &period)
 *		Calculate pressure (dBAR = PRES_CalculatePressure_dBar(t_period, p_period, &TdegC, &Ppsia))
 *
 * Created: 10/08/2016 4:12:55 PM
 *  Author: rvandommelen / Scott Feener
 *
 *	History: 
 *		2017-04-21, SKF: Updates for rev B PCB
 */ 
#ifndef PRESSURE_H_
#define PRESSURE_H_

#include <compiler.h>
#include <stdint.h>

# define PRES_Ok 0
# define PRES_Failed 1

# define PRES_AlreadySampling 2
# define PRES_NotDone 3
# define PRES_UnknownSensor 4

typedef enum { DIGIQUARTZ_Nothing, DIGIQUARTZ_PRESSURE, DIGIQUARTZ_TEMPERATURE } digiquartz_t;

typedef struct DigiQuartz_Calibration_Coefficients {
  F64 U0;
  F64 Y1;
  F64 Y2;
  F64 Y3;
  F64 C1;
  F64 C2;
  F64 C3;
  F64 D1;
  F64 D2;
  F64 T1;
  F64 T2;
  F64 T3;
  F64 T4;
  F64 T5;
} DigiQuartz_Calibration_Coefficients_t;

void PRES_InitGpio(void);

/*! \brief Turns on/off Digiquartz power supply
	\param	on	Applies power to Digiquartz if true, removes power if false
 */
void PRES_power(bool on);

/*! \brief	Calculates pressure in dBar
	
	Also calculates Digiquartz temperature and pressure in psi (absolute)

	\param	t_period	measured temperature signal period (in microseconds)
	\param	p_period	measured pressure signal period (in microseconds)
	\param	TdegC		calculated temperature, in Celsius
	\param	Ppsia		calculated pressure, in psi
	
	\return				Calculated pressure, in dBar
 */
// F64 PRES_CalculatePressure_dBar(F64 t_period, F64 p_period, F64 *TdegC, F64 *Ppsia);

F64 PRES_Calc_Pressure_dBar(F64 t_period, F64 p_period, DigiQuartz_Calibration_Coefficients_t const* coeffs, F64 *TdegC, F64 *Ppsia);

/*! \brief	Initializes the Digiquartz pressure sensor driver resources
 *  \return     0 for success, 1 for failure
 */
S16 PRES_Initialize(void);

/*! \brief	Starts a measurement on the specified Digiquartz channel (temperature or pressure)
	\param	sensor		start measurement on specified sensor type: pressure (PRES_FREQ) or temperature (TEMP_FREQ)
	\return				CEC_Ok if successful, CEC_Failed otherwise
 */
S16 PRES_StartMeasurement(digiquartz_t sensor);

/*! \brief	Completes measurement, calculates and returns period of currently running sample (temperature or pressure)
	\param	sensor			currently sampling sensor type: pressure (PRES_FREQ) or temperature (TEMP_FREQ) 
	\param	period_usecs	measured period, in microseconds
	\return					CEC_Failed if not currently sampling, CEC_Ok otherwise			
 */
#if (MCU_BOARD_REV == REV_A)
S16	PRES_GetPerioduSecs(digiquartz_t *sensor, F64 *period_usecs, int* counts, U32* duration, int16_t frequency_divisor );
#else
S16 PRES_GetPerioduSecs(digiquartz_t sensortype, digiquartz_t *sensor, F64 *period_usecs, int* counts, U32* duration, int16_t frequency_divisor );
#endif

#if (MCU_BOARD_REV == REV_A)
digiquartz_t PRES_GetCurrentMeasurement( void );
#else
digiquartz_t PRES_GetCurrentMeasurement(digiquartz_t sensor); 
#endif

#endif /* PRESSURE_H_ */
