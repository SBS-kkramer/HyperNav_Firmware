/*! \file io_funcs.h
 *
 *  \brief Wrapper functions for basic telemetry functions.
 *
 *  @author      B.Plache
 *  @date        2010-11-15
 */

#ifndef IO_FUNCS_H_
#define IO_FUNCS_H_

# include <compiler.h>
# include <time.h>

# define IO_OK		 0
# define IO_FAIL	-1

//!	\brief	temporary function to make debugging simpler
void io_out_stackSize ( char* msg );
void io_out_heap ( char* msg );
void io_out_memUsage ( char* msg );

//! \brief Send string through telemetry port
//! @param string	String to be sent, must be NUL terminated.
//! @return Number of bytes sent, or -1 if an error occurred (check 'nsyserrno').
S16 io_out_string ( char const* const string );

F64 printAbleF64 ( F64 val );
F32 printAbleF32 ( F32 val );

//! \brief Workaround for printf() bug that hangs for %.<p>f formats
//!			for some values of the argument.
//! @param string		String to be assigned to.
//! @param stringLen	Length of string available.
//! @param value		Value to be formatted.
//! @param precisions	Number of digits past decimal point.
//! @return Number of bytes sent, or -1 if an error occurred (check 'nsyserrno').
//S16 float2fstring_simple ( char* string, S16 stringLen, F32 value, U16 precision );

//! \brief Workaround for printf() bug that hangs for %.<p>f formats
//!			for some values of the argument.
//! @param value		Value to be formatted.
//! @return Number of bytes sent, or -1 if an error occurred (check 'nsyserrno').
S16 io_out_F32 ( char* format, F32 value );
S16 io_own_F32 ( char* pre, F32 value, int digits, char* post );

S16 io_out_F64 ( char* format, F64 value );
S16 io_own_F64 ( char* pre, F64 value, int digits, char* post );

S16 io_out_S32 ( char* format, S32 value );
S16 io_own_S32 ( char* pre, S32 value, char* post );
char* S32_to_str_dec ( S32 v, char string[], int strsize, int digits );

S16 io_out_S16 ( char* format, S16 value );
S16 io_print_U32 ( char* format, U32 value );

S16 io_dump_U32 ( U32 value, char* post );
S16 io_dump_X64 ( U64 value, char* post );
S16 io_dump_X32 ( U32 value, char* post );
S16 io_dump_X16 ( U16 value, char* post );
S16 io_dump_X8  ( U8  value, char* post );


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
//	@param	interactive
//			If set, echo back received characters,
//			(and interpret backspace to erase previous character)
//
S16 io_in_getstring ( char* string, S16 maxLen, U16 timeout, U16 idleDone );


//	List of predefined date/time output strings

# define DATE_TIME_APF_FORMAT "%m/%d/%Y %H:%M:%S"
# define DATE_TIME_ISO_FORMAT "%F %T"
//	FIXME	Decide if to use YYYY/mm/dd or YYYY-mm-dd
# define DATE_TIME_IO_FORMAT "%Y/%m/%d %H:%M:%S"

//!	\brief	Convert a time_t value to a date/time string
//!
//!	@param	epoch	Seconds since 1970
//!	@param	format	Format specification for result string, see strftime()
//!
//!	@return			pointer to a static character array, empty string on failure.
char* str_time_formatted ( time_t const epoch, const char* const format );

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
char* scan_to_next_EOL ( char start[], char** next_start );

#endif /* IO_FUNCS_H_ */
