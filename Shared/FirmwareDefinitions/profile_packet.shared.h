/*! \file profile_packet.shared.h
 *
 *  \brief Define the data structure for profile_packets.
 *         For data transfer, a profile is converted into
 *         a sequence of packets.
 *         Over a faulty modem connection, it is easier to 
 *         restart the transfer at a specified packet.
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef   _PROFILE_PACKET_SHARED_H_
# define   _PROFILE_PACKET_SHARED_H_

# include <unistd.h>
# include <fcntl.h>
//# include <sys/types.h>
//# include <sys/stat.h>

# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"

typedef struct Profile_Packet_Definition {

  uint16_t  profiler_sn;
  uint16_t  profile_id;

  uint16_t  numData_SBRD;
  uint16_t  numData_PORT;
  uint16_t  numData_OCR;
  uint16_t  numData_MCOMS;

//uint16_t  nPerPacket_SBRD;
//uint16_t  nPerPacket_PORT;
//uint16_t  nPerPacket_OCR;
//uint16_t  nPerPacket_MCOMS;

  uint16_t  numPackets_SBRD;
  uint16_t  numPackets_PORT;
  uint16_t  numPackets_OCR;
  uint16_t  numPackets_MCOMS;

} Profile_Packet_Definition_t;

# define META_SZ 32      //  Packet Meta information - as characters
# define MXHNV    7      //  Constraint: Packet size <= 32kB
# warning FIX TransFer Packet Spectrometer Auxiliary Content
# define  SPEC_AUX_SERIAL_SIZE 42	//  For debugging, have 62
# define   OCR_AUX_SERIAL_SIZE 4
# define MCOMS_AUX_SERIAL_SIZE 4
# define MXOCR (MXHNV*(1+(SPEC_AUX_SERIAL_SIZE-1)/OCR_AUX_SERIAL_SIZE)) //  Constraint: Match auxiliary data
# define MXMCM (MXHNV*(1+(SPEC_AUX_SERIAL_SIZE-1)/MCOMS_AUX_SERIAL_SIZE)) //  Constraint: Match auxiliary data

# define FLAT_SZ (32+MXHNV*2*2048+120)

typedef struct Begin_Packet {

  //char  HyperNAV_ID   [ 4];  //  HYNV
  //char  hyperNAV_num  [ 4];  //  ####
  uint8_t  HYNV_num[2];

  //char  Profile_ID    [ 3];  //  PRF
  //char  profile_yyddd [ 5];  //  profile yyddd
  uint8_t  PROF_num[2];

  //char  Packet_ID     [ 4];  //  PCKT
  //char  packet_number [ 4];  //  >= '001'; Packet '000' == Profile_Packaging
  uint8_t  PCKT_num[2];

  /////////////////////////////

  // char  CRC32         [ 8];  // in HEX

  /////////////////////////////

  char  num_dat_SBRD  [ 4];
  char  num_dat_PORT  [ 4];
  char  num_dat_OCR   [ 4];
  char  num_dat_MCOMS [ 4];

  char  num_pck_SBRD  [ 4];
  char  num_pck_PORT  [ 4];
  char  num_pck_OCR   [ 4];
  char  num_pck_MCOMS [ 4];

  /////////////////////////////

# define BEGPAK_META (6*16)
  char  meta_info   [6*16];  // To be defined. E.g., time of profile, ...

} Profile_Info_Packet_t;

typedef struct Packet {

  uint8_t  HYNV_num[2];
  uint8_t  PROF_num[2];
  uint8_t  PCKT_num[2];

  /////////////////////////////

//char  CRC32         [ 8];  // in HEX

  /////////////////////////////

  struct {
    char  sensor_type;         // 'S', 'P', 'O', 'M'
    char  empty_space;
    char  sensor_ID     [10];  // e.g., 'SATYLU1234'
    char  number_of_data[ 4];  // (# of items in this packet (up to MXHNV (7?) for Hyper; bigger for OCR and MCOMS)

    char  representation;      // 'B' = Binary; 'G' = Graycode
    char  noise_bits_removed;  // 'N': No bitplaning; '0'-'F': (hex) # of bits removed; #bitplanes = 16 - # of bits removed
    char  compression;         // '0': None; 'B': Bzip2; 'G': Gzip
    char  ASCII_encoding;      // 'N': None; 'A': ASCII85; 'B': Base64

    char  compressed_sz [ 6];  // number of bytes
    char  encoded_sz    [ 6];  // number of bytes
  } header;

  union {

    struct {

      union {
        //uint16_t  pixel[MXHNV][N_SPEC_PIX];  // accomodate up to 7 spectra
        uint8_t  bitplanes  [MXHNV*N_SPEC_PIX*2];
        uint8_t   ocr_serial[MXOCR*4*4];
        uint8_t mcoms_serial[MXMCM*3*(3*2+4)];
      } sensor_data;

      union {
        // Spec_Aux_Data_t   spec_aux[MXHNV];
        //  OCR_Aux_Data_t    ocr_aux[MXOCR];
        //MCOMS_Aux_Data_t  mcoms_aux[MXMCM];
	uint8_t  spec_serial [MXHNV* SPEC_AUX_SERIAL_SIZE];
	uint8_t   ocr_serial [MXOCR*  OCR_AUX_SERIAL_SIZE];
	uint8_t mcoms_serial [MXMCM*MCOMS_AUX_SERIAL_SIZE];
      } aux_data;

    } structured;

    uint8_t flat_bytes[FLAT_SZ];

  } contents;

} Profile_Data_Packet_t;


# endif // _PROFILE_PACKET_SHARED_H_

