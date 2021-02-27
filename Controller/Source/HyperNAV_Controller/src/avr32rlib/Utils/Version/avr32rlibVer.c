/*! \file avr32rlibVer.c ***********************************************************
 *
 * \brief AVR32 Resources Library Version Information
 *
 *      @author      Diego Sorrentino, Satlantic Inc.
 *      @date        2011-1-31
 *
 **********************************************************************************/

#include "compiler.h"
#include "avr32rlibVer.h"


//*****************************************************************************
// Version definition
//*****************************************************************************
#define	VER_MAJOR	1
#define	VER_MINOR	4
#define	VER_PATCH	0


// Library variants (customizations) information
// Variants:
#define AVR32RLIB_DEFAULT_VAR	0	// Default variant

#if (AVR32RLIB_VARIANT==AVR32RLIB_DEFAULT_VAR)
#define AVR32RLIB_VAR_DESCRIPTION	"Default"
#else
#error "AVR32RLIB - Unknown or unspecified variant, cannot build."
#endif


//*****************************************************************************
// Exported Functions Implementations
//*****************************************************************************

//! \brief AVR32 Resources Library Version
void avr32rlib_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[])
{
	if(major != NULL)	*major = VER_MAJOR;
	if(minor != NULL)	*minor = VER_MINOR;
	if(patch != NULL)	*patch = VER_PATCH;
	if(variant != NULL)	*variant = AVR32RLIB_VARIANT;
	if(var_desc != NULL) *var_desc = AVR32RLIB_VAR_DESCRIPTION;
}

