/* \file        frames.c
 * \brief       generate data frames
 *              if compiled with -D BUILD_CAL_FILES becomes standalone calibration software
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 ************************************/

# define PRESSURE_RAWS

# ifndef BUILD_CAL_FILES

# include "frames.h"

# include <compiler.h>
# include <stdio.h>
# include <stdint.h>
# include <string.h>

# include "datalogfile.h"
# include "telemetry.h"

# if 0
# include <math.h>
# include <time.h>

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc    malloc
#define vPortFree       free
#endif

# include "avr32rerrno.h"
# include "syslog.h"
# include "watchdog.h"

# include "extern.h"
# include "filesystem.h"
# include "numerical.h"
# include "info.h"
# include "crc.h"

# endif

# include "FreeRTOS.h"

# include "telemetry.h"
# include "io_funcs.controller.h"
# include "datalogfile.h"
# include "files.h"
# include "config.controller.h"

//  Start of Satlantic Frame Generation
//
# define ASCII_REDUCED 4

static uint32_t frm_YearYdayValue ( time_t time_secs ) {

    char yearstr[10];
    strftime ( yearstr, sizeof(yearstr)-1, "%Y", gmtime ( &time_secs ) );

    char ydaystr[10];
    strftime ( ydaystr, sizeof(ydaystr)-1, "%j", gmtime ( &time_secs ) );

    return 1000 * atoi (yearstr)
                + atoi (ydaystr);
}

char* frm_DateString ( time_t time_secs )
/*
 * Returns date as an ASCII string with format YYYYJJJ
 */
{
    static char datestr[10];

    strftime ( datestr, sizeof(datestr)-1, "%Y%j", gmtime ( &time_secs ) );

    return datestr;
}

F64 frm_DecimalTime ( time_t time_secs, suseconds_t time_ticks )
/*
 * Returns time as a float
 */
{
    struct tm* now = gmtime ( &time_secs );        // calc UTC time

    F64 dec_hr =   (F64)(now->tm_hour)                              //  hour
               + ( (F64)(now->tm_min ) ) /          60              //  minutes
               + ( (F64)(now->tm_sec ) ) /        3600              //  seconds
               + ( (F64)(time_ticks  ) ) / ( (F64)3600*1000000.0 ); //  microseconds

    return dec_hr;
}

# if 0
static char* frm_DecimalTimeString ( time_t time_secs, suseconds_t time_ticks )
/*
 * Returns time as a decimal hour string, to 6 decimal places
 */
{
    static char timestr[12];

    F64 dec_hr = frm_DecimalTime ( time_secs, time_ticks );

    snprintf ( timestr, sizeof(timestr), "%02.6f", printAbleF64(dec_hr) );

    return timestr;
}
# endif

# define WRITE_EACH 0

int16_t frm_out_or_log (
        Spectrometer_Data_t* hsd,
        uint8_t tlm_frame_subsampling,
        uint8_t log_frame_subsampling ) {

    int i;
    U32 log_check_sum = 0;
    U32 tlm_check_sum = 0;

# define ASCII 1  //   Force telemetry output in ASCII, for ease of testing

    uint8_t to_file = ( log_frame_subsampling<12);
    uint8_t to_tlm  = ( tlm_frame_subsampling<12);

    if ( log_frame_subsampling>=12 ) log_frame_subsampling = 12;
    if ( tlm_frame_subsampling>=12 ) tlm_frame_subsampling = 12;

    //  Temporary variables
//  uint16_t  tmp_len = 0;
//  uint8_t  tmp_U8;
//  U32 tmp_U32;
//  F32 tmp_F32;
//  F32 const invalid_F32 = -99;
//  F32 const invalid_Conc = -1;

    uint16_t to_write;

    uint8_t  frame_before_spectrum[ 64 ];
    int          n_before_spectrum = 0;

    char     tmp_buf [32];
    union {
      uint8_t  tmp_mem[8];
      F64      tmp_BD8;
      uint32_t tmp_U32;
      int32_t  tmp_S32;
      uint16_t tmp_U16;
      int16_t  tmp_S16;
      uint8_t  tmp_U8;
    } V;

    //  Header
    //

    uint32_t frame_sn = 0;
    switch ( hsd->aux.side ) {
      case 0: frame_sn = CFG_Get_Frame_Port_Serial_Number(); break;
      case 1: frame_sn = CFG_Get_Frame_Starboard_Serial_Number(); break;
    }

    char frameType = 'N';
    if ( hsd->aux.tag & SAD_TAG_DARK ) {
      frameType = 'D';
    } else if ( hsd->aux.tag & SAD_TAG_LIGHT ) {
      frameType = 'L';
    } else if ( hsd->aux.tag & SAD_TAG_CHAR_DARK ) {
      frameType = 'D';
    } else if ( hsd->aux.tag & SAD_TAG_LIGHT_MINUS_DARK ) {
      frameType = 'C';
    }

    if ( 0 == frame_sn ) {
      log_frame_subsampling = 12;
      tlm_frame_subsampling = 12;
    }

    if ( to_file ) {
      snprintf ( tmp_buf, sizeof(tmp_buf), "SATX%c%c%04lu", frameType, 'Z'-log_frame_subsampling, frame_sn%10000 );
      to_write = strlen(tmp_buf);
# if WRITE_EACH
      if ( to_write != DLF_Write ( (uint8_t*)tmp_buf, to_write ) )    { return (int16_t)-1; }
# else
      memcpy ( frame_before_spectrum, tmp_buf, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= tmp_buf[i];

    }

    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), "SATY%c%c%04lu", frameType, 'Z'-tlm_frame_subsampling, frame_sn%10000 );
      } else {
        snprintf ( tmp_buf, sizeof(tmp_buf), "SATX%c%c%04lu", frameType, 'Z'-tlm_frame_subsampling, frame_sn%10000 );
      }
      to_write = strlen(tmp_buf);
      if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-2; }
      for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
    }

    //  Date as YYYYDDD
    //
    V.tmp_S32 = frm_YearYdayValue ( hsd->aux.acquisition_time.tv_sec );
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-3; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%7ld", V.tmp_S32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-4; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-4; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Hour as decimal
    //
    V.tmp_BD8 = frm_DecimalTime( hsd->aux.acquisition_time.tv_sec, hsd->aux.acquisition_time.tv_usec );
    to_write =  8;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-5; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%.6lf", V.tmp_BD8 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-6; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-6; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Side
    //
    V.tmp_U16 = hsd->aux.side;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-7; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-8; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-8; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Sample Number
    //
    V.tmp_U16 = hsd->aux.sample_number;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-9; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-10; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-10; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Integration Time
    //
    V.tmp_U16 = hsd->aux.integration_time;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-11; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-12; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-12; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Dark avg
    //
    V.tmp_U16 = hsd->aux.dark_average;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-13; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-14; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-14; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Dark noise
    //
    V.tmp_U16 = hsd->aux.dark_noise;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-15; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-16; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-16; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

# if 0
    //  Spec Min
    //
    V.tmp_U16 = hsd->aux.spec_min;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-13; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-14; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-14; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Spec max
    //
    V.tmp_U16 = hsd->aux.spec_max;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-15; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-16; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-16; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }
# endif

    //  Shift up
    //
    V.tmp_S16 = hsd->aux.light_minus_dark_up_shift;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-17; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-18; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-18; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Spectrometer temperature
    //
    V.tmp_S16 = hsd->aux.spectrometer_temperature;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-19; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-20; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-20; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Pressure
    //
    V.tmp_S32 = hsd->aux.pressure;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-21; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%ld", V.tmp_S32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

# ifdef PRESSURE_RAWS

    V.tmp_S32 = hsd->aux.pressure_T_counts;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-21; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%ld", V.tmp_S32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    V.tmp_U32 = hsd->aux.pressure_T_duration;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-21; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%lu", V.tmp_U32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }


    V.tmp_S32 = hsd->aux.pressure_P_counts;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-21; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%ld", V.tmp_S32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    V.tmp_U32 = hsd->aux.pressure_P_duration;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-21; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%lu", V.tmp_U32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-22; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }


# endif

    //  Sun azimuth
    //
    V.tmp_U16 = hsd->aux.sun_azimuth;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-23; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-24; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-24; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Housing heading
    //
    V.tmp_U16 = hsd->aux.housing_heading;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-23; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-24; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-24; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Housing pitch
    //
    V.tmp_S16 = hsd->aux.housing_pitch;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-25; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-26; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-26; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    // Housing roll
    //
    V.tmp_S16 = hsd->aux.housing_roll;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-27; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-28; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-28; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Radiometer pitch
    //
    V.tmp_S16 = hsd->aux.spectrometer_pitch;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-29; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-30; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-30; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Radiometer roll
    //
    V.tmp_S16 = hsd->aux.spectrometer_roll;
    to_write = 2;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-31; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hd", V.tmp_S16 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-32; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-32; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

    //  Tag
    //
    V.tmp_U32 = hsd->aux.tag;
    to_write = 4;

    if( to_file ) {
# if WRITE_EACH
      if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-33; }
