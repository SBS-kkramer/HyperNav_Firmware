/* slld_hal_example.c - SLLD Hardware Abstraction Layer example Code */
 
/**************************************************************************
* Copyright 2011 Spansion LLC. All Rights Reserved. 
*
* This software is owned and published by: 
* Spansion LLC, 915 DeGuigne Drive, Sunnyvale, CA 94088 ("Spansion").
*
* BY DOWNLOADING, INSTALLING OR USING THIS SOFTWARE, YOU AGREE TO BE BOUND 
* BY ALL THE TERMS AND CONDITIONS OF THIS AGREEMENT.
*
* This software constitutes driver source code for use in programming Spansion's 
* Flash memory components. This software is licensed by Spansion to be adapted only 
* for use in systems utilizing Spansion's Flash memories. Spansion is not be 
* responsible for misuse or illegal use of this software for devices not 
* supported herein.  Spansion is providing this source code "AS IS" and will 
* not be responsible for issues arising from incorrect user implementation 
* of the source code herein.  
*
* Spansion MAKES NO WARRANTY, EXPRESS OR IMPLIED, ARISING BY LAW OR OTHERWISE, 
* REGARDING THE SOFTWARE, ITS PERFORMANCE OR SUITABILITY FOR YOUR INTENDED 
* USE, INCLUDING, WITHOUT LIMITATION, NO IMPLIED WARRANTY OF MERCHANTABILITY, 
* FITNESS FOR A  PARTICULAR PURPOSE OR USE, OR NONINFRINGEMENT.  Spansion WILL 
* HAVE NO LIABILITY (WHETHER IN CONTRACT, WARRANTY, TORT, NEGLIGENCE OR 
* OTHERWISE) FOR ANY DAMAGES ARISING FROM USE OR INABILITY TO USE THE SOFTWARE, 
* INCLUDING, WITHOUT LIMITATION, ANY DIRECT, INDIRECT, INCIDENTAL, 
* SPECIAL, OR CONSEQUENTIAL DAMAGES OR LOSS OF DATA, SAVINGS OR PROFITS, 
* EVEN IF Spansion HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.  
*
* This software may be replicated in part or whole for the licensed use, 
* with the restriction that this Copyright notice must be included with 
* this software, whether used in part or whole, at all times.  
*/

#include <stdio.h>
#include "slld_targetspecific.h"
#include "slld.h"
#include "slld_hal.h"

#include "spi.h"
#include "board.h"

# include "FreeRTOS.h"  //  For vTaskDelay()
# include "task.h"      //  For vTaskDelay()

