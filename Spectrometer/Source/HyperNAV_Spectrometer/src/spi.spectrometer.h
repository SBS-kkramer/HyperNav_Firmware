/*
 * spi.spectrometer.h
 *
 * Created: 02/06/2016 1:32:49 PM
 *  Author: rvandommelen
 */ 


#ifndef SPI_SPECTROMETER_H_
#define SPI_SPECTROMETER_H_


#include <compiler.h>
#include <stdint.h>

#include "smc_sram.h"  //  Needed to ensure sram adressing done properly

#define FALSE 0
#define TRUE  1

typedef enum {
  SPI_IND_Active,
  SPI_IND_Passive
} SPI_Indicator_t;

void SPI_InitGpio(void);

//! \brief   Set Own SPI Indicator to Active or Passive
void SPI_Set_MyBoard_Indicator( SPI_Indicator_t indicator );

//! \brief   Get Own Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
SPI_Indicator_t SPI_Get_MyBoard_Indicator(void);

//! \brief   Get Other Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
SPI_Indicator_t SPI_Get_OtherBoard_Indicator(void);

# if 0
//! \brief  Is a received packet available?
//! @return	TRUE: packet available, FALSE: no packet available
Bool SPI_is_packet(void);

//! \brief  Copy the packet header
//! @param	header	Address at which to store the header
void SPI_get_packet_header(uint8_t* header);

//! \brief Copy the packet data
//! @param	data	Address at which to store the data
void SPI_get_packet_data(uint8_t* data);

//! \brief Return the number of received bytes (packet size)
//! @return	Number of received bytes
int16_t SPI_return_packet_size(void);
# endif

//! \brief   Clear out any remaining data from hardware RX buffer
//! @return  The number of bytes that were present
int16_t SPI_clear_rx(void);

//! \brief Receive a packet from the spectrometer board.
//! @param  header          Pointer to the packet header
//! @param  header_size     Header size in bytes
//! @param  data_ram        Pointer to the packet data in ram
//! @param  data_sram       Pointer to the packet data in sram
//! @param  data_size       Data size in bytes
//! @return                 The number of header+data bytes transmitted
//!                       0 : Error: Nothing transferred
//!                      -1 : Error: other board dropped out
//!                      -2 : Error: Rx/Tx board-board conflict
//!                      -3 : Error: Time out
//!                      -4 : Error: Generic
//!                      -5 : Error: other board not resonsive
//!                      -6 : Error: requested transfer size exceeds capability
//
//  Only one of data_ram and data_sram is provided, the other will be NULL.
int32_t SPI_rx_packet(uint8_t* header, int16_t header_size, uint8_t* data_ram, sram_pointer2 data_sram, int16_t data_size);

//! \brief Send a packet from the spectrometer board.
//!
//! @param	header			Pointer to the packet header
//! @param 	header_size		Header size in bytes
//! @param	data			Pointer to the packet data
//! @param 	data_size		Data size in bytes
//! @return					The number of header+data bytes transmitted
//!
int16_t SPI_tx_packet(uint8_t* header, int16_t header_size, uint8_t* data, int16_t data_size, int16_t* tx_size );

#endif /* SPI.SPECTROMETER_H_ */
