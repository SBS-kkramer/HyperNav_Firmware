/*
 * spi.controller.c
 *
 * Created: 01/06/2016 10:54:18 AM
 *  Author: rvandommelen
 */ 

#include "spi.controller.h"
#include "spi.h"
#include <compiler.h>
#include <stdint.h>
#include "board.h"
#include "io_funcs.controller.h"
#include <stdlib.h>
#include <string.h>
#include <gpio.h>
#include <FreeRTOS.h>
#include <task.h>

#include <time.h>
#include <sys/time.h>

// Local variables
# define TimedOut(timeZero) (timeZero>25000L)

# ifdef SPI_HANDSHAKE_VIA_2_LINES

//! \brief   Set Own SPI Indicator to Active or Passive
inline
void SPI_Set_MyBoard_Indicator( SPI_Indicator_t indicator ) {
  switch ( indicator ) {
    case SPI_IND_Active:  gpio_clr_gpio_pin( CTRL_RDY ); break;
    case SPI_IND_Passive: gpio_set_gpio_pin( CTRL_RDY ); break;
  }
}

//! \brief   Get Own Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
inline
SPI_Indicator_t SPI_Get_MyBoard_Indicator() {
  return gpio_get_pin_value( CTRL_RDY ) ? SPI_IND_Passive : SPI_IND_Active;
}

//! \brief   Get Other Board's SPI Indicator
//! @return  SPI_IND_Active or SPI_IND_Passive
inline
SPI_Indicator_t SPI_Get_OtherBoard_Indicator() {
  return gpio_get_pin_value( SPEC1_RDY__INT2 ) ? SPI_IND_Passive : SPI_IND_Active;
}

# else

# endif

//! \brief   Clear out any remaining data from hardware RX buffer
//! @return  The number of bytes that were present
int16_t SPI_clear_rx() {
  int16_t n_rx = 0;
  int16_t n_try = 0;
  uint16_t rxword;
  while ( spi_readRegisterFullCheck(SPI_MASTER_1) && n_rx<16 && n_try<512) {
    if ( SPI_OK == spi_read( SPI_MASTER_1, &rxword ) ) {
      n_rx++;
    }
    n_try++;
  }

  // while ( n_try>0 ) { io_out_string("t"); n_try--; };
  // while ( n_rx>0 ) { io_out_string("R"); n_rx--; };

  return n_rx;
}

//! \brief Send a packet to the spectrometer board.
//! @param  header          Pointer to the packet header
//! @param  header_size     Header size in bytes
//! @param  data            Pointer to the packet data
//! @param  data_size       Data size in bytes
//! @return                 The number of header+data bytes transmitted
int16_t SPI_tx_packet(uint8_t* header, int16_t const header_size, uint8_t* data, int16_t const data_size, int16_t* tx_size ) {

    int16_t return_value = 0;

    // Check header+data size
    int32_t const tx_sz = (int32_t)header_size + (int32_t)data_size;
    union {
        uint16_t asValue;
        uint8_t  asBytes[2];
    } size;
    size.asValue = header_size + data_size;

    if ( size.asValue != tx_sz || tx_sz >= 0x8000 ) {
        return -6;
    }

    // Clear out anything that might be in hardware rx buffer
    SPI_clear_rx();

    // Initialize the controller-board-ready line
    SPI_Set_MyBoard_Indicator( SPI_IND_Active );

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
       SPI_Set_MyBoard_Indicator( SPI_IND_Passive );
       return -5;
    }

 // taskENTER_CRITICAL();

    // Select chip.  Second argument hardwires for spec board.
    spi_status_t spi_status = spi_selectChip( SPI_MASTER_1, 0 );

    uint16_t rxItem;

    //  Get in sync:
    //  Send sync[0]=='?' until receiving '?' character.
    //  Then, send remainder of sync string, expecting previous
    //  character returned.
    //  Expect 'R' in response to 'T' to confirm other side will receive.
    //  If receiving 'T' as response to 'T', there is a conflict,
    //  and this function and the other function will abort.
    //
    char const* sync = "?SNCTX";
    //char const* back = "!sncrx";
    int  const  sync_len = strlen(sync);

    int in_sync = 1;
    int conflict = 0;

