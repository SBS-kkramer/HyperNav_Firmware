/*
 * CRC.h
 *
 *  Created on: Dec 9, 2010
 *      Author: scott
 */

void CRC_reset(void);

U32 CRC_updateCRC32(U32 crc, U8 c);

/*
To calculate a CRC, the following three steps must be followed:

   1. Initialize the CRC value. CRC-32 starts with an initial value of 0xffffffffL.

   2. For each byte of the data starting with the first byte, call the
      function update_crc_32() to recalculate the value of the CRC.

   3. Only for CRC-32: When all bytes have been processed, take the
      one's complement of the obtained CRC value.  (crc_32    ^= 0xffffffffL;)

 */
