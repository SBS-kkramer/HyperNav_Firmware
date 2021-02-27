/*! \file io_funcs.c
 *
 *  \brief Wrapper functions for basic telemetry functions.
 *
 *  @author      B.Plache
 *  @date        2010-11-15
 */

# include "io_funcs.controller.h"

# include "tasks.controller.h"

# include <math.h>
# include <string.h>
# include <stdio.h>
# include <sys/time.h>

# include "watchdog.h"
# include "telemetry.h"
# include "extern.controller.h"


static char* F64_to_string ( F64 f, char string[], int strlen, int dd ) {

  if ( !string || strlen<=1 ) return 0;

  int idx=0;

  if ( f<0 ) { string[idx++] = '-'; f = -f; }

  int    int_f = (int)floor(f);
  double dec_f = f - int_f;

  int mask = 10;
  while ( mask <= int_f ) mask *= 10;
  mask /= 10;

  do {
    int digit = int_f / mask;
    string[idx++] = '0' + digit;
    int_f -= digit * mask;
    mask /= 10;

  } while ( mask && idx<strlen-1 );

  if ( mask ) return 0;

  if ( dd>0 ) {
    string[idx++] = '.';

    while ( dd>0 && idx<strlen-1 ) {

      dec_f *= 10;

      int dig_f = (int)floor(dec_f);
      string[idx++] = '0' + dig_f;

      dec_f -= dig_f;
      dd--;
    }

    if (dd) return 0;
  }

  string [idx] = 0;

  return string;
}

static char* F32_to_string ( F32 f, char string[], int strlen, int dd ) {

  if ( !string || strlen<=1 ) return 0;

  int idx=0;

  if ( f<0 ) { string[idx++] = '-'; f = -f; }

  int    int_f = (int)floor(f);
  double dec_f = f - int_f;

  int mask = 10;
  while ( mask <= int_f ) mask *= 10;
  mask /= 10;

  do {
    int digit = int_f / mask;
    string[idx++] = '0' + digit;
    int_f -= digit * mask;
    mask /= 10;

  } while ( mask && idx<strlen-1 );

  if ( mask ) return 0;

  if ( dd>0 ) {
    string[idx++] = '.';

    while ( dd>0 && idx<strlen-1 ) {

      dec_f *= 10;

      int dig_f = (int)floor(dec_f);
      string[idx++] = '0' + dig_f;

      dec_f -= dig_f;
      dd--;
    }

    if (dd) return 0;
  }

  string [idx] = 0;

  return string;
}



char* S32_to_str_dec (S32  v, 
											char string[],
											int  strsize,
											int  digits
                     )
{
  if  (!string || strsize < 1)
		return 0;

  if  (!v && !digits)
	{
		string[0] = '0';
		string[1] = 0;
		return string;
	}

  int idx = 0;

  if  (v < 0)
	{
		string[idx++] = '-';
		v = -v;
	}

  int mask = 10;

  if  (!digits)
	{
    while  ( mask <= v )
			mask *= 10;
  }
	else
	{
    while  (--digits > 0)
			mask *= 10;

    //  This check guarantees that the string will not contain more digits than requested.
    if  (mask < v)
			v %= mask;
  }

  mask /= 10;

  do
	{
    int digit = v / mask;
    string[idx++] = '0' + digit;
    v -= digit * mask;
    mask /= 10;

  } while ( mask );

  string [idx] = 0;

  return string;
}



F32 printAbleF32 ( F32 val ) {

	if ( 0.0F == val || !finitef(val) ) {
		return 0.0F;
	} else if ( val > 0 ) {
		return fmaxf ( 1e-15F, fminf ( val, 1e15F ) );
	} else {
		return fmaxf ( -1e15F, fminf ( val, -1e-15F ) );
	}

}

F64 printAbleF64 ( F64 val ) {

	if ( 0.0 == val || !finite(val) ) {
		return 0.0;
	} else if ( val > 0 ) {
		return fmax ( 1e-15, fmin ( val, 1e15 ) );
	} else {
		return fmax ( -1e15, fmin ( val, -1e-15 ) );
	}
}


S16 io_out_string ( char const* const string ) {

	if ( !string ) return 0;
	return tlm_send ( (void*) string, strlen(string), 0 /* BLOCKING */ );
}

static S16 U32_to_string_simple ( char* string, S16 stringLen, U32 value ) {
	if (!string) return 0;
	return snprintf ( string, stringLen, "%lu", value );
}

