# include "command.spectrometer.h"
# include "errorcodes.h"

// Standard C
//
# include <stdio.h>
# include <ctype.h>
# include <string.h>
# include <stdlib.h>
# include <time.h>
# include <sys/time.h>
# include <math.h>

# include <compiler.h>

//  Atmel Software Framework
# include "flashc.h"
# include "wdt.h"
# include "sysclk.h"
# include "FreeRTOS.h"
# include "task.h"
# include "queue.h"

# include "gplp_trace.spectrometer.h"

//  avr32 resource library
# include "avr32rerrno.h"


# include "extern.spectrometer.h"

// Locally coded

# include "hypernav.sys.spectrometer.h"
# include "config.spectrometer.h"
# include "io_funcs.spectrometer.h"
// # include "telemetry.h"
# include "version.spectrometer.h"
# include "tasks.spectrometer.h"
# include "data_exchange_packet.h"
# include "data_exchange.spectrometer.h"
# include "data_acquisition.h"
# include "setup.spectrometer.h"
# include "info.spectrometer.h"
# include "systemtime.spectrometer.h"

// SPI
#include "board.h"
#include "spi.h"
#include "gpio.h"
// RTC
#include <ast.h>

// Tec-5/zeiss ccd
#include "cgs.h"

// external SRAM and FIFO support
#include "smc_sram.h"
#include "smc_fifo_A.h"
#include "smc_fifo_B.h"
#include "smc.h"

//for TWI testing
#include "twi_mux.h"
#include "components.h"
#include "lsm303.h"
#include "max6633.h"
#include "sram_memory_map.spectrometer.h"
#include "shutters.h"  // for shutter testing
#include "pressure.h"  // for pressure sensor testing
#include "FlashMemory.h"  // for Flash Memory testing

// for head accelerometers
#include "orientation.h"

// FIFOs used in Spectrometer->ADC->FIFO chain
# include "sn74v283.h"
# include "spi.spectrometer.h"

// packet receiving queue
# define N_RX_PACKETS 16
static xQueueHandle rxPackets = NULL;

//  Use RAM for data sending via packet transfer
typedef union {
  Config_Data_t          Config_Data;
} any_sending_data_t;
static data_exchange_data_package_t sending_data_package;


static S16 Cmd_SelfTest (void) {

  return Info_SensorDevices()
  ? CEC_Ok
  : CEC_Failed;
}

static S16 Cmd_Info ( char* option, char* result, S16 r_max_len ) {

  if ( 0 == strcasecmp( "FirmwareVersion", option ) ) {

    strncpy ( result,
                 HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_SPEC_FW_VERSION_PATCH,
    r_max_len-1 );
    return CEC_Ok;
  
  } else if ( 0 == strcasecmp("intclks", option) ) {

    snprintf(result, r_max_len, "CPU: %ld PBA: %ld PBB: %ld PBC: %ld\r\n", 
    sysclk_get_cpu_hz(), sysclk_get_pba_hz(), sysclk_get_pbb_hz(), sysclk_get_pbc_hz() );
    return CEC_Ok;

  } else {
  
    return CEC_Failed;
  }
}


//! Send user-inputed data over the SPI bus.
//  For testing only - SPI must be in master mode.
static S16 Cmd_SPItest (char* data) {

    int count;

    // Send data
    if ( spi_writeRegisterEmptyCheck( &AVR32_SPI0 ) ) {
        // Select chip.  Second argument hardwires for spec board.
        spi_selectChip( &AVR32_SPI0, 0);
        int dataLength = strlen(data);
        for (count=0; count<dataLength; ) {
            spi_write( &AVR32_SPI0, data[count++] );
        }
        spi_unselectChip( &AVR32_SPI0, 0);
    }

    return CEC_Ok;

}

static S16 Cmd_SPIpins ( char* option, char* value ) {
# if 0
  if ( option ) {
    if ( 0 == strcasecmp ( "TX", option ) ) {
      Bool yn = ( value && value[0] == '1' ) ? true : false; SPI_I_Want_To_Send(yn); io_out_string ( yn ? "yesTX\r\n" : "noTX\r\n" );
    } else if ( 0 == strcasecmp( "RX", option ) ) {
      Bool yn = ( value && value[0] == '1' ) ? true : false; SPI_I_Can_Receive (yn); io_out_string ( yn ? "yesRX\r\n" : "noRX\r\n" );
    } else {
      io_out_string( SPI_You_Want_To_Send() ? "Other TX\r\n" : "Other not TX\r\n" );
      io_out_string( SPI_You_Can_Receive () ? "Other RX\r\n" : "Other not RX\r\n" );
    }
  } else {
      io_out_string ( "NooP\r\n" );
  }
# else
      io_out_string ( "NOOP\r\n" );
# endif
  return CEC_Ok;
}

// headtilt port|star nAvg
static S16 Cmd_SpecTemp( char* option, char* value )
{
    float T = 0;
    int  rv = 0;

    int delay_sec = (value && value[0]) ? atoi(value) : 0;
    if ( delay_sec>30 ) delay_sec = 30;

    while ( delay_sec>0 ) {
        delay_sec--;
        char out[2]; out[0]=(delay_sec%10)+'0'; out[1]=0;
        io_out_string ( out );
        vTaskDelay((portTickType)TASK_DELAY_MS(1000));
    }

    switch ( option [0] ) {

    case 'a':
    case 'A':
    case 'p':
    case 'P': rv = max6633_measure ( TWI_MUX_SPEC_TEMP_A, &T );
              break;

    case 'b':
    case 'B':
    case 's':
    case 'S': rv = max6633_measure ( TWI_MUX_SPEC_TEMP_B, &T );
              break;

    default : io_out_string ( "Error: Need 's' or 'p' argument\r\n" );
              return CEC_Failed; 
    }

    if ( MAX6633_OK == rv ) 
	{
        io_out_F64(     "T = %.2f\r\n", (F64)T );
    } else {
        io_out_S16( "Measurement failed (err %hd)\r\n", (S16)rv );
    }

    return CEC_Ok;
}



// headtilt port|star nAvg
static S16 Cmd_MeasureHeadTilt( char* option, char* value)
{
	int nAvg = (value && value[0]) ? atoi ( value ) : 10;
	if ( nAvg <  1 ) nAvg =  1;
	if ( nAvg > 50 ) nAvg = 50;

	// not using averaging yet

	switch ( option[0] ) {

        case 'a': case 'A': case 'p': case 'P':
		if ( ORIENT_OK != ORIENT_TestAccelerometer(PORT_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_3_125Hz) ) {
			return CEC_Failed;
		}
		break;

        case 'b': case 'B': case 's': case 'S':
		if ( ORIENT_OK != ORIENT_TestAccelerometer(STARBOARD_ACCELEROMETER_ADDRESS, LIS3DSH_ODR_3_125Hz) ) {
			return CEC_Failed;
		}
		break;

	default:
		io_out_string("Missing Arguments a/p/b/s \r\n"); // [B-Right-Starboard] [A-Left-Port]
		return CEC_Failed;
	}

	return CEC_Ok;
}

static S16 Cmd_TestTWIMux( char* option, char* value ) {

    int n = (option && option[0]) ? atoi ( option ) : 10;
    if ( n <  1 ) n =  1;
    if ( n > 20 ) n = 20;

    int delay = (value && value[0]) ? atoi ( value ) : 10;
    if ( delay <    0 ) delay =    0;
    if ( delay > 1000 ) delay = 1000;

    return (0==twi_mux_test(n,delay) ) ? CEC_Ok : CEC_Failed;
}

