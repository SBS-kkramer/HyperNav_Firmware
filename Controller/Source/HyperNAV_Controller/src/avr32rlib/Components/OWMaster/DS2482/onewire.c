/*! \file onewire.c *************************************************************
 *
 * \brief 1-Wire Master Driver.
 *
 * Adapted from MAXIM's AN3684 - "How to Use the DS2482 I²C 1-Wire® Master"
 * & from MAXIM's AN187 - "1-Wire Search Algorithm",
 * & from MAXIM's AN27 - "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2010-11-18
 *
 *
 **********************************************************************************/
#include "compiler.h"

#include "i2c.h"
#include "onewire.h"
#include "ds2482_cfg.h"


#ifndef NO_OW

//*****************************************************************************
// Configuration
//*****************************************************************************

// OW Busy Polling retries
#define	POLL_LIMIT	100


//--1-Wire bus master bitmasks--

// Status register
#define STATUS_1WB	0x01
#define STATUS_PPD	0x02
#define STATUS_SD	0x04
#define STATUS_SBR	0x20
#define STATUS_TSB	0x40
#define STATUS_DIR	0x80

// Configuration register
#define CONFIG_APU	0x01
#define CONFIG_SPU	0x04
#define CONFIG_1WS	0x08


//--DS2482 1-Wire bus master commands--

//Device reset
#define CMD_DRST	0xF0
//Set read pointer
#define CMD_SRP		0xE1
//Write configuration
#define CMD_WCFG	0xD2
//1-Wire bus reset
#define CMD_1WRS	0xB4
//1-Wire single bit
#define CMD_1WSB	0x87
//1-Wire Write Byte
#define CMD_1WWB	0xA5
//1-Wire Read Byte
#define CMD_1WRB	0x96
//1-Wire Triplet
#define CMD_1WT		0x78



//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************

// 1-Wire bus master
static Bool DS2482_detect(U8 addr);
static Bool DS2482_reset(void);
static Bool DS2482_write_config(U8 config);
static U8 DS2482_search_triplet(U8 search_direction);


// 1-Wire helpers
static S16 OWTouchBit(Bool sendbit, Bool *rcvbit);
static Bool OWSearch(U8* ROMno);
static U8 calc_crc8(U8 value);


//*****************************************************************************
// Local Variables
//*****************************************************************************

//--DS2482 state--

// I2C adress
static U8 DS2482Address;
// Short detected flag
static Bool short_detected;
// Config register (Default configuration = APU enabled, Standard 1-Wire speed, No Strong Pull-Up)
#define DS2482_DFLT_CFG	CONFIG_APU
static U8 DS2482Config = DS2482_DFLT_CFG;

//--OW device search algorithm--

// Search state
static int LastDiscrepancy;
static int LastFamilyDiscrepancy;
static int LastDeviceFlag;
static U8 crc8;


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************



/* \brief Initialize One-Wire bus access
 * @note This function MUST be called once before calling any other of this module
 * @param	fCPU	CPU frequency in Hz
 * @return OW_OK: Success	OW_FAIL: Failed to initialize
 */
S16 ow_init(U32 fCPU)
{
	// Init I2C for 1-Wire bus master
	I2C_init(fCPU);

	// Detect and configure bus master device
	if(!DS2482_detect(DS2482_ADDRESS))
		return OW_FAIL;

	return OW_OK;
}


