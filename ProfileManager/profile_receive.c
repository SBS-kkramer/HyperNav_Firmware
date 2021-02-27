# include "profile_receive.h"

# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/ioctl.h>
# include <unistd.h>

# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>

# include "zlib.h"

# include "profile_description.h"
# include "profile_packet.h"
# include "syslog.h"

# define MXPCKT 128
//  Assume 32 kB packet size and 128 byte bursts --> 32*8 = 256
# define MXBRST 256

//  Typedefined such that
//  memset to zero will give proper initialization
typedef struct assembling_profile {

  //  Make sure packets added to this data structure
  //  belong to the same profile
  //  -- Long shot, but would become relevant if more than
  //     one float were to send to a single server
  //

  //  The Profile Packet Definition is based on the content of packet#0,
  //  which is of type Profile_Info_Packet_t
  //
  Profile_Packet_Definition_t profile_def;

  //  Keep track if packets that have been received
  //
  Profile_Info_Packet_t   pip;                  //  packet 0
  char                    pip_have;

  Profile_Data_Packet_t   dp      [ MXPCKT ];  //  packets >= 1
  char                    dp_have [ MXPCKT ];
  uint16_t                dp_need;
  uint16_t                dp_rxed;

  //  Only use the items below if burst receiving
  //
  char*             b      [ MXPCKT ][ MXBRST ];
  uint16_t          b_size [ MXPCKT ][ MXBRST ];
  char              b_have [ MXPCKT ][ MXBRST ];
  uint16_t          b_need [ MXPCKT ];
  uint16_t          b_rxed [ MXPCKT ];

} Assembling_Profile_t;

static Assembling_Profile_t rx_profile;
static const char const* Terminator = "\r\n";


static void initialize_profile ( Assembling_Profile_t* ap ) {

  ap -> pip_have = 0;

  ap ->  dp_need = 0;
  ap ->  dp_rxed = 0;
  uint16_t p;
  for ( p=0; p<MXPCKT; p++ ) {
    ap -> dp_have [p] = 0;

    ap -> b_need [p] = 0;  //  assigned when burst 0 is received
    ap -> b_rxed [p] = 0;

    uint16_t b;
    for ( b=0; b<MXBRST; b++ ) {
      ap -> b_have [p][b] = 0;
      ap -> b_size [p][b] = 0;
      ap -> b      [p][b] = 0;
    }

  }
}

