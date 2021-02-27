# ifndef _MODEM_H_ 
# define _MODEM_H_

# include <stdint.h>
# include <time.h>
# include <sys/time.h>

# ifdef FW_SIMULATION
# else
# endif

# ifdef FW_SIMULATION
int MDM_open_serial_port ( char* serial_device, uint32_t baudrate );
# endif

void MDM_DTR_clear();
void MDM_DTR_assert();

void MDM_Enable( uint32_t BaudRate);
void MDM_Disable( void );

//  Check modem carrier detect
//  return 1: carrier present
//         0: carrier not present
int16_t MDM_cd();

//  All MDM_*Flush()
//    return n>=0 : ok, n = number of bytes that were flushed
//             -1 : fail
uint16_t MDM_IFlush();
uint16_t MDM_OFlush();
uint16_t MDM_IOFlush();

uint16_t MDM_IBytes();
uint16_t MDM_OBytes();

//  MDM_putb
//    return 1 : byte put
//           0 : failed to put
//          -1 : error
int16_t MDM_putb( char  byte );
//  MDM_putb
//    return 1 : byte gotten
//           0 : failed to get a byte
//          -1 : error
int16_t MDM_getb( char* byte );

//  read up to 'size' bytes from modem, put into buf
//  returns -1 if no carrier at modem 
//        n>=0 else number of bytes read
int16_t MDM_gets  ( char* buf, int size, time_t sec, const char* trm);

//  read up to size butes from modem
//  returns -1 if no carrier at modem 
//        n>=0 else number of bytes read
int16_t MDM_getbuf( void* buf, int size, time_t sec);

//  write string to modem
//  returns -1 if no carrier at modem 
//        n>=0 else number of bytes written
int16_t MDM_puts  ( const char* buf,           time_t sec, const char* trm );

//  write size number of bytes to modem
//  returns -1 if no carrier at modem 
//        n>=0 else number of bytes written
//          
int16_t MDM_putbuf( const void* buf, int size, time_t sec );

//  write cmd bytes and wait for expected response
//  returns -1 if no carrier at modem 
//         ==0 if exchange failed
//          >0 if exchange was successful
int16_t MDM_chat( const char* cmd, const char* expect, time_t sec, const char *trm );

int16_t MDM_expect( const char *prompt, const char *response, time_t sec, const char *trm );

# endif