//!\brief Reset all of the devices on the 1-Wire Net and return the result.
//!	@param	PPD	OUTPUT:	If TRUE presence pulse(s)were detected, else presence pulse(s)were not detected.
//! @return OW_OK:  Success
//! @return OW_FAIL: Failure while trying to reset the 1-Wire bus
//! @return	OW_BUSY: 1-Wire bus is busy, cannot reset, try again later.
S16 ow_reset(Bool* PPD)
{
	U8 status;
	U32 poll_count = 0;
	Bool waitForOWBusy = TRUE;

	// 1-Wire reset (Case B)
	//   S AD,0 [A] 1WRS [A] Sr AD,1 [A] [Status] A [Status] A\ P
	//                                   \--------/
	//                       Repeat until 1WB bit has changed to 0
	//  [] indicates from slave
	I2C_start();
	I2C_write(DS2482Address | I2C_WRITE, TRUE);
	// Handle Case C
	if(!I2C_write(CMD_1WRS, TRUE))
	{
		I2C_stop();
		return OW_BUSY;
	}
	I2C_rep_start();
	I2C_write(DS2482Address | I2C_READ, TRUE);

	// loop checking 1WB bit for completion of 1-Wire operation
	// abort if poll limit reached
	status = I2C_read(ACK);
	do
	{
		waitForOWBusy = (status & STATUS_1WB) && (poll_count++ < POLL_LIMIT);
		status = I2C_read(waitForOWBusy);
	}
	while (waitForOWBusy);

	I2C_stop();

	// check for failure due to poll limit reached
	if (poll_count >= POLL_LIMIT)
	{
		// Handle error
		// Reset bus master and flag error
		DS2482_reset();
		DS2482_write_config(DS2482_DFLT_CFG);
		return OW_FAIL;
	}

	// check for short condition
	if (status & STATUS_SD)
		short_detected = TRUE;
	else
		short_detected = FALSE;

	// check for presence detect
	*PPD = (status & STATUS_PPD);

	return OW_OK;
}




//! \brief Send 1 bit of communication to the 1-Wire Net.
S16 ow_writeBit(Bool sendbit)
{
   return OWTouchBit(sendbit,NULL);
}




//! \brief Reads 1 bit of communication from the 1-Wire Net
S16 ow_readBit(Bool* bit)
{
   return OWTouchBit(TRUE,bit);
}




//! \brief Send 8 bits of communication to the 1-Wire Net
S16 ow_writeByte(U8 sendbyte)
{
   U8 status;
   Bool waitForOWBusy = TRUE;
   int poll_count = 0;

   // 1-Wire Write Byte (Case B)
   //   S AD,0 [A] 1WWB [A] DD [A] Sr AD,1 [A] [Status] A [Status] A\ P
   //                                          \--------/
   //                             Repeat until 1WB bit has changed to 0
   //  [] indicates from slave
   //  DD data to write

   I2C_start();
   I2C_write(DS2482Address | I2C_WRITE, TRUE);
   // Handle Case C (see DS2482 datasheet)
   if(!I2C_write(CMD_1WWB, TRUE))
   {
	   I2C_stop();
	   return OW_BUSY;
   }
   I2C_write(sendbyte, TRUE);
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_READ, TRUE);

   // loop checking 1WB bit for completion of 1-Wire operation
   // abort if poll limit reached
   status = I2C_read(ACK);
   do
   {
	   waitForOWBusy = (status & STATUS_1WB) && (poll_count++ < POLL_LIMIT);
	   status = I2C_read(waitForOWBusy);
   }
   while (waitForOWBusy);

   I2C_stop();

   // check for failure due to poll limit reached
   if (poll_count >= POLL_LIMIT)
   {
      // Handle error
      DS2482_reset();
      DS2482_write_config(DS2482_DFLT_CFG);
      return OW_FAIL;
   }

   return OW_OK;
}



