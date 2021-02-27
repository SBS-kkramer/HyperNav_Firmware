/*! \file diskio_mci_df.c *************************************************************
 *
 * \brief FatFs port layer. Targeted to plattform with an eMMC and a DATA-Flash.
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2011-05-05
 *
 *      @revised
 *      2011-11-19 - DAS - Implemented eMMC access through memory abstraction layer
 *      				   to take advantage of its access control (mutex) implementation.
 *
 **********************************************************************************/

#include "sd_mmc_mci.h"
#include "sd_mmc_mci_mem.h"
#include "board.h"
#include "ds3234.h"
#include "gpio.h"
#include "ffconf.h"
#include "avr32rerrno.h"
#include "string.h"

#include "ctrl_access.h"
#include "diskio.h"

/* Volume numbering */
#define EMMC_DRV		0	/* eMMC drive */


/* Card type flags (CardType) */
#define CT_MMC		0x01		/* MMC ver 3 */
#define CT_SD1		0x02		/* SD ver 1 */
#define CT_SD2		0x04		/* SD ver 2 */
#define CT_SDC		(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK	0x08		/* Block addressing */


//*****************************************************************************
// Local Functions Declarations
//*****************************************************************************

//eMMC peripheral resources init
static void eMMCInit(void);

//*****************************************************************************
// Local Variables
//*****************************************************************************
static Bool driveInitialized[_VOLUMES] = {FALSE};
BYTE CardType = CT_MMC;								/* b0:MMC, b1:SDv1, b2:SDv2, b3:Block addressing */

// eMMC sector buffer
#if (defined __GNUC__) && (defined __AVR32__)
  __attribute__((__aligned__(4)))
#elif (defined __ICCAVR32__)
  #pragma data_alignment = 4
#endif
static BYTE eMMC_sectorBuffer[_MAX_SS];			   // A 4 bytes aligned buffer is needed for the MCI sector R/W functions


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

DSTATUS disk_initialize (BYTE drive)
{
	// eMMC
	if(drive == EMMC_DRV)
	{
		// Initialize eMMC resources
		eMMCInit();

		// Initialize card
		if(!sd_mmc_mci_card_init(EMMC_SLOT)) {
			avr32rerrno = EMMCNRDY;
			return STA_NOINIT;
		}

		// eMMC will be handled through memory access abstraction layer
		if(!ctrl_access_init())
			return STA_NOINIT;

		driveInitialized[drive] = TRUE;
	}

	// Initialization succeeded
	return 0;
}

DSTATUS disk_status (BYTE drive)
{
	// NOTE: Does not check for presence or write protection

	if(!driveInitialized[drive])
		return STA_NOINIT;
	else
		return 0;
}

DRESULT disk_read (
	BYTE drv,			/* Physical drive number (0) */
	BYTE *buff,			/* Pointer to the data buffer to store read data */
	DWORD sector,		/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..128) */
)
{
	DSTATUS s;

	s = disk_status(drv);
	if (s & STA_NOINIT) return RES_NOTRDY;
	if (!count) return RES_PARERR;


	//eMMC
	if(drv == EMMC_DRV)
	{
		while(count > 0)
		{
			// Single block read. Read data in aligned sector buffer
			if(	memory_2_ram(LUN_ID_SD_MMC_MCI_0_MEM, sector+count-1, eMMC_sectorBuffer) != CTRL_GOOD )
				break;

			// Copy data to destination buffer
			memcpy(buff+(count-1)*_MAX_SS, eMMC_sectorBuffer, _MAX_SS);

			count--;
		}
		if(count == 0)
			return RES_OK;
	}

	return RES_ERROR;
}

#if	_READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive number (0) */
	const BYTE *buff,	/* Pointer to the data to be written */
	DWORD sector,		/* Start sector number (LBA) */
	BYTE count			/* Sector count (1..128) */
)
{
	DSTATUS s;

	s = disk_status(drv);
	if (s & STA_NOINIT) return RES_NOTRDY;
	if (!count) return RES_PARERR;


	//eMMC
	if(drv == EMMC_DRV)
	{
		while(count > 0)
		{
			// Copy data to aligned sector buffer
			memcpy(eMMC_sectorBuffer, buff+(count-1)*_MAX_SS, _MAX_SS);

			// Single block write
			if (ram_2_memory(LUN_ID_SD_MMC_MCI_0_MEM, sector+count-1, eMMC_sectorBuffer) != CTRL_GOOD)
				break;

			count--;
		}
		if(count == 0)
			return RES_OK;
	}

	return RES_ERROR;

}
#endif

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;
	DWORD u_32;


	if (disk_status(drv) & STA_NOINIT)					/* Check if card is in the socket */
		return RES_NOTRDY;

	res = RES_ERROR;


	// eMMC
	if( drv == EMMC_DRV)
	{
		switch (ctrl)
		{
		case CTRL_SYNC :		//TODO DIEGO: /* Make sure that there is no pending write process */
			res = RES_OK;
			break;

		case GET_SECTOR_COUNT :	/* Get number of sectors on the disk (DWORD) */
			if(mem_read_capacity(LUN_ID_SD_MMC_MCI_0_MEM, &u_32) == CTRL_GOOD)
			{
				*(DWORD*)buff = u_32;
				res = RES_OK;
			}
			break;

		case GET_BLOCK_SIZE :	/* Get erase block size in unit of sector (DWORD) */
			*(DWORD*)buff = 1;	// Unknown block size
			res = RES_OK;
			break;

#if _USE_ERASE == 1
#error "Sector erase not implemented!"
		case CTRL_ERASE_SECTOR :	// Erases part of the flash memory specified by a DWORD array
			// (start sector,end sector) pointed by buffer
			res = RES_OK;
			break;
#endif
		default:
			res = RES_PARERR;
		}
	}


	return res;
}

/* RTC function */
#if !_FS_READONLY
DWORD get_fattime (void)
{
	U16 year;
	U8 month, date, day, hour, minutes, seconds;
	DWORD ret = 0;

	if(ds3234_getTime2(&year, &month, &date,&day, &hour, &minutes, &seconds) != DS3234_FAIL)
	{
		ret |= (year - 1980) << 25;
		ret |= month << 21;
		ret |= date << 16;
		ret |= hour << 11;
		ret |= minutes << 5;
		ret |= (seconds/2);
	}

	return ret;
}
#endif

//*****************************************************************************
// Local Functions Implementations
//*****************************************************************************

//!\brief Initialize MMC resources
static void eMMCInit(void)
{
	static const gpio_map_t eMMCMap = EMMC_PINS_CFG;

	// MCI options.
	static const mci_options_t mciOptions =
	{
			.card_speed = EMMC_SPEED,
			.card_slot  = EMMC_SLOT,
	};

	// Assign I/Os to MCI.
	gpio_enable_module(eMMCMap, sizeof(eMMCMap) / sizeof(eMMCMap[0]));

#ifdef SD_CARD_DETECT_PU
	// Enable pull-up for Card Detect.
	gpio_enable_pin_pull_up(SD_SLOT_8BITS_CARD_DETECT);
#endif

#ifdef SD_CARD_WP_PU
	// Enable pull-up for Write Protect.
	gpio_enable_pin_pull_up(SD_SLOT_8BITS_WRITE_PROTECT);
#endif

	// De-assert eMMC reset line
#ifdef EMMC_NRST
	gpio_set_gpio_pin(EMMC_NRST);
#endif

	// Initialize SD/MMC with MCI PB clock.
	sd_mmc_mci_init(&mciOptions, FOSC0, FOSC0);

}
