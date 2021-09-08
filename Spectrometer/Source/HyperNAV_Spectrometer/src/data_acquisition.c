 /*! \file data_acquisition.c *******************************************************
 *
 * \brief Data acquisition task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <sys/time.h>

# include "data_acquisition.h"

# include "data_exchange.spectrometer.h"
# include "data_exchange_packet.h"
# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"

# include "FreeRTOS.h"
# include "semphr.h"
# include "queue.h"

# include "ast.h"
# include "power_clocks_lib.h"

# include "tasks.spectrometer.h"
# include "config.spectrometer.h"

//# include "telemetry.h"
# include "io_funcs.spectrometer.h"
# include "gplp_trace.spectrometer.h"

# include "shutters.h"
# include "lsm303.h"
# include "twi_mux.h"
# include "max6633.h"
# include "pressure.h"
# include "cgs.h"
# include "sn74v283.h"
# include "sram_memory_map.spectrometer.h"
# include "smc_sram.h"
# include "smc_fifo_A.h"
# include "smc_fifo_B.h"
# include "FlashMemory.h"

# include "SunPosition.h"

//*****************************************************************************
// Settings
//*****************************************************************************

//*****************************************************************************
// Local Variables
//*****************************************************************************

// Task control
static Bool gRunTask = false;
static Bool gTaskIsRunning = false;

static xSemaphoreHandle gTaskCtrlMutex = NULL;

// packet receiving queue
# define N_RX_PACKETS 16
static xQueueHandle rxPackets = NULL;

//  Expect to need four pointers for spectrometer data,
//  two for actively collecting,
//  and two that are sent around via packets to other task

typedef union
{
  Spectrometer_Data_t spec_data;
} any_acquired_data_t;

# define NumSpectrometers 2  //  Spectrometer 0 == A == PORT      == LEFT
                             //  Spectrometer 1 == B == STARBOARD == RIGHT
# define NumADP 4
static data_exchange_data_package_t acquired_data_package[NumADP] = { {0, 0, 0} };

# define UVDARKS 0   //  While shutters not operational

//*****************************************************************************
// Local Functions
//*****************************************************************************
//static Bool parseAndTimeStamp(U8 port, char* header, U16 expectedFrameLen,  U16 *foundFrameLen);
//static void timevalToSatlanticTimestamp(const struct timeval* timeval, U8* SatlanticTimestamp);
//static U8 createPyrometerFrame(char* frame, U16 max_len, pyro_msg_t* pyrometer);

# if 0
static char* flt2str ( float f, char string[], int dd ) {

  int idx=0;

  if ( f<0 ) { string[idx++] = '-'; f = -f; }

  int int_f = (int)floor(f);
  float dec_f = f - int_f;

  int mask = 9;
  while ( mask < int_f ) mask *= 10;
  mask /= 9;

  do {
    int digit = int_f / mask;
    string[idx++] = '0' + digit;
    int_f -= digit * mask;
    mask /= 10;

  } while ( mask );

  if ( dd>0 ) {
    string[idx++] = '.';

    while ( dd>0 ) {

      dec_f *= 10;

      int dig_f = (int)floor(dec_f);
      string[idx++] = '0' + dig_f;

      dec_f -= dig_f;
      dd--;
    }
  }

  string [idx] = 0;

  return string;
}
# endif

static char* int2str ( int v, char string[] )
{

  if ( !v ) { string[0] = '0'; string[1] = 0; return string; }

  int idx=0;

  if ( v<0 ) { string[idx++] = '-'; v = -v; }

  int mask = 9;
  while ( mask < v ) mask *= 10;
  mask /= 9;

  do {
    int digit = v / mask;
    string[idx++] = '0' + digit;
    v -= digit * mask;
    mask /= 10;

  } while ( mask );

  string [idx] = 0;

  return string;
}


static void Init_GPS_Position ( gps_data_t* gps ) {
  gps -> lon = CFG_Get_Longitude();
  gps -> lat = CFG_Get_Latitude();
  gps -> magnetic_declination = CFG_Get_Magnetic_Declination();
}

# define PRES_AT_SURFACE 10.5

static float PRES_fake( float start_pressure, float ascent_rate ) {

    static float  PDeep = 0;
    static float  Rate  = 0;
    static time_t TDeep = 0;

    struct timeval now;
    gettimeofday( &now, NULL) ;

    if ( start_pressure > 0 ) {
        PDeep = start_pressure;
        Rate  = ascent_rate;
        TDeep = now.tv_sec;
        return start_pressure;
    } else {
        float p = PDeep - Rate * ( ( now.tv_sec - TDeep ) + ((double)now.tv_usec)/1000000.0 );
        return ( p>PRES_AT_SURFACE ) ? p : PRES_AT_SURFACE;
    }
}

static void Init_DigiQuartz_Calibration_Coefficients ( DigiQuartz_Calibration_Coefficients_t* dq_cc ) {
  dq_cc -> U0 = CFG_Get_Pres_U0();
  dq_cc -> Y1 = CFG_Get_Pres_Y1();
  dq_cc -> Y2 = CFG_Get_Pres_Y2();
  dq_cc -> Y3 = CFG_Get_Pres_Y3();
  dq_cc -> C1 = CFG_Get_Pres_C1();
  dq_cc -> C2 = CFG_Get_Pres_C2();
  dq_cc -> C3 = CFG_Get_Pres_C3();
  dq_cc -> D1 = CFG_Get_Pres_D1();
  dq_cc -> D2 = CFG_Get_Pres_D2();
  dq_cc -> T1 = CFG_Get_Pres_T1();
  dq_cc -> T2 = CFG_Get_Pres_T2();
  dq_cc -> T3 = CFG_Get_Pres_T3();
  dq_cc -> T4 = CFG_Get_Pres_T4();
  dq_cc -> T5 = CFG_Get_Pres_T5();
}

static void Init_Acc_Mag_Coefficients (
  float*              mounting_angle,
  lsm303_Accel_Raw_t* vertical,
  lsm303_Mag_Raw_t*   mag_min,
  lsm303_Mag_Raw_t*   mag_max ) {

  *mounting_angle = CFG_Get_Acc_Mounting();
  vertical -> x = CFG_Get_Acc_X();
  vertical -> y = CFG_Get_Acc_Y();
  vertical -> z = CFG_Get_Acc_Z();
  mag_min  -> x = CFG_Get_Mag_Min_X();
  mag_min  -> y = CFG_Get_Mag_Min_Y();
  mag_min  -> z = CFG_Get_Mag_Min_Z();
  mag_max  -> x = CFG_Get_Mag_Max_X();
  mag_max  -> y = CFG_Get_Mag_Max_Y();
  mag_max  -> z = CFG_Get_Mag_Max_Z();
}

typedef struct Profile_Descripion {
  float upper_interval;
  float upper_start;
  float middle_interval;
  float middle_start;
  float lower_interval;
  float lower_start;
} Profile_Descripion_t;

static int Init_APM ( float point[], int max ) {

  Profile_Descripion_t pd;

  pd . upper_interval  = CFG_Get_Prof_Upper_Interval();
  pd . upper_start     = CFG_Get_Prof_Upper_Start();
  pd . middle_interval = CFG_Get_Prof_Middle_Interval();
  pd . middle_start    = CFG_Get_Prof_Middle_Start();
  pd . lower_interval  = CFG_Get_Prof_Lower_Interval();
  pd . lower_start     = CFG_Get_Prof_Lower_Start();

  int nPoints = 0;

  float pressure = pd.lower_start;
//char ppp[24];
  do {
    point[nPoints] = pressure;
//io_out_string ( "APM-PP[" );
//io_out_string( int2str( nPoints, ppp ) );
//io_out_string ( "] = " );
//io_out_string( flt2str( pressure, ppp, 4 ) );
//io_out_string ( "\r\n" );
    nPoints++;
    pressure -= pd.lower_interval;
  } while ( pressure > pd.middle_start && nPoints<max-2 );

  pressure = pd.middle_start;

  do {
    point[nPoints] = pressure;
//io_out_string ( "APM-PP[" );
//io_out_string( int2str( nPoints, ppp ) );
//io_out_string ( "] = " );
//io_out_string( flt2str( pressure, ppp, 4 ) );
//io_out_string ( "\r\n" );
    nPoints++;
    pressure -= pd.middle_interval;
  } while ( pressure > pd.upper_start && nPoints<max-1 );

  point[nPoints] = pd.upper_start;
//io_out_string ( "APM-PP[" );
//io_out_string( int2str( nPoints, ppp ) );
//io_out_string ( "] = " );
//io_out_string( flt2str( pressure, ppp, 4 ) );
//io_out_string ( "\r\n" );
  nPoints++;

  return nPoints;
}

typedef enum {
  PRF_Complete,   // Surfaced
  PRF_Continuous, // Upper profile section
  PRF_Single,     // Take a single measurement
  PRF_Delay_0,    // Getting close, check again
//PRF_Delay_1m,   // ETA at 1 minute
//PRF_Delay_2m,   // ETA at 2 minutes
//PRF_Delay_5m,   // ETA at 5 minutes
//PRF_Delay_10m   // ETA at 10 minutes
} Profile_Action_t;

static Profile_Action_t measureNow ( double current, float point[], int next, int last )
{

//char ppp[16];
//io_out_string ( "NeasureNow " );
//io_out_string( flt2str( current, ppp, 4 ) );
//io_out_string ( " <> " );
//io_out_string( flt2str( point[next], ppp, 4 ) );
//io_out_string ( " [ " );
//io_out_string( int2str( next, ppp ) );
//io_out_string ( " ] " );
//io_out_string ( " [ " );
//io_out_string( int2str( last, ppp ) );
//io_out_string ( " ]" );

  if ( current < point[last-1] ) {
//io_out_string ( " C\r\n" );
    return PRF_Continuous;  //  Continuous sampling
  }

  if ( current <= point[next] ) {
//io_out_string ( " S\r\n" );
    return PRF_Single;
  } else {
//io_out_string ( " D\r\n" );
    return PRF_Delay_0;
  }
}

//*****************************************************************************
// Local Tasks Implementation
//*****************************************************************************

// Data acquisition task
static void DAQ_Task( void* pvParameters ) {

# define _static_ static

int const DBG = 0;  //  ALERT: Changing to 1 typically causes watchdog resets !!!

# if 0
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif

# define NumControls      NumSpectrometers
                      //  1 == Spectrometers run in sync

  //  Declare static to avoid putting on heap ???
  //
# define N_SPEC_SKIP     10
# define N_SPEC_MARGINS  21
  static U16 darkSpectrum[NumSpectrometers][N_SPEC_PIX+N_SPEC_MARGINS];
  static U16 lghtSpectrum[NumSpectrometers][N_SPEC_PIX+N_SPEC_MARGINS];
//U32    startIntegration[NumSpectrometers];
//U32      endIntegration[NumSpectrometers];

  //  Carry readout quality from readout to data transfer
  //
  // uint16_t readoutOk[NumSpectrometers];

  //  AST time value, provided by spectrometer API.
  //  Useful to determine precise time of data acquisition.
  //
//U32    startReadout;  //  Not sure if required

  //  GPS position
  //
  _static_ gps_data_t gps_pos;

  //  This variable carries the accelerometer sensor calibration vector.
  //
  _static_ float  lsm303_mounting_angle = 0;

  _static_ lsm303_Accel_Raw_t lsm303_vertical;  //  Initialized at start of data acquisition
  _static_ float   housing_pitch   [NumSpectrometers][2];  //  2 for measuring average and standard deviation
  _static_ float   housing_roll    [NumSpectrometers][2];
  _static_ float   housing_tilt    [NumSpectrometers][2];
  _static_ int16_t tilt_measurements[NumSpectrometers] = {0};

  _static_ lsm303_Mag_Raw_t lsm303_mag_min;  //  Initialized at start of data acquisition
  _static_ lsm303_Mag_Raw_t lsm303_mag_max;  //  Initialized at start of data acquisition
  _static_ float   housing_heading [NumSpectrometers][2];  //  2 for measuring average and standard deviation
  _static_ int16_t heading_measurements[NumSpectrometers] = {0};

  //  FIXME - Not implemented yet
  //
  _static_ float   spectrometer_pitch      [NumSpectrometers][2];
  _static_ float   spectrometer_roll       [NumSpectrometers][2];
  _static_ int16_t spectrometer_measurement[NumSpectrometers] = {0};

  //  Measurement of Spectrometer Temperature
  //
  _static_ int16_t spectrometer_temp       [NumSpectrometers] = { 0, 0 };

  //  Pressure measurements
  //
  _static_ DigiQuartz_Calibration_Coefficients_t dq_cc = { 0 };
  _static_ digiquartz_t dq_sensor;

  _static_ Bool         run_pressure_sensor   = false;
  _static_ Bool             pressure_started  = false;
  _static_ struct timeval   pressure_start_time;

  _static_ float          last_pressure_value = -99;
  _static_ struct timeval last_pressure_time = { 0, 0 };

  //  For testing in the absence of pressure,
  //  can simulate a profile. -- !!! Not Tested Yet !!!
  //
  _static_ Bool simulate_pressure_profile = false;

# define APM_Point_Max 32
  _static_ float                APM_Point_Pressure[APM_Point_Max];
  _static_ int                  APM_Point_Next = 0;
  _static_ int                  APM_Point_Last = 0;

  _static_ float           pressure_value [NumSpectrometers];
  _static_ struct timeval  pressure_time  [NumSpectrometers];
//int             t_counts [NumSpectrometers] = { 0, 0 };
//U32             t_durat  [NumSpectrometers] = { 0, 0 };
//int             p_counts [NumSpectrometers] = { 0, 0 };
//U32             p_durat  [NumSpectrometers] = { 0, 0 };

# define INTEGRATION_TIME_STEP_FACTOR_IS_2

# ifdef INTEGRATION_TIME_STEP_FACTOR_IS_2
//U16 const available_integration_times[] = { 11, 16,     32,     64,     128,      256,      512,      1024,       1999 };
  _static_ U16 const available_integration_times[] = { 11, 20,     40,     80,     160,      320,      640,      1280,       1920 };
  _static_ int16_t const adjust_factor = 4;
# else
  _static_ U16 const available_integration_times[] = { 11, 16, 23, 32, 45, 64, 91, 128, 181, 256, 362, 512, 724, 1024, 1448, 1999 };  //  Units of milliseconds
  _static_ int16_t const adjust_factor = 4;
# endif
  int const number_of_integration_times = sizeof(available_integration_times) / sizeof(U16);

  typedef enum {

    DAQ_Stt_Idle,

    DAQ_Stt_FloatProfile,

    DAQ_Stt_Calibrate,
    DAQ_Stt_DarkCharacterize,
    DAQ_Stt_StreamLMD,
    DAQ_Stt_StreamLandD,

//  DAQ_Stt_OffloadData,

  } DAQ_STATE_t; DAQ_STATE_t DAQ_state = DAQ_Stt_Idle;

  int OCR_started = 0;

  Bool prepareToStop = false;
  Bool offload_FlashMemory = false;

  U16 Fixed_Integration_Time = 0;  //  a value of 0 indicates that the integration time will be adjusted based on spectrum values
  int Lights_Per_Dark = 10;
  int Darks_Per_Integration_Time = 4;  //  Used in DAQ_Stt_DarkCharacterize only

  //  Data Acquisiton is implemented as an infinite loop,
  //  where the Phase determines which action will be executed
  //  in a particular loop iteration.
  typedef enum {
    DAQ_PHS_Idle,
    DAQ_PHS_CloseShutter,
    DAQ_PHS_StartDark,
    DAQ_PHS_GetDark,
    DAQ_PHS_TransferD,
    DAQ_PHS_OpenShutter,
    DAQ_PHS_StartLight,
    DAQ_PHS_GetLight,
    DAQ_PHS_TransferL,
    DAQ_PHS_TransferLMD
  } DAQ_PHASE_t; DAQ_PHASE_t DAQ_Phase[NumSpectrometers];

  _static_ U16          dark_fifo_over   [NumSpectrometers] = { 1, 1 };
  _static_ U16          lght_fifo_over   [NumSpectrometers] = { 1, 1 };
  _static_ U16          shutter_is_closed[NumSpectrometers] = { 1, 1 };
  _static_ U16        dark_sample_number [NumSpectrometers];
  _static_ U16       light_sample_number [NumSpectrometers];
  _static_ U16  current_integration_idx  [NumSpectrometers];  //  index to available_integration_times[]
  _static_ U16  current_integration_time [NumSpectrometers];  //  units of milliseconds
  _static_ U16     next_integration_idx  [NumSpectrometers];  //  index to available_integration_times[]
  _static_ U16                  dark_avg [NumSpectrometers];
  _static_ U16                  dark_sdv [NumSpectrometers];
  _static_ U16                  dark_min [NumSpectrometers];
  _static_ U16                  dark_max [NumSpectrometers];
  _static_ U16                  lght_min [NumSpectrometers];
  _static_ U16                  lght_max [NumSpectrometers];
  _static_ int         lights_after_dark [NumSpectrometers];

  int firstSpec = 1;
  int  lastSpec = 0;

  int spectrometer;
  for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ ) {
    DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
    lights_after_dark[spectrometer] = 0;
  }

  Profile_Action_t PRF_action = PRF_Complete;

  data_exchange_address_t const myAddress = DE_Addr_DataAcquisition;

  for(;;) {

    if(gRunTask) {

      gTaskIsRunning = true;

# if 0
      //  Report stack water mark, if changed to previous water mark
      //
      if ( INCLUDE_uxTaskGetStackHighWaterMark ) { }
      stackWaterMark = uxTaskGetStackHighWaterMark(0);

      //  DEBUG/DEVELOPMENT only
      if ( stackWaterMark < stackWaterMark_threshold ) {
        while ( stackWaterMark < stackWaterMark_threshold ) stackWaterMark_threshold/=2;
        data_exchange_packet_t packet_message;
        packet_message.to   = DE_Addr_ControllerBoard_Commander;
        packet_message.from = myAddress;
        packet_message.type = DE_Type_Syslog_Message;
        packet_message.data.Syslog.number = SL_MSG_StackWaterMark;
        packet_message.data.Syslog.value  = stackWaterMark;
    //  data_exchange_packet_router ( myAddress, &packet_message );
      }
# endif

      //  Check for incoming packets
      //  ERROR if packet.to not myself (myAddress)
      //  Accept   type == Ping, Command
      //  ERROR if other type

      data_exchange_packet_t packet_rx_via_queue;

      if ( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) )
      {

        if ( packet_rx_via_queue.to != myAddress )
        {
          //  This is not supposed to happen
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
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
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

        }
        else
        {
          //  If there is a need to respond,
          //  the following address will be overwritten.
          //  A non DE_Addr_Nobody value will be used as a flag
          //  to send the packet_response.
          data_exchange_packet_t packet_response;
          packet_response.to = DE_Addr_Nobody;

          switch ( packet_rx_via_queue.type ) {

          case DE_Type_Nothing:
            break;

          case DE_Type_Ping:
            packet_response.to = packet_rx_via_queue.from;
            packet_response.from = myAddress;
            packet_response.type = DE_Type_Response;
            packet_response.data.Response.Code = RSP_ALL_Ping;
            packet_response.data.Response.value.rsp = RSP_ANY_ACK;  //  Will be ignored
            break;

          case DE_Type_Command:

            switch ( packet_rx_via_queue.data.Command.Code ) {

            case CMD_ANY_Query:
              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_DAQ_Query;
              packet_response.data.Response.value.s16[0] = DAQ_state;
              packet_response.data.Response.value.s16[1] = DAQ_Phase[0];
              packet_response.data.Response.value.s16[2] = 0;
              packet_response.data.Response.value.s16[3] = 0;
              break;

            case CMD_DAQ_Start:

              if (1)
              {  //  For DEBUG/DEVELOPMENT only
                data_exchange_packet_t packet_message;
                packet_message.to   = DE_Addr_ControllerBoard_Commander;
                packet_message.from = myAddress;
                packet_message.type = DE_Type_Syslog_Message;
                packet_message.data.Syslog.number = SL_MSG_DAQ_Started;
                packet_message.data.Syslog.value  = packet_rx_via_queue.data.Command.value.s16[0];
                data_exchange_packet_router ( myAddress, &packet_message );
              }

              if  ( DAQ_state == DAQ_Stt_Idle )
              {

                switch ( packet_rx_via_queue.data.Command.value.s16[0] ) {
                case 0: //  Stream and generate LMD frames - also acquire OCR & MCOMS

                  //  Pass on start command to auxiliary_data_acquisition()
                  //
                  if ( DAQ_Stt_Idle == DAQ_state || DAQ_Stt_StreamLMD == DAQ_state ) {
                    packet_response.to   = DE_Addr_AuxDataAcquisition;
                    packet_response.from = myAddress;
                    packet_response.type = DE_Type_Command;
                    packet_response.data.Command.Code = CMD_ADQ_StartStreaming;
                    packet_response.data.Command.value.s32[0] = 0;   //  For REAL mode
                  }

                  if  ( DAQ_Stt_Idle == DAQ_state )
                  {

                    Fixed_Integration_Time = packet_rx_via_queue.data.Command.value.s16[1];
                    if ( Fixed_Integration_Time ) {
                      if ( Fixed_Integration_Time < available_integration_times[0] ) {
                              Fixed_Integration_Time = available_integration_times[0];
                      }
                      if ( Fixed_Integration_Time > available_integration_times[number_of_integration_times-1] ) {
                              Fixed_Integration_Time = available_integration_times[number_of_integration_times-1];
                      }
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ ) {
                        current_integration_time[spectrometer] = Fixed_Integration_Time;
                      }
                    } else {
                      //  Start at intermediate integration time
                      //
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ ) {
                        current_integration_idx [spectrometer] = number_of_integration_times/2-1;
                        current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                        next_integration_idx [spectrometer] = current_integration_idx [spectrometer];
                      }
                    }

                    Lights_Per_Dark        = packet_rx_via_queue.data.Command.value.s16[2];
                    if ( Lights_Per_Dark < 1 ) Lights_Per_Dark = 10;

                    run_pressure_sensor = true;

                    DAQ_state = DAQ_Stt_StreamLMD;
                  }

                  break;

                case 1: //  Stream and generate separate L and D frames - also acquire OCR & MCOMS

                  //  Pass on start command to auxiliary_data_acquisition()
                  //
                  if ( DAQ_Stt_Idle == DAQ_state || DAQ_Stt_StreamLandD == DAQ_state ) {
                    packet_response.to   = DE_Addr_AuxDataAcquisition;
                    packet_response.from = myAddress;
                    packet_response.type = DE_Type_Command;
                    packet_response.data.Command.Code = CMD_ADQ_StartStreaming;
                  //packet_response.data.Command.value.s32[0] = -1;  //  For FAKE mode
                    packet_response.data.Command.value.s32[0] = 0;   //  For REAL mode
                  }

                  if ( DAQ_Stt_Idle == DAQ_state )
                  {
                    Fixed_Integration_Time = packet_rx_via_queue.data.Command.value.s16[1];
                    if ( Fixed_Integration_Time )
                    {
                      if  (Fixed_Integration_Time < available_integration_times[0])
                      {
                        Fixed_Integration_Time = available_integration_times[0];
                      }
                      if ( Fixed_Integration_Time > available_integration_times[number_of_integration_times-1] )
                      {
                        Fixed_Integration_Time = available_integration_times[number_of_integration_times-1];
                      }
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ )
                      {
                        current_integration_time[spectrometer] = Fixed_Integration_Time;
                      }
                    }
                    else
                    {
                      //  Start at short integration time
                      //
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ )
                      {
                        current_integration_idx [spectrometer] = 0;
                        current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                        next_integration_idx [spectrometer] = current_integration_idx [spectrometer];
                      }
                    }

                    Lights_Per_Dark        = packet_rx_via_queue.data.Command.value.s16[2];
                    if ( Lights_Per_Dark < 1 ) Lights_Per_Dark = 10;

                    run_pressure_sensor = true;

                    DAQ_state = DAQ_Stt_StreamLandD;
                  }

                  break;

                case 2: //  Calibration mode, no pressure, separate L and D frames - no OCR or MCOMS
                  if ( DAQ_Stt_Idle == DAQ_state )
                  {
                    Fixed_Integration_Time = packet_rx_via_queue.data.Command.value.s16[1];
                    if ( Fixed_Integration_Time )
                    {
                      if ( Fixed_Integration_Time < available_integration_times[0] )
                      {
                        Fixed_Integration_Time = available_integration_times[0];
                      }
                      if ( Fixed_Integration_Time > available_integration_times[number_of_integration_times-1] )
                      {
                        Fixed_Integration_Time = available_integration_times[number_of_integration_times-1];
                      }
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ )
                      {
                        current_integration_time[spectrometer] = Fixed_Integration_Time;
                      }
                    }
                    else
                    {
                      //  Start at intermediate integration time
                      //
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ )
                      {
                        current_integration_idx [spectrometer] = number_of_integration_times/2-1;
                        current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                        next_integration_idx [spectrometer] = current_integration_idx [spectrometer];
                      }
                    }

                    Lights_Per_Dark = packet_rx_via_queue.data.Command.value.s16[2];
                    if  (Lights_Per_Dark < 1)
                      Lights_Per_Dark = 10;

                    //  Not needed for calibration,
                    //  but needed to power the daughter board  ???  FIXME ???
                    //
                    run_pressure_sensor = false;

                    DAQ_state = DAQ_Stt_Calibrate;
                  }

                  break;

                case 3: //  Dark characterizaton, no pressure, only D frames - no OCR or MCOMS

                  if ( DAQ_Stt_Idle == DAQ_state )
                  {

                    Fixed_Integration_Time = packet_rx_via_queue.data.Command.value.s16[1];
                    if ( Fixed_Integration_Time )
                    {
                      if ( Fixed_Integration_Time < available_integration_times[0] )
                      {
                        Fixed_Integration_Time = available_integration_times[0];
                      }
                      if ( Fixed_Integration_Time > available_integration_times[number_of_integration_times-1] )
                      {
                        Fixed_Integration_Time = available_integration_times[number_of_integration_times-1];
                      }
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++)
                      {
                        current_integration_time[spectrometer] = Fixed_Integration_Time;
                      }
                    }
                    else
                    {
                      for ( spectrometer=0; spectrometer<NumSpectrometers; spectrometer++ )
                      {
                        current_integration_idx [spectrometer] = 0;
                        current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                        next_integration_idx [spectrometer] = current_integration_idx [spectrometer];
                      }
                    }

                    Darks_Per_Integration_Time = packet_rx_via_queue.data.Command.value.s16[2];
                    if ( Darks_Per_Integration_Time <= 0 ) Darks_Per_Integration_Time = 4;

                    //  Not needed for dark characterization,
                    //  but needed to power the daughter board  ???  FIXME ???
                    //
                    run_pressure_sensor = false;

                    DAQ_state = DAQ_Stt_DarkCharacterize;
                  }

                  break;
                }

                if ( z CFG_Get_Frame_Port_Serial_Number() || CFG_Get_Frame_Starboard_Serial_Number() )
                {
                  //  Start Spectrometers
                  //
                  CGS_Power_On ( (Bool)(CFG_Get_Frame_Port_Serial_Number()     !=0),
                                 (Bool)(CFG_Get_Frame_Starboard_Serial_Number()!=0) );
                  CGS_Initialize((bool)(CFG_Get_Frame_Port_Serial_Number()     !=0),
                                 (bool)(CFG_Get_Frame_Starboard_Serial_Number()!=0),false);

                  //  Prepare for shutter operation
                  //
                  shutters_power_on();
                }

                firstSpec = CFG_Get_Frame_Port_Serial_Number()      ? 0 : 1;
                lastSpec  = CFG_Get_Frame_Starboard_Serial_Number() ? 1 : 0;

                if ( run_pressure_sensor )
                {
                  float const start_pressure = CFG_Get_Pres_Simulated_Depth();
                  float const ascent_rate    = 0.01*CFG_Get_Pres_Simulated_Ascent_Rate();
                  if ( start_pressure>0 && ascent_rate>0 )
                  {
                    simulate_pressure_profile = true;
                    PRES_fake( start_pressure, ascent_rate );
                    run_pressure_sensor = false;
                  }
                  else
                  {
                    simulate_pressure_profile = false;
                  }
                }


                if ( run_pressure_sensor ) {
                  //  Start Pressure Sensor
                  //
                  PRES_power(true);
                  vTaskDelay(1000);
                  if ( PRES_Initialize() )
                  {
                    //  Failed to initialize, don't collect pressure during data acquisition
                    run_pressure_sensor = false;
                  }
                  else
                  {
                    vTaskDelay(250);
                    Init_DigiQuartz_Calibration_Coefficients ( &dq_cc );
                  }
                }

                //  Initialize last known GPS position
                //
                Init_GPS_Position( &gps_pos );

                //  Start housing tilt and heading sensor
                //
                lsm303_init();
                Init_Acc_Mag_Coefficients ( &lsm303_mounting_angle, &lsm303_vertical, &lsm303_mag_min, &lsm303_mag_max );

                //  Start data acquisition
                //
                for  (spectrometer = 0;  spectrometer < NumSpectrometers;  spectrometer++)
                {
                     dark_sample_number [spectrometer] = 0;
                    light_sample_number [spectrometer] = 0;
                }

                if ( CFG_Get_Frame_Port_Serial_Number()      ) DAQ_Phase[0] = DAQ_PHS_CloseShutter;
                else                                           DAQ_Phase[0] = DAQ_PHS_Idle;

                if ( CFG_Get_Frame_Starboard_Serial_Number() ) DAQ_Phase[1] = DAQ_PHS_CloseShutter;
                else                                           DAQ_Phase[1] = DAQ_PHS_Idle;
              }

              break;

            case CMD_DAQ_Stop:

              if ( DAQ_state != DAQ_Stt_Idle ) {
                prepareToStop = true;
              }

              if (1)
              {  //  For DEBUG/DEVELOPMENT only
                data_exchange_packet_t packet_message;
                packet_message.to   = DE_Addr_ControllerBoard_Commander;
                packet_message.from = myAddress;
                packet_message.type = DE_Type_Syslog_Message;
                packet_message.data.Syslog.number = SL_MSG_DAQ_Stopped;
                packet_message.data.Syslog.value  = 0;
                data_exchange_packet_router ( myAddress, &packet_message );
              }
       
              //  If auxiliary data acquiring, pass on stop command 
              //
              if ( DAQ_state == DAQ_Stt_FloatProfile
                || DAQ_state == DAQ_Stt_StreamLMD
                || DAQ_state == DAQ_Stt_StreamLandD )
              {
                packet_response.to   = DE_Addr_AuxDataAcquisition;
                packet_response.from = myAddress;
                packet_response.type = DE_Type_Command;
                packet_response.data.Command.Code = CMD_ADQ_Stop;
                packet_response.data.Command.value.s64 = 0;
              }

              break;

            case CMD_DAQ_Profiling:

              if (1)
              {  //  For DEBUG/DEVELOPMENT only
                data_exchange_packet_t packet_message;
                packet_message.to   = DE_Addr_ControllerBoard_Commander;
                packet_message.from = myAddress;
                packet_message.type = DE_Type_Syslog_Message;
                packet_message.data.Syslog.number = SL_MSG_DAQ_Profiling;
                packet_message.data.Syslog.value  = 0;
                data_exchange_packet_router ( myAddress, &packet_message );
              }

//            //  Pass on start command to auxiliary_data_acquisition()
//            //
//            if ( DAQ_Stt_Idle == DAQ_state || DAQ_Stt_FloatProfile == DAQ_state ) {
//                packet_response.to   = DE_Addr_AuxDataAcquisition;
//                packet_response.from = myAddress;
//                packet_response.type = DE_Type_Command;
//                packet_response.data.Command.Code = CMD_ADQ_StartStreaming;
//                packet_response.data.Command.value.s32[0] = 0;   //  For REAL mode
//            }

              if ( DAQ_state == DAQ_Stt_Idle )
              {

                if ( CFG_Get_Frame_Port_Serial_Number() || CFG_Get_Frame_Starboard_Serial_Number() )
                {
                    firstSpec = ( CFG_Get_Frame_Port_Serial_Number()      ) ? 0 : 1;
                     lastSpec = ( CFG_Get_Frame_Starboard_Serial_Number() ) ? 1 : 0;
                }

                Fixed_Integration_Time = 0;
                Lights_Per_Dark = packet_rx_via_queue.data.Command.value.s64;

                //  Start at longest integration time
                //
                for  (spectrometer = 0;  spectrometer < NumSpectrometers;  spectrometer++)
                {
                  current_integration_idx  [spectrometer] = number_of_integration_times - 1;
                  current_integration_time [spectrometer] = available_integration_times [current_integration_idx[spectrometer]];
                  next_integration_idx     [spectrometer] = current_integration_idx     [spectrometer];
                }

                //  Start Pressure Sensor
                //
                run_pressure_sensor = true;
                {
                  float const start_pressure = CFG_Get_Pres_Simulated_Depth ();
                  float const ascent_rate    = 0.01 * CFG_Get_Pres_Simulated_Ascent_Rate ();
                  if  (start_pressure>0 && ascent_rate > 0)
                  {
                    simulate_pressure_profile = true;
                    PRES_fake( start_pressure, ascent_rate );
                    run_pressure_sensor = false;
                  }
                  else
                  {
                    simulate_pressure_profile = false;
                  }
                }

                if ( run_pressure_sensor )
                {
                  //  Start Pressure Sensor
                  //
                  PRES_power(true);
                  vTaskDelay(1000);
                  if ( !PRES_Initialize() )
                  {
                  //  FIXME --  Cannot profile without pressure sensor!!!
                  }
                  vTaskDelay(250);
                }

                Init_DigiQuartz_Calibration_Coefficients ( &dq_cc );

                //  Define the measurement points
                //
                APM_Point_Last = Init_APM( APM_Point_Pressure, APM_Point_Max );
                APM_Point_Next = 0;

                //  Initialize last known GPS position
                //
                Init_GPS_Position (&gps_pos);

                //  Start housing tilt and heading sensor
                //
                lsm303_init();
                Init_Acc_Mag_Coefficients ( &lsm303_mounting_angle, &lsm303_vertical, &lsm303_mag_min, &lsm303_mag_max );

                //  Start data acquisition
                //
                for  (spectrometer = 0;  spectrometer < NumSpectrometers;  spectrometer++)
                {
                  dark_sample_number  [spectrometer] = 0;
                  light_sample_number [spectrometer] = 0;
                  DAQ_Phase           [spectrometer] = DAQ_PHS_Idle;
                }
                PRF_action = PRF_Delay_0;

                //
                //  Start Spectrometers will be done when appropriate depths are reached.
                //

                DAQ_state = DAQ_Stt_FloatProfile;
              }

              break;

            case CMD_DAQ_Profiling_End:

              if (1)
              {  //  For DEBUG/DEVELOPMENT only
                data_exchange_packet_t packet_message;
                packet_message.to   = DE_Addr_ControllerBoard_Commander;
                packet_message.from = myAddress;
                packet_message.type = DE_Type_Syslog_Message;
                packet_message.data.Syslog.number = SL_MSG_DAQ_ProfileEnd;
                packet_message.data.Syslog.value  = 0;
                data_exchange_packet_router ( myAddress, &packet_message );
              }

              if  ( DAQ_state == DAQ_Stt_FloatProfile )
              {
                prepareToStop = true;
                //  stop ADQ
                //  packet_response.to   = DE_Addr_AuxDataAcquisition;
                //  packet_response.from = myAddress;
                //  packet_response.type = DE_Type_Command;
                //  packet_response.data.Command.Code = CMD_ADQ_Stop;
                //  packet_response.data.Command.value.s64 = 0;
              }
              break;

            default:
              //  Ignore all other command values
              break;
            }

            break;

          case DE_Type_Response:
          case DE_Type_Syslog_Message:
            //  Currently ignoring because expecting none
            break;

          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            //  Unexpected.
            //  Clean up data regardless.

            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) )
            {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM )
              {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              }
              else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM )
              {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }

          break;


          //  Do NOT add a "default:" here.
          //  That way, the compiler will generate a warning
          //  if one possible packet type was missed.
          }

          if ( packet_response.to != DE_Addr_Nobody )
          {
            data_exchange_packet_router ( myAddress, &packet_response );
          }

        }
      }

      if  ( prepareToStop
        && DAQ_Phase[0] == DAQ_PHS_Idle
        && DAQ_Phase[1] == DAQ_PHS_Idle )
      {

        //  Stop Spectrometers
        //
        CGS_Power_Off();

        //  Turn off shutter power supply
        //
        shutters_power_off();

        if  ( run_pressure_sensor )
        {
            //  Stop Pressure Sensor
            //
          # warning THIS MAY NEED TO BE FIXED
            // TODO
            // Replace by PRES_DeInitialize()
            PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE,(digiquartz_t*)0, (F64*)0, (int*)0, (U32*)0, (int16_t)1 );
            PRES_power(false);
        }

        simulate_pressure_profile = false;

        data_exchange_packet_t packet_message;
        packet_message.to   = DE_Addr_ProfileManager;
        packet_message.from = myAddress;
        packet_message.type = DE_Type_Command;
        packet_message.data.Command.Code = CMD_PMG_Offloading;
	      //  TODO - Add number of frames that will be transferred,
	      //  maybe reported separately for port/startboard and light/dark?
        packet_message.data.Command.value.s16[0] = 0;
        packet_message.data.Command.value.s16[1] = 0;
        packet_message.data.Command.value.s16[2] = 0;
        packet_message.data.Command.value.s16[3] = 0;
        data_exchange_packet_router ( myAddress, &packet_message );

        DAQ_state = DAQ_Stt_Idle;
        prepareToStop = false;

        offload_FlashMemory = true;
      }

      if ( offload_FlashMemory )
      {
        if ( FlashMemory_IsEmpty() )
        {
          data_exchange_packet_t packet_message;
          packet_message.to   = DE_Addr_ProfileManager;
          packet_message.from = myAddress;
          packet_message.type = DE_Type_Command;
          packet_message.data.Command.Code = CMD_PMG_Offloading_Done;
          data_exchange_packet_router ( myAddress, &packet_message );
          offload_FlashMemory = false;
	      }

        else
        {
          io_out_S32( "x%ld", FlashMemory_nOfFrames() );

          int adp; int use_adp = -1;

          for ( adp=0; adp<NumADP && use_adp==-1; adp++ )
          {
            if ( pdTRUE == xSemaphoreTake ( acquired_data_package[adp].mutex, 250 /*portMAX_DELAY*/ /*15*/ ) )
            {
              if ( acquired_data_package[adp].state == EmptyRAM )
              {
                use_adp = adp;
              }
              else
              {
                xSemaphoreGive ( acquired_data_package[adp].mutex );
              }
            }
          }

          if ( use_adp >= 0 )
          {
            Spectrometer_Data_t* local_data_pointer = (Spectrometer_Data_t*)acquired_data_package[use_adp].address;
            FlashMemory_RetrieveFrame ( local_data_pointer );

            data_exchange_packet_t SDATA_packet;

            SDATA_packet.from = myAddress;

            //  Provide destination of data:
            //    Float Profile -> Profile Manager
            //    Else          -> Stream & Log
            // if ( DAQ_Stt_FloatProfile == DAQ_state ) {
              SDATA_packet.to = DE_Addr_ProfileManager;
            // } else {
            // SDATA_packet.to = DE_Addr_StreamLog;
            // }
                   
            SDATA_packet.type = DE_Type_Spectrometer_Data;
            acquired_data_package[use_adp].state = FullRAM;

            SDATA_packet.data.DataPackagePointer = &acquired_data_package[use_adp];

            //  Send packet to destination, will go onto a queue, managed by another thread
            //
            data_exchange_packet_router ( myAddress, &SDATA_packet );

            xSemaphoreGive ( acquired_data_package[use_adp].mutex );
          }
	      }
      }

      if  ( DAQ_state == DAQ_Stt_FloatProfile  &&  DAQ_Phase[0] == DAQ_PHS_Idle  &&  DAQ_Phase[1] == DAQ_PHS_Idle )
      {
        if  ( PRF_Continuous != PRF_action  &&  PRF_Complete   != PRF_action )
        {
          float currentPressure = 0;

          if  ( simulate_pressure_profile )
          {
            currentPressure = PRES_fake (0.0F, 0.0F);
            //io_out_string( "v" );
            //io_out_string( "_" );
            //char ppp[16];
            //io_out_string( flt2str( currentPressure, ppp, 4 ) );
            //io_out_string( "_" );
          }
          else
          {
            if  ( run_pressure_sensor )
            {
              //  Currently ignoring if the StartMeasurement() calls return an error.
              //  An error is returned if the measurement is already under way,
              //
              PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
              PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
              vTaskDelay( (portTickType)TASK_DELAY_MS( 250 ) );

              F64 Temperature_period;  int Temperature_counts = 0;  U32 Temperature_duration = 0;
              F64    Pressure_period;  int    Pressure_counts = 0;  U32    Pressure_duration = 0;

              if ( PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_TEMPERATURE, &dq_sensor, &Temperature_period, &Temperature_counts, &Temperature_duration, CFG_Get_Pres_TDiv() )
                && PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE,    &dq_sensor, &Pressure_period,    &Pressure_counts,    &Pressure_duration,    CFG_Get_Pres_PDiv() ) )
              {
                currentPressure = PRES_Calc_Pressure_dBar (Temperature_period, Pressure_period, &dq_cc, (F64*)0, (F64*)0 );
              }
              else
              {
                //  FIXME -- Try Again!!!
              }
            }
          }

          PRF_action = measureNow( currentPressure, APM_Point_Pressure, APM_Point_Next, APM_Point_Last );

          switch ( PRF_action )
          {
          case PRF_Single:
            APM_Point_Next++;

          case PRF_Continuous:
            if  ( CFG_Get_Frame_Port_Serial_Number() || CFG_Get_Frame_Starboard_Serial_Number() )
            {
              //  Start Spectrometers
              //
              CGS_Power_On ( (Bool)(CFG_Get_Frame_Port_Serial_Number()     !=0),
                             (Bool)(CFG_Get_Frame_Starboard_Serial_Number()!=0) );
              CGS_Initialize((bool)(CFG_Get_Frame_Port_Serial_Number()     !=0),
                             (bool)(CFG_Get_Frame_Starboard_Serial_Number()!=0), false);

              //  Prepare for shutter operation
              //
              shutters_power_on();

              if  (CFG_Get_Frame_Port_Serial_Number ())
                DAQ_Phase[0] = DAQ_PHS_CloseShutter;
              else
                DAQ_Phase[0] = DAQ_PHS_Idle;

              if  (CFG_Get_Frame_Starboard_Serial_Number ())
                DAQ_Phase[1] = DAQ_PHS_CloseShutter;
              else
                DAQ_Phase[1] = DAQ_PHS_Idle;

              //  Pass on start command to auxiliary_data_acquisition()
              //
              if  ( !OCR_started && PRF_action == PRF_Continuous )
              {
                data_exchange_packet_t packet_cmd;
                packet_cmd.to   = DE_Addr_AuxDataAcquisition;
                packet_cmd.from = myAddress;
                packet_cmd.type = DE_Type_Command;
                packet_cmd.data.Command.Code = CMD_ADQ_StartProfiling;
                packet_cmd.data.Command.value.s32[0] = 0;   //  For REAL mode
                data_exchange_packet_router ( myAddress, &packet_cmd );
                OCR_started = 1;
              }

            }

            firstSpec = CFG_Get_Frame_Port_Serial_Number()      ? 0 : 1;
            lastSpec  = CFG_Get_Frame_Starboard_Serial_Number() ? 1 : 0;
            break;

          case PRF_Delay_0:
            //  case PRF_Delay_1m:
            //  case PRF_Delay_2m:
            //  case PRF_Delay_5m:
            //  case PRF_Delay_10m:
            {
              //data_exchange_packet_t packet_error;
              //packet_error.from               = myAddress;
              //packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              //packet_error.type               = DE_Type_Syslog_Message;
              //packet_error.data.Syslog.number = SL_MSG_DAQ_Pressure;
              //packet_error.data.Syslog.value  = round( 10000*currentPressure );
              //data_exchange_packet_router( myAddress, &packet_error );
            }
            vTaskDelay ( (portTickType)TASK_DELAY_MS( 333 ) );
            break;

          default:
            {
              data_exchange_packet_t packet_error;
            
              packet_error.from               = myAddress;
              packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              packet_error.type               = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_MSG_DAQ_Pressure;
              packet_error.data.Syslog.value  = 999999999;
            
              data_exchange_packet_router (myAddress, &packet_error);
            
              vTaskDelay ((portTickType)TASK_DELAY_MS(2000));
            }
            break;
          }
        }
      }

      if  ( DAQ_state != DAQ_Stt_Idle )
      {
        switch ( DAQ_state )
        {
          case DAQ_Stt_FloatProfile:     io_out_string( "f" ); break;
          case DAQ_Stt_Calibrate:        io_out_string( "c" ); break;
          case DAQ_Stt_DarkCharacterize: io_out_string( "d" ); break;
          case DAQ_Stt_StreamLMD:        io_out_string( "s" ); break;
          case DAQ_Stt_StreamLandD:      io_out_string( "S" ); break;
          default:                       io_out_string( "-" ); break;
        }

        for  ( spectrometer = 0;  spectrometer < NumSpectrometers;  spectrometer++ )
        {
          if ( run_pressure_sensor )
          {
            if ( pressure_started )
            {
           
              //  If the maximum measurement time approaches,
              //  get pressure value and save for potential use.
              //
              struct timeval now;
              gettimeofday( &now, NULL) ;
              double time_diff  = 1000000 * ( now.tv_sec - pressure_start_time.tv_sec );
                     time_diff +=                 now.tv_usec;
                     time_diff -= pressure_start_time.tv_usec;

              //io_out_string ( "p" );
              if ( time_diff > 250000.0 )
              { 
                // > 250 ms
                F64 Temperature_period;  int Temperature_counts = 0;  U32 Temperature_duration = 0;
                F64    Pressure_period;  int    Pressure_counts = 0;  U32    Pressure_duration = 0;

                if ( PRES_Ok == PRES_GetPerioduSecs (DIGIQUARTZ_TEMPERATURE, &dq_sensor, &Temperature_period, &Temperature_counts, &Temperature_duration, CFG_Get_Pres_TDiv() )
                  && PRES_Ok == PRES_GetPerioduSecs (DIGIQUARTZ_PRESSURE,    &dq_sensor, &Pressure_period,    &Pressure_counts,    &Pressure_duration,    CFG_Get_Pres_PDiv() ) )
                {
                  if  ( time_diff < 1000000.0 )
                  {
                    //io_out_string ( "+" );
                    gettimeofday( &now, NULL) ;
                    last_pressure_time.tv_sec  = now.tv_sec;
                    last_pressure_time.tv_usec = now.tv_usec;
                    last_pressure_value = PRES_Calc_Pressure_dBar( Temperature_period, Pressure_period, &dq_cc, (F64*)0, (F64*)0 );
                  }
                  else
                  {
                    //io_out_string ( "-" );
                  }
                }

                pressure_started = false;
              }
              else
              {
                //io_out_string ( "." );
              }
            }

            //  If pressure is not measuring,
            //  start now.
            //
            if  (!pressure_started)
            {
              PRES_StartMeasurement (DIGIQUARTZ_TEMPERATURE);
              PRES_StartMeasurement (DIGIQUARTZ_PRESSURE);
              pressure_started = true;
              gettimeofday (&pressure_start_time, NULL);
            }
          }

          component_selection_t componentID;
          spec_num_t                 SpecID;
          U16*                         fifo;

          switch ( spectrometer )
          {
            //                                                          ((U16*)AVR32_EBI_CS2_ADDRESS)
            case  0: SpecID = SPEC_A; componentID = component_A; fifo = FIFO_DATA_A; break;
            case  1: SpecID = SPEC_B; componentID = component_B; fifo = FIFO_DATA_B; break;
            default: break;
          }

          if  ( DBG )
          {
            static DAQ_STATE_t lastState = DAQ_Stt_Idle;
            static DAQ_PHASE_t lastPhase = DAQ_PHS_Idle;

            data_exchange_packet_t packet_message;
            packet_message.to   = DE_Addr_ControllerBoard_Commander;
            packet_message.from = myAddress;
            packet_message.type = DE_Type_Syslog_Message;
            if ( DAQ_state != lastState )
            {
              packet_message.data.Syslog.number = SL_MSG_DAQ_State;
              packet_message.data.Syslog.value  = DAQ_state;
              data_exchange_packet_router ( myAddress, &packet_message );
              lastState = DAQ_state;
            }
            if ( DAQ_Phase[spectrometer] != lastPhase )
            {
              packet_message.data.Syslog.number = SL_MSG_DAQ_Phase;
              packet_message.data.Syslog.value  = DAQ_Phase[spectrometer];
              data_exchange_packet_router ( myAddress, &packet_message );
              lastPhase = DAQ_Phase[spectrometer];
            }
          }

          switch  (DAQ_Phase[spectrometer])
          {
          case DAQ_PHS_Idle:
            //  Do nothing
            break;

          case DAQ_PHS_CloseShutter:
            //io_out_string ( "C" );
            //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00001000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

            if  ( firstSpec <= lastSpec )
            {
# if UVDARKS
# else
              switch ( spectrometer )
              {
                //  Proper delay is done inside shutter_._close() call
                case 0: shutter_A_close(); break;
                case 1: shutter_B_close(); break;
                default: break;
              }
              shutter_is_closed[spectrometer] = 1;
# endif
            }

            //  Start MCOMS when shutter is closed
            //
            if  (0)
            {
              data_exchange_packet_t packet_command;
              packet_command.to   = DE_Addr_AuxDataAcquisition;
              packet_command.from = myAddress;
              packet_command.type = DE_Type_Command;
              packet_command.data.Command.Code = CMD_ADQ_PollMCOMS;
              packet_command.data.Command.value.s32[0] = 0;  //  Ignored
              data_exchange_packet_router ( myAddress, &packet_command );
              vTaskDelay( (portTickType)TASK_DELAY_MS( 1500 ) );
            }

            DAQ_Phase[spectrometer] = DAQ_PHS_StartDark;

            break;

          case DAQ_PHS_StartDark:
            //io_out_string ( "d" );

            //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00004000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

            if  (firstSpec <= lastSpec)
            {
              //  Make sure the critical sections in the SPI transfers cannot interfere with the spectrometer readouts
              //  data_exchange_spectrometer_pauseTask();    //  FIXME -- May no longer be needed for REV_B Boards
              
              int const num_clearout = CFG_Get_Number_of_Clearouts();
              U32 endReadout;
              
              if  (CGS_OK != CGS_StartSampling (SpecID, 
                                                (double)current_integration_time[spectrometer],
                                                num_clearout, 1, &endReadout ) )
              {
                //  Abort
                //  { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= 0x40000000; pcl_write_gplp ( 0, gplp0 ); }
                DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                prepareToStop = true;
              }
              else
              {
                DAQ_Phase[spectrometer] = DAQ_PHS_GetDark;
              }
            }
            else
            {
              DAQ_Phase[spectrometer] = DAQ_PHS_GetDark;
            }
            break;

          case DAQ_PHS_GetDark:

            //io_out_string ( "D" );
            //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00010000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }
            if  ( firstSpec <= lastSpec )
            {
              if  ( SN74V283_GetNumOfSpectra (componentID) > 0 )
              {
                int8_t twi_channel;
                if ( 0 == spectrometer )
                {
                  twi_channel = TWI_MUX_SPEC_TEMP_A;
                }
                else
                {
                  twi_channel = TWI_MUX_SPEC_TEMP_B;
                }

                float T;
                if ( MAX6633_OK == max6633_measure ( twi_channel, &T ) )
                {
                    spectrometer_temp[spectrometer] = (int16_t)(100 * T);
                }
                else
                {
                    spectrometer_temp[spectrometer] = (int16_t)-9999;
                }

                //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x10000000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }
                //io_out_S32 ( "GD %ld",     (S32)spectrometer );
                //io_out_S32 ( " # %ld ", (S32)SN74V283_GetNumOfCleared(componentID) );
                //io_out_S32 ( " %ld\r\n", (S32)SN74V283_GetNumOfSpectra(componentID) );

                //  Spectrometer readouts complete; critical sections in the SPI transfers are permitted again
                // data_exchange_spectrometer_resumeTask();

                int px;

                uint16_t nClear = SN74V283_GetNumOfCleared (componentID);

                // kurt,  Assuming every refernces to fifo causes a 2 byte data to pop off a queue
                //  This next loop is to clear out rows of pixel data from fifo.
                int cc;
                for  (cc = 0;  cc < nClear;  cc++)
                {
                  for  (px = 0;  px < N_SPEC_PIX + N_SPEC_MARGINS;  px++)
                  {
                    U16 discard = fifo[0];
                  }
                }

                for  (px = N_SPEC_PIX + N_SPEC_MARGINS - 1;  px >= 0;  px--)
                {
                  darkSpectrum[spectrometer][px] = fifo[0];
                }

                // readoutOk[spectrometer] = N_SPEC_PIX;

                dark_fifo_over[ spectrometer ] = 0;
					      // Burkhard typically see ~10 unaccounted for pixels. Unknown reasons VS 5/17/18
					      // We are reading all of the pixels we expect to see but the fifo receives more pin triggers than expected
					      // Need to look into why fifo is triggered to read these extra values, not sure if before or after data acq
                while  (!SN74V283_IsEmpty ( componentID )  &&  dark_fifo_over [ spectrometer ] < 99)
                {
                  U16 discard = fifo[0];
                  dark_fifo_over [spectrometer]++;
                }

                SN74V283_Start(componentID);

                {
                  U32 d_avg = 0;
# if UVDARKS
                  for ( px=spec_fifo_over[spectrometer]+N_SPEC_SKIP; px<64; px++ ) {
                    d_avg += darkSpectrum[spectrometer][px];
                  }
                  d_avg /= 64;
# else
                  for (px = 10 - dark_fifo_over[spectrometer];
                       px < 10 - dark_fifo_over[spectrometer] + N_SPEC_PIX;
                       px++
                      )
                  {
                    d_avg += darkSpectrum[spectrometer][px];
                  }
                  d_avg /= N_SPEC_PIX;
# endif
                  U32 d_sdv = 0;
                  U32 d_min = 0xFFFF;
                  U32 d_max = 0;
# if UVDARKS
                  for ( px=0; px<64; px++ ) {
                    S32 val  = darkSpectrum[spectrometer][px];
                    if ( d_max < val ) d_max = val;
                    if ( d_min > val ) d_min = val;
                    S32 diff = val;
                    diff -= d_avg;
                    d_sdv += diff*diff;
                  }
                  d_sdv /= 64;
# else
                  for  (px = 10 - dark_fifo_over[spectrometer];  px < 10 - dark_fifo_over[spectrometer] + N_SPEC_PIX;  px++)
                  {
                    S32 val  = darkSpectrum[spectrometer][px];
                    if ( d_max < val ) d_max = val;
                    if ( d_min > val ) d_min = val;
                    S32 diff = val;
                    diff -= d_avg;
                    d_sdv += diff*diff;
                  }
                  d_sdv /= N_SPEC_PIX;
# endif
                  d_sdv  = (d_sdv>0 ) ? ceil(sqrt(d_sdv)) : 0;

                  dark_avg[spectrometer] = d_avg;
                  dark_sdv[spectrometer] = d_sdv;

                  dark_min[spectrometer] = d_min;
                  dark_max[spectrometer] = d_max;

# if 0
                  if ( DBG ) {
                    io_out_S32 ( "Spec %ld ", (S32)spectrometer );
                    io_out_S32 ( "D %ld +/- ", d_avg );
                    io_out_S32 ( "%ld [ ", d_sdv );
                    io_out_S32 ( "%ld ", d_min );
                    io_out_S32 ( "%ld ]\r\n", d_max );
                  }
# endif
                }

                //  Stop MCOMS
                //
                if(0)
                {
                  vTaskDelay( (portTickType)TASK_DELAY_MS( 1500 ) );
                  data_exchange_packet_t packet_command;
                  packet_command.to   = DE_Addr_AuxDataAcquisition;
                  packet_command.from = myAddress;
                  packet_command.type = DE_Type_Command;
                  packet_command.data.Command.Code = CMD_ADQ_InterruptMCOMS;
                  packet_command.data.Command.value.s32[0] = 0;  //  Ignored
                  data_exchange_packet_router ( myAddress, &packet_command );
                }


                dark_sample_number[spectrometer]++;

                switch ( DAQ_state )
                {
                case DAQ_Stt_StreamLandD:
                case DAQ_Stt_Calibrate:
                      lights_after_dark[spectrometer] = 0;
                case DAQ_Stt_DarkCharacterize:
                      DAQ_Phase[spectrometer] = DAQ_PHS_TransferD;
                      break;
                case DAQ_Stt_FloatProfile:
                case DAQ_Stt_StreamLMD:
                      lights_after_dark[spectrometer] = 0;
                      DAQ_Phase[spectrometer] = DAQ_PHS_OpenShutter;
                      break;
                case DAQ_Stt_Idle:  /*  Cannot happen  */
                      DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                      break;
                }
              }

            }
            else
            {

              switch ( DAQ_state )
              {
                case DAQ_Stt_StreamLandD:
                case DAQ_Stt_Calibrate:
                      lights_after_dark[spectrometer] = 0;
                case DAQ_Stt_DarkCharacterize:
                      DAQ_Phase[spectrometer] = DAQ_PHS_TransferD;
                      break;
                case DAQ_Stt_FloatProfile:
                case DAQ_Stt_StreamLMD:
                      lights_after_dark[spectrometer] = 0;
                      DAQ_Phase[spectrometer] = DAQ_PHS_OpenShutter;
                      break;
                case DAQ_Stt_Idle:        /* Cannot happen */
                      DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                      break;
              }
            }

            break;

          case DAQ_PHS_OpenShutter:

//  io_out_string ( "O" );
//          { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00040000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }
            if  (firstSpec <= lastSpec)
            {
# if UVDARKS
# else
              switch ( spectrometer )
              {
                //  Proper delay is done inside shutter_._close() call
                case 0: shutter_A_open(); break;
                case 1: shutter_B_open(); break;
                default: break;
              }
              shutter_is_closed[spectrometer] = 0;
# endif
            }

            DAQ_Phase[spectrometer] = DAQ_PHS_StartLight;

            break;



          case DAQ_PHS_StartLight:
//io_out_string ( "l" );
//             { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00100000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

            if  (firstSpec<=lastSpec )
            {

              //  Make sure the critical sections in the SPI transfers cannot interfere with the spectrometer readouts
              // data_exchange_spectrometer_pauseTask();

              int const num_clearout = CFG_Get_Number_of_Clearouts();
              U32 endReadout;
              if  (CGS_OK != CGS_StartSampling (SpecID, (double)current_integration_time[spectrometer],  num_clearout,  1,  &endReadout)) 
              {
                //  Abort
                //  io_out_string ( "CGS_StartSampling(L) failed\r\n" );
                DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                prepareToStop = true;
              }
              else
              {
                DAQ_Phase[spectrometer] = DAQ_PHS_GetLight;
              }

            }
            else
            {
              DAQ_Phase[spectrometer] = DAQ_PHS_GetLight;
            }

            //  Tilt and heading at start of light
            //
            housing_pitch[spectrometer][0]  = 0;
            housing_pitch[spectrometer][1]  = 0;
            housing_roll [spectrometer][0]  = 0;
            housing_roll [spectrometer][1]  = 0;
            housing_tilt [spectrometer][0]  = 0;
            housing_tilt [spectrometer][1]  = 0;
            tilt_measurements[spectrometer] = 0;
            
            lsm303_Accel_Raw_t acc_raw;
            if  (LSM303_OK == lsm303_get_accel_raw ( 1, &acc_raw ) )
            {
              double pitch, roll, tilt;
              if  (LSM303_OK == lsm303_calc_tilt (&acc_raw,  &lsm303_vertical,  lsm303_mounting_angle,  &pitch, &roll, &tilt) )
              {
                housing_pitch[spectrometer][0] += pitch;
                housing_pitch[spectrometer][1] += pitch*pitch;
                housing_roll [spectrometer][0] += roll;
                housing_roll [spectrometer][1] += roll*roll;
                housing_tilt [spectrometer][0] += tilt;
                housing_tilt [spectrometer][1] += tilt*tilt;
                tilt_measurements[spectrometer]++;
              }
            }

            housing_heading [spectrometer][0]  = 0;
            housing_heading [spectrometer][1]  = 0;
            heading_measurements[spectrometer] = 0;
            
            lsm303_Mag_Raw_t mag_raw;
            if ( LSM303_OK == lsm303_get_mag_raw ( 1, &mag_raw ) )
            {
              double heading;
              if  ( LSM303_OK == lsm303_calc_heading (&mag_raw,  &lsm303_mag_min,  &lsm303_mag_max,  lsm303_mounting_angle,  &heading))
              {
                housing_heading [spectrometer][0]  += heading;
                housing_heading [spectrometer][1]  += heading*heading;
                heading_measurements[spectrometer] ++;
              }
            }

            //  Spectrometer Tilts
            //
            spectrometer_measurement[spectrometer] = 0;

            break;

          case DAQ_PHS_GetLight:

            //io_out_string ( "L" );
            //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x00400000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }
            {
              lsm303_Accel_Raw_t acc_raw;
              if ( LSM303_OK == lsm303_get_accel_raw ( 1, &acc_raw ) )
              {
                double pitch, roll, tilt;
                if  ( LSM303_OK == lsm303_calc_tilt ( &acc_raw, &lsm303_vertical, lsm303_mounting_angle, &pitch, &roll, &tilt) )
                {
                  housing_pitch[spectrometer][0] += pitch;
                  housing_pitch[spectrometer][1] += pitch*pitch;
                  housing_roll [spectrometer][0] += roll;
                  housing_roll [spectrometer][1] += roll*roll;
                  housing_tilt [spectrometer][0] += tilt;
                  housing_tilt [spectrometer][1] += tilt*tilt;
                  tilt_measurements[spectrometer]++;
                }
              }
              
              lsm303_Mag_Raw_t mag_raw;
              if  ( LSM303_OK == lsm303_get_mag_raw ( 1, &mag_raw ) )
              {
                double heading;
                if  ( LSM303_OK == lsm303_calc_heading ( &mag_raw, &lsm303_mag_min, &lsm303_mag_max, lsm303_mounting_angle, &heading) )
                {
                  housing_heading [spectrometer][0]  += heading;
                  housing_heading [spectrometer][1]  += heading*heading;
                  heading_measurements[spectrometer] ++;
                }
              }
              
              if  (firstSpec <= lastSpec)
              {

                //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x10000000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

                if  ( SN74V283_GetNumOfSpectra(componentID) > 0)
                {
                  io_out_string ( "Y\r\n" );
                  //io_out_S32 ( "GL %ld",   (S32)spectrometer );
                  //io_out_S32 ( " # %ld ",  (S32)SN74V283_GetNumOfCleared(componentID) );
                  //io_out_S32 ( " %ld\r\n", (S32)SN74V283_GetNumOfSpectra(componentID) );
                  
                  //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x40000000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

                  //  Spectrometer readouts complete; critical sections in the SPI transfers are permitted again
                  // data_exchange_spectrometer_resumeTask();

                  if  ( simulate_pressure_profile )
                  {
                    struct timeval now;
                    gettimeofday( &now, NULL) ;
                    last_pressure_time.tv_sec  = now.tv_sec;
                    last_pressure_time.tv_usec = now.tv_usec;
                    last_pressure_value = PRES_fake(0.0F,0.0F);
                    //t_counts[spectrometer] = 2;
                    //t_durat [spectrometer] = 2;
                    //p_counts[spectrometer] = 2;
                    //p_durat [spectrometer] = 2;
                  }
                  else
                  {
                    if  ( run_pressure_sensor )
                    {
                      if  ( pressure_started )
                      {
                        //  If enough time has passed since pressure sensor was started: GetPressureValue
                        //  Else reuse last pressure
                        //
                        struct timeval now;
                        gettimeofday( &now, NULL) ;
                        double time_diff  = 1000000 * ( now.tv_sec - pressure_start_time.tv_sec );
                               time_diff += now.tv_usec;
                               time_diff -= pressure_start_time.tv_usec;

                        if ( time_diff > 250000.0 )
                        {
                          // > 250 ms
                          //  Take measurement & update last
                          //
                          F64 Temperature_period;  int Temperature_counts = 0;  U32 Temperature_duration = 0;
                          F64    Pressure_period;  int    Pressure_counts = 0;  U32    Pressure_duration = 0;
                        
                          if  ( PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_TEMPERATURE, &dq_sensor, &Temperature_period, &Temperature_counts, &Temperature_duration, CFG_Get_Pres_TDiv() )
                             && PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE,    &dq_sensor, &Pressure_period,    &Pressure_counts,    &Pressure_duration,    CFG_Get_Pres_PDiv() ) )
                          {
                        
                            //  Make sure there was no possibility of counts overflow
                            //
                            //          if ( Temperature_duration < 0.25*CFG_Get_Pres_TDiv()*3686400
                            //            &&    Pressure_duration < 0.25*CFG_Get_Pres_PDiv()*3686400 ) 
                            if ( time_diff < 1000000.0 )
                            {
                              gettimeofday( &now, NULL) ;
                              last_pressure_time.tv_sec  = now.tv_sec;
                              last_pressure_time.tv_usec = now.tv_usec;
                              last_pressure_value = PRES_Calc_Pressure_dBar( Temperature_period, Pressure_period, &dq_cc, (F64*)0, (F64*)0 );
                              //t_counts[spectrometer] = Temperature_counts;
                              //t_durat [spectrometer] = Temperature_duration;
                              //p_counts[spectrometer] = Pressure_counts;
                              //p_durat [spectrometer] = Pressure_duration;
                            }
                        
                            //  Restart pressure measurement
                            PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
                            PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
                            gettimeofday ( &pressure_start_time, NULL );
                          }
                          else
                          {
                            last_pressure_value = -5;
                            //t_counts[spectrometer] = 1;
                            //t_durat [spectrometer] = 1;
                            //p_counts[spectrometer] = 1;
                            //p_durat [spectrometer] = 1;
                            PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
                            PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
                            gettimeofday ( &pressure_start_time, NULL );
                          }
                        }
                      }
                      else
                      {
                        //  Pressure sensor was not started
                        last_pressure_value = -3;
                        //t_counts[spectrometer] = 1;
                        //t_durat [spectrometer] = 1;
                        //p_counts[spectrometer] = 1;
                        //p_durat [spectrometer] = 1;
                        PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
                        PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
                        gettimeofday ( &pressure_start_time, NULL );
                        pressure_started = true;
                      }
                    }
                    else
                    {
                      //  Pressure sensor intentionally not operating
                      last_pressure_value = -2;
                      //t_counts[spectrometer] = 1;
                      //t_durat [spectrometer] = 1;
                      //p_counts[spectrometer] = 1;
                      //p_durat [spectrometer] = 1;
                    }
                  }

                  pressure_value[ 0 /* spectrometer */ ] = last_pressure_value;
                  pressure_time [ 0 /* spectrometer */ ].tv_sec  = last_pressure_time.tv_sec;
                  pressure_time [ 0 /* spectrometer */ ].tv_usec = last_pressure_time.tv_usec;

                  int px;

                  uint16_t nClear = SN74V283_GetNumOfCleared(componentID);

                  int cc;
                  for  (cc = 0;  cc < nClear;  cc++)
                  {
                    for ( px=0; px<N_SPEC_PIX+N_SPEC_MARGINS; px++ )
                    {
                      U16 discard = fifo[0];
                    }
                  }

                  for ( px=N_SPEC_PIX+N_SPEC_MARGINS-1; px>=0; px-- )
                  {
                    lghtSpectrum[spectrometer][px] = fifo[0];
                  }

                  // readoutOk[spectrometer] = 2048;

                  lght_fifo_over[ spectrometer ] = 0;

                  while ( !SN74V283_IsEmpty ( componentID )
                       && lght_fifo_over [ spectrometer ] < 99 )
                  {
                      U16  discard = fifo[0];
                      lght_fifo_over [ spectrometer ] ++;
                  }

                  SN74V283_Start(componentID);

                  {
                    U32 l_avg = 0;
                    for ( px=10-lght_fifo_over[spectrometer];
                          px<10-lght_fifo_over[spectrometer]+N_SPEC_PIX; px++ )
                    {
                      l_avg += lghtSpectrum[spectrometer][px];
                    }
                    l_avg /= N_SPEC_PIX;

                    U32 l_sdv = 0;
                    U32 l_min = 0xFFFF;
                    U32 l_max = 0;
                    
                    for ( px = 10 - lght_fifo_over[spectrometer];  
                          px < 10 - lght_fifo_over[spectrometer] + N_SPEC_PIX;  px++)
                    {
                      S32 val  = lghtSpectrum[spectrometer][px];
                      if ( l_max < val ) l_max = val;
                      if ( l_min > val ) l_min = val;
                      S32 diff = val;
                      diff -= l_avg;
                      l_sdv += diff*diff;
                    }
                    l_sdv /= N_SPEC_PIX;
                    l_sdv  = (l_sdv>0 ) ? ceil(sqrt(l_sdv)) : 0;

                    lght_min[spectrometer] = l_min;
                    lght_max[spectrometer] = l_max;

# if 0
                    if ( DBG ) {
                      io_out_S32 ( "Spec %ld ", (S32)spectrometer );
                      io_out_S32 ( "L %ld +/- ", l_avg );
                      io_out_S32 ( "%ld [ ", l_sdv );
                      io_out_S32 ( "%ld ", l_min );
                      io_out_S32 ( "%ld ]\r\n", l_max );
                    }
# endif
                  }

                  //  TODO Spectrometer Tilt
                  //
                  //  Currently a FAKE!
                  spectrometer_pitch       [spectrometer][0] = 0;
                  spectrometer_roll        [spectrometer][0] = 0;
                  spectrometer_measurement [spectrometer] = 1;

                  int8_t twi_channel;
                  if  ( 0 == spectrometer )
                  {
                    twi_channel = TWI_MUX_SPEC_TEMP_A;
                  }
                  else
                  {
                    twi_channel = TWI_MUX_SPEC_TEMP_B;
                  }

                  float T;
                  if  ( MAX6633_OK == max6633_measure ( twi_channel, &T ) )
                  {
                    spectrometer_temp[spectrometer] = (int16_t)(100 * T);
                  }
                  else
                  {
                    spectrometer_temp[spectrometer] = (int16_t)-9999;
                  }

                  if  ( !Fixed_Integration_Time && firstSpec<=lastSpec )
                  {
                    //  Adapt integration time to current light condition
                    //
                    uint16_t const SATURATION = CFG_Get_Saturation_Counts();
                    if ( lght_max[spectrometer] > SATURATION )
                    {
                      if  ( current_integration_idx[spectrometer] > 0 )
                        next_integration_idx[spectrometer] = current_integration_idx[spectrometer] - 1;
                    }
                    else
                    {
                      if  ((lght_max[spectrometer]-dark_max[spectrometer]) < SATURATION/adjust_factor )
                      {
                        if  ( current_integration_idx[spectrometer] < number_of_integration_times-1 )
                          next_integration_idx[spectrometer] = current_integration_idx[spectrometer]+1;
                      }
                    }
                  }

                  lights_after_dark   [spectrometer]++;
                  light_sample_number [spectrometer]++;

                  switch ( DAQ_state )
                  {
                  case DAQ_Stt_StreamLandD:
                  case DAQ_Stt_Calibrate:
                        DAQ_Phase[spectrometer] = DAQ_PHS_TransferL;
                        break;
                  
                  case DAQ_Stt_StreamLMD:
                  case DAQ_Stt_FloatProfile:
                        DAQ_Phase[spectrometer] = DAQ_PHS_TransferLMD;
                        break;
                  
                  case DAQ_Stt_DarkCharacterize:  /* Cannot happen */
                  case DAQ_Stt_Idle:              /* Cannot happen */
                        DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                        break;
                  }
                }
              }
              else
              {

                switch ( DAQ_state ) {
                case DAQ_Stt_StreamLandD:
                case DAQ_Stt_Calibrate:
                      DAQ_Phase[spectrometer] = DAQ_PHS_TransferL;
                      break;
                case DAQ_Stt_FloatProfile:
                case DAQ_Stt_StreamLMD:
                      DAQ_Phase[spectrometer] = DAQ_PHS_TransferLMD;
                      break;
                case DAQ_Stt_DarkCharacterize:  /* Cannot happen */
                case DAQ_Stt_Idle:              /* Cannot happen */
                      DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                      break;
                }
              }

            }
            break;

          case DAQ_PHS_TransferD:
          case DAQ_PHS_TransferL:
          case DAQ_PHS_TransferLMD:

            //  io_out_string ( "t" );
            //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= (0x01000000<<spectrometer); pcl_write_gplp ( 0, gplp0 ); }

            {
              double h_pitch_avg, h_roll_avg, h_tilt_avg, h_hdng_avg;
              h_pitch_avg = h_roll_avg = h_tilt_avg = h_hdng_avg = 0;
              double h_pitch_sdv, h_roll_sdv, h_tilt_sdv, h_hdng_sdv;
              h_pitch_sdv = h_roll_sdv = h_tilt_sdv = h_hdng_sdv = 0;
              
              if  ( tilt_measurements[spectrometer] > 0)
              {
                h_pitch_avg = housing_pitch[spectrometer][0] / tilt_measurements[spectrometer];
                h_pitch_sdv = housing_pitch[spectrometer][1] / tilt_measurements[spectrometer];
                h_pitch_sdv -= h_pitch_avg*h_pitch_avg;
                h_pitch_sdv  = (h_pitch_sdv>0) ? sqrt(h_pitch_sdv) : 0;
                h_roll_avg = housing_roll [spectrometer][0] / tilt_measurements[spectrometer];
                h_roll_sdv = housing_roll [spectrometer][1] / tilt_measurements[spectrometer];
                h_roll_sdv -=  h_roll_avg* h_roll_avg;
                h_roll_sdv  = ( h_roll_sdv>0) ? sqrt( h_roll_sdv) : 0;
                h_tilt_avg = housing_roll [spectrometer][0] / tilt_measurements[spectrometer];
                h_tilt_sdv = housing_roll [spectrometer][1] / tilt_measurements[spectrometer];
                h_tilt_sdv -=  h_tilt_avg* h_tilt_avg;
                h_tilt_sdv  = ( h_tilt_sdv>0) ? sqrt( h_tilt_sdv) : 0;
              
                housing_pitch[spectrometer][0]  = 0;
                housing_pitch[spectrometer][1]  = 0;
                housing_roll [spectrometer][0]  = 0;
                housing_roll [spectrometer][1]  = 0;
                housing_tilt [spectrometer][0]  = 0;
                housing_tilt [spectrometer][1]  = 0;
                tilt_measurements[spectrometer] = 0;
              }
              
              if  ( heading_measurements[spectrometer] )
              {
                h_hdng_avg = housing_heading [spectrometer][0] / heading_measurements[spectrometer];
                h_hdng_sdv = housing_heading [spectrometer][1] / heading_measurements[spectrometer];
                h_hdng_sdv -=  h_hdng_avg* h_hdng_avg;
                h_hdng_sdv  = ( h_hdng_sdv>0) ? sqrt( h_hdng_sdv) : 0;
              
                housing_heading[spectrometer][0]   = 0;
                housing_heading[spectrometer][1]   = 0;
                heading_measurements[spectrometer] = 0;
              }

              {
                int adp; int use_adp = -1;
              
                for ( adp=0; adp<NumADP && use_adp==-1; adp++ )
                {
                  if  ( pdTRUE == xSemaphoreTake ( acquired_data_package[adp].mutex, 250 /*portMAX_DELAY*/ /*15*/ ) )
                  {
                    if ( acquired_data_package[adp].state == EmptyRAM)
                    {
                      use_adp = adp;
                    }
                    else
                    {
                      xSemaphoreGive ( acquired_data_package[adp].mutex );
                    }
                  }
                }
              
                if  ( use_adp >= 0 )
                {
                  //char ooo[2]; ooo[0] = '0'+use_adp; ooo[1] = 0;
                  //io_out_string(  ooo );
                  Spectrometer_Data_t* local_data_pointer = (Spectrometer_Data_t*)acquired_data_package[use_adp].address;
              
                  local_data_pointer -> aux.tag = 0; // Will be OR-ed with flags further down
              
                  //   if ( readoutOk[spectrometer] == 0 ) {
                  //     local_data_pointer -> aux.tag |= SAD_TAG_READOUT_NO;
                  //   } else if ( readoutOk[spectrometer] < N_SPEC_PIX ) {
                  //     local_data_pointer -> aux.tag |= SAD_TAG_READOUT_PART;
                  //   }
              
                  gettimeofday( &(local_data_pointer->aux.acquisition_time), NULL);
                  struct tm* stt = gmtime ( &(local_data_pointer->aux.acquisition_time.tv_sec) );
                  HNV_time_t hnv_time = {
                    .year  = stt->tm_year,
                    .month = stt->tm_mon,
                    .day   = stt->tm_mday,
                    .hour  = stt->tm_hour,
                    .min   = stt->tm_min,
                    .sec   = stt->tm_sec,
                    .timeZone = 0
                  };
              
                  double minutesSinceMidnight;
                  double julianCentury;
                  calcTime( &hnv_time, &minutesSinceMidnight, &julianCentury );
              
                  double solar_azimuth = calcSolarAzimuthAngle( julianCentury, gps_pos.lon, gps_pos.lat, minutesSinceMidnight, 0 /*timezone*/ );
                  //  solar_azimuth should be in 0..360
                  //  but make sure
                  if ( solar_azimuth < 0 )
                    solar_azimuth += 360;
                  local_data_pointer -> aux.sun_azimuth = (uint16_t)round(100.0*solar_azimuth);
              
                  local_data_pointer -> aux.side = spectrometer;
                  
                  //  Converting AST times to milliseconds:
                  //                      double actualIntegration = ( endIntegration[spectrometer].u32 - startIntegration[spectrometer] ) / ( 115.200 );
                  //  TODO compare intended vs. true integration time
                  local_data_pointer -> aux.integration_time = current_integration_time[spectrometer];

                  local_data_pointer -> aux.dark_average = dark_avg[spectrometer];
                  local_data_pointer -> aux.dark_noise   = dark_sdv[spectrometer];

                  if ( DAQ_PHS_TransferD == DAQ_Phase[spectrometer] )
                  {
                    local_data_pointer -> aux.  sample_number  =  dark_sample_number[spectrometer];
                  }
                  else
                  {
                    local_data_pointer -> aux.  sample_number  = light_sample_number[spectrometer];
                  }

                  local_data_pointer -> aux.spectrometer_temperature = spectrometer_temp[spectrometer];

                  local_data_pointer -> aux.pressure = (int32_t)round(10000.0 * pressure_value[ 0 /* spectrometer */ ] );
                  //  The following data are transferred for debugging purposes
                  //
                  local_data_pointer -> aux.pressure_T_counts   = 1; // t_counts[spectrometer];
                  local_data_pointer -> aux.pressure_T_duration = 1; // t_durat [spectrometer];
                  local_data_pointer -> aux.pressure_P_counts   = 1; // p_counts[spectrometer];
                  local_data_pointer -> aux.pressure_P_counts   = dark_fifo_over[spectrometer];
                  local_data_pointer -> aux.pressure_P_duration = 1; // p_durat [spectrometer];
                  local_data_pointer -> aux.pressure_P_duration = lght_fifo_over[spectrometer];
                  //
                  //  End of debugging data

# if HAVE_PRESSURE_FLAGS
                  if ( DAQ_PHS_TransferD != DAQ_Phase[spectrometer] ) {
                    if ( run_pressure_sensor ) {
                      switch ( PT_state ) {
                      case PT_NOP:          local_data_pointer -> aux.tag |= SAD_TAG_PT_TEMP_NO; break;
                      case PT_NotStarted:   local_data_pointer -> aux.tag |= SAD_TAG_PT_TEMP_NS; break;
                      case PT_NotAvailable: local_data_pointer -> aux.tag |= SAD_TAG_PT_TEMP_NA; break;
                      }
                      switch ( PT_state ) {
                      case PT_NOP:          local_data_pointer -> aux.tag |= SAD_TAG_PT_PRES_NO; break;
                      case PT_NotStarted:   local_data_pointer -> aux.tag |= SAD_TAG_PT_PRES_NS; break;
                      case PT_NotAvailable: local_data_pointer -> aux.tag |= SAD_TAG_PT_PRES_NA; break;
                      }
                    } else {
                      local_data_pointer -> aux.tag |= SAD_TAG_PT_TEMP_OFF;
                      local_data_pointer -> aux.tag |= SAD_TAG_PT_PRES_OFF;
                    }
                  }
# endif

                  local_data_pointer -> aux.housing_heading = (uint16_t)round(100.0*h_hdng_avg);

                  //  Flag Sun/Shade condition in tag variable
                  //  TODO
                  //  Based on sun_azimuth & heading & port/starboard position of radiometer head
                  //
                  local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_S_MASK & 0x0000 );

                  local_data_pointer -> aux.housing_pitch   = (int16_t)round(100.0 * h_pitch_avg);
                  local_data_pointer -> aux.housing_roll    = (int16_t)round(100.0 * h_roll_avg);

                  //  Flag float tilt in tag variable
                  //
                  double h_tilt = h_tilt_avg;
                  if ( h_tilt < 1.0 )
                  {
                    local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_F_MASK & SAD_TAG_QUAL_F_TILT_NO );
                  }
                  
                  else if ( h_tilt < 3.0 )
                  {
                    local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_F_MASK & SAD_TAG_QUAL_F_TILT_LOW );
                  }
                  
                  else if ( h_tilt < 5.0 )
                  {
                    local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_F_MASK & SAD_TAG_QUAL_F_TILT_MED );
                  }
                  
                  else
                  {
                    local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_F_MASK & SAD_TAG_QUAL_F_TILT_HIGH );
                  }

                  if ( spectrometer_measurement[spectrometer] > 0 )
                  {
                    double s_pitch_avg, s_roll_avg = 0;
                    double s_pitch_sdv, s_roll_sdv = 0;

                    s_pitch_avg = spectrometer_pitch[spectrometer][0] / spectrometer_measurement[spectrometer];
                    s_pitch_sdv = spectrometer_pitch[spectrometer][1] / spectrometer_measurement[spectrometer];
                    s_pitch_sdv -= s_pitch_avg*s_pitch_avg;
                    s_pitch_sdv  = (s_pitch_sdv>0) ? sqrt(s_pitch_sdv) : 0;
                    s_roll_avg = spectrometer_roll[spectrometer][0] / spectrometer_measurement[spectrometer];
                    s_roll_sdv = spectrometer_roll[spectrometer][1] / spectrometer_measurement[spectrometer];
                    s_roll_sdv -=  s_roll_avg* s_roll_avg;
                    s_roll_sdv  = ( s_roll_sdv>0) ? sqrt( s_roll_sdv) : 0;

                    spectrometer_measurement[spectrometer] = 0;

                    local_data_pointer -> aux.spectrometer_pitch = (int16_t)round(100.0*s_pitch_avg);
                    local_data_pointer -> aux.spectrometer_roll  = (int16_t)round(100.0*s_roll_sdv);

                    double s_tilt = sqrt ( s_pitch_avg*s_pitch_avg + s_roll_avg*s_roll_avg );

                    //  Flag radiometer tilt in tag variable
                    if  (s_tilt < 1.0 )
                    {
                      local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_R_MASK & SAD_TAG_QUAL_R_TILT_NO );
                    }

                    else if  ( s_tilt < 3.0 )
                    {
                      local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_R_MASK & SAD_TAG_QUAL_R_TILT_LOW );
                    }

                    else if  ( s_tilt < 5.0 )
                    {
                      local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_R_MASK & SAD_TAG_QUAL_R_TILT_MED );
                    }

                    else
                    {
                      local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_R_MASK & SAD_TAG_QUAL_R_TILT_HIGH );
                    }
                  }
                  else
                  {
                    local_data_pointer -> aux.spectrometer_pitch = 0; //N/A;
                    local_data_pointer -> aux.spectrometer_roll  = 0; //N/A;
                    local_data_pointer -> aux.tag |= ( SAD_TAG_QUAL_R_MASK & 0x0000 );
                  }

                  {
                    int px, dpx, lpx;

                    if ( DAQ_PHS_TransferLMD == DAQ_Phase[spectrometer] )
                    {

                      local_data_pointer -> aux.tag |= ( SAD_TAG_DATA_MASK & SAD_TAG_LIGHT_MINUS_DARK );

                      //  When transferring Light-minus-Dark,
                      //  must add an up-shift value to ensure
                      //  the difference does not become negative (using unsigned integers!)
                      uint16_t use_shift = 0;
                      for ( dpx = 10 - dark_fifo_over[spectrometer], lpx=10-lght_fifo_over[spectrometer];
                            dpx < 10 - dark_fifo_over[spectrometer] + N_SPEC_PIX;
                            dpx++, lpx++)
                      {
                        if  ( lghtSpectrum[spectrometer][lpx] < darkSpectrum[spectrometer][dpx] )
                        {
                          uint16_t shift = darkSpectrum[spectrometer][dpx] - lghtSpectrum[spectrometer][lpx];
                          if ( shift>use_shift )
                            use_shift = shift;
                        }
                      }
                      local_data_pointer -> aux.light_minus_dark_up_shift = use_shift;

                      for  (px=0, dpx=10-dark_fifo_over[spectrometer], lpx=10-lght_fifo_over[spectrometer];
                            px<N_SPEC_PIX;
                            px++, dpx++, lpx++ )
                      {
                        uint32_t difference = use_shift + lghtSpectrum[spectrometer][lpx]; difference -= darkSpectrum[spectrometer][dpx];
                        local_data_pointer -> hnv_spectrum[px] = difference;
                      }

                      local_data_pointer -> aux.spec_min = lght_min[spectrometer];
                      local_data_pointer -> aux.spec_max = lght_max[spectrometer];
                    }


                    else if  ( DAQ_PHS_TransferL == DAQ_Phase[spectrometer] )
                    {
                      local_data_pointer -> aux.tag |= ( SAD_TAG_DATA_MASK & SAD_TAG_LIGHT );

                      for ( px=0, lpx=10-lght_fifo_over[spectrometer];
                            px<N_SPEC_PIX;
                            px++, lpx++ )
                      {
                        local_data_pointer -> hnv_spectrum[px] = lghtSpectrum[spectrometer][lpx];
                      }
                      local_data_pointer -> aux.light_minus_dark_up_shift = 0;

                      local_data_pointer -> aux.spec_min = lght_min[spectrometer];
                      local_data_pointer -> aux.spec_max = lght_max[spectrometer];
                    }

                    else /*   DAQ_PHS_TransferD == DAQ_Phase[spectrometer] */
                    {
                      if  ( DAQ_Stt_DarkCharacterize == DAQ_state )
                      {
                        local_data_pointer -> aux.tag |= ( SAD_TAG_DATA_MASK & SAD_TAG_CHAR_DARK );
                      }

                      else
                      {
                        local_data_pointer -> aux.tag |= ( SAD_TAG_DATA_MASK & SAD_TAG_DARK );
                      }

                      for  (px = 0, dpx = 10 - dark_fifo_over[spectrometer];   px < N_SPEC_PIX;   px++, dpx++)
                      {
                        local_data_pointer -> hnv_spectrum[px] = darkSpectrum[spectrometer][dpx];
                      }

                      local_data_pointer -> aux.light_minus_dark_up_shift = 0;

                      local_data_pointer -> aux.spec_min = dark_min[spectrometer];
                      local_data_pointer -> aux.spec_max = dark_max[spectrometer];
                    }
                  }

                  if  (DAQ_state == DAQ_Stt_FloatProfile)
                  {
                    io_out_S32 ("A%ld", FlashMemory_nOfFrames ());
                    FlashMemory_AddFrame (local_data_pointer);
			            }

                  else
                  {
                    data_exchange_packet_t SDATA_packet;

                    SDATA_packet.from = myAddress;

                    //  Provide destination of data:
                    //    Float Profile -> Profile Manager
                    //    Else          -> Stream & Log
                    if  ( DAQ_Stt_FloatProfile == DAQ_state )
                    {
                      SDATA_packet.to = DE_Addr_ProfileManager;
                    }
                    else
                    {
                      SDATA_packet.to = DE_Addr_StreamLog;
                    }
                    
                    SDATA_packet.type = DE_Type_Spectrometer_Data;
                    acquired_data_package[use_adp].state = FullRAM;

                    SDATA_packet.data.DataPackagePointer = &acquired_data_package[use_adp];

                    //  Send packet to destination, will go onto a queue, managed by another thread
                    //
                    data_exchange_packet_router ( myAddress, &SDATA_packet );
			            }

                  SemaphoreGive ( acquired_data_package[use_adp].mutex );
                }
              }

              if ( prepareToStop )
              {
                DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
              }

              else
              {
                //  Prepare for next spectrometer acquisition
                //
                switch  ( DAQ_state )
                {
                case DAQ_Stt_DarkCharacterize:
                  if  ( !Fixed_Integration_Time )
                  {
                    if ( 0 == ( dark_sample_number[spectrometer] % Darks_Per_Integration_Time ) )
                    {
                      current_integration_idx[spectrometer]++;
                      if ( current_integration_idx[spectrometer] == number_of_integration_times )
                      {
                        current_integration_idx[spectrometer] = 0;
                      }
                      current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                    }
                  }
                  DAQ_Phase[spectrometer] = DAQ_PHS_StartDark;
                  break;

                case DAQ_Stt_StreamLandD:
                case DAQ_Stt_StreamLMD:
                case DAQ_Stt_Calibrate:
                  if ( lights_after_dark[spectrometer] == Lights_Per_Dark
                    || ( !Fixed_Integration_Time && next_integration_idx[spectrometer] != current_integration_idx[spectrometer] ) )
                  {
                    if  (!Fixed_Integration_Time)
                    {
                      current_integration_idx [spectrometer] = next_integration_idx [spectrometer];
                      current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                    }

                    DAQ_Phase[spectrometer] = DAQ_PHS_CloseShutter;
                  }
                  else
                  {
                    if  ( shutter_is_closed[spectrometer] )
                    {
                      DAQ_Phase[spectrometer] = DAQ_PHS_OpenShutter;
                    }
                    else
                    {
                      DAQ_Phase[spectrometer] = DAQ_PHS_StartLight;
                    }
                  }
                  break;

                case DAQ_Stt_FloatProfile:
                  //{ uint32_t gplp0 = pcl_read_gplp(0); gplp0 |= 0x04000000; pcl_write_gplp ( 0, gplp0 ); }
                  if  ( PRF_Single == PRF_action )
                  {
                    current_integration_idx [spectrometer] = next_integration_idx [spectrometer];
                    current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                    DAQ_Phase[spectrometer] = DAQ_PHS_Idle;

                    //  If both are now idle, switch action to Delay
                    if ( DAQ_Phase[ 1-spectrometer ] == DAQ_PHS_Idle )
                    {
                      //  Turn off spectrometer after both spectrometers are done
                      //
                      CGS_Power_Off();

                      PRF_action = PRF_Delay_0;
                    }
                  }

                  else if ( PRF_Continuous == PRF_action )
                  {
                    if  ( lights_after_dark[spectrometer] == Lights_Per_Dark
                       || next_integration_idx[spectrometer] != current_integration_idx[spectrometer] ) 
                    {
                      current_integration_idx [spectrometer] = next_integration_idx [spectrometer];
                      current_integration_time[spectrometer] = available_integration_times[current_integration_idx[spectrometer]];
                      DAQ_Phase[spectrometer] = DAQ_PHS_CloseShutter;

                    }
                    else
                    {
                      if  ( shutter_is_closed[spectrometer] )
                      {
                        DAQ_Phase[spectrometer] = DAQ_PHS_OpenShutter;
                      }

                      else
                      {
                        DAQ_Phase[spectrometer] = DAQ_PHS_StartLight;
                      }
                    }

                    //  If pressure stable, assume float is at surface.
                    //  Collect an additional limited number of frames, then stop.
                    //
                    if  ( pressure_value[ 0 /* spectrometer */ ] <= PRES_AT_SURFACE + 0.5 )
                    {
                      static int numAtSurface = 0;

                      if ( numAtSurface < 40 )
                      {
                        numAtSurface ++;
                      }

                      else
                      {
                        DAQ_Phase[spectrometer] = DAQ_PHS_Idle;

                        if  ( DAQ_Phase[ 1-spectrometer ] == DAQ_PHS_Idle )
                        {
                          PRF_action    = PRF_Complete;
                          prepareToStop = true;
                          numAtSurface  = 0;
                          if  ( OCR_started )
                          {
                            data_exchange_packet_t packet_cmd;
                            packet_cmd.to   = DE_Addr_AuxDataAcquisition;
                            packet_cmd.from = myAddress;
                            packet_cmd.type = DE_Type_Command;
                            packet_cmd.data.Command.Code = CMD_ADQ_Stop;
                            packet_cmd.data.Command.value.s64 = 0;   //  For REAL mode
                            data_exchange_packet_router ( myAddress, &packet_cmd );
                            OCR_started = 0;
                          }
                           //io_out_string ( "APF ZZ\r\n" );
                        }
                      }
                    }
                  }
                  else
                  {
                    //  Impossible
                  }
                  break;

                case DAQ_Stt_Idle:  /*  Cannot happen  */
                  DAQ_Phase[spectrometer] = DAQ_PHS_Idle;
                  break;
                }
              }
            }
            io_out_string ( "T\r\n" );
            break;
          }

          if  ( run_pressure_sensor )
          {
            if  ( pressure_started )
            {
              //  If the maximum measurement time approaches,
              //  get pressure value and save for potential use.
              //
              struct timeval now;
              gettimeofday( &now, NULL) ;
              double time_diff  = 1000000 * ( now.tv_sec - pressure_start_time.tv_sec );
                     time_diff +=                 now.tv_usec;
                     time_diff -= pressure_start_time.tv_usec;
              
              if  ( time_diff > 250000.0 )
              {
                // > 250 ms
                F64 Temperature_period;  int Temperature_counts = 0;  U32 Temperature_duration = 0;
                F64    Pressure_period;  int    Pressure_counts = 0;  U32    Pressure_duration = 0;
              
                if  ( PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_TEMPERATURE, &dq_sensor, &Temperature_period, &Temperature_counts, &Temperature_duration, CFG_Get_Pres_TDiv() )
                   && PRES_Ok == PRES_GetPerioduSecs(DIGIQUARTZ_PRESSURE,    &dq_sensor, &Pressure_period,    &Pressure_counts,    &Pressure_duration,    CFG_Get_Pres_PDiv() ) )
                {
                  if  (time_diff < 1000000.0)
                  {
                    gettimeofday (&now, NULL) ;
                    last_pressure_time.tv_sec  = now.tv_sec;
                    last_pressure_time.tv_usec = now.tv_usec;
                    last_pressure_value = PRES_Calc_Pressure_dBar( Temperature_period, Pressure_period, &dq_cc, (F64*)0, (F64*)0 );
                  }
                }
              
                pressure_started = false;
              }
            }

            //  If pressure is not measuring,
            //  start now.
            //
            if ( !pressure_started )
            {
              PRES_StartMeasurement(DIGIQUARTZ_TEMPERATURE);
              PRES_StartMeasurement(DIGIQUARTZ_PRESSURE);
              pressure_started = true;
              gettimeofday ( &pressure_start_time, NULL );
            }
          }
        }
      }
    }
    else
    {
      gTaskIsRunning = false;
    }

    // Sleep
    gHNV_DataAcquisitionTask_Status = TASK_SLEEPING;
    if ( DAQ_Stt_Idle == DAQ_state )
    {
      vTaskDelay( (portTickType)TASK_DELAY_MS( 2*HNV_DataAcquisitionTask_PERIOD_MS ) );
    }
    else
    {
      vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataAcquisitionTask_PERIOD_MS ) );
    }
    gHNV_DataAcquisitionTask_Status = TASK_RUNNING;
  }
}



