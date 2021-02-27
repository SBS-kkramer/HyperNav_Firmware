/*! \file stream_log.c *******************************************************
 *
 * \brief Streaming log task
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#define  OPERATION_AUTONOMOUS


# if defined(OPERATION_AUTONOMOUS)

# include "stream_log.h"

# include <string.h>
# include <time.h>
# include <sys/time.h>

# include "FreeRTOS.h"
# include "semphr.h"

# include "tasks.controller.h"
# include "data_exchange_packet.h"
# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"

# include "sram_memory_map.controller.h"

# include "datalogfile.h"
# include "frames.h"
# include "telemetry.h"
# include "syslog.h"
# include "io_funcs.controller.h"
# include "config.controller.h"


//*****************************************************************************
// Settings
//*****************************************************************************

//*****************************************************************************
// Local Variables
//*****************************************************************************

// Task control
static Bool gRunTask = FALSE;
static Bool gTaskIsRunning = FALSE;

static xSemaphoreHandle gTaskCtrlMutex = NULL;

// packet receiving queue
# define N_RX_PACKETS 6
static xQueueHandle rxPackets = NULL;

//  Use RAM for data received via packet transfer
typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Frame_t                          OCR_Frame;
  MCOMS_Frame_t                      MCOMS_Frame;
} any_received_data_t;
static data_exchange_data_package_t received_data_package;
static data_exchange_type_t         received_data_type;

# define MAX_SLG_FRAME_LEN (4096+512)
//static uint8_t* slg_frame = 0;

//*****************************************************************************
// Local Tasks Implementation
//*****************************************************************************

// Streaming and logging task
static void stream_log_loop( __attribute__((unused)) void* pvParameters_notUsed ) {

# if 0
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif
  //  Data arrive via queue, organized in packets
  //  Data are written streamed to RS-232 and written to eMMC

  //  Initial state: Wait for data or command.
  //    If idle && receiving data, switch to SLG_Rxing
  //
  enum { SLG_Idle, SLG_Rxing } slg_state = SLG_Idle;
//time_t last_received = 0;
//S64 start_value = 0;

  data_exchange_address_t const myAddress = DE_Addr_StreamLog;

  for(;;) {

    if(gRunTask)
    {
      gTaskIsRunning = TRUE;
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
      //  Check for incoming packets
      //  ERROR if packet.to not myself (myAddress)
      //  Accept   type == Ping, Command, DE_Type_HyperNav_Spectrometer_Data, DE_Type_HyperNav_Auxiliary_Data
      //  ERROR/Discard if other type

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

        } else {  //  packet_rx_via_queue.to == myAddress

          //  If there is a need to respond,
          //  the address packet_response.to will be overwritten later.
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
            packet_response.data.Response.value.u64 = 0;  //  Will be ignored
            break;

          case DE_Type_Command:

            switch ( packet_rx_via_queue.data.Command.Code ) {

            case CMD_ANY_Query:
              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_SLG_Query;
              switch ( slg_state ) {
              case SLG_Idle:  packet_response.data.Response.value.rsp = RSP_SLG_Query_Is_Idle; break;
              case SLG_Rxing: packet_response.data.Response.value.rsp = RSP_SLG_Query_Is_Rx;   break;
              }
              break;

            case CMD_SLG_Start:

              //  During development only: Report back to commander
              if (1) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_SLG_Started;
                  packet_message.data.Syslog.value  = 0;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if ( SLG_Idle == slg_state ) {

                struct timeval now; gettimeofday ( &now, NULL );
//              last_received = now.tv_sec;
                struct tm date_time; gmtime_r ( &(now.tv_sec), &date_time );
                char yymmdd[32]; strftime ( yymmdd, sizeof(yymmdd), "%y-%m-%d", &date_time );
                int16_t rv = DLF_Start( yymmdd, 'F' );

                //  Report back to commander if error occurred
                if ( rv < 0 ) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_ERR_SLG_StartFail;
                  packet_message.data.Syslog.value  = rv;
                  data_exchange_packet_router ( myAddress, &packet_message );
                } else {
                  slg_state = SLG_Rxing;
                }
              }

              if ( SLG_Rxing == slg_state ) {
                //  If start up succeeded,
                //  pass on command to data acquisition task,
                //  even of this task was already started.
                //  That way, re-sending the start command
                //  is a safe approach to re-attempting a start.
                packet_response.to                = DE_Addr_DataAcquisition;
                packet_response.from              = myAddress;
                packet_response.type              = DE_Type_Command;
                packet_response.data.Command.Code = CMD_DAQ_Start;
//              start_value = 
                packet_response.data.Command.value.s64 = packet_rx_via_queue.data.Command.value.s64;
              }

              break;

            case CMD_SLG_Stop:

              //  During development only: Report back to commander
              if (1) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_SLG_Stopped;
                  packet_message.data.Syslog.value  = 0;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if ( SLG_Rxing == slg_state ) {

                slg_state = SLG_Idle;

                DLF_Stop();
              }

              if ( SLG_Idle == slg_state ) {
                //  Pass on command to data acquisition task,
                //  even of this task is stopped.
                //  That way, re-sending the stop command
                //  is a safe approach to re-attempting a stop.
                packet_response.to                = DE_Addr_DataAcquisition;
                packet_response.from              = myAddress;
                packet_response.type              = DE_Type_Command;
                packet_response.data.Command.Code = CMD_DAQ_Stop;
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

          case DE_Type_Syslog_Message:
          //  Not expecting any syslog messages
          {
              data_exchange_packet_t packet_error;

              packet_error.from               = myAddress;
              packet_error.to                 = DE_Addr_ControllerBoard_Commander;
              packet_error.type               = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_ERR_UnexpectedSysMessage;
              packet_error.data.Syslog.value  = packet_rx_via_queue.from;

              data_exchange_packet_router( myAddress, &packet_error );
          }
          break;

          case DE_Type_Spectrometer_Data:

            //  This packet contains spectrometer data.
            //  Make a local copy of the data.
            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  //  RAM to RAM copy
                  memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Spectrometer_Data_t) );
                  received_data_type = DE_Type_Spectrometer_Data;
                  received_data_package.state = FullRAM;
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  //  SRAM to RAM copy
                  sram_read ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(Spectrometer_Data_t) );
                  received_data_type = DE_Type_Spectrometer_Data;
                  received_data_package.state = FullRAM;
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "data_exchange_controller_task", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_OCR_Frame:

            //  This packet contains a OCR frame.
            //  Make a local copy of the frame.

            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  //  RAM to RAM copy
                  memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(OCR_Frame_t) );
                  received_data_type = DE_Type_OCR_Frame;
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                  received_data_package.state = FullRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  //  SRAM to RAM copy
                  sram_read ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(OCR_Frame_t) );
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                  received_data_package.state = FullRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "data_exchange_controller_task", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_MCOMS_Frame:

            //  This packet contains a MCOMS data address.
            //  Make a local copy of the data.

            if ( received_data_package.state == EmptyRAM ) {
              if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                  memcpy ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(MCOMS_Frame_t) );
                  received_data_type = DE_Type_MCOMS_Frame;
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                  received_data_package.state = FullRAM;
                } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                  //  SRAM to RAM copy
                  sram_read ( received_data_package.address, packet_rx_via_queue.data.DataPackagePointer->address, sizeof(MCOMS_Frame_t) );
                  received_data_type = DE_Type_MCOMS_Frame;
                  received_data_package.state = FullRAM;
                  packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                } else {
                  syslog_out ( SYSLOG_ERROR, "data_exchange_controller_task", "Incoming data empty" );
                }
                xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
              }
            }

            break;

          case DE_Type_Configuration_Data:
          case DE_Type_Profile_Info_Packet:
          case DE_Type_Profile_Data_Packet:
            syslog_out ( SYSLOG_ERROR, "stream_log_task", "Discarding unexpected data packet" );
            if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
              if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
              } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
              }
              xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
            }

            break;

          //  Do NOT add a "default:" here.
          //  That way, the compiler will generate a warning
          //  if one possible packet type was missed.

          }

          //  If needed, send back the response packet
          if  ( packet_response.to != DE_Addr_Nobody )
          {
            data_exchange_packet_router ( myAddress, &packet_response );
          }

          if ( FullRAM == received_data_package.state )
          {

            switch ( received_data_type ) {

            case DE_Type_Spectrometer_Data:

              if ( SLG_Rxing == slg_state )
              {

                struct timeval now; gettimeofday ( &now, NULL );
//              last_received = now.tv_sec;

                uint8_t const outFrameSubsampling = CFG_Get_Output_Frame_Subsampling();
//              CFG_Log_Frames const logFrameType = CFG_Get_Log_Frames();

                uint8_t const logFrameSubsampling = 0;

# if 1
                int16_t rv = frm_out_or_log ( received_data_package.address, outFrameSubsampling, logFrameSubsampling );

                if ( FRAME_OK != rv ) {
                  io_out_S32 ( "ERROR %ld at out/log frame.\r\n", (S32)rv );
                }
# else

                uint16_t slg_frame_len = 0;

                if ( FRAME_OK != frm_buildFrame( outFrameSubsampling,
                                   slg_frame, MAX_SLG_FRAME_LEN, &slg_frame_len, received_data_package.address ) ) {
                  io_out_string ( "ERROR: out frame not built.\r\n" );
                } else {

                  {
                    //io_out_S32 ( "Stack High Water Mark %ld\r\n", uxTaskGetStackHighWaterMark(0) );
                  }

                  if ( slg_frame_len > 0 ) {
                  tlm_send ( slg_frame, slg_frame_len, 0 );
                  }

                  {
                    Spectrometer_Data_t* sdp = received_data_package.address;

                    uint32_t a=1, b=0;
                    size_t idx;
                    size_t const len = sizeof(Spectrometer_Data_t);
                    uint8_t* mem = (uint8_t*)sdp;

                    for ( idx=0; idx<len; idx++ ) {
                      a = ( a + mem[idx] ) % 65521;
                      b = ( b + a )        % 65521;
                    }

                    io_out_S32 ( "FAKE FRAME OUT %ld", slg_frame_len );
                      io_out_S32 ( " rx %d ", (S32)sdp->aux.side );
                      io_out_S32 ( "%d ", (S32)sdp->aux.sample_number );

                      int allOK = 1;
                      int sameVal = 0;
                      uint16_t val = 0;

                      int i;
                      for ( i=0; i<2048; i++ ) {

                        if ( allOK ) {
                          if ( i != sdp->light_minus_dark_spectrum[i] ) {
                            allOK = 0;
                            io_out_S32 ( "OK: %ld ", (S32)i );
                            val = sdp->light_minus_dark_spectrum[i];
                            sameVal = 0;
                          }
                        } else {
                          if ( val == sdp->light_minus_dark_spectrum[i] ) {
                            sameVal++;
                          } else {
                            if ( sameVal>0 ) io_out_S32 ( " %ld x ", (S32)(sameVal+1) );
                            io_out_X16 ( (U16)val, " " );
                            val = sdp->light_minus_dark_spectrum[i];
                            sameVal = 0;
                          }
                        }
                      }

                      if ( sameVal>0 ) {
                            if ( sameVal>0 ) io_out_S32 ( " %ld x ", (S32)(sameVal+1) );
                            io_out_X16 ( (U16)val, " " );
                      }

                      if ( allOK ) {
                            io_out_S32 ( "OK: %ld ", (S32)i );
                      }

                      io_out_S32 ( "%d ", (S32)len );
                      io_out_X32 ( (U32)( ( b<<16 ) | a ), "\r\n" );
                  }
                }
                if ( 1 || CFG_Internal_Data_Logging_Available == CFG_Get_Internal_Data_Logging()
                  && CFG_Log_Frames_No != logFrameType ) {    //  i.e., requested to log

                  if ( outFrameSubsampling > 0 || 0 == slg_frame_len ) {
                    //  Rebuild frame if frame subsampling differs or the frame was not built above

                    slg_frame_len = 0;

                    //  No subsampling for internally logged frame
                    //
                    if ( FRAME_OK != frm_buildFrame( (uint8_t)0,
                                       slg_frame, MAX_SLG_FRAME_LEN, &slg_frame_len, received_data_package.address ) ) {
                      io_out_string ( "ERROR: log frame not built.\r\n" );
                    }
                  }

                  //  Report stack water mark
                  //
                  {
                    //io_out_S32 ( "Stack High Water Mark %ld\r\n", uxTaskGetStackHighWaterMark(0) );
                  }

# if 1
                  if ( slg_frame_len > 0 ) {
                    int32_t written;
                    if ( slg_frame_len != ( written = DLF_Write ( slg_frame, slg_frame_len ) ) ) {
                      io_out_S32 ( "ERROR: frame of length %ld not written to file.", slg_frame_len );
                      io_out_S32 ( "Write code %ld\r\n", written );
                    }
                  }
# endif
                }
# endif
              }
              else
              {
                io_out_string ( "Error: RX Spec Data in non acq mode\r\n" );
	            }
              break;

            case DE_Type_OCR_Frame:

              if ( SLG_Rxing == slg_state ) {
                if ( CFG_Get_Output_Frame_Subsampling() <= 13 ) {
                  tlm_send ( received_data_package.address, OCR_FRAME_LENGTH, 0 );
                }

                if ( OCR_FRAME_LENGTH != DLF_Write ( received_data_package.address, OCR_FRAME_LENGTH ) ) {
                  io_out_string ( "ERROR: frame not logged to file.\r\n" );
                }
              } else {
                io_out_string ( "Error: RX OCR Data in non acq mode\r\n" );
	      }
              break;

            case DE_Type_MCOMS_Frame:
              //  MCOMS is variable length frame.
              //  The address points to the frame, but the frame is not NUL character terminated.
              //  Search for CRLF terminator in frame, but cannot use strstr() due to non NUL termination.
              if ( SLG_Rxing == slg_state ) {
                uint8_t* const CR = memchr ( received_data_package.address, '\r', -1 );
                if ( CR && *(CR+1)=='\n' ) {
                  uint16_t const frame_length = ((void*)CR+2)-received_data_package.address;

                  if ( CFG_Get_Output_Frame_Subsampling() <= 13 ) {
                    tlm_send ( received_data_package.address, frame_length, 0 );
                  }

                  if ( MCOMS_FRAME_LENGTH != DLF_Write ( received_data_package.address, MCOMS_FRAME_LENGTH ) ) {
                    io_out_string ( "ERROR: frame not logged to file.\r\n" );
                  }
                }
              } else {
                io_out_string ( "Error: RX MCOMS Data in non acq mode\r\n" );
	      }
              break;

            case DE_Type_Nothing:
            case DE_Type_Ping:
            case DE_Type_Command:
            case DE_Type_Response:
            case DE_Type_Syslog_Message:
            case DE_Type_Configuration_Data:
            case DE_Type_Profile_Info_Packet:
            case DE_Type_Profile_Data_Packet:
              //  Cannot happen. Ignore.
              break;
            }

            //  All handled.
            //  Clean up.
            received_data_type = DE_Type_Nothing;
            received_data_package.state = EmptyRAM;
          }

        }  //  closing bracket for packet_rx_via_queue.address == myAddress
      }  //  closing bracket for xQueueReceive( ..., &packet_rx_via_queue, ... )

# ifdef RESTART
      struct timeval now; gettimeofday ( &now, NULL );
      if ( now.tv_sec > last_received + 30 ) {
          data_exchange_packet_t packet_restart;
          packet_restart.to                = DE_Addr_DataAcquisition;
          packet_restart.from              = myAddress;
          packet_restart.type              = DE_Type_Command;
          packet_restart.data.Command.Code = CMD_DAQ_Start;
          packet_restart.data.Command.value.s64 = start_value;
          last_received = now.tv_sec;
      }
# endif


    } else {
      gTaskIsRunning = FALSE;
    }

    // Delay so other tasks can operate
    THIS_TASK_WILL_SLEEP( gHNV_StreamLogTask_Status );
    switch ( slg_state ) {
    case SLG_Idle:  vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_StreamLogTask_PERIOD_MS   ) ); break;
    case SLG_Rxing: vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_StreamLogTask_PERIOD_MS/5 ) ); break;
    }
    THIS_TASK_IS_RUNNING( gHNV_StreamLogTask_Status );
  }
}

//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

//*****************************************************************************
// Exported functions
//*****************************************************************************

//  Send a packet to the stream log task.
//  The stream log tasks expects only two types of packets:
//  (a) command packets
//  (b) data packets from data_acquisition and auxiliary_acquisition
//
void stream_log_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
    // io_out_string ( "DBG SLG push packet\r\n" );
  }
}

// Resume running the task
Bool stream_log_resumeTask(void)
{
  // Reset sync errors counter
//stream_output_resetSyncErrorCnt();
//stream_output_resetFrameCnt();

  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = TRUE;

  // Wait for task to start running
  while(!gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}


// Pause the stream log task
Bool stream_log_pauseTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = FALSE;

  // Wait for task to stop running
  while(gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}



// Allocate the stream log task and associated control mutex
Bool stream_log_createTask(void)
{
  static Bool gTaskCreated = FALSE;

  if(!gTaskCreated) {

    // Create task
    if ( pdPASS != xTaskCreate( stream_log_loop,
                  HNV_StreamLogTask_NAME, HNV_StreamLogTask_STACK_SIZE, NULL,
                  HNV_StreamLogTask_PRIORITY, &gHNV_StreamLogTask_Handler)) {
      return FALSE;
    }

    // Create control mutex
    if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) ) {
      return FALSE;
    }

    // Create message queue
    if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) ) {
      return FALSE;
    }

    //  Allocate memory for data received via packets
    received_data_package.mutex = NULL;  //  No need for mutex on this package, all handled in-task!
    received_data_package.state = EmptyRAM;
    received_data_package.address = (void*) pvPortMalloc ( sizeof(any_received_data_t) );
    if ( NULL == (received_data_package.address) ) {
      return FALSE;
    }
    received_data_type    = DE_Type_Nothing;

    //  Allocate memory for frame output to serial port and written to eMMC
# if 0
    slg_frame = (void*) pvPortMalloc ( sizeof(MAX_SLG_FRAME_LEN) );
    if ( NULL == ( slg_frame ) ) {
      return FALSE;
    }
# else
    {
        // Set flashc master type to last default to save one cycle for each branch.
        avr32_hmatrix_scfg_t scfg;

        scfg = AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_FLASH];
        scfg.defmstr_type = AVR32_HMATRIX_DEFMSTR_TYPE_LAST_DEFAULT;
        AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_FLASH] = scfg;

        scfg = AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_SRAM];
        scfg.defmstr_type = AVR32_HMATRIX_DEFMSTR_TYPE_LAST_DEFAULT;
        AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_SRAM] = scfg;

        scfg = AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_EMBEDDED_SYS_SRAM_0];
        scfg.defmstr_type = AVR32_HMATRIX_DEFMSTR_TYPE_LAST_DEFAULT;
        AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_EMBEDDED_SYS_SRAM_0] = scfg;

        scfg = AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_EMBEDDED_SYS_SRAM_1];
        scfg.defmstr_type = AVR32_HMATRIX_DEFMSTR_TYPE_LAST_DEFAULT;
        AVR32_HMATRIX.SCFG[AVR32_HMATRIX_SLAVE_EMBEDDED_SYS_SRAM_1] = scfg;
    }

    //slg_frame = 0xFF000000;  //  HRAMC0 - 32 kB Fast RAM via High Speed Bud
# endif

    gTaskCreated = TRUE;
    return TRUE;

  } else {

    return TRUE;
  }
}

# endif