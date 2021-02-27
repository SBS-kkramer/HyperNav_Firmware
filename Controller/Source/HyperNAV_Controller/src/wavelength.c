/*
 * wavelength.c
 *
 *  Created on: Jan 28, 2011
 *      Author: plache
 */

# include "wavelength.h"

# include <errno.h>
# include <stdlib.h>
# include <string.h>

# include "files.h"
# include "extern.controller.h"
# include "io_funcs.controller.h"

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif

# define WL_COEFS 5
static U32 wl_SpecSN[2] = { 0, 0 };
static F64 wl_values[2][WL_COEFS];
static S16 wl_numValues[2] = { 0, 0 };
static S16 wl_PixBase[2] = { 0, 0 };
static S16 wl_PixMin [2] = { 0, 0 };
static S16 wl_PixMax [2] = { 0, 0 };


//!	\brief	Generate the z-coeff file name.
//!
//!	@param	filename	provide the generated file name here.
//!	@param	maxlen		size of char array passed
//!
char const* wl_MakeFileName ( S16 side ) {

	switch ( side ) {
	case  0: return EMMC_DRIVE "SPCSBDWL.WLC";
	case  1: return EMMC_DRIVE "SPCPRTWL.WLC";
	default: return NULL;
	}
}

static char const* wl_MakeBinFName ( S16 side ) {

	switch ( side ) {
	case  0: return EMMC_DRIVE "SPCSBDWL.WLB";
	case  1: return EMMC_DRIVE "SPCPRTWL.WLB";
	default: return NULL;
	}
}

static S16 wl_writeBin ( S16 side ) {

	char const* bin_fn = wl_MakeBinFName( side );
	if ( !bin_fn ) return WL_FAIL;

	fHandler_t fh;
	if ( FILE_OK != f_open ( bin_fn, O_WRONLY | O_CREAT, &fh ) ) {
		return WL_FAIL;
	}

	S32 total = f_write ( &fh, &wl_SpecSN[side], sizeof(wl_SpecSN[side]) )
			  + f_write ( &fh, &wl_values[side], sizeof(wl_values[side]) )
			  + f_write ( &fh, &wl_numValues[side], sizeof(wl_numValues[side]) )
			  + f_write ( &fh, &wl_PixBase[side], sizeof(wl_PixBase[side]) )
			  + f_write ( &fh, &wl_PixMin [side], sizeof(wl_PixMin [side]) )
			  + f_write ( &fh, &wl_PixMax [side], sizeof(wl_PixMax [side]) );

	f_close ( &fh );

	if ( total == sizeof(wl_SpecSN[side])
			    + sizeof(wl_values[side])
			    + sizeof(wl_numValues[side])
			    + sizeof(wl_PixBase[side])
			    + sizeof(wl_PixMin [side])
			    + sizeof(wl_PixMax [side]) ) {
		return WL_OK;
	} else {
		return WL_FAIL;
	}
}

static S16 wl_readBin ( S16 side ) {

	char const* bin_fn = wl_MakeBinFName( side );
	if ( !bin_fn ) return WL_FAIL;

	fHandler_t fh;
	if ( FILE_OK != f_open ( bin_fn, O_RDONLY, &fh ) ) {
		return WL_FAIL;
	}

	S32 total = f_read ( &fh, &wl_SpecSN[side], sizeof(wl_SpecSN[side]) )
			  + f_read ( &fh, &wl_values[side], sizeof(wl_values[side]) )
			  + f_read ( &fh, &wl_numValues[side], sizeof(wl_numValues[side]) )
			  + f_read ( &fh, &wl_PixBase[side], sizeof(wl_PixBase[side]) )
			  + f_read ( &fh, &wl_PixMin[side], sizeof(wl_PixMin[side]) )
			  + f_read ( &fh, &wl_PixMax[side], sizeof(wl_PixMax[side]) );

	f_close ( &fh );

	if ( total == sizeof(wl_SpecSN[side])
			    + sizeof(wl_values[side])
			    + sizeof(wl_numValues[side])
			    + sizeof(wl_PixBase[side])
			    + sizeof(wl_PixMin[side])
			    + sizeof(wl_PixMax[side]) ) {
		return WL_OK;
	} else {
		return WL_FAIL;
	}

}

/*
 *!	\brief	Search for CAL file on dist, use latest revision.
 *
 *  Allocate space for new file name, and return pointer.
 *  Caller has to free!!!
 */
