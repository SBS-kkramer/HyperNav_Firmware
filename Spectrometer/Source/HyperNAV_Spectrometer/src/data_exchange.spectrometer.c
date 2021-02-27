/*! \file data_exchange_spectrometer.c *******************************************************
 *
 * \brief Data Exchange - On Spectrometer - Acronym DXS
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# include "data_exchange.spectrometer.h"

# include <string.h>

# include "FreeRTOS.h"
# include "semphr.h"

//# include "power_clocks_lib.h"
# include "tasks.spectrometer.h"
# include "data_exchange_packet.h"
# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"
# include "config_data.h"
# include "profile_packet.shared.h"
# include "spi.spectrometer.h"

# include "sram_memory_map.spectrometer.h"

# include "io_funcs.spectrometer.h"
# include "gplp_trace.spectrometer.h"

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

typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Data_t                            OCR_Data;
  MCOMS_Data_t                        MCOMS_Data;
  Profile_Info_Packet_t      Profile_Info_Packet;
  Profile_Data_Packet_t      Profile_Data_Packet;

} any_local_data_t;

# define N_DA_POINTERS 2
static data_exchange_data_package_t local_data_package[N_DA_POINTERS] = { { 0 } };

//*****************************************************************************
// Local Functions
//*****************************************************************************

//*****************************************************************************
// Local Tasks Implementation
//*****************************************************************************

// Data exchange task
//
// Loop forever
//   (1) Check for packet received from an on-this-board task
//       and pass it via SPI to other board
//   (2) Check for receive request from other-board
//       and build a packet from received data
//       and pass it to recipient on-this-board task
//   (3) Delay a bit to give other tasks a chance to complete their work
//
static void data_exchange_spectrometer( void* pvParameters ) {

# if 0 // def WATERMARKING
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif

  data_exchange_address_t const myAddress = DE_Addr_SpectrometerBoard_DataExchange;

  for(;;)
  {
    if(gRunTask) {

      gTaskIsRunning = true;

uint32_t gplp1  = pcl_read_gplp(1);
         gplp1 &= 0x000FFFFF;
                 pcl_write_gplp( 1, gplp1 );
//io_out_string( "*" );
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

      //  The other board will indicate if it 
      //  has intention / no intention of sending something.
      //  If the other board wants to send,
      //  we will defer checking the in-queue for 'to-be-sent' packets,
      //  and will first service the request from the other board.
      if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {

        data_exchange_packet_t packet_rx_via_queue;

//io_out_string ( "DXS P\r\n" );

        if ( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) ) {

//io_out_string ( "DXS PQ\r\n" );

          if ( packet_rx_via_queue.to == myAddress ) {

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x00100000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "r" );

            //  Only handle Ping, ignore all others.

            if ( packet_rx_via_queue.type == DE_Type_Ping ) {

              data_exchange_packet_t packet_response;

              packet_response.to = packet_rx_via_queue.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_ALL_Ping;
              packet_response.data.Response.value.u64 = 0;  //  Will be ignored

              data_exchange_packet_router ( myAddress, &packet_response );

            } else {

# if 0
              //  This is not supposed to happen
              //
              data_exchange_packet_t packet_error;

              packet_error.to   = DE_Addr_ControllerBoard_Commander;
              packet_error.from = myAddress;
              packet_error.type = DE_Type_Syslog_Message;
              packet_error.data.Syslog.number = SL_ERR_RxNonPingPacket;
              packet_error.data.Syslog.value  = 0;

              data_exchange_spectrometer_pushPacket_viaSPI ( &packet_error );
# endif

              //  Mark data as free to prevent lockup.
              //
              switch ( packet_rx_via_queue.type ) {

              case DE_Type_Configuration_Data:
              case DE_Type_Spectrometer_Data:
//            case DE_Type_OCR_Data:
              case DE_Type_OCR_Frame:
//            case DE_Type_MCOMS_Data:
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

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x00200000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "R" );

          } else {  //  The final destination of this packet is another task. -- Pass it on.

              int data_size = 0;
              int usePointer = 0;

              switch ( packet_rx_via_queue.type ) {

              case DE_Type_Ping:                 data_size = sizeof(packet_rx_via_queue.data.Ping_Message); break;
              case DE_Type_Command:              data_size = sizeof(data_exchange_command_t);               break;
              case DE_Type_Response:             data_size = sizeof(data_exchange_respond_t);               break;
              case DE_Type_Syslog_Message:       data_size = sizeof(data_exchange_syslog_t);                break;

              case DE_Type_Configuration_Data:   data_size = sizeof(        Config_Data_t); usePointer = 1; break;
              case DE_Type_Spectrometer_Data:    data_size = sizeof(  Spectrometer_Data_t); usePointer = 1; break;
              case DE_Type_OCR_Frame:            data_size =             OCR_FRAME_LENGTH ; usePointer = 1; break;
              case DE_Type_MCOMS_Frame:          data_size =           MCOMS_FRAME_LENGTH ; usePointer = 1; break;
              case DE_Type_Profile_Info_Packet:  data_size = sizeof(Profile_Info_Packet_t); usePointer = 1; break;
              case DE_Type_Profile_Data_Packet:  data_size = sizeof(Profile_Data_Packet_t); usePointer = 1; break;

              case DE_Type_Nothing:              break;
              //  Do NOT add a "default:" here.
              //  That way, the compiler will generate a warning
              //  if one possible packet type was missed.

              }

              uint16_t const packet_header_size = sizeof(data_exchange_address_t)
                                                + sizeof(data_exchange_address_t)
                                                + sizeof(data_exchange_type_t);
              uint8_t  packet_header[ packet_header_size ];

              //  Packet header as bytes

              memcpy ( packet_header,                                   &(packet_rx_via_queue.to  ), sizeof(data_exchange_address_t) );
              memcpy ( packet_header+  sizeof(data_exchange_address_t), &(packet_rx_via_queue.from), sizeof(data_exchange_address_t) );
              memcpy ( packet_header+2*sizeof(data_exchange_address_t), &(packet_rx_via_queue.type), sizeof(data_exchange_type_t)    );

              int16_t num_bytes_sent = 0;
              int16_t sent_status = 0;

              if ( !usePointer ) {


                switch ( packet_rx_via_queue.type ) {

                case DE_Type_Ping:                sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                                                         (uint8_t*)packet_rx_via_queue.data.Ping_Message, sizeof(packet_rx_via_queue.data.Ping_Message),
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Command:             sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Command), sizeof(data_exchange_command_t),
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Response:            sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Response), sizeof(data_exchange_respond_t),
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Syslog_Message:      sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Syslog), sizeof(data_exchange_syslog_t),
                                                                         &num_bytes_sent );
                                                  break;

                default:
                                                  //  Handled via data_size > 0 in following else block
                                                  break;
                }

              } else {

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x00400000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "s" );

                //  Inside this code block,
                //  (1) the data pointed to by the received packet is pused to SPI
                //  (2) Then the received memory is released (flagged as Empty[S]RAM),

                //  Mutex control access to received memory
                //  (1) send to SPI
                //  (2) flag reseived as Empty
                //
                if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x01000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "u" );

                  if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                    //  send data from RAM
                    sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                           packet_rx_via_queue.data.DataPackagePointer->address, data_size,
                                           &num_bytes_sent );
                    packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                  } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                    //  send data from SRAM
                    sent_status = SPI_tx_packet ( packet_header, packet_header_size,
                                           packet_rx_via_queue.data.DataPackagePointer->address, data_size,
                                           &num_bytes_sent );
                    packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                  }

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x02000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "U" );

                  xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );

                  if(0) io_out_S32 ( "TXF %lx\r\n", (S32)(packet_rx_via_queue.data.DataPackagePointer->address) );

                  if ( sent_status ) {
                    io_out_S32 ( "TX %ld ", (S32)sent_status );
                    io_out_S32 ( "%ld ", (S32)num_bytes_sent );
                    io_out_S32 ( "%ld\r\n", (S32)(packet_header_size+data_size) );
                  }

                } else {
                  io_out_string ( "TX Not Attempted, Mutex Locked\r\n" );
                  //  TODO  --  Push back onto queue, to eventually have the memory made empty
                }
gplp1  = pcl_read_gplp(1);
gplp1 |= 0x00800000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "S" );

              }
//io_out_string ( "DXS Td\r\n" );
          }

          vTaskDelay( (portTickType)TASK_DELAY_MS( 25 ) );
        } // End of while loop

      //  Check for incoming packet via SPI
      } /* else */ if ( SPI_IND_Active == SPI_Get_OtherBoard_Indicator() ) {

          data_exchange_packet_t packet_rx_via_SPI;

          uint16_t const packet_header_size = sizeof(data_exchange_address_t)
                                            + sizeof(data_exchange_address_t)
                                            + sizeof(data_exchange_type_t);
          uint8_t  packet_header[ packet_header_size ];

          int32_t received_data_size;

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x10000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "a" );

          int data_copied = 0;
          int i;
          for ( i=0; i<N_DA_POINTERS && !data_copied; i++) {
            if ( pdTRUE == xSemaphoreTake ( local_data_package[i].mutex, portMAX_DELAY /*10*/ ) ) {

              if ( local_data_package[i].state == EmptySRAM ) {

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x04000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "b" );

                received_data_size = SPI_rx_packet( packet_header, packet_header_size,
                                                    (uint8_t*)0, local_data_package[i].address,
                                                    sizeof(any_local_data_t) );
gplp1  = pcl_read_gplp(1);
gplp1 |= 0x08000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "B" );


                int32_t remaining_data_size;

                if ( received_data_size > packet_header_size ) {
                  memcpy ( &packet_rx_via_SPI.to  , packet_header                                  , sizeof(data_exchange_address_t) );
                  memcpy ( &packet_rx_via_SPI.from, packet_header+  sizeof(data_exchange_address_t), sizeof(data_exchange_address_t) );
                  memcpy ( &packet_rx_via_SPI.type, packet_header+2*sizeof(data_exchange_address_t), sizeof(data_exchange_type_t)    );

                  remaining_data_size = received_data_size-packet_header_size;
                } else {
                  remaining_data_size = 0;
                  packet_rx_via_SPI.type = DE_Type_Nothing;
                  packet_rx_via_SPI.to   = DE_Addr_Nobody;
                  packet_rx_via_SPI.from = DE_Addr_Nobody;
                }

                uint16_t value_size = 0;
                uint16_t data_size = 0;

                switch ( packet_rx_via_SPI.type ) {

                case DE_Type_Ping:                 value_size = sizeof(packet_rx_via_SPI.data.Ping_Message);
                                                   if ( value_size == remaining_data_size ) {
                                                     copy_to_ram_from_sram ( (uint8_t*)&packet_rx_via_SPI.data.Ping_Message, local_data_package[i].address, value_size );
						     
                                                   } else {
                                                     packet_rx_via_SPI.to = DE_Addr_Nobody;
                                                   }
                                                   break;

                case DE_Type_Command:              value_size = sizeof(data_exchange_command_t);
                                                   if ( value_size == remaining_data_size ) {
                                                     copy_to_ram_from_sram ( (uint8_t*)&packet_rx_via_SPI.data.Command, local_data_package[i].address, value_size );
                                                   } else {
                                                     packet_rx_via_SPI.to = DE_Addr_Nobody;
                                                   }
                                                   break;

                case DE_Type_Response:             value_size = sizeof(data_exchange_respond_t);
                                                   if ( value_size == remaining_data_size ) {
                                                     copy_to_ram_from_sram ( (uint8_t*)&packet_rx_via_SPI.data.Response, local_data_package[i].address, value_size );
                                                   } else {
                                                     packet_rx_via_SPI.to = DE_Addr_Nobody;
                                                   }
                                                   break;

                case DE_Type_Syslog_Message:       value_size = sizeof(data_exchange_syslog_t);
                                                   if ( value_size == remaining_data_size ) {
                                                     copy_to_ram_from_sram ( (uint8_t*)&packet_rx_via_SPI.data.Syslog, local_data_package[i].address, value_size );
                                                   } else {
                                                     packet_rx_via_SPI.to = DE_Addr_Nobody;
                                                   }
                                                   break;

                case DE_Type_Configuration_Data:   data_size = sizeof(        Config_Data_t); break;
                case DE_Type_Spectrometer_Data:    data_size = sizeof(  Spectrometer_Data_t); break;
//              case DE_Type_OCR_Data:             data_size = sizeof(           OCR_Data_t); break;
                case DE_Type_OCR_Frame:            data_size =             OCR_FRAME_LENGTH ; break;
//              case DE_Type_MCOMS_Data:           data_size = sizeof(         MCOMS_Data_t); break;
                case DE_Type_MCOMS_Frame:          data_size =           MCOMS_FRAME_LENGTH ; break;
                case DE_Type_Profile_Info_Packet:  data_size = sizeof(Profile_Info_Packet_t); break;
                case DE_Type_Profile_Data_Packet:  data_size = sizeof(Profile_Data_Packet_t); break;

                case DE_Type_Nothing:              // when error in SPI_rx_packet() occurred
                default:                           /*  Should never happen -- Trigger ERROR message -- TODO??? */
                                                   break;
                }

                if ( data_size > 0 ) {
                  if ( data_size == remaining_data_size ) {
                    packet_rx_via_SPI.data.DataPackagePointer = &local_data_package[i];
                    local_data_package[i].state = FullSRAM;
                  } else {
                    //  data size mismatch!
                    packet_rx_via_SPI.to = DE_Addr_Nobody;
                  }
                }

                switch ( received_data_size ) {
                case -1:  io_out_string ( "RX NoCom\r\n" ); break;
                case -2:  io_out_string ( "RX Conflict\r\n" ); break;
                case -3:  io_out_string ( "RX Timeout\r\n" ); break;
                case -4:  io_out_string ( "RX Fail\r\n" ); break;
                case -5:  io_out_string ( "RX NoResp\r\n" ); break;
                default:  if ( ( data_size && remaining_data_size == data_size )
                            || ( value_size && remaining_data_size == value_size ) ) {
                        //io_out_string ( "RX Ok\r\n" );
                          } else {
                          io_out_string ( "RX Error\r\n" );
                          }
                }

                data_copied = 1;
              }
              xSemaphoreGive( local_data_package[i].mutex );

            }
          }

