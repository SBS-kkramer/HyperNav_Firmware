# include "modem.h"

# ifdef FW_SIMULATION
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <sys/ioctl.h>
# include <termios.h>
# include <string.h>
# include <errno.h>
# include <unistd.h>
# else
# endif

# include <stdio.h>
# include <ctype.h>

# include "syslog.h"

# ifdef FW_SIMULATION
static int serial_port_fd = -1;
# endif

static char* asPrintStr( char c ) {
  static char rStr[8];
         if( c == '\r' ) {
    return "\\r";
  } else if( c == '\n' ) {
    return "\\n";
  } else if( isprint((int)c)) {
    snprintf ( rStr, 7, "%c",       c );
    return rStr;
  } else {
    snprintf ( rStr, 7, "[0x%02x]", c );
    return rStr;
  }
}

# ifdef FW_SIMULATION
static uint32_t GetBaudCode( uint32_t baud) {
  switch(baud) {
  case    9600: return B9600;
  case   19200: return B19200;
//case   38400: return B38400;
//case   57600: return B57600;
//case  115200: return B115200;
//case  230400: return B230400;
  default     : return B9600;
  }
}
# else
static unsigned char GetBaudCode( uint32_t baud) {
   
   switch(baud) {

//    case   300: return 0x44;
//    case   600: return 0x55;
//    case  1200: return 0x66;
//    case  2400: return 0x88;
//    case  4800: return 0x99;
      case  9600: return 0xbb;
      case 19200: return 0xcc;
      default:   /* set the baud to be 9600 */
                  return 0xbb;
   }
}
# endif



# ifdef FW_SIMULATION
int MDM_open_serial_port ( char* serial_device, uint32_t baudrate ) {
  static char* sFN = "MDM_open_serial_port";

  int serial_port = open( serial_device, O_RDWR );
  if( serial_port<0 ) {
    syslog_out ( SYSLOG_ERROR, sFN, "%s: Cannot open serial port %s", strerror(errno), serial_device );
    return -1;
  }

  struct termios oldtio;
  if( tcgetattr( serial_port, &oldtio ) ) {
    syslog_out ( SYSLOG_ERROR, sFN, "%s: Failed to get attributess of %s", strerror(errno), serial_device );
    close ( serial_port );
    return -1;
  }

  struct termios newtio;
  memset( &newtio, 0, sizeof(newtio) );

//newtio.c_cflag = B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;
//newtio.c_iflag = IGNPAR;
//newtio.c_oflag = 0;

  newtio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  newtio.c_oflag &= ~OPOST;
  newtio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  newtio.c_cflag &= ~(CSIZE | PARENB);
  newtio.c_cflag |= GetBaudCode(baudrate) | CRTSCTS | CS8 | CLOCAL | CREAD;

  //fcntl (serial_port, F_SETOWN, getpid());

  int n=1;
  ioctl ( serial_port, FIONBIO, &n);

//fcntl (serial_port, F_SETFL, O_NONBLOCK );

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 10 chars received */

  if ( tcflush( serial_port, TCIOFLUSH ) ) {
    syslog_out ( SYSLOG_ERROR, sFN, "%s: Failed to flush IO of %s", strerror(errno), serial_device );
    close ( serial_port );
    return -1;
  }

  if ( tcsetattr( serial_port, TCSANOW, &newtio ) ) {
    syslog_out ( SYSLOG_ERROR, sFN, "%s: Failed to get attributess of %s", strerror(errno), serial_device );
    close ( serial_port );
    return -1;
  }

  serial_port_fd = serial_port;

  return 0;
}
# endif

void MDM_DTR_clear() {
# ifdef FW_SIMULATION
# else
   //??Modem_DtrClear();
# endif
}

void MDM_DTR_assert() {
# ifdef FW_SIMULATION
# else
   Modem_DtrAssert();
# endif
}