static void packet_from_bursts( uint16_t hynv_number, uint16_t profile_ID, uint16_t packet_number, int io_fd, const char* data_dir ) {
  const char* const function_name = "packet_from_bursts";

  uint16_t p_size = 0;
  int b;
  for ( b=1; b<=rx_profile.b_need[packet_number]; b++ ) {
    p_size += rx_profile.b_size[packet_number][b];
  }

  //  Copy all burst data into single packet data,
  //  and clean-up burst data
  //
  char* p_data = malloc ( (size_t)p_size );
  if ( !p_size ) {
    syslog_out ( SYSLOG_ERROR, function_name, "Out-of-memory %4hu", packet_number );
    //  FIXME -- How to handle internal error!
  } else {
    //  Copy burst[1..N] -> p_data
    //  Reset burst information back to empty.
    //  Then, if the content of this packet turns out to be invalid,
    //  the empty burst items will all be re-received.
    uint16_t location = 0;
    for ( b=1; b<=rx_profile.b_need[packet_number]; b++ ) {
      memcpy ( p_data+location,
               rx_profile.b[packet_number][b],
               rx_profile.b_size[packet_number][b] );
      location += rx_profile.b_size[packet_number][b];
      free ( rx_profile.b[packet_number][b] );
      rx_profile.b     [packet_number][b] = 0;
      rx_profile.b_size[packet_number][b] = 0;
      rx_profile.b_have[packet_number][b] = 0;
    }

    rx_profile.b_need[packet_number] = 0;
    rx_profile.b_rxed[packet_number] = 0;

    if ( 0 == packet_number ) {

      if ( info_packet_fromBytes ( &(rx_profile.pip), p_data, p_size ) ) {

        //  Error returned by info_packet_fromBytes():
        //  Must resend all of packet packet_number(=0),
        //  because we cannot identify the place of error.
        char rsnd[32];
        snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                   hynv_number, profile_ID, packet_number, 999 );
        syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu all", packet_number );

        uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, rsnd, strlen(rsnd) );
        char crcStr[9];
        snprintf ( crcStr, 9, "%08X", crc );
        write ( io_fd, rsnd, strlen(rsnd) );
        write ( io_fd, crcStr, strlen(crcStr) );
        write ( io_fd, Terminator, strlen(Terminator) );

      } else {

        rx_profile.pip.HYNV_num = hynv_number;
        rx_profile.pip.PROF_num = profile_ID;
        rx_profile.pip.PCKT_num = packet_number;

        if ( rx_profile.pip_have ) {
          syslog_out ( SYSLOG_ERROR, function_name, "Re-received %4hu", packet_number );
          //  FIXME -- Keep ignoring???
        } else {
          rx_profile.pip_have = 1;
        }

        //  Now read the number of packets stored in the pip,
        //  so this receiver knows how many packets to expect in total
        //
        if ( profile_packet_definition_from_info_packet ( &(rx_profile.profile_def), &(rx_profile.pip) ) ) {
          //  ERROR FIXME Should not possibly happen
          syslog_out ( SYSLOG_ERROR, function_name, "Packet %4hu parse error", packet_number );
        } else {
          rx_profile.dp_need = rx_profile.profile_def.numPackets_SBRD
                             + rx_profile.profile_def.numPackets_PORT
                             + rx_profile.profile_def.numPackets_OCR
                             + rx_profile.profile_def.numPackets_MCOMS;
        }

        //  ACK this packet
        char rxed[32];
        snprintf ( rxed, 32, "RXED,%04hu,%05hu,%04hu,%03hu,",
                   hynv_number, profile_ID, packet_number, 999 );

        uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, rxed, strlen(rxed) );
        char crcStr[9];
        snprintf ( crcStr, 9, "%08X", crc );
        write ( io_fd, rxed, strlen(rxed) );
        write ( io_fd, crcStr, strlen(crcStr) );
        write ( io_fd, Terminator, strlen(Terminator) );
      }

    } else { // packet_number > 0

      Profile_Data_Packet_t working_packet;

      if ( data_packet_fromBytes ( &working_packet, p_data, p_size ) ) {

        //  Error returned by data_packet_fromBytes():
        //  Must resend all of packet packet_number(>0),
        //  because we cannot identify the place of error.
        char rsnd[32];
        snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                   hynv_number, profile_ID, packet_number, 999 );
        syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu all", packet_number );

        uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, rsnd, strlen(rsnd) );
        char crcStr[9];
        snprintf ( crcStr, 9, "%08X", crc );
        write ( io_fd, rsnd, strlen(rsnd) );
        write ( io_fd, crcStr, strlen(crcStr) );
        write ( io_fd, Terminator, strlen(Terminator) );

      } else {

        working_packet.HYNV_num = hynv_number;
        working_packet.PROF_num = profile_ID;
        working_packet.PCKT_num = packet_number;

        data_packet_save ( &working_packet, data_dir, "r" );

        syslog_out ( SYSLOG_DEBUG, function_name,
                                   "received packet SZ %hu 32+%6.6s", p_size, working_packet.compressed_sz );

        if ( working_packet.ASCII_encoding != 'N' ) {

          Profile_Data_Packet_t decoded;
          int ec;
          if ( ec = data_packet_decode ( &working_packet, &decoded ) ) {
            syslog_out ( SYSLOG_ERROR, function_name, "data_packet_decode() [%d] failed %4hu", ec, packet_number );
          } else {
            memcpy ( &working_packet, &decoded, sizeof( Profile_Data_Packet_t ) );
          }
        }

        data_packet_save ( &working_packet, data_dir, "d" );

        if ( working_packet.compression == 'G' ) {

          Profile_Data_Packet_t uncompressed;
          if ( data_packet_uncompress ( &working_packet, &uncompressed ) ) {
            syslog_out ( SYSLOG_ERROR, function_name, "data_packet_uncompress() failed %4hu", packet_number );
          } else {
            memcpy ( &working_packet, &uncompressed, sizeof( Profile_Data_Packet_t ) );
          }
        }

        data_packet_save ( &working_packet, data_dir, "u" );

        if ( working_packet.compression        == '0'
          && working_packet.noise_bits_removed != 'N' ) {

          Profile_Data_Packet_t debitplaned;
          if ( data_packet_debitplane ( &working_packet, &debitplaned ) ) {
            syslog_out ( SYSLOG_ERROR, function_name, "data_packet_debitplane() failed %4hu", packet_number );
          } else {
            memcpy ( &working_packet, &debitplaned, sizeof( Profile_Data_Packet_t ) );
          }
        }

        data_packet_save ( &working_packet, data_dir, "x" );

        if ( working_packet.compression        == '0'
          && working_packet.noise_bits_removed == 'N'
          && working_packet.representation     == 'G' ) {

          Profile_Data_Packet_t binarycode;
          if ( data_packet_gray2bin ( &working_packet, &binarycode ) ) {
            syslog_out ( SYSLOG_ERROR, function_name, "data_packet_gray2bin() failed %4hu", packet_number );
          } else {
            memcpy ( &working_packet, &binarycode, sizeof( Profile_Data_Packet_t ) );
          }
        }

        data_packet_save ( &working_packet, data_dir, "z" );

        memcpy ( &(rx_profile.dp[packet_number]), &working_packet, sizeof( Profile_Data_Packet_t ) );

        if ( rx_profile.dp_have[packet_number] ) {
          syslog_out ( SYSLOG_ERROR, function_name, "Re-received %4hu", packet_number );
        } else {
          rx_profile.dp_have[packet_number] = 1;
          rx_profile.dp_rxed                 ++;
        }

        //  ACK this packet
        char rxed[32];
        snprintf ( rxed, 32, "RXED,%04hu,%05hu,%04hu,%03hu,",
                   hynv_number, profile_ID, packet_number, 999 );

        uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, rxed, strlen(rxed) );
        char crcStr[9];
        snprintf ( crcStr, 9, "%08X", crc );
        write ( io_fd, rxed, strlen(rxed) );
        write ( io_fd, crcStr, strlen(crcStr) );
        write ( io_fd, Terminator, strlen(Terminator) );

        //  TODO -- as frames to file - either here on a packet-by-packet base,
        //  or at the end???
        //
        //  In any case, will need another flag
        //  rx_profile.p_done[packet_number]  to indicate that the packet has been output
        //

      }

    }

    free ( p_data );
  }
}

