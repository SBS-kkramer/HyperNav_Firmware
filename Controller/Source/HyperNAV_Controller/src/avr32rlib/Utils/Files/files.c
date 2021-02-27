/*! \file files.c *************************************************************
 *
 * \brief File handling library.
 *
 *
 *      @author      Diego, Satlantic Inc.
 *      @date        2010-11-10
 *
 * @revised
 * 2011-10-25	-DAS- Power loss handling feature added to close all open files

 **********************************************************************************/

#include "compiler.h"
#include "string.h"
#include "stdio.h"
#include "avr32rerrno.h"
#include "files.h"

#include "file.h"

#ifdef USE_ATMEL_FS
#include "fsaccess.h"
#include "navigation.h"
#include "fat.h"
#endif

#ifdef USE_FATFS
#include "ff.h"
#include "time.h"
#endif


#ifdef USE_FATFS
//! Partition table
PARTITION VolToPart[] = {
        {0, 1},    /* Logical drive 0 ==> Physical drive 0 (eMMC), 1st partition */
        {1, 0}     /* Logical drive 1 ==> Physical drive 1, auto detection */
    };
#endif


//*****************************************************************************
// Local variables
//*****************************************************************************
// Array of open files handlers
static fHandler_t* openFiles[FILE_MAX_FILES] = {0};
static char nRegFiles = 0;


//*****************************************************************************
// Local functions
//*****************************************************************************
static void registerFileHandler(fHandler_t* file);
static void unregisterFileHandler(fHandler_t* file);

//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//-----------------------------------------------------------------------------
// FATFs
//-----------------------------------------------------------------------------
#ifdef USE_FATFS

// File System objects for available volumes (logical drives)
static FATFS _fs[_VOLUMES];

// Directory to be listed
static DIR g_listedDirInstance;
static DIR* g_listedDir = &g_listedDirInstance;