void MDM_Enable( uint32_t BaudRate)
{
# ifdef FW_SIMULATION
  return;  //  Is folded into MDM_open_serial_port()
# else
   /* define objects to manage the RF switch hammer */
   int n; const int N=4;
   
   /* Modem initialization */
   DUART_CRA = 0xa;                 // disable tx and rx
   DUART_CRA = 0x20;                // reset receiver
   DUART_CRA = 0xa;                 // delay between misc commands
   DUART_CRA = 0x30;                // reset transmitter
   DUART_CRA = 0xa;                 // delay between misc commands
   DUART_CRA = 0x40;                // reset error status
   DUART_CRA = 0xa;                 // delay between misc commands
   DUART_CRA = 0xc0;                // disable timeout mode
   DUART_CRA = 0xa;                 // delay between misc commands
   DUART_CRA = 0xb0;                // set MR pointer to 0
   DUART_MRA = 0;                   // mode register 0
   DUART_MRA = 0x13;                // mode register 1: 8 data bits, no parity
   DUART_MRA = 7;                   // mode register 2: one stop bit
   DUART_CSRA = GetBaudCode(BaudRate);
   DUART_CRA = 0x55;                // enable TX and RX

   /* make sure the DTR line is asserted when the port is powered up */
   Modem_DtrAssert();
   
   /* power-up the modem port */
   Psd835PortASet(IRIDIUM_PON);

   /* allow time for 2.7s reset LBT EXT_ON_OFF pin and enable interrupts */
   Wait(2595); EX0=1; Wait(5);

   /* initialize the state of the RF switch signals */
   RfSwitchPin2Clear(); RfSwitchPin3Clear();

   /* loop-hammer for the RF switch into the iridium position */
   for (n=0; n<N; n++)
   {
      /* smack the RF switch into the iridium position */
      RfSwitchPin3Assert(); Wait(100); RfSwitchPin3Clear();  Wait(100);
   }
   
   /* initialize the state of the RF switch signals */
   RfSwitchPin2Clear(); RfSwitchPin3Clear();

   /* flush the modem fifo */
   flush(&ModemFifo);
# endif
}

/*------------------------------------------------------------------------*/
/* function to disable and power-down the modem serial port               */
/*------------------------------------------------------------------------*/
/**
   This function disables and powers-down the modem serial port.
*/
void MDM_Disable(void) {
# ifdef FW_SIMULATION
  return;
# else

   char status = 0;

   /* stack-check assertion */
   assert(StackOk());

   /* pet the watchdog */
   WatchDog();
   
   /* if the Tx buffer is not empty then pause to flush */
   status=DUART_SRA; if (!(status&0x08)) {Wait(100);}

   /* disable Tx/Rx and power-down the modem port */
   DUART_CRA = 0xa;  Psd835PortAClear(IRIDIUM_PON);

   DUART_CRA = 0x20; // reset receiver
   DUART_CRA = 0xa;  // delay between misc commands
   DUART_CRA = 0x30; // reset transmitter
   DUART_CRA = 0xa;  // delay between misc commands
   DUART_CRA = 0x40; // reset error status
   
   Wait(250); ModemDtrAssert();
# endif
}

int16_t MDM_cd() {
  return 1;  //  Carrier Detect Present
}

# ifdef FW_SIMULATION
# warning "MDM_*Flush() are no-op"
# else
# error "MDM_*Flush() not implemented."
# endif
uint16_t MDM_IFlush()  { return 0; }
uint16_t MDM_OFlush()  { return 0; }
uint16_t MDM_IOFlush() { return 0; }

# ifdef FW_SIMULATION
# warning "MDM_[I|O]Bytes() are no-op"
# else
# error "MDM_[I|O]Bytes() not implemented."
# endif
uint16_t MDM_IBytes() { return 0; }
uint16_t MDM_OBytes() { return 0; }

int16_t MDM_putb( char byte ) {

  if ( !MDM_cd() ) {
    return -1;
  }

# ifdef FW_SIMULATION
  return write ( serial_port_fd, &byte, 1 );
# else
# error "MDM_putb() not implemented." 
  return 0;
# endif
}

int16_t MDM_getb( char* byte ) {

  if ( !MDM_cd() ) {
    return -1;
  }

# ifdef FW_SIMULATION
  return read ( serial_port_fd, byte, 1 );
# else
# error "MDM_getb() not implemented." 
  return 0;
# endif
}

