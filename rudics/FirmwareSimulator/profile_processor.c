# include "profile_processor.h"
# include "tasks.spectrometer.h"

# include <string.h>

# include "modem.h"
# include "network.h"
# include "syslog.h"	//  TODO -- Convert into packet messaging

//  Create/pause/resume control
//
static int8_t runTask = 0;
static int8_t taskIsRunning = 0;


# ifdef FW_SIMULATION
#   include <unistd.h>
#   define portTickType useconds_t
#   define TASK_DELAY_MS(M) (M)
#   define vTaskDelay(X) usleep(X*1000)
#   include <stdlib.h>
#   define pvPortMalloc malloc
#   define vPortFree free
# endif

# ifdef FW_SIMULATION
#   include <pthread.h>
    static pthread_mutex_t taskCtrlMutex;
# else
    static xSemaphoreHandle taskCtrlMutex = NULL;
# endif

// packet receiving queue
//
# define N_RX_PACKETS 16

# ifdef FW_SIMULATION
    static pthread_mutex_t circbufMutex;
    typedef struct circbuf {
      data_exchange_packet_t content[N_RX_PACKETS];
      int16_t start;
      int16_t count;
    } circbuf_t;
    static circbuf_t rxPackets;
# else
    static xQueueHandle rxPackets = NULL;
# endif


// Data management
// Note: memory addresses
//     local_data[i]
//   &(local_data[i]->spectrometer_data)
//   &(local_data[i]->fake)
// are identical.
//
# define N_DA_POINTERS 1
typedef union {
  HyperNav_Spectrometer_Data_t spectrometer_data;
  int fake;
} local_data_t;

static local_data_t* local_data[N_DA_POINTERS];
static int8_t can_use_local_data[N_DA_POINTERS];

// API and local (static) functions

# ifdef FW_SIMULATION
static void profile_processor_initCircbuf() {
  rxPackets.start = 0;
  rxPackets.count = 0;
}
# endif

//  Enqueues a packet onto the queue
//
int16_t profile_processor_sendPacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  if ( !packet ) {

    return -1;                                 //  return 'fail, null pointer packet'

  } else {

# ifdef FW_SIMULATION

    pthread_mutex_lock( &circbufMutex );

    if ( N_RX_PACKETS ==rxPackets.count ) {    //  buffer full, do not insert

      pthread_mutex_unlock( &circbufMutex );
      return 1;                                //  return 'fail, is full'

    } else {                                   //  insert at next available location

      memcpy( &(rxPackets.content[ (rxPackets.start+rxPackets.count) % N_RX_PACKETS ]),
              packet, sizeof(data_exchange_packet_t) );
      rxPackets.count++;

      pthread_mutex_unlock( &circbufMutex );
      return 0;                                //  return 'ok, is inserted'

    }
  
# else
    //  We trust that the queue is implemented thread-safe.
    //
    xQueueSendToBack ( rxPackets, packet, 0 );
    return 0;
# endif
  }
}

//  get the oldest packet from the queue
//
//  Return 0  ok, got a packet
//         1  no packet available
//        -1  argument error
//
static int16_t profile_processor_dequeuePacket( data_exchange_packet_t* packet ) {
  //  !!! This function MUST remain thread-safe !!!
  //  Multiple threads may call concurrently.
  if ( !packet ) {

    return -1;                                 //  return 'fail, null pointer packet'

  } else {

# ifdef FW_SIMULATION

    pthread_mutex_lock( &circbufMutex );

    if ( 0 == rxPackets.count ) {              //  buffer empty, nothing to return

      pthread_mutex_unlock( &circbufMutex );
      return 1;                                //  return 'fail, is empty'

    } else {                                   //  insert at next available location

      memcpy( packet, &(rxPackets.content[ rxPackets.start ]), sizeof(data_exchange_packet_t) );
      rxPackets.start = (rxPackets.start+1) % N_RX_PACKETS;
      rxPackets.count--;

      pthread_mutex_unlock( &circbufMutex );
      return 0;                                //  return 'ok, packet is returned'

    }
  
# else
    //  We trust that the queue is implemented thread-safe.
    //
    if ( pdPASS == xQueueReceive( rxPackets, packet, 0 ) ) {
      return 0;                                //  return 'ok, packet is returned'
    } else {
      return 1;                                //  return 'fail, is empty'
    }
# endif
  }
}

