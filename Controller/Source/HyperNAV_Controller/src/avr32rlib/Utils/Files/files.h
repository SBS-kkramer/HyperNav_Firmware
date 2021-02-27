/*! \file files.h**********************************************************
 *
 * \brief File handling library API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-11-10
 *
  ***************************************************************************/
#ifndef FILES_H_
#define FILES_H_

#include "compiler.h"
#include "time.h"
#include "fcntl.h"


// Choose file system implementation (uncomment choice)
#define	USE_FATFS
//#define USE_ATMEL_FS

#ifdef USE_ATMEL_FS
typedef int fHandler_t;
#endif

#ifdef USE_FATFS
#include "ff.h"
typedef FIL fHandler_t;
#endif


//*****************************************************************************
// Config
//*****************************************************************************
#define FILE_MAX_FILES	4	//!< Maximum simultaneous files open


//*****************************************************************************
// Input Constants
//*****************************************************************************

//! @note: Do not change these definitions.
#define  FS_SEEK_SET       0x00  //!< start at the beginning
#define  FS_SEEK_END       0x01  //!< start at the end of file and rewind
#define  FS_SEEK_CUR_RE    0x02  //!< start at the current position and rewind
#define  FS_SEEK_CUR_FW    0x03  //!< start at the current position and foward

# if 0  //  Already defined via "fcntl.h" << "sys/fcntl.h" << "_default_fcntl.h"
//! File access flags
//! @note: Do not change these definitions.
#ifndef	_FCNTL_
#define	O_RDONLY    0
#define	O_WRONLY    0x0001
#define	O_RDWR      0x0002
#define	O_APPEND    0x0008
#define	O_CREAT     0x0200
#endif
#endif

//*****************************************************************************
// Returned constants
//*****************************************************************************
#define FILE_OK		0
#define FILE_FAIL	-1
#define FILE_LIST_END 1


//*****************************************************************************
// Exported data types
//*****************************************************************************


//*****************************************************************************
// Exported functions
//*****************************************************************************

//-----------------------------------------------------------------------------
// FILE HANDLING
//-----------------------------------------------------------------------------

//! \brief Open file.
//! @param pathname		A file path in the "x:\\file.ext" format
//! @param flags		Flags to give file access rights. Should be:
//!								O_CREAT  - create file if not exist
//!                             O_APPEND - add data to the end of file
//!                             O_RDONLY - Read Only
//!                             O_WRONLY - Write Only
//!                             O_RDWR   - Read/Write
//! @param file		   OUTPUT: A valid file handler (if no errors occurred)
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16 f_open(const char *pathname, U16 flags, fHandler_t* file);


//! \brief Close a file.
//! @param file		 File handler of the file to be closed.
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16 f_close(fHandler_t* file);


//! \brief Write to a file.
//! @param file  File handler
//! @param buf   Pointer from where data are written.
//! @param count Amount of bytes to write
//! @return Amount of data written (-1 if error)
S32 f_write(fHandler_t* file, const void *buf, U16 count);


//! \brief Read from a file.
//! @param file  File handler
//! @param buf   Pointer for data that are read.
//! @param count Amount of bytes to read
//! @return Amount of data read (-1 if error)
S32 f_read(fHandler_t* file, void *buf, U16 count);


//! \brief Get the position in the file
//! @return    Position in file (-1 if an error occurred)
S32 f_getPos(fHandler_t* file);


//! \brief Change the position in the file
//!
//! @param 	   file    File handler
//! @param     pos     Number of byte to seek
//! @param     whence  direction of seek:
//!                        FS_SEEK_SET   , start at the beginning and foward
//!                        FS_SEEK_END   , start at the end of file and rewind
//!                        FS_SEEK_CUR_RE, start at the current position and rewind
//!                        FS_SEEK_CUR_FW, start at the current position and foward
//!
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16  f_seek(fHandler_t* file, U32 pos , U8 whence);


//! \brief Check if at the beginning of file
//! @return    1     The position is at the beginning of file
//! @return    0     The position isn't at the beginning of file
//! @return    -1    Error
S16 f_bof(fHandler_t* file);


//! \brief Check if at the end of file
//! @return    1     The position is at the end of file
//! @return    0     The position isn't at the end of file
//! @return    -1    Error
S16 f_eof(fHandler_t* file);


//! \brief Get the size of an open file
//! @param fd   File handler.
//! @return 	Size of the file  (-1 if an error occurred).
S32 f_getSize(fHandler_t* file);



//-----------------------------------------------------------------------------
// FILE SYSTEM HANDLING
//-----------------------------------------------------------------------------

//! \brief Initialize file system
//! @return TRUE: File system successfully initialized. FALSE: Could not initialize file system.
Bool f_fsProbe(void);


//! \brief Check whether a file exists
//! @param filename A file/directory path in the "x:\\file.ext" format
//! @return TRUE: The file exists. FALSE: The file does not exist.
Bool f_exists(const char* filename);


//! \brief Delete a file
//! Delete a file or entire directory recursively.
//! @param filename	A file path in the "x:\\file.ext" format
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16 f_delete(const char* filename);


//! \brief Move a file
//! @param src	Source path in the "x:\\file.ext" format
//! @param dst	Destination path in the "x:\\file.ext" format (NOTE: Do not use drive number if using FATFs!)
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
//! @note WARNING: Does not work when moving files across different directories.
S16 f_move(const char* src, const char* dst);


//! \brief Get the total and available space in a drive
//! @param drive	An uppercase char ('A','B',etc) specifying the drive to be analyzed
//! @param avail	OUTPUT: Available drive space in bytes.
//! @param total	OUTPUT: Total drive space in bytes.
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16 f_space(char drive, U64* avail, U64* total);


//! \brief Initialize file/directory listing in the given directory.
//! Use in conjunction with 'file_getNextListElement()' to list the directoy contents.
//! @param	dirName	Directory path to list
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno'
S16 file_initListing(const char* dirName);


//! \brief Get the next element of the file list.
//! @param	name	OUTPUT: Element (file or directory) name.
//! @param	maxName	Maximum space allocated (including null termination) for name
//! @param	size	OUTPUT: Element size (in bytes)
//! @param	isDir	OUTPUT: TRUE if element is a directory, FALSE otherwise
//! @param  timetag OUTPUT: Last modified timetag (pass NULL if not needed)
//! @return FILE_OK: Success	FILE_FAIL: Error, check 'nsyserrno' FILE_LIST_END: No more elements in the list
S16 file_getNextListElement(char* name, U8 maxName, U32* size, Bool *isDir, struct tm* timetag);


//! \brief Create a directory
//! @param dirName	Full path (drive included) of the new directory to be created. (Ex. "0:/dir/newDir")
S16 file_mkDir(const char* dirName);

//! \brief Check for directory empty
//! @param	dirName 	Directory name to check
//! @return TRUE: The directory is empty	FALSE: The directory is NOT empty
Bool file_isDirEmpty(const char* dirName);


//! \brief Close all open files
//! @note This function is provided to implement the power loss shut down. All file opened using f_open() will be closed
void f_closeAll(void);


#endif /* FILES_H_ */
