#ifndef PKT_H
#define PKT_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/pkt.c#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: pkt.c,v $
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * Revision 1.2  2002/10/08 23:52:36  swift
 * Combined header file into source file.
 *
 * Revision 1.1  2002/05/07 22:16:38  swift
 * Initial revision
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define PktChangeLog "$RCSfile: pkt.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

unsigned char PktMonitor(int status);
unsigned char PktType(void);
void Pkt1k(void);
void Pkt128b(void);

#endif /* PKT_H */

#include "xmodem.h"

/* store a fixed packet type */
static int fixed_pkt_type=0;

/* store success/failure of the most recent 8 packet xmit attempts */
static unsigned char pkt_history=0x38;

/*------------------------------------------------------------------------*/
/* function to maintain a record of the last 8 xmit attempts              */
/*------------------------------------------------------------------------*/
/**
   This function maintains a record of the success or failure of the eight
   most recent packet transmission attempts.  The bits of a byte are used to
   store and age the record.  An asserted bit indicates a failed attempt to
   transmit a packet.  This function should be called after each attempt is
   made to transmit a packet...each call causes the bits to be left-shifted
   by one bit so that only the most recent 8 xmit attempts are stored.

      \begin{verbatim}
      input:

         status ... The status of the most recent xmit attempt.  If the xmit
                    attempt was successful then 'OkPkt' should be passed to
                    this function.  Any other value indicates a failed xmit
                    attempt.

      output:

         This function returns the byte that records the history of the most
         recent eight transmissions.
      \end{verbatim}

   written by Dana Swift
*/
unsigned char PktMonitor(int status)
{
   /* left-shift to age the history */
   pkt_history<<=1;

   /* assert the least significant bit if the packet xmit was not OK */
   if (status!=OkPkt) pkt_history |= 0x01;

   return pkt_history;
}

/*------------------------------------------------------------------------*/
/* function to determine the packet type to use for the next xmit attempt */
/*------------------------------------------------------------------------*/
/**
   This function determines the packet type to use for the next xmit
   attempt.

      \begin{verbatim}
      output:

         This function returns a 128-byte (SOH) packet or a 1K (STX) packet
         depending on the following criteria:
         
         1. If a fixed packet type (either STX or SOH) has been requested then
            this function returns that fixed packet type.

         2. If the number of failed xmits exceeds 3 then a 128-byte (SOH)
            packet type is returned, otherwise a 1k (STX) packet type is
            returned.
      \end{verbatim}

   written by Dana Swift
*/
unsigned char PktType(void)
{
   int i,n,mask,pkt=SOH;

   /* check if a fixed packet type has been requested */
   if (fixed_pkt_type==SOH || fixed_pkt_type==STX) pkt=fixed_pkt_type;
   
   else
   {
      /* loop over each bit of history */
      for (mask=0x01,n=0,i=0; i<8; i++,mask<<=1)
      {
         /* count the number of recorded failed xmit attempts */
         if (pkt_history & mask) n++;
      }

      /* resort to 128 byte packets if the xmit history is poor */
      pkt = (n>3) ? SOH : STX;
   }
   
   return pkt;
}

/*------------------------------------------------------------------------*/
/* function to set a fixed 1K packet type                                 */
/*------------------------------------------------------------------------*/
void Pkt1k(void)
{
   fixed_pkt_type=STX;
}

/*------------------------------------------------------------------------*/
/* function to set a fixed 128 byte type                                  */
/*------------------------------------------------------------------------*/
void Pkt128b(void)
{
   fixed_pkt_type=SOH;
}
