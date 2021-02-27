/*
 * lsm303.h
 *
 * Created: 28/07/2016 11:25:42 AM
 *  Author: rvandommelen
 */ 

#ifndef LSM303_H_
#define LSM303_H_

# include <compiler.h>
# include <stdint.h>

//#include "board.h"

# define LSM303_OK          0
# define LSM303_IO_ERROR   -1
# define LSM303_ARG_ERROR  -2
# define LSM303_PROC_ERROR -3

typedef struct lsm303_Accel_Raw {
	int16_t x;
	int16_t y;
	int16_t z;
} lsm303_Accel_Raw_t;

typedef struct lsm303_Mag_Raw {
	int16_t x;
	int16_t y;
	int16_t z;
} lsm303_Mag_Raw_t;

# if 0
typedef struct lsm303AccelData_s {
	float x;
	float y;
	float z;
} lsm303AccelData;
	
typedef struct lsm303MagData_s {
	float x;
	float y;
	float z;
	float orientation;
} lsm303MagData;
# endif

# ifdef COMPILE_OBSOLETE
typedef enum {
  Mag_CC_Min_X,
  Mag_CC_Max_X,
  Mag_CC_Min_Y,
  Mag_CC_Max_Y,
  Mag_CC_Min_Z,
  Mag_CC_Max_Z,
  Tlt_CC_Off_P,
  Tlt_CC_Off_R
} Mag_CalCoef_t;

typedef union {
  F32 f32;
  S16 s16[2];
} CC_Union_t;
# endif

int lsm303_init( void );
int lsm303_deinit( void );

int lsm303_get_accel_raw ( int nAvg, lsm303_Accel_Raw_t* raw );
int lsm303_calc_tilt ( lsm303_Accel_Raw_t* measured, lsm303_Accel_Raw_t* vertical,
                       double mounting_angle, double* pitch, double* roll, double* tilt );

//int lsm303_read_accel( double* LSM303_pitch, double* LSM303_roll );

int lsm303_get_mag_raw ( int nAvg, lsm303_Mag_Raw_t* raw );
int lsm303_calc_heading ( lsm303_Mag_Raw_t* raw, lsm303_Mag_Raw_t* min, lsm303_Mag_Raw_t* max,
	       double mounting_angle, double* heading );

//int lsm303_read_mag( double* LSM303_heading );

//int lsm303_setCalCoeff  ( CC_Union_t coeff, Mag_CalCoef_t which );
//CC_Union_t lsm303_getCalCoeff  ( Mag_CalCoef_t which );
//int lsm303_calibrate_mag( int16_t calCoeffs[] );

#endif /* LSM303_H_ */