static S16 Cmd_MeasureTilt( char* option ) {

    int nAvg = (option && option[0]) ? atoi ( option ) : 10;
    if ( nAvg <  1 ) nAvg =  1;
    if ( nAvg > 50 ) nAvg = 50;

    // Initialize the two-wire interface -- Already taken care of in hardware initialization
    //twi_mux_start();
    // Initialize the tilt/compass
    lsm303_init();

    double pitch_sum = 0;
    double pitch2sum = 0;
    double  roll_sum = 0;
    double  roll2sum = 0;
    double  tilt_sum = 0;
    double  tilt2sum = 0;
    double nMeasures = 0;

    int i;

    for ( i=0; i<nAvg; i++ ) {

        vTaskDelay((portTickType)TASK_DELAY_MS( 3 ));

        lsm303_Accel_Raw_t acc_raw;
        if ( LSM303_OK == lsm303_get_accel_raw ( 1, &acc_raw ) ) {

            lsm303_Accel_Raw_t vertical = {
                     .x = CFG_Get_Acc_X(),
                     .y = CFG_Get_Acc_Y(),
                     .z = CFG_Get_Acc_Z() };

            double mounting_angle = CFG_Get_Acc_Mounting();
            double pitch;
            double roll;
            double tilt;

            if ( LSM303_OK == lsm303_calc_tilt ( &acc_raw, &vertical, mounting_angle,
                                                 &pitch, &roll, &tilt) ) {

              double const ax = acc_raw.x;
              double const ay = acc_raw.y;
              double const az = acc_raw.z;

              double naive_pitch = (180.0/M_PI) * atan( ax / sqrt( ay*ay + az*az ) );
              double naive_roll  = (180.0/M_PI) * atan( ay / sqrt( ax*ax + az*az ) );

              io_out_string( "Accelerometer Measured x Vertical x Mounting:" );
              io_out_S16( "  %6ld", acc_raw.x );
              io_out_S16( "  %6ld", acc_raw.y );
              io_out_S16( "  %6ld", acc_raw.z );
              io_out_S16( "  x  %6ld", vertical.x );
              io_out_S16( "  %6ld", vertical.y );
              io_out_S16( "  %6ld", vertical.z );
              io_out_F64( "  x  %.2f", mounting_angle );
              io_out_F64( "  -> PR %.2f", pitch );
              io_out_F64(    "  %.2f", roll );
              io_out_F64(   " T %.2f", tilt );
              io_out_F64( " Naive PR %.2f", naive_pitch );
              io_out_F64(    "  %.2f\r\n", naive_roll );

        pitch_sum += pitch;
        pitch2sum += pitch*pitch;
         roll_sum += roll;
         roll2sum += roll*roll;
               tilt_sum +=  tilt;
               tilt2sum +=  tilt*tilt;
              nMeasures += 1;

            } else {
                io_out_string( "Tilt calculation FAILED\r\n" );
      }
  
        } else {
            io_out_string( "Accelerometer FAILED\r\n" );
      }
    }

    if ( nMeasures>1 ) {
      pitch_sum /= nMeasures;
      pitch2sum /= nMeasures;
      pitch2sum -= pitch_sum*pitch_sum;
      pitch2sum  = (pitch2sum>0) ? sqrt( pitch2sum ) : 0;
       roll_sum /= nMeasures;
       roll2sum /= nMeasures;
       roll2sum -=  roll_sum*roll_sum;
       roll2sum  = ( roll2sum>0) ? sqrt(  roll2sum ) : 0;
       tilt_sum /= nMeasures;
       tilt2sum /= nMeasures;
       tilt2sum -=  tilt_sum*tilt_sum;
       tilt2sum  = ( tilt2sum>0) ? sqrt(  tilt2sum ) : 0;
    }

    // Print the results
    io_out_F64("Pitch: %7.2f ", pitch_sum ); io_out_F64("+/- %5.2f deg", pitch2sum ); io_out_F64(" [%.0f]\r\n", nMeasures );
    io_out_F64("Roll:  %7.2f ",  roll_sum ); io_out_F64("+/- %5.2f deg",  roll2sum ); io_out_F64(" [%.0f]\r\n", nMeasures );
    io_out_F64("Tilt:  %7.2f ",  tilt_sum ); io_out_F64("+/- %5.2f deg",  tilt2sum ); io_out_F64(" [%.0f]\r\n", nMeasures );

    return CEC_Ok;
}

static S16 Cmd_CalAccelerometer( char* option ) {

    int nAvg = (option && option[0]) ? atoi ( option ) : 10;
    if ( nAvg <  1 ) nAvg =  1;
    if ( nAvg > 50 ) nAvg = 50;

    // Initialize the two-wire interface -- Already taken care of in hardware initialization
    //twi_mux_start();
    // Initialize the tilt/compass
    lsm303_init();

    lsm303_Accel_Raw_t acc_raw;
    if ( LSM303_OK == lsm303_get_accel_raw ( nAvg, &acc_raw ) ) {

        double const ax = acc_raw.x;
        double const ay = acc_raw.y;
        double const az = acc_raw.z;

        double pitch = (180.0/M_PI) * atan( ax / sqrt( ay*ay + az*az ) );
        double roll  = (180.0/M_PI) * atan( ay / sqrt( ax*ax + az*az ) );

        io_out_S32( "Accelerometer[nAvg=%ld] XYZ:", (S32)nAvg );
        io_out_S32( "  %6ld", (S32)acc_raw.x );
        io_out_S32( "  %6ld", (S32)acc_raw.y );
        io_out_S32( "  %6ld", (S32)acc_raw.z );
        io_out_F64( "-> Pitch,Roll  %.2f", pitch );
        io_out_F64(              "  %.2f\r\n", roll );

    } else {
        io_out_string( "Accelerometer FAILED\r\n" );
    }

    return CEC_Ok;
}

static S16 Cmd_MeasureHeading( char* option ) {

    int nAvg = (option && option[0]) ? atoi ( option ) : 10;
    if ( nAvg <  1 ) nAvg =  1;
    if ( nAvg > 50 ) nAvg = 50;

    // Initialize the two-wire interface -- Already taken care of in hardware initialization
    //twi_mux_start();
    // Initialize the tilt/compass
    lsm303_init();

    double  hdng_sum = 0;
    double  hdng2sum = 0;
    double nMeasures = 0;

    int i;

    for ( i=0; i<nAvg; i++ ) {

        vTaskDelay((portTickType)TASK_DELAY_MS( 3 ));

        lsm303_Mag_Raw_t mag_raw;
        if ( LSM303_OK == lsm303_get_mag_raw ( 1, &mag_raw ) ) {

            lsm303_Mag_Raw_t mag_min = {
                     .x = CFG_Get_Mag_Min_X(),
                     .y = CFG_Get_Mag_Min_Y(),
                     .z = CFG_Get_Mag_Min_Z() };

            lsm303_Mag_Raw_t mag_max = {
                     .x = CFG_Get_Mag_Max_X(),
                     .y = CFG_Get_Mag_Max_Y(),
                     .z = CFG_Get_Mag_Max_Z() };

            double mounting_angle = CFG_Get_Acc_Mounting();
            double heading;

            if ( LSM303_OK == lsm303_calc_heading ( &mag_raw, &mag_min, &mag_max,
                                                   mounting_angle, &heading) ) {

              io_out_string( "Magnetometer Measured x Min/Max x Mounting:" );
              io_out_S16( "  %6ld", mag_raw.x );
              io_out_S16( "  %6ld", mag_raw.y );
              io_out_S16( "  %6ld", mag_raw.z );
              io_out_S16( "  x  (%6ld", mag_min.x );
              io_out_S16( "  %6ld)", mag_max.x );
              io_out_S16(    "  (%6ld", mag_min.y );
              io_out_S16( "  %6ld)", mag_max.y );
              io_out_S16(    "  (%6ld", mag_min.z );
              io_out_S16( "  %6ld)", mag_max.z );
              io_out_F64( "  x  %.2f", mounting_angle );
              io_out_F64( "  -> H %.2f\r\n", heading );

               hdng_sum +=  heading;
               hdng2sum +=  heading*heading;
              nMeasures += 1;

            } else {
                io_out_string( "Heading calculation FAILED\r\n" );
            }

        } else {
            io_out_string( "Magnetometer FAILED\r\n" );
        }
    }

    if ( nMeasures>1 ) {
       hdng_sum /= nMeasures;
       hdng2sum /= nMeasures;
       hdng2sum -=  hdng_sum*hdng_sum;
       hdng2sum  = ( hdng2sum>0) ? sqrt(  hdng2sum ) : 0;
    }

    // Print the results
    io_out_F64("Heading:  %7.2f ",  hdng_sum ); io_out_F64("+/- %5.2f deg",  hdng2sum ); io_out_F64(" [%.0f]\r\n", nMeasures );

    return CEC_Ok;
}

