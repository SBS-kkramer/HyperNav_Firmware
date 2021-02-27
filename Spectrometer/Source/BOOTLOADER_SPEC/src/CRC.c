/*
 * CRC.c
 *
 *  Created on: Dec 9, 2010
 *      Author: Scott Feener
 *
 *      CRC-32 functions.
 *
 *      Adapted from code found at http://www.lammertbies.nl/download/lib_crc.zip
 *      (original file info:
 *      	Library         : lib_crc
 *   		File            : lib_crc.c
 *			Author          : Lammert Bies  1999-2008
 *   		E-mail          : info@lammertbies.nl
 *   		Language        : ANSI C
 *   	)
 *
 *      Support for the CRC routines in this library can be found in the
 *      Error Detection and Correction forum at http://www.lammertbies.nl/forum/viewforum.php?f=11
 */

#include "compiler.h"
#include "CRC.h"

//Constant used to determine CRC polynomial coefficients
#define	P_32	0xEDB88320L

/*
 * The CRC algorithm uses a look-up table with pre-calculated values to
 * speed up calculations.  The first time the CRC is called, the
 * table is initialized.
 */
static bool		crc_tab32_init = false;
static U32 		crc_tab32[256];


/*
 * Initialize CRC lookup table
 */
static void init_crc32_tab( void )
{
    S16 i, j;
    U32 crc;

    for (i = 0; i < 256; i++) {

        crc = (U32) i;

        for (j = 0; j < 8; j++) {

            if ( crc & 0x00000001L ) crc = ( crc >> 1 ) ^ P_32;
            else                     crc =   crc >> 1;
        }

        crc_tab32[i] = crc;
    }

    crc_tab32_init = true;

}

/*
 * Calculates a new CRC-32 value.
 *
 */
U32 CRC_updateCRC32(U32 crc, U8 c )
{

    U32 tmp, long_c;

    long_c = 0x000000ffL & (U32) c;

    if ( ! crc_tab32_init ) init_crc32_tab();
    //if (init) init_crc32_tab();

    tmp = crc ^ long_c;
    crc = (crc >> 8) ^ crc_tab32[ tmp & 0xff ];

    return crc;

}

void CRC_reset(void)
{
	crc_tab32_init = false;
}
