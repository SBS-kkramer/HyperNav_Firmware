# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include "profiles_list.h"
# include "profile_acquire.h"
# include "profile_transmit.h"
# include "profile_receive.h"
# include "code_verify.h"
# include "syslog.h"

static char ProgramDescription[] = "HyperNav Profile Manager [Satlantic]";
static char ProgramRevision   [] = "Revision 0.1  2015-12-03";



/************************
 *  Function: Print usage
 */
static void print_usage ( char* pn ) {

  printf ( "%s\n%s\n", ProgramDescription, ProgramRevision );
  printf ( "Usage: %s [-h -? -gG -dD] {-l | -a | -tID [-Sk -r{g|b} -P -c{n|g}] | -r | -v }\n", pn );
  printf ( "       -h, -?  Print this usage message.\n" );
  printf ( "       -gx     Debug (x=D|N|W|E (debug,notice,warn,error)\n" );
  printf ( "       -dD     Use directory D for file I/O, default is working directory\n" );
  printf ( "       -l      List stored profiles\n" );
  printf ( "       -aN     Simulate profile acquisition, optional identifyer N (YYDDD)\n" );
  printf ( "       -tN     Transmit profile N (N=YYDDD)\n" );
  printf ( "        -Sk     Size limit on raw packets\n" );
  printf ( "        -R{g|b} Representation of spectra (g=Grayscale, b=Binary)\n" );
  printf ( "        -Pn     Transpose spectra into bitplanes, removing n noise bits [default: -P0]\n" );
  printf ( "        -Q      Do NOT transpose to pitplane\n" );
  printf ( "        -C{n|g} Compress (n=do not; g=zlib)\n" );
  printf ( "        -E{n|a} Encode (n=do not; a=ASCII85)\n" );
  printf ( "        -Bnnnn  Burst size; 0: whole packet transfer\n" );
  printf ( "       -r      Receive profile\n" );
  printf ( "       -v      Verify code\n" );

}

/******************************
 *  Function: Main
 *    Parse command line opions
 *    and start requested mode.
 *    Also handle arg errors.
 */
