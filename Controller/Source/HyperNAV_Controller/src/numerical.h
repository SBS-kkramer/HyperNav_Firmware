/*
 *  File: numerical.h
 *  Nitrate Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

/*
 *  All functions related to converting the spectrum
 *  into a nitrate value.
 */


# ifndef _NUMERICAL_H_
# define _NUMERICAL_H_

# error "numerical.h should not be included"

# if 0

# include <compiler.h>

//# define NUM_SATURATED	 2
# define NUM_EXTINCT 1
# define NUM_OK		 0
# define NUM_FAIL	-1

/*
 *   Processing types:
 *   0 = S-T-correction, fit for NO3, BL0, BL1
 *   1 = fit for NO3, SWA, BL0, BL1
 *   2 = fit for NO3, SWA, TSWA, BL0, BL1
 */

/*
 *  Define the maximum number of species the firmware can fit for.
 *  ALERT: Increasing NUM_MAX_SPECIES may have side effects,
 *         regarding running out of dynamic memory!!!
 */
# define NUM_MAX_SPECIES          3     //  NO3 + SWA + TSWA
# define NUM_MAX_FIT (NUM_MAX_SPECIES+2)	//  NO3 + SWA + TSWA + BL0 + BL1

/**
 *!	\brief	Rebuild NUM data from scratch (i.e. CAL file and processing configuration parameters).
 */
S16 NUM_Rebuild( char const* fullfilename );

/**
 *!	\brief	Load cal file into memory, needed for processing.
 *!	\return	NUM_OK if cal file ok, NUM_FAIL otherwise.
 */
S16 NUM_InitializeData ( void );

/**
 *! \brief	Check if CAL file derived data are present.
 */
bool NUM_HaveCoefs( void );


typedef enum {
	NUM_PROC_RESULT_INVALID=0,
    NUM_PROC_RESULT_FITTED,
    NUM_PROC_RESULT_FIXED
} num_process_result_source;


typedef struct Num_Process_Result {

	F64							nitrate;
	num_process_result_source	nitrate_source;
	F64							salinity;
	num_process_result_source	salinity_source;
	F64							third;
	num_process_result_source	third_source;
	F64							baseline_0;
	num_process_result_source	baseline_0_source;
	F64							baseline_1;
	num_process_result_source	baseline_1_source;

								//	(Square)Root of the Mean Square Error
	F64							rmse;

	//	Additional processing results

	F64							abs_at_254;
	F64							abs_at_350;

	F64							bromide_trace;

} num_process_result_t;

//struct Frame_Data;			//	Forward declaration
//enum light_or_dark_frame;	//	Froward declaration
/**
 *!	\brief	Convert spectral data into concentration values
 *!
 *!	@param	light_spectrum	full spectrum of light counts
 *!	@param	dark_spectrum	full spectrum of dark counts\
 *!	@param
 */

//!	\brief	Generate cal file directory name.
//!
//!	return	pointer to static char[]	if file name was generated
//!			0							if name could not be generated
char* NUM_MakeCalDirName ();

//!	\brief	Generate a cal file name for the revision letter.
//!
//!	@param	filename	provide the generated file name here.
//!	@param	maxlen		size of char array passed
//!	@param	letter		revision letter for file name (A-Z).
//!	@param	fallback_to_default		permit the letter '-' to be accepted as a fallback to the initial default file.
//!
//!	return	NUM_OK		if file name could be generated
//!			NUM_FAIL	if file name could not be generated
//S16 NUM_MakeCalFileName ( char* filename, size_t maxlen, U16 serialNumber, char letter, bool fallback_to_default );
char const* NUM_GetActiveCalFileName();
S16 NUM_CalFileSpecies();

S16 NUM_BuildDefaultCalFile( void );

//int    NUM_GenNumTempCoefs( void );
//double NUM_GetTempCoef ( int i );

//! \brief	Match wavelength to pixel value.
//! @param 	wavelength		The wavelength to be matched.
//! @param	direction
//
//  The wavelength / pixel logic is used in interval determination.
//  Typically, a wavelength range is converted into a pixel range.
//  The pixel range is defined by
//		wavelength_low <= wavelength[pixel] <= wavelength_high
//	The direction flag defines if the low or high limit is calculated.

F64  NUM_PixelToWavelength ( int pixel );

S16 NUM_WavelengthRange_to_PixelRange (
		F32	const wl_low,
		F32 const wl_high,
		U16* px_low,
		U16* px_high );

# endif

# endif  /*  ifndef  _NUMERICAL_H_  */
