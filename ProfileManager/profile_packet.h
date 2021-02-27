# ifndef _PM_PROFILE_PACKET_H_
# define _PM_PROFILE_PACKET_H_

# include "profile_packet.shared.h"

# if 0
int info_packet_txmit ( Profile_Info_Packet_t* pip, int burst_size, int tx_fd, uint8_t bstStat[] );
int info_packet_save  ( Profile_Info_Packet_t* pip, const char* data_dir, const char* type );
int info_packet_fromBytes( Profile_Info_Packet_t* pip, unsigned char* asBytes, uint16_t nBytes );

int data_packet_txmit ( Profile_Data_Packet_t* p, int burst_size, int tx_fd, uint8_t bstStat[] );
int data_packet_save  ( Profile_Data_Packet_t* p, const char* data_dir, const char* type );
int data_packet_fromBytes( Profile_Data_Packet_t* p, unsigned char* asBytes, uint16_t nBytes );

int data_packet_retrieve ( Profile_Data_Packet_t* p, const char* data_dir, const char* type, uint16_t profile_id, uint16_t number_of_packet );
int data_packet_bin2gray ( Profile_Data_Packet_t* bin, Profile_Data_Packet_t* gray );
int data_packet_gray2bin ( Profile_Data_Packet_t* gray, Profile_Data_Packet_t* bin );
int data_packet_bitplane ( Profile_Data_Packet_t* pix, Profile_Data_Packet_t* bp, uint16_t remove_noise_bits );
int data_packet_debitplane ( Profile_Data_Packet_t* bp, Profile_Data_Packet_t* pix );
int data_packet_compress ( Profile_Data_Packet_t* unc, Profile_Data_Packet_t* com, char algorithm );
int data_packet_uncompress ( Profile_Data_Packet_t* cmp, Profile_Data_Packet_t* ucp );
int data_packet_encode ( Profile_Data_Packet_t* unenc, Profile_Data_Packet_t* enc, char encoding );
int data_packet_decode ( Profile_Data_Packet_t* dec, Profile_Data_Packet_t* enc );
# endif

int data_packet_compare( Profile_Data_Packet_t* p1, Profile_Data_Packet_t* p2, char ID1, char ID2 );

# endif