static S16 Cmd_CalCompass( char* option )
{
    int nMeasure = (option && option[0]) ? atoi ( option ) : 100;
    if ( nMeasure <  20 ) nMeasure =  20;
    if ( nMeasure > 500 ) nMeasure = 500;

    // Initialize the tilt/compass
    lsm303_init();

    int i;

    static lsm303_Mag_Raw_t mag_max = { .x=-32768, .y=-32768, .z=-32768 };
    static lsm303_Mag_Raw_t mag_min = { .x= 32767, .y= 32767, .z= 32767 };

              io_out_string( "  initial" );
              io_out_S16 ( " [ %6hd ",  mag_min.x );
              io_out_S16 ( ".. %6hd ]", mag_max.x );
              io_out_S16 ( " [ %6hd ",  mag_min.y );
              io_out_S16 ( ".. %6hd ]", mag_max.y );
              io_out_S16 ( " [ %6hd ",  mag_min.z );
              io_out_S16 ( ".. %6hd ]", mag_max.z );
              io_out_string( "\r\n" );

    for ( i=0; i<nMeasure; i++ ) {

        vTaskDelay((portTickType)TASK_DELAY_MS( 250 ));
        gHNV_SetupCmdSpecTask_Status = TASK_RUNNING;

        lsm303_Mag_Raw_t mag_raw;
        if ( LSM303_OK == lsm303_get_mag_raw ( 1, &mag_raw ) ) {

            int changed = 0;
 
            if ( mag_raw.x < mag_min.x ) { mag_min.x = mag_raw.x; changed = 1; }
            if ( mag_raw.x > mag_max.x ) { mag_max.x = mag_raw.x; changed = 1; }
            if ( mag_raw.y < mag_min.y ) { mag_min.y = mag_raw.y; changed = 1; }
            if ( mag_raw.y > mag_max.y ) { mag_max.y = mag_raw.y; changed = 1; }
            if ( mag_raw.z < mag_min.z ) { mag_min.z = mag_raw.z; changed = 1; }
            if ( mag_raw.z > mag_max.z ) { mag_max.z = mag_raw.z; changed = 1; }

            if ( changed ) {
              io_out_string( "  changed" );
              io_out_S16 ( " [ %6hd ",  mag_min.x );
              io_out_S16 ( ".. %6hd ]", mag_max.x );
              io_out_S16 ( " [ %6hd ",  mag_min.y );
              io_out_S16 ( ".. %6hd ]", mag_max.y );
              io_out_S16 ( " [ %6hd ",  mag_min.z );
              io_out_S16 ( ".. %6hd ]", mag_max.z );
              io_out_string( "\r\n" );
            }
        }
    }

              io_out_string( "    final" );
              io_out_S16 ( " [ %6hd ",  mag_min.x );
              io_out_S16 ( ".. %6hd ]", mag_max.x );
              io_out_S16 ( " [ %6hd ",  mag_min.y );
              io_out_S16 ( ".. %6hd ]", mag_max.y );
              io_out_S16 ( " [ %6hd ",  mag_min.z );
              io_out_S16 ( ".. %6hd ]", mag_max.z );
              io_out_string( "\r\n" );

    return CEC_Ok;
}

static S16 Cmd_ShutterTest(void)
{
    io_out_string("This test takes 12 seconds to complete.");

    shutters_power_on();

    // Cycle the shutters 4 times
    for(int i=0;i<4;i++) {
        io_out_string( "Opening\r\n" );
        shutter_A_open();
        shutter_B_open();
        vTaskDelay(1500);
        io_out_string( "Closing\r\n" );
        shutter_A_close();
        shutter_B_close();
        vTaskDelay(1500);
    }

    shutters_power_off();

    return CEC_Ok;
}


static S16 Cmd_PressureTest( char* option, char* value )
{

    PRES_power(true);
    vTaskDelay(1000);
    if ( PRES_Initialize() ) {
            io_out_string("Cannot start pressure sensor\r\n");
            return CEC_Failed;
    }
    vTaskDelay(500);

    int nMeasure = (option && option[0]) ? atoi ( option ) : 10;
    if ( nMeasure < 1 ) nMeasure = 1;
    if ( nMeasure > 1000 ) nMeasure = 1000;

    int delay = (value && value[0]) ? atoi ( value ) : 250;
    if ( delay < 1 ) delay = 1;
    if ( delay > 20000 ) delay = 20000;

    DigiQuartz_Calibration_Coefficients_t dq_cc = {
      .U0 = CFG_Get_Pres_U0(),
      .Y1 = CFG_Get_Pres_Y1(),
      .Y2 = CFG_Get_Pres_Y2(),
      .Y3 = CFG_Get_Pres_Y3(),
      .C1 = CFG_Get_Pres_C1(),
      .C2 = CFG_Get_Pres_C2(),
      .C3 = CFG_Get_Pres_C3(),
      .D1 = CFG_Get_Pres_D1(),
      .D2 = CFG_Get_Pres_D2(),
      .T1 = CFG_Get_Pres_T1(),
      .T2 = CFG_Get_Pres_T2(),
      .T3 = CFG_Get_Pres_T3(),
      .T4 = CFG_Get_Pres_T4(),
      .T5 = CFG_Get_Pres_T5()
    };

    double Tfreq_Sum[2] = { 0, 0 };
    double Pfreq_Sum[2] = { 0, 0 };
    double T_C___Sum[2] = { 0, 0 };
    double P_db__Sum[2] = { 0, 0 };
    double nTP          = 0;

    while ( nTP<nMeasure ) {

        int needCRLF = 0;

        digiquartz_t sensortype;

        F64 Tper;
        int Tcounts;
        U32 Tduration;
//         if (PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE) == PRES_Ok) {
//             vTaskDelay(250);
// #if (MCU_BOARD_REV == REV_A)
//             while (PRES_GetPerioduSecs(&sensortype, &Tper, &Tcounts, &Tduration, CFG_Get_Pres_TDiv() ) != PRES_Ok) { io_out_string( "T" ); needCRLF = 1; /* try again */ }
// #else
// 			while (PRES_GetPerioduSecs(DIGIQUARTZ_TEMPERATURE, &sensortype, &Tper, &Tcounts, &Tduration, CFG_Get_Pres_TDiv() ) != PRES_Ok) { io_out_string( "T" ); needCRLF = 1; /* try again */ }
// #endif
//         } else {
//             io_out_string("Failed to start T measurement\r\n");
//             return CEC_Failed;
//         }
// 
        F64 Pper;
        int Pcounts;
        U32 Pduration;
// 
//         if (PRES_StartMeasurement(DIGIQUARTZ_PRESSURE) == PRES_Ok) {
//             vTaskDelay(500);
// #if (MCU_BOARD_REV == REV_A)
//             while (PRES_GetPerioduSecs(&sensortype, &Pper, &Pcounts, &Pduration, CFG_Get_Pres_PDiv() ) != PRES_Ok) { io_out_string( "P" ); needCRLF = 1; /* try again */ }
// #else
// 			while (PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE, &sensortype, &Pper, &Pcounts, &Pduration, CFG_Get_Pres_PDiv() ) != PRES_Ok) { io_out_string( "P" ); needCRLF = 1; /* try again */ }
// #endif
//         } else {
//             io_out_string("Failed to start P measurement\r\n");
//             return CEC_Failed;
//         }
// 
//         if ( needCRLF ) { io_out_string( "\r\n" ); }

		PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
		PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
		vTaskDelay(delay);
		PRES_GetPerioduSecs(DIGIQUARTZ_TEMPERATURE, &sensortype, &Tper, &Tcounts, &Tduration, CFG_Get_Pres_TDiv() );
		PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE, &sensortype, &Pper, &Pcounts, &Pduration, CFG_Get_Pres_PDiv() );

        // we now have periods, calculate pressure
        F64 TdegC, Ppsia;
        F64 PdBar = PRES_Calc_Pressure_dBar(Tper, Pper, &dq_cc, &TdegC, &Ppsia);

        io_out_S32("T %ld cnt ", (S32)Tcounts );
        io_out_S32( " %ld clk ", (S32)Tduration);
        io_out_F64( " %.6lf us ", Tper);
        io_out_F64( " %.4lf Hz  ", 1000000.0/Tper);
        io_out_S32("P %ld cnt ", (S32)Pcounts );
        io_out_S32( " %ld clk ", (S32)Pduration);
        io_out_F64( " %.6lf us ", Pper);
        io_out_F64( " %.4lf Hz  ", 1000000.0/Pper);
        io_out_F64("T %.2lf C ", TdegC);
        io_out_F64("P %.3lf psia ", Ppsia);
        io_out_F64( " %.3lf dBar\r\n", PdBar);

        Tfreq_Sum[0] += (1000000.0/Tper);
        Tfreq_Sum[1] += (1000000.0/Tper)*(1000000.0/Tper);
        Pfreq_Sum[0] += (1000000.0/Pper);
        Pfreq_Sum[1] += (1000000.0/Pper)*(1000000.0/Pper);
		T_C___Sum[0] += TdegC;
		T_C___Sum[1] += TdegC*TdegC;
		P_db__Sum[0] += PdBar;
		P_db__Sum[1] += PdBar*PdBar;
        nTP          += 1;
    }

    if ( nTP > 0 ) {
      Tfreq_Sum[0] /= nTP;
      Tfreq_Sum[1] /= nTP;
      Tfreq_Sum[1] -= Tfreq_Sum[0]*Tfreq_Sum[0];
      Tfreq_Sum[1]  = (Tfreq_Sum[1]>0) ? sqrt(Tfreq_Sum[1]) : 0.0;
      Pfreq_Sum[0] /= nTP;
      Pfreq_Sum[1] /= nTP;
      Pfreq_Sum[1] -= Pfreq_Sum[0]*Pfreq_Sum[0];
      Pfreq_Sum[1]  = (Pfreq_Sum[1]>0) ? sqrt(Pfreq_Sum[1]) : 0.0;
      T_C___Sum[0] /= nTP;
      T_C___Sum[1] /= nTP;
      T_C___Sum[1] -= T_C___Sum[0]*T_C___Sum[0];
      T_C___Sum[1]  = (T_C___Sum[1]>0) ? sqrt(T_C___Sum[1]) : 0.0;
      P_db__Sum[0] /= nTP;
      P_db__Sum[1] /= nTP;
      P_db__Sum[1] -= P_db__Sum[0]*P_db__Sum[0];
      P_db__Sum[1]  = (P_db__Sum[1]>0) ? sqrt(P_db__Sum[1]) : 0.0;
    }

    io_out_F64 ( "Statistics %.0f measurements\r\n", nTP );
    io_out_F64 ( "Statistics T freq : %12.4f", Tfreq_Sum[0] );
    io_out_F64 (                " +/- %.4f", Tfreq_Sum[1] );
    io_out_F64 (                " (%.4f ppm)\r\n", 1000000.0*Tfreq_Sum[1]/Tfreq_Sum[0] );
    io_out_F64 ( "Statistics P freq : %12.4f", Pfreq_Sum[0] );
    io_out_F64 (                " +/- %.4f", Pfreq_Sum[1] );
    io_out_F64 (                " (%.4f ppm)\r\n", 1000000.0*Pfreq_Sum[1]/Pfreq_Sum[0] );
    io_out_F64 ( "Statistics T  C   : %12.4f", T_C___Sum[0] );
    io_out_F64 (                " +/- %.4f", T_C___Sum[1] );
    io_out_F64 (                " (%.4f %%)\r\n", 100.0*T_C___Sum[1]/T_C___Sum[0] );
    io_out_F64 ( "Statistics P dBar : %12.4f", P_db__Sum[0] );
    io_out_F64 (                " +/- %.4f", P_db__Sum[1] );
    io_out_F64 (                " (%.4f %%)\r\n", 100.0*P_db__Sum[1]/P_db__Sum[0] );

    return CEC_Ok;
}