void profile_processor_pauseTask ( void ) {
# ifdef FW_SIMULATION

  // Grab control mutex
  pthread_mutex_lock( &taskCtrlMutex );

  // Command
  runTask = 0;

  // Wait for task to start running
  while( taskIsRunning ) vTaskDelay( (portTickType)(TASK_DELAY_MS(10)) );

  // Return control mutex
  pthread_mutex_unlock( &taskCtrlMutex );

# else

  // Grab control mutex
  xSemaphoreTake(taskCtrlMutex, portMAX_DELAY);

  // Command
  runTask = 0;

  // Wait for task to start running
  while( taskIsRunning ) vTaskDelay( (portTickType)(TASK_DELAY_MS(10)) );

  // Return control mutex
  xSemaphoreGive(taskCtrlMutex);

# endif

  return;
}
void profile_processor_resumeTask( void ) {
# ifdef FW_SIMULATION

  // Grab control mutex
  pthread_mutex_lock( &taskCtrlMutex );

  // Command
  runTask = 1;

  // Wait for task to start running
  while( !taskIsRunning ) vTaskDelay( (portTickType)(TASK_DELAY_MS(10)) );

  // Return control mutex
  pthread_mutex_unlock( &taskCtrlMutex );

# else

  // Grab control mutex
  xSemaphoreTake(taskCtrlMutex, portMAX_DELAY);

  // Command
  runTask = 1;

  // Wait for task to start running
  while( !taskIsRunning ) vTaskDelay( (portTickType)(TASK_DELAY_MS(10)) );

  // Return control mutex
  xSemaphoreGive(taskCtrlMutex);

# endif

  return;
}


# if 0
static int DataExchange()
{
   /* function name for log entries */
// static const char FuncName[] = "DataExchange";

   if ( !profile_manager_have_data_to_send() ) {
      return 0;  //  Done
   }

   /* initialize the return value */
   int status=0;

   /* seek iridium satellite signals */
   if( NTW_Search() > 0 ) {

      /* execute gps services */
   // GpsServices(dest);
      
      /* activate the modem serial port */
      MDM_Enable(19200);

      /* register the modem with the iridium system */
      if ( NTW_Register() > 0 )
      {
         /* download the mission configuration from the remote host */
         if (DownLoadMissionCfg(2)>0) { /*vitals.status &= ~DownLoadCfg;*/ }

         /* upload logs and profiles to the remote host */
         status=UpLoad(2);

         /* logout of the remote host */
         logout();

         /* format for logentry */
      // static const char format[]="DataExchange cycle complete: PrfId=%d "
      //    "ConnectionAttempts=%u  Connections=%u\n";
         
         /* write the number connections and connection attempts */
      // LogEntry(FuncName,format,PrfIdGet(),vitals.ConnectionAttempts,vitals.Connections); 
      }

   // else
   // {
   //    static const char msg[]="Iridium modem registration failed.\n";
   //    LogEntry(FuncName,msg); 
   // }
      
      /* deactivate the modem serial port */
      MDM_Disable();
   }

   return status;
}
# endif

# ifdef FW_SIMULATION
typedef void* taskReturnValue;
# else
typedef void  taskReturnValue;
# endif

