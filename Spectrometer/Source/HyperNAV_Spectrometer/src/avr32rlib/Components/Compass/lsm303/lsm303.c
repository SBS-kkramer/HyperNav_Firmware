/*
 * lsm303.c
 * Ultra compact high performance e-compass
 * 3D accelerometer and 3D magnetometer module 
 * http://www.st.com/resource/en/datasheet/lsm303dlhc.pdf
 * Example code found here: 
 * https://github.com/adafruit/Adafruit_LSM303/blob/master/Adafruit_LSM303.cpp
 *
 * Created: 28/07/2016 11:24:33 AM
 *  Author: rvandommelen
 
 * History:
 *	2017-04-20, SF: Updates for E980031B - removing hardcoded TWI links
 */ 

#include "lsm303.h"

#include <math.h>

# ifndef QUAT_ROT_FILE

typedef struct vector3d {
  double x;
  double y;
  double z;
} vector3d_t;

typedef struct quaternion {

  double q0;
  double qi;
  double qj;
  double qk;
  
} quaternion_t;

typedef struct quat_rot {
  double c;
  double s;
  double x;
  double y;
  double z;

} quat_rot_t;

static double V3d_norm ( vector3d_t v ) {
  return sqrt ( v.x*v.x + v.y*v.y + v.z*v.z );
}

static void V3d_normalize ( vector3d_t* v ) {

  if ( !v ) return;

  double const norm = sqrt ( v->x*v->x + v->y*v->y + v->z*v->z );

  if ( norm > 0 ) {
    v->x /= norm;
    v->y /= norm;
    v->z /= norm;
  }
}

static vector3d_t V3d_crossProduct ( vector3d_t a, vector3d_t b ) {

  vector3d_t c = {
    .x = a.y * b.z - a.z * b.y,
    .y = a.z * b.x - a.x * b.z,
    .z = a.x * b.y - a.y * b.x 
  };

  return c;
}

