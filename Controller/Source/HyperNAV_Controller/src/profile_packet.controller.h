/*! \file profile_packet.controller.h
 *
 *  \brief Define function prototypes for profile packets
 *         used on the controller board.
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef _PROFILE_PACKET_CONTROLLER_H_
# define _PROFILE_PACKET_CONTROLLER_H_

# include <stdint.h>

# include "profile_packet.shared.h"

//int info_packet_txmit ( Profile_Info_Packet_t* pip, int burst_size, int tx_fd, uint8_t bstStat[] );
//int info_packet_fromBytes( Profile_Info_Packet_t* pip, unsigned char* asBytes, uint16_t nBytes );
//
int16_t info_packet_save_native     ( Profile_Info_Packet_t* pip, const char* file_name );
int16_t info_packet_retrieve_native ( Profile_Info_Packet_t* pip, const char* file_name );

//int data_packet_txmit ( Profile_Data_Packet_t* p, int burst_size, int tx_fd, uint8_t bstStat[] );
//int data_packet_fromBytes( Profile_Data_Packet_t* p, unsigned char* asBytes, uint16_t nBytes );

int16_t data_packet_save_native     ( Profile_Data_Packet_t* p, const char* file_name );
int16_t data_packet_retrieve_native ( Profile_Data_Packet_t* p, const char* file_name );

int16_t data_packet_bin2gray ( Profile_Data_Packet_t* bin, Profile_Data_Packet_t* gray );
//int data_packet_gray2bin ( Profile_Data_Packet_t* gray, Profile_Data_Packet_t* bin );
int16_t data_packet_bitplane ( Profile_Data_Packet_t* pix, Profile_Data_Packet_t* bp, uint16_t remove_noise_bits );
//int data_packet_debitplane ( Profile_Data_Packet_t* bp, Profile_Data_Packet_t* pix );
int16_t data_packet_compress ( Profile_Data_Packet_t* unc, Profile_Data_Packet_t* com, char algorithm );
//int data_packet_uncompress ( Profile_Data_Packet_t* cmp, Profile_Data_Packet_t* ucp );
int16_t data_packet_encode ( Profile_Data_Packet_t* unenc, Profile_Data_Packet_t* enc, char encoding );
//int data_packet_decode ( Profile_Data_Packet_t* dec, Profile_Data_Packet_t* enc );

//int data_packet_compare( Profile_Data_Packet_t* p1, Profile_Data_Packet_t* p2, char ID1, char ID2 );

# endif // _PROFILE_PACKET_CONTROLLER_H_
