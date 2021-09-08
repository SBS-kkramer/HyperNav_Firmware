/*
 * filesystem.c
 *
 *  Created on: Dec 3, 2010
 *      Author: plache
 */

# include "filesystem.h"

# include <ctype.h>
# include <stdio.h>
# include <string.h>

# include "telemetry.h"
# include "io_funcs.controller.h"

# include "extern.controller.h"
# include "config.controller.h"
# include "files.h"

# include "filesystem.h"
# include "datalogfile.h"
# include "errorcodes.h"
# include "syslog.h"
# include "xmodem.h"
# include "watchdog.h"
# include "wavelength.h"

# include "profile_manager.h"

// Use thread-safe dynamic memory allocation if available
#ifdef FREERTOS_USED
#include "FreeRTOS.h"
#else
#define pvPortMalloc	malloc
#define vPortFree		free
#endif


static char const* fsys_makePkgFileName () {

	return EMMC_DRIVE "PKG.ZIP";
}

static bool isValidFileName ( char const* filename ) {

	if ( 0 == filename ) {
		return false;
	}

	S16 const fn_len = strlen ( filename );

	//  Need at least ?:\ plus single character
	if ( fn_len < 4 ) return false;

	//	Check for drive designation

	if ( filename[0] != EMMC_DRIVE_CHAR ) return false;

	if ( filename[1] != ':'
	  || filename[2] != '\\' ) return false;

	int c;

	for ( c=3; c<fn_len; c++ ) {

		if ( !isalnum(filename[c])
		  && '\\' != filename[c]
		  && '/' != filename[c]
		  && '_' != filename[c]
		  && '-' != filename[c]
		  && '.' != filename[c] ) {
			return false;
		}
	}

	return true;
}

static S16 fsys_eraseFile ( char const* filename ) {

	if ( !f_exists( filename ) ) {
		return CEC_FileNotPresent;
    }

	return ( FILE_OK == f_delete ( filename ) ) ? CEC_Ok : CEC_Failed;
}


static S16 fsys_eraseDirectoryContent ( char const* directory ) {

	if ( !directory ) return CEC_Failed;

	tlm_flushRecv();

	if ( FILE_OK == file_initListing( directory ) ) {

		char fullFileName [ strlen(directory) + 1 + 32 + 1 ];

		char fName[64];
		U32 fSize;
		Bool isDir;
		struct tm when;
		while ( FILE_OK == file_getNextListElement ( fName, sizeof(fName), &fSize, &isDir, &when) ) {

			if ( strcmp( fName, "." ) && strcmp(  fName, ".." ) ) {

				  watchdog_clear();
				  strcpy ( fullFileName, directory );
				  strcat ( fullFileName, "\\" );
				  strcat ( fullFileName, fName );

				  io_out_string ( "Erasing " );
				  io_out_string ( fullFileName );
				  io_out_string ( "\r\n" );

				  if ( FSYS_FAIL ==  fsys_eraseFile ( fullFileName ) ) {
					  return CEC_Failed+2;
				  }
			}

			char input;
			if ( tlm_recv ( &input, 1, TLM_PEEK | TLM_NONBLOCK ) > 0 ) {

				tlm_recv ( &input, 1, TLM_NONBLOCK );
				return CEC_Ok;
			}
		}

		return CEC_Ok;

	} else {
io_out_string ( "Cannot list " );
io_out_string ( directory );
io_out_string ( "\r\n" );

		return CEC_Failed;
	}
}

static S16 fsys_receiveFile ( char const* filename, bool useCRC ) {

	if ( !isValidFileName ( filename ) ) {
		return CEC_FileNameInvalid;
	}

	//  Receive file via xmodem.

	if ( f_exists( filename ) ) {
		return CEC_FileExists;
	}

	io_out_string( "\r\n$ACK" );

	switch ( XMDM_recv_from_usart( filename, useCRC ) ) {
	case XMDM_TIMEOUT: return CEC_RcvTimeout;
	case XMDM_TOO_MANY_ERRORS:
	case XMDM_FAIL:		return CEC_RcvFailed;
	case XMDM_CAN:		return CEC_RcvCancelled;
	case XMDM_OK:		return CEC_Ok;
	default:			return CEC_RcvFailed;
	}
}