//! \brief Open file.
S16 f_open(const char *pathname, U16 flags, fHandler_t* file)
{
	BYTE	access = FA_READ;

	// Do not allow to open more files than we can keep track of
	if(nRegFiles >= FILE_MAX_FILES)
	{
		avr32rerrno = EFOPEN;
		return FILE_FAIL;
	}

	// Sanity check
	if(pathname == NULL || file == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Assemble flags mask
	 if(Tst_bits(flags, O_CREAT))
		access |= FA_OPEN_ALWAYS;
	 if(Tst_bits(flags, O_RDWR))
		access |= FA_READ | FA_WRITE;
	 if(Tst_bits(flags, O_WRONLY))
	 {
		access |= FA_WRITE;
		access &= ~FA_READ;
	 }

//	if(flags | O_RDONLY)
//	{
//		access |= FA_READ;
//		access &= ~FA_WRITE;
//	}

	if(FATFs_f_open (file, pathname, access) == FR_OK)
	{
		// Advance file pointer to end of file if opening to append
		if(Tst_bits(flags, O_APPEND ))
			FATFs_f_lseek(file, FATFs_f_size(file));

		// Keep track of open files
		registerFileHandler(file);

		return FILE_OK;
	}
	else
	{
		avr32rerrno = EFOPEN;
		return FILE_FAIL;
	}
}


//! \brief Close a file.
S16 f_close(fHandler_t* file)
{
	// Sanity check
	if(file == NULL )
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	FATFs_f_close(file);

	// Unregister file handler
	unregisterFileHandler(file);

	return FILE_OK;
}


//! \brief Close all open files
//! @note This function is provided to implement the power loss shut down. All file opened using f_open() will be closed
void f_closeAll(void)
{
	unsigned char i;

	for(i=0; i<FILE_MAX_FILES; i++)
		if(openFiles[i] != 0)
			f_close(openFiles[i]);
}


//! \brief Write to a file.
S32 f_write(fHandler_t* file, const void *buf, U16 count)
{
	UINT wr;

	// Sanity check
	if(file == NULL || buf == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	if( FATFs_f_write(file,buf,count,&wr) != FR_OK)
	{
		avr32rerrno = EFERR;
		return -1;
	}

	return wr;
}


//! \brief Read from a file.
S32 f_read(fHandler_t* file, void *buf, U16 const count)
{
	UINT rd;
	UINT const toRd = count;

	// Sanity check
	if(file == NULL || buf == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	if(FATFs_f_read(file,buf,toRd,&rd) != FR_OK)
	{
		avr32rerrno = EFERR;
		return -1;
	}
	return rd;
}


//! \brief Get the position in the file
S32   f_getPos(fHandler_t* file)
{
	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	 return FATFs_f_tell(file);

}


//! \brief Change the position in the file
S16  f_seek(fHandler_t* file, U32 pos , U8 whence)
{
	DWORD offset;

	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	switch(whence)
	{
	case FS_SEEK_SET:		offset = pos; break;
	case FS_SEEK_END:		offset = FATFs_f_size(file) - pos; break;
	case FS_SEEK_CUR_RE:	offset = FATFs_f_tell(file) - pos; break;
	case FS_SEEK_CUR_FW:	offset = FATFs_f_tell(file) + pos; break;
	default:				offset = pos; break;
	}

	if(FATFs_f_lseek(file,offset) != FR_OK)
	{
		avr32rerrno = EFERR;
		return FILE_FAIL;
	}

	return FILE_OK;
}


//! \brief Check if at the beginning of file
S16 f_bof(fHandler_t* file)
{
	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	return FATFs_f_tell(file) == 0;
}


//! \brief Check if at the end of file
S16 f_eof(fHandler_t* file)
{
	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	return FATFs_f_eof(file);
}


//! \brief Get the size of an open file
S32 f_getSize(fHandler_t* file)
{
	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	return FATFs_f_size(file);
}


//! \brief Initialize file system
Bool f_fsProbe(void)
{
	DWORD nclst;
	int vol;
	char volStr[4];
	FATFS *fSys;

	// Check drives space. This will trigger necessary hardware initialization routines.
	// Register work areas for both drives
	for(vol=0; vol<_VOLUMES; vol++)
		FATFs_f_mount(vol, &_fs[vol]);

	// Get free space in each drive (this access will trigger low level disk initializations)
	for(vol=0; vol<_VOLUMES; vol++)
	{
		snprintf(volStr, 4, "%d:",vol);
		if(FATFs_f_getfree(volStr,&nclst, &fSys) != FR_OK)
			break;
	}

	// Did any disk init fail?
	if(vol<_VOLUMES)
	{
		// Unregister work areas
		for(vol=0; vol<_VOLUMES; vol++)
				FATFs_f_mount(vol, NULL);

		// Return error condition
		return FALSE;
	}

	// Success
	return TRUE;
}


//! \brief Check whether a file exists
Bool f_exists(const char* filename)
{
	FILINFO fileInfo;

	if(filename == NULL)
	{
		avr32rerrno = EPARAM;
		return FALSE;
	}

	return 	FATFs_f_stat(filename, &fileInfo) == FR_OK;
}


//! \brief Delete a file
S16 f_delete(const char* filename)
{
	if(FATFs_f_unlink(filename) != FR_OK)
		return FILE_FAIL;

	return FILE_OK;
}


//! \brief Move a file
S16 f_move(const char* src, const char* dst)
{
	if ( !src || !dst ) {
		return FILE_FAIL;
	}

	//	FATFs_f_rename() does not accept leading drive designators.
	//	The destination drive designator is implicitly assumed to
	//	be the source drive designator.
	//	In other words, the function does not work across drives.
	const char* use_dst;

	if ( strlen(dst) > 3 && dst[1] == ':' && dst[2] == '\\' ) {

		//	The destination has a drive designator.
		//	Now make sure that the source has the same.
		//	Otherwise, fail right here.

		if ( strlen(src) <= 3
		  || src[2] != '\\'
		  || src[1] != ':'
		  || src[0] != dst[0]) {
			return FILE_FAIL;
		}

		//	Source and destination have the same drive.
		//	Ignore drive in destination string.
		use_dst = dst+3;
	} else {
		//	The destination did not contain a drive designator.
		//	Use as is.
		use_dst = dst;
	}

	if(FATFs_f_rename(src,use_dst) != FR_OK)
		return FILE_FAIL;

	return FILE_OK;
}



//! \brief Get the total and available space in a drive
S16 f_space(char drive, U64* avail, U64* total)
{
	FATFS *fs;
	DWORD fre_clust;
	char driveStr[3];

	sprintf(driveStr,"%c:",drive);

	/* Get volume information and free clusters of drive 1 */
	if(FATFs_f_getfree(driveStr, &fre_clust, &fs) != FR_OK)
		return FILE_FAIL;

	/* Get total sectors and free sectors */
	DWORD tot_sect = (fs->n_fatent - 2) * fs->csize;
	DWORD fre_sect = fre_clust * fs->csize;

#if _MAX_SS == 512
	*avail = fre_sect * 512;
	*total = tot_sect * 512;
#else
	*avail = fre_sect * fs->ssize /* 512 */;
	*total = tot_sect * fs->ssize /* 512 */;
#endif

	return FILE_OK;
}



//! \brief Initialize file/directory listing in the given directory.
S16 file_initListing(const char* dirName)
{
	if(FATFs_f_opendir(g_listedDir, dirName) == FR_OK)
		return FILE_OK;
	else
		return FILE_FAIL;
}


//! \brief Get the next element of the file list.
S16 file_getNextListElement(char* name, U8 maxName, U32* size, Bool *isDir, struct tm* timetag)
{
	FILINFO	finfo;

	// Sanity check
	if(name == NULL || size == NULL || isDir == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	*size = 0;
	*isDir = FALSE;
	*name = '\0';

	if(FATFs_f_readdir(g_listedDir, &finfo) != FR_OK)
		return FILE_FAIL;

	if(finfo.fname[0] == '\0')
		return FILE_LIST_END;

	// Copy item info
	snprintf(name, maxName,"%s", finfo.fname);
	*size = finfo.fsize;
	*isDir = finfo.fattrib & AM_DIR;

	// Time tag (See FATFs 'FILINFO' documentation for time format)
	if(timetag != NULL)
	{
		timetag->tm_year = (finfo.fdate >> 9) + 1980;
		timetag->tm_mon = (finfo.fdate >> 5) & 15;
		timetag->tm_mday = finfo.fdate & 31;
		timetag->tm_hour = (finfo.ftime >> 11);
		timetag->tm_min = (finfo.ftime >> 5) & 63;
		timetag->tm_sec = (finfo.ftime & 31)*2;
	}


	return FILE_OK;
}



//! \brief Create a directory
S16 file_mkDir(const char* dirName)
{
	// Sanity check
	if(dirName == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	if(FATFs_f_mkdir(dirName) != FR_OK)
		return FILE_FAIL;

	return FILE_OK;
}


//! \brief Check for directory empty
Bool file_isDirEmpty(const char* dirName)
{
	char name;
	U32 size;
	Bool isDir;

	// An empty directory contains only two list elements (the '.' and '..' directories)
	// which are always listed first.
	file_initListing(dirName);

	// List '.' and '..'
	file_getNextListElement(&name, 1, &size, &isDir, NULL);
	file_getNextListElement(&name, 1, &size, &isDir, NULL);

	if(file_getNextListElement(&name, 1, &size, &isDir, NULL) != FILE_LIST_END)
		return FALSE;
	else
		return TRUE;
}

//! \brief Unmount(unregister) a volume
S16 f_unmount(char vol)
{
	if(FATFs_f_mount(vol,NULL) == FR_OK)
		return FILE_OK;
	else
		return FILE_FAIL;
}

#endif	// #ifdef USE_FATFS


//-----------------------------------------------------------------------------
// ATMEL FS
//-----------------------------------------------------------------------------
#ifdef USE_ATMEL_FS


//! \brief Open file.
S16 f_open(const char *pathname, U16 flags, fHandler_t* file)
{
	int fd;

	// Sanity check
	if(pathname == NULL || file == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Open file using fsaccess lib
	fd = open(pathname, flags);

	if(fd == -1)
	{
		avr32rerrno = EFOPEN;
		return FILE_FAIL;
	}
	else
	{
		*file = fd;
		return FILE_OK;
	}
}


//! \brief Close a file.
S16 f_close(fHandler_t* file)
{
	// Sanity check
	if(file == NULL )
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Sanity check
	if(*file <0)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}
	// Close file using fsaccess
	close(*file);

	return FILE_OK;
}


//! \brief Write to a file.
S32 f_write(fHandler_t* file, const void *buf, U16 count)
{
	// Sanity check
	if(file == NULL || buf == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return -1;
	}
	// Write a file using fsaccess
	return write(*file, buf, count);
}


//! \brief Read from a file.
S32 f_read(fHandler_t* file, void *buf, U16 count)
{
	// Sanity check
	if(file == NULL || buf == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return -1;
	}
	// Read a file using fsaccess
	return read(*file, buf, count);
}


//! \brief Get the position in the file
S32   f_getPos(fHandler_t* file)
{
	S32 position;

	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	// Sanity check
	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	// Take the mutex for nav access
	 fsaccess_take_mutex();

	 // Set navigator to selected file (the handler is actually a navigator id)
	 if(!nav_select(*file))
	 {
		 avr32rerrno = ENAV;
		 fsaccess_give_mutex();
		 return -1;
	 }

	 // Get file position
	 position = file_getpos();
	 fsaccess_give_mutex();
	 return position;
}


//! \brief Change the position in the file
S16  f_seek(fHandler_t* file, U32 pos , U8 whence)
{
	// Sanity check
	if(file == NULL)
	{
		avr32rerrno = EPARAM;
		return -1;
	}

	// Sanity check
	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Take the mutex for nav access
	fsaccess_take_mutex();

	// Set navigator to selected file (the handler is actually a navigator id)
	if(!nav_select(*file))
	{
		avr32rerrno = ENAV;
		fsaccess_give_mutex();
		return FILE_FAIL;
	}

	if(!file_seek(pos,whence))
	{
		// Flag error
		avr32rerrno = EBADFPOS;
		fsaccess_give_mutex();
		return FILE_FAIL;
	}

	fsaccess_give_mutex();
	return FILE_OK;
}


//! \brief Check if at the beginning of file
S16 f_bof(fHandler_t* file)
{
	S16 ret;

	// Sanity check
	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Take the mutex for nav access
	fsaccess_take_mutex();

	// Set navigator to selected file (the handler is actually a navigator id)
	if(!nav_select(*file))
	{
		avr32rerrno = ENAV;
		fsaccess_give_mutex();
		return FILE_FAIL;
	}

	// Query for beginning of file
	ret = file_bof();

	if(ret == 0xFF)
		ret = -1;

	fsaccess_give_mutex();
	return ret;
}


//! \brief Check if at the end of file
S16 f_eof(fHandler_t* file)
{
	S16 ret;

	// Sanity check
	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Take the mutex for nav access
	fsaccess_take_mutex();

	// Set navigator to selected file (the handler is actually a navigator id)
	if(!nav_select(*file))
	{
		avr32rerrno = ENAV;
		fsaccess_give_mutex();
		return FILE_FAIL;
	}

	// Query for end of file
	ret = file_eof();

	if(ret == 0xFF)
		ret = -1;

	fsaccess_give_mutex();
	return ret;
}


//! \brief Get the size of an open file
S32 f_getSize(fHandler_t* file)
{
	// Sanity check
	if(*file < 0)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	return fsaccess_file_get_size(*file);
}


//! \brief Check whether a file exists
Bool f_exists(const char* filename)
{
	if(filename == NULL)
	{
		avr32rerrno = EPARAM;
		return FALSE;
	}

	// Try to to set navigator on queried file. Will return FALSE if 'filename' does not exist.
	return nav_setcwd((FS_STRING)filename, TRUE, FALSE);
}


//! \brief Delete a file
S16 f_delete(const char* filename)
{
	//!note This procedure was copied from the AVR32 FAT demo 'rm' (remove) command.
	Fs_index sav_index;
	Bool deleted = FALSE;

	// Sanity check
	if(filename == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Try to to set navigator on arg1 folder or file.
	if (!nav_setcwd((FS_STRING)filename, TRUE, FALSE))
	{
		avr32rerrno = ENOFILE;
		return FILE_FAIL;
	}

	// Save current nav position. (apparently 'nav_file_del()' may fool with this nav index)
	sav_index = nav_getindex();
	// Try to delete the file
	deleted = nav_file_del(FALSE);
	// Restore nav position.
	nav_gotoindex(&sav_index);

	if(!deleted)
	{
		avr32rerrno = ENAV;
		return FILE_FAIL;
	}

	return FILE_OK;
}


//! \brief Move a file
S16 f_move(const char* src, const char* dst)
{
	//!note This procedure was copied from the AVR32 FAT demo 'mv' (move) command.
	Fs_index sav_index;
	Bool moved = FALSE;

	// Sanity check
	if(src == NULL || dst == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Try to to set navigator on src folder or file.
	if (!nav_setcwd((FS_STRING)src, TRUE, FALSE))
	{
		avr32rerrno = ENOFILE;
		return FILE_FAIL;
	}

	// Save current nav position.
	sav_index = nav_getindex();
	// Try to rename the file
	moved = nav_file_rename((FS_STRING)dst);
	// Restore nav position.
	nav_gotoindex(&sav_index);

	if(!moved)
	{
		avr32rerrno = ENAV;
		return FILE_FAIL;
	}

	return FILE_OK;
}



//! \brief Get the total and available space in a drive
S16 f_space(char drive, U64* avail, U64* total)
{
	//!note This procedure was copied from the AVR32 FAT demo 'df' (disk free) command.
	Fs_index sav_index;

	// Sanity check
	if(avail == NULL || total == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	drive -= 'A';
	if(drive >= nav_drive_nb())
	{
		avr32rerrno = ENODRIVE;
		return FILE_FAIL;
	}

	// Save current nav position.
    sav_index = nav_getindex();
    // Select drive.
    nav_drive_set(drive);
    // Try to mount.
    if (nav_partition_mount())
    {
    	*avail = nav_partition_freespace() << FS_SHIFT_B_TO_SECTOR;
        *total = nav_partition_space() << FS_SHIFT_B_TO_SECTOR;
    }
    else
    {
    	avr32rerrno = ENAV;
        // Restore nav position.
        nav_gotoindex(&sav_index);
    	return FILE_FAIL;
    }

    // Restore nav position.
    nav_gotoindex(&sav_index);
    return FILE_OK;
}



//! \brief Initialize file/directory listing in the given directory.
S16 file_initListing(const char* dirName)
{
	//!note This procedure is based on the AVR32 FAT demo 'cd' (change directory) command.
	char dirNameFixed[MAX_FILE_PATH_LENGTH+2];

	// Sanity check
	if(dirName == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	// Append the '/' char for the nav_setcwd to enter the chosen directory.
//	snprintf(dirNameFixed,sizeof(dirNameFixed)-1,"%s/",dirName);

	//  Changed back by Burkhard 2010-12-30.
	//	Using the '/' does not work for me.

	snprintf(dirNameFixed,sizeof(dirNameFixed)-1,"%s",dirName);

	// Try to to set navigator on src folder or file.
	if (!nav_setcwd((FS_STRING)dirNameFixed, TRUE, FALSE))
	{
		avr32rerrno = ENOFILE;
		return FILE_FAIL;
	}

    // Try to sort items by folders
	if (!nav_filelist_first(FS_DIR))
	{
		// Sort items by files
		nav_filelist_first(FS_FILE);
	}
	// reset filelist before to start the listing
	nav_filelist_reset();

	return FILE_OK;
}


//! \brief Get the next element of the file list.
S16 file_getNextListElement(char* name, U8 maxName, U32* size, Bool *isDir, struct tm* timetag)
{
	//!note This procedure is based on the AVR32 FAT demo 'ls' (list) command.

	// Sanity check
	if(name == NULL || size == NULL || isDir == NULL)
	{
		avr32rerrno = EPARAM;
		return FILE_FAIL;
	}

	*size = 0;
	*isDir = FALSE;
	*name = '\0';

	if(nav_filelist_set(0, FS_FIND_NEXT))
	{
		if(!nav_file_name((FS_STRING)name, maxName, FS_NAME_GET, TRUE))
		{
			avr32rerrno  = ENAV;
			return FILE_FAIL;
		}

		*size = nav_file_lgt();
		*isDir = nav_file_isdir();

		// Time tag (not available in ATMEL FS)
		if(timetag != NULL)
		{
			timetag->tm_year = 0;
			timetag->tm_mon = 0;
			timetag->tm_mday = 0;
			timetag->tm_hour = 0;
			timetag->tm_min = 0;
			timetag->tm_sec = 0;
		}

		return FILE_OK;
	}
	else
		return FILE_LIST_END;
}

#endif	// #ifdef USE_ATMEL_FS


//*****************************************************************************
// Local functions
//*****************************************************************************

// Register file handler in array of open files
static void registerFileHandler(fHandler_t* file)
{
	unsigned char i;

	// Look for first available spot in array
	for(i=0; i<FILE_MAX_FILES && openFiles[i]!=0; i++);

	// Record handler
	if(i<FILE_MAX_FILES)
	{
		openFiles[i] = file;
		nRegFiles++;
	}
}

// Remove file handler from array of open files
static void unregisterFileHandler(fHandler_t* file)
{
	unsigned char i;

	// Look for handler to unregister
	for(i=0; i<FILE_MAX_FILES; i++)
	{
		// Remove handler from list
		if(openFiles[i] == file)
		{
			openFiles[i] = 0;
			nRegFiles--;
		}
	}
}