//  while ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
//      io_out_string ( "Cannot write to SPI\r\n" );
//  }

    //  Make sure both boards are in agreement on who is sending and who is receiving.

    spi_write( SPI_MASTER_1, (uint16_t)sync[in_sync] );

    timeZero = 0;

    while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && in_sync < sync_len
        && !TimedOut(timeZero)
        && !conflict ) {

        if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            //
            //  Idle...
            //
            //  In normal operation, this should not take long.
            timeZero++;
        } else {

            if ( spi_readRegisterFullCheck( SPI_MASTER_1 ) ) {

              spi_read( SPI_MASTER_1, &rxItem );

              if ( sync[in_sync] == 'X' ) {

                //  Expect the other side to respond with 'R'
                //  indicating it is ready to receive.
                //  Else, abort
                if ( rxItem != 'R' ) {
                  conflict = 1;
                } else {
                  in_sync++;
                }

              } else {

                //  Expect a response of the previously sent character.
                //  Initially, the slave will send '?' until it receives an 'S'
                //  If getting out of sync, start over.
                if ( (char)rxItem == sync[in_sync-1] ) {
                  in_sync++;
                } else {
                  in_sync = 1;
                }
              }

              if ( in_sync < sync_len ) {
                if ( SPI_OK != spi_write( SPI_MASTER_1, (uint16_t)sync[in_sync] ) ) {
//                io_out_string ( "Cannot write to SPI\r\n" );
                } else {
//                io_out_string ( "Wrote " );
//                io_out_X8  ( (U8) sync[in_sync], " to SPI\r\n" );
                }
              }

              timeZero = 0;
            }
        }
    }

    if ( sync_len != in_sync ) {

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

      //  Send two bytes, telling other board how many bytes to expect.
      //
      int tx_sz = 0;
      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && tx_sz < 2
          && !TimedOut(timeZero) ) {

        if ( spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            spi_write( SPI_MASTER_1, (uint16_t) size.asBytes[tx_sz++] );
            timeZero = 0;
        } else {
            timeZero ++;
        }
      }


      // Send header
      int16_t tx_header_size = 0;
      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && tx_header_size < header_size
        && !TimedOut(timeZero) ) {

        if ( spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            spi_write( SPI_MASTER_1, (uint16_t)header[tx_header_size++] );
            timeZero = 0;
        } else {
            timeZero++;
        }
      }

      if ( tx_header_size < header_size ) {
        if ( TimedOut(timeZero) ) {
          return_value = -3;
        } else {
          return_value = -1;
        }
      }

      timeZero = 0;

      // Send data
      int16_t tx_data_size = 0;

      if ( 0 == return_value ) {
       while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && tx_data_size < data_size
        && !TimedOut(timeZero) ) {

        if ( spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            spi_write( SPI_MASTER_1, (uint16_t)data[tx_data_size++] );
            timeZero = 0;
        } else {
            timeZero++;
        }
       }

       if ( tx_data_size < data_size ) {
        if ( TimedOut(timeZero) ) {
          return_value = -3;
        } else {
          return_value = -1;
        }
       }
      }

      if ( 0 == return_value ) {

       *tx_size = tx_header_size + tx_data_size;

       while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && !TimedOut(timeZero) ) {
        if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            timeZero ++;
        }
       }
      }
    }

    spi_status = spi_unselectChip( SPI_MASTER_1, 0 );

    // Indicate to spectrometer board that packet data is complete
    SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

 // taskEXIT_CRITICAL();

    return return_value;
}


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
int16_t SPI_rx_packet(uint8_t header[], int16_t const header_size, uint8_t data[], int16_t const data_size, int16_t* rx_size ) {

    uint16_t const  txDummy = 0xAA99;
    int16_t count_dummy_received = 0;

    int16_t return_value = 0;

    uint16_t rxItem;  //  Memory location to place received item

    // Clear out anything that might be in hardware rx buffer
    SPI_clear_rx();

    // Indicate to other board we are ready to receive
    SPI_Set_MyBoard_Indicator( SPI_IND_Active );

//taskENTER_CRITICAL();

    // Select chip.  Second argument hardwires for spec board.
    spi_selectChip( SPI_MASTER_1, 0 );

    //  Get in sync:
    //  Send sync[1]=='S' until receiving until receiving '?' character.
    //  Then, send remainder of sync string, expecting previous
    //  character returned.
    //  Expect 'R' in response to 'T' to confirm other side will receive.
    //  If receiving 'T' as response to 'T', there is a conflict,
    //  and this function and the other function will abort.
    //
    char const* sync = "?SNCRX";
    int  const  sync_len = strlen(sync);

    int in_sync = 1;
    int32_t timeZero = 0;
    int conflict = 0;

    //  Make sure both boards are in agreement on who is sending and who is receiving.

    spi_write( SPI_MASTER_1, (uint16_t)sync[in_sync] );

    while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
        && in_sync < sync_len
        && !TimedOut(timeZero)
        && !conflict ) {

        if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            //
            //  Idle...
            //
            //  In normal operation, this should not take long.
            timeZero++;
        } else {

            if ( spi_readRegisterFullCheck( SPI_MASTER_1 ) ) {

              spi_read( SPI_MASTER_1, &rxItem );

              if ( sync[in_sync] == 'X' ) {

                //  Expect the other side to respond with 'R'
                //  indicating it is ready to receive.
                //  Else, abort
                if ( rxItem != 'T' ) {
                  conflict = 1;
                } else {
                  in_sync++;
                }

              } else {

                //  Expect a response of the previously sent character.
                //  Initially, the slave will send '?' until it receives an 'S'
                //  If getting out of sync, start over.
                if ( (char)rxItem == sync[in_sync-1] ) {
                  in_sync++;
                } else {
                  in_sync = 1;
                }
              }

              if ( in_sync < sync_len ) {
                spi_write( SPI_MASTER_1, (uint16_t)sync[in_sync] );
              }

              timeZero = 0;
            }
        }
    }

    if ( sync_len != in_sync ) {

      if ( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -1;   //  Other Board SPI Passive
      } else if ( conflict ) {
          return_value = -2;   //  Intention (Tx/Rx) Conflict
      } else if ( TimedOut(timeZero) ) {
          return_value = -3;   //  Sync Timeout
      } else {
          return_value = -4;   //  Unknown failure
      }

    } else {

      spi_write( SPI_MASTER_1, txDummy );

      //  Receive two bytes,
      //  to be interpreted as total to-be-received size (header + data)
      //
      union {
        uint16_t asValue;
        uint8_t  asBytes[2];
      } size;

      int16_t rx_sz = 0;

      while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && rx_sz < 2
          && !TimedOut(timeZero) ) {

        if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
          timeZero ++;
        } else {
          if ( spi_readRegisterFullCheck( SPI_MASTER_1 ) ) {
            spi_read( SPI_MASTER_1, &rxItem );
            size.asBytes[rx_sz++] = (uint8_t)(rxItem & 0xFF);
	    if ( (rxItem & 0xFF) == (txDummy & 0xFF) ) {
              count_dummy_received++;
	    }
            spi_write( SPI_MASTER_1, txDummy );
            timeZero = 0;
          } else {
            timeZero ++;
          }
        }
      }

      if ( TimedOut(timeZero) ) {
        return_value = -13;
      } else if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
        return_value = -11;
      }

      if ( 0 == return_value ) {

        timeZero = 0;

        int16_t rx_header_size = 0;

        while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && rx_header_size < header_size
          && size.asValue > 0
          && !TimedOut(timeZero) ) {

          if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            timeZero ++;
          } else {
            if ( spi_readRegisterFullCheck( SPI_MASTER_1 ) ) {
              spi_read( SPI_MASTER_1, &rxItem );
              header[rx_header_size++] = (uint8_t)(rxItem & 0xFF);
	      if ( (rxItem & 0xFF) == (txDummy & 0xFF) ) {
                count_dummy_received++;
	      }
              size.asValue--;
              spi_write( SPI_MASTER_1, txDummy );
              timeZero = 0;
            } else {
              timeZero ++;
            }
          }
        }

        *rx_size = rx_header_size;

        if ( TimedOut(timeZero) ) {
          return_value = -23;
        } else if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -21;
        }
      }

      if ( 0 == return_value ) {

        timeZero = 0;

        int16_t rx_data_size = 0;

        while( SPI_IND_Active == SPI_Get_OtherBoard_Indicator()
          && rx_data_size < data_size
          && size.asValue > 0
          && !TimedOut(timeZero) ) {

          if ( !spi_writeRegisterEmptyCheck( SPI_MASTER_1 ) ) {
            timeZero ++;
          } else {
            if ( spi_readRegisterFullCheck( SPI_MASTER_1 ) ) {
              spi_read( SPI_MASTER_1, &rxItem );
              data[rx_data_size++] = (uint8_t)(rxItem&0xFF);
	      if ( (rxItem & 0xFF) == (txDummy & 0xFF) ) {
                count_dummy_received++;
	      }
              size.asValue--;
              spi_write( SPI_MASTER_1, txDummy );
              timeZero = 0;
            } else {
              timeZero ++;
            }
          }
        }

        *rx_size += rx_data_size;

        if ( TimedOut(timeZero) ) {
          return_value = -33;
        } else if( SPI_IND_Passive == SPI_Get_OtherBoard_Indicator() ) {
          return_value = -31;
        } else if ( size.asValue > 0 ) {
          return_value = -37;
        } else if ( rx_data_size >= data_size ) {
          return_value = -34;
        }

      }

      if ( 0 == return_value ) {
        if ( count_dummy_received > 0 ) {
          return_value = -1000-count_dummy_received;
        }
      }
    }

    spi_unselectChip( SPI_MASTER_1, 0 );

    // Indicate to spectrometer board that packet data is received
    SPI_Set_MyBoard_Indicator( SPI_IND_Passive );

//taskEXIT_CRITICAL();

    return return_value;
}
