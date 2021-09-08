# include "profile_transmit.h"

# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>

# include <sys/ioctl.h>

# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>

# include "zlib.h"

# include "profile_description.h"
# include "profile_packet.h"

static int packet_process_transmit ( Profile_Data_Packet_t* original, transmit_instructions_t* tx_instruct, uint8_t* bstStatus, const char* data_dir, int tx_fd ) {

  // Process packet
  //   Binary -> Graycode -> Bitplaned -> Compressed -> ASCII-Encoded

  //  Represent spectra pixel values in graycode
  //

  Profile_Data_Packet_t represent;
    
  //  TODO: Round to proper position before graycoding if noise_remove>0

  if  (tx_instruct->representation == REP_GRAY)
  {
    memset( &represent, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bin2gray( &original, &represent ) )
    {
      fprintf ( stderr, "Failed in data_packet_bin2gray()\n" );
      return 1;
    }
    else
    {
      data_packet_save ( &represent, data_dir, "G" );
    }
  }
  else
  {
    memcpy( &represent, &original, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  gray %d\n", p );

  //  Arrange the spectral data in bitplanes
  //

  Profile_Data_Packet_t arranged;

  if  ( tx_instruct->use_bitplanes == BITPLANE )
  {
    memset( &arranged, 0, sizeof(Profile_Data_Packet_t) );
    if ( data_packet_bitplane( &represent, &arranged, tx_instruct->noise_bits_remove ) )
    {
      fprintf ( stderr, "Failed in data_packet_bitplane()\n" );
      return 1;
    }
    data_packet_save ( &arranged, data_dir, "B" );
  }
  else
  {
    memcpy ( &arranged, &represent, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  plne %d\n", p );

  //  Compress the packet
  //

  Profile_Data_Packet_t compressed;
    
  if ( tx_instruct->compression == ZLIB )
  {
    memset( &compressed, 0, sizeof(Profile_Data_Packet_t) );
    char compression = 'G';
    if ( data_packet_compress( &arranged, &compressed, compression ) )
    {
      fprintf ( stderr, "Failed in data_packet_compress()\n" );
      return 1;
    }
    data_packet_save ( &compressed, data_dir, "C" );
  } else {
    memcpy( &compressed, &arranged, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  zlib %d\n", p );

  //  Encode the packet
  //

  Profile_Data_Packet_t encoded;
    
  if ( tx_instruct->encoding != NO_ENCODING )
  {
    memset( &encoded, 0, sizeof(Profile_Data_Packet_t) );
    char encoding;
  
    switch ( tx_instruct->encoding )
    {
    case ASCII85: encoding = 'A'; break;
    case BASE64 : encoding = 'B'; break;
    default     : encoding = 'N'; break;
    }
    
    if  ( data_packet_encode ( &compressed, &encoded, encoding ) )
    {
      fprintf ( stderr, "Failed in data_packet_compress()\n" );
      return 1;
    }
    data_packet_save ( &encoded, data_dir, "E" );
  }
  else
  {
    memcpy( &encoded, &compressed, sizeof(Profile_Data_Packet_t) );
  }
  //fprintf ( stderr, "  zlib %d\n", p );

  //  Transmit the packet
  //

  return data_packet_txmit ( &encoded, tx_instruct->burst_size, tx_fd, bstStatus );
}



int profile_transmit ( const char*              data_dir,
                       uint16_t                 proID,
                       transmit_instructions_t* tx_instruct,
                       uint16_t                 port,
                       char*                    host_ip
                     )
{
  const char* const function_name = "profile_transmit()";



  int tx_fd = -1;   //  Transmit file descriptor
                    //  If 0==port use stdout (fd=1)
                    //  Else open socket to host_ip:port

  if ( 0 == port ) {
    tx_fd = 1;
  } else {

    tx_fd = socket(AF_INET, SOCK_STREAM, 0);
    if ( tx_fd < 0 ) {
      perror ( "PMTX-ERROR" );
      return 1;
    }

    struct hostent* server = gethostbyname( host_ip );
    if ( NULL == server ) {
      fprintf ( stderr, "PMTX-ERROR, no '%s' host\n", host_ip );
      return 1;
    }

    struct sockaddr_in server_address;
    memset ( &server_address, 0, sizeof(server_address) );

    server_address.sin_family = AF_INET;
 // memcpy ( &server_address.sin_addr.s_addr, server->h_addr, server->h_length);
    bcopy((char *)server->h_addr, (char *)&server_address.sin_addr.s_addr, server->h_length);
    server_address.sin_port = htons(port);

    if (connect( tx_fd, (struct sockaddr*) &server_address, sizeof(server_address) ) < 0 ) {
      char msg[128];
      snprintf ( msg, 128, "PMTX-ERROR: Connect to %s %x", host_ip, server->h_addr );
      perror( msg );
      return 1;
    }
    
  }

  //  Set I/O to NON Blocking --  Required to not block on no input
  //
  int flg = 1;
  ioctl ( tx_fd, FIONBIO, &flg );

  // Get information about the profile to be transferred
  //
  Profile_Description_t profile_description;
  profile_description_retrieve ( &profile_description, data_dir, proID );

  profile_description.profiler_sn = 876;

  // Define packets
  //
  Profile_Packet_Definition_t packet_definition;
  packet_definition.profiler_sn    = profile_description.profiler_sn;
  packet_definition.profile_yyddd  = profile_description.profile_yyddd ;

  packet_definition.numData_SBRD   = profile_description.nSBRD;
  packet_definition.numData_PORT   = profile_description.nPORT;
  packet_definition.numData_OCR    = profile_description.nOCR;
  packet_definition.numData_MCOMS  = profile_description.nMCOMS;

  packet_definition.nPerPacket_SBRD   = 5;
  packet_definition.nPerPacket_PORT   = 5;
  packet_definition.nPerPacket_OCR    = 500;
  packet_definition.nPerPacket_MCOMS  = 500;

  if ( profile_description.nSBRD )
  {
    packet_definition.numPackets_SBRD   = 1 +  ( profile_description.nSBRD -1 ) / packet_definition.nPerPacket_SBRD;
  }
  else
  {
    packet_definition.numPackets_SBRD   = 0;
  }

  if ( profile_description.nPORT )
  {
    packet_definition.numPackets_PORT   = 1 +  ( profile_description.nPORT -1 ) / packet_definition.nPerPacket_PORT;
  }
  else
  {
    packet_definition.numPackets_PORT   = 0;
  }

  if ( profile_description.nOCR )
  {
    packet_definition.numPackets_OCR    = 1 +  ( profile_description.nOCR  -1 ) / packet_definition.nPerPacket_OCR;
  }
  else
  {
    packet_definition.numPackets_OCR    = 0;
  }
  
  if ( profile_description.nMCOMS )
  {
    packet_definition.numPackets_MCOMS  = 1 +  ( profile_description.nMCOMS-1 ) / packet_definition.nPerPacket_MCOMS;
  }
  else
  {
    packet_definition.numPackets_MCOMS  = 0;
  }

  // Generate packet files from profile data files
  // (split by size and prepend by packet header)
  //
  uint16_t num_packets = 0;
  char fname[32];

  // generate_header packet file (number 0)
  Profile_Info_Packet_t info_packet;
  profile_packet_definition_to_info_packet ( &packet_definition, &info_packet );

  // FIXME -- Profile_Info_Packet_t MetaData
  int m;
  for  (m = 0;  m < BEGPAK_META;  m++)
  {
    info_packet.meta_info[m] = '.';
  }

  info_packet_save  ( &info_packet, data_dir, "Z" );

  num_packets ++;

  //  Package Starboard Radiometer Data

  sprintf ( fname, "%s/%05d/M.sbd", data_dir, profile_description.profile_yyddd );
  FILE* fp = fopen ( fname, "r" );

  if ( !fp ) {
    fprintf ( stderr, "Cannot open %s\n", fname );
    return 1;
  }

  uint16_t sP;
  for ( sP = 1;  sP <= packet_definition.numPackets_SBRD;  sP++, num_packets++  )
  {
    //fprintf ( stderr, "Building packet %c %d\n", 'S', sP );
    Profile_Data_Packet_t raw;
    char numString[16];

    raw.HYNV_num = packet_definition.profiler_sn;
    raw.PROF_num = packet_definition.profile_yyddd;
    raw.PCKT_num = num_packets;

    raw.sensor_type = 'S';
    raw.empty_space = ' ';
    memcpy ( raw.sensor_ID, "SATYLU0000", 10 );
    uint16_t number_of_data;
    if ( sP<packet_definition.numPackets_SBRD )
    {
      number_of_data = packet_definition.nPerPacket_SBRD;
    }
    else
    {  //  The last packet may contain fewer items
      number_of_data = profile_description.nSBRD - (packet_definition.numPackets_SBRD-1) * packet_definition.nPerPacket_SBRD;
    }
    snprintf ( numString, 5, "% 4hu", number_of_data );
    memcpy ( raw.number_of_data, numString, 4 );

    raw.representation     = 'B';
    raw.noise_bits_removed = 'N';
    raw.compression        = '0';
    raw.ASCII_encoding     = 'N';

    int d;
    for ( d = 0;  d < number_of_data;  d++ )
    {
      //fprintf ( stderr, "... frame %d\n", d+1 );
      Spectrometer_Data_t h;
      fread( &h, sizeof(Spectrometer_Data_t), 1, fp );

      memcpy ( raw.contents.structured.sensor_data.pixel[d], h.light_minus_dark_spectrum, N_SPEC_PIX*sizeof(uint16_t));
      memcpy ( &(raw.contents.structured.aux_data.spec_aux[d]), &(h.aux), sizeof(Spec_Aux_Data_t) );
    }

    uint16_t data_size = number_of_data * ( N_SPEC_PIX*sizeof (uint16_t) + sizeof (Spec_Aux_Data_t) );

    snprintf ( numString, 7, "% 6hu", data_size );
    memcpy ( raw.compressed_sz, "      ", 6 );
    memcpy ( raw.encoded_sz,    "      ", 6 );

    if ( data_packet_save ( &raw, data_dir, "Z" ) )
    {
       fclose ( fp );
       return 1;
    }
  }

  fclose( fp );
  
  // TODO PORT, OCR, MCOMS

  size_t start_input = 0;
  size_t end_input = 0;
  size_t const size_input = 2*1024L;
  unsigned char* all_input = malloc ( size_input );

  // num_packets = num_data * sizeof(packet_data) / sizeof(data)
  //

  uint16_t  maxBrsts  = sizeof(Profile_Data_Packet_t) / tx_instruct->burst_size;

  uint8_t   allAck = 0;
  //  Status: 0 = Done Sending / 1 = To Send / 2 = Received acknowledged
  uint8_t*  pckStatus = malloc ( (size_t)num_packets * sizeof(uint8_t) );
  uint8_t** bstStatus = malloc ( (size_t)num_packets * sizeof(uint8_t*) );
  if ( bstStatus ) {
    bstStatus[0] = (uint8_t*) malloc ( (size_t)num_packets*maxBrsts*sizeof(uint8_t) );
    if ( !bstStatus[0] ) {
      free ( bstStatus );
      bstStatus = 0;
    } else {
      uint16_t p;
      for ( p=1; p<num_packets; p++ ) {
        bstStatus[p] = bstStatus[p-1] + maxBrsts;
      }
    }
  }

  if ( !all_input | !pckStatus | !bstStatus ) {
    //  SYSTEM FAILURE!!!
    //  Need fallback
    fprintf ( stderr, "SYSTEM FAILURE MALLOC - Implement fallback" );
  }
  
  uint16_t p;
  uint16_t b;

  for  (p = 0;  p < num_packets;  p++)
  {
    pckStatus[p] = 1;
    for  (b = 0;  b < maxBrsts;  b++)
    {
      bstStatus[p][b] = 1;
    }
  }

  time_t lastRx = time((time_t*)0);

  uint16_t needToSend = 1;

  do {

    uint8_t newResendRequest = 0;

    for ( p = 0;  p < num_packets  &&  !newResendRequest;  p++ )
    {

      if  (1 != pckStatus[p])
      {
        //fprintf ( stderr, "Tx %hu not required\n", p );
      }
      else
      {
        fprintf ( stderr, "Tx %hu ...\n", p );

        if  ( 0 == p )
        {
          if  ( 0 == info_packet_txmit ( &info_packet, tx_instruct->burst_size, tx_fd, bstStatus[p] ) )
          {
            pckStatus[p] = 0;  //  transmit was ok, done this one
            for  (b = 0;  b < maxBrsts;  b++ )
            {
              bstStatus[p][b] = 0;
            }
            fprintf ( stderr, "Tx %hu done\n", p );
          } 
          else
          {
            fprintf ( stderr, "Tx %hu failed\n", p );
          }
        }
        else
        {

          // Read a packet
          //
          Profile_Data_Packet_t original;
          memset ( &original, 0, sizeof (Profile_Data_Packet_t) );
          data_packet_retrieve ( &original, data_dir, "Z", proID, p );
          //fprintf ( stderr, "  read %d\n", p );
	        //

	        if  ( 0 == packet_process_transmit ( &original, tx_instruct, bstStatus[p], data_dir, tx_fd ) ) 
          {
            pckStatus[p] = 0;  //  transmit was ok, done this one
            for  ( b = 0;  b < maxBrsts;  b++ )
            {
              bstStatus[p][b] = 0;
            }
	        }
        }
      }

      //  Read up to RXBUFSZ bytes from current input into buffer
      //
    # define RXBUFSZ 4096
      char buf[RXBUFSZ];
      size_t n;

      if ( 0 >= ( n = read ( tx_fd, buf, RXBUFSZ ) ) )
      {
        //fprintf ( stderr, "No input.\n" );
      }
      else
      {

        lastRx = time((time_t*)0);

        //  Append new input to existing (unparsed) input
        //  start_input == 0 by design
        //
        memcpy ( all_input+end_input, buf, n );
        end_input += n;

        // fprintf ( stderr, "Parse %d bytes [%d..%d]\n", end_input-start_input, start_input, end_input );
        // fprintf ( stderr, "Parse %*.*s\n", end_input-start_input, end_input-start_input, all_input+start_input );

        //  Scan as long as there is terminated input
        //
        unsigned char* terminator = 0;
        while  ( end_input - start_input > 6
             &&  0 != (terminator = strstr ( all_input + start_input, "\r\n" ) ) )
        {

          terminator[0] = 0;

          uint16_t hID, prID, paID, bID;
          uint32_t crcRX;

          if  ( 0 == strncmp ( all_input+start_input, "RXED", 4 ) ) 
          {

            sscanf ( all_input + start_input + 4, ",%hu,%hu,%hu,%hu,%x",
                            &hID, &prID, &paID, &bID, &crcRX );
            
            if  ( hID != profile_description.profiler_sn    ||    prID != profile_description.profile_yyddd )
            {
              fprintf ( stderr, "<- Server RXED incorrect IDs: %s\n", all_input+start_input );
            }
            else
            {
              uLong crc = crc32 ( 0L, Z_NULL, 0 );
              crc = crc32 ( crc, all_input + start_input, 25 );

              if  ( crc != crcRX )
              {
                fprintf ( stderr, "<- Server RXED Packet %hu - CRC %08x %08x mismatch\n", paID, crcRX, crc );
              }
              else
              {

                fprintf ( stderr, "<- Server RXED Packet %hu\n", paID );
                if  ( paID < num_packets )
                {
                  switch ( pckStatus[paID] )
                  {
                    case  0: pckStatus[paID] = 2; break;
                    case  1: fprintf ( stderr, "Error: Server confirmed receive of not yet sent packet %hu\n", paID ); break;
                    case  2: fprintf ( stderr, "Error: Server re-confirmed receive of packet %hu\n", paID ); break;
                    default: fprintf ( stderr, "Fatal: Server requested resend of undefined packet %hu\n", paID ); break;
                  }
                }
              }
            }
          }

          else if  ( 0 == strncmp ( all_input + start_input, "RSND", 4 ) )
          {

            sscanf ( all_input+start_input + 4, ",%hu,%hu,%hu,%hu,%x",
                            &hID,
                            &prID,
                            &paID,
                            &bID,
                            &crcRX
                   );

            if  ( hID != profile_description.profiler_sn  ||  prID != profile_description.profile_yyddd )
            {
              fprintf ( stderr, "<- Server RSND incorrect IDs: %s\n", all_input+start_input );
            }
            else
            {
              uLong crc = crc32 ( 0L, Z_NULL, 0 );
                    crc = crc32 ( crc, all_input + start_input, 25 );
              if ( crc != crcRX )
              {
                fprintf ( stderr, "<- Server RSND Packet %hu - CRC %08x %08x mismatch\n", paID, crcRX, crc );
              }
              else
              {

                fprintf ( stderr, "<- Server RSND request Packet %hu %hu\n", paID, bID );
                if  ( paID < num_packets )
                {

                  newResendRequest = 1;

                  if  ( 0 == pckStatus[paID]   ||   1 == pckStatus[paID] )
                  {
                    if ( 999 == bID )
                    {
                      //  Resend all bursts
                      for ( b=0; b<maxBrsts; b++ )
                      {
                        bstStatus[paID][b] = 1;
                      }
                    }
                    else
                    {
                        bstStatus[paID][bID] = 1;
                    }
                  }

                  switch ( pckStatus[paID] )
                  {
                    case  0: pckStatus[paID] = 1; break;
                    case  1: if ( 999 == bID ) fprintf ( stderr, "Error: Server requested resend of not yet sent packet %hu\n", paID ); break;
                    case  2: fprintf ( stderr, "Error: Server requested resend of received packet %hu\n", paID ); break;
                    default: fprintf ( stderr, "Fatal: Server requested resend of undefined packet %hu\n", paID ); break;
                  }
                }
              }
            }

          }
          else if ( 0 == strncmp ( all_input + start_input, "ALIF", 4 ) )
          {
            //  TODO mod timeout
          }

          else
          {
            fprintf ( stderr, "<- Server sent %s\n", all_input+start_input );
          }

          start_input += strlen(all_input+start_input) + 2;
        }

        //  Shift unparsed input to start of memory
        //
        memmove ( all_input, all_input+start_input, end_input-start_input );
        end_input -= start_input;
        start_input = 0;

        if  ( end_input > 0 )
          fprintf ( stderr, "Unparsed %d\n", end_input );

        if ( end_input > 64 )
        {
          //  There is a problem!
          //  Commands have a max lenght of 64 bytes,
          //  and incomplete commands must be cleaned up
          //  to prevent input buffer contamination!
        }
      }

    }  //  End of packet tx loop


    if  ( !newResendRequest )
    {
      //  Allow time to receive all resend requests

      allAck = 2;

      for ( p=0; p<num_packets; p++ )
      {
        if ( 2 != pckStatus[p] )
        {
          allAck = 0;
        }
      }
    }

    //fprintf ( stderr, "%d %d %d\n", (int)allAck, time((time_t*)0), lastRx );

  } while ( allAck!=2 && time((time_t*)0)<lastRx+120 );

  //  TODO == Send End-of-Transmission ???

  free ( all_input );

  return 0;
}