// ***************************************************************************
//  FLASH_READ - HAL read function
//
//  input : command               write a single command byte to flash
//          sys_addr              system address to be used
//          data_buffer           Pointer to the data buffer where to store the read data
//          Number_Of_Read_Bytes  number of bytes to be read
//
//  return value : status of the operation - FAIL or SUCCESS
// ***************************************************************************
SLLD_STATUS FLASH_READ
(
BYTE     command,                        /* write a single command byte to flash */
ADDRESS  sys_addr,                       /* system address to be used */
BYTE    *data_buffer,                    /* Pointer to the data buffer containing data to be written */
int      Number_Of_Read_Bytes            /* number of bytes to be read */
)
{
    int data_cycle, Number_Of_Dummy_Bytes = 0;

    // Select SPI chip
    if ( SPI_OK != spi_selectChip( SPI_MASTER_1, 0 ) ) {
            spi_unselectChip( SPI_MASTER_1, 0 );
	    return SLLD_E_SPI_CS_ERROR;
    }
    
    // Write the command
    //
    if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)command ) ) {
            spi_unselectChip( SPI_MASTER_1, 0 );
	    return SLLD_E_SPI_WR_0_ERROR;
    }

    // Write the address
    if (sys_addr != ADDRESS_NOT_USED)
    {
    //  Always use 4-byte addressing
    //  Ignore dual or quad instructions - treat as normal
    //
    //  switch (command)
    //  {
    //      case SPI_DUALIO_HPRD_CMD:
    //      case SPI_DUALIO_HPRD_4B_CMD:
    //      {
    //          SPIDATA_D = (BYTE)((sys_addr >> 16) & 0x000000FF);
    //          SPIDATA_D = (BYTE)((sys_addr >>  8) & 0x000000FF);
    //          SPIDATA_D = (BYTE) (sys_addr        & 0x000000FF);
    //          break;
    //      }
    //      case SPI_QUADIO_HPRD_CMD:
    //      case SPI_QUADIO_HPRD_4B_CMD:
    //      {
    //          SPIDATA_Q = (BYTE)((sys_addr >> 16) & 0x000000FF);
    //          SPIDATA_Q = (BYTE)((sys_addr >>  8) & 0x000000FF);
    //          SPIDATA_Q = (BYTE) (sys_addr        & 0x000000FF);
    //          break;
    //      }
    //      default:
    //      {
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >> 24) & 0x000000FF) ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_1_ERROR;
		}
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >> 16) & 0x000000FF) ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_1_ERROR;
		}
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >>  8) & 0x000000FF) ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_1_ERROR;
		}
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)( sys_addr        & 0x000000FF) ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_1_ERROR;
		}
    //          break;
    //      }
    //  }
    }

    // Write the dummy bytes
    switch (command)
    {
        case SPI_FAST_READ_CMD:
        case SPI_FAST_READ_4B_CMD:
        case SPI_DUALIO_RD_CMD:
        case SPI_DUALIO_RD_4B_CMD:
        case SPI_QUADIO_RD_CMD:
        case SPI_QUADIO_RD_4B_CMD:
        case SPI_OTPR_CMD:
        {
            Number_Of_Dummy_Bytes = 1;
            // Write a dummy (latency) byte to the data bus
            for (data_cycle = 0; data_cycle < Number_Of_Dummy_Bytes; data_cycle++)
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)0x0 ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_2_ERROR;
		}
            break;
        }
    //  case SPI_DUALIO_HPRD_CMD:
    //  case SPI_DUALIO_HPRD_4B_CMD:
    //  {
    //      Number_Of_Dummy_Bytes = 1;
    //      // Write a dummy byte to the data bus - This is actually the mode bit
    //      for (data_cycle = 0; data_cycle < Number_Of_Dummy_Bytes; data_cycle++)
    //          SPIDATA_D = 0x0;
    //      break;
    //  }
    //  case SPI_QUADIO_HPRD_CMD:
    //  case SPI_QUADIO_HPRD_4B_CMD:
    //  {
    //      Number_Of_Dummy_Bytes = 3;
    //      // Write the dummy bytes to the data bus - The first byte is actually the mode bit
    //      for (data_cycle = 0; data_cycle < Number_Of_Dummy_Bytes; data_cycle++)
    //          SPIDATA_Q = 0x0;
    //      break;
    //  }
        case SPI_RES_CMD:
        {
            Number_Of_Dummy_Bytes = 3;
            // Write the dummy bytes to the data bus
            for (data_cycle = 0; data_cycle < Number_Of_Dummy_Bytes; data_cycle++)
                if ( SPI_OK != spi_write ( SPI_MASTER_1, (uint16_t)0x0 ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_3_ERROR;
		}
            break;
        }
        default:
        {
            Number_Of_Dummy_Bytes = 0;
            break;
        }
    }

    // Read the data
    if (Number_Of_Read_Bytes != 0)
    {
    //  switch (command)
    //  {
    //      case SPI_DUALIO_RD_CMD:
    //      case SPI_DUALIO_RD_4B_CMD:
    //      case SPI_DUALIO_HPRD_CMD:
    //      case SPI_DUALIO_HPRD_4B_CMD:
    //      {
    //          // Read the data using the relevant mode
    //          for (data_cycle = 0; data_cycle < Number_Of_Read_Bytes; data_cycle++)
    //              *(data_buffer + data_cycle) = SPIDATA_D;
    //          break;
    //      }
    //      case SPI_QUADIO_RD_CMD:
    //      case SPI_QUADIO_RD_4B_CMD:
    //      case SPI_QUADIO_HPRD_CMD:
    //      case SPI_QUADIO_HPRD_4B_CMD:
    //      {
    //          // Read the data using the relevant mode
    //          for (data_cycle = 0; data_cycle < Number_Of_Read_Bytes; data_cycle++)
    //              *(data_buffer + data_cycle) = SPIDATA_Q;
    //          break;
    //      }
    //      default:
    //      {
                // Read the data using the relevant mode
        vTaskDelay(1);

                for (data_cycle = 0; data_cycle < Number_Of_Read_Bytes; data_cycle++) {

                    uint16_t rxData = 0x0;
                    if ( SPI_OK != spi_write ( SPI_MASTER_1,  rxData ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_WR_4_ERROR;
		}
		    if ( SPI_OK != spi_read  ( SPI_MASTER_1, &rxData ) ) {
                        spi_unselectChip( SPI_MASTER_1, 0 );
			return SLLD_E_SPI_RD_0_ERROR;
		}
                    data_buffer[data_cycle] = rxData;
		}
    //          break;
    //      }
    //  }
    }

    // De-Select SPI chip
    if ( SPI_OK != spi_unselectChip( SPI_MASTER_1, 0 ) ) return SLLD_E_SPI_CU_ERROR;

    return SLLD_OK;
}


