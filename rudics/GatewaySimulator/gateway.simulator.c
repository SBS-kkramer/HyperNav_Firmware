
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <termios.h>
# include <stdio.h>
# include <errno.h>
# include <string.h>

# include <netdb.h>
# include <sys/socket.h>


static int open_serial_port ( char* serial_device ) {

  static int serial_port_FAILED = -1;

  int serial_port = open( serial_device, O_RDWR );
  if( serial_port<0 ) {
    perror(serial_device);
    return serial_port_FAILED;
  }

  struct termios oldtio;
  if( tcgetattr( serial_port, &oldtio ) ) {
    perror("Failed in get attr");
    return serial_port_FAILED;
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
  newtio.c_cflag |= B9600 | CRTSCTS | CS8 | CLOCAL | CREAD;

  //fcntl (serial_port, F_SETOWN, getpid());

  int n=1;
  ioctl ( serial_port, FIONBIO, &n);

//fcntl (serial_port, F_SETFL, O_NONBLOCK );

  /* set input mode (non-canonical, no echo,...) */
  newtio.c_lflag = 0;

  newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused */
  newtio.c_cc[VMIN]     = 1;   /* blocking read until 10 chars received */

  if ( tcflush( serial_port, TCIOFLUSH ) ) {
    perror("Failed in flush IO");
    return serial_port_FAILED;
  }

  if ( tcsetattr( serial_port, TCSANOW, &newtio ) ) {
    perror("Failed in set attr");
    return serial_port_FAILED;
  }

  return serial_port;
}

int open_rudics_port ( const char* hostname, const char* service, const char* protocol, int servPort ) {

  int rudics_port_FAILED = -1;
  int rudics_sock = rudics_port_FAILED;

  int status = 0;

  struct hostent *host;
   
  if ( !hostname ) {

    fprintf( stderr, "Missing hostname.\n");
    return rudics_port_FAILED;

  } else if ( !(host=gethostbyname(hostname)) ) {

    fprintf( stderr, "Hostname not found: %s\n", hostname );
    return rudics_port_FAILED;

  } else {

    int flags, err;

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));

    addr.sin_family=AF_INET;
    memcpy(&addr.sin_addr,host->h_addr_list[0],host->h_length);

# if 0
    const struct servent *serv=getservbyname(service,protocol);
    if (serv) addr.sin_port=serv->s_port;
# else
    int serv = servPort;
              addr.sin_port=htons(servPort);
# endif

    if (!serv) {

      fprintf( stderr, "/etc/services entry: %s/%s not found.\n", service, protocol );
      return rudics_port_FAILED;

    } else if( -1 == (rudics_sock=socket(AF_INET,SOCK_STREAM,0)) ) {

      fprintf( stderr, "Attempt to open socket failed.\n" );
      return rudics_port_FAILED;

    } else if( 0 > ( err = connect( rudics_sock, (struct sockaddr *)&addr, sizeof(addr)) ) ) {

      fprintf( stderr, "At %ld: Socket %d connection failed: err=%d, errno=%d, %s\n", time((time_t*)0), rudics_sock, err, errno, strerror(errno) );
      return rudics_port_FAILED;

    } else if( 0 > ( flags = fcntl( rudics_sock, F_GETFL, 0 ) ) ) {

      fprintf( stderr, "Attempt to get socket %d configuration failed.\n", rudics_sock );
      return rudics_port_FAILED;

    } else {

      flags |= O_NONBLOCK;

      if( 0 > fcntl ( rudics_sock, F_SETFL, flags ) ) {

        fprintf( stderr, "Attempt to set nonblocking IO on socket %d failed.\n", rudics_sock );
        return rudics_port_FAILED;

      } else {

        status=1;
        return rudics_sock;
      }
    }
  }

  //  Can never get here,
  //  but may prevent compiler warnings about non-return 
  return rudics_port_FAILED;
}

# define CBMAX 4096
typedef struct circbuff {

  char content[CBMAX];
  int  start;
  int  count;
  
} circbuff_t;

static void circbuff_init ( circbuff_t* cb ) {
  cb->start = 0;
  cb->count = 0;
}

static void circbuff_push ( circbuff_t* cb, char byte ) {

  if ( CBMAX == cb->count ) { // Buffer is full: Overwrite oldest item.
	  //  TODO  --  Implement content as dynamically allocated memory
    cb->content[  cb->start                    ] = byte;
    cb->start = (cb->start+1) % CBMAX;
  } else {                    //  Insert at next free position
    cb->content[ (cb->start+cb->count) % CBMAX ] = byte;
    cb->count ++;
  }
}

static int circbuff_empty ( circbuff_t* cb ) {
  return 0 == cb->count;
}

static char circbuff_front ( circbuff_t* cb ) {
  if ( cb->count > 0 ) {
    return cb->content[cb->start];
  } else {
    //  ERROR: Empty buffer,
    //  but code below only calls this function if not empty
    return 0;
  }
}

static char circbuff_pop ( circbuff_t* cb ) {
  if ( cb->count > 0 ) {
    char return_char = cb->content[cb->start];
    cb->start = (cb->start+1) % CBMAX;
    cb->count--;
    return return_char;
  } else {
    //  ERROR: Empty buffer,
    //  but code below only calls this function if not empty
    return 0;
  }
}