# if 0
static S16 Cmd_ASTAlarmTest(void)
{
    U32 ast_now, alarm;
    char tmp[50];
    avr32_ast_pir1_t pir = {
        .insel = 21
    };
    // note that the clock source for the periodic timer is not the prescaled clock!!
    int i;

    ASTtimeout = false;

    io_out_string("Testing AST Alarm\r\n");

    // interrupts registered during ADC task creation
    //Disable_global_interrupt();
    //INTC_register_interrupt((__int_handler)&ASTtimeout_irq, AVR32_AST_ALARM_IRQ, 1);
    //Enable_global_interrupt();

    ast_disable_alarm_interrupt(&AVR32_AST, 0);
    ast_clear_alarm_status_flag(&AVR32_AST, 0);
    ast_disable_alarm0(&AVR32_AST);

    ast_now = ast_get_counter_value(&AVR32_AST);
    alarm = ast_now + 115200 + 115200;  // 2s
    ast_set_alarm0_value(&AVR32_AST, alarm);

    ast_enable_alarm_interrupt(&AVR32_AST,0);
    //ast_enable_alarm0(&AVR32_AST);

    snprintf(tmp, sizeof(tmp), "Now: %lu Alarm: %lu\r\n", ast_now, alarm);
    io_out_string(tmp);

    while (!ASTtimeout);

    ast_disable_alarm_interrupt(&AVR32_AST,0);

    ast_now = ast_get_counter_value(&AVR32_AST);
    snprintf(tmp, sizeof(tmp), "AST Alarm at %lu\r\n", ast_now);
    io_out_string(tmp);

    io_out_string("Testing periodic AST Alarm\r\n");

    ast_disable_periodic_interrupt(&AVR32_AST, 1);
    ast_clear_periodic_status_flag(&AVR32_AST, 1);
    ast_set_periodic1_value(&AVR32_AST, pir);
    ast_enable_periodic_interrupt(&AVR32_AST, 1);
    i = 0;
    while (i < 100) {       // until wdt reset??

        if (ASTperiodic1) {
            i++;
            ASTperiodic1 = false;
            io_out_string("!");
        }
    }

    return CEC_Ok;
}
# endif

static S16 Cmd_ExternalSRAMtest(void)
{
	U32 i;
	U16 j = 0;
    unsigned int noErrors = 0;
    volatile U16 *sram = SRAM_U16;
    struct timeval tvs, tve;
    char temp[75];

    io_out_string("Testing External SRAM - ");
	io_dump_U32((U32)SRAM_SIZE, " 16-bit words\r\n");		// 8 Mbit, organized as 512K x 16

    gettimeofday(&tvs, NULL);

	//  Fill SRAM with numbers
	//  j will over-run, due to SRAM size > 16bit
	//
    for( i=0; i<SRAM_SIZE; i++, j++) {
        sram[i] = j;						// write value to ext SRAM
	}

	//  Read back & compare
	//
    for( i=0; i<SRAM_SIZE; i++, j++) {

        if( j != sram[i] ) { // read back and compare

            noErrors++;
            io_out_string("ERROR at 0x"); io_dump_X32((U32) &sram[i], ", "); 
			io_out_string("wrote "), io_dump_U32((U32)j, " "); io_out_string("read "), io_dump_U32((U32)sram[i], "\r\n");

			if (noErrors >= 10) {
				io_out_string("Aborting test\r\n");
				break;
			}

        }
    }

    gettimeofday(&tve, NULL);

    io_out_string("SRAM test completed with ");
    io_dump_U32(noErrors, " errors out of ");
    io_dump_U32(SRAM_SIZE/1024, "kB tests\r\n");

    snprintf(temp, sizeof(temp), "Test start: %lu.%06lu End: %lu.%06lu s\r\n", tvs.tv_sec, tvs.tv_usec, tve.tv_sec, tve.tv_usec);
    io_out_string(temp);

    return CEC_Ok;
}



// usage: specpwr A|B ON|OFF
static S16 Cmd_SpecPWR (char *option, char *value) 
{
    Bool runA = false;
    Bool runB = false;

    if ( 0 == strcasecmp("A", option) ) {
        runA = true;
    } else if ( 0 == strcasecmp("B", option) ) {
        runB = true;
    } else if ( 0 == strcasecmp("BOTH", option) ) {
        runA = true;
        runB = true;
    } else if (0 == strcasecmp("GND", option)) {
    //  neither
    } else {
        io_out_string("Invalid spec\r\n");
        return CEC_Failed;
    }

    if ( 0 == strcasecmp("ON", value) ) {
        CGS_Power_On( runA, runB );
    io_out_string("Spec power on\r\n");
    } else if (0 == strcasecmp("OFF", value) ) {
        CGS_Power_Off(); io_out_string("Spec power off\r\n");
    } else {
        io_out_string("Invalid command\r\n");
        return CEC_Failed;
    }

    return CEC_Ok;
}



