/*! \file aux_data_acquisition.c *******************************************************
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
# include <string.h>
# include <time.h>
# include <sys/time.h>

# include <board.h>

# include "aux_data_acquisition.h"

# include "data_exchange_packet.h"
# include "ocr_data.h"
# include "mcoms_data.h"
# include "generic_serial.h"

# include "FreeRTOS.h"
# include "semphr.h"
# include "queue.h"

# include "tasks.controller.h"

# include "io_funcs.controller.h"


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
# define N_RX_PACKETS 6
static xQueueHandle rxPackets = NULL;

static uint32_t const OCR_BAUDRATE = 57600;
static uint8_t        OCR_buffer[OCR_FRAME_LENGTH];
static uint16_t       OCR_bytes = 0;
struct timeval        OCR_timestamp;

static uint32_t const MCOMS_BAUDRATE = 19200;
static char           MCOMS_buffer[MCOMS_FRAME_LENGTH] = { 0 };
static uint16_t       MCOMS_bytes = 0;
static uint16_t const MCOMS_fields = 13;
static  int16_t       MCOMS_num_tabs = 0;
struct timeval        MCOMS_timestamp;

//  Expect to need four pointers for auxiliary sensor data,
//  two each for collecting,
//  and two each sent via packets to other task
typedef union {
  OCR_Frame_t         OCR_Frame;
  MCOMS_Frame_t     MCOMS_Frame;
} any_acquired_data_t;



# define N_DA_POINTERS 4
static data_exchange_data_package_t acquired_data_package[N_DA_POINTERS] = { {0, 0, 0} };



//*****************************************************************************
// Local Functions
//*****************************************************************************

/* Subtract the ‘struct timeval’ values x and y,
   result  = x - y
   Return 1 if the difference is negative, otherwise 0. */



static inline struct timeval timeval_diff ( struct timeval const early, struct timeval const late )
{

  if ( late.tv_usec < early.tv_usec )
  {
    return (struct timeval){ tv_sec:  late.tv_sec  -       1 - early.tv_sec,
                             tv_usec: late.tv_usec + 1000000 - early.tv_usec };
  }
  else
  {
    return (struct timeval){ tv_sec:  late.tv_sec  - early.tv_sec,
                             tv_usec: late.tv_usec - early.tv_usec };
  }
}



/* Subtract the ‘struct timespec’ values x and y,
   result  = x - y
   Return 1 if the difference is negative, otherwise 0. */

static inline struct timespec timespec_diff ( struct timespec const early, struct timespec const late )
{
  if ( late.tv_nsec < early.tv_nsec )
  {
    return (struct timespec){ tv_sec:  late.tv_sec  -          1 - early.tv_sec,
                              tv_nsec: late.tv_nsec + 1000000000 - early.tv_nsec };
  }
  else
  {
    return (struct timespec){ tv_sec:  late.tv_sec  - early.tv_sec,
                              tv_nsec: late.tv_nsec - early.tv_nsec };
  }
}



//*****************************************************************************
// Local Tasks Implementation
//*****************************************************************************

