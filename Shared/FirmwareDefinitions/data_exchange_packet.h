/*! \file data_exchange_packet.h ********************************************
 *
 * \brief Definition of packet used to pass information (commands, information, data) from one task to another.
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

#ifndef _DATA_EXCHANGE_PACKET_H_
#define _DATA_EXCHANGE_PACKET_H_

# include <stdint.h>

# include <time.h>
# include <sys/time.h>

# include "FreeRTOS.h"
# include "semphr.h"

# if 0
//# include "smc_sram.h"

# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"

# include "profile_packet.shared.h"
# endif

//*****************************************************************************
// Data types
//*****************************************************************************

typedef union
{
  F64 f64   ;
  S64 s64   ;
  U64 u64   ;
  F32 f32[2];
  S32 s32[2];
  U32 u32[2];
  S16 s16[4];
  U16 u16[4];
  S8  s8 [8];
  U8  u8 [8];

  enum {

    //  Responses to CMD_ANY_Query:
    //  by ADQ
    RSP_ADQ_Query_Is_Idle,
    RSP_ADQ_Query_Error_AllOff,
    RSP_ADQ_Query_Error_OCROff,
    RSP_ADQ_Query_Error_MCOMSOff,
    RSP_ADQ_Query_Ok,
    RSP_ADQ_Query_MCOMSPolling,
    //  by PMG
    RSP_PMG_Query_Is_Idle,
    RSP_PMG_Query_Is_Rx,
    RSP_PMG_Query_Is_Tx,
    //  by SLG
    RSP_SLG_Query_Is_Idle,
    RSP_SLG_Query_Is_Rx,
    RSP_SLG_Query_Is_Dark,

    //  by ANY
    RSP_ANY_ACK = 0x18273645,
    RSP_ANY_NAK

  } rsp;

} DEUnion64;


//
//  Provide a unique address for each task
//  that will communicate with other tasks
//  using  data_exchange_packet_t
//
typedef enum {

  DE_Addr_Nobody = 0,  /*  DO NOT CHANGE!  */

  DE_Addr_ControllerBoard_Commander,
# if defined(OPERATION_AUTONOMOUS)
  DE_Addr_StreamLog,
# else
  DE_Addr_StreamLog_MISSING,
# endif
  DE_Addr_AuxDataAcquisition,
# if defined(OPERATION_NAVIS)
  DE_Addr_ProfileManager,
# else
  DE_Addr_ProfileManager_MISSING,
# endif

  DE_Addr_ControllerBoard_DataExchange,  //  Never send a packet to this task!


  DE_Addr_SpectrometerBoard_DataExchange,  //  Never send a packet to this task!

  DE_Addr_SpectrometerBoard_Commander,
  DE_Addr_DataAcquisition,
  DE_Addr_ProfileProcessor,

  DE_Addr_Void

} data_exchange_address_t;


typedef enum {

  DE_Type_Nothing = 0,   /*  DO NOT CHANGE!  */

  DE_Type_Ping,

  DE_Type_Command,

  DE_Type_Response,

  DE_Type_Syslog_Message,

  DE_Type_Configuration_Data,
  DE_Type_Spectrometer_Data,
//DE_Type_OCR_Data,
  DE_Type_OCR_Frame,
//DE_Type_MCOMS_Data,
  DE_Type_MCOMS_Frame,
  DE_Type_Profile_Info_Packet,
  DE_Type_Profile_Data_Packet

} data_exchange_type_t;

typedef enum {

  Tx_Sts_Not_Started       = 0,
  Tx_Sts_No_Profile        = 1,
  Tx_Sts_Empty_Profile     = 2,
  Tx_Sts_Package_Fail      = 3,
  Tx_Sts_Modem_Fail        = 4,
  Tx_Sts_Early_Termination = 5,
  Tx_Sts_Txed_00Percent    = 6,
  Tx_Sts_Txed_25Percent    = 7,
  Tx_Sts_Txed_50Percent    = 8,
  Tx_Sts_Txed_75Percent    = 9,
  Tx_Sts_AllDone           = 10

} Transfer_Status_t;