// ***************************************************************************
//  FLASH_WRITE - HAL write function
//
//  input : command                  write a single command byte to flash
//          sys_addr                 system address to be used
//          data_buffer              Pointer to the data buffer where to store the written data
//          Number_Of_Written_Bytes  number of bytes to be written
//
//  return value : status of the operation - FAIL or SUCCESS
// ***************************************************************************
SLLD_STATUS FLASH_WRITE
(
BYTE     command,                        /* write a single command byte to flash */
ADDRESS  sys_addr,                       /* system address to be used */
BYTE    *data_buffer,                    /* Pointer to the data buffer containing data to be written */
int      Number_Of_Written_Bytes         /* number of bytes to be written */
)
{
    SLLD_STATUS status = SLLD_OK;
    int data_cycle;

    // Select SPI chip
    spi_status_t spi_status = spi_selectChip( SPI_MASTER_1, 0 );
    
    // Write the command
    //
    spi_write ( SPI_MASTER_1, (uint16_t)command );

    // Write the address
    if (sys_addr != ADDRESS_NOT_USED)
    {
        spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >> 24) & 0x000000FF) );
        spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >> 16) & 0x000000FF) );
        spi_write ( SPI_MASTER_1, (uint16_t)((sys_addr >>  8) & 0x000000FF) );
        spi_write ( SPI_MASTER_1, (uint16_t)( sys_addr        & 0x000000FF) );
    }

    // Write the data
    if (Number_Of_Written_Bytes != 0)
    {
    //  switch (command)
    //  {
    //      case SPI_QPP_CMD:
    //      case SPI_QPP_4B_CMD:
    //      {
    //          // Write the data using the relevant mode
    //          for (data_cycle = 0; data_cycle < Number_Of_Written_Bytes; data_cycle++)
    //              SPIDATA_Q = *(data_buffer + data_cycle);
    //          break;
    //      }
    //      default:
    //      {
                // Write the data using the relevant mode
                for (data_cycle = 0; data_cycle < Number_Of_Written_Bytes; data_cycle++)
                    spi_write ( SPI_MASTER_1, (uint16_t)( *(data_buffer+data_cycle) ) );
    //          break;
    //      }
    //  }
    }

    // De-Select SPI chip
    spi_status = spi_unselectChip( SPI_MASTER_1, 0 );

    return(status);
}


/*****************************************************************************/