# else
      memcpy ( frame_before_spectrum+n_before_spectrum, V.tmp_mem, to_write );
      n_before_spectrum += to_write;
# endif
      for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
    }
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%lx", V.tmp_U32 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-34; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-34; }
        for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
      }
    }

# if WRITE_EACH
    //
# else
    if( to_file ) {
      if ( n_before_spectrum != DLF_Write ( frame_before_spectrum, n_before_spectrum ) )    { return (int16_t)-33; }
    }
# endif

    if ( frame_sn ) {

      //  Spectrum to file
      //
      if ( to_file ) {

        if ( 0 == log_frame_subsampling ) {

          to_write = 2*N_SPEC_PIX;
          if ( to_write != DLF_Write ( (uint8_t*)hsd->hnv_spectrum, to_write ) ) { return (int16_t)-35; }
          uint8_t* mm = (uint8_t*) hsd->hnv_spectrum;
          for ( i=0; i<to_write; i++) log_check_sum -= mm[i];

        } else {

          uint16_t stepSize = 1 << log_frame_subsampling;

          if ( stepSize <= N_SPEC_PIX ) {

            int px;
            for ( px=stepSize-1; px<N_SPEC_PIX; px+=stepSize ) {

              V.tmp_U16 = hsd->hnv_spectrum[px];
              to_write = 2;
              if ( to_write != DLF_Write ( V.tmp_mem, to_write ) ) { return (int16_t)-37; }
              for ( i=0; i<to_write; i++) log_check_sum -= V.tmp_mem[i];
            }
          }
        }
      }

      //  Spectrum to telemetry
      //
      if ( to_tlm ) {

        if ( 0 == tlm_frame_subsampling ) {

          int px;
          for( px=0; px<N_SPEC_PIX; px++ ) {

            V.tmp_U16 = hsd->hnv_spectrum[px];

            to_write = 2;
            if ( ASCII ) {
              snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
              to_write = strlen(tmp_buf);
              if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-36; }
              for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
            } else {
              if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-36; }
              for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
            }
          }

        } else {

          uint16_t stepSize = 1 << tlm_frame_subsampling;

          if ( stepSize <= N_SPEC_PIX ) {

            int px;
            for ( px=stepSize-1; px<N_SPEC_PIX; px+=stepSize ) {
              V.tmp_U16 = hsd->hnv_spectrum[px];
              to_write = 2;
              if ( ASCII ) {
                snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", V.tmp_U16 );
                to_write = strlen(tmp_buf);
                if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-38; }
                for ( i=0; i<to_write; i++) tlm_check_sum -= tmp_buf[i];
              } else {
                if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-38; }
                for ( i=0; i<to_write; i++) tlm_check_sum -= V.tmp_mem[i];
              }
            }
          }
        }
      }

    }

    //  Checksum
    //
    V.tmp_U8 = log_check_sum & 0xFF;
    to_write = 1;

    if( to_file ) { if ( to_write != DLF_Write ( V.tmp_mem, to_write ) )    { return (int16_t)-31; } }

    V.tmp_U8 = tlm_check_sum & 0xFF;
    if ( to_tlm ) {
      if ( ASCII ) {
        snprintf ( tmp_buf, sizeof(tmp_buf), ",%hu", (U16)V.tmp_U8 );
        to_write = strlen(tmp_buf);
        if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-32; }
      } else {
        if ( to_write != tlm_send  ( V.tmp_mem, to_write, 0 ) ) { return (int16_t)-32; }
      }
    }

    //  All complete. Terminate.

    snprintf ( tmp_buf, sizeof(tmp_buf), "\r\n" );
    to_write = strlen(tmp_buf);

    if( to_file ) { if ( to_write != DLF_Write ( tmp_buf, to_write ) )    { return (int16_t)-39; } }
    if( to_tlm  ) { if ( to_write != tlm_send  ( tmp_buf, to_write, 0 ) ) { return (int16_t)-40; } }

    return FRAME_OK;
}

# if 0
//
//  The frames are defined in a doc called SUNA-LC-Frame-Definitions.ods
//
//  Frame elements are grouped into, and included depending on frame type.
//
//      General:    Header, Date, Time
//      Fit:        Nitrate, other species & baselines, RMSE
//      Auxiliary:  Bromide-trace, Absorbances at 254, 350; maybe more.
//      Spec Stats: Spec AVG (obsolete?), Dark Avg (as used in this processing)
//      IntFactor:  make part of Spec Stats
//      Spectrum:   Some/all channles
//      System:     Temperatures, Humidity, Voltages, Current
//      CheckSum:   1-byte check-sum - TODO in the future: Replace by CRC
//      Terminator: "\r\n" [ASCII only]
//
//

int16_t frm_buildFrame (
        uint8_t frame_subsampling,
        uint8_t frameBuffer[], uint16_t bufferSize, uint16_t* filledSize,
        Spectrometer_Data_t* hsd ) {

    //  Sanity check on input

    if ( !frameBuffer || 0 == bufferSize || 0 == filledSize || 0 == hsd ) {
        return FRAME_FAIL;
    }

    if(1){
    int i;
    int end = ( 32<bufferSize ) ? 32 : bufferSize;
    for ( i=0; i<end; i++ ) {
      frameBuffer[i] = 'z';
    }
    frameBuffer[end-1] = 0;
    *filledSize = end;
    return FRAME_OK;
    }

    //  Provide temporary variables to assemble the frame.

//  char tmp_buf [128];
//  uint16_t  tmp_len = 0;
//  uint8_t  tmp_U8;
    uint16_t tmp_U16;
//  U32 tmp_U32;
    F32 tmp_F32;
//  F32 const invalid_F32 = -99;
//  F32 const invalid_Conc = -1;

    //  Start with an empty frame,
    //  and keep track how far the frame is filled.
    uint16_t usedBuffer = 0;
    frameBuffer[0] = 0;
    *filledSize = 0;

    //  General:    Header

    if ( bufferSize < snprintf ( (char*)frameBuffer, bufferSize, "SATHN%c%04d",
            'Z', CFG_Get_Serial_Number() ) ) {
        return FRAME_FAIL;
    }

    usedBuffer += strlen((char*)frameBuffer);

    //  Date & Time

    if ( usedBuffer + sizeof(U32) + sizeof(F64) >= bufferSize ) {
      io_out_string ( "DT\r\n" );
      return FRAME_FAIL;
    }

    uint32_t yyyyddd = frm_YearYdayValue ( hsd->aux.acquisition_time.tv_sec );
    memcpy ( frameBuffer+usedBuffer, &yyyyddd, sizeof ( yyyyddd ) );
    usedBuffer += sizeof ( yyyyddd );

    F64 decimal_hours = frm_DecimalTime( hsd->aux.acquisition_time.tv_sec,
            hsd->aux.acquisition_time.tv_usec );
    memcpy ( frameBuffer+usedBuffer, &decimal_hours, sizeof ( decimal_hours ) );
    usedBuffer += sizeof ( decimal_hours );

    //  Spec Avg, Dark Avg

    if ( usedBuffer + 3*sizeof(uint16_t) >= bufferSize ) {
      io_out_string ( "AV\r\n" );
      return FRAME_FAIL;
    }

    tmp_U16 = hsd->aux.integration_time;
    memcpy ( frameBuffer+usedBuffer, &tmp_U16, sizeof ( tmp_U16 ) );
    usedBuffer += sizeof ( tmp_U16 );

    tmp_U16 = hsd->aux.dark_average;
    memcpy ( frameBuffer+usedBuffer, &tmp_U16, sizeof ( tmp_U16 ) );
    usedBuffer += sizeof ( tmp_U16 );

    tmp_U16 = hsd->aux.dark_noise;
    memcpy ( frameBuffer+usedBuffer, &tmp_U16, sizeof ( tmp_U16 ) );
    usedBuffer += sizeof ( tmp_U16 );

    //  Spectrum

    if ( 0 == frame_subsampling ) {

      uint16_t const sz = sizeof(uint16_t)*N_SPEC_PIX;

      if ( usedBuffer + sz >= bufferSize ) {
        //io_out_S32 ( "SPB %d", (S32)usedBuffer );
        //io_out_S32 ( " %d\r\n", (S32)sz );
        return FRAME_FAIL;
      }

      memcpy ( frameBuffer+usedBuffer, &(hsd->hnv_spectrum[0]), sz );
      usedBuffer += sz;

    } else {

      int nAvg = 1 << frame_subsampling;

      if ( usedBuffer + sizeof(uint16_t)*N_SPEC_PIX/nAvg >= bufferSize ) {
        //io_out_S32 ( "SPB %d", (S32)usedBuffer );
        //io_out_S32 ( " %d\r\n", (S32)sz );
        return FRAME_FAIL;
      }

      if ( nAvg <= N_SPEC_PIX ) {

        int px;
        for ( px=0; px<N_SPEC_PIX; px+=nAvg ) {
          uint32_t value = 0;
          int a;
          for ( a=0; a<nAvg; a++ ) {
            value += hsd->hnv_spectrum[px+a];
          }
          uint16_t const avg_value = value / nAvg;

          memcpy ( frameBuffer+usedBuffer, &avg_value, sizeof(uint16_t) );
          usedBuffer += sizeof(uint16_t);
        }
      }
    }


    //  Add physical/electrical

    if ( usedBuffer + 5*sizeof(F32) >= bufferSize ) {
      io_out_string ( "PH\r\n" );
      return FRAME_FAIL;
    }

    tmp_F32 = hsd->aux.spectrometer_temperature;
    memcpy ( frameBuffer+usedBuffer, &tmp_F32, sizeof ( tmp_F32 ) );
    usedBuffer += sizeof ( tmp_F32 );

    tmp_F32 = hsd->aux.pressure;
    memcpy ( frameBuffer+usedBuffer, &tmp_F32, sizeof ( tmp_F32 ) );
    usedBuffer += sizeof ( tmp_F32 );

    tmp_F32 = hsd->aux.housing_heading;
    memcpy ( frameBuffer+usedBuffer, &tmp_F32, sizeof ( tmp_F32 ) );
    usedBuffer += sizeof ( tmp_F32 );

    tmp_F32 = hsd->aux.housing_pitch;
    memcpy ( frameBuffer+usedBuffer, &tmp_F32, sizeof ( tmp_F32 ) );
    usedBuffer += sizeof ( tmp_F32 );

    tmp_F32 = hsd->aux.housing_roll;
    memcpy ( frameBuffer+usedBuffer, &tmp_F32, sizeof ( tmp_F32 ) );
    usedBuffer += sizeof ( tmp_F32 );

    //  Checksum / CRC

# ifdef USING_CRC
    //  TODO in the future: Improve Satlantic style frames by replacing check sum with CRC.
    uint16_t const crc16 = fCrc16Bit ( frameBuffer, usedBuffer );

    if ( usedBuffer + 2 >= bufferSize ) {
      io_out_string ( "CC\r\n" );
      return FRAME_FAIL;
    }
    memcpy ( frameBuffer+usedBuffer, &crc16, sizeof ( crc16 ) );
    usedBuffer += sizeof ( crc16 );

# else
    U32 check_sum = 0;

    int i;
    for ( i=0; i<usedBuffer; i++ ) {
        check_sum -= frameBuffer [ i ];
    }

    uint8_t cs = check_sum & 0xFF;

    if ( usedBuffer + 1 >= bufferSize ) {
      io_out_string ( "CS\r\n" );
      return FRAME_FAIL;
    }
    frameBuffer [ usedBuffer++ ] = cs;
# endif

    //  All complete.

    *filledSize = usedBuffer;

    return FRAME_OK;
}
# endif

