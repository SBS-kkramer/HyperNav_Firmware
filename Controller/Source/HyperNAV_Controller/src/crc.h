/*
 * crc.h
 *
 *  Created on: Dec 15, 2010
 *      Author: plache
 */

#ifndef CRC_H_
#define CRC_H_

# include "compiler.h"

U16 CRC_Upd ( U8 nextByte, U16 crc_so_far );
U16 fCrc16Bit(const U8 *msg);
U16 fCrc16BitN(const U8 *msg, U16 n );

#endif /* CRC_H_ */