static S16 fsys_sendFile ( char const* filename, bool use1k ) {

	if ( !f_exists( filename ) ) {
		return CEC_FileNotPresent;
    }

	//  Send file via xmodem.

	tlm_flushRecv();		//	Create clean input stream
	io_out_string ( "\r\n$ACK" );	//	Tell other side that we are ready

	switch ( XMDM_send_to_usart( filename, use1k ) ) {
	case XMDM_TIMEOUT:	return CEC_SndTimeout;
	case XMDM_FAIL:		return CEC_SndFailed;
	case XMDM_CAN :		return CEC_SndCancelled;
	case XMDM_OK:		return CEC_Ok;
	default:			return CEC_SndFailed;
	}
}

static S16 fsys_dispFile ( char const* filename, char how ) {

	if ( !f_exists( filename ) ) {
		return CEC_FileNotPresent;
    }

	fHandler_t fh;
	if ( FILE_OK != f_open ( filename, O_RDONLY, &fh ) ) {
		//  TODO: This should be a different error code.
		return CEC_CannotOpenFile;
	}

# define BLOCK_SIZE 64
	S32 const fileSize = f_getSize ( &fh );
	
	if ( FILE_FAIL == fileSize ) {
		f_close ( &fh );
		//  TODO: This should be a different error code.
		return CEC_CannotOpenFile;
	}
	
	S32 const numBlocks = 1 + ( fileSize-1 ) / BLOCK_SIZE;
	S32 const finalBlockSize = fileSize - BLOCK_SIZE * (numBlocks-1);

	//  Send packets
	U8 block [ BLOCK_SIZE ];

	S32	b;
	for ( b=0; b<numBlocks; b++ ) {

		watchdog_clear();

		S16 const this_block_size = (b<numBlocks-1)
				? BLOCK_SIZE
				: finalBlockSize;

		//snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "Block %ld (%d bytes): ", b, this_block_size );
		//io_out_string( glob_tmp_str );

		if ( this_block_size != f_read ( &fh, block, this_block_size ) ) {

			f_close ( &fh );
			return CEC_Failed;
		}

		if ( 'A' == how ) {
			tlm_send( block, this_block_size, 0 );
		} else {
			char asHex [2*BLOCK_SIZE+1];

			int i;
			for ( i=0; i<this_block_size; i++ ) {
				snprintf ( asHex+2*i, 3, "%02hx", (U16)block[i] );
			}
			tlm_send( asHex, 2*this_block_size, 0 );
		}
	}

	f_close ( &fh );

	return CEC_Ok;
# undef BLOCK_SIZE
}

//! \brief list files/folders in a folder
//
//	@param  folder - content of which is to be listed
//	return  CEC_FileNotPresent : folder does not exist
//	        CEC_Failed : folder cannot be listed
//	        CEC_OK : folder was sucessfully listed

S16	FSYS_CmdList ( char const* folder ) {

    char const* prv_folder = (folder && *folder) ? folder : "0:\\";

	//	The f_exist() call does not recognize "0:", "0:\" etc as existing
	//	Thus, exempt those special cases from the check for existence.
	if ( strcmp(prv_folder,"0:")
	  && strcmp(prv_folder,"0:\\")
	  && strcmp(prv_folder,"1:")
	  && strcmp(prv_folder,"1:\\")
	  && !f_exists(prv_folder) ) {
		return CEC_FileNotPresent;
	}

	char const heading_format[] =   "\t%15s\t%19s\t%s\r\n";
	char const listing_format[] = "%s\t%15lu\t%04d-%02d-%02d %02d:%02d:%02d\t%s\r\n";
	char listing[64+32];

	if ( FILE_OK == file_initListing( prv_folder ) ) {

		io_out_string ( "DIR name is " );
		io_out_string ( prv_folder );
		io_out_string ( "\r\n" );

		S16 nFiles = 0;

		snprintf ( listing, sizeof(listing), heading_format, "Size (bytes)", "Date Time", "Name" );
		io_out_string ( listing );

		char fName[64];
		U32 fSize;
		Bool isDir;
		struct tm when;
		while ( FILE_OK == file_getNextListElement ( fName, sizeof(fName), &fSize, &isDir, &when) ) {

			if ( strcmp( fName, "." ) && strcmp(  fName, ".." ) ) {
				snprintf ( listing, sizeof(listing), listing_format,
						isDir?"Dir":"", fSize,
						when.tm_year, when.tm_mon, when.tm_mday,
						when.tm_hour, when.tm_min, when.tm_sec,
						fName );
				io_out_string ( listing );
				nFiles++;

				if ( 0 == (nFiles & 0xff) ) {
					watchdog_clear();
				}
			}

			char input;
			if ( tlm_recv ( &input, 1, TLM_PEEK | TLM_NONBLOCK ) > 0 ) {

				tlm_recv ( &input, 1, TLM_NONBLOCK );
				if ( '$' == input ) return CEC_Failed;
			}

		}
		snprintf ( glob_tmp_str, sizeof(glob_tmp_str), "Total of %hd items listed.\r\n\r\n", nFiles );
		io_out_string ( glob_tmp_str );

		return CEC_Ok;
	} else {
		io_out_string ( "Problem listing '" );
		io_out_string ( prv_folder );
		io_out_string ( "'\r\n" );
		return CEC_Failed;
	}
}

