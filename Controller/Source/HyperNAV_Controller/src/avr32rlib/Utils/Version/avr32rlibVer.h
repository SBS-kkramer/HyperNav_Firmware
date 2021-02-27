/*! \file avr32rlibVer.h********************************************************
 *
 * \brief AVR32 Resources Library Version Information API
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2011-1-31
 *
  ***************************************************************************/

#ifndef AVR32RVER_H_
#define AVR32RVER_H_

#include "compiler.h"


//*****************************************************************************
// Exported functions
//*****************************************************************************

//! \brief AVR32 Resources Library Version
//! @param major	OUTPUT: Major revision number
//! @param minor	OUTPUT: Minor revision number
//! @param patch	OUTPUT: Patch revision number
//! @param variant	OUTPUT: Variant (customization) number
//! @param var_desc	OUTPUT: Variant (customization) description
void avr32rlib_ver(U8* major, U8* minor, U16* patch, U8* variant, const char* var_desc[]);



#endif /* AVR32RVER_H_ */