gplp1  = pcl_read_gplp(1);
gplp1 |= 0x20000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "A" );

          if ( !data_copied ) {
                          io_out_string ( "RX NotTried\r\n" );
          }

           //  Send the just received packet on its way
           //  or respond to it if addressed to myself
           //
           if ( packet_rx_via_SPI.to == myAddress ) {

            //  Only expecting Ping, ignore all others (that were sent in error!).

            if ( packet_rx_via_SPI.type == DE_Type_Ping ) {

              data_exchange_packet_t packet_response;

              packet_response.to = packet_rx_via_SPI.from;
              packet_response.from = myAddress;
              packet_response.type = DE_Type_Response;
              packet_response.data.Response.Code = RSP_ALL_Ping;
              packet_response.data.Response.value.u64 = 0;  //  Will be ignored

              data_exchange_packet_router ( myAddress, &packet_response );

            } else {

              //  This is not supposed to happen.
              //  If received a packet containing data, clean data,
              //  ie., throw out, and mark location as empty, ie. reusable
              //
              switch ( packet_rx_via_SPI.type ) {

              case DE_Type_Configuration_Data:
              case DE_Type_Spectrometer_Data:
//            case DE_Type_OCR_Data:
              case DE_Type_OCR_Frame:
//            case DE_Type_MCOMS_Data:
              case DE_Type_MCOMS_Frame:
              case DE_Type_Profile_Info_Packet:
              case DE_Type_Profile_Data_Packet:
                if ( pdTRUE == xSemaphoreTake ( packet_rx_via_SPI.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {
                  if ( packet_rx_via_SPI.data.DataPackagePointer->state == FullRAM ) {
                    packet_rx_via_SPI.data.DataPackagePointer->state = EmptyRAM;
                  } else if ( packet_rx_via_SPI.data.DataPackagePointer->state == FullSRAM ) {
                    packet_rx_via_SPI.data.DataPackagePointer->state = EmptySRAM;
                  }
                  xSemaphoreGive ( packet_rx_via_SPI.data.DataPackagePointer->mutex );
                }
              break;

              default:
                //  No action required. Packet has been consumed without a trace.
              break;
              }

            }
           } else if ( packet_rx_via_SPI.to != DE_Addr_Nobody ) {
            data_exchange_packet_router ( myAddress, &packet_rx_via_SPI );
           }
gplp1  = pcl_read_gplp(1);
gplp1 |= 0x40000000;
         pcl_write_gplp( 1, gplp1 );
//io_out_string( "C" );

      }

    } else {

      gTaskIsRunning = false;
    }

    gHNV_DataXSpectrmTask_Status = TASK_SLEEPING;   //  Notify task monitor to not trigger a watchdog reset on behalf of this task
    //  Allow scheduler to run other tasks
    vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataXSpectrmTask_PERIOD_MS ) );
    gHNV_DataXSpectrmTask_Status = TASK_RUNNING;    //  Notify task monitor to trigger a watchdog reset if this task becomes unresponsive
  }

}