# if 0
int16_t frm_generateAndOutput( Spectrometer_Data_t* hsd ) {

  U8 const outFrameSubsampling = CFG_Get_Output_Frame_Subsampling();
  CFG_Log_Frames const logFrameType = CFG_Get_Log_Frames();


  uint16_t const frame_len_max_A = 256 + 6*N_SPEC_PIX/ASCII_REDUCED;
  uint16_t const frame_len_max_B = 128 + 2*N_SPEC_PIX;

  uint16_t frame_len_max = (frame_len_max_B>frame_len_max_A) ? frame_len_max_B : frame_len_max_A;

  char* frame = pvPortMalloc ( frame_len_max );

  if ( 0 == frame ) {
    //SYSLOG ( SYSLOG_ERROR, "Out of memory %d.", frame_len_max );
    io_out_string ( "ERROR: frame out of memory.\r\n" );
    return FRAME_FAIL;
  }

  uint16_t frame_len;
  bool frame_ok = false;


    //watchdog_clear();

    if ( FRAME_OK != frm_buildFrame( CFG_Get_Output_Frame_Subsampling(),
                       frame, frame_len_max, &frame_len, hsd ) ) {
      //SYSLOG ( SYSLOG_ERROR, "Cannot build output frame." );
      io_out_string ( "ERROR: out frame not built.\r\n" );
      frame_ok = false;
    } else {
      frame_ok = true;

      //  output this
      tlm_send ( frame, frame_len, 0 );
    }

  if ( CFG_Internal_Data_Logging_Available == CFG_Get_Internal_Data_Logging()
    && CFG_Log_Frames_No != logFrameType ) {    //  i.e., requested to log

    if ( outFrameSubsampling > 0 || !frame_ok ) {
      //  Rebuild frame if frame types differ or the frame was not built above

      if ( FRAME_OK == frm_buildFrame( (U8)0,
                         frame, frame_len_max, &frame_len, hsd ) ) {
        frame_ok = true;
      } else {
        //SYSLOG ( SYSLOG_ERROR, "Cannot build log frame." )
        io_out_string ( "ERROR: log frame not built.\r\n" );
        frame_ok = false;
      }
    }

    if ( frame_ok ) {

      if ( frame_len != DLF_Write ( frame, frame_len ) ) {
        io_out_string ( "ERROR: frame not logged to file.\r\n" );
        //SYSLOG ( SYSLOG_ERROR, "Frame not written to file." );
      } else {
        //io_out_string ( "DBG: frame logged to file.\r\n" );
      }
    }
  }

  vPortFree ( frame );

  return frame_ok;
}
# endif

# else

# include <libgen.h>
# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <unistd.h>

# warning "When building calibration file generation, memmem() function prototype provided manually."
//  Manually provide function prototype
void *memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

static double lampIpol ( double wavelength[],
                         double irradiance[],
                         int    numValues,
                         double neededWL ) {
  //  Set to zero outside ranges:
  //
  if ( neededWL < wavelength[0] ) return 0.0;
  if ( wavelength[numValues-1] < neededWL ) return 0.0;

  //  Find bracketing
  //  wavelength[i] <= neededWL < wavelength[i+1]
  //
  int i = 0;
  while ( i<numValues && wavelength[i] < neededWL ) {
    i++;
  }

  //  Interpolate linearly
  //
  //    R - R0   R1 - R0
  //    ------ = -------
  //    W - W0   W1 - W0
  //
  //                           W  - W0
  //    R = R0 + ( R1 - R0 ) * -------
  //                           W1 - W0
  //

  double W0 = wavelength[i-1];
  double W1 = wavelength[i];

  double I0 = irradiance[i-1];
  double I1 = irradiance[i];

  return I0 + ( I1 - I0 ) * ( neededWL - W0 ) / ( W1 - W0 );
}

static double targIpol ( double wavelength[],
                         double reflectance[],
                         int    numValues,
                         double neededWL ) {
  //  Set to zero outside ranges:
  //
  if ( neededWL < wavelength[0] ) return 0.0;
  if ( wavelength[numValues-1] < neededWL ) return 0.0;

  //  Find bracketing
  //  wavelength[i] <= neededWL < wavelength[i+1]
  //
  int i = 0;
  while ( i<numValues && wavelength[i] < neededWL ) {
    i++;
  }

  //  Interpolate linearly
  //
  //    R - R0   R1 - R0
  //    ------ = -------
  //    W - W0   W1 - W0
  //
  //                           W  - W0
  //    R = R0 + ( R1 - R0 ) * -------
  //                           W1 - W0
  //

  double W0 = wavelength[i-1];
  double W1 = wavelength[i];

  double R0 = reflectance[i-1];
  double R1 = reflectance[i];

  return R0 + ( R1 - R0 ) * ( neededWL - W0 ) / ( W1 - W0 );
}

static double immersionCoefficient_viaFit( double wl ) {

    double const wl2 = wl*wl;
    double const wl3 = wl*wl2;
    double const wl4 = wl2*wl2;
    double const wl5 = wl2*wl3;
    double const wl6 = wl3*wl3;

    return 4.041614E-17*wl6
	 - 1.605660E-13*wl5
	 + 2.590799E-10*wl4
	 - 2.172093E-07*wl3
	 + 9.996268E-05*wl2
	 - 2.413078E-02*wl
	 + 4.162810E+00;
}

