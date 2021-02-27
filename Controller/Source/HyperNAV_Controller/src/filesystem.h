/*
 * filesystem.h
 *
 *  Created on: Dec 3, 2010
 *      Author: plache
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

# include "command.controller.h"


# define FSYS_OK	 0
# define FSYS_FAIL	-1

S16 FSYS_getCRCOfFile ( char const* filename, U16* crc );
S16 FSYS_RcvWlCoefFile ( S16 side );
# if 0
S16 FSYS_SetActiveCalFile( char* filename );
# endif

S16 FSYS_CmdList ( char const* folder );
S16 FSYS_CmdReceive ( char const* option, char const* specifier, Access_Mode_t const* const access_mode, bool useCRC );
S16 FSYS_CmdSend ( char const* option, char const* specifier, Access_Mode_t const* const access_mode, bool use1k );
S16 FSYS_CmdCRC ( char const* option, char const* specifier, char* result, S16 r_max_len );
S16 FSYS_CmdDisplay ( char const* option, char const* specifier, Access_Mode_t const* const access_mode, char how );
S16 FSYS_CmdDelete ( char const* option, char const* specifier, Access_Mode_t const* const access_mode );
S16 FSYS_CmdErase ( char const* option, char const* specifier, Access_Mode_t const* const access_mode );

S16 FSYS_Setup ( void );

#endif /* FILESYSTEM_H_ */
