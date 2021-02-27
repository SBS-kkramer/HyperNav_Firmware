/* \file dbg.h
 * \brief Debug prints in the standard error output
 *
 *
 * @author      Diego Sorrentino, Satlantic Inc.
 * @date        2010-07-26
 *
 ***************************************************************************/

#ifndef DBG_H_
#define DBG_H_


#ifdef DEBUG_PRN
#include "stdio.h"
#define DEBUG(format, ...)  {fprintf(stdout,"%s()::",__func__); fprintf(stdout, format, ##__VA_ARGS__); fprintf(stdout,"\r\n");}
#else
#define DEBUG(format,...)
#endif



#endif /* DBG_H_ */
