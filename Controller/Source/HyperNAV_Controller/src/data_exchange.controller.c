/*! \file data_exchange_controller.c *******************************************************
 *
 * \brief Data Exchange - On Controller
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# include "data_exchange.controller.h"

# include <string.h>
# include <stdio.h>

# include "FreeRTOS.h"
# include "semphr.h"

# include "tasks.controller.h"
# include "data_exchange_packet.h"
# include "config_data.h"
# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"
# include "profile_packet.shared.h"
# include "sram_memory_map.controller.h"
# include "spi.controller.h"

# include "io_funcs.controller.h"
//# include "syslog.h"
# include "files.h"

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
# define N_RX_PACKETS 16
static xQueueHandle rxPackets = NULL;

typedef union {
  Spectrometer_Data_t          Spectrometer_Data;
  OCR_Data_t                            OCR_Data;
  MCOMS_Data_t                        MCOMS_Data;
  Profile_Info_Packet_t      Profile_Info_Packet;
  Profile_Data_Packet_t      Profile_Data_Packet;

} any_local_data_t;

# define N_DA_POINTERS 2  //  Must match sram layout!
static data_exchange_data_package_t local_data_package[N_DA_POINTERS] = { {0, 0, 0} };

//*****************************************************************************
// Local Functions
//*****************************************************************************

//! Transmits a packet to the slave via SPI
//! @param      header          Pointer to the packet header
//! @param      header_size     Header size in bytes
//! @param      data            Pointer to the packet data
//! @param      data_size       Data size in bytes
//! @return                     The number of header+data bytes transmitted
static int16_t send_via_SPI_RAM_RAM( uint8_t* header, int16_t header_size, uint8_t* data, int16_t data_size, int16_t* tx_size ) {

  return SPI_tx_packet( header, header_size, data, data_size, tx_size );
}

//! Transmits a packet to the slave via SPI
//! @param      header          Pointer to the packet header
//! @param      header_size     Header size in bytes
//! @param      data            Pointer to the packet data
//! @param      data_size       Data size in bytes
//! @return                     The number of header+data bytes transmitted
static int16_t send_via_SPI_RAM_SRAM( uint8_t* header, int16_t header_size, uint8_t* data, int16_t data_size, int16_t* tx_size ) {

  return SPI_tx_packet( header, header_size, data, data_size, tx_size );
}


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
static void data_exchange_controller( __attribute__((unused)) void* pvParameters ) {
# if 0
  int16_t stackWaterMark_threshold = 4096;
  int16_t stackWaterMark = uxTaskGetStackHighWaterMark(0);
  while ( stackWaterMark > stackWaterMark_threshold ) stackWaterMark_threshold/=2;
# endif
  data_exchange_address_t const myAddress = DE_Addr_ControllerBoard_DataExchange;

  for(;;) {

    if (gRunTask) {

      gTaskIsRunning = TRUE;

uint32_t gplp  = pm_read_gplp( &AVR32_PM, 1 );
         gplp &= 0x000FFFFF;
                 pm_write_gplp( &AVR32_PM, 1, gplp );

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

      //  The other board will indicate if it 
      //  has intention / no intention of sending something.
      //  If the other board wants to send,
      //  we will defer checking the in-queue for 'to-be-sent' packets,
      //  and will first service the request from the other board.
      if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {

        data_exchange_packet_t packet_rx_via_queue;

        if( pdPASS == xQueueReceive( rxPackets, &packet_rx_via_queue, 0 ) ) {

          if ( packet_rx_via_queue.to == myAddress ) {

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x00100000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

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

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x00200000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

          } else {  //  The final destination of this packet is another task. -- Pass it on.

# if 0
            //  Negotiate TX/RX
            //
            enum { NEG_Undefined, NEG_Can_Send, NEG_Will_Receive, NEG_Conflict, NEG_No_Response } negotiation = NEG_Undefined;

            //  Inform the other board that we intend to send a packet via SPI
            SPI_Set_MyBoard_Indicator( SPI_IND_Active );

            //  Wait some time (i.e., 20*HNV_DataXControlTask_PERIOD_MS/10 milliseconds
            //  until the other board confirms that it is responsive
            int wait = 50;
            while ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() && --wait>0 ) {
              io_out_string ( "Wait for Other Board Active\r\n" );
              vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataXControlTask_PERIOD_MS ) );
            }

            if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {

              negotiation = NEG_No_Response;

            } else {

# if 0
              switch ( SPI_Negotiate ( SPI_INT_WantToTransmit ) ) {
              case SPI_NEG_Conflict:    negotiation = NEG_Conflict; break;
              case SPI_NEG_CommFailure: negotiation = NEG_No_Response; break;
              case SPI_NEG_Agreed:      negotiation = NEG_Can_Send; break;
              default:                  negotiation = NEG_Undefined; break;
              }
# else
                                        negotiation = NEG_Can_Send;
# endif

            }

            if ( negotiation != NEG_Can_Send ) {

              //  The other board did not respond within the time period
              //  or the negotiation did not succees.
              //  ==> Abort the attempt to transfer
              //  and return the current packet to the queue
              //  for another attempt.  FIXME  If SPI communication
              //  continues to fail, the queue will block.
              xQueueSendToFront ( rxPackets, &packet_rx_via_queue, 0 );
              SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

              io_out_string ( "Negotiation failed\r\n" );

            } else {  //  NEG_Can_Send == negotiation

              io_out_string ( "Other Board Is Active\r\n" );
# endif
              int data_size = 0;
              int usePointer = 0;

              switch ( packet_rx_via_queue.type ) {

              case DE_Type_Nothing:              break;

              case DE_Type_Ping:                 data_size = sizeof(packet_rx_via_queue.data.Ping_Message); break;
              case DE_Type_Command:              data_size = sizeof(data_exchange_command_t);               break;
              case DE_Type_Response:             data_size = sizeof(data_exchange_respond_t);               break;
              case DE_Type_Syslog_Message:       data_size = sizeof(data_exchange_syslog_t);                break;

              case DE_Type_Configuration_Data:   data_size = sizeof(        Config_Data_t); usePointer = 1; break;
              case DE_Type_Spectrometer_Data:    data_size = sizeof(  Spectrometer_Data_t); usePointer = 1; break;
//            case DE_Type_OCR_Data:             data_size = sizeof(           OCR_Data_t); usePointer = 1; break;
              case DE_Type_OCR_Frame:            data_size =             OCR_FRAME_LENGTH ; usePointer = 1; break;
//            case DE_Type_MCOMS_Data:           data_size = sizeof(         MCOMS_Data_t); usePointer = 1; break;
              case DE_Type_MCOMS_Frame:          data_size =           MCOMS_FRAME_LENGTH ; usePointer = 1; break;
              case DE_Type_Profile_Info_Packet:  data_size = sizeof(Profile_Info_Packet_t); usePointer = 1; break;
              case DE_Type_Profile_Data_Packet:  data_size = sizeof(Profile_Data_Packet_t); usePointer = 1; break;

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

//  FIXME -- Artifical Delay to ensure the other side is ready to receive
//            vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataXControlTask_PERIOD_MS/5 ) );

              int16_t num_bytes_sent = 0;
              int16_t sent_status;

              if ( !usePointer ) {

                switch ( packet_rx_via_queue.type ) {

                case DE_Type_Ping:                sent_status = send_via_SPI_RAM_RAM ( packet_header, packet_header_size,
                                                                         (uint8_t*)packet_rx_via_queue.data.Ping_Message, data_size,
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Command:             sent_status = send_via_SPI_RAM_RAM ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Command), data_size,
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Response:            sent_status = send_via_SPI_RAM_RAM ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Response), data_size,
                                                                         &num_bytes_sent );
                                                  break;

                case DE_Type_Syslog_Message:      sent_status = send_via_SPI_RAM_RAM ( packet_header, packet_header_size,
                                                                         (uint8_t*)(&packet_rx_via_queue.data.Syslog), data_size,
                                                                         &num_bytes_sent );
                                                  break;

                default: sent_status = 0;
                                                  //  Handled via data_size > 0 in following else block
                                                  break;
                }

              } else {

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x00400000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

                //  Inside this code block,
                //  (1) the data pointed to by the received packet is sent via SPI
                //  (2) Then the received memory is released (flagged as Empty[S]RAM),
                //  TODO: If transfer fails?
                if ( pdTRUE == xSemaphoreTake ( packet_rx_via_queue.data.DataPackagePointer->mutex, portMAX_DELAY ) ) {

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x01000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

                  if ( packet_rx_via_queue.data.DataPackagePointer->state == FullRAM ) {
                    //  send data from RAM
                    sent_status = send_via_SPI_RAM_RAM ( packet_header, packet_header_size,
                                           packet_rx_via_queue.data.DataPackagePointer->address, data_size,
                                           &num_bytes_sent );
                    packet_rx_via_queue.data.DataPackagePointer->state = EmptyRAM;
                  } else if ( packet_rx_via_queue.data.DataPackagePointer->state == FullSRAM ) {
                    //  send data from SRAM
                    sent_status = send_via_SPI_RAM_SRAM ( packet_header, packet_header_size,
                                           packet_rx_via_queue.data.DataPackagePointer->address, data_size,
                                           &num_bytes_sent );
                    packet_rx_via_queue.data.DataPackagePointer->state = EmptySRAM;
                  }
gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x02000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

                  xSemaphoreGive ( packet_rx_via_queue.data.DataPackagePointer->mutex );
                }

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x00800000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

              }

              if(0) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_DXC_Sent;
                  packet_message.data.Syslog.value  = num_bytes_sent;
                  data_exchange_packet_router ( myAddress, &packet_message );
              }

              if(0) {
                  fHandler_t fh;
                  int ok;
                  char const* lfn = "0:\\DAT\\DXC.LOG";

                  if ( !f_exists(lfn) ) {
                    ok = ( FILE_OK == f_open( lfn, O_WRONLY | O_CREAT, &fh ) );
                  } else {
                    ok = ( FILE_OK == f_open( lfn, O_WRONLY | O_APPEND, &fh ) );
                  }

                  if ( ok ) {
                    char msg[64];

                    switch ( sent_status ) {
                    case -1: f_write( &fh, "TX NoCom\r\n", 10 ); break;
                    case -2: f_write( &fh, "TX Conflict\r\n", 13 ); break;
                    case -3: f_write( &fh, "TX Timeout\r\n", 12 ); break;
                    case -4: f_write( &fh, "TX Fail\r\n", 9 ); break;
                    case -5: f_write( &fh, "TX NoResp\r\n", 11 ); break;
                    case -6: f_write( &fh, "TX TooLarge\r\n", 13 ); break;
                    default: {
                               snprintf( msg, 64, "TX Sizes %02x %02x %02x %hd\r\n",
                                                   packet_rx_via_queue.type & 0xFF,
                                                   packet_rx_via_queue.to   & 0xFF,
                                                   packet_rx_via_queue.from & 0xFF,
                                                   num_bytes_sent );
                                f_write ( &fh, msg, strlen(msg) );
                             }
                    }
                    f_close( &fh );
                  }
              }

              if (0) {
              switch ( sent_status ) {
              case -1:  io_out_string ( "TX NoCom\r\n" ); break;
              case -2:  io_out_string ( "TX Conflict\r\n" ); break;
              case -3:  io_out_string ( "TX Timeout\r\n" ); break;
              case -4:  io_out_string ( "TX Fail\r\n" ); break;
              case -5:  io_out_string ( "TX NoResp\r\n" ); break;
              default:  if ( num_bytes_sent != packet_header_size+data_size ) {
                        io_out_string ( "TX SzErr\r\n" );
                        } else {
                        io_out_string ( "TX SzOk\r\n" );
                        }
              }
              }

# if 0
              //  Tell the other board that all is done.
              //  This should already have been done inside SPI_tx_packet().

              SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

              wait = 50;
              while ( SPI_IND_Active == SPI_Get_OtherBoard_Indicator() && --wait>0 ) {
                io_out_string ( "Wait for Other Board to turn off\r\n" );
                vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataXControlTask_PERIOD_MS ) );
              }
            }
# endif
          }
        }

             //  Check for incoming packet via SPI
      } /* else */ if ( SPI_IND_Active == SPI_Get_OtherBoard_Indicator() ) {

# if 0
        //SPI_Set_MyBoard_Indicator( SPI_IND_Active );
        //vTaskDelay( (portTickType)TASK_DELAY_MS( 1 ) );

        if ( 0 && SPI_NEG_Agreed != SPI_Negotiate ( SPI_INT_ReadyToReceive ) ) {
          
          //  This should be impossible to occur.
          SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

        } else {
# endif
          data_exchange_packet_t packet_rx_via_SPI;

          uint16_t const packet_header_size = sizeof(data_exchange_address_t)
                                            + sizeof(data_exchange_address_t)
                                            + sizeof(data_exchange_type_t);
          uint8_t  packet_header[ packet_header_size ];
          int16_t received_size;
          int16_t received_status;

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x10000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

          int data_copied = 0;
          int i;
          for ( i=0; i<N_DA_POINTERS && !data_copied; i++) {
            if ( pdTRUE == xSemaphoreTake ( local_data_package[i].mutex, 100 /*portMAX_DELAY*/ ) ) {
              if ( local_data_package[i].state == EmptySRAM ) {

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x04000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );
                received_status = SPI_rx_packet ( packet_header, packet_header_size,
                                                     local_data_package[i].address,
                                                     sizeof(any_local_data_t),
                                                     &received_size );
gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x08000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

                int32_t remaining_data_size;

                if ( received_size > packet_header_size ) {
                  memcpy ( &(packet_rx_via_SPI.to  ), packet_header                                  , sizeof(data_exchange_address_t) );
                  memcpy ( &(packet_rx_via_SPI.from), packet_header+  sizeof(data_exchange_address_t), sizeof(data_exchange_address_t) );
                  memcpy ( &(packet_rx_via_SPI.type), packet_header+2*sizeof(data_exchange_address_t), sizeof(data_exchange_type_t)    );

                  remaining_data_size = received_size-packet_header_size;
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
                                                   if ( value_size <= remaining_data_size ) {
                                                     memcpy ( &packet_rx_via_SPI.data.Ping_Message, local_data_package[i].address, value_size );
                                                   }
                                                   break;

                case DE_Type_Command:              value_size = sizeof(data_exchange_command_t);
                                                   if ( value_size <= remaining_data_size ) {
                                                     memcpy ( &packet_rx_via_SPI.data.Command, local_data_package[i].address, value_size );
                                                   }
                                                   break;

                case DE_Type_Response:             value_size = sizeof(data_exchange_respond_t);
                                                   if ( value_size <= remaining_data_size ) {
                                                     memcpy ( &packet_rx_via_SPI.data.Response, local_data_package[i].address, value_size );
                                                   }
                                                   break;

                case DE_Type_Syslog_Message:       value_size = sizeof(data_exchange_syslog_t);
                                                   if ( value_size <= remaining_data_size ) {
                                                     memcpy ( &packet_rx_via_SPI.data.Syslog, local_data_package[i].address, value_size );
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
                  if ( data_size <= remaining_data_size ) {
                    packet_rx_via_SPI.data.DataPackagePointer = &local_data_package[i];
                    local_data_package[i].state = FullSRAM;
                  }
                }


                if(0) {
                  data_exchange_packet_t packet_message;
                  packet_message.to   = DE_Addr_ControllerBoard_Commander;
                  packet_message.from = myAddress;
                  packet_message.type = DE_Type_Syslog_Message;
                  packet_message.data.Syslog.number = SL_MSG_DXC_Received;
                  packet_message.data.Syslog.value  = received_size;
                  data_exchange_packet_router ( myAddress, &packet_message );
                }

                if(0) {
                  fHandler_t fh;
                  int ok;
                  char const* lfn = "0:\\DAT\\DXC.LOG";

                  if ( !f_exists(lfn) ) {
                    ok = ( FILE_OK == f_open( lfn, O_WRONLY | O_CREAT, &fh ) );
                  } else {
                    ok = ( FILE_OK == f_open( lfn, O_WRONLY | O_APPEND, &fh ) );
                  }

                  if ( ok ) {
                    char msg[64];

                    switch ( received_status ) {
                    case  0: f_write( &fh, "RX NoData\r\n", 11 ); break;
                    case -1: f_write( &fh, "RX NoCom\r\n", 10 ); break;
                    case -2: f_write( &fh, "RX Conflict\r\n", 13 ); break;
                    case -3: f_write( &fh, "RX Timeout\r\n", 12 ); break;
                    case -4: f_write( &fh, "RX Fail\r\n", 9 ); break;
                    case -5: f_write( &fh, "RX NoResp\r\n", 11 ); break;
                    case -6: f_write( &fh, "RX TooLarge\r\n", 13 ); break;
                    default: {
                               snprintf( msg, 64, "RX Sizes %02x %02x %02x %hd %hu %hu\r\n",
                                                   packet_rx_via_SPI.type & 0xFF,
                                                   packet_rx_via_SPI.to   & 0xFF,
                                                   packet_rx_via_SPI.from & 0xFF,
                                                   received_size,
                                                   value_size,
                                                   data_size );
                                f_write ( &fh, msg, strlen(msg) );
                             }
                    }
                    f_close( &fh );
                  }
                }

                if (0) {
                switch ( received_status ) {
                case -1:  io_out_string ( "RX NoCom\r\n" ); break;
                case -2:  io_out_string ( "RX Conflict\r\n" ); break;
                case -3:  io_out_string ( "RX Timeout\r\n" ); break;
                case -4:  io_out_string ( "RX Fail\r\n" ); break;
                case -5:  io_out_string ( "RX NoResp\r\n" ); break;
                default:  if ( value_size && remaining_data_size<value_size ) {

                            char msg[64];
                            snprintf( msg, 64, "RX VSzErr %hx %02x %02x %02x\r\n",
                                               received_size,
                            packet_rx_via_SPI.type & 0xFF,
                            packet_rx_via_SPI.to   & 0xFF,
                            packet_rx_via_SPI.from & 0xFF );
                            io_out_string ( msg );

                            packet_rx_via_SPI.to = DE_Addr_Nobody;

                          } else if ( data_size && remaining_data_size<data_size ) {

                            io_out_S32 ( "RX DSzErr %ld\r\n", (S32)received_size );

                            packet_rx_via_SPI.to = DE_Addr_Nobody;

                          } else if ( !value_size && !data_size ) {

                            io_out_S32 ( "RX ?SzErr %ld\r\n", (S32)received_size );

                            packet_rx_via_SPI.to = DE_Addr_Nobody;
                          }
                }
                }

                data_copied = 1;
              }
              xSemaphoreGive( local_data_package[i].mutex );
            }
          }

gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x20000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

# if 0
          if ( !data_copied ) {
                          io_out_string ( "RX NotTried\r\n" );
          }
# endif

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

              //syslog_out ( SYSLOG_ERROR, "data_exchange_controller", "Received nonPING/QUERY packet" );

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
# if 0
        }
# endif
gplp  = pm_read_gplp( &AVR32_PM, 1 );
gplp |= 0x40000000;
        pm_write_gplp( &AVR32_PM, 1, gplp );

      }

    } else {

      gTaskIsRunning = FALSE;
    }

    // Sleep
    gHNV_DataXControlTask_Status = TASK_SLEEPING;
    vTaskDelay( (portTickType)TASK_DELAY_MS( HNV_DataXControlTask_PERIOD_MS ) );
    gHNV_DataXControlTask_Status = TASK_RUNNING;
  }

}