/* imm: calculate the immersion coefficient for a radiance sensor with fused silica window,
    for wavelength, temperature, salinity */
/*  Austin, 1976.
    Zibordi, G., 2006. Immersion Factor of In-Water Radiance Sensors: Assessment for a Class of Radiometers.
    Boss, E., 2016. Matlab program If.m, Rg.m, RInW. */

/* nG: calculate index of refraction of fused silica glass for wavelength, temperature */
/* Leviton & Frey. Temperature-dependent absolute refractive index measurements of synthetic fused silica */
static double nG(double lambda_nm, double tC)
/*  lambda: wavelength (nm)
    tC: temperature (degrees C)
    return: index of refraction for fused silica glass */
{
    double tK = tC+273.15; /* convert to Kelvin */
    double t[5] = { 1, tK, tK*tK, tK*tK*tK, tK*tK*tK*tK };

    double ss[3][5] = {
        { 1.10127E+00, -4.94251E-05, 5.27414E-07, -1.59700E-09, 1.75949E-12 },
        { 1.78752E-05, 4.76391E-05, -4.49019E-07, 1.44546E-09, -1.57223E-12 },
        { 7.93552E-01, -1.27815E-03, 1.84595E-05, -9.20275E-08, 1.48829E-10 },
    };
    double ll[3][5] = {
        { -8.90600E-02, 9.08730E-06, -6.53638E-08, 7.77072E-11, 6.84605E-14 },
        { 2.97562E-01, -8.59578E-04, 6.59069E-06, -1.09482E-08, 7.85145E-13 },
        { 9.34454E+00, -7.09788E-03, 1.01968E-04, -5.07660E-07, 8.21348E-10 },
    };
    double s[3] = {0,0,0};
    double l[3] = {0,0,0};

    for( int i=0;i<3;i++)
        for( int j=0;j<5;j++)
        {
            s[i] = s[i] + ss[i][j] * t[j];
            l[i] = l[i] + ll[i][j] * t[j];
        }

    double lambda_um = lambda_nm/1000; /* convert to microns */
    double lambda_um_2 = lambda_um * lambda_um;

    return sqrt( 1 + s[0] * lambda_um_2 / ( lambda_um_2 - l[0]*l[0] )
                   + s[1] * lambda_um_2 / ( lambda_um_2 - l[1]*l[1] )
                   + s[2] * lambda_um_2 / ( lambda_um_2 - l[2]*l[2] ));
}


/* nW: calculate index of refraction for water for wavelength, temperature, salinity */
/* Quan & Fry 1995 */
static double nW(double lambda, double tC, double sal)
/*  lambda: wavelength (nm)
    tC: temperature (degrees C)
    sal: salinity (PSU)
    n: index of refraction for water */
{
    double       n = 0;
    double const n0 = 1.31405;
    double const n1 = 1.779e-4;
    double const n2 = -1.05e-6;
    double const n3 = 1.6e-8;
    double const n4 = -2.02e-6;
    double const n5 = 15.868;
    double const n6 = 0.01155;
    double const n7 = -0.00423;
    double const n8 = -4382;
    double const n9 = 1.1455e6;
    n = n0 + ( n1 + n2 * tC +n3 * pow(tC,2) ) * sal
        + n4 * pow(tC,2)
        + ( n5 + n6 * sal + n7 * tC ) / lambda
        + n8 / pow(lambda,2)
        + n9 / pow(lambda,3);
    return n;
}

static double const imm(double const lambda, double const tC, double const sal)
{
    double const rIW = nW(lambda, tC, sal);
    double const rIG = nG(lambda, tC);
    double const imm = rIW * pow(rIW+rIG,2) / pow(1+rIG,2);
    //printf("Water refactive index: %f\n",rIW);
    //printf("Glass refactive index: %f\n",rIG);
    //printf("Immersion Factor: %f\n",imm);
    return imm;
}


static double immersionCoefficient( double wl ) {

    double const tC = 20;
    double const sal = 30;
    return imm(wl, tC, sal);
}

//  ALERT: Unsafe code: Overrun if coeff
static int parse_thermal_correction_file ( char* filename, double coeff[] ) {

  if ( !filename || !filename[0] ) return 0;
  if ( !coeff ) return 0;

  int nCoeffs = 0;

  FILE* fp = fopen ( filename, "r" );

  if ( fp ) {
    char line[128];
    while ( line == fgets( line, 127, fp ) ) {
      if ( !strncmp( line, "SE0 = ", 6 ) ) {
        coeff[0] = atof ( line + 6 );
        nCoeffs ++;
      } else if ( !strncmp( line, "XE0 = ", 6 ) ) {
        coeff[1] = atof ( line + 6 );
        nCoeffs ++;
      } else if ( !strncmp( line, "WED = ", 6 ) ) {
        coeff[2] = atof ( line + 6 );
        nCoeffs ++;
      } else {
        // fprintf ( stderr, "Info: Skipping %s from %s\n", line, filename );
      }
    }

    fclose ( fp );
  }

  return nCoeffs;
}

# define MAXSPECTRA 32  // If increasing, make static or move to global scope.
# define SPECSIZE   2048
  uint16_t  darkSpectrum [  MAXSPECTRA][SPECSIZE];
  uint16_t  darkIntTime  [  MAXSPECTRA];
//double    darkAcqTime  [  MAXSPECTRA];
  uint16_t  darkAverage  [  MAXSPECTRA];
  int      nDarks = 0;
  uint16_t lightSpectrum [8*MAXSPECTRA][SPECSIZE];
  uint16_t lightIntTime  [8*MAXSPECTRA];
//double   lightAcqTime  [8*MAXSPECTRA];
   int16_t sTemperature  [8*MAXSPECTRA];
  int      nLights = 0;

  double    XM [ SPECSIZE ][ SPECSIZE ];

static int parse_inverse_stray_light_file ( char* filename ) {

  if ( !filename || !filename[0] ) return 0;

  int nCoeffs = 0;

  FILE* fp = fopen ( filename, "r" );

  if ( fp ) {
    int row, col;
    for ( row=0; row<SPECSIZE; row++ ) {
    for ( col=0; col<SPECSIZE; col++ ) {
      if ( 1 != fscanf ( fp, "%lf", &(XM[row][col]) ) ) {
        fclose ( fp );
        return nCoeffs;
      }
      nCoeffs++;
    }
    }

    fclose ( fp );
  }

  return nCoeffs;
}