S16 FSYS_RcvWlCoefFile ( S16 side ) {

	//	The calls to f_move() seem to only work
	//	when the destination file name is without path.
	//	Thus the pieces of code where the full
	//	file name is scanned from the end for back-slash or slash.

	char const* const ZTMP = "W.W";
	char const* filename = wl_MakeFileName( side );
	char* base_fn = 0;

	{
		char tmpname[strlen(filename)+1];
		tmpname[0] = 0;

		if ( f_exists(filename) ) {

			strcpy ( tmpname, filename );
			char* slash = strrchr ( tmpname, '\\' );
			if ( !slash ) slash = strrchr ( tmpname, '/' );
			if ( slash ) {
				slash++;
				size_t remainder = strlen ( slash );
				strncpy ( slash, ZTMP, remainder-1 );
			} else {
				tmpname[0] = 0;	//  Cannot build temp name
			}

			if ( tmpname[0] ) {

				base_fn = strrchr ( filename, '\\' );
				if ( !base_fn ) base_fn = strrchr ( filename, '/' );
				base_fn++;

				if ( f_exists ( tmpname ) ) {
					f_delete ( tmpname );
				}
				if ( FILE_FAIL == f_move( filename, ZTMP ) ) {
					//	Fallback: Remove file.
					f_delete ( filename );
					tmpname[0] = 0;
				}
			}
		}

		S16 cec = fsys_receiveFile ( filename, false );

		if ( cec != CEC_Ok ) {
			//	Upload failed
   			f_delete ( filename );
			if ( tmpname[0] ) {
				f_move( tmpname, base_fn );
				wl_ParseFile( filename, side );
			}
			return cec;
		} else {
			if ( WL_FAIL == wl_ParseFile( filename, side ) ) {
				f_delete ( filename );
				if ( tmpname[0] ) {
					f_move( tmpname, base_fn );
					wl_ParseFile( filename, side );
				}
				return CEC_ZFile_Invalid;
			} else {
				if ( tmpname[0] ) f_delete ( tmpname );
				return CEC_Ok;
			}
		}
    }
}