//*****************************************************************************
// Exported functions
//*****************************************************************************

void data_exchange_controller_pushPacket( data_exchange_packet_t* packet ) {
  //  Multiple threads may call concurrently.
  //  We trust that the queue is implemented thread-safe.
  if ( packet ) {
    // io_out_string ( "DBG DXC push packet\r\n" );
    xQueueSendToBack ( rxPackets, packet, 0 );
  }
}

// Resume this task
Bool data_exchange_controller_resumeTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Potentially do setup here

  // Command
  gRunTask = TRUE;

  // Wait for task to start running
  while(!gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}


// Pause this task
Bool data_exchange_controller_pauseTask(void)
{
  // Grab control mutex
  xSemaphoreTake(gTaskCtrlMutex, portMAX_DELAY);

  // Command
  gRunTask = FALSE;

  // Wait for task to stop running
  while(gTaskIsRunning) vTaskDelay( (portTickType)TASK_DELAY_MS(10) );

  // Potentially do cleanup here

  // Return control mutex
  xSemaphoreGive(gTaskCtrlMutex);

  return TRUE;
}



// Allocate this task and associated control mutex
Bool data_exchange_controller_createTask(void)
{
  static Bool gTaskCreated = FALSE;

  Bool res = FALSE;

  if(!gTaskCreated)
  {
    // Start bail-out block
    do {
      // Create task
      if ( pdPASS != xTaskCreate( data_exchange_controller,
                HNV_DataXControlTask_NAME, HNV_DataXControlTask_STACK_SIZE, NULL,
                HNV_DataXControlTask_PRIORITY, &gHNV_DataXControlTask_Handler))
        break;

      // Create control mutex
      if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) )
        break;

      // Create / allocate / initialize other locally needed resources here

      // Create receive message queue
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
          local_data_package[i].address = sram_DXC_1;
        } else if ( 1 == i ) {
          local_data_package[i].address = sram_DXC_2;
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

      res = TRUE;
      gTaskCreated = TRUE;

    } while (0);
  
  } else {
    res = TRUE;
  }

  return res;
}