/*---------------------------------------------------------------------*/
/* function to read bytes from the serial port                         */
/*---------------------------------------------------------------------*/
/**
   This function reads (and stores in a buffer) bytes from the serial port
   until one of two termination criteria are satisfied.

      1) A specified maximum number of bytes are read.  This criteria
         prevents buffer overflow.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if insufficient data are
         available after a specified number of seconds.

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointer
   buf is non-NULL and that the maximum buffer size is strictly positive.

      input:

         size....The maximum number of bytes that will be read from the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to read bytes from the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>0.
         
      output:

         buf.....The buffer into which the bytes that are read from the
                 serial port will be stored.  This buffer must be at least
                 (size+1) bytes.

         This function returns the number of bytes in the buffer on exit from
         this function.
*/
int16_t MDM_getbuf( void* vbuf, int size, time_t sec) {

  if ( !MDM_cd() ) {
    return -1;
  }

  if( !vbuf || size<0 ) {
    return -1;
  }

  if( 0==size ) {
    return 0;
  }

  char* buf = (char*) vbuf;

  if ( sec < 0 ) sec = 0;

  time_t To=0, T=To;              /*  Record the current time */

  int n=0;                        /*  To record (&return) the number of bytes that have been read  */
  buf[0]=0;                       /*  NUL terminare buffer read so far  */
      
  do {
                                  /*  attempt to read the next byte from the serial port */
    if( MDM_getb( &buf[n] ) > 0 ) {

      n++;
      buf[n]=0;

    } else {

      T=time(NULL);               /*  get the current time */
      if ( 0 == To) {
	To=T;
      }

      if ( difftime(T,To)>sec ) { /*  Check time-related exit criteria */
        break;
      }
    }
  } while (n<size);               /*  Check termination criteria */
   
   return n;
}

/*------------------------------------------------------------------------*/
/* function to read a string from the serial port                         */
/*------------------------------------------------------------------------*/
/**
   This function reads (and stores in a buffer) bytes from the serial port
   until one of three termination criteria are satisfied.

      1) A specified termination string is read.  For example, if the
         termination string is "\r\n" then once this string is read from the
         serial port the function returns the string read up to that
         point.  The termination string itself is discarded.

      2) A specified maximum number of bytes are read.  This criteria
         prevents buffer overflow.

      3) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if insufficient data are
         available after a specified number of seconds.

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   buf and trm are non-NULL and that the maximum buffer size is strictly positive.

      input:

         size....The maximum number of bytes that will be read from the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
j                function will attempt to read bytes from the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>0.

         trm.....The termination string.  For example, if the termination
                 string is "\r\n" then once this string is read from the
                 serial port the function returns the string read up to that
                 point.  The termination string is not included in buf.
         
      output:

         buf.....The buffer into which the bytes that are read from the
                 serial port will be stored.  This buffer must be at least
                 (size+1) bytes.  Although the termination string (trm) is
                 not returned, the buffer must be large enough to contain
                 all bytes read including the termination string.

         This function returns the number of bytes read from the serial port
         including the termination string (if a termination string was
         read).
*/
int16_t MDM_gets( char* buf, int size, time_t sec, const char* trm) {

  if ( !MDM_cd() ) {
    return -1;
  }

  if( !buf || !trm || size<0 ) {
    return -1;
  }

  if( 0==size ) {
    return 0;
  }

  if ( sec < 0 ) sec = 0;

  int trmlen=strlen(trm);         /*  Length of the termination string */
  int trm_found=0;                /*  [0|1] Flag; asserted when termination string found */

  time_t To=0, T=To;              /*  Record the current time */
      
  int n=0;                        /*  To record (&return) the number of bytes that have been read  */
  buf[0]=0;                       /*  NUL terminare buffer read so far  */
      
  do {
                                  /*  attempt to read the next byte from the serial port */
    if( MDM_getb( &buf[n] ) > 0 ) {

      n++;
      buf[n]=0;
                                  /*  check for the line terminator string */
      if( n>=trmlen && !strcmp(buf+n-trmlen,trm) ) {

                                  /*  remove the termination string from the buffer */
        trm_found=1; buf[n-trmlen]=0;
      }
         
    } else {

      T=time(NULL);               /*  get the current time */
      if ( 0 == To) {
	To=T;
      }

      if ( difftime(T,To)>sec ) { /*  Check time-related exit criteria */
        break;
      }
    }
  } while (n<size && !trm_found); /* check termination criteria */
   
  return n;
}