S16 io_dump_U32 ( U32 value, char* post ) {
    char formatted [16];
    U32_to_string_simple (formatted, sizeof(formatted), value );
    //snprintf ( formatted_value, sizeof(formatted_value), "%s%ld", (value<0)?"-":"", value );
    return tlm_send ( (void*) formatted, strlen(formatted), 0 /* BLOCKING */ )
    		+ post ? io_out_string ( post ) : 0;
}

static S16 X64_to_string_simple ( char* string, S16 stringLen, U64 value ) {
	if (!string) return 0;
	return snprintf ( string, stringLen, "%016llx", value );
}

S16 io_dump_X64 ( U64 value, char* post ) {
    char formatted [24];
    X64_to_string_simple (formatted, sizeof(formatted), value );
    //snprintf ( formatted_value, sizeof(formatted_value), "%s%ld", (value<0)?"-":"", value );
    return tlm_send ( (void*) formatted, strlen(formatted), 0 /* BLOCKING */ )
    		+ post ? io_out_string ( post ) : 0;
}

static S16 X32_to_string_simple ( char* string, S16 stringLen, U32 value ) {
	if (!string) return 0;
	return snprintf ( string, stringLen, "%08lx", value );
}

S16 io_dump_X32 ( U32 value, char* post ) {
	char formatted [16];
	X32_to_string_simple (formatted, sizeof(formatted), value );
	//snprintf ( formatted_value, sizeof(formatted_value), "%s%ld", (value<0)?"-":"", value );
	return tlm_send ( (void*) formatted, strlen(formatted), 0 /* BLOCKING */ )
	+ post ? io_out_string ( post ) : 0;
}

static S16 X16_to_string_simple ( char* string, S16 stringLen, U16 value ) {
	if (!string) return 0;
	return snprintf ( string, stringLen, "%04hx", value );
}

S16 io_dump_X16 ( U16 value, char* post ) {
    char formatted [16];
    X16_to_string_simple (formatted, sizeof(formatted), value );
    //snprintf ( formatted_value, sizeof(formatted_value), "%s%ld", (value<0)?"-":"", value );
    return tlm_send ( (void*) formatted, strlen(formatted), 0 /* BLOCKING */ )
    		+ post ? io_out_string ( post ) : 0;
}

static S16 X8_to_string_simple ( char* string, S16 stringLen, U8 value ) {
	if (!string) return 0;
	return snprintf ( string, stringLen, "%02hx", (U16)value );
}

S16 io_dump_X8  ( U8  value, char* post ) {

	char formatted [16];
    X8_to_string_simple (formatted, sizeof(formatted), value );
    //snprintf ( formatted_value, sizeof(formatted_value), "%s%ld", (value<0)?"-":"", value );
    return tlm_send ( (void*) formatted, strlen(formatted), 0 /* BLOCKING */ )
    		+ post ? io_out_string ( post ) : 0;

}

S16 io_out_S32 ( char* format, S32 value ) {
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), format, value );
	return tlm_send ( (void*) glob_tmp_str, strlen(glob_tmp_str), 0 /* BLOCKING */ );
}

S16 io_own_S32 ( char* pre, S32 value, char* post ) {
        char valAsString[16];
        if( !S32_to_str_dec( value, valAsString, 16, 0 ) ) valAsString[0] = 0;

	return tlm_send ( (void*)  pre, strlen( pre), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) valAsString, strlen(valAsString), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) post, strlen(post), 0 /* BLOCKING */ );
}

S16 io_out_S16 ( char* format, S16 value ) {
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), format, value );
	return tlm_send ( (void*) glob_tmp_str, strlen(glob_tmp_str), 0 /* BLOCKING */ );
}

S16 io_print_U32 ( char* format, U32 value ) {
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str), format, value );
	return tlm_send ( (void*) glob_tmp_str, strlen(glob_tmp_str), 0 /* BLOCKING */ );
}

S16 io_out_F32 ( char* format, F32 value ) {

    snprintf ( glob_tmp_str, sizeof(glob_tmp_str), format, printAbleF32(value) );
    return tlm_send ( (void*) glob_tmp_str, strlen(glob_tmp_str), 0 /* BLOCKING */ );
}

S16 io_own_F32 ( char* pre, F32 value, int digits, char* post ) {

        char valAsString[32];
        if( !F32_to_string( value, valAsString, 32, digits ) ) valAsString[0] = 0;

	return tlm_send ( (void*)  pre, strlen( pre), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) valAsString, strlen(valAsString), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) post, strlen(post), 0 /* BLOCKING */ );
}

S16 io_out_F64 ( char* format, F64 value ) {

    snprintf ( glob_tmp_str, sizeof(glob_tmp_str), format, printAbleF64(value) );
    return tlm_send ( (void*) glob_tmp_str, strlen(glob_tmp_str), 0 /* BLOCKING */ );
}