static double V3d_scalarProduct ( vector3d_t a, vector3d_t b ) {

  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static quaternion_t QAT_product ( quaternion_t u, quaternion_t v ) {

  quaternion_t uv = {
    .q0 = u.q0*v.q0 - u.qi*v.qi - u.qj*v.qj - u.qk*v.qk,
    .qi = u.q0*v.qi + u.qi*v.q0 + u.qj*v.qk - u.qk*v.qj,
    .qj = u.q0*v.qj - u.qi*v.qk + u.qj*v.q0 + u.qk*v.qi,
    .qk = u.q0*v.qk + u.qi*v.qj - u.qj*v.qi + u.qk*v.q0,
  };

  return uv;
}

# ifdef QURTERNION_TESTING
static quaternion_t QAT_ROT_product ( quaternion_t u, quaternion_t v ) {

  quaternion_t uv = {
    .q0 = u.q0*v.q0 - u.qi*v.qi - u.qj*v.qj - u.qk*v.qk,
    .qi = u.q0*v.qi + u.qi*v.q0 + u.qj*v.qk - u.qk*v.qj,
    .qj = u.q0*v.qj - u.qi*v.qk + u.qj*v.q0 + u.qk*v.qi,
    .qk = u.q0*v.qk + u.qi*v.qj - u.qj*v.qi + u.qk*v.q0,
  };

  return uv;
}

static vector3d_t ROT ( quat_rot_t qr, vector3d_t v ) {

  double omc = 1.0-qr.c;

  vector3d_t r = {
    .x = ( qr.x*qr.x*omc +      qr.c ) * v.x
       + ( qr.x*qr.y*omc - qr.z*qr.s ) * v.y
       + ( qr.x*qr.z*omc + qr.y*qr.s ) * v.z,
    .y = ( qr.y*qr.x*omc + qr.z*qr.s ) * v.x
       + ( qr.y*qr.y*omc +      qr.c ) * v.y
       + ( qr.y*qr.z*omc - qr.x*qr.s ) * v.z,
    .z = ( qr.z*qr.x*omc - qr.y*qr.s ) * v.x
       + ( qr.z*qr.y*omc + qr.x*qr.s ) * v.y
       + ( qr.z*qr.z*omc +      qr.c ) * v.z
  };

  return r;
}
# endif

static vector3d_t rot_meas_by_vert( vector3d_t measured, vector3d_t vertical ) {

  vector3d_t const down = { 0, 0, 1 };
  double const down_norm = 1.0;

  double const vert_norm = V3d_norm ( vertical );

  //  The axis of rotation is the vector normal to both the down vector and the calibration acceleration.
  //
  vector3d_t rotation_axis = V3d_crossProduct ( down, vertical );
  V3d_normalize ( &rotation_axis );

  //  The angle of rotation is obtained via
  //  cal dot down = |cal| x |down| x cos(theta)
  //
  double const theta = acos ( V3d_scalarProduct ( down, vertical ) / vert_norm / down_norm );

  //  Quaternion rotations use the cosine and sine of half the rotation angle.
  //
  double const theta_half = theta / 2;
  double const cos_theta_half = cos(theta_half);
  double const sin_theta_half = sin(theta_half);

  quaternion_t const rotation = {
    .q0 = cos_theta_half,
    .qi = sin_theta_half * rotation_axis.x,
    .qj = sin_theta_half * rotation_axis.y,
    .qk = sin_theta_half * rotation_axis.z
  };

  quaternion_t const rotation_inv = {
    .q0 =  rotation.q0,
    .qi = -rotation.qi,
    .qj = -rotation.qj,
    .qk = -rotation.qk
  };

  quaternion_t const qMeasured = {
    .q0 = 0,
    .qi = measured.x,
    .qj = measured.y,
    .qk = measured.z
  };


  quaternion_t const rim  = QAT_product ( rotation_inv, qMeasured );
  quaternion_t const rimr = QAT_product ( rim, rotation );

  vector3d_t const trueMeasured = {
    .x = rimr.qi,
    .y = rimr.qj,
    .z = rimr.qk
  };

  return trueMeasured;
}

static vector3d_t rot_around_z( vector3d_t vec, double angle ) {

  double const angle_half = angle / 2;
  double const cos_angle_half = cos(angle_half);
  double const sin_angle_half = sin(angle_half);

  quaternion_t const rotation = {
    .q0 = cos_angle_half,
    .qi = 0,
    .qj = 0,
    .qk = sin_angle_half
  };

  quaternion_t const rotation_inv = {
    .q0 =  rotation.q0,
    .qi = -rotation.qi,
    .qj = -rotation.qj,
    .qk = -rotation.qk
  };

  quaternion_t const qVec = {
    .q0 = 0,
    .qi = vec.x,
    .qj = vec.y,
    .qk = vec.z
  };

  quaternion_t const riv = QAT_product ( rotation_inv, qVec );
  quaternion_t const rivr = QAT_product ( riv, rotation );

  vector3d_t const rotVec = {
    .x = rivr.qi,
    .y = rivr.qj,
    .z = rivr.qk
  };

  return rotVec;
}

# endif


#include "board.h"

#include "twi_mux.h"
#include "twim.h"
#include "math.h"
#include "FreeRTOS.h"
#include "task.h"

#include "io_funcs.spectrometer.h"

#if (MCU_BOARD_REV==REV_A)
	#define LSM303_TWI_PORT	&AVR32_TWIM0
#else
	#define LSM303_TWI_PORT TWI_MUX_PORT
#endif	
	#define LSM303_TWI_PORT TWI_MUX_PORT


#define LSM303_ADDRESS_ACCEL          (0x32 >> 1)         // 0011001x
#define LSM303_ADDRESS_MAG            (0x3C >> 1)         // 0011110x
#define LSM303_ID                     (0b11010100)
#define LSM303_TENBIT                 false

typedef enum    { // DEFAULT    TYPE
    LSM303_REGISTER_ACCEL_CTRL_REG1_A         = 0x20,   // 00000111   rw
    LSM303_REGISTER_ACCEL_CTRL_REG2_A         = 0x21,   // 00000000   rw
    LSM303_REGISTER_ACCEL_CTRL_REG3_A         = 0x22,   // 00000000   rw
    LSM303_REGISTER_ACCEL_CTRL_REG4_A         = 0x23,   // 00000000   rw
    LSM303_REGISTER_ACCEL_CTRL_REG5_A         = 0x24,   // 00000000   rw
    LSM303_REGISTER_ACCEL_CTRL_REG6_A         = 0x25,   // 00000000   rw
    LSM303_REGISTER_ACCEL_REFERENCE_A         = 0x26,   // 00000000   r
    LSM303_REGISTER_ACCEL_STATUS_REG_A        = 0x27,   // 00000000   r
    LSM303_REGISTER_ACCEL_OUT_X_L_A           = 0x28,
    LSM303_REGISTER_ACCEL_OUT_X_H_A           = 0x29,
    LSM303_REGISTER_ACCEL_OUT_Y_L_A           = 0x2A,
    LSM303_REGISTER_ACCEL_OUT_Y_H_A           = 0x2B,
    LSM303_REGISTER_ACCEL_OUT_Z_L_A           = 0x2C,
    LSM303_REGISTER_ACCEL_OUT_Z_H_A           = 0x2D,
    LSM303_REGISTER_ACCEL_FIFO_CTRL_REG_A     = 0x2E,
    LSM303_REGISTER_ACCEL_FIFO_SRC_REG_A      = 0x2F,
    LSM303_REGISTER_ACCEL_INT1_CFG_A          = 0x30,
    LSM303_REGISTER_ACCEL_INT1_SOURCE_A       = 0x31,
    LSM303_REGISTER_ACCEL_INT1_THS_A          = 0x32,
    LSM303_REGISTER_ACCEL_INT1_DURATION_A     = 0x33,
    LSM303_REGISTER_ACCEL_INT2_CFG_A          = 0x34,
    LSM303_REGISTER_ACCEL_INT2_SOURCE_A       = 0x35,
    LSM303_REGISTER_ACCEL_INT2_THS_A          = 0x36,
    LSM303_REGISTER_ACCEL_INT2_DURATION_A     = 0x37,
    LSM303_REGISTER_ACCEL_CLICK_CFG_A         = 0x38,
    LSM303_REGISTER_ACCEL_CLICK_SRC_A         = 0x39,
    LSM303_REGISTER_ACCEL_CLICK_THS_A         = 0x3A,
    LSM303_REGISTER_ACCEL_TIME_LIMIT_A        = 0x3B,
    LSM303_REGISTER_ACCEL_TIME_LATENCY_A      = 0x3C,
    LSM303_REGISTER_ACCEL_TIME_WINDOW_A       = 0x3D
} lsm303AccelRegisters_t;

typedef enum {
    LSM303_REGISTER_MAG_CRA_REG_M             = 0x00,
    LSM303_REGISTER_MAG_CRB_REG_M             = 0x01,
    LSM303_REGISTER_MAG_MR_REG_M              = 0x02,
    LSM303_REGISTER_MAG_OUT_X_H_M             = 0x03,
    LSM303_REGISTER_MAG_OUT_X_L_M             = 0x04,
    LSM303_REGISTER_MAG_OUT_Z_H_M             = 0x05,
    LSM303_REGISTER_MAG_OUT_Z_L_M             = 0x06,
    LSM303_REGISTER_MAG_OUT_Y_H_M             = 0x07,
    LSM303_REGISTER_MAG_OUT_Y_L_M             = 0x08,
    LSM303_REGISTER_MAG_SR_REG_Mg             = 0x09,
    LSM303_REGISTER_MAG_IRA_REG_M             = 0x0A,
    LSM303_REGISTER_MAG_IRB_REG_M             = 0x0B,
    LSM303_REGISTER_MAG_IRC_REG_M             = 0x0C,
    LSM303_REGISTER_MAG_TEMP_OUT_H_M          = 0x31,
    LSM303_REGISTER_MAG_TEMP_OUT_L_M          = 0x32
} lsm303MagRegisters_t;

typedef enum {
    LSM303_MAGGAIN_1_3                        = 0x20,  // +/- 1.3
    LSM303_MAGGAIN_1_9                        = 0x40,  // +/- 1.9
    LSM303_MAGGAIN_2_5                        = 0x60,  // +/- 2.5
    LSM303_MAGGAIN_4_0                        = 0x80,  // +/- 4.0
    LSM303_MAGGAIN_4_7                        = 0xA0,  // +/- 4.7
    LSM303_MAGGAIN_5_6                        = 0xC0,  // +/- 5.6
    LSM303_MAGGAIN_8_1                        = 0xE0   // +/- 8.1
} lsm303MagGain;

#define SIZE_MUX_CHANNEL_LSM303 1    //! \brief Constant to define the mux channel for the LSM303

const uint8_t MUX_CHANNEL_LSM303[] = {TWI_MUX_ECOMPASS};
const uint8_t POWERUP_ACCEL[] = {LSM303_REGISTER_ACCEL_CTRL_REG1_A, 0x27}; // 10 Hz, Normal power
const uint8_t         ACCEL[] = {0x28|0x80}; // 0x28 | 0x80 for auto increment of registers
const uint8_t POWERUP_MAG[]   = {LSM303_REGISTER_MAG_MR_REG_M, 0x00}; // Continuous
const uint8_t         MAG[]   = {0x03|0x80}; // 0x03 | 0x80 for auto increment of registers
const uint8_t        TEMP[]   = {0x31|0x80}; // 0x31 | 0x80 for auto increment of registers

typedef struct AccelData_s {
    int16_t x;
    int16_t y;
    int16_t z;
} AccelData;

typedef struct MagData_s {
    int16_t x;
    int16_t y;
    int16_t z;
} MagData;

#define rad2deg 57.2957795131  // 180.0/3.14159265358979323846

# ifdef COMPILE_OBSOLETE
static int16_t cal_minx = 0;
static int16_t cal_miny = 0;
static int16_t cal_minz = 0;
static int16_t cal_maxx = 0;
static int16_t cal_maxy = 0;
static int16_t cal_maxz = 0;

static F32     cal_offP = 0;
static F32     cal_offR = 0;
# endif

/*
 * \brief Initialize the LSM303 Ecompass
 * Power on the accelerometers and magnetometers and set the sample rates.
 *
 * NOTE that there is a short delay before the readings start.
 *
 * \retval LSM303_OK        If successful initializing TWI-mux and powering up Ecompass
 * \retval LSM303_IO_ERROR  If fail
 */
int lsm303_init( void ) {

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    //
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        // io_out_string("\r\nError setting up TWI mux set to write to LM303 Ecompass.");
        return LSM303_IO_ERROR;
    }

    // Power up accelerometers
    //
    if ( STATUS_OK != twim_write(LSM303_TWI_PORT, POWERUP_ACCEL, 2, LSM303_ADDRESS_ACCEL, LSM303_TENBIT) ) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

    // Check the value written
    //  twim_write(LSM303_TWI_PORT, POWERUP, 1, LSM303_ADDRESS_ACCEL, LSM303_TENBIT);
    //  uint8_t read_data[6];
    //  twim_read(LSM303_TWI_PORT, read_data, 1, LSM303_ADDRESS_ACCEL, LSM303_TENBIT);

    // Power up magnetometers
    //
    if ( STATUS_OK != twim_write(LSM303_TWI_PORT, POWERUP_MAG, 2, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

    return LSM303_OK;
}


/*
 * \brief Deinitialize the LSM303 Ecompass
 * Power off the accelerometers and magnetometers.
 *
 * \retval LSM303_OK        If successful deinitializing TWI-mux and powering down Ecompass
 * \retval LSM303_IO_ERROR  If fail
 */
int lsm303_deinit( void ) {
# warning "lsm303_deinit() not functional."
# if 0
    int status;

    // TODO Power down accelerometers
    status = twim_write(LSM303_TWI_PORT, POWERUP_ACCEL, 2, LSM303_ADDRESS_ACCEL, LSM303_TENBIT);
    if (status!=STATUS_OK) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

    // TODO Power dowm magnetometers
    status = twim_write(LSM303_TWI_PORT, POWERUP_MAG, 2, LSM303_ADDRESS_MAG, LSM303_TENBIT);
    if (status!=STATUS_OK) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

# endif

    return LSM303_OK;
}

/*
 * \brief Read the LSM303 accelerometers
 * 
 * \param nAvg  number of readings to average over
 * \param raw   data structure to return the (averaged) reading in
 *
 * \retval LSM303_OK        If successful reading accelerometers
 * \retval LSM303_IO_ERROR  If failed
 */
int lsm303_get_accel_raw ( int nAvg, lsm303_Accel_Raw_t* raw ) {

    //  Silently fix invalid / unreasonable nAvg values
    if (nAvg< 1) nAvg =  1;
    if (nAvg>20) nAvg = 20;

    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_z = 0;

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        return LSM303_IO_ERROR;
    }

    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        return LSM303_IO_ERROR;
    }

    int16_t i;

    for ( i=0; i<nAvg; i++ ) {

        // vTaskDelay((portTickType)TASK_DELAY_MS(50));

        // Read the accelerometer registers
        if ( STATUS_OK != twim_write(LSM303_TWI_PORT, ACCEL, 1, LSM303_ADDRESS_ACCEL, LSM303_TENBIT) ) {
            return LSM303_IO_ERROR;
        }

        uint8_t read_data[6];
        if (STATUS_OK != twim_read(LSM303_TWI_PORT, read_data, 6, LSM303_ADDRESS_ACCEL, LSM303_TENBIT) ) {
            return LSM303_IO_ERROR;
        }

        // Shift values to create properly formed integer
        // Note low before high (different order than magnetometers)
        int16_t const x = ((read_data[1] << 8) | read_data[0]);
        int16_t const y = ((read_data[3] << 8) | read_data[2]);
        int16_t const z = ((read_data[5] << 8) | read_data[4]);

# ifdef DEBUG
        io_out_S16( "Acc[%02hd] XYZ: ", i );
        io_out_S16( "  %4hx", x );
        io_out_S16( "  %4hx", y );
        io_out_S16( "  %4hx\r\n", z );
# endif

        sum_x += x;
        sum_y += y;
        sum_z += z;
    }

    raw->x = sum_x / nAvg;
    raw->y = sum_y / nAvg;
    raw->z = sum_z / nAvg;

    return LSM303_OK;
}