static void multiplex_io ( const int sPort, const int rPort ) {

   char byte,buf[1024];

  const int mxPort = ( sPort>rPort ) ? sPort : rPort;

  // enable nonblocking mode for both ports
  int n = 1;
  ioctl( sPort, FIONBIO, &n);
  ioctl( rPort, FIONBIO, &n);

  //  define and initialize two circular buffers for IO
  circbuff_t s2r;  circbuff_init( &s2r );
  circbuff_t r2s;  circbuff_init( &r2s );

  fprintf ( stderr, "Now multiplexing IO\n" );
   
  while (1) {

    // initialize the controlling file-descriptor bits
    fd_set rbits; FD_ZERO(&rbits);
    fd_set wbits; FD_ZERO(&wbits);

    // these conditionals keep the FIFOs from becoming larger than 1024 bytes
    if (!circbuff_empty( &s2r )) FD_SET(rPort,&wbits); else FD_SET(sPort,&rbits);
    if (!circbuff_empty( &r2s )) FD_SET(sPort,&wbits); else FD_SET(rPort,&rbits);

    // multiplexed IO
    switch( select(mxPort+1, &rbits, &wbits, NULL, NULL) ) {

      case -1: {   // exception condition
        // ignore the error if select() was interrupted
        if (errno==EINTR) continue;

        // log the execption and crash out
        fprintf( stderr, "Exception [%d] : %s\n", errno, strerror(errno));
        return;
      }

      // this shouldn't happen because select() uses a blocking model
      case  0: {
        sleep(5); continue;
      }

      default: {

        if ( FD_ISSET( sPort, &rbits ) ) {             //  Check if input is available at sPort

          int rs = read( sPort, buf, sizeof(buf) );    //  Read input into buffer
          if ( rs< 0 && errno==EWOULDBLOCK ) continue; //  No input was available
          if ( rs<=0                       ) break;    //  Check for exceptions
	  int i;
          for ( i=0; i<rs; i++) {                      //  Add buf to FIFO
            circbuff_push ( &s2r, buf[i]);
	  }
          FD_SET( rPort, &wbits);                      //  Indicate that bytes should be written to rPort
        }

        if ( FD_ISSET( rPort,&wbits) ) {          //  Check if output is ready for rPort

          while (! circbuff_empty ( &s2r ) ) {    //  Attempt to write the whole FIFO

            byte = circbuff_front ( &s2r );       //  Get next byte from FIFO

            if (write( rPort, &byte, 1) > 0 ) {   // write byte to rPort
              circbuff_pop ( &s2r );
	    } else {
              break;
            }
          }
        }

        if ( FD_ISSET( rPort, &rbits ) ) {             //  Check if input is available at rPort

          int rr = read( rPort, buf, sizeof(buf) );    //  Read input into buffer
          if ( rr< 0 && errno==EWOULDBLOCK ) continue; //  No input was available
          if ( rr<=0                       ) break;    //  Check for exceptions
	  int i;
          for ( i=0; i<rr; i++) {                      //  Add buf to FIFO
            circbuff_push ( &r2s, buf[i]);
	  }
          FD_SET( sPort, &wbits);                      //  Indicate that bytes should be written to rPort
        }

        if ( FD_ISSET( sPort,&wbits) ) {          //  Check if output is ready for sPort

          while (! circbuff_empty ( &r2s ) ) {    //  Attempt to write the whole FIFO

            byte = circbuff_front ( &r2s );       //  Get next byte from FIFO

            if (write( sPort, &byte, 1) > 0 ) {   // write byte to sPort
              circbuff_pop ( &r2s );
	    } else {
              break;
            }
          }
        }
      }
    }
  }

  //  Never to return
}


static void usage(char* progname) {
  fprintf ( stderr, "usage: %s serial-port-device host-name\n", progname );
}

int main( int argc, char* argv[] ) {

  if ( argc != 3 ) {
    usage(argv[0]);
    return 1;
  }

  int serial_port = open_serial_port ( argv[1] );

  if ( serial_port < 0 ) {
    fprintf ( stderr, "Cannot open serial port %s. EXIT.\n", argv[1] );
    return 1;
  }

  int n = 1;
  ioctl( serial_port, FIONBIO, &n);

  char input;
  while ( 0 >= read ( serial_port, &input, 1 ) ) {
    fprintf ( stderr, "Waiting for ring...\n" );
    sleep ( 1 );
  }

  const char* service  = "rudics";
  const char* protocol = "tcp";
  const int   port     = 37999;

  int rudics_port = open_rudics_port ( argv[2], service, protocol, port );

  if ( rudics_port < 0 ) {
    fprintf ( stderr, "Cannot open %s/%s at port %d at %s. EXIT.\n", service, protocol, port, argv[2] );
    return 1;
  }

  multiplex_io ( serial_port, rudics_port );

# if 0
  int try = 20;

  while (try>0) {  //  Forever read input

    int nRead;
    char buf[256];

    if ( 0 < ( nRead = read ( serial_port, buf, 255 ) ) ) {

      int r;
      for ( r=0; r<nRead; r++ ) {
        printf ( "RX: %02x %c\n", (int)buf[r], buf[r] );
      }

      try = 20;

    } else {

        sleep (3);
        try--;
        printf ( "--: %2d\n", try );
    }

  }

  if ( tcsetattr( serial_port, TCSANOW, &oldtio ) ) {
    perror("Failed to set back attr");
    return 1;
  }

  close ( serial_port );

# endif

# if 0
   // shutdown and clean-up
   cleanup();

   // kill the rudics server
   exit(0);
# endif

  return 0;
}
