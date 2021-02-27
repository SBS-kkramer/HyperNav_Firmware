
/*! \file data_exchange_packet.c *******************************************************
 *
 * \brief Data Exchange - Generic functionality
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# include "data_exchange_packet.h"

# include "command.controller.h"
# include "aux_data_acquisition.h"
# include "stream_log.h"
# include "profile_manager.h"

# include "data_exchange.controller.h"

# include "io_funcs.controller.h"

typedef enum {

        DE_Brd_None,
        DE_Brd_Controller,
		DE_Brd_Spectrometer

} Address_Board_t;

static Address_Board_t BoardOf( data_exchange_address_t address ) {

  switch ( address ) {

  case DE_Addr_ControllerBoard_Commander:
# if defined(OPERATION_AUTONOMOUS)
  case DE_Addr_StreamLog:
# endif
  case DE_Addr_AuxDataAcquisition:
# if defined(OPERATION_NAVIS)
  case DE_Addr_ProfileManager:
# endif
  case DE_Addr_ControllerBoard_DataExchange:
          return DE_Brd_Controller; break;

  case DE_Addr_SpectrometerBoard_DataExchange:
  case DE_Addr_SpectrometerBoard_Commander:
  case DE_Addr_DataAcquisition:
  case DE_Addr_ProfileProcessor:
          return DE_Brd_Spectrometer; break;

  case DE_Addr_Nobody:
  case DE_Addr_Void:
  default:
          return DE_Brd_None; break;
  }
}

char const* taskName( data_exchange_address_t address ) {

  static const char* name[] = {
    [DE_Addr_Nobody]                         = "None",

    [DE_Addr_ControllerBoard_Commander]      = "Command-Controller",
# if defined(OPERATION_AUTONOMOUS)
    [DE_Addr_StreamLog]                      = "Stream-and-Log",
# else
    [DE_Addr_StreamLog_MISSING]              = "MISSING-Stream-and-Log",
# endif
    [DE_Addr_AuxDataAcquisition]             = "Auxiliary-Data-Acquisition",
# if defined(OPERATION_NAVIS)
    [DE_Addr_ProfileManager]                 = "Profile-Manager",
# else
    [DE_Addr_ProfileManager_MISSING]         = "MISSING-Profile-Manager",
# endif
    [DE_Addr_ControllerBoard_DataExchange]   = "Data-Exchange-Controller",

    [DE_Addr_SpectrometerBoard_DataExchange] = "Data-Exchange-Spectrometer",
    [DE_Addr_SpectrometerBoard_Commander]    = "Command-Spectrometer",
    [DE_Addr_DataAcquisition]                = "Data-Acquisition",
    [DE_Addr_ProfileProcessor]               = "Profile-Processor",

    [DE_Addr_Void]                           = "Void"
  };

  if ( /* DE_Addr_Nobody <= address && */  /*  DE_Addr_Nobody == 0 always <= unsigned value */  
       address <= DE_Addr_Void ) return name[address];
  else return "?";
}

char const* taskAcronym( data_exchange_address_t address ) {

  static const char* acronym[] = {
    [DE_Addr_Nobody]                         = "None",

    [DE_Addr_ControllerBoard_Commander]      = "CMC",
# if defined(OPERATION_AUTONOMOUS)
    [DE_Addr_StreamLog]                      = "SLG",
# else
    [DE_Addr_StreamLog_MISSING]              = "MISSING-SLG",
# endif
    [DE_Addr_AuxDataAcquisition]             = "ADQ",
# if defined(OPERATION_NAVIS)
    [DE_Addr_ProfileManager]                 = "PMG",
# else
    [DE_Addr_ProfileManager_MISSING]         = "MISSING-PMG",
# endif
    [DE_Addr_ControllerBoard_DataExchange]   = "DXC",

    [DE_Addr_SpectrometerBoard_DataExchange] = "DXS",
    [DE_Addr_SpectrometerBoard_Commander]    = "CMS",
    [DE_Addr_DataAcquisition]                = "DAQ",
    [DE_Addr_ProfileProcessor]               = "PPR",

    [DE_Addr_Void]                           = "Void"
  };

  if ( /* DE_Addr_Nobody <= address && */  /*  DE_Addr_Nobody == 0 always <= unsigned value */
       address <= DE_Addr_Void ) return acronym[address];
  else return "?";
}