int lsm303_calc_tilt ( lsm303_Accel_Raw_t* measured, lsm303_Accel_Raw_t* vertical, double mounting_angle,
        double* pitch, double* roll, double* tilt ) {

  if ( !measured
    || ( 0 == measured->x
      && 0 == measured->y
      && 0 == measured->z ) ) {
    if ( pitch ) *pitch = 90.0;
    if ( roll  ) *roll  = 90.0;
    if ( tilt  ) *tilt  = 90.0;
    return LSM303_ARG_ERROR;
  }

  double mx = measured->x;
  double my = measured->y;
  double mz = measured->z;

  double vx;
  double vy;
  double vz;

  if ( !vertical
    || 0 == vertical->x
    || 0 == vertical->y
    || 0 == vertical->z ) {

    vx = 0;
    vy = 0;
    vz = 1;
  } else {

    vx = vertical->x;
    vy = vertical->y;
    vz = vertical->z;
  }

  if ( tilt ) {
    //  tilt = angle between vertical acceleration and measured acceleration
    //  vector calculus (dot product): a dot b = |a| x |b| x cos(angle)
    //
    double abs_m = sqrt( mx*mx + my*my + mz*mz );
    double abs_v = sqrt( vx*vx + vy*vy + vz*vz );
    double m_dot_v = mx*vx + my*vy + mz*vz;

    *tilt = rad2deg * acos ( m_dot_v / ( abs_m*abs_v) );
  }

  //  Rotate to get the y axis parallel to the radiometer arms.
  //
  vector3d_t a_meas = { .x = mx, .y = my, .z = mz };
  vector3d_t a_vert = { .x = vx, .y = vy, .z = vz };

  vector3d_t a_meas_rot;
  vector3d_t a_vert_rot;

  if ( mounting_angle ) {
      a_meas_rot = rot_around_z( a_meas, mounting_angle*M_PI/180.0 );
      a_vert_rot = rot_around_z( a_vert, mounting_angle*M_PI/180.0 );
  } else {
      a_meas_rot = a_meas;
      a_vert_rot = a_vert;
  }

  //  Rotate the measured acceleration.
  //  The rotation is the one that rotates the vertical acceleration to (x=0,y=0,z=1).
  //  Hence the true acceleration becomes the offset from that direction.
  //
  vector3d_t a_true = rot_meas_by_vert ( a_meas_rot, a_vert_rot );

  //  Calculate pitch and roll angles
  //
  if ( pitch ) *pitch = rad2deg * atan( a_true.x / sqrt( a_true.y*a_true.y + a_true.z*a_true.z ) );
  if ( roll  ) *roll  = rad2deg * atan( a_true.y / sqrt( a_true.x*a_true.x + a_true.z*a_true.z ) );

  return LSM303_OK;
}