static S16 FSYS_RcvCfgFile ( Access_Mode_t access_mode, bool useCRC ) {

	char const* const CFGTMP = EMMC_DRIVE "CFG.CFG";

	if ( f_exists(CFGTMP) ) f_delete( CFGTMP );

	S16 cec = fsys_receiveFile ( CFGTMP, useCRC );

   	if ( cec != CEC_Ok ) {
   		//	Upload failed
		f_delete ( CFGTMP );
   		return cec;
   	} else {
   		fHandler_t fh;
   		if ( FILE_OK != f_open ( CFGTMP, O_RDONLY, &fh ) ) {
   			f_delete ( CFGTMP );
   			return CEC_Failed;
   		}

   		S32 f_size = f_getSize ( &fh );

   		if ( FILE_FAIL == f_size ) {
   			f_close ( &fh );
   			f_delete ( CFGTMP );
   			return CEC_Failed;
   		}

   		//	Parse the file for commands
   		//	Each line is "parameter value" or "parameter value,value"
   		//	meaning: set parameter = value

   		U16 const block_size = 224;	//	Make longer than the longest expected line
   		char f_data [2*block_size+1];

   		char* const block_1 = f_data;
   		char* const block_2 = f_data + block_size;

   		S32 remaining_size = f_size;
   		U16 to_read = remaining_size > 2*block_size ? 2*block_size : (U16)remaining_size;

   		if ( to_read != f_read ( &fh, f_data, to_read ) ) {
   			f_close ( &fh );
   			f_delete ( CFGTMP );
   			return CEC_Failed;
   		}

   		//  Terminate data with an additional NUL character.
   		//  This makes scanning for lines with an automatic
   		//  end-of-data marker easier.
   		f_data [ f_size ] = 0;
   		remaining_size -= to_read;

   		char* next_start = f_data;

   		//  Start scanning the file content for individual lines
   		char* this_line = scan_to_next_EOL ( next_start, &next_start );

   		while ( this_line ) {

   			if ( this_line[0] != '\032'	// XMODEM padding at end of file
   			  && this_line[0] != '#'	// Comment
   			  && strlen(this_line) > 9 ) {

   				this_line[8] = 0;
   				if ( CEC_Ok != CFG_CmdSet ( this_line, this_line+9, access_mode ) ) {
   					io_out_string ( "Failed " );
   					io_out_string ( this_line );
   					io_out_string ( " " );
   					io_out_string ( this_line+9 );
   					io_out_string ( "\r\n" );
   					cec = CEC_Failed;
   				}
   			}

   			if ( next_start > block_2 && remaining_size ) {

   				//	Move the second block into the first block,
   				//	and adjust the current position.
   				memcpy ( block_1, block_2, block_size );
   				next_start -= block_size;

   				U16 to_read = remaining_size > block_size ? block_size : (U16)remaining_size;
   				if ( to_read != f_read ( &fh, block_2, to_read ) ) {
   					f_close ( &fh );
   					f_delete ( CFGTMP );
   					return CEC_Failed;
   				}

   				block_2[ to_read ] = 0;
   				remaining_size -= to_read;
   			}

   			this_line = scan_to_next_EOL ( next_start, &next_start );
   		}

		f_close ( &fh );
		f_delete ( CFGTMP );

		CFG_Save( true );

		return cec;
    }
}

S16 FSYS_CmdReceive ( char const* option, char const* specifier, Access_Mode_t const* const access_mode, bool useCRC ) {

	if ( !option ) return CEC_Failed;

	if ( 0 == strncasecmp ( option, "Z", 1 ) ) {

		if ( *access_mode < Access_Admin )
			return CEC_PermissionDenied;
		else {
			switch ( option[1] ) {
			case 'S': return FSYS_RcvWlCoefFile ( (S16)0 );  // Starboard
			case 'P': return FSYS_RcvWlCoefFile ( (S16)1 );  // Port
			default : return CEC_CmdRcvUnknown;
			}
		}

	} else if ( 0 == strncasecmp ( option, "CFG", 3 ) ) {

		//	Ignore specifier
# if 0
		if ( *access_mode < Access_Admin )
			return CEC_PermissionDenied;
		else
# endif
			return FSYS_RcvCfgFile ( *access_mode, useCRC );

	} else if ( 0 == strncasecmp ( option, "PKG", 3 ) ) {

		char const* filename = fsys_makePkgFileName();
		if ( f_exists(filename)) {
			f_delete(filename);
		}
		return fsys_receiveFile ( filename, useCRC );

	} else if ( *access_mode >= Access_Factory ) {

		return fsys_receiveFile ( option, useCRC );

	} else {
		return CEC_CmdRcvUnknown;
    }

	//	Execution never gets here
}