//*****************************************************************************
// Exported functions
//*****************************************************************************

//  This function allows other tasks to push a data_exchange_packet_t to this task.
void data_exchange_spectrometer_pushPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

// Resume running the task
Bool data_exchange_spectrometer_resumeTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Potentially do setup here

  // Command
  gRunTask = true;

  // Wait for task to start running
  while(!gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return true;
}


// Pause the Data acquisition task
Bool data_exchange_spectrometer_pauseTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = false;

  // Wait for task to stop running
  while(gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Potentially do cleanup here

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return true;
}



// Allocate the Data acquisition task and associated control mutex
Bool data_exchange_spectrometer_createTask(void)
{
  static Bool gTaskCreated = false;

  Bool res = false;

  if(!gTaskCreated)
  {
    // Start bail-out block
    do {
      // Create task
      if ( pdPASS != xTaskCreate( data_exchange_spectrometer,
                HNV_DataXSpectrmTask_NAME, HNV_DataXSpectrmTask_STACK_SIZE, NULL,
                HNV_DataXSpectrmTask_PRIORITY, &gHNV_DataXSpectrmTask_Handler))
        break;

      // Create control mutex
      if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) )
        break;

      // Create / allocate / initialize other locally needed resources here

      // Create message queue
      if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) )
        break;

      // Allocate memory for frames that are received
      //
      bool all_allocated = true;
      int i;
      for ( i=0; i<N_DA_POINTERS; i++) {
        local_data_package[i].mutex = xSemaphoreCreateMutex();
        if ( NULL == local_data_package[i].mutex ) {
          all_allocated = false;
        }

        local_data_package[i].state = EmptySRAM;

        if ( 0 == i ) {
          local_data_package[0].address = sram_DXS_1;
        } else if ( 1 == i )  {
          local_data_package[1].address = sram_DXS_2;
        } else {
          all_allocated = false;
        }
      }

      if ( !all_allocated ) {
        for ( i=0; i<N_DA_POINTERS; i++) {
          if ( local_data_package[i].mutex ) {
            //  This is strangely implemented by Atmel:
            //  There is no direct function to delete a mutex.
            //  But a mutex is implemented as a queue,
            //  hence deleting the underlying queue deletes the mutex.
            vQueueDelete(local_data_package[i].mutex);
            local_data_package[i].mutex = NULL;
          }
        }

        break;  //  To return FALSE
      }

      //  All done

      res = true;
      gTaskCreated = true;

    } while (0);

  } else {
    res = true;
  }

  return res;
}