# ifdef COMPILE_OBSOLETE
/*
 * \brief Read the LSM303 accelerometers and calculate pitch and roll
 * 
 * \param pitch in degrees from vertical (-180..+180)
 * \param roll  in degrees from vertical (-180..+180)
 *
 * \retval LSM303_OK         If successful reading accelerometers
 * \retval LSM303_IO_ERROR   If fail
 */
int lsm303_read_accel(double* LSM303_pitch, double* LSM303_roll) {

    uint8_t read_data[6];
    AccelData accel;

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    //
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        // io_out_string("\r\nError setting up TWI mux set to write to LM303 Ecompass.");
        return LSM303_IO_ERROR;
    }

    // Read the accelerometer registers
    //
    if ( STATUS_OK != twim_write(LSM303_TWI_PORT, ACCEL, 1, LSM303_ADDRESS_ACCEL, LSM303_TENBIT) ) {
        // io_out_string("\r\nError setting up LSM303");
        return LSM303_IO_ERROR;
    }

    if ( STATUS_OK != twim_read(LSM303_TWI_PORT, read_data, 6, LSM303_ADDRESS_ACCEL, LSM303_TENBIT) ) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

    // Shift values to create properly formed integer
    // Note low before high (different order than magnetometers)
    accel.x = (int16_t)((read_data[1] << 8) | read_data[0]);
    accel.y = (int16_t)((read_data[3] << 8) | read_data[2]);
    accel.z = (int16_t)((read_data[5] << 8) | read_data[4]);