//! \brief Read a byte from the 1-Wire network
S16 ow_readByte(U8* rcv)
{
   U8 status;
   Bool waitForOWBusy = TRUE;
   int poll_count = 0;

   /* 1-Wire Read Bytes (Case C)
   //   S AD,0 [A] 1WRB [A] Sr AD,1 [A] [Status] A [Status] A\
   //                                   \--------/
   //                     Repeat until 1WB bit has changed to 0
   //   Sr AD,0 [A] SRP [A] E1 [A] Sr AD,1 [A] DD A\ P
   //
   //  [] indicates from slave
   //  DD data read
   */
   I2C_start();
   I2C_write(DS2482Address | I2C_WRITE, TRUE);
   // Handle Case D (see DS2482 datasheet)
   if(!I2C_write(CMD_1WRB, TRUE))
   {
	   I2C_stop();
	   return OW_BUSY;
   }
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_READ, TRUE);

   // loop checking 1WB bit for completion of 1-Wire operation
   // abort if poll limit reached
   status = I2C_read(ACK);
   do
   {
	   waitForOWBusy = (status & STATUS_1WB) && (poll_count++ < POLL_LIMIT);
	   status = I2C_read(waitForOWBusy);
   }
   while(waitForOWBusy);

   // check for failure due to poll limit reached
   if (poll_count >= POLL_LIMIT)
   {
      // handle error
	  I2C_stop();
      DS2482_reset();
      DS2482_write_config(DS2482_DFLT_CFG);
      return OW_FAIL;
   }

   // Read data
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_WRITE, TRUE);
   I2C_write(CMD_SRP, TRUE);
   I2C_write(0xE1, TRUE);
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_READ, TRUE);
   *rcv =  I2C_read(NACK);
   I2C_stop();

   return OW_OK;
}


//! \brief Send 8 bits of communication to the 1-Wire Net and enables 'Strong Pull-Up (SPU)'
//! @note This function must be used to send the last byte that will put a 1-Wire slave
//! into a state where it will need extra power from the bus. The strong pull up condition will
//! persist until the next bit/byte transfer, or until explicitly disabled through 'ow_disableSPU()'
//! @param sendbyte	Byte to send
//!	@return OW_OK: Byte sent sucessfully
//!	@return OW_BUSY: OneWire bus is busy cannot send, try again later.
//! @return OW_FAIL: Error while sending byte
S16 ow_writeByteSPU(U8 sendbyte)
{
	// Set SPU bit in configuration register
	DS2482Config |= CONFIG_SPU;

	// Write new configuration
	if(!DS2482_write_config(DS2482Config))
		return OW_FAIL;

	// Write byte
	return ow_writeByte(sendbyte);
}



//! \brief Disable Srong Pull-Up
//!	@return OW_OK: SPU doisabled
//! @return OW_FAIL: Error while disabling
S16 ow_disableSPU(void)
{
	// Set SPU bit in configuration register
	DS2482Config |= CONFIG_SPU;

	// Write new configuration
	if(!DS2482_write_config(DS2482Config))
		return OW_FAIL;
	else
		return OW_OK;
}




/*! \brief Find the 'first' device on the 1-Wire network
 * @param	ROMno	OUTPUT: ROM number of the device found (if found).
 * @return	TRUE: Device found. Check 'ROMno' for ROM number.
 * @return 	FALSE: No device present.
 */
Bool ow_first(U64* ROMno)
{
    // reset the search state
   LastDiscrepancy = 0;
   LastDeviceFlag = FALSE;
   LastFamilyDiscrepancy = 0;

   // Call OW search. 'ROMno' is casted as byte pointer.
   return OWSearch((U8*)ROMno);
}


/*! Find the 'next' devices on the 1-Wire network
 * @param	ROMno	OUTPUT: ROM number of the device found (if found).
 * @return	TRUE: Device found. Check 'ROMno' for ROM number.
 * @return	FALSE: Device not found, end of search
 */
Bool ow_next(U64* ROMno)
{
	// leave the search state alone

	// Call OW search. 'ROMno' is casted as byte pointer.
	return OWSearch((U8*)ROMno);
}





//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************


//*****************************************************************************
// 1-Wire Helpers
//*****************************************************************************

