/*
 * spi.spectrometer.c
 *
 * Created: 02/06/2016 1:32:25 PM
 *  Author: rvandommelen
 */

#include "spi.spectrometer.h"
#include "spi.h"
#include <compiler.h>
#include <stdint.h>
#include "board.h"
#include "io_funcs.spectrometer.h"
#include <gpio.h>
#include <stdlib.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <time.h>
#include <sys/time.h>

# define TimedOut(timeZero) (timeZero>25000L)

void SPI_InitGpio() {

  // Initialize the SPI control lines.
  gpio_configure_pin(SPEC_RDY, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
  gpio_enable_pin_pull_up(SPEC_RDY);
  gpio_enable_gpio_pin(SPEC_RDY);

  gpio_configure_pin(CTRL_RDY, GPIO_DIR_INPUT);
  gpio_enable_gpio_pin(CTRL_RDY);

}


//! \brief   Set Own SPI Indicator to Active or Passive
inline
void SPI_Set_MyBoard_Indicator( SPI_Indicator_t indicator ) {
  switch ( indicator ) {
    case SPI_IND_Active:  gpio_clr_gpio_pin( SPEC_RDY ); break;
    case SPI_IND_Passive: gpio_set_gpio_pin( SPEC_RDY ); break;
  }
}

//! \brief   Get Own Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
inline
SPI_Indicator_t SPI_Get_MyBoard_Indicator() {
  return gpio_get_pin_value( SPEC_RDY ) ? SPI_IND_Passive : SPI_IND_Active;
}

//! \brief   Get Other Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
inline
SPI_Indicator_t SPI_Get_OtherBoard_Indicator() {
  return gpio_get_pin_value( CTRL_RDY ) ? SPI_IND_Passive : SPI_IND_Active;
}


//! \brief   Clear out any remaining data from hardware RX buffer
//! @return  The number of bytes that were present
int16_t SPI_clear_rx() {
  int16_t n_rx = 0;
  int16_t n_try = 0;
  uint16_t rxword;
  while ( spi_readRegisterFullCheck(SPI_SLAVE_0) && n_rx<16 && n_try<512 ) {
    if ( SPI_OK == spi_read( SPI_SLAVE_0, &rxword ) ) {
      n_rx++;
    }
    n_try++;
  }

  return n_rx;
}

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
int32_t SPI_rx_packet(uint8_t* header, int16_t header_size, uint8_t* data_ram, sram_pointer2 data_sram, int16_t data_size) {

    int16_t return_value = 0;

    // Note the use of the ready lines is interpreted as the hardware is done
    // rather than ready to receive

    // Clear anything in the hardware RX buffer.
//  while ( spi_readRegisterFullCheck(SPI_SLAVE_0) )
 //     spi_read( SPI_SLAVE_0, &rxword );

    SPI_clear_rx();

    //  Get in sync:
    //  Send '?' character until receiving 'S', which indicates start of SYNC string

    char const* sync = "?SNCRX";
    int  const  sync_len = strlen(sync);

    int in_sync = 0;
    int conflict = 0;
    int32_t timeZero = 0;

//  taskENTER_CRITICAL();

    // Indicate to controller board that spectrometer brd is waiting
    SPI_Set_MyBoard_Indicator( SPI_IND_Active );

    //  Make sure both boards are in agreement on who is sending and who is receiving.

    spi_write( SPI_SLAVE_0, (uint16_t)sync[in_sync] );

    while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && in_sync < sync_len-1
        && !TimedOut(timeZero)
        && !conflict ) {

        if ( !spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
            //
            //  Idle...
            //
            //  FIXME: This is time-wasting idling,
            //  inside a critical section, i.e., all other tasks are blocked.
            //  However, there seems to be no other way,
            //  because as SPI slave, there is no possibility
            //  to know when the SPI master will become active.
            timeZero++;
       } else {

            if ( spi_readRegisterFullCheck( SPI_SLAVE_0 ) ) {

                uint16_t rxItem;
                spi_read( SPI_SLAVE_0, &rxItem );

                if ( sync[in_sync] == 'C' ) {
                    if ( (char)(rxItem&0xFF) != 'T' ) {
                        conflict = 1;
                    } else {
                        in_sync++;
                    }
                } else {
                    if ( (char)(rxItem&0xFF) == sync[in_sync+1] ) {
                        in_sync++;
                    } else {
                        in_sync = 0;
                    }
                }

                if ( in_sync < sync_len-1 ) {
                    spi_write( SPI_SLAVE_0, (uint16_t)sync[in_sync] );
                }

                timeZero = 0;
            }
        }
    }

    if ( sync_len-1 != in_sync ) {

      if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -1;
      } else if ( conflict ) {
          return_value = -2;
      } else if ( TimedOut(timeZero) ) {
          return_value = -3;
      } else {
          return_value = -4;
      }

    } else {

      timeZero = 0;

      //  Receive two bytes,
      //  to be interpreted as total to-be-received size (header + data)
      //
      union {
        uint16_t asValue;
        uint8_t  asBytes[2];
      } size;

      int rx_sz = 0;
      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && rx_sz < 2
          && !TimedOut(timeZero) ) {

        if ( spi_readRegisterFullCheck( SPI_SLAVE_0 ) ) {
            uint16_t rxItem;
            spi_read( SPI_SLAVE_0, &rxItem );
            size.asBytes[rx_sz++] = (uint8_t)rxItem;
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }


      // Recv bytes from the controller master,
      // and always send back the previously received datum.
      //
      int rx_header_size = 0;
      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && rx_header_size < header_size
          && size.asValue > 0
          && !TimedOut(timeZero) ) {

        if ( spi_readRegisterFullCheck( SPI_SLAVE_0 ) ) {
            uint16_t rxItem;
            spi_read( SPI_SLAVE_0, &rxItem );
            header[rx_header_size++] = (uint8_t)rxItem;
            size.asValue--;
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }

      timeZero = 0;

      // Recv bytes from the controller master
      int rx_data_size = 0;

      union {
        uint8_t u8[2];
        uint16_t u16;
      } sram_tmp;
      sram_tmp.u16 = 0;

      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && rx_data_size < data_size
        && size.asValue > 0
        && !TimedOut(timeZero) ) {

        if ( spi_readRegisterFullCheck( SPI_SLAVE_0 ) ) {
            uint16_t rxItem;
            spi_read( SPI_SLAVE_0, &rxItem );
            if ( data_ram ) {
              data_ram[rx_data_size++] = (uint8_t)rxItem;
            } else {
              if ( rx_data_size&1 ) {
                sram_tmp.u8[1] = rxItem;
                data_sram[rx_data_size/2] = sram_tmp.u16;
              } else {
                sram_tmp.u8[0] = rxItem;
              }
              rx_data_size++;
            }
            size.asValue--;
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }

      return_value = rx_header_size + rx_data_size;
    }

    // Indicate to controller board that receive done
    SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