char const* syslogAcronym( data_exchange_syslog_n syslog ) {

  static const char* acronym[] = {
    [SL_Nothing]                  = "Nothing",

    [SL_ERR_RxEmptyMemoryPacket]  = "RxEmMem",
    [SL_ERR_RxNonPingPacket]      = "RxNoPng",
    [SL_ERR_MisaddressedPacket]   = "WrgAddr",
    [SL_ERR_UnexpectedCommand]    = "UexCmmd",
    [SL_ERR_UnexpectedResponse]   = "UexResp",
    [SL_ERR_UnexpectedData]       = "UexData",
    [SL_ERR_UnexpectedPacketType] = "UexType",

    [SL_MSG_StackWaterMark]       = "StackWaterMark",

    [SL_MSG_DXC_Received]         = "DXC_RX",
    [SL_MSG_DXC_Sent]             = "DXC_TX",

    [SL_MSG_SLG_Started]          = "SLGStart",
    [SL_MSG_SLG_Darks]            = "SLGDarks",
    [SL_ERR_SLG_StartFail]        = "SLGStartFail",
    [SL_MSG_SLG_Stopped]          = "SLGStop",

    [SL_MSG_PMG_Started]          = "PMGStart",
    [SL_ERR_PMG_StartFail]        = "PMGStartFail",
    [SL_MSG_PMG_Stopped]          = "PMGStop",

    [SL_MSG_DAQ_Started]          = "DAQStart",
    [SL_MSG_DAQ_Darks]            = "DAQDarks",
    [SL_MSG_DAQ_Stopped]          = "DAQStop",
    [SL_MSG_DAQ_Pressure]         = "DAQPressure",

    [SL_MSG_DAQ_Profiling]        = "DAQProfiling",
    [SL_MSG_DAQ_ProfileEnd]       = "DAQProfileEnd",

    [SL_MSG_DAQ_State]            = "DAQState",
    [SL_MSG_DAQ_Phase]            = "DAQPhase",

    [SL_MSG_ADQ_Started]          = "ADQStart",
    [SL_MSG_ADQ_Stopped]          = "ADQStop",

    [SL_Void]                     = "Void",
  };

  if ( /* SL_Nothing <= syslog && */  /*  SL_Nothing == 0 always <= unsigned value */
       syslog <= SL_Void ) return acronym[syslog];
  else return "?";
}