typedef struct {

  enum {
    //  List of all available commands, ordered by recipient task
    //  When adding a comand, specify the sender(s)
    //
    //  The enum values are CMD_<destination>_<description>
    //

    //  ControllerBoard_Commander
    //
    CMD_CMC_StartSpecBoard,   //  From SpectrometerBoard_Commander, request to sync time, send configuration... This should only be relevant after a spec board reset, while the controller kept running

//  CMD_CMC_Profiling_Status, //  From Data Acquisition, only when in NAVIS Profiling mode

    CMD_CMC_Transfer_Status,  //  From Profile Manager, updating status of data TX

    //  SpectrometerBoard_Commander
    //
    CMD_CMS_SyncTime,         //  From ControllerBoard_Commander, upon request from spectrometer board

    //  Data Acquisition
    //
    //  Continuous
    CMD_DAQ_Start,            //  From Streamer & Logger
    CMD_DAQ_Stop,             //  From Streamer & Logger
    //
    //  Profiling
    CMD_DAQ_Profiling,        //  From Profile Manager or from Command/Control
    CMD_DAQ_Profiling_End,    //  From Profile Manager or from Command/Control

    //  Auxiliary Data Acquisition
    //
    //  Continuous
    CMD_ADQ_StartStreaming,   //  From Data Acquisition
    CMD_ADQ_StartProfiling,   //  From Data Acquisition
    CMD_ADQ_Stop,             //  From Data Acquisition
    CMD_ADQ_PollMCOMS,        //  From Data Acquisition - Generate response
    CMD_ADQ_InterruptMCOMS,   //  From Data Acquisition - Generate response

    //  Streamer & Logger
    //
    CMD_SLG_Start,            //  From Command/Control - Value determines if FreeFall (V=0), Calibration(V>0, 1xDark, VxLight), DarkTest (V<0, IntTime=-V)
    CMD_SLG_Stop,             //  From Command/Control

    //  Profile Manager
    //
    CMD_PMG_Profile,          //  From Command/Control
    CMD_PMG_Profile_End,      //  From Command/Control
    CMD_PMG_Offloading,       //  From Data Acquisition
    CMD_PMG_Offloading_Done,  //  From Data Acquisition
    CMD_PMG_Transmit,         //  From Command/Control
    CMD_PMG_Transmit_End,     //  From Command/Control

//  CMD_PMG_SendPacket,       //  From Profile Processor

//  CMD_PMG_NetworkDown,      //  From Profile Processor
//  CMD_PMG_NetworkConnected, //  From Profile Processor
//  CMD_PMG_PacketRequest,    //  From Profile Processor
//  CMD_PMG_TransferComplete, //  From Profile Processor

//  CMD_PMG_Transmit_Fake,    //  From Command/Control

    //  Profile Processor
    //
//  CMD_PPR_StartTransfer,    //  From Profile Manager
//  CMD_PPR_StopTransfer,     //  From Profile Manager

    //  Query state of task
    //
    CMD_ANY_Query,            //  From anywhere         - Generate response

    //  NoOp
    //
    CMD_ALL_NoOp              //  Any to any, for testing, ignored

  } Code;

  DEUnion64 value;

} data_exchange_command_t;


typedef struct {

  enum {
    //  List of all available responses types,
    //
    //  The enum values are RSP_<destination>_<description>
    //

    //  Responding to DE_Type_Ping
    RSP_ALL_Ping = 0x0A0B0C0D,

    //  Data Acquisition
    RSP_PMG_Profiling_Begun,
    RSP_PMG_Profiling_Ended,
    RSP_PMG_Transmitting_Begun,
    RSP_PMG_Transmitting_Ended,

    //  Data Acquisition
    //
    //  Continuous
    //
    //  Profiling
    //
    //  Any
    RSP_DAQ_MCOMSDone,      //  Response after completion of CMD_ADQ_PollMCOMS
    RSP_DAQ_MCOMSQuiet,     //  Response after completion of CMD_ADQ_InterruptMCOMS

    //  Auxiliary Data Acquisition
    //

    //  Profile Processor
    //

    //  Responding to CMD_ANY_Query:
    RSP_ADQ_Query,
    RSP_DXC_Query,
    RSP_PMG_Query,
    RSP_SLG_Query,

    RSP_CMS_Query,
    RSP_DAQ_Query,
    RSP_DXS_Query,
    RSP_PPR_Query,

    //  NoOp
    //
    RSP_ALL_NoOp

  } Code;

  DEUnion64 value;

} data_exchange_respond_t;

