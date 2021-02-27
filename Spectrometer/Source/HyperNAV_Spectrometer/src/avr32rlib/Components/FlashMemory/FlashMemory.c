/*
 * FlashMemory.c
 *
 * The spectrometer board Flash is an S25FL512S 512 Mbit / 64 MByte.
 * The sector size is 256 KBytes.
 * The page size is 512 Bytes.
 *
 * Created: 9/19/2017 3:29:40 PM
 *  Author: bplache
 */

# include "FlashMemory.h"

# include "slld.h"
# include "slld_hal.h"
# include "gpio.h"
# include "SatlanticHardware.h"
# include "FreeRTOS.h"
# include "task.h"
# include "string.h"
# include "errorcodes.h"
# include "io_funcs.spectrometer.h"

# define FlashDebug

/******************************************************************************
 *
 * flashMemory_isWriteInProgress - Check the Write In Progress bit of Status Register 1
 *
 * WIP bit means Device Busy: a Write Registers, program, erase, or other
 * operation is in progress.
 *
 * RETURNS: 1 if the Flash chip is busy
 *          0 if the Flash chip is in standby mode and can accept commands
 */
static Bool flashMemory_isWriteInProgress() {

    // Using dummy value of FF because default state of status register is 00
    Byte rxByte = 0xFF;

    // Read byte from the Status Register (SR)
    slld_RDSRCmd( &rxByte );

    // WIP bit is bit 0
    if ((rxByte & 0x01) == 0)
        return FALSE;
    else
        return TRUE;

}

/******************************************************************************
 *
 * Flash_Data_Read - Read data from the flash memory
 *
 */
static int flashMemory_Data_Read( ) {

# ifdef FlashDebug
    io_out_string("Read Flash memory space:");
# endif

    int      nBytes = 3;
    ADDRESS  flashAddress = 0;
    BYTE     rxBuf[128];
    for ( int i=0; i<nBytes; i++ ) {
        rxBuf[i] = 0xa5;
    }

    SLLD_STATUS sts = SLLD_OK;
    // Issue read command to Flash
    sts = FLASH_READ(0x13,flashAddress,rxBuf,3);

# ifdef FlashDebug
    switch ( sts ) {
    case SLLD_OK: for ( int i=0; i<nBytes; i++ ) {
                      io_out_S32(" %02lx", (S32)rxBuf[i] );
                  }
                  io_out_string("\r\n");
                  break;
    default     : io_out_S32(" FAIL %lx\r\n", sts);
    }
# endif

    return 0;
}

/******************************************************************************
 *
 * Flash_Data_Write - Write data to the flash memory
 *
 */
static int flashMemory_Data_Write( ADDRESS flashAddress, BYTE* txBuf, unsigned int nBytes ) {

# ifdef FlashDebug
    io_out_string("Write Flash memory space (0xb2).");
# endif

//  int      nBytes = 3;
//  ADDRESS  flashAddress = 0;
//  BYTE     txBuf[nBytes];
//  for ( int i=0; i<nBytes; i++ ) {
//      txBuf[i] = 0xb2;
//  }

    // Issue Write Enable command
    //
    slld_WRENCmd();

    // Issue write command to Flash
    //
    SLLD_STATUS sts = FLASH_WRITE( 0x12, flashAddress, txBuf, nBytes );

    DEVSTATUS ds;
    slld_BufferedProgram_4BOp( flashAddress, txBuf, nBytes, &ds );

# ifdef FlashDebug
    switch ( sts ) {
    case SLLD_OK: io_out_S32(" OK %lx\r\n", sts);
                  break;
    default     : io_out_S32(" FAIL %lx\r\n", sts);
    }
# endif

    return 0;
}



/******************************************************************************
 *
 * Flash_Bulk_Erase - Erase all of the flash memory
 *
 */
