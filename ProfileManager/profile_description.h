# ifndef _PM_PROFILE_DESCRIPTION_H_
# define _PM_PROFILE_DESCRIPTION_H_

# include <unistd.h>
# include <stdint.h>
# include "profile_packet.h"

typedef struct Profile_Description {

  uint16_t  profiler_sn;
  uint16_t  profile_yyddd;
  uint16_t  nSBRD;
  uint16_t  nPORT;
  uint16_t  nOCR;
  uint16_t  nMCOMS;

} Profile_Description_t;

typedef struct Profile_Packet_Definition {

  uint16_t  profiler_sn;
  uint16_t  profile_yyddd;

  uint16_t  numData_SBRD;
  uint16_t  numData_PORT;
  uint16_t  numData_OCR;
  uint16_t  numData_MCOMS;

  uint16_t  nPerPacket_SBRD;
  uint16_t  nPerPacket_PORT;
  uint16_t  nPerPacket_OCR;
  uint16_t  nPerPacket_MCOMS;

  uint16_t  numPackets_SBRD;
  uint16_t  numPackets_PORT;
  uint16_t  numPackets_OCR;
  uint16_t  numPackets_MCOMS;

} Profile_Packet_Definition_t;

int profile_description_init ( Profile_Description_t* pd, const char* data_dir, uint16_t profile_ID );
int profile_description_update( Profile_Description_t* pd, char measurement_type );
int profile_description_save( Profile_Description_t* pd, const char* data_dir );
int profile_description_retrieve( Profile_Description_t* pd, const char* data_dir, uint16_t profile_ID );

int profile_packet_definition_to_info_packet ( Profile_Packet_Definition_t* ppd, Profile_Info_Packet_t* pip );
int profile_packet_definition_from_info_packet ( Profile_Packet_Definition_t* ppd, Profile_Info_Packet_t* pip );

# endif