/* \brief Search device on 1-Wire network
 *
 * The 'OWSearch' function does a general search. This function
 * continues from the previous search state. The search state
 * can be reset by using the 'ow_first' function.
 * This function contains one parameter 'alarm_only'.
 * When 'alarm_only' is TRUE (1) the find alarm command
 * 0xEC is sent instead of the normal search command 0xF0.
 * Using the find alarm command 0xEC will limit the search to only
 * 1-Wire devices that are in an 'alarm' state.
 *
 * @return   TRUE:    when a 1-Wire device was found and its
 *                    Serial Number placed in the global ROM
 *
 * @return   FALSE:   when no new device was found.  Either the
 *                    last search was the last device or there
 *                    are no devices on the 1-Wire Net.
 */
static Bool OWSearch(U8* ROM_NO)
{
   int id_bit_number;
   int last_zero, rom_byte_number;
   Bool search_result;
   int id_bit, cmp_id_bit;
   U8 rom_byte_mask, search_direction, status;
   S16 owReset;
   Bool PPD = FALSE;

   // initialize for search
   id_bit_number = 1;
   last_zero = 0;
   rom_byte_number = 0;
   rom_byte_mask = 1;
   search_result = FALSE;
   crc8 = 0;

   // if the last call was not the last one
   if (!LastDeviceFlag)
   {
      // 1-Wire reset
	  owReset = ow_reset(&PPD);
      if (owReset != OW_OK || !PPD)
      {
         // reset the search
         LastDiscrepancy = 0;
         LastDeviceFlag = FALSE;
         LastFamilyDiscrepancy = 0;
         return FALSE;
      }

      // issue the search command
      ow_writeByte(0xF0);

      // loop to do the search
      do
      {
         // if this discrepancy if before the Last Discrepancy
         // on a previous next then pick the same as last time
         if (id_bit_number < LastDiscrepancy)
         {
            if ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
               search_direction = 1;
            else
               search_direction = 0;
         }
         else
         {
            // if equal to last pick 1, if not then pick 0
            if (id_bit_number == LastDiscrepancy)
               search_direction = 1;
            else
               search_direction = 0;
         }

         // Perform a triple operation on the DS2482 which will perform
         // 2 read bits and 1 write bit
         status = DS2482_search_triplet(search_direction);

         // check bit results in status byte
         id_bit = ((status & STATUS_SBR) == STATUS_SBR);
         cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
         search_direction =	((status & STATUS_DIR) == STATUS_DIR) ? (U8)1 : (U8)0;

         // check for no devices on 1-Wire
         if ((id_bit) && (cmp_id_bit))
            break;
         else
         {
            if ((!id_bit) && (!cmp_id_bit) && (search_direction == 0))
            {
               last_zero = id_bit_number;

               // check for Last discrepancy in family
               if (last_zero < 9)
                  LastFamilyDiscrepancy = last_zero;
            }

            // set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1)
               ROM_NO[rom_byte_number] |= rom_byte_mask;
            else
               ROM_NO[rom_byte_number] &= (U8)~rom_byte_mask;

            // increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;

            // if the mask is 0 then go to new SerialNum byte rom_byte_number
            // and reset mask
            if (rom_byte_mask == 0)
            {
               calc_crc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
               rom_byte_number++;
               rom_byte_mask = 1;
            }
         }
      }
      while(rom_byte_number < 8);  // loop until through all ROM bytes 0-7

      // if the search was successful then
      if (!((id_bit_number < 65) || (crc8 != 0)))
      {
         // search successful so set LastDiscrepancy,LastDeviceFlag
         // search_result
         LastDiscrepancy = last_zero;

         // check for last device
         if (LastDiscrepancy == 0)
            LastDeviceFlag = TRUE;

         search_result = TRUE;
      }
   } // end - if (!LastDeviceFlag)

   // if no device found then reset counters so next
   // 'search' will be like a first

   if (!search_result || (ROM_NO[0] == 0))
   {
      LastDiscrepancy = 0;
      LastDeviceFlag = FALSE;
      LastFamilyDiscrepancy = 0;
      search_result = FALSE;
   }

   return search_result;
}


