# ifndef _NETWORK_H_
# define _NETWORK_H_

# include <stdint.h>

/*------------------------------------------------------------------------*/
/* function to register the modem with the iridium system                 */
/*------------------------------------------------------------------------*/
/*
 *  Return values:
 *    0: OK
 *   -1: Generic fail
 *   -2: Modem not responding (to "AT" command)
 *   -3: Modem is not an Iridium LBT
 *   -4: Modem not registered
 */
int16_t NTW_Register();


int16_t NTW_Search();

//  Check for continued connection via carrier detect
//
//  Return values:
//    0: OK
//   -1: Generic fail
//
int16_t NTW_ConnectionFailed();

//
//  Check connection state:
//  return 0: Is not connected
//         1: Yes, is connected
//    
int16_t NTW_IsConnected( void );

//
//  Establish connection to network endpoint.
//  If there is already a connection, do nothing.
//
//  return: <0: Failure
//           0: Success
//           1: Is Already Connected
//
int16_t NTW_Connect( char const* dialString,
                     char const* username,
                     char const* password );

//
//  Terminate connection to network endpoint.
//  If there is already no connection, do nothing.
//
//  return:  1: There was no connection
//           0: Success
//
int16_t NTW_Disconnect( void );

# endif//_NETWORK_H_
