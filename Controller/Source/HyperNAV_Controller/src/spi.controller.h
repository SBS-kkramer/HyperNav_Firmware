/*
 * spi.controller.h
 *
 * Created: 01/06/2016 11:25:37 AM
 *  Author: rvandommelen
 */ 


#ifndef SPI_CONTROLLER_H_
#define SPI_CONTROLLER_H_

#include <compiler.h>
#include <stdint.h>

# define SPI_HANDSHAKE_VIA_2_LINES 1
# ifdef SPI_HANDSHAKE_VIA_2_LINES

typedef enum {
  SPI_IND_Active,
  SPI_IND_Passive
} SPI_Indicator_t;

//! \brief   Set Own SPI Indicator to Active or Passive
void SPI_Set_MyBoard_Indicator( SPI_Indicator_t indicator );

//! \brief   Get Own Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
SPI_Indicator_t SPI_Get_MyBoard_Indicator();

//! \brief   Get Other Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
SPI_Indicator_t SPI_Get_OtherBoard_Indicator();

# else

# endif

//! \brief   Clear out any remaining data from hardware RX buffer
//! @return  The number of bytes that were present
int16_t SPI_clear_rx();

//! \brief Send a packet to the spectrometer board.
//! @param  header          Pointer to the packet header
//! @param  header_size     Header size in bytes
//! @param  data            Pointer to the packet data
//! @param  data_size       Data size in bytes
//! @return                 The number of header+data bytes transmitted
int16_t SPI_tx_packet(uint8_t* header, int16_t header_size, uint8_t* data, int16_t data_size, int16_t* tx_size );

//! Receive a packet from the spectrometer board.
//!
//! @param  header          Pointer to the packet header
//! @param  header_size     Header size in bytes
//! @param  data            Pointer to the packet data
//! @param  data_size       Data size in bytes
//! @return                 The number of header+data bytes received
//!
//!  NOTE! This function currently assumes only one spec board is present.
//!
int16_t SPI_rx_packet(uint8_t header[], int16_t const header_size, uint8_t data[], int16_t const data_size, int16_t* rx_size );



#endif /* SPI.CONTROLLER_H_ */