int main( int argc, char* argv[] ) {

  union {
    uint16_t s16;
    uint16_t u16;
    uint8_t  u8[2];
  } u16_swapper;
  uint8_t u8_tmp;

  //  Command line arguments control cal file type
  //
  char   wavelength_source       = 'Z';
  int    wavelength_pixel_shift  = 0;
  double integration_time_offset = 0.0;  //  In milliseconds. - Ideally = 0.  Found to be -0.25 ms
  char   thermal_correction      = 'N';
  char*  ST_fn = 0;
  int    nST = 0;
  double ST[3] = { 0, 0, 0 };
  char*  DA_fn = 0;
  int    nDA = 0;
  double DA[3] = { 0, 0, 0 };
  char   stray_light_correction  = 'N';
  char*  XM_fn = 0;
  int    nXM = 0;

  char*  ID                      = "";

  int opt;
  int opterr = 0;

  while( (opt = getopt(argc, argv, "w:O:I:S:D:X:Z:" )) != -1 ) {
    switch (opt) {
    case 'w': switch ( optarg[0] ) {
              case 'N':
              case 'n': wavelength_source = 'N'; // NIST laser lines
            //case 'L':
            //case 'l': wavelength_source = 'L'; // Lamp spectra lines
            //case 'F':
            //case 'f': wavelength_source = 'F'; // Fraunhofer lines
	      }
              break;
    case 'Z': wavelength_pixel_shift = atoi ( optarg );
              break;
    case 'O': integration_time_offset = atof ( optarg );
              break;
    case 'S': thermal_correction = 'Y';
	      ST_fn = optarg;
	      nST = parse_thermal_correction_file ( ST_fn, ST );
	      if ( nST != 3 ) {
		nST = 0;
                ST_fn = 0;
		ST[0] = ST[1] = ST[2] = 0;
		thermal_correction = 'x';
	      }
              break;
    case 'D': thermal_correction = 'Y';
	      DA_fn = optarg;
	      nDA = parse_thermal_correction_file ( DA_fn, DA );
	      if ( nDA != 3 ) {
		nDA = 0;
                DA_fn = 0;
		DA[0] = DA[1] = DA[2] = 0;
		thermal_correction = 'z';
	      }
              break;
    case 'X': stray_light_correction = 'Y';
	      XM_fn = optarg;
	      nXM = parse_inverse_stray_light_file ( XM_fn /* arg as global: XM */ );
	      if ( nXM != SPECSIZE*SPECSIZE ) {
                nXM = 0;
                XM_fn = 0;
		stray_light_correction = 'n';
	      }
              break;
    case 'I': {
                int IDlen = strlen(optarg);
		if ( IDlen > 0 ) {
		  ID = malloc ( 1+IDlen+1 );
		  ID[0] = '-';
		  strcpy ( ID+1, optarg );
		}
	      }
              break;

    default:  /* '?' */
              opterr = 1;
	      break;
    }
  }

  if ( opterr > 0 ) {
    fprintf ( stderr, "Error: %d input argument%s problematic. Abort.\n", opterr>1?"s":"" );
    return 1;
  }


  if ( argc != optind+6 ) {
    fprintf ( stderr, "Usage: %s [-I ID] [-S SpecTemp-Thermal] [-D DarkCount-Thermal] [-O offset_of_integration_time_in_seconds] [-X StrayLightMatrix] [-w{Z|N}] HyperNavSN DarkDataFile LightDataFile SpecCoefFile LampFile TargetFile\r\n", argv[0] );
    fprintf ( stderr, "argc = %d   optind+6 = %d\n", argc, optind+6 );
    fprintf ( stderr, "Source code in HyperNav Firmware Controller frames.c file\n" );
    return 1;
  }

  int  const  HyperNavSN   = atoi ( argv[optind+0] );
  char const* dDatFileName =        argv[optind+1]  ;
  char const* lDatFileName =        argv[optind+2]  ;
  char const* wlenFileName =        argv[optind+3]  ;
  char const* lampFileName =        argv[optind+4]  ;
  char const* targFileName =        argv[optind+5]  ;

  int canCalculateCalibrationCoefficients = 1;

  //  Read Data File -> D[0..N], L[0..M]
  //  File must contain dark and light frames.
  //  Frames must be binary encoded.
  //  Frames must not be subsampled.
  //
  uint16_t calibrationIntegrationTime = 999;  //  Units: milli-seconds
  double   avg_darkAverage;
  double   avg_sTemperature;
  double const Spec_Temp_Factor = 0.01;
  double   avgDark [SPECSIZE];
  double   sdvDark [SPECSIZE];
  int      cntDark [SPECSIZE];
  double   avgLight[SPECSIZE];
  double   sdvLight[SPECSIZE];
  double   slpLight[SPECSIZE];
  int      cntLight[SPECSIZE];

  double   LMD     [SPECSIZE];
  double   XMxLMD  [SPECSIZE];

  size_t const frame_aux  = 6 + 4  // SATXDZnnnn
                          + 4      // Date
                          + 8      // Time
                          + 3*2    // Side + Sample + IntTime
                          + 3*2    // DarkAvg + DarkStddev + ShiftUp
                          + 2      // SpecTemp
                          + 4      // Pressure
# ifdef PRESSURE_RAWS
                          + 4*4    // Counts & Durations
# endif
                          + 2      // Sun Azimuth
                          + 2      // Body Heading
                          + 2*2    // Body Tilt x & y
                          + 2*2    // Head Tilt x & y
                          + 4;     // Tag
  size_t const frame_spec = 2048*2;
  size_t const frame_end  = 1;  //  TODO +2 for terminator

  size_t const framesize = frame_aux + frame_spec + frame_end;

  FILE* ddfp = fopen ( dDatFileName, "r" );

  if ( !ddfp ) {
    fprintf ( stderr, "ERROR: Cannot open dark data file %s\r\n", dDatFileName );

    canCalculateCalibrationCoefficients = 0;
  } else {

    fseek ( ddfp, 0L, SEEK_END );
    long file_size = ftell ( ddfp );
    fseek ( ddfp, 0L, SEEK_SET );

    fprintf ( stderr, "File %s size %ld\n", dDatFileName, file_size );

    uint8_t* fileInMemory = malloc ( file_size );

    if ( !fileInMemory ) {
      fprintf ( stderr, "Cannot allocate.\n" );
      file_size = 0;
    } else {
      if ( 1 != fread ( fileInMemory, file_size, 1, ddfp ) ) {
        fprintf ( stderr, "Cannot read.\n" );
        file_size = 0;
        free ( fileInMemory );
        fileInMemory = 0;
      }
    }

    fclose ( ddfp );

    if ( file_size ) {

      uint8_t* haystack = fileInMemory;
      size_t   haysize  = file_size;
      char     needle[16]; snprintf ( needle, 16, "SATXDZ%04d", HyperNavSN );
      size_t   needleSz = 10;

      fprintf ( stderr, "Searching for %s\n", needle );

      uint8_t* occurrence = 0;

      while ( haystack && ( occurrence = memmem( haystack, haysize, needle, needleSz ) ) ) {

        size_t forward = occurrence-haystack;
        haysize -= forward;

        if ( haysize > framesize  ) {
          if ( nDarks<MAXSPECTRA ) {

            memcpy ( &(darkIntTime [nDarks]), occurrence+       26,    2 );
            memcpy ( &(darkAverage [nDarks]), occurrence+       28,    2 );
            memcpy (   darkSpectrum[nDarks] , occurrence+frame_aux, 4096 );
            nDarks++;
            haystack = occurrence + framesize;
            haysize -= framesize;
          } else {
            haystack = 0;
          }
        } else {
          haystack = 0;
        }

      }

      //  TODO
      //  Verify frame validity using checksum

      free ( fileInMemory );
    }

    fprintf ( stderr, "Have %d dark spectra\n", nDarks );
  }

  FILE* ldfp = fopen ( lDatFileName, "r" );

  if ( !ldfp ) {
    fprintf ( stderr, "ERROR: Cannot open light data file %s\r\n", lDatFileName );

    canCalculateCalibrationCoefficients = 0;
  } else {

    fseek ( ldfp, 0L, SEEK_END );
    long file_size = ftell ( ldfp );
    fseek ( ldfp, 0L, SEEK_SET );

    fprintf ( stderr, "File %s size %ld\n", lDatFileName, file_size );

    uint8_t* fileInMemory = malloc ( file_size );

    if ( !fileInMemory ) {
      fprintf ( stderr, "Cannot allocate.\n" );
      file_size = 0;
    } else {
      if ( 1 != fread ( fileInMemory, file_size, 1, ldfp ) ) {
        fprintf ( stderr, "Cannot read.\n" );
        file_size = 0;
        free ( fileInMemory );
        fileInMemory = 0;
      }
    }

    fclose ( ldfp );

    if ( file_size ) {


      uint8_t* haystack = fileInMemory;
      size_t   haysize  = file_size;
      char     needle[16]; snprintf ( needle, 16, "SATXLZ%04d", HyperNavSN );
      size_t   needleSz = 10;

      fprintf ( stderr, "Searching for %s\n", needle );

      uint8_t* occurrence = 0;

      while ( haystack && ( occurrence = memmem( haystack, haysize, needle, needleSz ) ) ) {

        size_t forward = occurrence-haystack;
        haysize -= forward;

        if ( haysize > framesize  ) {
          if ( nLights<8*MAXSPECTRA ) {

            memcpy ( &(lightIntTime [nLights]), occurrence+       26,    2 );
            memcpy ( &(sTemperature [nLights]), occurrence+       34,    2 );
            memcpy (   lightSpectrum[nLights] , occurrence+frame_aux, 4096 );
            nLights++;
            haystack = occurrence + framesize;
            haysize -= framesize;
          } else {
            haystack = 0;
          }
        } else {
          haystack = 0;
        }

      }

      //  TODO
      //  Verify frame validity using checksum

      free ( fileInMemory );
    }

    fprintf ( stderr, "Have %d light spectra\n", nLights );
  }

  if ( 0 == nDarks || 0 == nLights ) {
    canCalculateCalibrationCoefficients = 0;
  }

  if ( 0 == canCalculateCalibrationCoefficients ) {
    //  FAKE
    //
    int c;
    for ( c=0; c<SPECSIZE; c++ ) {
      avgDark[c] = 0;
      avgLight[c] = 0;
    }
    calibrationIntegrationTime = 10000;
  } else {

    //  Make sure to use only frames with the same integration time.
    //  Determine the last integration time and then
    //  go back to as long as that same integration time is used.
    //

    int d;
    for ( d=0; d<nDarks; d++ ) {
          u16_swapper.u16 = darkIntTime[d];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          darkIntTime[d] = u16_swapper.u16;
    }

    for ( d=0; d<nDarks; d++ ) {
          u16_swapper.u16 = darkAverage[d];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          darkAverage[d] = u16_swapper.u16;
    }

    int l;
    for ( l=0; l<nLights; l++ ) {
          u16_swapper.u16 = lightIntTime[l];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          lightIntTime[l] = u16_swapper.u16;
    }

    for ( l=0; l<nLights; l++ ) {
          u16_swapper.u16 = sTemperature[l];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          sTemperature[l] = u16_swapper.s16;
    }

    if ( lightIntTime[nLights-1] != darkIntTime[nDarks-1] ) {
      fprintf ( stderr, "ERROR: Integration time mismatch: Dark %hu != Light %hu\n", lightIntTime[nLights-1], darkIntTime[nDarks-1] );
    }

    calibrationIntegrationTime = lightIntTime[nLights-1];

    int useDark;
    for ( useDark=nDarks-1; useDark>=0; useDark-- ) {
        if ( darkIntTime[useDark] != calibrationIntegrationTime ) {
            break;
	}
    }
    useDark++;

    int useLight;
    for ( useLight=nLights-1; useLight>=0; useLight-- ) {
        if ( lightIntTime[useLight] != calibrationIntegrationTime ) {
            break;
	}
    }
    useLight++;

    int const skipLights = (nLights-useLight)/10;
    int const fromLight = useLight + skipLights;

    fprintf ( stderr, "Using integration time %hu\n", calibrationIntegrationTime );
    fprintf ( stderr, "Using dark  frames %d .. %d\n", useDark,  nDarks -1 );
    fprintf ( stderr, "Havin light frames %d .. %d\n", useLight, nLights-1 );
    fprintf ( stderr, "Using light frames %d .. %d\n", fromLight, nLights-1 );

    int c;
    for ( c=0; c<SPECSIZE; c++ ) {

      avgDark[c] = 0;
      sdvDark[c] = 0;
      cntDark[c] = 0;

      avgLight[c] = 0;
      sdvLight[c] = 0;
      slpLight[c] = 0;
      cntLight[c] = 0;

      LMD     [c] = 0;
      XMxLMD  [c] = 0;

      for ( d=useDark; d<nDarks; d++ ) {

          u16_swapper.u16 = darkSpectrum[d][c];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          double counts = u16_swapper.u16;
          avgDark[c] += counts;
          sdvDark[c] += counts*counts;
          cntDark[c] += 1;
      }

      //  Auxiliary terms to calculate light slope
      //
      double S = 0;
      double Sl = 0;
      double Sc = 0;
      double Sll = 0;
      double Slc = 0;

      for ( l=fromLight; l<nLights; l++ ) {

          u16_swapper.u16 = lightSpectrum[l][c];
          u8_tmp = u16_swapper.u8[0];
          u16_swapper.u8[0] = u16_swapper.u8[1];
          u16_swapper.u8[1] = u8_tmp;
          double counts = u16_swapper.u16;
          avgLight[c] += counts;
          sdvLight[c] += counts*counts;
          cntLight[c] += 1;

	  S   += 1;
	  Sl  += l;
	  Sc  += counts;
	  Sll += l*l;
	  Slc += l*counts;
      }

      if ( cntDark[c] > 0 ) {
        avgDark[c] /= cntDark[c];
        sdvDark[c] /= cntDark[c];
        sdvDark[c] -= avgDark[c] * avgDark[c];
        sdvDark[c]  = (sdvDark[c]>0) ? sqrt(sdvDark[c]) : 0;
      }

      if ( cntLight[c] > 0 ) {
        avgLight[c] /= cntLight[c];
        sdvLight[c] /= cntLight[c];
        sdvLight[c] -= avgLight[c] * avgLight[c];
        sdvLight[c]  = (sdvLight[c]>0) ? sqrt(sdvLight[c]) : 0;

	double DET = S*Sll-Sl*Sl;
	double a = (Sll*Sc-Sl*Slc)/DET;
	double b = (S*Slc-Sl*Sc)/DET;

      //slpLight[c] = b / avgLight[c];  // relative to counts
	slpLight[c] = b * cntLight[c];  // counts during calibration
      }

      LMD[c] = avgLight[c] - avgDark[c];
    }

    int cnt = 0;
    avg_darkAverage = 0;
    for ( d=useDark; d<nDarks; d++ ) {
      avg_darkAverage += darkAverage[d];
      cnt++;
    }
    avg_darkAverage /= cnt;

    cnt = 0;
    avg_sTemperature = 0;
    for ( l=fromLight; l<nLights; l++ ) {
      avg_sTemperature += sTemperature[l];
      cnt++;
    }
    avg_sTemperature /= cnt;
    avg_sTemperature *= Spec_Temp_Factor;
  }

  if ( nXM ) {

    //  Apply inverse stray light matrix multiplication
    //

    int row;

    for ( row=0; row<SPECSIZE; row+=1 ) {

      XMxLMD[row] = 0;

      int col;
      for ( col=0; col<SPECSIZE; col++ ) {
        XMxLMD[row] += XM[row][col] * LMD[col];
      }
    }

  }

  //
  //
  //        Find stabilized integration time (SIT)
  //        Average darks at SIT
  //        Average lights at SIT
  //

  //  Read Spectrometer Wavelenght Polynomial & Serial Number
  //
  //  File format:
  //  Comment lines begin with a '#' character
  //  Coefficient lines begin with a 'C' character,
  //  followed by an integer, followed by whitespace, followed by the coefficient, e.g.,
  //  C0 1020.54
  //  C1 -0.4
  //  ...
  //  Coefficients must be ordered.
  //
# define MAXSPECCOEFS 6
  double specCoef [MAXSPECCOEFS];
  int    specSN    =  0;
  double lowWL     =  0;
  double highWL    =  0;
  int   nSpecCoefs =  0;
  int    c;
  int    const lowPix    = 2058;
  int    const highPix   =   11;

  int    channel;         //  1..SPECSIZE
  int    channel_index;   //  0..SPECSIZE-1
  double wavelength[SPECSIZE];  //  Note: This array is 0 based

  FILE* wfp = fopen ( wlenFileName, "r" );

  if ( !wfp ) {
    fprintf ( stderr, "ERROR: Cannot open spectrometer coefficient file %s\r\n", wlenFileName );
    return 1;
  } else {

    if ( 'Z' == wavelength_source ) {

      char line[128];
      while ( line == fgets( line, 127, wfp ) ) {

        if ( '#' == line[0] ) {

          char* found;
          char* match = "Spectrometer CGS UV-NIR";

          if ( ( found = strstr ( line, match ) ) ) {
            specSN = strtol ( found + strlen(match), (char**)0, 10 );
          }

          match = "Low actual wavelength";
          if ( ( found = strstr ( line, match ) ) ) {
            lowWL = strtod ( found + strlen(match), (char**)0 );
          }

          match = "High actual wavelength";
          if ( ( found = strstr ( line, match ) ) ) {
            highWL = strtod ( found + strlen(match), (char**)0 );
          }

          //  TODO: Scan for more items to be added to the calibration file?

        } else if ( 'C' == line[0] ) {

          int idx;
          double coef;
          if( 2 == sscanf ( line+1, "%d%lf", &idx, &coef ) ) {
            //fprintf ( stderr, "%s -> %d %f\n", line, idx, coef );
            if ( idx == nSpecCoefs ) {
              specCoef[nSpecCoefs++] = coef;
            } else {
              int ll = strlen(line);
              while ( line[ll-1] == '\r' || line[ll-1] == '\n' ) {
                ll--;
                line[ll] = 0;
              }
              fprintf ( stderr, "ERROR: Spectrometer coefficient file %s contains misordered coefficient line '%s'\r\n", wlenFileName, line );
            }
          } else {
            int ll = strlen(line);
            while ( line[ll-1] == '\r' || line[ll-1] == '\n' ) {
              ll--;
              line[ll] = 0;
            }
            fprintf ( stderr, "ERROR: Spectrometer coefficient file %s contains misformatted coefficient line '%s'\r\n", wlenFileName, line );
          }
        } else {
          int ll = strlen(line);
          while ( line[ll-1] == '\r' || line[ll-1] == '\n' ) {
            ll--;
            line[ll] = 0;
          }
          fprintf ( stderr, "ERROR: Spectrometer coefficient file %s contains misformatted line '%s'\r\n", wlenFileName, line );
        }
      }

      fclose ( wfp );

      for ( c=0; c<nSpecCoefs; c++ ) {
        fprintf ( stderr, "%d %12e\n", c, specCoef[c] );
      }

      double wllow = specCoef[0];
      double pp = lowPix;
      for ( c=1; c<nSpecCoefs; c++ ) {
        wllow += specCoef[c] * pp;
        pp *= lowPix;
      }
      if ( fabs(wllow-lowWL) >= 0.007 ) {
        fprintf ( stderr, "ERROR: low wavelength mismatch %.3f %.3f\r\n", lowWL, wllow );
        return 1;
      }

      double wlhigh = specCoef[0];
      pp = highPix;
      for ( c=1; c<nSpecCoefs; c++ ) {
        wlhigh += specCoef[c] * pp;
        pp *= highPix;
      }
      if ( fabs(wlhigh-highWL) >= 0.007 ) {
        fprintf ( stderr, "ERROR: low wavelength mismatch %.3f %.3f\r\n", highWL, wlhigh );
        return 1;
      }

      for ( channel=1; channel<=SPECSIZE; channel+=1 ) {

        int px = lowPix - channel + 1 + wavelength_pixel_shift;

        double wl = specCoef[0];

        pp = px;
        for ( c=1; c<nSpecCoefs; c++ ) {
            wl += specCoef[c] * pp;
            pp *= px;
        }

	channel_index = channel-1;
	wavelength[channel_index] = wl;
      }
    } else if ( 'N' == wavelength_source ) {

      channel_index=0;

      char line[128];
      while ( line == fgets( line, 127, wfp ) ) {
        wavelength[channel_index] = atof ( line );
	channel_index ++;
      }

      fclose ( wfp );

      if ( channel_index != SPECSIZE ) {
        fprintf ( stderr, "ERROR: Wrong number if wavelength in '%s'\r\n", wlenFileName );
        return 1;
      }

    }
  }


  //  Read Lamp File
  //  File Format:
  //  1. Line - Comment
  //  Rest    - WLEN Irradiance
  //
# define MAXLAMPDATA 128
  double lampWavelength[MAXLAMPDATA];
  double lampIntensity [MAXLAMPDATA];
  char   lampInfo[128];
  int   nLampData = 0;

  FILE* lfp = fopen ( lampFileName, "r" );

  if ( !lfp ) {
    fprintf ( stderr, "ERROR: Cannot open lamp file %s\r\n", lampFileName );
    canCalculateCalibrationCoefficients = 0;
  } else {

    fgets ( lampInfo, 128, lfp );

    while ( 2 == fscanf ( lfp, "%lf%lf", &lampWavelength[nLampData], &lampIntensity[nLampData]) ) {
      nLampData++;
    }

    fclose ( lfp );

    fprintf ( stderr, "Have %d lamp data\n", nLampData );
  }

  if ( 0 == nLampData ) {
    canCalculateCalibrationCoefficients = 0;
  }

  //  Read Plaque File
  //
# define MAXTARGDATA 10000
  double targWavelength[MAXTARGDATA];
  double targReflectivity [MAXTARGDATA];
  int   nTargData = 0;

  FILE* tfp = fopen ( targFileName, "r" );

  if ( !tfp ) {
    fprintf ( stderr, "ERROR: Cannot open target file %s\r\n", targFileName );
    canCalculateCalibrationCoefficients = 0;
  } else {

    char skipLine [128];
    fgets ( skipLine, 128, tfp );
    fgets ( skipLine, 128, tfp );
    fgets ( skipLine, 128, tfp );

    while ( 2 == fscanf ( tfp, "%lf%lf", &targWavelength[nTargData], &targReflectivity[nTargData]) ) {
      nTargData++;
    }

    fclose ( tfp );

    fprintf ( stderr, "Have %d target data\n", nTargData );
  }

  if ( 0 == nTargData ) {
    canCalculateCalibrationCoefficients = 0;
  }


  //  Calibration file output
  //

  //  Encoding: 0 == ASCII 'Y', 1 == Binary 'X'
  //
# define ENCODINGS 2
  char encodeChar[ENCODINGS] = { 'Y', 'X'      };
  int encoding;

  //  Frame types: 'D' dark frame; 'L' light frame; 'C' corrected (light-minus-dark) frame
  //
# define NFTYPES 3
  char fTypeChar[NFTYPES] = { 'D', 'L', 'C' };
  int fType;

  //  Generate for all subsamplings
  //
  int subsmpl;

  for ( encoding=0; encoding<ENCODINGS; encoding++ ) {
  for ( fType=0; fType<NFTYPES; fType++ ) {
  for ( subsmpl=0; subsmpl<13; subsmpl++ ) {
  if ( 'Z' == 'Z' - subsmpl || 'R' == 'Z' - subsmpl ) {

    int const channelStep = 1<<subsmpl;
    char calFileName[64];
    snprintf ( calFileName, 63, "CalFiles%s/HyperNav-SAT%c%c%c%04d%s-W%c%+d-I%c-T%c-S%c.cal", (0==canCalculateCalibrationCoefficients) ? "Nominal" : "",
                                       encodeChar[encoding], fTypeChar[fType], 'Z'-subsmpl, HyperNavSN, ID,
	                               wavelength_source, wavelength_pixel_shift,
				       integration_time_offset ? 'Y' : 'N',
				       thermal_correction,
				       stray_light_correction );

    FILE* cfp = fopen( calFileName, "w" );

    if ( cfp ) {

    char date_time[64];
    time_t now = time( (time_t*)0 );
    strftime ( date_time, sizeof(date_time)-1, "%Y-%b-%d %H:%M (UTC)", gmtime ( &now ) );

          //  The FEL lamp irradiance is given for a 50 cm calibration distance.
          //  The plaque (target) has a known reflectivity.
          //  The FEL lamp to plaque (=target) distance is 130 cm:
          //  Scaling factor = ( calibration_distance / target_distance )
          //  Divide by PI (3.1415...) [WHY?]
# define CALIBRATED_DISTANCE 50.0
# define TARGET_DISTANCE 130.0
# define CAL_OVER_TARGET_DISTANCE (CALIBRATED_DISTANCE/TARGET_DISTANCE)


//  fprintf ( stderr, "Generating %s\n", calFileName );
    fprintf ( cfp, "################################################\r\n" );
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "# Satlantic HyperNAV Radiometer\r\n" );
    fprintf ( cfp, "# Radiometer s/n %04d\r\n", HyperNavSN );
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "# Operator       %s\r\n", getlogin() );
    fprintf ( cfp, "# File generated %s\r\n", date_time );
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "################################################\r\n" );
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "# Spectrometer s/n %d\r\n", specSN );
    fprintf ( cfp, "#\r\n" );
    for ( c=0; c<nSpecCoefs; c++ ) {
    fprintf ( cfp, "# C%d %14e\r\n", c, specCoef[c] );
    }
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "# p = pixel, range: %d .. %d\r\n", lowPix, highPix );
    fprintf ( cfp, "# wavelength(p) = C0 + C1*p + C2*p^2 + C3 * p^3\r\n" );
    fprintf ( cfp, "# wavelength range: %.2f %.2f\r\n", lowWL, highWL );
    if ( wavelength_pixel_shift ) {
    fprintf ( cfp, "# Pixel readout shifted by %+d pixel\r\n", wavelength_pixel_shift );
    fprintf ( cfp, "# If pixel shift > 0, then wavelength shift for a given channel < 0\r\n" );
    fprintf ( cfp, "# If pixel shift < 0, then wavelength shift for a given channel > 0\r\n" );
    }
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "################################################\r\n" );
    fprintf ( cfp, "#\r\n" );
    fprintf ( cfp, "# spec   data from %s\r\n", lDatFileName );
    fprintf ( cfp, "# lamp   data from %s\r\n", lampFileName );
    fprintf ( cfp, "# lamp calibration distance %.1f cm\r\n", (double)CALIBRATED_DISTANCE );
    fprintf ( cfp, "# lamp  to  plaque distance %.1f cm\r\n", (double)TARGET_DISTANCE );
    fprintf ( cfp, "# plaque data from %s\r\n", targFileName );
    fprintf ( cfp, "#\r\n" );
    if ( ST_fn ) {
    fprintf ( cfp, "# Spec Temp Characterization File %s\r\n", basename(ST_fn) );
    }
    if ( DA_fn ) {
    fprintf ( cfp, "# Dark Average Characterization File %s\r\n", basename(DA_fn) );
    }
    if ( XM_fn ) {
    fprintf ( cfp, "# (Inverted) Stray Light Matrix file is %s\r\n", basename(XM_fn) );
    }
    if ( ST_fn || DA_fn || XM_fn ) {
    fprintf ( cfp, "#\r\n" );
    }
    fprintf ( cfp, "################################################\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "%sINSTRUMENT SAT%c%c%c%04d '' 10 AS 0 NONE\r\n",
                (0==encoding)?"VLF_":"", encodeChar[encoding], fTypeChar[fType], 'Z'-subsmpl, HyperNavSN );
    } else {
      fprintf ( cfp, "INSTRUMENT SAT%c%c%c '' 6 AS 0 NONE\r\n",
                encodeChar[encoding], fTypeChar[fType], 'Z'-subsmpl );
      fprintf ( cfp, "SN %04d '' 4 AI 0 COUNT\r\n", HyperNavSN );
    }
    fprintf ( cfp, "\r\n" );

    fprintf ( cfp, "CAL_SPEC_TEMP %.3f 'C' 0 BU 0 NONE\r\n", avg_sTemperature);
    if ( ST_fn && 3 == nST ) {
      fprintf ( cfp, "# Spec Temp Characterization File %s\r\n", basename(ST_fn) );
      fprintf ( cfp, "RSP_SPEC_TEMP NONE '' 0 BU 1 QCORR\r\n" );
      fprintf ( cfp, "%.6f %.6f %.6f %.3f\r\n", ST[0], ST[1], ST[2], avg_sTemperature);
    }
    fprintf ( cfp, "\r\n" );

    fprintf ( cfp, "CAL_DARK_AVER %.1f 'count' 0 BU 0 NONE\r\n", avg_darkAverage );
    if ( DA_fn && 3 == nDA ) {
      fprintf ( cfp, "# Dark Average Characterization File %s\r\n", basename(DA_fn) );
      fprintf ( cfp, "RSP_DARK_AVER NONE '' 0 BU 1 QCORR\r\n" );
      fprintf ( cfp, "%.6f %.6f %.6f %.1f\r\n", DA[0], DA[1], DA[2], avg_darkAverage );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','         1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "DATEFIELD NONE 'YYYYDDD'   V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "DATEFIELD NONE 'YYYYDDD'   4 BS 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','         1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "TIMEFIELD NONE 'HH.hhhhhh' V AF 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "TIMEFIELD NONE 'HH.hhhhhh' 8 BD 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "SIDE      NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "SIDE      NONE ''  2 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "SAMPLE    NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "SAMPLE    NONE ''  2 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "INTTIME   LU 'sec' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "INTTIME   LU 'sec' 2 BU 1 POLYU\r\n" );
    }
    fprintf ( cfp, "%.6f 0.001\r\n", integration_time_offset*0.001 );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "DARK_AVE  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "DARK_AVE  NONE ''  2 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "DARK_SDV  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "DARK_SDV  NONE ''  2 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "SHIFTUP   NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "SHIFTUP   NONE ''  2 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "TEMP_SPEC NONE 'C' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "TEMP_SPEC NONE 'C' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 %.2f\r\n", Spec_Temp_Factor );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "PRES      NONE 'm' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "PRES      NONE 'm' 4 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.0001\r\n" );
    fprintf ( cfp, "\r\n" );