//    taskEXIT_CRITICAL();

    vTaskDelay( (portTickType)TASK_DELAY_MS( 5 ) );

    // Return total number of bytes sent
    return return_value;
}


//! Send a packet from the spectrometer board.
//!
//! @param  header       Pointer to the packet header
//! @param  header_size  Header size in bytes
//! @param  data         Pointer to the packet data
//! @param  data_size    Data size in bytes
//! @return              >0 : The number of header+data bytes transmitted
//!                       0 : Error: Nothing transferred
//!                      -1 : Error: other board dropped out
//!                      -2 : Error: Rx/Tx board-board conflict
//!                      -3 : Error: Time out
//!                      -4 : Error: Generic
//!                      -5 : Error: other board not responsive
//!                      -6 : Error: requested transfer size exceeds capability
//!
int16_t SPI_tx_packet(uint8_t* header, int16_t const header_size, uint8_t* data, int16_t const data_size, int16_t* tx_size ) {

    int16_t return_value = 0;

    // Check header+data size
    int const tx_sz = header_size + data_size;
    union {
        uint16_t asValue;
        uint8_t  asBytes[2];
    } size;
    size.asValue = header_size + data_size;

    if ( size.asValue != tx_sz || tx_sz >= 0x8000 ) {
        return -6;
    }

    SPI_clear_rx();

    // Indicate to controller that spec brd has a packet to send
    SPI_Set_MyBoard_Indicator ( SPI_IND_Active );

    int32_t timeZero = 0;

    // Wait up to 1000 ms for other board to become active

    portTickType startTick = xTaskGetTickCount();
    int pastEndTime = 0;
    while ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator()
        && !pastEndTime ) {

      vTaskDelay( (portTickType)TASK_DELAY_MS( 5 ) );

      pastEndTime = ( ( xTaskGetTickCount() - startTick ) / ( portTICK_RATE_MS ) ) > 1000;
    }

    if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
       SPI_Set_MyBoard_Indicator ( SPI_IND_Passive );
       return -5;
    }