// usage: spectest A|B|{AB|BA|BOTH} value
//  value = [-]<inttime>[,nSamples[,nClearouts]]
static S16 Cmd_SpecTest (char* option, char* value)
{

    bool runA, runB;

    if ( 0 == option || 0 == *option ) {
        runA = true; runB = false;
    } else {
      if ( 0 == strcasecmp("A", option) ) {
        runA = true; runB = false;
      } else if ( 0 == strcasecmp("B", option) ) {
        runA = false; runB = true;
      } else if ( 0 == strcasecmp("AB", option)
               || 0 == strcasecmp("BA", option)
               || 0 == strcasecmp("*", option)
               || 0 == strcasecmp("BOTH", option) ) {
        runA = true; runB = true;
      } else {
        io_out_string("Invalid spec\r\n");
        return CEC_Failed;
      }
    }

    int printChannels = 1;
    F64 integration_time;
    int nSamples = 0;
    int nClearouts = 1;

    if ( 0 == value  || 0 == *value  ) {
        printChannels = 1;
        integration_time = 100;
    } else {

      if ( '-' == value[0] ) {
        printChannels = 0;
        value++;
	  } else if ( '+' == value[0] ) {
        printChannels = 2;
        value++;
      } else {
        printChannels = 1;
      }

      integration_time = strtod(value,NULL);

      char* comma = strchr ( value, ',' );

      if ( comma && comma[1] ) {
        nSamples = atoi ( comma+1 );
        comma = strchr ( comma+1, ',' );
        if ( comma && comma[1] ) {
           nClearouts = atoi ( comma+1 );
        }
      }
    }

    //  String for testing output
    //
	char fifo_state[48];

    //  Check FIFO before spectrometer activity
    //
	if(runA) {
             snprintf ( fifo_state, 48, "FIFO A %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_A) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_A) ? " H" : "nH",
                    SN74V283_IsFull    (component_A) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_A ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_A ) );
	}

	if(runB) {
			 snprintf ( fifo_state, 48, "FIFO B %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_B) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_B) ? " H" : "nH",
                    SN74V283_IsFull    (component_B) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_B ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_B ) );
	}

    // Spectrometer power
    //
    CGS_Power_On( runA, runB );  // power up requested spectrometers

    //  Spectrometer software setup:
	//  ADCs, FIFOs, clock, interrupts, timers.
    //
	CGS_Initialize( runA, runB, /* autoClearing = */ false );
    vTaskDelay(500);
    //
    // at this point, clock should be reaching both specs.

	//  Confirm there is no data in the FIFO before starting the spectrometers
	//
	if ( 0 ) {  //  Now done inside SN74V283_Start() which is called from CGS_Initialize()
	int rot = 5;
	int cnt = 0;
	int zro = 0;
	do {
      U16 v = FIFO_DATA_A[0];
	  if (v) cnt++; else zro++;
	  rot--;
	} while ( rot || ( cnt<32768 && !SN74V283_IsEmpty(component_A) ) );
    snprintf ( fifo_state, 64, "FIFO A Pre-empty %d %d\r\n", cnt, zro ); io_out_string( fifo_state );

	rot = 5;
	cnt = 0;
	zro = 0;
	do {
      U16 v = FIFO_DATA_B[0];
	  if (v) cnt++; else zro++;
	  rot--;
	} while ( rot || ( cnt<32768 && !SN74V283_IsEmpty(component_B) ) );
    snprintf ( fifo_state, 64, "FIFO B Pre-empty %d %d\r\n", cnt, zro ); io_out_string( fifo_state );
	}

	//  Start the spectrometer(s)
	//
	if( runA ) CGS_StartSampling ( SPEC_A, integration_time, nClearouts, nSamples, (U32*)0 );
    vTaskDelay(10);
	if( runB ) CGS_StartSampling ( SPEC_B, integration_time, nClearouts, nSamples, (U32*)0 );
    vTaskDelay(10);

    int ccc;
    for ( ccc=0; ccc<=nClearouts; ccc++ ) {

	    if(runA) {
             snprintf ( fifo_state, 48, "FIFO A %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_A) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_A) ? " H" : "nH",
                    SN74V283_IsFull    (component_A) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_A ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_A ) );
	    }

	    if(runB) {
			 snprintf ( fifo_state, 48, "FIFO B %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_B) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_B) ? " H" : "nH",
                    SN74V283_IsFull    (component_B) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_B ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_B ) );
	    }

    }

    int sss;
    for ( sss=0; sss<=nSamples; sss++ ) {

        vTaskDelay( (int)integration_time );

	    if(runA) {
             snprintf ( fifo_state, 48, "FIFO A %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_A) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_A) ? " H" : "nH",
                    SN74V283_IsFull    (component_A) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_A ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_A ) );
	    }

	    if(runB) {
			 snprintf ( fifo_state, 48, "FIFO B %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_B) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_B) ? " H" : "nH",
                    SN74V283_IsFull    (component_B) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_B ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_B ) );
	    }
    }

	int waitLonger = 5;

	    if(runA) {

        int nC = SN74V283_GetNumOfCleared( component_A );
        int nS = SN74V283_GetNumOfSpectra( component_A );

	    while ( waitLonger && nC+nS < nClearouts+nSamples ) {

            snprintf ( fifo_state, 48, "FIFO A %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_A) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_A) ? " H" : "nH",
                    SN74V283_IsFull    (component_A) ? " F" : "nF" );
	         io_out_string ( fifo_state );
             io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_A ) );
             io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_A ) );

            vTaskDelay(1000);
			waitLonger--;

            nC = SN74V283_GetNumOfCleared( component_A );
            nS = SN74V283_GetNumOfSpectra( component_A );
	    }
	    }

	waitLonger = 5;

    if(runB) {

        int nC = SN74V283_GetNumOfCleared( component_B );
        int nS = SN74V283_GetNumOfSpectra( component_B );

	    while ( waitLonger && nC+nS < nClearouts+nSamples ) {

            snprintf ( fifo_state, 48, "FIFO B %s %s %s\t\t",
                    SN74V283_IsEmpty   (component_B) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_B) ? " H" : "nH",
                    SN74V283_IsFull    (component_B) ? " F" : "nF" );
        io_out_string ( fifo_state );
            io_out_S32 ( "C %d  ",   SN74V283_GetNumOfCleared( component_B ) );
            io_out_S32 ( "S %d\r\n", SN74V283_GetNumOfSpectra( component_B ) );

            vTaskDelay(1000);
			waitLonger--;

            nC = SN74V283_GetNumOfCleared( component_B );
            nS = SN74V283_GetNumOfSpectra( component_B );
		}
	}

    // Realized that data will be clocked in even during clearing stage
	// will either // have to modify (flywire) the WEn FIFO connection,
	// or power down the ADC (via PD pin),
	// or keep ADC in reset.
    #warning "2017-04-28 SKF.  LEFT OFF HERE.  GOOD LUCK!!!!!!"

    if ( runA ) { io_out_string( CGS_Get_A_EOScode() ); snprintf( fifo_state, 48, "  Rx EOS %u\r\n", CGS_Get_A_EOS() ); io_out_string( fifo_state ); }
    if ( runB ) { io_out_string( CGS_Get_B_EOScode() ); snprintf( fifo_state, 48, "  Rx EOS %u\r\n", CGS_Get_B_EOS() ); io_out_string( fifo_state ); }

	vTaskDelay(3000);

			# if 0
    //  FIFO Readout
	//
    if(runA) SN74V283_ReadEnable(component_A);
    if(runB) SN74V283_ReadEnable(component_B);
			# endif

    vTaskDelay((portTickType)TASK_DELAY_MS(250));

	int i;
    if ( runA ) {
      int cntA = 0;
	  for ( i=0; i<nClearouts+nSamples; i++) {

       int j;
       for ( j=0; j<2069; j++ ) {
		 U16 v = FIFO_DATA_A[0];
		 if ( 2==printChannels || (1==printChannels && i>=nClearouts ) ) {
		   snprintf ( fifo_state, 64, "%2d %4d %5hu ", i, j, v );
           io_out_string ( fifo_state );
           snprintf ( fifo_state, 48, " %s %s %s\r\n",
                    SN74V283_IsEmpty   (component_A) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_A) ? " H" : "nH",
                    SN74V283_IsFull    (component_A) ? " F" : "nF" );
           io_out_string ( fifo_state );
		}
         if ( !SN74V283_IsEmpty ( component_A ) ) cntA++;
		}
	   if ( 2==printChannels || (1==printChannels && i>=nClearouts ) ) io_out_string ( "\r\n" );
		}

      io_out_S32 ( "FIFO A expected %ld    ", (S32)((nClearouts+nSamples)*2069) );
      io_out_S32 (       "contained %ld\r\n", (S32)cntA );

	  while ( !SN74V283_IsEmpty ( component_A ) ) {
		 U16 v = FIFO_DATA_A[0];
		 cntA++;
			}

      io_out_S32 ( "FIFO A total contained %ld\r\n", (S32)cntA );
				}



    if ( runB ) {
      int cntB = 0;
	  for ( i=0; i<nClearouts+nSamples; i++) {
       U32 sum = 0;
       int j;
       for ( j=0; j<2069; j++ ) {
		 U16 v = FIFO_DATA_B[0];
		 if ( 2==printChannels || (1==printChannels && i>=nClearouts ) ) {
		   snprintf ( fifo_state, 64, "%2d %4d %5hu ", i, j, v );
           io_out_string ( fifo_state );
           snprintf ( fifo_state, 48, " %s %s %s\r\n",
                    SN74V283_IsEmpty   (component_B) ? " E" : "nE",
                    SN74V283_IsHalfFull(component_B) ? " H" : "nH",
                    SN74V283_IsFull    (component_B) ? " F" : "nF" );
           io_out_string ( fifo_state );
		}
         if ( !SN74V283_IsEmpty ( component_B ) ) cntB++;
			}
	   if ( 2==printChannels || (1==printChannels && i>=nClearouts ) ) io_out_string ( "\r\n" );
	}

      io_out_S32 ( "FIFO B expected %ld    ", (S32)((nClearouts+nSamples)*2069) );
      io_out_S32 (       "contained %ld\r\n", (S32)cntB );

	  while ( !SN74V283_IsEmpty ( component_B ) ) {
		 U16 v = FIFO_DATA_B[0];
		 cntB++;
}

      io_out_S32 ( "FIFO B total contained %ld\r\n", (S32)cntB );
    }