S16 FSYS_CmdSend ( char const* option, char const* specifier, Access_Mode_t const* const access_mode, bool use1k )
{
	if ( !option ) return CEC_Failed;
	if ( !specifier ) return CEC_FileNameMissing;

	if ( 0 == strncasecmp ( option, "PKG", 3 ) ) {

		return fsys_sendFile ( fsys_makePkgFileName(), use1k );

	} else if ( 0 == strncasecmp ( option, "Z", 1 ) ) {

		switch ( option[1] ) {
			case 'S': return fsys_sendFile ( wl_MakeFileName( (S16)0 ), use1k );  // Starboard
			case 'P': return fsys_sendFile ( wl_MakeFileName( (S16)1 ), use1k );  // Port
			default : return CEC_CmdRcvUnknown;
		}

	} else if ( 0 == strncasecmp ( option, "LOG", 3 ) ) {

		char const* syslog_dir = syslog_LogDirName();
		char fullFileName [ strlen(syslog_dir) + 1 + strlen(specifier) + 1 ];

		strcpy ( fullFileName, syslog_dir );
		strcat ( fullFileName, "\\" );
		strcat ( fullFileName, specifier );

		return fsys_sendFile ( fullFileName, use1k );
# if 0
	} else if ( 0 == strncasecmp ( option, "DATA", 4 ) ) {

		char* dir = dlf_LogDirName();
		char fullFileName [ strlen(dir) + 1 + strlen(specifier) + 1 ];

		strcpy ( fullFileName, dir );
		strcat ( fullFileName, "\\" );
		strcat ( fullFileName, specifier );

		return fsys_sendFile ( fullFileName, use1k );
# endif
	} else if ( /* *access_mode >= Access_Admin && */ 0 == strncasecmp ( option, "*", 1 ) ) {
		return fsys_sendFile ( specifier, use1k );
	} else {
		return CEC_CmdSndUnknown;
	}

	//	Execution never gets here
}

S16 FSYS_getCRCOfFile ( char const* filename, U16* crc ) {

	if ( !f_exists( filename ) ) {
		return CEC_FileNotPresent;
    }

	fHandler_t fh;

	if ( FILE_OK != f_open ( filename, O_RDONLY, &fh ) ) {
		return CEC_CannotOpenFile;
	}

	*crc = 0;

	U8 this_byte = 0;
	U16 wdt = 0;

# define XMODEM_PAD_CHAR	0x1A
	while ( 1 == f_read( &fh, &this_byte, 1) ) {
		if ( this_byte != XMODEM_PAD_CHAR ) {

			*crc = *crc ^ (int) this_byte << 8;
			S16 i = 8;
			do
			{
				if ( *crc & 0x8000)
					*crc = *crc << 1 ^ 0x1021;
				else
					*crc = *crc << 1;
			} while(--i);
		}

		if ( ++wdt == 0xffff ) watchdog_clear();
	}
# undef XMODEM_PAD_CHAR

	f_close( &fh );

	return CEC_Ok;
}

static S16 fsys_fileCRC ( char const* filename, char* result, S16 r_max_len ) {

	U16 crc;

	S16 cec = FSYS_getCRCOfFile ( filename, &crc );

	if ( CEC_FileNotPresent == cec ) {
		snprintf( result, r_max_len, "0" );
		cec = CEC_Ok;
	} else {
		snprintf( result, r_max_len, "%04hu", crc );
	}

	return cec;
}

S16 FSYS_CmdCRC ( char const* option, char const* specifier, char* result, S16 r_max_len ) {

	if ( !option ) return CEC_Failed;

	if ( 0 == strncasecmp ( option, "PKG", 3 ) ) {

		return fsys_fileCRC (  fsys_makePkgFileName(), result, r_max_len );

	} else {
		return CEC_CmdCRCUnknown;
	}

	//	Execution never gets here
}

S16 FSYS_CmdDisplay (char const*                option,
										 char const*                specifier,
										 Access_Mode_t const* const access_mode,
										 char                       how
                    )
{

	if ( !option ) return CEC_Failed;
	if ( !specifier ) return CEC_FileNameMissing;

	if ( 0 == strncasecmp ( option, "LOG", 4 ) )
	{
    char const* syslog_dir = syslog_LogDirName();
		char fullFileName [ strlen(syslog_dir) + 1 + strlen(specifier) + 1 ];

		strcpy ( fullFileName, syslog_dir );
		strcat ( fullFileName, "\\" );
		strcat ( fullFileName, specifier );

		return fsys_dispFile ( fullFileName, how );

	}
	else if (0 == strncasecmp ( option, "*", 1 ) )
	{

		return fsys_dispFile ( specifier, how );

	}
	else
	{
		return CEC_CmdDspUnknown;
	}

	//	Execution will not get here.
}