static int flashMemory_Bulk_Erase( int waitForCompletion ) {

# ifdef FlashDebug
    io_out_string("Erasing flash bulk.\r\n");
# endif

    // Need to issue WRite ENable command before a bulk erase
    slld_WRENCmd();

    // Bulk erase
    SLLD_STATUS sts = slld_BECmd();

# ifdef FlashDebug
    switch ( sts ) {
    case SLLD_OK: io_out_S32(" OK %lx\r\n", sts);
                  break;
    default     : io_out_S32(" FAIL %lx\r\n", sts);
    }


    if ( waitForCompletion ) {
        io_out_string( "Bulk erase" );
        // Wait until erase is complete
        while( flashMemory_isWriteInProgress() ) {
            io_out_string( "." );
            vTaskDelay(500);
        }

        io_out_string( "done.\r\n" );
    }
# else
    if ( waitForCompletion ) {
        // Wait until erase is complete
        while( flashMemory_isWriteInProgress() ) {
            io_out_string( "." );
        }
    }
# endif

    return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////

//  Reserve lowest addressed 512 bytes of Flash Memory
//  for managing data storage.
//
static BYTECOUNT const flashMemory_Reserved = 512;

static ADDRESS   const flashMemory_memorySize = 64L*1024L*1024L;

//  This address is always a multiple of flashMemory_SectorSize
//  It marks the first non-writable address on flash memory.
//  If a write is needed past this address, the sector starting
//  at that address must first be erased.
//
# define flashMemory_SectorSize (256*1024L)
static ADDRESS   flashMemory_noWriteAddress = 0;

static int       flashMemory_writeable = 0;

//  Keep track of FlashMemory content via static variables
//
static int       flashMemory_nOfFrames = 0;
static BYTECOUNT flashMemory_frameSize = 0;

/******************************************************************************
 *
 * Flash_Sector_Erase - Erase a sector of the flash memory
 *
 * Sectors are 256 kBytes in size (flashMemory_SectorSize)
 * Sector erase should take 520 millisec to complete
 * Sector start addresses are 00000000h,00040000h,00080000h,...,03F40000h,03F80000h,03FC0000h
 */
static int flashMemory_Next_Sector_Erase() {

    // Sector erase with 4-byte address
    DEVSTATUS devStatus;
    if ( SLLD_OK == slld_SE_4BOp( flashMemory_noWriteAddress, &devStatus ) ) {
        flashMemory_noWriteAddress += flashMemory_SectorSize;
    }

    io_out_S32 ( "FM ER %lx\r\n", flashMemory_noWriteAddress );

    return 0;
}

//  Generic read/write functions
//
static int flashMemory_write ( ADDRESS fmAddress, BYTE* data, BYTECOUNT size ) {

  if ( fmAddress+size >= flashMemory_memorySize ) {
	  io_out_S32 ( "FM OOM %ld\r\n", fmAddress );
	  io_out_S32 ( "FM OOM %ld\r\n", size );
	  io_out_S32 ( "FM OOM %ld\r\n", fmAddress+size );
	  io_out_S32 ( "FM OOM %ld\r\n", flashMemory_memorySize );
	  return 1;
  }

  if ( fmAddress+size >= flashMemory_noWriteAddress )
  {
    flashMemory_Next_Sector_Erase();
  }

  DEVSTATUS dev_status;

  switch  (slld_BufferedProgram_4BOp ( fmAddress, data, size, &dev_status ) )
  {
  case SLLD_OK:                                    return 0;
  default     : io_out_string( "FM BP FAIL\r\n" ); return 1;
  }
}



static int flashMemory_read ( ADDRESS fmAddress, BYTE* data, BYTECOUNT size ) {

  if ( fmAddress+size >= flashMemory_memorySize ) return 1;

  switch ( slld_Read_4BCmd( fmAddress, data, size ) ) {
  case SLLD_OK: return 0;
  default     : io_out_string ( "FM RD FAIL\r\n" ); return 1;
  }
}

//  API functions
//

int FlashMemory_Init() {

  io_out_S32 ( "FM SZ %lx\r\n", flashMemory_memorySize );

  // Clear Write Protect line
  //
  // This line can stay clear throughout.
  // While technically useable to protect against unintended writes,
  // there is no practical reason.
  // All write commands would clear that line anyways,
  // and there is no flach memory access from outside this module.
  //
  gpio_clr_gpio_pin(FLASH_PRT_N);

  //  Initially, open up 4 sectors (4 x 256 kB) for writing
  flashMemory_Next_Sector_Erase();
  flashMemory_Next_Sector_Erase();
  flashMemory_Next_Sector_Erase();
  flashMemory_Next_Sector_Erase();

  flashMemory_writeable = 1;

  return 0;
}

int FlashMemory_IsEmpty( void     ) {
  
  return flashMemory_nOfFrames == 0;
}

int FlashMemory_nOfFrames( void     ) {
  
  return flashMemory_nOfFrames;
}

//  FlashMemory_AddFrame - Treat Flash Memory as a stack,
//  add frame to top of stack, and
//  incrementing the static variable frame counter.
//
int FlashMemory_AddFrame ( Spectrometer_Data_t* frame )
{ 

  //  Initialization <=> Erasing low-memory sectors
  //
  if ( !flashMemory_writeable )
    FlashMemory_Init();

  //  Sanity checks
  //
  BYTECOUNT const nBytes = sizeof(Spectrometer_Data_t);

  if ( flashMemory_nOfFrames == 0 )
  {
    flashMemory_frameSize = nBytes;
  }
  
  else if ( nBytes != flashMemory_frameSize )
  {
    return 2;  //  Error: On-flash frame size differs from current frame size
  }

  //  Location where to store on Flash Memory
  //
  ADDRESS const flashAddress = flashMemory_Reserved  +  flashMemory_nOfFrames * flashMemory_frameSize;

  //  TODO  --  Make sure the required ADDRESS+BYTECOUNT are inside the
  //            sectors that were erased during FlashMemory_Init().
  //            Otherwise, erase another sector to make more memory avaialable.

  if   (flashMemory_write ( flashAddress, (BYTE*)frame, nBytes ) )
    return 1;

  //  Update frame counter
  //
  flashMemory_nOfFrames ++;

  return 0;
}

int FlashMemory_RetrieveFrame( Spectrometer_Data_t* frame ) {

  BYTECOUNT const nBytes = sizeof(Spectrometer_Data_t);

  if ( flashMemory_nOfFrames == 0 ) {
    return 3;  //  Error: No frame in flash memory
  } else if ( nBytes != flashMemory_frameSize ) {
    return 2;  //  Error: On-flash frame size differs from current frame size
  }

  //  Location from where to retrieve Flash Memory
  //
  ADDRESS const offset = flashMemory_Reserved + (flashMemory_nOfFrames-1)*flashMemory_frameSize;

  if ( flashMemory_read ( offset, (BYTE*)frame, nBytes ) ) return 1;

  //  Cannot re-write already uses location.
  //  Update frame counter
  //
  flashMemory_writeable = 0;
  flashMemory_nOfFrames --;

  if ( 0 == flashMemory_nOfFrames ) {
    flashMemory_frameSize = 0;
    flashMemory_noWriteAddress = 0;  //  Force erase after one-time use
  }

  return 0;
}


# include "io_funcs.spectrometer.h"

int FlashMemory_Test( char* option, char* value ) { 

    ADDRESS    flashAddress = 0x00;
# define DBLCK 128
    BYTE       rxBuf[DBLCK];
    BYTE       txBuf[DBLCK];
    BYTECOUNT  nBytes       = 8;
    BYTECOUNT  i;
    SLLD_STATUS sts = SLLD_OK;

    Bool sectorErase = FALSE;
    Bool bulkErase = FALSE;
    Bool dataWrite = FALSE;
    Bool dataRead = FALSE;
    Bool readStatus = FALSE;

    if ( 0 == strcasecmp("sectorerase", option) ) {
        sectorErase = true;
    } else if ( 0 == strcasecmp("bulkerase", option) ) {
        bulkErase = true;
    } else if ( 0 == strcasecmp("write", option) ) {
        dataWrite = true;
    } else if ( 0 == strcasecmp("read", option) ) {
        dataRead = true;
    } else if ( 0 == strcasecmp("status", option) ) {
        readStatus = true;
    } else {
        io_out_string("Invalid option\r\n");
        return CEC_Failed;
    }

    if (readStatus) {
        // Read Status Register (SR)
        io_out_string("Status Register (SR):");
        // Fill the RX buffer with bogus bytes
        nBytes       = 1;
        for ( i=0; i<nBytes; i++ ) {
                    rxBuf[i] = 0xa5;
        }
        // Read nBytes bytes from the Status Register (SR)
        sts = slld_RDSRCmd( rxBuf );
        switch ( sts ) {
        case SLLD_OK: for ( i=0; i<nBytes; i++ ) {
                    io_out_S32(" %02lx", (S32)rxBuf[i] );
                  }
                  io_out_string("\r\n");
                  break;
        default     : io_out_S32(" FAIL %lx\r\n", sts);
        }

        // Read Configuration Register (CR)
        io_out_string("Configuration Register (CR):");
        // Fill the RX buffer with bogus bytes
        nBytes       = 1;
        for ( i=0; i<nBytes; i++ ) {
                    rxBuf[i] = 0xa5;
        }
        // Read nBytes bytes from the flash memory
        sts = slld_RCRCmd( rxBuf );
        switch ( sts ) {
        case SLLD_OK: for ( i=0; i<nBytes; i++ ) {
                    io_out_S32(" %02lx", (S32)rxBuf[i] );
                  }
                  io_out_string("\r\n");
                  break;
        default     : io_out_S32(" FAIL %lx\r\n", sts);
        }

        // Read Advanced Sector Protection (ASP) register
        io_out_string("Advanced Sector Protection Register (ASP):");
        nBytes       = 2;
        for ( i=0; i<nBytes; i++ ) {
                    rxBuf[i] = 0xa5;
        }
        sts = slld_RASPCmd( (WORD*)rxBuf );
        switch ( sts ) {
        case SLLD_OK: for ( i=0; i<nBytes; i++ ) {
                    io_out_S32(" %02lx", (S32)rxBuf[i] );
                  }
                  io_out_string("\r\n");
                  break;
        default     : io_out_S32(" FAIL %lx\r\n", sts);
        }
                  io_out_string("\r\n");
    }

    if (sectorErase)
        flashMemory_Next_Sector_Erase();
    if (bulkErase)
        flashMemory_Bulk_Erase( 1 );
    if (dataWrite) {
        ADDRESS   fa = 0;
		BYTE      tx = 0x5a;
		BYTECOUNT nb = 1;
		flashMemory_Data_Write( fa, tx, nb );
	}
    if (dataRead)
        flashMemory_Data_Read();

# if 0
    // Read Flash memory space
    io_out_string("Read Flash memory space:");
    nBytes    = 1;
    BYTE prev = 0;
    int nSame = 0;
    for ( flashAddress=0; flashAddress<(64) /**1024*1024)*/; flashAddress += nBytes ) {
      for ( i=0; i<nBytes; i++ ) {
                    rxBuf[i] = 0xa5;
      }
	  // Write a Read Command to Flash Device and read data using 4-bytes addressing scheme
      sts = slld_Read_4BCmd( flashAddress, rxBuf, nBytes );
      switch ( sts ) {
      case SLLD_OK:
		    if ( prev == rxBuf[0] ) {
                      nSame++;
		    } else {
                      if ( nSame ) {
                        io_out_S32(" [ %02lx", prev );
                        io_out_S32(" x %ld ]", 1+nSame );
		      } else {
                        io_out_S32(" %02lx", prev );
		      }
		      nSame = 0;
		      prev = rxBuf[0];
		    }
                    break;
    default     : io_out_S32(" FAIL %lx\r\n", sts);
      }
    }
# endif

    return 0;
}