# ifdef PRESSURE_RAWS
    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "PT_COUNT  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "PT_COUNT  NONE ''  4 BS 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "PT_DURAT  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "PT_DURAT  NONE ''  4 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "PP_COUNT  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "PP_COUNT  NONE ''  4 BS 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "PP_DURAT  NONE ''  V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "PP_DURAT  NONE ''  4 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

# endif

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD      NONE ','      1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "SUNAZIMUTH NONE 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "SUNAZIMUTH NONE 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','      1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "HEADING   NONE 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "HEADING   NONE 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','      1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "TILT Y 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "TILT Y 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','       1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "TILT X 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "TILT X 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','           1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "OTHER_TILT Y 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "OTHER_TILT Y 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','            1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "OTHER_TILT X 'degree' V AI 1 POLYU\r\n" );
    } else {
      fprintf ( cfp, "OTHER_TILT X 'degree' 2 BS 1 POLYU\r\n" );
    }
    fprintf ( cfp, "0 0.01\r\n" );
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ',' 1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "TAG       NONE ''  V AS 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "TAG       NONE ''  4 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( channelStep <= SPECSIZE ) {

      //  spectrometer channels

      for ( channel=channelStep; channel<=SPECSIZE; channel+=channelStep ) {
        channel_index = channel - 1;

        int px = lowPix - channel + 1;

        double wl = wavelength[channel_index];

        fprintf ( cfp, "# channel %d, pixel %d\r\n", channel, px );
        if ( 0 == encoding ) {
          fprintf ( cfp, "FIELD     NONE ','      1 AS 0 DELIMITER\r\n" );
          fprintf ( cfp, "LU %.2f 'uW/cm^2/nm/sr' V AI 1 OPTIC3\r\n", wl );
        } else {
          fprintf ( cfp, "LU %.2f 'uW/cm^2/nm/sr' 2 BU 1 OPTIC3\r\n", wl );
        }

        uint16_t dark = 0;
        double calCoef = 1.0;
        double immCoef = 1.0;
        double lmp = 0;
        double trg = 0;
        double rac = 0;
        double Radiance = 0;


        if ( canCalculateCalibrationCoefficients ) {

          dark = avgDark[channel_index];

          lmp             = lampIpol ( lampWavelength, lampIntensity, nLampData, wl );

	  if ( lmp == 1 ) {

            rac = Radiance = trg = targIpol ( targWavelength, targReflectivity, nTargData, wl );


	  } else {

            trg      = targIpol ( targWavelength, targReflectivity, nTargData, wl );
            Radiance = lmp
                     * trg
                     * ( CAL_OVER_TARGET_DISTANCE*CAL_OVER_TARGET_DISTANCE )
                     / M_PI;
            rac = Radiance;
	  }

          double diff;
	 
	  if ( nXM ) {
            diff = XMxLMD[channel_index];
	  } else {
            diff = LMD[channel_index];
	  }

          if ( diff > 0 ) {
            calCoef = Radiance / diff;
          } else {
            calCoef = 0.0;
          }

          immCoef = immersionCoefficient( wl );
        }

        switch ( fType ) {
        case 0: //  Dark calibration file
          fprintf ( cfp, "%hu\t%.12f\t%.3f\t%.5f\r\n", dark, calCoef, immCoef, (calibrationIntegrationTime+integration_time_offset)*0.001 );
          break;
        case 1: //  Light calibration file
          fprintf ( cfp, "%hu\t%.12f\t%.3f\t%.5f\r\n", dark, calCoef, immCoef, (calibrationIntegrationTime+integration_time_offset)*0.001 );
          break;
        case 2: //  LMD calibration file
          fprintf ( cfp, "%hu\t%.12f\t%.3f\t%.5f\r\n", 0,    calCoef, immCoef, (calibrationIntegrationTime+integration_time_offset)*0.001 );
          break;
        default:
          fprintf ( cfp, "%hu\t%.12f\t%.3f\t%.5f\r\n", 0, 0.0, 0.0, 0.0 );
        }

        fprintf ( cfp, "# (%f * %f * %f / PI) = %f / ( %.2f (+/-%.2f [%d]) (slope %.2f)  - %.2f (+/-%.2f [%d]) ) (rel counts error %.4f)\r\n",
			  lmp, trg, CAL_OVER_TARGET_DISTANCE*CAL_OVER_TARGET_DISTANCE,
                                               rac, avgLight[channel_index], sdvLight[channel_index], cntLight[channel_index], slpLight[channel_index],
					            avgDark [channel_index], sdvDark [channel_index], cntDark [channel_index],
	                                            sqrt( sdvLight[channel_index]*sdvLight[channel_index]
							+ sdvDark [channel_index]*sdvDark [channel_index] )
						    / (avgLight[channel_index] - avgDark[channel_index] ) );

        double immCoef_vf = immersionCoefficient_viaFit( wl );
	fprintf ( cfp, "# Immersion coefficients %.3f %.3f and ratio %.5f\r\n", immCoef, immCoef_vf, immCoef/immCoef_vf );
        fprintf ( cfp, "\r\n" );
      }
    }

    if ( 0 == encoding ) {
      fprintf ( cfp, "FIELD     NONE ','         1 AS 0 DELIMITER\r\n" );
      fprintf ( cfp, "CHECK SUM '' V AI 0 COUNT\r\n" );
    } else {
      fprintf ( cfp, "CHECK SUM '' 1 BU 0 COUNT\r\n" );
    }
    fprintf ( cfp, "\r\n" );

    if ( 0 == encoding ) {
      //  Terminator
      //
      fprintf ( cfp, "TERMINATOR NONE '\\x0d\\x0a' 2 AS 0 DELIMITER\r\n" );
    }

    fclose ( cfp );
    }
  }
  }
  }
  }

  return 0;
}


# endif