//! \brief Send 1 bit of communication to the 1-Wire Net and return the result 1 bit read from the 1-Wire Net.
//!  The parameter 'sendbit'least significant bit is used and the least significant bit
//! of the result is the return bit.
//! @param sendbit The least significant bit is the bit to send
//! @param rcvbit	OUTPUT: received bit. (if NULL received bit is not output).
//! @return OW_OK:Sucess, OW_FAIL: Failure, OW_BUSY: OneWire bus is busy. Try again later.
static S16 OWTouchBit(Bool sendbit, Bool *rcvbit)
{
   U8 status;
   Bool waitForOWBusy = TRUE;
   int poll_count = 0;

   // 1-Wire bit (Case B)
   //   S AD,0 [A] 1WSB [A] BB [A] Sr AD,1 [A] [Status] A [Status] A\ P
   //                                          \--------/
   //                           Repeat until 1WB bit has changed to 0
   //  [] indicates from slave
   //  BB indicates byte containing bit value in msbit
   I2C_start();
   I2C_write(DS2482Address | I2C_WRITE, TRUE);
   I2C_write(CMD_1WSB, TRUE);
   I2C_write(sendbit ? 0x80 : 0x00, TRUE);
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_READ, TRUE);

   // loop checking 1WB bit for completion of 1-Wire operation
   // abort if poll limit reached
   status = I2C_read(ACK);
   do
   {
	   waitForOWBusy = (status & STATUS_1WB) && (poll_count++ < POLL_LIMIT);
       status = I2C_read(waitForOWBusy);
   }
   while (waitForOWBusy);

   I2C_stop();

   // check for failure due to poll limit reached
   if (poll_count >= POLL_LIMIT)
   {
      // Handle error
      DS2482_reset();
      DS2482_write_config(DS2482Config);
      return OW_FAIL;
   }

   // return bit state
   if(rcvbit != NULL)
	   *rcvbit = (status & STATUS_SBR);
   return OW_OK;
}


/*! \brief Update CRC
 *
 * Calculate the CRC8 of the byte value provided with the current global 'crc8' value.
 * @return Current global crc8 value
 */

static U8 dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};


static U8 calc_crc8(U8 value)
{
   crc8 = dscrc_table[crc8 ^ value];
   return crc8;
}





//*****************************************************************************
// 1-Wire Bus Master API
//*****************************************************************************


//! \brief DS2428 Detect routine
//! Sets the I2C address and then performs a device reset followed by writing the
//! configuration byte to default values:
//!
//! - 1-Wire speed (c1WS) = standard (0)
//! - Strong pullup (cSPU) = off (0)
//! - Presence pulse masking (cPPM) = off (0)
//! - Active pullup (cAPU) = on (CONFIG_APU = 0x01)
//!
//!	@param	addr	I2C address of OW bus master
//! @return TRUE if device was detected and written.
//! @return FALSE device not detected or failure to write configuration byte
static Bool DS2482_detect(U8 addr)
{
	// set global address
	DS2482Address = addr;
	// reset the DS2482 ON selected address
	if (!DS2482_reset())
		return FALSE;
	// write the default configuration setup
	if (!DS2482_write_config(DS2482Config))
		return FALSE;
	return TRUE;
}


//! \brief Perform a device reset on the DS2482
//!
//!	@return TRUE if device was reset
//! @return FALSE device not detected or failure to perform reset
static Bool DS2482_reset(void)
{
	U8 status;

	// Device Reset
	// S AD,0 [A] DRST [A] Sr AD,1 [A] [SS] A\ P
	// [] indicates from slave
	// SS status byte to read to verify state
	I2C_start();
	I2C_write(DS2482Address | I2C_WRITE, TRUE);
	I2C_write(CMD_DRST, TRUE);
	I2C_rep_start();
	I2C_write(DS2482Address | I2C_READ, TRUE);
	status = I2C_read(NACK);
	I2C_stop();

	// check for failure due to incorrect read back of status
	return ((status & 0xF7) == 0x10);
}