int profile_receive ( const char* data_dir, uint16_t port ) {
  const char* const function_name = "profile_receive";

  syslog_out ( SYSLOG_NOTICE, function_name, "Start" );

  int socket_fd = -1;
  int io_fd = -1;   //  i/o file descriptor
                    //  If 0==port - cannot use for output!
                    //  Else will comm with (so far any) client

  if ( 0 == port ) {
    io_fd = 0;
  } else {

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( socket_fd < 0 ) {
      syslog_out ( SYSLOG_ERROR, function_name, "%s", strerror(errno) );
      return 1;
    }

# if RESTRICT_CONNECTIONS
    struct hostent* server = gethostbyname( float_host_ip );
    if ( NULL == server ) {
      syslog_out ( SYSLOG_ERROR, function_name, "no '%s' host", host_ip );
      return 1;
    }
# endif

    struct sockaddr_in server_address;
    memset ( &server_address, 0, sizeof(server_address) );

    server_address.sin_family = AF_INET;
 // memcpy ( &server_address.sin_addr.s_addr, server->h_addr, server->h_length);
# if RESTRICT_CONNECTIONS
    bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
# else
    server_address.sin_addr.s_addr = INADDR_ANY;
# endif
    server_address.sin_port = htons(port);

    if (bind( socket_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
      syslog_out ( SYSLOG_ERROR, function_name, "%s: Error on binding", strerror(errno) );
      return 1;
    }

    if ( listen( socket_fd, 5 ) < 0 ) {
      syslog_out ( SYSLOG_ERROR, function_name, "%s: Error on listening", strerror(errno) );
      return 1;
    }

    struct sockaddr_in client_address;
    socklen_t clilen = sizeof(client_address);
    io_fd = accept( socket_fd, (struct sockaddr *) &client_address, &clilen);
    if ( io_fd < 0) {
      syslog_out ( SYSLOG_ERROR, function_name, "%s: Error on accept", strerror(errno) );
      return 1;
    }
  }

  syslog_out ( SYSLOG_NOTICE, function_name, "Connected" );

  //  Initialize profile
  initialize_profile( &rx_profile );

  //  Receive all input into a single bit array
  //  MAYBE TODO: Make this a circular buffer
  size_t start_input = 0;
  size_t end_input = 0;
  size_t size_input = 64L*1024L*1024L;
  unsigned char* all_input = malloc ( size_input );

  //  Keep incoming sync information
  //
  char* brst_sync = "BRST";
# if 0
  char* pack_sync = "HYNV";
# endif

  unsigned char* sync32 = 0;

  int waitmax = 120;
  time_t now = time((time_t*)0);
  time_t timeout = now + waitmax;
  time_t lastrxed  = now-10;
  time_t heartbeat = now-10;
  time_t statusout = now-10;

  // Non-blocking I/O
  int const on = 1;
  ioctl(io_fd,FIONBIO,&on);

  do {

    ssize_t n;
    char buf[1024];

    //  Read up to 1024 bytes from current input into buffer
    //
    if ( 0 < ( n = read ( io_fd, buf, 1024 ) ) ) {

    # ifdef TXRXERRORRATE
      if ( TXRXERRORRATE>0 ) {
        if ( TXRXERRORRATE*n > drand48() ) {
          buf[rand()%n] ^= 0x66;
        }
      }
    # endif

      memcpy ( all_input+end_input, buf, n );
      end_input += n;

      lastrxed = time((time_t*)0);
      timeout = lastrxed + waitmax;

      if ( lastrxed > heartbeat+5 ) {
        const char* const alif = "ALIF";
        write ( io_fd, alif, strlen(alif) );
        write ( io_fd, Terminator, strlen(Terminator) );
        heartbeat = lastrxed;
      }
    }

    //  Look for complete 32 byte header
    //    Search currently available input
    //    for start of a BURST.
    //    Skip over all non-sync data,
    //    but only assign sync32 if at least 32 characters are available.
    //
    //    At this point, the frame sync may already be available,
    //    but the data of BURST may not be complete yet.
    //    In that case, skip the while() loop.
    //
    while ( !sync32 && end_input-start_input>=32 )
    {

      if ( memcmp ( all_input+start_input, brst_sync, 4 ) )
      {
        start_input++;
      }
      else
      {
        sync32 = all_input+start_input;
      }
    }

# if 0
    if ( !sync32 && end_input-start_input<32 ) {
          syslog_out ( SYSLOG_TRACE, function_name, 
                       "Have [%d] '%*.*s' waiting for more...",
                          end_input-start_input, 
                          end_input-start_input, 
                          end_input-start_input, 
                          all_input+start_input );
    }
# endif

    //  If 32 char header was received, parse the 32 bytes,
    //  and maybe wait for more input

    if  ( sync32 )
    {

      uint16_t hynv_number = 0,
               profile_ID = 0,
               packet_number = 0,
               burst_number = 0,
               burst_size = 0;
      uLong    crc = 0;

      if ( 0 == memcmp ( sync32+17, "ZZZZZZZ", 7 ) )
      {

        if ( 4 != sscanf ( sync32, "BRST%4hu%5hu%4huZZZZZZZ%8X",
                            &hynv_number, &profile_ID, &packet_number, &crc ) )
        {

          syslog_out ( SYSLOG_NOTICE, function_name, "EOP burst header misformatted" );
          sync32 = 0; start_input += 4;

        }
        else
        {

          //  Assemble packet from bursts OR identify missing bursts
          //

          //  First make sure data were not corrupted in transfer
          uLong calcCRC = crc32 ( 0L, Z_NULL, 0 );
          calcCRC = crc32 ( calcCRC, sync32, 24 );

          if ( calcCRC != crc )
          {
            syslog_out ( SYSLOG_NOTICE, function_name, "burst %hu end, CRC Error", packet_number );
            sync32 = 0; start_input+=4;
          }
          else
          {

            //  Unexpected, but not impossible: Burst zero might have been lost
            if ( 0 == rx_profile.profile_def.profiler_sn && 0 == rx_profile.profile_def.profile_yyddd )
            {
              rx_profile.profile_def.profiler_sn = hynv_number;
              rx_profile.profile_def.profile_yyddd = profile_ID;
            }

            if ( hynv_number != rx_profile.profile_def.profiler_sn
              || profile_ID != rx_profile.profile_def.profile_yyddd )
            {
              syslog_out ( SYSLOG_ERROR, function_name, "Profile mismatch %04hu %04hu P %05hu %05hu",
                                  rx_profile.profile_def.profiler_sn, hynv_number,
                                  rx_profile.profile_def.profile_yyddd, profile_ID );
              //  Discard bursts intended for different profiler/profile
              sync32 = 0; start_input+=32;

            }
            else
            {

              syslog_out ( SYSLOG_DEBUG, function_name, "Have %hu %hu %4hu end brst %8x",
                                hynv_number, profile_ID, packet_number, crc );

              //  Check if all bursts in this packet were received
              //
              if ( rx_profile.b_need[packet_number]
                && rx_profile.b_need[packet_number] == rx_profile.b_rxed[packet_number] )
              {

                //  All received, thus re-assemble this packet from individual bursts
                //

                packet_from_bursts( hynv_number, profile_ID, packet_number, io_fd, data_dir );

                if ( packet_number > 0 )
                {

                  //
                  //  Check if previous packets are not assembled (0==dp_have)
                  //  In that case, check if all bursts are received.
                  //  In that case, assemble the packet.
                  //
                  //  If not all bursts were received,
                  //  there may already be a resend request on its way.
                  //  !!!  NEVER issue double resend requests  !!!
                  //  !!!  It wastes valuable modem-on time and float energy.  !!!
                  //  However, there may be the case where a resend request
                  //  was issued and serviced, but neither that burst not the
                  //  following EOP burst were received.
                  //  In that case, the resend must be re-issued.
                  //
                  //  FIXME  Resolve that condition
                  //  FIXME  Maybe via timeout checking
                  //  
                  uint16_t p;
                  for ( p=0; p<packet_number; p++ )
                  {
                    if ( !rx_profile.dp_have[p] )
                    {
                      if ( rx_profile.b_need[p]
                        && rx_profile.b_need[p] == rx_profile.b_rxed[p] )
                      {
                        packet_from_bursts( hynv_number, profile_ID, p, io_fd, data_dir );
                      }
                    }
                  }

                }

              }
              
              else if ( 0 == rx_profile.b_need[packet_number] )
              {

                //  This means that burst zero in this packet was not received.
                //  Request resend of burst [packet_number][0]
                //
                char rsnd[32];
                snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                           hynv_number, profile_ID, packet_number, 0 );
                syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu %3hu", packet_number, 0 );

                uLong crc = crc32 ( 0L, Z_NULL, 0 );
                      crc = crc32 ( crc, rsnd, strlen(rsnd) );
                char crcStr[9];
                snprintf ( crcStr, 9, "%08X", crc );
                write ( io_fd, rsnd, strlen(rsnd) );
                write ( io_fd, crcStr, strlen(crcStr) );
                write ( io_fd, Terminator, strlen(Terminator) );

              }

              else
              {
                //  b_need > 0 && b_need != b_rxed
                //  This occurs when one or more bursts in the frame were not received
                //  Request resending those bursts
                //
                int b;
                for ( b = 1;  b <= rx_profile.b_need[packet_number];  b++ )
                {
                  if ( !rx_profile.b_have[packet_number][b] )
                  {
                    //  Request resend of burst [packet_number][b]
                    //
                    char rsnd[32];
                    snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                               hynv_number, profile_ID, packet_number, b );
                    syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu %3hu", packet_number, b );

                    uLong crc = crc32 ( 0L, Z_NULL, 0 );
                          crc = crc32 ( crc, rsnd, strlen(rsnd) );
                    char crcStr[9];
                    snprintf ( crcStr, 9, "%08X", crc );
                    write ( io_fd, rsnd, strlen(rsnd) );
                    write ( io_fd, crcStr, strlen(crcStr) );
                    write ( io_fd, Terminator, strlen(Terminator) );

                  }
                }
              }

              sync32 = 0; start_input+=32;
            }
          }
        }
      }

      else
      {
        /// Not final burst in packet (ZZZZZZZ burst)

        if ( 6 != sscanf ( sync32, "BRST%4hu%5hu%4hu%3hu%4hu%8X",
                            &hynv_number, &profile_ID, &packet_number,
                            &burst_number, &burst_size, &crc ) )
        {
          syslog_out ( SYSLOG_NOTICE, function_name, "burst header misformatted" );
          sync32 = 0; start_input += 4;
        }
        else
        {
          if ( 0 == burst_number )
          {
            //  Burst #0 has 32 bytes, all ready to use

            //  In burst #0, reinterpret the 5th value
            uint16_t num_of_bursts = burst_size;

            uLong calcCRC = crc32 ( 0L, Z_NULL, 0 );
            calcCRC = crc32 ( calcCRC, sync32, 24 );

            if ( calcCRC != crc )
            {
              syslog_out ( SYSLOG_NOTICE, function_name, "burst %hu 0, CRC Error", packet_number );
              sync32 = 0; start_input+=4;
            }
            else
            {
              if ( 0 == rx_profile.profile_def.profiler_sn && 0 == rx_profile.profile_def.profile_yyddd )
              {
                rx_profile.profile_def.profiler_sn = hynv_number;
                rx_profile.profile_def.profile_yyddd = profile_ID;
              }

              if ( hynv_number != rx_profile.profile_def.profiler_sn
                || profile_ID != rx_profile.profile_def.profile_yyddd )
              {

                syslog_out ( SYSLOG_WARNING, function_name, "Profile mismatch %04hu %04hu P %05hu %05hu",
                                    rx_profile.profile_def.profiler_sn, hynv_number,
                                    rx_profile.profile_def.profile_yyddd, profile_ID );
                //  Discard
                sync32 = 0; start_input+=32;
              }
              else
              {

                if ( rx_profile.b_have[ packet_number ][ burst_number ] )
                {
                  syslog_out ( SYSLOG_ERROR, function_name, "Re-received %4hu %4hu", packet_number, burst_number );
                }
                else
                {
                  rx_profile.b_have[ packet_number ][ burst_number ] = 1;
                  rx_profile.b_size[ packet_number ][ burst_number ] = 0;
                  rx_profile.b_need[ packet_number ]                 = num_of_bursts;
                  // do not set rx_profile.b_rxed[ packet_number ] = 0 , since previous burst for this packet may have been received

                  syslog_out ( SYSLOG_DEBUG, function_name, "Have %hu %hu %4hu %3hu %4hu %8x",
                                hynv_number, profile_ID, packet_number,
                                burst_number, burst_size, crc );

                }
                sync32 = 0; start_input+=32;
              }
            }
          }
          else
          { 
            //  burst_number > 0

            //  Need complete burst for CRC calculation
            //
            //  In order to calculate the CRC,
            //  need burst_size bytes after the already present 32 bytes

            if  ( end_input - start_input >= 32 + burst_size )
            {
              uLong calcCRC = crc32 ( 0L, Z_NULL, 0 );
              calcCRC = crc32 ( calcCRC, sync32, 24 );
              calcCRC = crc32 ( calcCRC, sync32+32, burst_size );

              if ( calcCRC != crc )
              {
                syslog_out ( SYSLOG_NOTICE, function_name, "burst %hu %hu, CRC Error", packet_number, burst_number );
                sync32 = 0; start_input+=4;
              }

              else
              {
                if ( 0 == rx_profile.profile_def.profiler_sn && 0 == rx_profile.profile_def.profile_yyddd )
                {
                  //  Unexpected, but not impossible: Burst zero might have been lost
                  rx_profile.profile_def.profiler_sn = hynv_number;
                  rx_profile.profile_def.profile_yyddd = profile_ID;
                }

                if ( hynv_number != rx_profile.profile_def.profiler_sn
                  || profile_ID != rx_profile.profile_def.profile_yyddd )
                {
                  syslog_out ( SYSLOG_WARNING, function_name, "Profile mismatch %04hu %04hu P %05hu %05hu",
                                    rx_profile.profile_def.profiler_sn, hynv_number,
                                    rx_profile.profile_def.profile_yyddd, profile_ID );
                  //  Discard
                  sync32 = 0; start_input+=32;
                }
                else
                {

                  if ( rx_profile.b_have[ packet_number ][ burst_number ] )
                  {
                    syslog_out ( SYSLOG_ERROR, function_name, "Re-received %4hu %4hu", packet_number, burst_number );
                  }
                  else
                  {
                    char* bcp = malloc ( (size_t)burst_size );
                    if ( !bcp )
                    {
                      syslog_out ( SYSLOG_ERROR, function_name, "Out-of-memory %4hu %4hu", packet_number, burst_number );
                    }
                    
                    else
                    {
                      memcpy ( bcp, sync32+32, burst_size );
                    
                      rx_profile.b     [ packet_number ][ burst_number ] = bcp;
                      rx_profile.b_size[ packet_number ][ burst_number ] = burst_size;
                      rx_profile.b_have[ packet_number ][ burst_number ] = 1;
                      rx_profile.b_rxed[ packet_number ]                  ++;

                      syslog_out ( SYSLOG_DEBUG,
                                  function_name,
                                  "Have %hu %hu %4hu %3hu %4hu %8x (%u)",
                                  hynv_number, profile_ID, packet_number,
                                  burst_number, burst_size, crc, end_input-start_input-32
                                 );
                    }
                  }

                  sync32 = 0; start_input+=(32+burst_size);
                }
              }

            }
            else
            {
               //  wait for more input
            }
          }
        }
      }
    }

    now = time((time_t*)0);

    //  If we did not receive anything for the last 60 seconds,
    //  assume the sender believes it is done.
    //
    //  If we are not done, take action:

    if ( now > lastrxed + 15 ) {

      if ( !rx_profile.dp_need && rx_profile.dp_rxed > 0 ) {
        //  We did not receive packet zero, but other packet(s).
        //  Issue resend packet zero request
        //
        char rsnd[32];
        snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                              rx_profile.profile_def.profiler_sn,
                              rx_profile.profile_def.profile_yyddd, 0, 999 );
        syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu all", 0 );

        uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, rsnd, strlen(rsnd) );
        char crcStr[9];
        snprintf ( crcStr, 9, "%08X", crc );
        write ( io_fd, rsnd, strlen(rsnd) );
        write ( io_fd, crcStr, strlen(crcStr) );
        write ( io_fd, Terminator, strlen(Terminator) );
      } else if ( rx_profile.dp_need
               && rx_profile.dp_rxed < rx_profile.dp_need ) {
        //  We received packet zero, but not all other packets.
        //  (a) assemble those packets where all burst were received.
        //  (b) issue re-send requests where bursts are missing
        uint16_t p;
        for ( p=1; p<=rx_profile.dp_need; p++ ) {
          if ( !rx_profile.dp_have[p] ) {
            if ( rx_profile.b_need[p]
              && rx_profile.b_need[p] == rx_profile.b_rxed[p] ) {
              packet_from_bursts( rx_profile.profile_def.profiler_sn,
                                              rx_profile.profile_def.profile_yyddd, p, io_fd, data_dir );
            } else if ( 0 == rx_profile.b_need[p] ) {
              //  Request resend of burst [p][0]
              char rsnd[32];
              snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                              rx_profile.profile_def.profiler_sn,
                              rx_profile.profile_def.profile_yyddd, p, 0 );
              syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu %3hu", p, 0 );

              uLong crc = crc32 ( 0L, Z_NULL, 0 );
                    crc = crc32 ( crc, rsnd, strlen(rsnd) );
              char crcStr[9];
              snprintf ( crcStr, 9, "%08X", crc );
              write ( io_fd, rsnd, strlen(rsnd) );
              write ( io_fd, crcStr, strlen(crcStr) );
              write ( io_fd, Terminator, strlen(Terminator) );

            } else {  //  b_need > 0 && b_need != b_rxed

              //  This occurs when one or more bursts in the frame were not received
              //  Request resending those bursts
              //
              int b;
              for ( b=1; b<=rx_profile.b_need[p]; b++ ) {
                if ( !rx_profile.b_have[p][b] ) {
                  //  Request resend of burst [p][b]
                  char rsnd[32];
                  snprintf ( rsnd, 32, "RSND,%04hu,%05hu,%04hu,%03hu,",
                              rx_profile.profile_def.profiler_sn,
                              rx_profile.profile_def.profile_yyddd, p, b );
                  syslog_out ( SYSLOG_NOTICE, function_name, "Request Resend %4hu %3hu", p, b );

                  uLong crc = crc32 ( 0L, Z_NULL, 0 );
                        crc = crc32 ( crc, rsnd, strlen(rsnd) );
                  char crcStr[9];
                  snprintf ( crcStr, 9, "%08X", crc );
                  write ( io_fd, rsnd, strlen(rsnd) );
                  write ( io_fd, crcStr, strlen(crcStr) );
                  write ( io_fd, Terminator, strlen(Terminator) );
                }
              }
            }
          }
        }
      }

      lastrxed = now;   //  Make sure to wait again!
    }

    now = time((time_t*)0);
    if ( now > statusout+5 ) {
      syslog_out ( SYSLOG_INFO, function_name, "Status %hu / %hu", rx_profile.dp_rxed, rx_profile.dp_need );
      statusout = now;
    }

    //  Loop as long as
    //    there is no packet zero (indicated by dp_need == 0)
    //    or not all packets are received (dp_rxed < dp_need)
    //  However, there is a timeout!

  } while ( (rx_profile.dp_need==0 || rx_profile.dp_rxed < rx_profile.dp_need )
         && time((time_t*)0) < timeout );

  syslog_out ( SYSLOG_INFO, function_name, "Status %hu / %hu", rx_profile.dp_rxed, rx_profile.dp_need );

  if ( rx_profile.dp_rxed < rx_profile.dp_need ) {
    uint16_t p;
    for ( p=1; p<=rx_profile.dp_need; p++ ) {
      syslog_out ( SYSLOG_INFO, function_name, "Status %hu : %c", p, rx_profile.dp_have[p] ? '+' : '-' );
    }
  }

  //  If total timeout, process all available data.
  //

  syslog_out ( SYSLOG_NOTICE, function_name, "Received %zu characters", end_input );

  free ( all_input );

  close( io_fd );
  close( socket_fd );

  syslog_out ( SYSLOG_NOTICE, function_name, "End" );

  return 0;
}