/*------------------------------------------------*/
/* function to write a string to the modem port   */
/*------------------------------------------------*/
/**
   This function writes bytes to the serial port until one of two
   termination criteria are satisfied.

      1) The whole buffer plus termination string are written to the serial
         port.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if the serial port is not
         writable for whatever reason.

      input:

         buf.....The buffer containing the NULL terminated string to write
                 to the serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to write bytes to the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>=0.

         trm.....The string terminator to write to the serial port.
                 A NULL pointer passed to this function means that no trm
		 terminator is appended.

      output:

         This function returns the number of bytes written to the serial
         port (including the terminator string).
*/
int16_t MDM_puts(const char* buf, time_t sec, const char* trm) {

  if ( !MDM_cd() ) {
    return -1;
  }

  int const buflen = (NULL==buf) ? 0 : strlen(buf);
  int const trmlen = (NULL==trm) ? 0 : strlen(trm);
  if ( sec < 0 ) sec = 0;

  int n = -1;

  int all_ok = 1;                 /*  all_ok */

  time_t To=0, T=To;              /* record the current time */

  for( n=0; n<buflen; /**/ ) {    /* loop to transmit the buffer string */

                                  /* attempt to write the next byte
				     of the buffer to the serial port */
    if( ( all_ok=MDM_putb(buf[n])) > 0 ) {
      n++;
    } else {

      T=time(NULL);               /* get the current time */
      if ( 0 == To) {
	To=T;
      }
                                  /* check termination criteria */
      if( difftime(T,To)>sec ) {
        all_ok=0;
       	break;
      }
    }
  }

                                  /* if buffer completely transmitted, (n>=buflen)
                                   * and no timeout yet (all_ok>0)
                                   * and have non-zero terminator string */
  if (n>=buflen && all_ok>0 && trmlen>0) {

         /* loop to transmit the termination string */
    int k;
    for (k=0; k<trmlen; /**/ ) {

                                  /* attempt to write the next byte
				     of the termination to the serial port */
      if ((all_ok=MDM_putb(trm[k])) > 0 ) {
        k++;
       	n++;
      } else {

        T=time(NULL);             /* get the current time */
        if ( 0 == To) {
	  To=T;
        }
                                  /* check termination criteria */
        if( difftime(T,To)>sec ) {
       	  break;
        }
      }
    }
  }

  /* check to see if the output buffer needs to be drained and monitored */
  if( sec>0 && MDM_OBytes()>0 ) {

    /* zero bytes in the output buffer indicates success */
    do {T=time(NULL); if (!To) {To=T;}}

    /* terminate the attempt if successful or else if timeout period has expired */
    while ( MDM_OBytes()>0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec );

    /* flush any remaining bytes from the output buffer */
    if ( MDM_OBytes() > 0 ) {
      n -= MDM_OBytes();
      MDM_OFlush();
      if (n<0) n=0;}
  }

  return n;
}

/*------------------------------------------------------------------------*/
/* function to write a buffer to the serial port                          */
/*------------------------------------------------------------------------*/
/**
   This function writes bytes to the serial port until one of two
   termination criteria are satisfied.

      1) The specified maximum number of bytes are written.

      2) A specified time-out period has elapsed.  This criteria prevents
         the function from hanging indefinitely if the serial port is not
         writable for whatever reason.

   This function attempts to protect against obviously invalid function
   parameters before using them.  In particular, it checks that the pointers
   port, port.putb, and buf are initialized with non-NULL values and that
   the buffer size and time-out period are non-negative.

      input:

         buf.....The buffer containing the bytes to write to the serial port.

         size....The maximum number of bytes that will be written to the
                 serial port.

         sec.....The maximum amount of time (measured in seconds) that this
                 function will attempt to write bytes to the serial port
                 before returning to the calling function.  Due to the
                 limited (ie., 1 sec) resolution of the time() function, the
                 actuall timeout period will be somewhere in the semiclosed
                 interval [sec,sec+1) if sec>=0.
         
      output:

         This function returns the number of bytes written to the serial port.
*/
int16_t MDM_putbuf(const void* vbuf, int size, time_t sec) {

  if ( !MDM_cd() ) {
    return -1;
  }

  if ( NULL==vbuf ) size = 0;
  if ( size < 0 ) size = 0;
  if ( sec < 0 ) sec = 0;

  char* buf = (char*) vbuf;

  int n;                          /*  Return number of bytes written  */
   
  time_t To=0, T=To;              /*  Record the current time */

  for (n=0; n<size; /**/ ) {

                                  /* attempt to write the next byte
				     of the buffer to the serial port */
    if( MDM_putb(buf[n]) > 0 ) {
      n++;
    } else {

      T=time(NULL);               /* get the current time */
      if ( 0 == To) {
	To=T;
      }
                                 /* check termination criteria */
      if ( difftime(T,To)>sec ) {
        break;
      }
    }
  }
      
  /* check to see if the output buffer needs to be drained and monitored */
  if( sec>0 && MDM_OBytes()>0 ) {

    /* zero bytes in the output buffer indicates success */
    do {T=time(NULL); if (!To) {To=T;}}

    /* terminate the attempt if successful or else if timeout period has expired */
    while ( MDM_OBytes()>0 && T>=0 && To>=0 && sec>0 && difftime(T,To)<=sec );

    /* flush any remaining bytes from the output buffer */
    if ( MDM_OBytes() > 0 ) {
      n -= MDM_OBytes();
      MDM_OFlush();
      if (n<0) n=0;}
  }
   
  return n;
}