S16 wl_ParseFile ( char const* filename, S16 side ) {

	//	First part:
	//	Get file content into memory.

	fHandler_t fh;

	if ( FILE_FAIL == f_open ( filename, O_RDONLY, &fh ) ) {
		return WL_FAIL;
	}

	S32 f_size = f_getSize ( &fh );

	if ( FILE_FAIL == f_size ) {
		f_close ( &fh );
		return WL_FAIL;
	}

	char* f_data = pvPortMalloc ( f_size+1 );

	if ( !f_data ) {
		f_close ( &fh );
		return WL_FAIL;
	}

	if ( f_size != f_read ( &fh, f_data, f_size ) ) {
		vPortFree ( f_data );
		f_close ( &fh );
		return WL_FAIL;
	}

	f_close ( &fh );

	//  Terminate data with an additional NUL character.
	//  This makes scanning for lines with an automatic
	//  end-of-data marker easier.
	f_data [ f_size ] = 0;

	char* next_start = f_data;

	//  Expect a file with comment lines at the beginning,
	//	and data lines of the form
	//	"C%d %f" index, coefficient
	//
	//	The first line ideally contains the serial number of the spectrometer.

	bool done_sn = false;
	S16 wlc;
	for ( wlc=0; wlc<WL_COEFS; wlc++ ) {
		wl_values [side][ wlc ] = 0.0;
	}

	//  Start scanning the file content for individual lines
	char* this_line = next_start;

	while ( this_line ) {

		if ( 'C' == this_line[0] && '0' <= this_line[1] && this_line[1] <= '9' ) {
			F64 val = 0;
			char* next;
			errno = 0;
			wlc = strtol ( this_line+1, &next, 10 );
			if ( !errno ) val = strtof ( next, (char**)0 );
			if ( !errno ) {
				if ( 0 <= wlc && wlc < WL_COEFS ) {
					wl_values[side][ wlc ] = val;
				} else {
					//	Invalid data line
				}
			} else {
				//	Line parsing failed
			}
		} else if ( 0 == strncmp ( this_line, "PIXBASE ", 7 ) ) {
					errno = 0;
					wl_PixBase[side] = strtol ( this_line+7, (char**)0, 10 );
					if ( errno ) {
						wl_PixBase[side] = 0;
					}
		} else if ( 0 == strncmp ( this_line, "PIXMIN ", 6 ) ) {
					errno = 0;
					wl_PixMin[side] = strtol ( this_line+6, (char**)0, 10 );
					if ( errno ) {
						wl_PixMin[side] = 0;
					}
		} else if ( 0 == strncmp ( this_line, "PIXMAX ", 6 ) ) {
					errno = 0;
					wl_PixMax[side] = strtol ( this_line+6, (char**)0, 10 );
					if ( errno ) {
						wl_PixMax[side] = 0;
					}
		} else {
			if ( !done_sn ) {

				//	Try to extract serial number from some line, which may be like:
				//  # Zeiss-CGS-UV-NIR 089384	

				char* sn = strstr ( this_line, "UV-NIR " );
				if ( sn ) {
					sn+=6;
					errno = 0;
					wl_SpecSN[side] = strtol ( sn, (char**)0, 10 );
					if ( errno ) {
						wl_SpecSN[side] = 0;
					}
				}

				done_sn = true;
			}
		}

		this_line = scan_to_next_EOL ( next_start, &next_start );
	}

	vPortFree ( f_data );

	wl_numValues[side] = WL_COEFS-1;

	while ( wl_numValues[side] >= 0 && 0.0 == wl_values[side][wl_numValues[side]] ) {
		wl_numValues[side] --;
	}

	wl_numValues[side]++;

	//	Need at least linear term to get a wavelength array.

	if ( wl_numValues[side] < 2 ) {
		return WL_FAIL;
	}

	//	We know the approximate polynomial to expect:
	//	WL = 190 + 0.8 * channel + Z[2]*channel^2
	//	Reject unreasonable numbers
//	if ( ! ( 170.0 <= wl_values[side][0] && wl_values[side][0] <= 210.0 ) ) {
//		return WL_FAIL;
//	}

//	if ( ! ( 0.7 <= wl_values[side][1] && wl_values[side][1] <= 0.9 ) ) {
//		return WL_FAIL;
//	}

	//	Write wavelength coefficients to binary file for faster retrieval
	wl_writeBin( side );

	return WL_OK;
}


F64 wl_wlenOfCell ( S16 side, S16 cell )
{
	//	Wavelengh coefficients are ??? based
	F64 cellF64 = (cell+wl_PixBase[side]);

	return wl_values[side][0]
	     + wl_values[side][1]*cellF64
	     + wl_values[side][2]*cellF64*cellF64
	     + wl_values[side][3]*cellF64*cellF64*cellF64
	     + wl_values[side][4]*cellF64*cellF64*cellF64*cellF64;
}

S16 wl_makeWL ( S16 side, F64 wl[], S16 numChannels )
{
	if ( wl_NumCoefs(side) < 2 )
		return WL_FAIL;

	S16 i;
	for( i=0; i<numChannels; i++ ) {
		wl[i] = wl_wlenOfCell(side, i);
	}

	return WL_OK;
}

S16 wl_verifyWL ( S16 side, F64 wl[], S16 numChannels, S16* numDiffs )
{
	if ( !numDiffs || wl_NumCoefs(side) < 2 )
		return WL_FAIL;

	*numDiffs = 0;

	S16 i;
	for( i=0; i<numChannels; i++ ) {
		F64 const delta = wl[i] - wl_wlenOfCell(side,i);
		if ( delta < -0.01 || 0.01 < delta ) {
			(*numDiffs)++;
		}
	}

	return WL_OK;
}

S16 wl_NumCoefs( S16 side ) {
	return wl_numValues[side];
}

F64 wl_GetCoef( S16 side, S16 c ) {
	if ( 0 <= c && c < WL_COEFS ) {
		return wl_values[side][c];
	} else {
		return 0.0;
	}
}

U32 wl_GetSN( S16 side ) {
	return wl_SpecSN[side];
}

S16 wl_Load ( S16 side ) {

	if ( WL_FAIL == wl_readBin( side ) ) {
		return wl_ParseFile ( wl_MakeFileName( side ), side );
	}

	return WL_OK;
}