//     io_out_string("\r\n");
//     io_out_X16((U16)accel.x, " Accel.x \r\n");
//     io_out_X16((U16)accel.y, " Accel.y \r\n");
//     io_out_X16((U16)accel.z, " Accel.z \r\n");

//     io_out_X16((U16)accel.x, " Accel.x \r\n");
//     io_out_X16((U16)accel.y, " Accel.y \r\n");
//     io_out_X16((U16)accel.z, " Accel.z \r\n");

    // Calculate pitch and roll
    double const ax = accel.x;
    double const ay = accel.y;
    double const az = accel.z;
    *LSM303_pitch = rad2deg * atan( ax / sqrt( ay*ay + az*az ) ) - cal_offP;
    *LSM303_roll  = rad2deg * atan( ay / sqrt( ax*ax + az*az ) ) - cal_offR;

    return LSM303_OK;
}
# endif


/*
 * \brief Read the LSM303 magnetometers and calculate heading
 * 
 * \param heading in degrees (0..360)
 *
 * \retval LSM303_OK        If successful reading magnetometers
 * \retval LSM303_IO_ERROR  If fail
 */
int lsm303_get_mag_raw ( int nAvg, lsm303_Mag_Raw_t* raw ) {

    //  Silently fix invalid / unreasonable nAvg values
    if (nAvg< 1) nAvg =  1;
    if (nAvg>20) nAvg = 20;

    int32_t sum_x = 0;
    int32_t sum_y = 0;
    int32_t sum_z = 0;

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        //io_out_string("\r\nError setting up TWI mux set to write to LM303 Ecompass.");
        return LSM303_IO_ERROR;
    }

    int16_t i;

    for ( i=0; i<nAvg; i++ ) {

        //vTaskDelay((portTickType)TASK_DELAY_MS(50));

        // Read the magnetometer registers
        if ( STATUS_OK != twim_write(LSM303_TWI_PORT, MAG, 1, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
        	return LSM303_IO_ERROR;
    	}

    	uint8_t read_data[6];
        if ( STATUS_OK != twim_read(LSM303_TWI_PORT, read_data, 6, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
        	return LSM303_IO_ERROR;
    	}

    	// Shift values to create properly formed integer
    	// Note high before low (different order than accelerometers)
    	// Note X-Z-Y order
    	int16_t const x = ((read_data[0] << 8) | read_data[1]);
    	int16_t const z = ((read_data[2] << 8) | read_data[3]);
    	int16_t const y = ((read_data[4] << 8) | read_data[5]);

# ifdef DEBUG
        io_out_S16( "Mag[%02hd] XYZ: ", i );
        io_out_S16( "  %4hx", x );
        io_out_S16( "  %4hx", y );
        io_out_S16( "  %4hx\r\n", z );
# endif

        sum_x += x;
        sum_y += y;
        sum_z += z;
    }

    raw->x = sum_x / nAvg;
    raw->y = sum_y / nAvg;
    raw->z = sum_z / nAvg;

    return LSM303_OK;
}


int lsm303_calc_heading ( lsm303_Mag_Raw_t* raw, lsm303_Mag_Raw_t* min, lsm303_Mag_Raw_t* max,
	       double mounting_angle, double* heading )  {

    // Calibrate the raw readings
    //
    double magx = (((double)raw->x - (double)min->x) / ((double)max->x - (double)min->x)) - 0.5;
    double magy = (((double)raw->y - (double)min->y) / ((double)max->y - (double)min->y)) - 0.5;
//  double magz = (((double)raw->z - (double)min->z) / ((double)max->z - (double)min->z)) - 0.5;

    // Calculate heading
    //
    *heading = rad2deg * atan2( magy, magx );
    *heading -= mounting_angle;   //  To account for the offset between mounting and housing forward
    if ( *heading < 0 ) *heading += 360.0;

    return LSM303_OK;
}

# ifdef COMPILE_OBSOLETE
/*
 * \brief Read the LSM303 magnetometers and calculate heading
 * 
 * \param heading in degrees (0..360)
 *
 * \retval LSM303_OK        If successful reading magnetometers
 * \retval LSM303_IO_ERROR  If fail
 */
int lsm303_read_mag(double *LSM303_heading) {

    uint8_t read_data[6];
    MagData mag;

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    //
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        // io_out_string("\r\nError setting up TWI mux set to write to LM303 Ecompass.");
        return LSM303_IO_ERROR;
    }

    // Read the magnetometer registers
    //
    if ( STATUS_OK != twim_write(LSM303_TWI_PORT, MAG, 1, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
        // io_out_string("\r\nError setting up LSM303");
        return LSM303_IO_ERROR;
    }

    if ( STATUS_OK != twim_read(LSM303_TWI_PORT, read_data, 6, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
        // io_out_string("\r\nError reading from LSM303\r\n");
        return LSM303_IO_ERROR;
    }

    // Shift values to create properly formed integer
    // Note high before low (different order than accelerometers)
    // Note X-Z-Y order
    mag.x = (int16_t)((read_data[0] << 8) | read_data[1]);
    mag.z = (int16_t)((read_data[2] << 8) | read_data[3]);
    mag.y = (int16_t)((read_data[4] << 8) | read_data[5]);

    // Calibrate the readings
    double doublemagx = (((double)mag.x - (double)cal_minx) / ((double)cal_maxx - (double)cal_minx)) - 0.5;
    double doublemagy = (((double)mag.y - (double)cal_miny) / ((double)cal_maxy - (double)cal_miny)) - 0.5;
    double doublemagz = (((double)mag.z - (double)cal_minz) / ((double)cal_maxz - (double)cal_minz)) - 0.5;

# if 0
    io_out_string("MAG XYZ ");
    io_dump_X16((U16)mag.x, " ");
    io_dump_X16((U16)mag.y, " ");
    io_dump_X16((U16)mag.z, "   ");

    io_out_S32("mag xyz %ld ", (S32)mag.x);
    io_out_S32("%ld ", (S32)mag.y);
    io_out_S32("%ld   ", (S32)mag.z);

    io_out_S32("min  %ld ", (S32)cal_minx);
    io_out_S32("%ld ", (S32)cal_miny);
    io_out_S32("%ld   ", (S32)cal_minz);

    io_out_S32("max  %ld ", (S32)cal_maxx);
    io_out_S32("%ld ", (S32)cal_maxy);
    io_out_S32("%ld\r\n", (S32)cal_maxz);

    io_out_F32("Mag.x dbl %5.2f\r\n", (F32)doublemagx);
    io_out_F32("Mag.y dbl %5.2f\r\n", (F32)doublemagy);
    io_out_F32("Mag.z dbl %5.2f\r\n", (F32)doublemagz);
# endif

    // Calculate heading
    //
    *LSM303_heading = rad2deg * atan2( doublemagy, doublemagx );
    *LSM303_heading -= 32.0;   //  To account for the offset between mounting and housing forward
    if ( *LSM303_heading < 0 ) *LSM303_heading += 360.0;

    return LSM303_OK;
}

CC_Union_t lsm303_getCalCoeff  ( Mag_CalCoef_t which ) {

  CC_Union_t rv;

  switch ( which ) {
  case Mag_CC_Min_X: rv.s16[0] = cal_minx; break;
  case Mag_CC_Max_X: rv.s16[0] = cal_maxx; break;
  case Mag_CC_Min_Y: rv.s16[0] = cal_miny; break;
  case Mag_CC_Max_Y: rv.s16[0] = cal_maxy; break;
  case Mag_CC_Min_Z: rv.s16[0] = cal_minz; break;
  case Mag_CC_Max_Z: rv.s16[0] = cal_maxz; break;
  case Tlt_CC_Off_P: rv.f32    = cal_offP; break;
  case Tlt_CC_Off_R: rv.f32    = cal_offR; break;
  default: rv.s16[0] = rv.s16[1] = 0;
  }

  return rv;
}

int lsm303_setCalCoeff  ( CC_Union_t coeff, Mag_CalCoef_t which ) {

  switch ( which ) {
  case Mag_CC_Min_X: cal_minx = coeff.s16[0]; break;
  case Mag_CC_Max_X: cal_maxx = coeff.s16[0]; break;
  case Mag_CC_Min_Y: cal_miny = coeff.s16[0]; break;
  case Mag_CC_Max_Y: cal_maxy = coeff.s16[0]; break;
  case Mag_CC_Min_Z: cal_minz = coeff.s16[0]; break;
  case Mag_CC_Max_Z: cal_maxz = coeff.s16[0]; break;
  case Tlt_CC_Off_P: cal_offP = coeff.f32   ; break;
  case Tlt_CC_Off_R: cal_offR = coeff.f32   ; break;
  }

  return LSM303_OK;
}

int lsm303_calibrate_mag( int16_t calCoeffs[] ) {

    uint8_t read_data[6];
    MagData mag;

    // Set up the TWI mux to transmit on the LSM303 Ecompass channel
    if ( STATUS_OK != twim_write(TWI_MUX_PORT, MUX_CHANNEL_LSM303, SIZE_MUX_CHANNEL_LSM303, TWI_MUX_ADDRESS, TWI_MUX_TENBIT) ) {
        // io_out_string("\r\nError setting up TWI mux set to write to LM303 Ecompass.");
        return LSM303_IO_ERROR;
    }

    int16_t tmp_minx =  32767;
    int16_t tmp_maxx = -32767;
    int16_t tmp_miny =  32767;
    int16_t tmp_maxy = -32767;
    int16_t tmp_minz =  32767;
    int16_t tmp_maxz = -32767;

    for (int count=0; count<500; count++) {

        // Read the magnetometer registers
	//
        if ( STATUS_OK != twim_write(LSM303_TWI_PORT, MAG, 1, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
            // io_out_string("\r\nError setting up LSM303");
            return LSM303_IO_ERROR;
        }

        if ( STATUS_OK != twim_read(LSM303_TWI_PORT, read_data, 6, LSM303_ADDRESS_MAG, LSM303_TENBIT) ) {
            // io_out_string("\r\nError reading from LSM303\r\n");
            return LSM303_IO_ERROR;
        }

        // Shift values to create properly formed integer
        // Note high before low (different order than accelerometers)
        mag.x = (int16_t)((read_data[0] << 8) | read_data[1]);
        mag.y = (int16_t)((read_data[2] << 8) | read_data[3]);
        mag.z = (int16_t)((read_data[4] << 8) | read_data[5]);

//      // Convert from twos complement
//      if ( mag.x & 0x8000 ) mag.x = -1*(~mag.x + 1);
//      if ( mag.y & 0x8000 ) mag.y = -1*(~mag.y + 1);
//      if ( mag.z & 0x8000 ) mag.z = -1*(~mag.z + 1);

        // Update the limits
        if (mag.x < tmp_minx) tmp_minx = mag.x;
        if (mag.x > tmp_maxx) tmp_maxx = mag.x;
        if (mag.y < tmp_miny) tmp_miny = mag.y;
        if (mag.y > tmp_maxy) tmp_maxy = mag.y;
        if (mag.z < tmp_minz) tmp_minz = mag.z;
        if (mag.z > tmp_maxz) tmp_maxz = mag.z;

        // Visual
        io_out_S32("%ld:  ", count);
        io_out_S32("X %ld ",     tmp_minx);
        io_out_S32( " %ld ",     tmp_maxx);
        io_out_S32("Y %ld ",     tmp_miny);
        io_out_S32( " %ld ",     tmp_maxy);
        io_out_S32("Z %ld  ",    tmp_minz);
        io_out_S32( " %ld \r\n", tmp_maxz);
        
        vTaskDelay(100);
    }

    calCoeffs[0] = tmp_minx;
    calCoeffs[1] = tmp_maxx;
    calCoeffs[2] = tmp_miny;
    calCoeffs[3] = tmp_maxy;
    calCoeffs[4] = tmp_minz;
    calCoeffs[5] = tmp_maxz;

    return LSM303_OK;
}
# endif

