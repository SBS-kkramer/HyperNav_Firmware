
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <stdio.h>
# include <errno.h>
# include <string.h>
# include <stdlib.h>

# include "syslog.h"
# include "profile_processor.h"

# if 0
static void receiveLoop( int serial_port ) {

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

  printf ( "Done receiving.\n" );

  exit ( 0 );

# if 0
    int c, res, x;
    uint8_t buf[255];



    while (1) {       /* loop for input */
      res = read(serial_port,buf,255);   /* returns after 5 chars have been input */


      for (x=0;x<res;x++)
      {
         printf("%x ", buf[x]);
      }
       printf(":%d\n",res);

    }

  if ( tcsetattr( serial_port, TCSANOW, &oldtio ) ) {
    perror("Failed to set back attr");
    return 1;
  }

  close ( serial_port );

  return 0;
# endif

}

static void sendLoop( int serial_port ) {

  char msg[80];

  printf ( "Enter msg: " ); fflush(stdout);

  while ( fgets( msg, sizeof(msg), stdin ) ) {

    if ( strlen(msg) != write ( serial_port, msg, strlen(msg) ) ) {
      perror("Failed to write");
      exit ( 1 );
    }
  
    printf ( "Enter msg: " ); fflush(stdout);
  }

  printf ( "Done sending\n" );

  exit( 0 );

}
# endif

void profile_manager() {

  while ( 1 ) {

    if ( 0 /* commanded from float to initiate telemetry */ ) {

      uint16_t profileID = 12345; // PM_getCurrentProfileID();

      //  Assemble packet Zero
    //Packet_t p0;

    //sendTo_PP( /* p0 */ );
    }

  }
}


static void usage(char* progname) {
  fprintf ( stderr, "usage: %s serial-port-device\n", progname );
}

int main( int argc, char* argv[] ) {
  static char* sFN = "main";

  if ( argc != 2 ) {
    usage(argv[0]);
    return 1;
  }

  char* serial_device = argv[1];

  syslog_setVerbosity( SYSLOG_DEBUG );
  syslog_enableOut   ( SYSLOG_FILE );
  syslog_enableOut   ( SYSLOG_STD  );

  //  Open serial port for modem communication
  //

  int16_t pp_status = profile_processor_createTask();

  if ( pp_status < 0 ) {
    syslog_out ( SYSLOG_ERROR, sFN, "Cannot start profile processor" );
  } else {
    if ( pp_status == 0 ) {
      syslog_out ( SYSLOG_DEBUG, sFN, "profile processor started" );
    } else {
      syslog_out ( SYSLOG_WARNING, sFN, "profile processor already running" );
    }
  }

  // run the main thread // OR // start as a thread, and send commands from main to profile_manager.
  profile_manager();

  //  Never get here.
  //  All threads are eternal loops
  return 1;
}