S16 io_own_F64 ( char* pre, F64 value, int digits, char* post ) {

        char valAsString[32];
        if( !F64_to_string( value, valAsString, 32, digits ) ) valAsString[0] = 0;

	return tlm_send ( (void*)  pre, strlen( pre), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) valAsString, strlen(valAsString), 0 /* BLOCKING */ );
	     + tlm_send ( (void*) post, strlen(post), 0 /* BLOCKING */ );
}

# define TIMEDIFF(Early,Late) ( (double)(Late.tv_sec-Early.tv_sec)+((double)(Late.tv_usec-Early.tv_usec))/1000000.0)

//	\brief	Receive input from telemetry port
//
//	@param	string
//			caller provided character array,
//			to be filled with received characters,
//			stripped of terminating '\r' and/or '\n',
//			will be NUL terminated.
//	@param	maxLen
//			size of caller provided string
//	@param	timeout	[seconds]
//			function will latest return after timeout [zero: no timeout]
//	@param	idleTime [milliseconds]
//			return if no new input within idleTime [zero: no idle check]
//
S16 io_in_getstring ( char* string, S16 maxLen, U16 timeout, U16 idleDone ) {

	if ( 0 == string || maxLen <= 0 ) {
		return IO_FAIL;
	}

	S16 numRead = 0;

	enum {
		DR_notDone,
		DR_timeout,
		DR_idle,
		DR_abort,
		DR_CRLF
	} doneReason = DR_notDone;

	string[0] = 0;
	char c;

	struct timeval timeStart;
	if ( timeout) gettimeofday ( &timeStart, 0 );

	struct timeval timeNow;
	if ( timeout || idleDone ) gettimeofday ( &timeNow, 0 );

	struct timeval timeRcv;
	if ( idleDone) gettimeofday ( &timeRcv, 0 );

	do {
		gHNV_SetupCmdCtrlTask_Status = TASK_RUNNING;

		if ( 0 == tlm_recv ( &c, 1, TLM_PEEK | TLM_NONBLOCK ) ) {

            vTaskDelay( (portTickType)TASK_DELAY_MS( 50 ) );

		} else {

			tlm_recv( &c, 1, TLM_NONBLOCK );

			switch ( c ) {

			//	Backspace
			case '\b':	//	Interpret backspace and CTRL-C
			case 0x7F:	//
				if( numRead > 0) {
					tlm_send ( &c, 1, 0 );
					/*	Can be tidy and blot out previous
					c = ' ';
					tlm_send ( &c, 1, 0 );
					c = '\b';
					tlm_send ( &c, 1, 0 );
					*/
					numRead--;					// Decrease index (delete last char)
				}
				break;

			//	Interpret any of
			//		"...\r"
			//		"...\r\n"
			//		"...\n"
			//	as end-of-input indicator.
			case '\n':
			case '\r':
				if ( c == '\r' ) {
					tlm_send( &c, 1, 0 );

					//  Check if there follows a '\n' on the input line,
					//  (wait a tad) and if so, remove it.
                    vTaskDelay( (portTickType)TASK_DELAY_MS( 50 ) );
					if ( 1 == tlm_recv ( &c, 1, TLM_PEEK | TLM_NONBLOCK ) ) {
						if ( '\n' == c ) {
							tlm_recv ( &c, 1, 0 );
						} else {
							c = '\n';
						}
						tlm_send ( &c, 1, 0 );
						//  else there is another character,
						//	to be left for the next tlm_recv() call.
					} else {
						c = '\n';
						tlm_send ( &c, 1, 0 );
					}
				} else {
					//	Clean output
					c = '\r';
					tlm_send( &c, 1, 0 );
					c = '\n';
					tlm_send( &c, 1, 0 );
				}

				string[numRead] = 0;	// Null terminate command line
				doneReason = DR_CRLF;
				break;

			//	Ctrl-C: Abort all done so far
			case 0x03:
				string[0] = 0;
				numRead = 0;
				doneReason = DR_abort;
				break;

			//  Add all other characters to the string.
			//  Note: Non-printable characters are accepted
			default:
				tlm_send ( &c, 1, 0 );	// echo
				if ( numRead < maxLen ) {
					string [numRead] = c;	//	Append c to string
					numRead++;
				} else {
					//  Eat overflow until line terminates
				}
				break;

			}

			gettimeofday ( &timeRcv, 0 );
		}

		if ( timeout || idleDone ) {
			gettimeofday ( &timeNow, 0 );
		}

		if ( timeout ) {
			if ( TIMEDIFF(timeStart,timeNow)>=timeout ) {
				doneReason = DR_timeout;
			}
		}

		if ( idleDone) {
            if ( 1000*TIMEDIFF(timeRcv,timeNow)>=idleDone ) {
    			doneReason = DR_idle;
            }
		}

	} while ( doneReason == DR_notDone );

	if ( 0 == numRead ) {
		if ( DR_timeout == doneReason
		  || DR_idle == doneReason ) {
			return -1;
		} else {
			return 0;
		}
	} else {
		return numRead;
	}
}