int main( int argc, char* argv[] ) {

  //  The program can operate in a number of mutually exclusive modes:
  enum { OPMODE_NOOP,
         OPMODE_LIST,
         OPMODE_ACQUIRE,
         OPMODE_TRANSMIT,
         OPMODE_RECEIVE,
         OPMODE_VERIFY,
  } op_mode = OPMODE_NOOP;

  //  Some operation modes require a profile ID
  uint16_t profileID = 0;

  //  Manage profiles for different sensors via directories
  char* data_directory = ".";

  //  Default packet transmit instructions,
  //  can be overwritten via command line arguments
  //  Transmit instructions are ignored in non-transmit mode.
  //
  //    max_raw_packet_size - the raw size
  //                          transmitted size may be smaller after compression
  //    representation      - graycode may help compression.
  //                          to be tested
  //    use_bitplanes       - bitplanes certainly improve compression
  //                          non-use for debugging / testing
  //    compression         - zlib is default
  //                          non-use for debugging / testing
  //    burst_size          - 0 == No bursting; else powers of 2 in the 128 to 2048 range
  transmit_instructions_t tx_instruct = { 32*1024, REP_GRAY, BITPLANE, 0, ZLIB, ASCII85, 0 };
  uint16_t port = 0;
  char*    host_ip = 0;

  char opt;

  for ( opterr=0; ( opt = getopt ( argc, argv, "?hg:d:la:t:S:R:QP:C:E:B:rH:p:v" ) ) != EOF ; ) {
    switch ( opt ) {
    // Print usage
    case '?':
    case 'h': optarg = OPMODE_NOOP; print_usage(argv[0]); break;
    //  Set debug level
    case 'g': switch ( optarg[0] ) {
              case 'd': case 'D': syslog_setVerbosity( SYSLOG_DEBUG   ); break;
              case 'i': case 'I': syslog_setVerbosity( SYSLOG_INFO    ); break;
              case 'n': case 'N': syslog_setVerbosity( SYSLOG_NOTICE  ); break;
              case 'w': case 'W': syslog_setVerbosity( SYSLOG_WARNING ); break;
              case 'e': case 'E': syslog_setVerbosity( SYSLOG_ERROR   ); break;
	      }
	      break;
    //  Set data directory
    case 'd': data_directory = strdup ( optarg ); break;
    //  Set mode to list profiles
    case 'l': op_mode = OPMODE_LIST; break;
    //  Set mode to acquire a (simulated) profile
    case 'a': op_mode = OPMODE_ACQUIRE; profileID = atol ( optarg ); break;
    //  Set mode to transmit a profile
    case 't': op_mode = OPMODE_TRANSMIT; profileID = atol ( optarg ); break;
    //  Set maximum raw packet size (size may be 
    case 'S': tx_instruct.max_raw_packet_size = atoi( optarg ); break;
    //  Set representation
    case 'R': switch ( optarg[0] ) {
              case 'g': case 'G': tx_instruct.representation = REP_GRAY; break;
              case 'b': case 'B': tx_instruct.representation = REP_BINARY; break;
              default : /* Ignore */ break;
              }
              break;
    //  UnSet bitplane and noise bit removal
    case 'Q': tx_instruct.use_bitplanes = NO_BITPLANE;
              tx_instruct.noise_bits_remove = 0;
              break;
    //  Set bitplane and noise bit removal
    case 'P': tx_instruct.use_bitplanes = BITPLANE;
              switch ( optarg[0] ) {
              case '0': tx_instruct.noise_bits_remove = 0; break;
              case '1': tx_instruct.noise_bits_remove = 1; break;
              case '2': tx_instruct.noise_bits_remove = 2; break;
              case '3': tx_instruct.noise_bits_remove = 3; break;
              case '4': tx_instruct.noise_bits_remove = 4; break;
              case '5': tx_instruct.noise_bits_remove = 5; break;
              case '6': tx_instruct.noise_bits_remove = 6; break;
              case '7': tx_instruct.noise_bits_remove = 7; break;
              default : /*  Never cut more than 7 bits */ 
                        tx_instruct.noise_bits_remove = 0; break;
              }
              break;
    //  Set burst size
    case 'B': {
              int burst_size = atoi ( optarg );
                     if ( burst_size <  128 ) { tx_instruct.burst_size = 0;
	      } else if ( burst_size <  256 ) { tx_instruct.burst_size = 128;
	      } else if ( burst_size <  512 ) { tx_instruct.burst_size = 256;
	      } else if ( burst_size < 1024 ) { tx_instruct.burst_size = 512;
	      } else if ( burst_size < 2048 ) { tx_instruct.burst_size = 1024;
	      } else if ( burst_size < 4096 ) { tx_instruct.burst_size = 2048;
	      } else if ( burst_size < 8192 ) { tx_instruct.burst_size = 4096;
	      } else                          { tx_instruct.burst_size = 8192;
              }
              }
              break;
    //  Set compression algorithm
    case 'C': switch ( optarg[0] ) {
              case 'g': case 'G': tx_instruct.compression = ZLIB; break;
              case 'n': case 'N': tx_instruct.compression = NO_COMPRESSION; break;
              default: /* Ignore */ break;
              }
              break;
    //  Set encoding schema
    case 'E': switch ( optarg[0] ) {
              case 'a': case 'A': tx_instruct.encoding = ASCII85; break;
              case 'n': case 'N': tx_instruct.encoding = NO_ENCODING; break;
              default: /* Ignore */ break;
              }
              break;
    //  Set mode to receive a profile
    case 'r': op_mode = OPMODE_RECEIVE; break;
    //  Set port number 
    case 'p': port = atoi ( optarg ); break;
    //  Set host IP address
    case 'H': host_ip = strdup( optarg ); break;
    //  Set mode to verify packaging / unpackaging code
    case 'v': op_mode = OPMODE_VERIFY; break;
    //  Unexpected option
    default : /* optarg = OPMODE_NOOP; */
              fprintf ( stderr, "%s: Ignoring unknown '-%c' option\n", argv[0], opt ); break;
    }
  }

  switch ( op_mode ) {
  case OPMODE_NOOP:     return 0; break;
  case OPMODE_LIST:     return profiles_list    ( data_directory ); break;
  case OPMODE_ACQUIRE:  return profile_acquire  ( data_directory, profileID ); break;
  case OPMODE_TRANSMIT: return profile_transmit ( data_directory, profileID, &tx_instruct, port, host_ip ); break;
  case OPMODE_RECEIVE:  return profile_receive  ( data_directory, port ); break;
  case OPMODE_VERIFY:   return code_verify      ( data_directory, profileID ); break;
  default:              fprintf ( stderr, "%s: Unknown operation mode. Exit.\n", argv[0] );
                        return 1;
  }

  free ( host_ip );

}