// Data acquisition task
static void aux_data_acquisition( __attribute__((unused)) void* pvParameters )
{
# if 0
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif
# warning "Hard Coding FPBA=FOSC0 for use in auxiliary sensor USARTs (Burkhard)"
  U32 const fPBA = FOSC0;

  enum {
    ADQ_Stt_Idle,
# if defined(OPERATION_AUTONOMOUS)
    ADQ_Stt_Stream,
# endif
# if defined(OPERATION_NAVIS)
    ADQ_Stt_Profile,
# endif
  } task_state = ADQ_Stt_Idle;

  enum {
    ADQ_OCR_Off,
    ADQ_OCR_Streaming
  } ADQ_OCR_phase = ADQ_OCR_Off;

  enum {
    ADQ_MCOMS_Off,
    ADQ_MCOMS_Idle,
    ADQ_MCOMS_Polled
  } ADQ_MCOMS_phase = ADQ_MCOMS_Off;

  data_exchange_address_t const myAddress = DE_Addr_AuxDataAcquisition;

  gettimeofday ( &OCR_timestamp, NULL );
  time_t next_ocr_send = OCR_timestamp.tv_sec;

  for(;;) {

    if(gRunTask) {
# if 0
      //  Report stack water mark, if changed to previous water mark
      //
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
      gTaskIsRunning = true;

      //  Order of operation:
      //  1. Check for & handle first incoming packet
      //  2. Read available data from OCR 504 port
      //  3. If full OCR 504 frame available, parse & send to stream_log or profile_manager
      //  4. Read available data from MCOMS port
      //  5. If full MCOMS frame available, parse & send to stream_log or profile_manager

      //  1. Check for incoming packets
      //     ERROR if packet.to not myself (myAddress)
      //     Accept   type == Ping, Command
      //     Ignore   other type

      data_exchange_packet_t packet_rx_via_queue;

      if ( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) ) {

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
          switch ( packet_rx_via_queue.type )
          {
          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
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
          //  the packet_response.to address will be overwritten.
          //  A non DE_Addr_Nobody value will be used as a flag
          //  to send the packet_response.
          data_exchange_packet_t packet_response;
          packet_response.to = DE_Addr_Nobody;

          switch ( packet_rx_via_queue.type )
          {

          case DE_Type_Nothing:
            break;

          case DE_Type_Ping:

            packet_response.to = packet_rx_via_queue.from;
            packet_response.from = myAddress;
            packet_response.type = DE_Type_Response;
            packet_response.data.Response.Code = RSP_ALL_Ping;
            packet_response.data.Response.value.u64 = 0;  //  Will be ignored
            break;

          case DE_Type_Command:

            switch ( packet_rx_via_queue.data.Command.Code )
            {

# if defined(OPERATION_AUTONOMOUS)
            case CMD_ADQ_StartStreaming:
              {
                int GSP_state = 0;

                if ( task_state == ADQ_Stt_Idle )
                {
                
                  GSP_state = GSP_Init ( GSP_OCR, fPBA, OCR_BAUDRATE, OCR_FRAME_LENGTH, 32 );

                  if ( 0 == GSP_state )
                  {
                    ADQ_OCR_phase = ADQ_OCR_Streaming;
                    ADQ_MCOMS_phase = ADQ_MCOMS_Idle;
                    task_state = ADQ_Stt_Stream;
		              }
                }

                if (1)
                {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_ADQ_Started;
                  packet_message.data.Syslog.value  = (U64)GSP_state;
                  data_exchange_packet_router ( myAddress, &packet_message );
                }

              }

              break;
# endif

# if defined(OPERATION_NAVIS)
            case CMD_ADQ_StartProfiling:

              if ( task_state == ADQ_Stt_Idle )
              {
                GSP_Init ( GSP_OCR, fPBA, OCR_BAUDRATE, OCR_FRAME_LENGTH, 32 );
                ADQ_OCR_phase = ADQ_OCR_Streaming;

                ADQ_MCOMS_phase = ADQ_MCOMS_Idle;

                task_state = ADQ_Stt_Profile;
              }
              break;
# endif

            case CMD_ADQ_Stop:

              if  (1)
              {
                data_exchange_packet_t packet_message;
		            packet_message.to   = DE_Addr_ControllerBoard_Commander;
		            packet_message.from = myAddress;
		            packet_message.type = DE_Type_Syslog_Message;
		            packet_message.data.Syslog.number = SL_MSG_ADQ_Stopped;
		            packet_message.data.Syslog.value  = 0;
                data_exchange_packet_router ( myAddress, &packet_message );
              }

              if  ( task_state != ADQ_Stt_Idle )
              {

                GSP_Deinit ( GSP_OCR );
                ADQ_OCR_phase = ADQ_OCR_Off;

                ADQ_MCOMS_phase = ADQ_MCOMS_Off;

                task_state = ADQ_Stt_Idle;
              }
              break;

            case CMD_ADQ_PollMCOMS:

              if  ( task_state != ADQ_Stt_Idle )
              {
                if  ( ADQ_MCOMS_phase == ADQ_MCOMS_Idle )
                {
                  GSP_Init ( GSP_MCOMS, fPBA, MCOMS_BAUDRATE, MCOMS_FRAME_LENGTH, 32 );
                  ADQ_MCOMS_phase = ADQ_MCOMS_Polled;
                  MCOMS_bytes = 0;

                  //  Wait for completion of polled data acquisition
                  //  before responding with RSP_DAQ_MCOMSDone
                }
                else
                {
                  //  This is an ERROR: The Poll request should only be sent to an idle MCOMS
                  //  Regardless: Immediately respond: MCOMS Quiet
                }
              }
              else
              {
                //  Immediately respond: MCOMS Quiet
              }
              break;

            case CMD_ADQ_InterruptMCOMS:

              if  ( task_state != ADQ_Stt_Idle )
              {

                if  ( ADQ_MCOMS_phase == ADQ_MCOMS_Polled )
                {
                  GSP_Deinit ( GSP_MCOMS );
                  ADQ_MCOMS_phase = ADQ_MCOMS_Idle;
                  //io_out_string ( "MCOMS off\r\n" );
                }
                else
                {
                  //  Ignore interrupt request while not polling
                }

              }
              else
              {
                //  Ignore interrupt request while idle
              }

              break;

            case CMD_ANY_Query:

              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_ADQ_Query;
              if ( task_state == ADQ_Stt_Idle )
              {
                packet_response.data.Response.value.rsp = RSP_ADQ_Query_Is_Idle;
              }
              else
              {
                if ( ADQ_OCR_phase == ADQ_OCR_Off && ADQ_MCOMS_phase == ADQ_MCOMS_Off )
                {
                  packet_response.data.Response.value.rsp = RSP_ADQ_Query_Error_AllOff;
                }
                
                else if ( ADQ_OCR_phase == ADQ_OCR_Off )
                {
                  packet_response.data.Response.value.rsp = RSP_ADQ_Query_Error_OCROff;
                }
                
                else if ( ADQ_MCOMS_phase == ADQ_MCOMS_Off )
                {
                  packet_response.data.Response.value.rsp = RSP_ADQ_Query_Error_MCOMSOff;
                }
                
                else if ( ADQ_MCOMS_phase == ADQ_MCOMS_Idle )
                {
                  packet_response.data.Response.value.rsp = RSP_ADQ_Query_Ok;
                }
                
                else
                {
                  packet_response.data.Response.value.rsp = RSP_ADQ_Query_MCOMSPolling;
                }
              }
              break;

            default:
              {
                data_exchange_packet_t packet_error;
                
                packet_error.from               = myAddress;
                packet_error.to                 = DE_Addr_ControllerBoard_Commander;
                packet_error.type               = DE_Type_Syslog_Message;
                packet_error.data.Syslog.number = SL_ERR_UnexpectedCommand;
                packet_error.data.Syslog.value  = packet_rx_via_queue.from;
                
                data_exchange_packet_router( myAddress, &packet_error );
              }
            }

            break;

          case DE_Type_Response:
            //  Not expecting any responses since this task is not sending any commands
            {
              data_exchange_packet_t packet_error;
              
              packet_error.from               = myAddress;
              packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              packet_error.type               = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_ERR_UnexpectedResponse;
              packet_error.data.Syslog.value  = packet_rx_via_queue.from;
              
              data_exchange_packet_router( myAddress, &packet_error );
            }
            break;

          case DE_Type_Configuration_Data:
          case DE_Type_Spectrometer_Data:
          case DE_Type_OCR_Frame:
          case DE_Type_MCOMS_Frame:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            //  Not expecting any data - must discard!
            {
              data_exchange_packet_t packet_error;
              
              packet_error.from               = myAddress;
              packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              packet_error.type               = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_ERR_UnexpectedData;
              packet_error.data.Syslog.value  = packet_rx_via_queue.from;
              
              data_exchange_packet_router( myAddress, &packet_error );
            }

            if  ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) )
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

          case DE_Type_Syslog_Message:
            {
              data_exchange_packet_t packet_error;

              packet_error.from               = myAddress;
              packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              packet_error.type               = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_ERR_UnexpectedPacketType;
              packet_error.data.Syslog.value  = packet_rx_via_queue.from;

              data_exchange_packet_router( myAddress, &packet_error );
            }
            break;

            //  Do NOT add a "default:" here.
            //  That way, the compiler will generate a warning
            //  if one possible packet type was missed.
          }

          if  ( packet_response.to != DE_Addr_Nobody )
          {
            data_exchange_packet_router ( myAddress, &packet_response );
          }
        }
      }

      if ( ADQ_OCR_phase == ADQ_OCR_Streaming )
      {

        //  2. Read available data from OCR 504 port
        //

        {

          uint8_t ocr_byte;

          while ( OCR_bytes < OCR_FRAME_LENGTH
               && 1 == GSP_Recv( GSP_OCR, &ocr_byte, 1, GSP_NONBLOCK ) )
          {

            //  Only accept bytes if the frame sync is present
            //
            if ( 0 == OCR_bytes && 'S' != ocr_byte ) {                continue; }
            if ( 1 == OCR_bytes && 'A' != ocr_byte ) { OCR_bytes = 0; continue; }
            if ( 2 == OCR_bytes && 'T' != ocr_byte ) { OCR_bytes = 0; continue; }
            if ( 3 == OCR_bytes && 'D' != ocr_byte ) { OCR_bytes = 0; continue; }
            if ( 4 == OCR_bytes && 'I' != ocr_byte ) { OCR_bytes = 0; continue; }
            if ( 5 == OCR_bytes && '4' != ocr_byte ) { OCR_bytes = 0; continue; }

            if ( 6 == OCR_bytes )
            {
              gettimeofday ( &OCR_timestamp, NULL );
            }

            OCR_buffer [ OCR_bytes ] = ocr_byte;
            OCR_bytes ++;
          }
        }

        //  3. If full OCR 504 frame available, parse & send to stream_log or profile_manager
        //
        //  Data Frame Format:
        //  AS10 SATDI4####
        //  AS10 Timer
        //  BS2  Delay
        //  BU4  Pixel1
        //  BU4  Pixel2
        //  BU4  Pixel3
        //  BU4  Pixel4
        //  BU2  Volts
        //  BU2  Temperature
        //  BU1  Frame Counter
        //  BU1  Check SUm
        //  BU2  CRLF Terminator
        //    46 total bytes
        //

        if ( OCR_FRAME_LENGTH == OCR_bytes )
        {
          if ( OCR_buffer[OCR_FRAME_LENGTH-2] != '\r'  || OCR_buffer[OCR_FRAME_LENGTH-1] != '\n' )
          {
            //  Invalid, no/wrong termination ==> discard
            //  The data reception is not accepting of lost bytes.
            //  If a byte is lost from a frame, then the 46 bytes
            //  that are received contain bytes from more than one frame.
            OCR_bytes = 0;
            //  TODO: A better approach is to look for the SATDI4 frame header
            //  in the buffer, and shift all to the beginning of the OCR_buffer.
          }
          else
          {
            //  TODO Verify chack sum
            uint8_t checksum = 0;
            if ( checksum ) 
            {

              //  Discard invalid data
              OCR_bytes = 0;

            }
            else
            {
              if  ( OCR_timestamp.tv_sec > next_ocr_send )
              {
                //  Extract values from frame and send via packet

                int sent_ocr = 0;
                int i;
                for ( i=0; i<N_DA_POINTERS && !sent_ocr; i++ )
                {
                  if ( pdTRUE == xSemaphoreTake ( acquired_data_package[i].mutex, portMAX_DELAY ) )
                  {

                    if  (acquired_data_package[i].state == EmptyRAM )
                    {

                      data_exchange_packet_t OCR_packet;

                      OCR_packet.from = myAddress;
                      switch ( task_state )
                      {
# if defined(OPERATION_AUTONOMOUS)
                      case ADQ_Stt_Stream:  OCR_packet.to = DE_Addr_StreamLog;      break;
# endif
# if defined(OPERATION_NAVIS)
                      case ADQ_Stt_Profile: OCR_packet.to = DE_Addr_ProfileManager; break;
# endif
                      case ADQ_Stt_Idle:    
               		    default:  OCR_packet.to = DE_Addr_Nobody;
                      }
                    
                      OCR_packet.type = DE_Type_OCR_Frame;
                      //  Copy OCR_buffer and time stamp into to-be-sent data package
                      //
                      OCR_Frame_t* ocr_frame = acquired_data_package[i].address;
                      memcpy ( ocr_frame->frame, OCR_buffer, OCR_FRAME_LENGTH );
                      ocr_frame->aux.acquisition_time = OCR_timestamp;
                      acquired_data_package[i].state = FullRAM;
                      
                      OCR_packet.data.DataPackagePointer = &acquired_data_package[i];
                      
                      //  Send packet to destination
                      //
                      data_exchange_packet_router ( myAddress, &OCR_packet );
                      sent_ocr = 1;
                    }

                    xSemaphoreGive ( acquired_data_package[i].mutex );
                  }
                }
            
                OCR_bytes = 0;

                next_ocr_send = OCR_timestamp.tv_sec + 4;

              }
              else
              {
                OCR_bytes = 0;
              }
            }
          }
        }
      }

      if  ( ADQ_MCOMS_phase == ADQ_MCOMS_Polled )
      {

        //  4. Read available data from MCOMS port
        //     Frame is variable length ASCII, tab separated:
        //     e.g.,
        //     MCOMSC-128 2954 335 2933 2933 3249 15251 15368 152510 3506 3506 187 1416 1416
        //     SerialNmbr LED  LG  HG   VAL  LED  LG    HG    VAL    LED  LED  LG  HG   VAL
        //               |  chlorophyll     |   Backscatter 700nm   |   FDOM                |
        //
        //     LED = LED forward voltage
        //     LG  = Low Gain reading
        //     HG  = High Gain reading
        //     VAL = Value to be used: { Low Gain * 10 | High Gain }
        //
        //     The two LED values for FDOM are always identical.
        //     They were kept in the output for backward compatibility.
        //

        {
          int rd = 0;
          uint8_t mcoms_byte;

          while  (MCOMS_bytes < MCOMS_FRAME_LENGTH  &&  1 == GSP_Recv( GSP_MCOMS, &mcoms_byte, 1, GSP_NONBLOCK ) )
          {

            rd++;

            //  Only accept bytes if the frame sync is present
            //
            if ( 0 == MCOMS_bytes && 'M' != mcoms_byte ) {                  continue; }
            if ( 1 == MCOMS_bytes && 'C' != mcoms_byte ) { MCOMS_bytes = 0; continue; }
            if ( 2 == MCOMS_bytes && 'O' != mcoms_byte ) { MCOMS_bytes = 0; continue; }
            if ( 3 == MCOMS_bytes && 'M' != mcoms_byte ) { MCOMS_bytes = 0; continue; }
            if ( 4 == MCOMS_bytes && 'S' != mcoms_byte ) { MCOMS_bytes = 0; continue; }
            if ( 5 == MCOMS_bytes && 'C' != mcoms_byte ) { MCOMS_bytes = 0; continue; }
            if ( 6 == MCOMS_bytes && '-' != mcoms_byte ) { MCOMS_bytes = 0; continue; }

            if  ('\t' == mcoms_byte)
              MCOMS_num_tabs++;

            MCOMS_buffer [ MCOMS_bytes ] = mcoms_byte;
            MCOMS_bytes ++;
          }
        }

        //  5. If full MCOMS frame available, parse & send to stream_log or profile_manager
        //

        //  If buffer is full without termination (i.e., unexpected input,
        //  because MCOMS_FRAME_LENGTH is big enough to hold any MCOMS frame),
        //  then clear buffer.
        if   (MCOMS_bytes == MCOMS_FRAME_LENGTH  &&  !(MCOMS_buffer[ MCOMS_FRAME_LENGTH-2] == '\r' && MCOMS_buffer[MCOMS_FRAME_LENGTH-1] == '\n' ) )
        {
          MCOMS_bytes = 0;
          MCOMS_num_tabs = 0;
        }

        //  If buffer is terminated, try parsing
        if  (MCOMS_buffer[MCOMS_bytes-2] == '\r' && MCOMS_buffer[MCOMS_bytes-1] == '\n' )
        {
          if  (MCOMS_num_tabs != MCOMS_fields )
          {
            //  Invalid line
            MCOMS_bytes = 0;
            MCOMS_num_tabs = 0;
          }
          else
          {
            int16_t  chl_led,  chl_low,  chl_hgh;  int32_t  chl_value;
            int16_t   bb_led,   bb_low,   bb_hgh;  int32_t   bb_value;
            int16_t fdom_led, fdom_low, fdom_hgh;  int32_t fdom_value;

            if ( (MCOMS_fields-1) == sscanf ( MCOMS_buffer, "MCOMSC-%*d"
                                                      "\t%hd\t%hd\t%hd\t%ld"
                                                      "\t%hd\t%hd\t%hd\t%ld"
                                                 "\t%*d\t%hd\t%hd\t%hd\t%ld\r\n",
                                                  &chl_led,  &chl_low,  &chl_hgh,  &chl_value,
                                                   &bb_led,   &bb_low,   &bb_hgh,   &bb_value,
                                                 &fdom_led, &fdom_low, &fdom_hgh, &fdom_value ) ) {

              //  Send buffer or data
              int sent_mcoms = 0;
              int i;
              for  ( i=0; i<N_DA_POINTERS && !sent_mcoms; i++ )
              {
                if ( pdTRUE == xSemaphoreTake ( acquired_data_package[i].mutex, portMAX_DELAY ) )
                {

                  if ( acquired_data_package[i].state == EmptyRAM )
                  {

                    data_exchange_packet_t MCOMS_packet;

                    MCOMS_packet.from = myAddress;
                    switch ( task_state )
                    {
# if defined(OPERATION_AUTONOMOUS)
                    case ADQ_Stt_Stream:  MCOMS_packet.to = DE_Addr_StreamLog;      break;
# endif
# if defined(OPERATION_NAVIS)
                    case ADQ_Stt_Profile: MCOMS_packet.to = DE_Addr_ProfileManager; break;
# endif
                    case ADQ_Stt_Idle:    
                    default:   MCOMS_packet.to = DE_Addr_Nobody;
		                }

                    MCOMS_packet.type = DE_Type_MCOMS_Frame;
                    //  Copy MCOMS_buffer into to-be-sent data package
                    MCOMS_Frame_t*  mcoms_frame = acquired_data_package[i].address;

                    memcpy ( mcoms_frame->frame, MCOMS_buffer, MCOMS_bytes );
                    
                    mcoms_frame->aux.acquisition_time = MCOMS_timestamp;
                    
                    acquired_data_package[i].state = FullRAM;
                    MCOMS_packet.data.DataPackagePointer = &acquired_data_package[i];

                    //  Send packet to destination
                    //
                    data_exchange_packet_router ( myAddress, &MCOMS_packet );
                    sent_mcoms = 1;
                    ADQ_MCOMS_phase = ADQ_MCOMS_Idle;
                  }

                  xSemaphoreGive ( acquired_data_package[i].mutex );
                }
              }
              MCOMS_bytes = 0;
              MCOMS_num_tabs = 0;
            }
            else
            {
              MCOMS_bytes = 0;
              MCOMS_num_tabs = 0;
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
    THIS_TASK_WILL_SLEEP( gHNV_AuxDatAcqTask_Status );
    vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_AuxDatAcqTask_PERIOD_MS ) );
    THIS_TASK_IS_RUNNING( gHNV_AuxDatAcqTask_Status );
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
void aux_data_acquisition_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

// Resume running the task
Bool aux_data_acquisition_resumeTask(void)
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
Bool aux_data_acquisition_pauseTask(void)
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
Bool aux_data_acquisition_createTask(void)
{
  static Bool gTaskCreated = false;

  Bool res = false;

  if  (!gTaskCreated)
  {
    // Start bail-out block
    do
    {
      // Create task
      if  ( pdPASS != xTaskCreate( aux_data_acquisition,
                HNV_AuxDatAcqTask_NAME, HNV_AuxDatAcqTask_STACK_SIZE, NULL,
                HNV_AuxDatAcqTask_PRIORITY, &gHNV_AuxDatAcqTask_Handler))
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
      int i;
      for  ( i = 0;  i < N_DA_POINTERS;  i++)
      {
        acquired_data_package[i].mutex = xSemaphoreCreateMutex();
        acquired_data_package[i].state = EmptyRAM;
        acquired_data_package[i].address = (void*) pvPortMalloc ( sizeof(any_acquired_data_t) );

        if ( NULL == acquired_data_package[i].mutex  ||  NULL == acquired_data_package[i].address )
        {
          all_allocated = 0;
        }
      }

      if ( !all_allocated )
      {

        for  (i = 0;  i < N_DA_POINTERS;  i++)
        {
          if ( acquired_data_package[i].mutex ) 
          {
            //  This is strangely implemented by Atmel:
            //  There is no direct function to delete a mutex.
            //  But a mutex is implemented as a queue,
            //  hence deleting the underlying queue deletes the mutex.
            vQueueDelete(acquired_data_package[i].mutex);
            acquired_data_package[i].mutex = NULL;
          }
          
          if ( acquired_data_package[i].address )
          {
            vPortFree ( acquired_data_package[i].address );
            acquired_data_package[i].address = NULL;
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