//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************


//*****************************************************************************
// Exported functions
//*****************************************************************************

//  Send a packet to the data acquisition task.
//  The data acquisition tasks expects only two types of packets:
//  (a) command packets
//  (b) release of memory pointer packets
//
void data_acquisition_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

// Resume running the task
Bool data_acquisition_resumeTask(void)
{
  // Reset sync errors counter
//data_acquisition_resetSyncErrorCnt();
//data_acquisition_resetFrameCnt();

  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = true;

  // Wait for task to start running
  while(!gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return true;
}


// Pause the Data acquisition task
Bool data_acquisition_pauseTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = false;

  // Wait for task to stop running
  while(gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return true;
}



// Allocate the Data acquisition task and associated control mutex
Bool data_acquisition_createTask(void)
{
  static Bool gTaskCreated = false;

  Bool res = false;

  if(!gTaskCreated)
  {
    // Start bail-out block
    do
    {
      // Create task
      if ( pdPASS != xTaskCreate( DAQ_Task,
                HNV_DataAcquisitionTask_NAME, HNV_DataAcquisitionTask_STACK_SIZE, NULL,
                HNV_DataAcquisitionTask_PRIORITY, &gHNV_DataAcquisitionTask_Handler))
        break;

      // Create control mutex
      if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) )
        break;

      // Create message queue
      if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) )
        break;

      // Allocate memory for frames that are acquired
      //
      int all_allocated = 1;
      int adp;
      for ( adp=0; adp<NumADP; adp++) {
        acquired_data_package[adp].mutex = xSemaphoreCreateMutex();
        acquired_data_package[adp].state = EmptyRAM;
        acquired_data_package[adp].address = (void*) pvPortMalloc ( sizeof(any_acquired_data_t) );

        if ( NULL == acquired_data_package[adp].mutex
          || NULL == acquired_data_package[adp].address ) {
          all_allocated = 0;
        }
      }

      if ( !all_allocated ) {

        for ( adp=0; adp<NumADP; adp++) {
          if ( acquired_data_package[adp].mutex ) {
            //  This is strangely implemented by Atmel:
            //  There is no direct function to delete a mutex.
            //  But a mutex is implemented as a queue,
            //  hence deleting the underlying queue deletes the mutex.
            vQueueDelete(acquired_data_package[adp].mutex);
            acquired_data_package[adp].mutex = NULL;
          }
          if ( acquired_data_package[adp].address ) {
            vPortFree ( acquired_data_package[adp].address );
            acquired_data_package[adp].address = NULL;
          }
        }

        break;  //  To return FALSE
      }

      res = true;
      gTaskCreated = true;

    } while (0);
  
  }
  else
    res = true;

  return res;
}