static taskReturnValue profile_processor_theTask( void* arg ) {

  uint16_t p;  //  loop index when iterating over packets
  uint16_t b;  //  loop index when iterating over bursts

  //  Network connection state
  //
  enum {

    PPR_NTW_IsDisconnected,

    PPR_NTW_Connect,
    PPR_NTW_Connecting,
    PPR_NTW_IsConnected,

    PPR_NTW_Disconnect,

  } PPR_NTW_State ntw_state = PPR_NTW_IsDisconnected;

  //  Packet transfer (from profile manager) state
  //
  enum {

    PPR_PTX_Idle,
    PPR_PTX_RequestInfoPacket,
    PPR_PTX_ProcessInfoPacket,
    PPR_PTX_RequestDataPacket,
    PPR_PTX_WaitingForPacket,
    PPR_PTX_ProcessPacket,
    PPR_PTX_Transmitting,

  } PPR_PTX_State ptx_state = PPR_PTX_Idle;

  time_t   packet_request_time   = 0;
  uint16_t packet_request_number = 0;

  //  Keep track of packets and bursts
  //
  typedef enum {

    PPR_PCKT_NonExistent,
    PPR_PCKT_WaitToReceive,
    PPR_PCKT_WaitToBeProcessed,
    PPR_PCKT_ReadyToSend,
    PPR_PCKT_Sent,
    PPR_PCKT_ResendRequest,
    PPR_PCKT_ReceivedConfirmed

  } PPR_PCKT_State;

  typedef enum {

    PPR_BRST_NonExistent,
    PPR_BRST_WaitToReceive,
    PPR_BRST_WaitToBeProcessed,
    PPR_BRST_ReadyToSend,
    PPR_BRST_Sent,
    PPR_BRST_ResendRequest

  } PPR_BRST_State;

  //  At StartTransfer: pckt_state[:] = brst_state[:][:] = NonExistent
  //  Then, update when packets are recieved from profile manager,
  //  when packets are processed, when bursts are transferred.
  //
  uint16_t profile_yyddd = 0;
  uint16_t number_of_packets = 0;
  PPR_PCKT_State pckt_state[NPACK];
  PPR_BRST_State brst_state[NPACK][NBRST];

  //  Know thyself
  //
  data_exchange_address_t const myAddress = DE_Addr_ProfileProcessor;

  while ( 1 /* eternal loop */ ) {

    if ( !runTask ) {
      taskIsRunning = 0;

    } else {
      taskIsRunning = 1;
      //  Keep the task manager informed that this task is not stuck
      gHNV_ProfileProcessorTask_Status = TASK_IS_RUNNING;

      //  Check for incoming packet and process command or data
      //
      data_exchange_packet_t packet;
      if ( 0 == profile_processor_dequeuePacket( &packet) ) {

        if ( packet.to != myAddress ) {
          io_out_string ( "ERROR PPR RX misaddressed packet\r\n" );
          //  TODO - Release potential data pointer back to sender!

        } else {
          //  If there is a need to respond,
          //  the following address will be overwritten.
          //  A non DE_Addr_Nobody value will be used as a flag
          //  to send the packet_response.
          data_exchange_packet_t packet_response;
          packet_response.to = DE_Addr_Nobody;

          switch ( packet.type ) {

          case DE_Type_Command:

            switch ( packet.data.Command.Code ) {

            //  Start a profile transmission
            case CMD_PPR_StartTransfer:

            # ifdef FW_SIMULATION
              char* serial_device = "/dev/ttyS5";
              if ( 0 < MDM_open_serial_port ( serial_device, 9600 ) ) {
                //  Convert to packet
                syslog_out ( SYSLOG_ERROR, "profile_processor_theTask", "Cannot open serial port %s to modem.", serial_device );
              }
            # endif

              for ( p=0; p<NPACK; p++ ) {
                pckt_state[p] = PPR_PCKT_NonExistent;
                for( b=0; b<NBRST; b++ ) {
                  brst_state[p][b] = PPR_BRST_NonExistent;
                }
              }

              ntw_state = PPR_NTW_Connect;
              ptx_state = PPR_PTX_RequestInfoPacket;
              packet_request_number = 0;
	      break;

            case CMD_PPR_StopTransfer:

              ntw_state = PPR_NTW_Disconnect;
              ptx_state = PPR_PTX_Idle;
	      break;

            default:
              //  This should never happen!
              //  This task only handles the above commands.
	      break;

            } // switch ( packet.data.Command.Code ) {

	    break;

          case DE_Type_Profile_Info_Packet:

            if ( ptx_state != PPR_PTX_WaitingForPacket ) {
              //  Unexpected!
            }

            ptx_state = PPR_PTX_ProcessInfoPacket:

	    break;

          case DE_Type_Profile_Data_Packet:

            if ( ptx_state != PPR_PTX_WaitingForPacket ) {
              //  Unexpected!
            }

            ptx_state = PPR_PTX_ProcessPacket:

	    break;


          default:
            //  This should never happen!
            //  This task only handles commands
            //  or received transfer packets.

	    break;
	    
	  } // switch ( packet.type ) {

	}

      }

      //  Processing
      //  (1) Network connect/disconnect
      //  (2) Packet request/receive
      //  (3) Burst generation/transfer
      //
      //  There are reports back to profile manager at some stages
      //  E.g. failure to establich a network connection, or
      //       network request to resend a packet
      //

      //  (1) Network connect/disconnect //////////////////////////////////////////////////
      //
      switch ( ntw_state ) {

      case PPR_NTW_IsDisconnected:
        //  Do nothing
        break;

      case PPR_NTW_Connect:
        //  Check network registration,
        //  dial into server, login
        //  If connecting can be done in stages,
        //  ntw_state = PPR_NTW_Connecting
        //  Else (assume for now)
            ntw_state = PPR_NTW_IsConnected
        break;

      case PPR_NTW_Connecting:
        //  If connecting can be done in stages,
        //  check for status & when connected,
            ntw_state = PPR_NTW_IsConnected
        break;

      case PPR_NTW_IsConnected:
        //  Check that network did not fail
        if ( NTW_ConnectionFailed() ) {
          //  TODO - Either report back to Profile Manager, and wait for re-connect command
          //  ???
          //         Or     try reconnecting (with max_#_retries & overall timeout
          //  implemented...
          if ( reconnect_attempts <= 0 ) {
            //  Give up, terminate this profile transfer attempt
            ntw_state = PPR_NTW_IsDisconnected;
            //  TODO - Report back to profile manager that transfer failed at this point in time.
          } else {
            reconnect_attempts--;
            ntw_state = PPR_NTW_Connect;
          }
        } else {
          //  PPR_NTW_IsConnected ==> burst transfers will proceed (see below)
        }
        break;

      case PPR_NTW_Disconnect:
        //  TODO  --  FIND OUT HOW TO DO THIS !!!
        //
        //  logout on rudics? may trigger carrier detect down?
        //
        //  Then, after that has been done,
        MDM_Disconnect();
        ntw_state = PPR_NTW_IsDisconnected;

        //  TODO - Set state variables to ???

        break;

      default: //  Impossible to get here - code error / stack corruption or something
        //  TODO Send SYSLOG packet to Controller
      }  // switch ( ntw_state ) {

      //  (2) Packet request/receive /////////////////////////////////////////////////////////
      //
      switch ( ptx_state ) {
      case PPR_PTX_Idle:
        //  Do nothing
        break;

      case PPR_PTX_RequestInfoPacket:
        //  This request can happen before the network connection has been established
        //  There will be overhead time on the profile manager side to prepare the packets
        //  HERE TODO -- request info packet (essentially the layout information of the profile)
        packet_request_time = time((time_t)*0);
        ptx_state = PPR_PTX_WaitingForPacket;
        break;

      case PPR_PTX_RequestDataPacket:
        //  TODO -- request packet from profile manager, and inclide packen# requested
        packet_request_time = time((time_t)*0);
        ptx_state = PPR_PTX_WaitingForPacket;
        break;

      case PPR_PTX_WaitingForPacket:
        if( time((time_t*)0) > packet_request_time + packet_exchange_timeout ) {
          //    profile_manager slow to respond!!!
          //    FIXME Handle situation
        }
        break;

      case PPR_PTX_ProcessInfoPacket:
        //  When the info packet was received,
        //  it was stored into memory,
        //  and the ptx_state was set
        //  from PPR_PTX_WaitingForPacket
        //    to PPR_PTX_ProcessInfoPacket
        //

        assert ( received_packet==sane );
        //
        profile_yyddd = 12345;          //  TODO: Get from info packet
        profile_number_of_packets = 4;  //  TODO: Get from info packet: nSBRD+nPORT+nOCR+nMCOMS
        assert ( profile_number_of_packets<=NPACK );

          pckt_state[0] = PPR_PCKT_ReadyToSend;
        for ( p=1; p<profile_number_of_packets; p++ ) {
          pckt_state[p] = PPR_PCKT_WaitToReceive;
        }

          brst_state[0][0] = PPR_BRST_ReadyToSend;
          brst_state[0][1] = PPR_BRST_ReadyToSend;
          brst_state[0][2] = PPR_BRST_ReadyToSend;

        //
        //  Immediately request first data packet.
        packet_request_number = 1;
        ptx_state = PPR_PTX_RequestDataPacket;
        break;

      case PPR_PTX_ProcessDataPacket:
        //  When a data packet was received,
        //  it was stored into rx_packet_raw memory,
        //  and the ptx_state was set
        //  from PPR_PTX_WaitingForPacket
        //   to  PPR_PTX_ProcessDataPacket
        //
        assert ( received_packet==sane );
        //
        p = 1; // TODO: Get from received data packet
        if ( p <= profile_number_of_packets ) {
          pckt_state[p] = PPR_PCKT_ReadyToSend;
          uint16_t nb = 5; // Number of burst in this packet. Get from received data packet
          for ( b=0; b<=nb; b++ ) {
            brst_state[p][b] = PPR_BRST_ReadyToSend;
          }
        } else {
          //  ERROR
        }
        //  TODO - convert packet into encoded bursts
        //       - keep track in pckt_state[]  brst_state[][]
        //
        ptx_state = PPR_PTX_Transmitting;
        break;

      case PPR_PTX_Transmitting:

        //  Check state of transmitted bursts
        //  and requirements.
        //  Maybe wait or request next packet
        //  or re-request a packet or all done
        packet_request_number = TODO;
        ptx_state = PPR_PTX_RequestDataPacket;
        break;

      default: //  Impossible to get here - code error / stack corruption or something
        //  TODO Send SYSLOG packet to Controller
      } // switch ( ptx_state ) {
      
      //  (3) Burst generation/transfer  //////////////////////////////////////////////////
      //
      if ( profile_yyddd > 0 ) {

      } // if ( profile_yyddd > 0 )
    }

    //  Manual scheduling aid
    vTaskDelay( (portTickType)(TASK_DELAY_MS(10)) );
  }

  return (taskReturnValue)0;

}