S16 FSYS_CmdDelete ( char const* option, char const* specifier, Access_Mode_t const* const access_mode ) {

	if ( !option ) return CEC_Failed;
	if ( !specifier ) return CEC_FileNameMissing;

	if ( 0 == strncasecmp ( option, "LOG", 3 ) )
	{

    char const* syslog_dir = syslog_LogDirName();
		char fullFileName [ strlen(syslog_dir) + 1 + strlen(specifier) + 1 ];

		strcpy ( fullFileName, syslog_dir );
		strcat ( fullFileName, "\\" );
		strcat ( fullFileName, specifier );

		return fsys_eraseFile( fullFileName );

# if defined(OPERATION_AUTONOMOUS)
	} else if ( 0 == strncasecmp ( option, "FREEFALL", 8 ) ) {

		char const* dir = dlf_LogDirName();

        if ( 0 == strcmp( "*", specifier ) && *access_mode >= Access_Factory ) {

			return fsys_eraseDirectoryContent ( dir );

		} else {

			char fullFileName [ strlen(dir) + 1 + strlen(specifier) + 1 ];

			strcpy ( fullFileName, dir );
			strcat ( fullFileName, "\\" );
			strcat ( fullFileName, specifier );

			return fsys_eraseFile ( fullFileName );
		}
# endif

# if defined(OPERATION_NAVIS)
	} else if ( 0 == strncasecmp ( option, "NAVIS", 5 ) ) {

		char const* dir = profile_manager_LogDirName();

		char navis_folder_name[32];
		snprintf ( navis_folder_name, 32, "%s\\%s", dir, specifier );

		fsys_eraseDirectoryContent ( navis_folder_name );
		return fsys_eraseFile ( navis_folder_name );
# endif

	} else if ( *access_mode >= Access_Factory
			&& 0 == strncasecmp ( option, "*", 1 ) ) {

		return fsys_eraseFile ( specifier );

	} else if ( *access_mode >= Access_Factory
			&& 0 == strncasecmp ( option, "#", 1 ) ) {

		char folder_name[32];
		snprintf ( folder_name, 32, EMMC_DRIVE "%s", specifier );

		fsys_eraseDirectoryContent ( folder_name );
		return fsys_eraseFile      ( folder_name );

	} else {
		return CEC_CmdDelUnknown;
	}

	//	Execution never gets here
}