//  Generic function, callable by every task on every board,
//  to properly route a packet
//  It is possible to send a packet to self!
void data_exchange_packet_router ( data_exchange_address_t current_node, data_exchange_packet_t* packet ) {

//io_out_S32 ( "DBG DX router (%d)", current_node );
//io_out_S32 ( " { %d ", packet->from );
//io_out_S32 ( "[%d] ", (S32)packet->type );
//if ( packet->type == DE_Type_HyperNav_Spectrometer_Data
//  || packet->type == DE_Type_HyperNav_Spectrometer_Data_Release ) {
//  io_out_S32 ( "<%p> ", (S32)packet->data.Spectrometer_Data_Pointer );
//} else {
//  io_out_S32 ( "<%d.", (S32)packet->data.Command.Code );
//  io_out_S32 ( "%d> ", (S32)packet->data.Command.Value );
//}
//io_out_S32 ( "%d }\r\n", packet->to );

  //  Sanity checks
  if ( !packet
    || packet->to == DE_Addr_Nobody || current_node == DE_Addr_Nobody
    || packet->to == DE_Addr_Void   || current_node == DE_Addr_Void ) {
    return;
  }

  //
  // Routing table:
  // Packets move from node to node.
  //
  // E.g., sending a packet from DataAcquirition to StreamLog:
  // DataAcquisition >--> DXS >--via.SPI--> DXC >--> StreamLog
  //
  // Gen-Ctrl == Generic (non data exchange) task on controller board
  // D-Ex-Ctrl == Data Exchange Controller task == DXC
  // D-Ex-Spec == Data Exchange Spectrometer task == DXS
  // Gen-Spec == Generic (non data exchange) task on spectrometer board
  //
  // drkt == direkt transfer to addressed task.
  //         When the destination task resides on the same board.
  // DXS*, DXC* indicate that the transfer will be via SPI.
  //
  //      \---
  //       -  to
  //        \-
  //         -     Gen   D-Ex   D-Ex  Gen
  //  from    \    Ctrl  Ctrl   Spec  Spec
  //  current  -
  //            \____________________________
  //            |             |             |
  //  Gen-Ctrl  |  drkt  drkt | DXC   DXC   |
  //            |             |             |
  //  D-Ex-Ctrl |  drkt  ---- | DXS*  DXS*  |
  //            |-------------|-------------|
  //  D-Ex-Spec |  DXC*  DXC* | ----  drkt  |
  //            |             |             |
  //  Gen-Spec  |  DXS   DXS  | drkt  drkt  |
  //            |_____________|_____________|


  Address_Board_t const current_board = BoardOf( current_node );
  Address_Board_t const destination_board = BoardOf( packet->to );

  if ( current_board == destination_board ) {

    switch ( packet->to ) {
# if defined(HNV_CONTROLLER_FIRMWARE)
    case DE_Addr_ControllerBoard_Commander:            commander_controller_pushPacket ( packet ); break;
# if defined(OPERATION_AUTONOMOUS)
    case DE_Addr_StreamLog:                                      stream_log_pushPacket ( packet ); break;
# else
    case DE_Addr_StreamLog_MISSING:                                                                break;
# endif
    case DE_Addr_AuxDataAcquisition:                   aux_data_acquisition_pushPacket ( packet ); break;
# if defined(OPERATION_NAVIS)
    case DE_Addr_ProfileManager:                            profile_manager_pushPacket ( packet ); break;
# else
    case DE_Addr_ProfileManager_MISSING:                                                           break;
# endif
    case DE_Addr_ControllerBoard_DataExchange:     data_exchange_controller_pushPacket ( packet ); break;
# elif defined(HNV_SPECTROMETER_FIRMWARE)
    case DE_Addr_SpectrometerBoard_DataExchange: data_exchange_spectrometer_pushPacket ( packet ); break;
    case DE_Addr_SpectrometerBoard_Commander:        commander_spectrometer_pushPacket ( packet ); break;
    case DE_Addr_DataAcquisition:                          data_acquisition_pushPacket ( packet ); break;
    case DE_Addr_ProfileProcessor:                        profile_processor_pushPacket ( packet ); break;
# else
#  error "Missing compiler directive: Must define either HNV_CONTROLLER_FIRMWARE or HNV_SPECTROMETER_FIRMWARE"
# endif
    default: // Unknown destination
      io_out_string ( "ERROR DX router " );
      io_out_string ( taskAcronym( packet->from ) );
      io_out_string ( " -> " );
      io_out_string ( taskAcronym( packet->to ) );
      io_out_string ( " not handled\r\n" );
      break;
    }

  } else if ( current_node == DE_Addr_ControllerBoard_DataExchange ) {

    // Should not happen:
    // Data Exchange Task will route directly via SPI to other board
    io_out_string ( "ERROR DX router: Unexpected destination: User direct " );
    io_out_string ( taskAcronym(packet->from) );
    io_out_string ( " Ct->Sp " );
    io_out_string ( taskAcronym(packet->to) );
    io_out_string ( " via SPI\r\n" );

  } else if ( current_node == DE_Addr_SpectrometerBoard_DataExchange ) {

    // Should not happen:
    // Data Exchange Task will route directly via SPI to other board
    io_out_string ( "ERROR DX router: Unexpected destination: User direct " );
    io_out_string ( taskAcronym(packet->from) );
    io_out_string ( " Sp->ct " );
    io_out_string ( taskAcronym(packet->to) );
    io_out_string ( " via SPI\r\n" );

  } else if ( current_board == DE_Brd_Controller ) {

    //  Implicit: this is not the data_exchange task,
    //  and the final destination is the spectrometer board.
    //  Thus, route via data_exchange_controller
    // io_out_string ( "DBG DX router ***->DXC\r\n" );

# if defined(HNV_CONTROLLER_FIRMWARE)
    //io_out_string ( "DBG DX_router via DX\r\n" );
    data_exchange_controller_pushPacket ( packet );
# else  // Above had already confirmed that either HNV_CONTROLLER_FIRMWARE or HNV_SPECTROMETER_FIRMWARE is defined
    //  Code should never get here
	io_out_string ( "ERROR: Spec FW" );
# endif

  } else if ( current_board == DE_Brd_Spectrometer ) {

    //  Implicit: this is not the data_exchange task,
    //  and the final destination is the controller board.
    //  Thus, route via data_exchange_spectrometer
    // io_out_string ( "DBG DX router ***->DXS\r\n" );

# if defined(HNV_SPECTROMETER_FIRMWARE)
    //io_out_string ( "DBG DX_router via DX\r\n" );
    data_exchange_spectrometer_pushPacket ( packet );
# else  // Above had already confirmed that either HNV_CONTROLLER_FIRMWARE or HNV_SPECTROMETER_FIRMWARE is defined
    //  Code should never get here
	io_out_string ( "ERROR: Ctrl FW" );
# endif

  } else {
    io_out_string ( "ERROR DX router unexpected routing configuration\r\n" );
  }

}