int16_t profile_processor_createTask( void ) {

  static int8_t taskCreated = 0;

  if( 1 == taskCreated ) {
    return 1;                      //  Already created
  } else {

    // Start bail-out block
    do {
      // Create task
    # ifdef FW_SIMULATION
      pthread_t pp_thread;
      pthread_create ( &pp_thread, NULL, profile_processor_theTask, (void*)0 );

    # else
      if ( pdPASS != xTaskCreate( profile_processor_theTask,
                HNV_ProfileProcessorTask_NAME, HNV_ProfileProcessorTask_STACK_SIZE, NULL,
                HNV_ProfileProcessorTask_PRIORITY, &gHNV_ProfileProcessorTask_Handler ) ) {
        break;
      }
    # endif

      // Create / initialize control mutex
    # ifdef FW_SIMULATION
      if ( 0 != pthread_mutex_init( &taskCtrlMutex, (pthread_mutexattr_t*)0 ) )
        break;
    # else
      if ( NULL == (gTaskCtrlMutex = xSemaphoreCreateMutex()) )
        break;
    # endif

      // Create / allocate / initialize other locally needed resources here

    # ifdef FW_SIMULATION
      profile_processor_initCircbuf();
    # else
      // Create message queue
      if ( NULL == ( rxPackets = xQueueCreate(N_RX_PACKETS, sizeof(data_exchange_packet_t) ) ) )
        break;
    # endif

      // Allocate memory for packets that are received
      //
      int8_t all_allocated = 1;
      int i;
      for ( i=0; i<N_DA_POINTERS; i++) {
        local_data[i] = (local_data_t*) pvPortMalloc ( sizeof(local_data_t) );
        if ( local_data[i] ) {
          can_use_local_data[i] = 1;
        } else {
          can_use_local_data[i] = 0;
          all_allocated = 0;
        }
      }
      if ( !all_allocated ) {

        for ( i=0; i<N_DA_POINTERS; i++) {
          if ( local_data[i] ) {
            vPortFree ( local_data[i] );
          }
        }
        break;
      }

      taskCreated = 1;

    } while (0);

    return (1==taskCreated)
         ? 0                             //  task was created successfully
         : -1;                           //  task could not be created  --  TODO: Diagnostic return value / error code
  
  }

}