//
//  Will wipe all folders inside option, with the specifier indicating the year:
//  option NAVIS    -> folders = 0:/NAVIS/YY???[_?]
//  option FREEFALL -> folders = 0:/FREEFALL/YY-mm-dd
//
S16 FSYS_CmdErase ( char const* option, char const* specifier, Access_Mode_t const* const access_mode ) {

  if ( !option ) return CEC_Failed;
  if ( !specifier ) return CEC_FileNameMissing;

  char fullFileName [ 32 ];

  if ( 0 == strncasecmp ( option, "NAVIS", 5 ) ) {

    int j;
    for ( j=0; j<=9; j++ ) {

      snprintf ( fullFileName, 31, EMMC_DRIVE "NAVIS\\%s%d", specifier, j );

      if ( f_exists( fullFileName ) ) {
        io_out_string ( "Erase " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
        fsys_eraseDirectoryContent ( fullFileName );
        fsys_eraseFile             ( fullFileName );
      } else {
        io_out_string ( "Skip  " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
      }

      char c;
      for ( c='A'; c<='Z'; c++ ) {

        snprintf ( fullFileName, 31, EMMC_DRIVE "NAVIS\\%s%d_%c", specifier, j, c );

        if ( f_exists( fullFileName ) ) {
          io_out_string ( "Erase " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
          fsys_eraseDirectoryContent ( fullFileName );
          fsys_eraseFile             ( fullFileName );
        } else {
          io_out_string ( "Skip  " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
        }
      }
    }

    return CEC_Ok;

  } else if ( 0 == strncasecmp ( option, "FREEFALL", 8 ) ) {

    int d;
    for ( d=1; d<=31; d++ ) {

      snprintf ( fullFileName, 31, EMMC_DRIVE "FREEFALL\\%s-%02d", specifier, d );

      if ( f_exists( fullFileName ) ) {
        io_out_string ( "Erase " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
        fsys_eraseDirectoryContent ( fullFileName );
        fsys_eraseFile             ( fullFileName );
      } else {
        io_out_string ( "Skip  " ); io_out_string ( fullFileName ); io_out_string ( "\r\n" );
      }
    }

    return CEC_Ok;

  } else {
    return CEC_CmdDelUnknown;
  }

    //	Execution never gets here
}

# if 0
static void fsys_wipeDir ( char* dirname ) {

	if ( dirname
	  && f_exists( dirname )
	  && FILE_OK == file_initListing( dirname ) ) {

		io_out_string ( "Removing all files from " );
		io_out_string ( dirname );
		io_out_string ( "\r\n" );

		char fName[32];
		U32 fSize;
		Bool isDir;
		U16 dirnameLen = strlen(dirname);
		char ffn [ dirnameLen + sizeof(fName) ];

		while ( FILE_OK == file_getNextListElement (
						fName, sizeof(fName), &fSize, &isDir, NULL) ) {


			snprintf ( ffn, sizeof(ffn), "%s%s", dirname, fName );
			io_out_string ( "Deleting " );
			io_out_string ( ffn );
			io_out_string ( "\r\n" );
			f_delete( ffn );
		}
	}
}
# endif

void FSYS_Info ( void ) {

	U64 sz_avail;
	U64 sz_total;

	char* unit[4] = { "", "k", "M", "G" };

	io_out_string( "\r\neMMC file system " );

	if ( FILE_OK != f_space( EMMC_DRIVE_CHAR, &sz_avail, &sz_total) ) {
		io_out_string( " inaccessible.\r\n" );
	} else {
		U64 sz_use = sz_total-sz_avail;
		S16 uu = 0;
		while ( sz_use > 128*1024 ) {
			sz_use /= 1024;
			uu++;
		}
		S16 ut = 0;
		while ( sz_total > 128*1024 ) {
			sz_total /= 1024;
			ut++;
		}
		snprintf ( glob_tmp_str, sizeof(glob_tmp_str),
				" uses %llu %sb of %llu %sb.\r\n",
				sz_use, unit[uu], sz_total, unit[ut] );
		io_out_string( glob_tmp_str );

		if ( FILE_OK == file_initListing( EMMC_DRIVE ) ) {

			io_out_string ( "Directory content for eMMC\r\n" );

			U8 const fNameMax = 32;
			char fName[fNameMax];
			U32 fSize;
			Bool isDir;
			struct tm when;
			while ( FILE_OK == file_getNextListElement (
							fName, fNameMax, &fSize, &isDir, &when) ) {

				char listing[20+fNameMax];
				snprintf ( listing, 20+fNameMax, "%10lu\t%s\t%04d-%02d-%02d %02d:%02d:%02d\t%s\r\n",
								fSize, isDir ? "DIR" : "FILE",
								when.tm_year, when.tm_mon, when.tm_mday,
								when.tm_hour, when.tm_min, when.tm_sec,
								fName );
				io_out_string ( listing );
			}
			io_out_string ( "\r\n" );
		}
	}

}

S16 FSYS_Setup ( void ) {

    //  Make subfolders for all file handling modules

# if defined(OPERATION_AUTONOMOUS)
    char const* dlf_dir = dlf_LogDirName();

    if ( !f_exists(dlf_dir) ) {
        if ( FILE_OK != file_mkDir( dlf_dir ) ) {
            return FSYS_FAIL;
        }
    }
# endif

# if defined(OPERATION_NAVIS)
    char const* prof_dir = profile_manager_LogDirName();

    if ( !f_exists(prof_dir) ) {
        if ( FILE_OK != file_mkDir( prof_dir ) ) {
            return FSYS_FAIL;
        }
    }
# endif

    char const* syslog_dir = syslog_LogDirName();

    if ( !f_exists(syslog_dir) ) {
        if ( FILE_OK != file_mkDir( syslog_dir ) ) {
            return FSYS_FAIL;
        }
    }

    FSYS_Info();

    return FSYS_OK;
}