/*------------------------------------------------------------------------*/
/* function to negotiate commands                                         */
/*------------------------------------------------------------------------*/
/**
   This function transmits a command string to the serial port and verifies
   an expected response.  The command string should not include that
   termination character (\r), as this function transmits the termination
   character after the command string is transmitted.  The command string
   may include wait-characters (~) to make the processing pause as needed.
   The command string is processed each byte in turn.  Each time a
   wait-character is encountered, processing is halted for one wait-period
   (1 sec) and then processing is resumed.
   
      input:

         cmd........The command string to transmit.

         expect.....The expected response to the command string.
	            Permitted to be empty!

         sec........The number of seconds this function will attempt to
                    match the prompt-string.

         trm........A termination string transmitted immediately after the
                    command string.  For example, if the termination string
                    is "\r\n" then the command string will be transmitted
                    first and followed immediately by the termination
                    string.  If trm=NULL or trm="" then no termination
                    string is transmitted.

      output:

         This function returns
	   > 0 <=> the exchange was successful
	   ==0 <=> the exchange failed.
	   < 0 <=> the function parameters were ill-defined. 
*/
int16_t MDM_chat( const char* cmd, const char* expect, time_t sec, const char *trm ) {

  /* define the logging signature */
  static const char FuncName[] = "MDM_chat";
   
  if( !cmd || !*cmd || !expect || sec <= 0 ) {
    return -1;
  }

  int status = -1;                                     /*  initialize return value  */

  MDM_IOFlush();

  if (sec==1) sec=2;                                   /*  Work around a time descretization problem */

  const unsigned char wait_char = '~';                 /*  If finding this character in command...  */
  const time_t        wait_period = 1;                 /*  ... wait for this duration [seconds]  */

  int i;
  int const cmdlen = strlen(cmd);
  for( status=0, i=0; i<cmdlen; i++) {                 /*  Transmit command characters */

    if( cmd[i]==wait_char) {                           /*  Handle wait character (if present)  */
      sleep(wait_period);
    } else if( MDM_putb( cmd[i] ) <=0 ) {
      syslog_out( SYSLOG_ERROR, FuncName, "Failed to send %s", cmd );
      goto Err;
    }
    syslog_out ( SYSLOG_DEBUG, FuncName, "Sent %s", asPrintStr(cmd[i]) );
  }

  if (trm && trm[0]) {                                 /*  Transmit the (optional) command termination */

    int const trmlen = strlen(trm);
    for (i=0; i<trmlen; i++) {

      if( MDM_putb( cmd[i] ) <=0 ) {
        syslog_out( SYSLOG_ERROR, FuncName, "Failed to send %s", cmd );
      }
    }
  }
      
  if (*expect) {                                       /*  If non-empty expected response,
							   Seek the expect string in the response  */
    char byte; int n=0;

    time_t To=time((time_t*)0),T=To;                   /*  get the reference time */
         
    int const explen = strlen(expect);                 /*  compute the length of the prompt string */
      
    i=0;                                               /*  define the index of prompt string */

    do {

      if( MDM_getb(&byte) > 0 ) {                      /*  read the next byte from the serial port */

        syslog_out ( SYSLOG_DEBUG, FuncName, "Received %s", asPrintStr(byte) );
               
        if (byte==expect[i]) {i++;} else i=0;          /* check if the current byte matches the expected byte from the prompt */

        if (i>=explen) {status=1; break;}              /* the expect-string has been found if the index (i) matches its length */

        if (n<0 || n>25) {T=time((time_t*)0); n=0;} else n++; /* don't allow too many bytes to be read between time checks */

      } else {

        T=time((time_t*)0);                            /*  get the current time */
      }

    } while (T>=0 && To>=0 && difftime(T,To)<sec); /* check the termination conditions */

    if (status<=0) {
      syslog_out( SYSLOG_ERROR, FuncName, "Expected string [%s] not received.", expect);
    } else {
      syslog_out( SYSLOG_DEBUG, FuncName, "Expected response [%s] received.", expect);
    }
  } else {
    status=1;
  }

   Err: /* collection point for errors */

   return status;
}


