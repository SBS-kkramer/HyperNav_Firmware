/*
 * wavelength.h
 *
 *  Created on: Jan 28, 2011
 *      Author: plache
 */

#ifndef WAVELENGTH_H_
#define WAVELENGTH_H_

# include <compiler.h>

# define WL_OK		 0
# define WL_FAIL	-1

char const* wl_MakeFileName ( S16 side );
S16	wl_ParseFile ( char const* filename, S16 side );

S16 wl_Load ( S16 side );

//	Cell in 0..255 range
//	but ZCOEFs use 1..256 range.
F64 wl_wlenOfCell ( S16 side, S16 cell );
S16 wl_makeWL ( S16 side, F64 wl[], S16 numChannels );
S16 wl_verifyWL ( S16 side, F64 wl[], S16 numChannels, S16* numDiffs );
S16 wl_NumCoefs( S16 side );
F64 wl_GetCoef( S16 side, S16 c );
U32 wl_GetSN( S16 side );

#endif /* WAVELENGTH_H_ */