# if 0
    if(runA) SN74V283_ReadDisable ( component_A );
    if(runB) SN74V283_ReadDisable ( component_B );
# endif

	CGS_DeInitialize( runA, runB );
	CGS_Power_Off();

	return CEC_Ok;
}


void command_spectrometer_reboot (void) {

    //  FIXME ---  This is NOT working !!!

    io_out_string ( "Reboot" );
    //  Set watchdog timer to 0.25 seconds
//  wdt_opt_t opt = {
//      .us_timeout_period = 520000L
//  };
//  wdt_enable(&opt);

    while(1) {
//      vTaskDelay((portTickType)TASK_DELAY_MS( 77 ));
        io_out_string ( "." );
    }

}

static S16 CMD_DropToBootloader(void) {

    //  FIXME -- Need different mechanism, no user flash on this board
    //  U32 const boot_mode = 0xA5A5BEEFL;
    //  flashc_memcpy((void *)(AVR32_FLASHC_USER_PAGE_ADDRESS+0), &boot_mode, sizeof(boot_mode), true);

    return CEC_Ok;
}

//    Set of commands understood by command shell:
//    Commands are
//    <command_string> [--option|--option value|--option=value]
typedef enum {
    //    General Information
    CS_Info,
    CS_SelfTest,
    //    Configuration parameters
    CS_Get,
    CS_Set,
    //    Receive data for permanent storage (e.g., stray light matrix)
//  CS_Receive,
    // Testing and Debugging
    CS_SPItest,
    CS_SPIpins,
    CS_SpecTest,
    CS_SpecPwr,
    CS_SRAMtest,
//  CS_ASTtest,
    CS_TestTWIMux,
    CS_MeasureTilt,
    CS_MeasureHeadTilt,
    CS_MeasureHeading,
    CS_CalAccelerometer,
    CS_CalCompass,
    CS_ShutterTest,
//  CS_StartTest,
    CS_PressureTest,
    CS_SpecTemp,
    CS_FlashMemTest,
    //    Program control
    CS_Slow,
    CS_FirmwareUpgrade,
    CS_Reboot,
    //    Accept but ignore
    CS_Dollar
} ShellCommandID;

typedef struct {
    char* name;
    ShellCommandID id;
} ShellCommand_t;

static ShellCommand_t shellCmd[] = {
    //  General Information
    //
    { "$Info",        CS_Info },  //  Support legacy command for SUNACom convenience
    { "SelfTest",     CS_SelfTest },

    //  Configuration parameters
    //
    { "Get",          CS_Get },
    { "Set",          CS_Set },

    //  File access
    //
//  { "Receive",      CS_Receive },

    // Testing and Debugging
    //
    { "SPItest",      CS_SPItest },
    { "SPIpins",      CS_SPIpins },
    { "SpecTest",     CS_SpecTest },
    { "SpecPWR",      CS_SpecPwr },
    { "ExtSRAM",      CS_SRAMtest },
//  { "ASTalarm",     CS_ASTtest },
    { "TWIMux",       CS_TestTWIMux },
    { "Tilt",         CS_MeasureTilt },
    { "HeadTilt",     CS_MeasureHeadTilt },
    { "Heading",      CS_MeasureHeading },
    { "CalAcc",       CS_CalAccelerometer },
    { "CalCompass",   CS_CalCompass },
    { "ShutterTest",  CS_ShutterTest },
//  { "StartTest",    CS_StartTest },
    { "PressureTest", CS_PressureTest },
    { "SpecTemp",     CS_SpecTemp },
    { "FlashMemTest", CS_FlashMemTest },

    //  Program control
    //
    { "Slow",         CS_Slow },
    { "Upgrade",      CS_FirmwareUpgrade },
    { "Reboot",       CS_Reboot },
    { "$",            CS_Dollar } 
};

static const S16 MAX_SHELL_CMDS = sizeof(shellCmd)/sizeof(ShellCommand_t);

static void CMD_Handle ( char* cmd, Access_Mode_t* access_mode, uint16_t* timeout, uint16_t* idleDone ) {

    gHNV_SetupCmdSpecTask_Status = TASK_IN_SETUP;

    Bool needReboot = false;
    Bool match = false;
    S16 try = 0;
    char* arg = 0;

    //  Skip leading white space
    while ( *cmd == ' ' || *cmd == '\t' ) cmd++;

    do {
        if ( 0 == strncasecmp ( cmd, shellCmd[try].name, strlen(shellCmd[try].name) ) ) {
            match = true;
            arg = cmd + strlen(shellCmd[try].name);
        } else {
            try++;
        }
    } while ( !match && try < MAX_SHELL_CMDS );

    S16 cec = CEC_Ok;
    char result[64];
    result[0] = 0;

    if ( try == MAX_SHELL_CMDS ) {
        if ( *cmd == 0 ) {
            cec = CEC_EmptyShellCommand;
        } else {
            cec = CEC_UnknownShellCommand;
        }
    } else {
        //  Split the remainder of the command into
        //  option and (optional) value parts

        //  Skip over spaces and "--" option prefix.
        //  Nothing will be done if this is an option-less command.
        while ( *arg == ' '
             || *arg == '\t'
             || *arg == '-' ) {
            arg++;
        }
        //  Option argument to current command starts now
        char* option = arg;

        //  Skip over option argument characters to see if value string is following
        while ( isalnum ( *arg )
             || *arg == '_'
             || *arg == '*' ) {
            arg++;
        }

        char* value;

        if ( 0 == *arg ) {
            //  No value in command string.
            value = "";
        } else {
            //  Skip over white space / assignment character
            while ( *arg == ' '
                 || *arg == '\t'
                 || *arg == '=' ) {
                *arg = 0;   //  Terminate the option string.
                arg++;
            }
            value = arg;    //  Value starts at first non-space/non-assignment next character.
        }

        switch ( shellCmd[try].id ) {
        //  General Information - Only at Op_Idle
        case CS_Info:       cec = Cmd_Info     ( option, result, sizeof(result) ); break;
        case CS_SelfTest:   cec = Cmd_SelfTest ();                                 break;

        //  Configuration parameters - Only at Op_Idle
        case CS_Get:               cec = CFG_CmdGet( option, result, sizeof(result) ); break;
        case CS_Set:               cec = CFG_CmdSet( option, value, *access_mode ); break;

        //  File access - Only at Op_Idle
//      case CS_Receive:           cec = FSYS_CmdReceive ( option, value, access_mode, XM_useCRC );
//                                 cec = CEC_Ok;                                break;
        //  Testing and Debugging
        case CS_SPItest:     cec = Cmd_SPItest     ( option );                  break;
        case CS_SPIpins:     cec = Cmd_SPIpins     ( option, value );           break;
        case CS_SpecTest:    cec = Cmd_SpecTest    ( option, value );           break;
        case CS_SpecPwr:     cec = Cmd_SpecPWR     ( option, value );           break;
        case CS_SRAMtest:    cec = Cmd_ExternalSRAMtest();                      break;
//      case CS_ASTtest:           cec = Cmd_ASTAlarmTest    ();                break;

        case CS_TestTWIMux:        cec = Cmd_TestTWIMux      ( option, value ); break;
        case CS_MeasureTilt:       cec = Cmd_MeasureTilt     ( option );        break;
        case CS_MeasureHeadTilt:   cec = Cmd_MeasureHeadTilt ( option, value ); break;
        case CS_MeasureHeading:    cec = Cmd_MeasureHeading  ( option );        break;
        case CS_CalAccelerometer:  cec = Cmd_CalAccelerometer( option );        break;
        case CS_CalCompass:        cec = Cmd_CalCompass      ( option );        break;

        case CS_ShutterTest:       cec = Cmd_ShutterTest     ();                break;
//      case CS_StartTest:         cec = Cmd_StartTest       ( option );        break;
        case CS_PressureTest:      cec = Cmd_PressureTest    ( option, value ); break;
        case CS_SpecTemp:          cec = Cmd_SpecTemp        ( option, value ); break;
        case CS_FlashMemTest:      cec = FlashMemory_Test    ( option, value ); break;
        //  Special function

        case CS_Slow:       *timeout = 30;
                            *idleDone = 10000;
                            cec = CEC_Ok;
                            break;

        case CS_FirmwareUpgrade:   cec = CMD_DropToBootloader();
                            if ( CEC_Ok == cec ) {
                                //CFG_Set_Firmware_Upgrade ( CFG_Firmware_Upgrade_Yes );
                                needReboot = true;
                            }
                            break;
        case CS_Reboot:     cec = CEC_Ok;
                            needReboot = true;
                            break;
        case CS_Dollar:     //  Accept "$", "$$", ...
                            //  Reject "$[^$]*"
                            while ( *cmd == '$' ) cmd++;
                            if ( *cmd ) cec = CEC_Failed;
                            else        cec = CEC_Ok;
                            break;

        default: break;
        }
    }

    if ( CEC_Ok == cec ) {
        if ( shellCmd[try].id != CS_Dollar ) {
            //  Output result (if present)
            io_out_string( "$Ok " );
            io_out_string( result );
            io_out_string( "\r\n" );
        } else {
            //  Dollar sign is a special case.
            //  It serves as a request to re-send the prompt.
            io_out_string( "\r\n" );
        }
    } else if ( cec == CEC_EmptyShellCommand ) {
        io_out_string( "\r\n" );
    } else {
        //  Output error code
        io_out_S32 ( "$Error: %ld\r\n", (S32)cec );
    }

    if ( needReboot ) {
        command_spectrometer_reboot();
    }

    gHNV_SetupCmdSpecTask_Status = TASK_RUNNING;

    return;
}