//    taskENTER_CRITICAL();

    //  Get in sync:
    //  Send '?' character until receiving 'S', which indicates start of SYNC string

    char const* sync = "?SNCTX";
    int  const  sync_len = strlen(sync);

    int in_sync = 0;
    int conflict = 0;
    timeZero = 0;

    //  Make sure both boards are in agreement on who is sending and who is receiving.

    spi_write( SPI_SLAVE_0, (uint16_t)sync[in_sync] );

    while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && in_sync < sync_len-1
        && !TimedOut(timeZero)
        && !conflict ) {

        if ( !spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
            //
            //  Idle...
            //
            //  FIXME: This is time-wasting idling,
            //  inside a critical section, i.e., all other tasks are blocked.
            //  However, there seems to be no other way,
            //  because as SPI slave, there is no possibility
            //  to know when the SPI master will become active.
            timeZero++;
       } else {

            if ( spi_readRegisterFullCheck( SPI_SLAVE_0 ) ) {

                uint16_t rxItem;
                spi_read( SPI_SLAVE_0, &rxItem );

                if ( sync[in_sync] == 'C' ) {
                    if ( (char)(rxItem&0xFF) != 'R' ) {
                        conflict = 1;
                    } else {
                        in_sync++;
                    }
                } else {
                    if ( (char)(rxItem&0xFF) == sync[in_sync+1] ) {
                        in_sync++;
                    } else {
                        in_sync = 0;
                    }
                }

                if ( in_sync < sync_len-1 ) {
                    spi_write( SPI_SLAVE_0, (uint16_t)sync[in_sync] );
                }

                timeZero = 0;
            }
        }
    }

    if ( sync_len-1 != in_sync ) {

      if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -1;
      } else if ( conflict ) {
          return_value = -2;
      } else if ( TimedOut(timeZero) ) {
          return_value = -3;
      } else {
          return_value = -4;
      }

    } else {

      timeZero = 0;

      //  Send two bytes, teling othe rboard how many bytes to expect.
      //
      int16_t tx_sz = 0;
      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && tx_sz < 2
          && !TimedOut(timeZero) ) {

        if ( spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
            spi_write( SPI_SLAVE_0, (uint16_t) size.asBytes[tx_sz++] );
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }

      // Send header one byte at a time
      int16_t tx_header_size = 0;

      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && tx_header_size < header_size
        && !TimedOut(timeZero) ) {

        if ( spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
            spi_write( SPI_SLAVE_0, (uint16_t) header[tx_header_size++] );
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }

      if ( tx_header_size < header_size ) {
        if ( TimedOut(timeZero) ) {
          return_value = -23;
        } else if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -21;
        } else {
          return_value = -24;
        }
      }

      *tx_size = tx_header_size;

      if ( 0 == return_value ) {

        timeZero = 0;

        // Send data one byte at a time
        int16_t tx_data_size = 0;

        while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && tx_data_size < data_size
          && !TimedOut(timeZero) ) {

          if ( spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
              spi_write( SPI_SLAVE_0, (uint16_t) data[tx_data_size++] );
              timeZero = 0;
          } else {
              timeZero ++;
          }
        }

        *tx_size += tx_data_size;

        if ( tx_data_size < data_size ) {
          if ( TimedOut(timeZero) ) {
            return_value = -33;
          } else if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
            return_value = -31;
          } else {
            return_value = -34;
          }
        }
      }

      timeZero = 0;

      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && !TimedOut(timeZero) ) {
        if ( !spi_writeRegisterEmptyCheck( SPI_SLAVE_0 ) ) {
            timeZero ++;
        }
      }

      if ( 0 == return_value ) {
        if ( TimedOut(timeZero) ) {
          return_value = -53;
        }
      }
    }

//  taskEXIT_CRITICAL();

    // Indicate to controller board that packet data is complete
    SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

    // Return the number of bytes sent
    return return_value;
}