# include "debug.h"

void io_out_stackSize ( char* msg ) {
# ifdef FREERTOS_USED
	unsigned portBASE_TYPE stack_size = uxTaskGetStackHighWaterMark( 0 );
# else
# define STACKORG 0xF000
    U32 const stack_size = (((U32)(&stack_size)) - STACKORG);
# endif

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str),
			"Stack space  %8lu  %s\r\n", stack_size, msg);
	io_out_string( glob_tmp_str );

}

void io_out_heap ( char* msg ) {

	snprintf ( glob_tmp_str, sizeof(glob_tmp_str),
			"Heap space  %5lu  %5lu  %5lu    %s\r\n",
			get_heap_total_used_size(),
			get_heap_curr_used_size(),
			get_heap_free_size(),
			msg);
	io_out_string( glob_tmp_str );

}

  #if (defined __GNUC__)
  #   include "malloc.h"
  #endif

void io_out_memUsage ( char* msg ) {

# ifdef FREERTOS_USED
	unsigned portBASE_TYPE stack_size = uxTaskGetStackHighWaterMark( 0 );
# else
# define STACKORG 0xF000
    U32 const stack_size = (((U32)(&stack_size)) - STACKORG);
# endif


# if 0
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str),
			"Heap space %5lu %5lu %5lu  Stack available %5lu  %s\r\n",
			get_heap_total_used_size(),
			get_heap_curr_used_size(),
			get_heap_free_size(),
			stack_size,
			msg);
# else
    struct mallinfo my_info=mallinfo();
	snprintf ( glob_tmp_str, sizeof(glob_tmp_str),
			"Heap %5lu %5lu %5lu %5lu %5lu %5lu %5lu Stack available %5lu  %s\r\n",
			(U32)my_info.arena,
			(U32)my_info.uordblks,
			(U32)my_info.fordblks,
			(U32)my_info.keepcost,
			(U32)my_info. ordblks,
			(U32)my_info.   hblks,
			(U32)my_info.   hblkhd,
			stack_size,
			msg);
# endif
	io_out_string( glob_tmp_str );


}

char* str_time_formatted ( time_t const epoch, const char* const format ) {

	static char result[64];

	struct tm* tp = gmtime(&epoch);
	if ( !tp )
		return 0;

	if ( !strftime ( result, sizeof(result), format, tp ) )
		return 0;

	return result;
}

/**
 *! \brief	Find the next non-empty line of text in a NUL terminated character array.
 *!
 *!	This function is destructive on the input character array.
 *!	Some '\r' and '\n' characters will be replaced by NUL characters.
 *!
 *!	The purpose of this function is to separate a character array into lines,
 *!	where lines are separated by '\r' and/or '\n' characters.
 *!
 *!	@param	start		pointer to position where to start scanning
 *! @param	next_start	will contain pointer to where to pick up the next search
 *!
 *! @return				Where the line data begins, of 0 if no line data to be found.
 */
char* scan_to_next_EOL ( char start[], char** next_start ) {

	//  Sanity check
	if ( (char*)0 == start ) return (char*)0;

	char* begin_of_line = start;

	//  Only return non-empty data, i.e. skip over separators at the beginning
	while ( *begin_of_line == '\r'
		 || *begin_of_line == '\n' )
		begin_of_line++;

	//  Check that we are not at the end of the data array
	if ( 0 == *begin_of_line ) return (char*)0;

	//  Scan through the data...
	char* end_of_line = begin_of_line;

	//  ...until either the end of data or the end of a line is encountered
	while ( *end_of_line != 0
		 && *end_of_line != '\r'
		 && *end_of_line != '\n' )
		end_of_line++;

	//  Terminate the line at the separator
	if ( *end_of_line == '\r'
	  || *end_of_line == '\n' ) {
		*end_of_line = 0;
		end_of_line++;

	}

	//  Caller may not want to use this feature
	if ( next_start ) {
		//  Continue next search here.
		//  This may point to the terminal NUL character
		//  or to the next data.
		*next_start = end_of_line;
	}

	return begin_of_line;
}