static void command_spectrometer_loop(void) {

  uint16_t timeout_sec = 60;
  uint16_t idleDone_ms = 15000;

# if 0 // def WATERMARKING
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif

  //  Get all spectrometer board tasks going.
  
  data_exchange_spectrometer_resumeTask();
  data_acquisition_resumeTask();
  //#warning "data resume removed"

  Access_Mode_t access_mode = Access_User;

  data_exchange_address_t const myAddress = DE_Addr_SpectrometerBoard_Commander;

  struct timeval now;
  gettimeofday( &now, (void*)0 );
  io_out_S32( "Time: %ld    ", now.tv_sec );
  io_out_string( "HyperNav SpecBoard> " );
  
  {
    //  This packet is intended to inform the controller that
    //  the spectrometer board just got operational,
    //  and is ready to get configured.
    //  Normally, the controller board will initiate configuration on its own.
    //  However, after a spectrometer board reset (e.g., due to watchdog),
    //  the controller is unaware that a spectrometer reset occurred.
    //
    data_exchange_packet_t packet_message;
    packet_message.to   = DE_Addr_ControllerBoard_Commander;
    packet_message.from = myAddress;
    packet_message.type = DE_Type_Command;
    packet_message.data.Command.Code  = CMD_CMC_StartSpecBoard;
    packet_message.data.Command.value.u64 = 0;
    data_exchange_packet_router ( myAddress, &packet_message );
  }

  while ( 1 ) {

    # if 0 // def WATERMARKING
      //  Report stack water mark, if changed to previous water mark
      //
      if ( INCLUDE_uxTaskGetStackHighWaterMark ) { }
      stackWaterMark = uxTaskGetStackHighWaterMark(0);

      if ( stackWaterMark < stackWaterMark_threshold ) {
        while ( stackWaterMark < stackWaterMark_threshold ) stackWaterMark_threshold/=2;
          data_exchange_packet_t packet_message;
          packet_message.to   = DE_Addr_ControllerBoard_Commander;
          packet_message.from = myAddress;
        packet_message.type = DE_Type_Syslog_Message;
        packet_message.data.Syslog.number = SL_MSG_StackWaterMark;
        packet_message.data.Syslog.value  = stackWaterMark;
          data_exchange_packet_router ( myAddress, &packet_message );
        }
    # endif

      data_exchange_packet_t packet_rx_via_queue;
      Bool gotPacket = false;

      if ( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) ) {

        gotPacket = true;

        if ( packet_rx_via_queue.to != myAddress ) {
          //  This is not supposed to happen
          //
          data_exchange_packet_t packet_error;

          packet_error.from               = myAddress;
          packet_error.to                 = DE_Addr_ControllerBoard_Commander;
          packet_error.type               = DE_Type_Syslog_Message;
          packet_error.data.Syslog.number = SL_ERR_MisaddressedPacket;
          packet_error.data.Syslog.value  = packet_rx_via_queue.from;

          data_exchange_packet_router( myAddress, &packet_error );

          //  Mark data as free to prevent lockup.
          //
          switch ( packet_rx_via_queue.type ) {

          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
//        case DE_Type_OCR_Data:
          case DE_Type_OCR_Frame:
//        case DE_Type_MCOMS_Data:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, 0 ) ) {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }
          break;

          case DE_Type_Ping:
          case DE_Type_Command:
          case DE_Type_Response:
          case DE_Type_Syslog_Message:
            //  No action required. Packet ignored.
          break;

          case DE_Type_Nothing:
          break;

          //  Do NOT add a "default:" here.
          //  That way, the compiler will generate a warning
          //  if one possible packet type was missed.
          }

        } else {

          //  If there is a need to respond,
          //  the following address will be overwritten.
          //  A non DE_Addr_Nobody value will be used as a flag
          //  to send the packet_response.
          data_exchange_packet_t packet_response;
          packet_response.to = DE_Addr_Nobody;

          switch ( packet_rx_via_queue.type ) {

          case DE_Type_Ping:

            packet_response.to = packet_rx_via_queue.from;
            packet_response.from = myAddress;
            packet_response.type = DE_Type_Response;
            packet_response.data.Response.Code = RSP_ALL_Ping;
            packet_response.data.Response.value.u64 = 0;  //  Will be ignored
            break;

          case DE_Type_Command:

            switch ( packet_rx_via_queue.data.Command.Code ) {

            case CMD_ANY_Query:

              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_CMS_Query;
              packet_response.data.Response.value.u32[0] = CFG_Get_ID();
              packet_response.data.Response.value.u32[1] = 1;

              break;

            case CMD_CMS_SyncTime:
              {
              struct timeval now;
              now.tv_sec = packet_rx_via_queue.data.Command.value.u32[0] + 3;
              now.tv_usec = 0;
              settimeofday( &now, (void*)0 );
              }
              break;

            default:
              //  Report ignored commands
              io_out_S32( "Got command %d\r\n", (S32)packet_rx_via_queue.data.Command.Code );
              }

              break;

          case DE_Type_Response:
            //  Currently ignoring
              break;

          case DE_Type_Configuration_Data:
            {
            if ( pdTRUE == xSemaphoreTake( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {

              Config_Data_t cd;

              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                memcpy ( &cd, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Config_Data_t) );
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                copy_to_ram_from_sram ( (uint8_t*)(&cd), packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Config_Data_t) );
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }

              CFG_Set ( &cd );
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            } else {
              io_out_string ( "Not getting CONF\r\n" );
            }

            }
            break;

          case DE_Type_Spectrometer_Data:
//        case DE_Type_OCR_Data:
          case DE_Type_OCR_Frame:
//        case DE_Type_MCOMS_Data:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:

# if 0  //  FIXME - Send spectrometer syslog via packets to controller
          //syslog_out ( SYSLOG_ERROR, "command_spectrometer_loop", "Incoming data unexpected, discard" );
# endif

            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }

          break;

          default:
            io_out_string ( "ERROR SCC ignoring unimplemented packet type\r\n" );
            break;

          }

          //  Do not pass received packet anywhere
          //  Send back the response packet to give data pointer back to sender
          if ( packet_response.to != DE_Addr_Nobody ) {
            data_exchange_packet_router ( myAddress, &packet_response );
          }
        }
      }  //  if packet received

      Bool gotInput = false;

      if ( !gotPacket ) {

        //  Handle input via Maintenance Port

        char cmd[64];

        S16 const inCode = io_in_getstring ( cmd, sizeof(cmd), timeout_sec, idleDone_ms );

        if ( inCode > 0 ) {
          gotInput = true;
          CMD_Handle ( cmd, &access_mode, &timeout_sec, &idleDone_ms );
        }

        if ( inCode >= 0 ) {
          gettimeofday( &now, (void*)0 );
          io_out_S32( "Time: %ld    ", now.tv_sec );
          io_out_string( "HyperNav SpecBoard> " );
        }
      }

      // Sleep
      gHNV_SetupCmdSpecTask_Status = TASK_SLEEPING;
      if ( gotPacket || gotInput ) {
        //  Only delay briefly if getting packet or input
        vTaskDelay((portTickType)TASK_DELAY_MS(5));
      } else {
        vTaskDelay((portTickType)TASK_DELAY_MS(HNV_SetupCmdSpecTask_PERIOD_MS));
      }
      gHNV_SetupCmdSpecTask_Status = TASK_RUNNING;
  }

}