typedef enum {
    SL_Nothing = 0,  /*  DO NOT CHANGE!  */

    //  Errors
    SL_ERR_RxEmptyMemoryPacket,
    SL_ERR_RxNonPingPacket,
    SL_ERR_MisaddressedPacket,
    SL_ERR_UnexpectedCommand,
    SL_ERR_UnexpectedResponse,
    SL_ERR_UnexpectedSysMessage,
    SL_ERR_UnexpectedData,
    SL_ERR_UnexpectedPacketType,

    SL_MSG_StackWaterMark,      //  value = stack space remaining

    //  Info messages
    SL_MSG_DXC_Received,        //  value = number of bytes
    SL_MSG_DXC_Sent,            //  value = number of bytes

    SL_MSG_SLG_Started,         //  value == 0
    SL_MSG_SLG_Darks,           //  value == 0
    SL_ERR_SLG_StartFail,       //  value == 0
    SL_MSG_SLG_Stopped,         //  value == 0

    SL_MSG_PMG_Started,         //  value == 0
    SL_ERR_PMG_StartFail,       //  value == 0
    SL_MSG_PMG_Stopped,         //  value == 0

    SL_MSG_DAQ_Started,         //  value == 0
    SL_MSG_DAQ_Darks,           //  value == 0
    SL_MSG_DAQ_Stopped,         //  value == 0
    SL_MSG_DAQ_Pressure,        //  value == current pressure pressure

    SL_MSG_DAQ_Profiling,       //  value == 0
    SL_MSG_DAQ_ProfileEnd,      //  value == 0

    SL_MSG_DAQ_State,           //  value == 0
    SL_MSG_DAQ_Phase,           //  value == 0

    SL_MSG_ADQ_Started,         //  value == 0
    SL_MSG_ADQ_Stopped,         //  value == 0

    //  When adding an entry here, add an acronym in syslogAcronym()
    //

    SL_Void
} data_exchange_syslog_n;

typedef struct {
  data_exchange_syslog_n number;
  U64                     value;
} data_exchange_syslog_t;

//  The sender specifies if the memory address is in RAM or in SRAM.
//  The receiver must then use the appropriate copy method
//  to make a local copy of the sent data.
typedef enum {
  EmptyRAM,
  EmptySRAM,
  FullRAM,
  FullSRAM
} data_exchange_data_state_t;

//  A data_exchange_data_package_t is created by the sender
//  of a data_exchange_packet.
//  The package contains a mutex to control access across tasks,
//  and a double-flag (empty/full)x(RAM/SRAM).
//  The sender can only copy data into an empty package, and then declares it full.
//  The receiver copies from a full package, and then declares it empty.
//  Copying into and out of memory is mutex protected.
//  The memory access method is 'normal' for RAM, and 'special' for SRAM.
typedef struct {

  xSemaphoreHandle            mutex;
  data_exchange_data_state_t  state;
  void*                       address;

} data_exchange_data_package_t;

typedef union {

  char                          Ping_Message[8];

  data_exchange_command_t       Command;
  data_exchange_respond_t       Response;
  data_exchange_syslog_t        Syslog;

  data_exchange_data_package_t* DataPackagePointer;

} data_exchange_container_t;



typedef struct data_exchange_packet {

  data_exchange_address_t    to;
  data_exchange_address_t    from;
  data_exchange_type_t       type;
  data_exchange_container_t  data;

} data_exchange_packet_t;

//*****************************************************************************
// Exported functions
//*****************************************************************************

char const* taskName( data_exchange_address_t address );
char const* taskAcronym( data_exchange_address_t address );
char const* syslogAcronym( data_exchange_syslog_n syslog );
void data_exchange_packet_router ( data_exchange_address_t current_node, data_exchange_packet_t* packet );

#endif /* _DATA_EXCHANGE_PACKET_H_ */