/*------------------------------------------------------------------------*/
/* function to respond to expected prompts                                */
/*------------------------------------------------------------------------*/
/**
   This function reads from a serial port until it receives an expected
   prompt and then it replies with a specified response.  It was designed to
   allow software to log into a computer.  If the expected prompt is not
   received then this function times out and returns.

      input:

         prompt.....Bytes are read from the serial port until this
                    prompt-string is detected.

         response...Once the prompt-string is read, this response-string is
                    transmitted to the serial port.
		    Response string is permitted to be NULL or the empty string

         sec........The number of seconds this function will attempt to
                    match the prompt-string.

         trm........A termination string transmitted immediately after the
                    response string.  For example, if the termination string
                    is "\r\n" then the response string will be transmitted
                    first and followed immediately by the termination
                    string.  If trm=NULL or trm="" then no termination
                    string is transmitted.

      output:

         This function returns a positive number if the exchange was
         successful.  Zero is returned if the exchange failed.  A negative
         number is returned if the function parameters were determined to be
         ill-defined. 

*/
int16_t MDM_expect( const char *prompt, const char *response, time_t sec, const char *trm ) {

  /* define the logging signature */
  static const char FuncName[] = "MDM_expect";
   
  if( !prompt || !*prompt ) {
    return -1;
  }

  if ( sec <= 0 ) sec = 1;

  int status=0;                                        /* initialize the return value */

  char byte;
  int n=0;

  time_t To=time((time_t*)0),T=To;                     /*  get the reference time */

  int const promptLen = strlen(prompt);                /*  compute the length of the prompt string */

  int i=0;                                             /*  define the index of prompt string */

  do {

    if ( MDM_getb(&byte) > 0 ) {                       /*  read the next byte from the serial port */

      syslog_out ( SYSLOG_DEBUG, FuncName, "Received %s", asPrintStr(byte) );
 
      if (byte==prompt[i]) {i++;} else i=0;            /*  check if the current byte matches the expected byte from the prompt */

      if (i>=promptLen) {status=1; break;}             /*  prompt string has been found if the index (i) matches its length */

      if (n<0 || n>25) {T=time((time_t*)0); n=0;} else n++; /* don't allow too many bytes to be read between time checks */

    } else {

      T=time((time_t*)0);                                  /* get the current time */
    }
  } while (T>=0 && To>=0 && difftime(T,To)<sec);           /* check the termination conditions */
      
  if (status<=0) {
    syslog_out( SYSLOG_ERROR, FuncName, "Prompt [%s] not received.", prompt);
    return status;
  } else {
    syslog_out( SYSLOG_DEBUG, FuncName, "Prompt [%s] received.", prompt);
  }

  /* write the response string to the serial port */
  if( !response || !response[0] ) {
    
    return 1;					/*  No resonse required, thus, all done.  */
  }
 
  int const rsplen = strlen(response);
  int const trmlen = (NULL==trm) ? 0 : strlen(trm);

  int const puts_len = MDM_puts( response, sec, trm );

  status = ( puts_len == rsplen+trmlen ) ? 1 : 0;
         
  for (i=0; i<puts_len; i++) {

    char byte = ( i<rsplen)
              ? response[i]
              : trm[i-rsplen];
    syslog_out ( SYSLOG_DEBUG, FuncName, "Sent %s", asPrintStr(byte) );
  }

  return status;
}