static void emergencyMenuEntry(void) {
# if IMPLEMENT_TLM
    char dummy;
    if ( tlm_recv ( &dummy, 1, O_PEEK | O_NONBLOCK ) > 0 ) {
        tlm_recv ( &dummy, 1, O_NONBLOCK );
        if ( 'X' == dummy /* || '$' == dummy */ ) {
            if ( tlm_recv ( &dummy, 1, O_PEEK | O_NONBLOCK ) > 0 ) {
                tlm_recv ( &dummy, 1, O_NONBLOCK );
                if ( 'X' == dummy /* || '$' == dummy */ ) {
                    command_spectrometer_loop( );
                }
            }
        }
    }
# endif
}

//*************************************************************************
//! \brief  setup_command_spectrometer_task()  Setup+Command task
//*************************************************************************
static void setup_command_spectrometer_task( void *pvParameters_notUsed ) {

    /*  Initialize system  */

    hnv_sys_spec_param_t initParameter;
    hnv_sys_spec_status_t initStatus;

    avr32rerrno = 0;
    char* upMsg = 0;

    if ( HNV_SYS_SPEC_FAIL == hnv_sys_spec_init( &initParameter, &initStatus ) ) {
            upMsg = "System failed to initialize.";
            goto sys_failed;
    }

    taskMonitor_spectrometer_startTask();
    gHNV_SetupCmdSpecTask_Status = TASK_RUNNING;

    //  Allow emergency entry into Menu
    emergencyMenuEntry();

    /*
     *  Find out what happened before this execution.
     */

    char* resetMsg;
    U8 const firmware_resetCause = hnv_sys_spec_resetCause();

    switch ( firmware_resetCause ) {
    case HNV_SYS_SPEC_WDT_RST:
        resetMsg = "watchdog timeout";
        break;
    case HNV_SYS_SPEC_PCYCLE_RST:
        //  Increment the power cycle counter.
     // CFG_Set_Power_Cycle_Counter(CFG_Get_Power_Cycle_Counter()+1);
        resetMsg = "power cycling";
        break;
    case HNV_SYS_SPEC_EXT_RST:
        resetMsg = "external reset";
        break;
    case HNV_SYS_SPEC_BOD_RST:
        resetMsg = "internal brown-out";
        break;
    case HNV_SYS_SPEC_CPUERR_RST:
        resetMsg = "CPU error";
        break;
    case HNV_SYS_SPEC_UNKNOWN_RST:
        resetMsg = "unknown event";
        //  Unknown happens when the supervisor wakes the firmware prior to full shutdown.
        //  This can happen when sending telemetry soon after the SUNA entered DEEP SLEEP,
        //  e.g., in periodic mode.  To make the SUNA more responsive, check supervisor_wakeup,
        //  and permit breaking into command shell inside those operating modes that support it.
        break;
    default:
        resetMsg = "no event";
    }

    io_out_string( "\r\n\nHyperNav Spectrometer Board"
             "\r\nFW Version " HNV_FW_VERSION_MAJOR "." HNV_FW_VERSION_MINOR "." HNV_SPEC_FW_VERSION_PATCH
             "\r\nBuilt " ); io_out_string( buildVersion() ); io_out_string( "\r\n" );

    io_out_string( "Reset from " ); io_out_string ( resetMsg ); io_out_string ( "\r\n" );

# if 0
    //  Optional startup messages
    {

      volatile avr32_pm_t* pm = &AVR32_PM;
      io_out_S32( "PM->mcctrl     = %08x\r\n", pm->mcctrl     );
      io_out_S32( "PM->cpusel     = %08x\r\n", pm->cpusel      );
      io_out_S32( "PM->hsbsel     = %08x\r\n", pm->hsbsel      );
      io_out_S32( "PM->pbasel     = %08x\r\n", pm->pbasel      );
      io_out_S32( "PM->pbbsel     = %08x\r\n", pm->pbbsel      );
      io_out_S32( "PM->pbcsel     = %08x\r\n", pm->pbcsel      );
      io_out_S32( "PM->cpumask    = %08x\r\n", pm->cpumask     );
      io_out_S32( "PM->hsbmask    = %08x\r\n", pm->hsbmask     );
      io_out_S32( "PM->pbamask    = %08x\r\n", pm->pbamask     );
      io_out_S32( "PM->pbbmask    = %08x\r\n", pm->pbbmask     );
      io_out_S32( "PM->pbcmask    = %08x\r\n", pm->pbcmask     );
      io_out_S32( "PM->pbadivmask = %08x\r\n", pm->pbadivmask  );
      io_out_S32( "PM->pbbdivmask = %08x\r\n", pm->pbbdivmask  );
      io_out_S32( "PM->pbcdivmask = %08x\r\n", pm->pbcdivmask  );
      io_out_S32( "PM->cfdctrl    = %08x\r\n", pm->cfdctrl     );
      io_out_S32( "PM->unlock     = %08x\r\n", pm->unlock     );
      io_out_S32( "PM->ier        = %08x\r\n", pm->ier        );
      io_out_S32( "PM->idr        = %08x\r\n", pm->idr        );
      io_out_S32( "PM->imr        = %08x\r\n", pm->imr        );
      io_out_S32( "PM->isr        = %08x\r\n", pm->isr        );
      io_out_S32( "PM->icr        = %08x\r\n", pm->icr        );
      io_out_S32( "PM->sr         = %08x\r\n", pm->sr         );
      io_out_S32( "PM->ppcr       = %08x\r\n", pm->ppcr       );
      io_out_S32( "PM->rcause     = %08x\r\n", pm->rcause     );
      io_out_S32( "PM->wcause     = %08x\r\n", pm->wcause     );
      io_out_S32( "PM->awen       = %08x\r\n", pm->awen       );
      io_out_S32( "PM->config     = %08x\r\n", pm->config     );
      io_out_S32( "PM->version    = %08x\r\n", pm->version    );
    }
# endif

    io_out_S32( "    gplp[0]    = %08x\r\n", pcl_read_gplp(0) );
    io_out_S32( "    gplp[1]    = %08x\r\n", pcl_read_gplp(1) );

    pcl_write_gplp ( 0, 0x00000000 );
    pcl_write_gplp ( 1, 0xF000000F );

    //  Enter command loop, which never returns
    command_spectrometer_loop();

    //  Goto here if fatal error occurred
sys_failed:

    io_out_string ( upMsg ); io_out_string ( "\r\n" );
    io_out_string ( "Shutdown..." );

    command_spectrometer_reboot();
}

//  This function allows other tasks to push a data_exchange_packet_t to this task.
void commander_spectrometer_pushPacket ( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    // io_out_string ( "DBG DXS push packet\r\n" );
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

Bool command_spectrometer_createTask(void) {

  static Bool gTaskCreated = false;

  if(!gTaskCreated)
  {
    // Create Setup&Command Task for Spectrometer Board
      if (pdPASS != xTaskCreate( setup_command_spectrometer_task,
                    HNV_SetupCmdSpecTask_NAME, HNV_SetupCmdSpecTask_STACK_SIZE, NULL,
                   HNV_SetupCmdSpecTask_PRIORITY, &gHNV_SetupCmdSpecTask_Handler)) {

      return false;
    }

    //  Different from all other tasks, this task does not support pause/resume.
    //  It is always running, thus there is no need to have a gTaskCtrlMutex

      // Create message queue
    if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) ) {
      return false;
    }

    //  Allocate memory for data sending via packets
    sending_data_package.mutex = xSemaphoreCreateMutex();
    sending_data_package.state = EmptyRAM;
    sending_data_package.address = (void*) pvPortMalloc ( sizeof(any_sending_data_t) );
    if ( NULL == sending_data_package.mutex
      || NULL == sending_data_package.address ) {
      return false;
    }

      gTaskCreated = true;
  }

  return true;
}