//! \ brief Write the configuration register in the DS2482.
//! @param config	The configuration options are provided in the lower nibble of the provided config byte.
//! 				The uppper nibble in bitwise inverted when written to the DS2482.
//! @return TRUE: config written and response correct
//! @return FALSE: response incorrect
static Bool DS2482_write_config(U8 config)
{
	U8 read_config;

	// Write configuration (Case A)
	// S AD,0 [A] WCFG [A] CF [A] Sr AD,1 [A] [CF] A\ P
	// [] indicates from slave
	// CF configuration byte to write
	I2C_start();
	I2C_write(DS2482Address | I2C_WRITE, TRUE);
	I2C_write(CMD_WCFG, TRUE);
	I2C_write(config | (~config << 4), TRUE);
	I2C_rep_start();
	I2C_write(DS2482Address | I2C_READ, TRUE);
	read_config = I2C_read(NACK);
	I2C_stop();
	// check for failure due to incorrect read back
	if (config != read_config)
	{
		//handle error
		DS2482_reset();
		return FALSE;
	}
	return TRUE;
}


//! \brief Use the DS2482 help command '1-Wire triplet' to perform one bit of a 1-Wire search.
//!This command does two read bits and one write bit. The write bit
//! is either the default direction (all device have same bit) or in case of
//! a discrepancy, the 'search_direction' parameter is used.
//! Returns – The DS2482 status byte result from the triplet command.
//! @note It is currently returning 0 when error.
//! @note It doesnt handle case C (1-Wire busy)
static U8 DS2482_search_triplet(U8 search_direction)
{
   U8 status;
   int poll_count = 0;

   // 1-Wire Triplet (Case B)
   //   S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
   //                                         \--------/
   //                           Repeat until 1WB bit has changed to 0
   //  [] indicates from slave
   //  SS indicates byte containing search direction bit value in msbit
   I2C_start();
   I2C_write(DS2482Address | I2C_WRITE, TRUE);
   I2C_write(CMD_1WT, TRUE);
   I2C_write(search_direction ? 0x80 : 0x00, TRUE);
   I2C_rep_start();
   I2C_write(DS2482Address | I2C_READ, TRUE);

   // loop checking 1WB bit for completion of 1-Wire operation
   // abort if poll limit reached
   status = I2C_read(ACK);
   do
   {
      status = I2C_read(status & STATUS_1WB);
   }
   while ((status & STATUS_1WB) && (poll_count++ < POLL_LIMIT));

   I2C_stop();

   // check for failure due to poll limit reached
   if (poll_count >= POLL_LIMIT)
   {
      // handle error
      DS2482_reset();
      return 0;
   }

   // return status byte
   return status;
}



#else
#warning "No OneWire support!"

//=============================================================================
// Stubs to compile for EVK
//=============================================================================

/* \brief Initialize One-Wire bus access
 */
S16 ow_init(U32 fCPU)
{
	return OW_OK;
}

//!\brief Reset all of the devices on the 1-Wire Net and return the result.

S16 ow_reset(Bool* PPD)
{
	*PPD = FALSE;
	return OW_OK;
}


/*! \brief Find the 'first' device on the 1-Wire network
 */
Bool ow_first(U64* ROMno)
{
	return FALSE;
}


/*! Find the 'next' devices on the 1-Wire network
 */
Bool ow_next(U64* ROMno)
{
	return FALSE;
}


/* \brief Send 1 bit of communication to the 1-Wire Net.
*/
S16 ow_writeBit(Bool sendbit)
{
	return OW_OK;
}



/*! \brief Reads 1 bit of communication from the 1-Wire Net
*/
S16 ow_readBit(Bool* bit)
{
	return OW_OK;
}



//! \brief Send 8 bits of communication to the 1-Wire Net
S16 ow_writeByte(U8 sendbyte)
{
	return OW_OK;
}




//! \brief Read a byte from the 1-Wire network
S16 ow_readByte(U8* rcv)
{
	return OW_OK;
}


#endif
