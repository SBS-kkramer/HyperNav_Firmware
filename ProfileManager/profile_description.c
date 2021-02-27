# include "profile_description.h"

# include <libgen.h>

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/stat.h>
# include <time.h>
# include <sys/time.h>

static char* profile_description_filename ( const char* data_dir, uint16_t profile_ID ) {

  //  Return description file name for the specified profile
  char* pd_fn = malloc ( (data_dir?strlen(data_dir):2) + 32 );

  if ( pd_fn ) {
    if ( !data_dir || !data_dir[0] ) {
      sprintf ( pd_fn, "%s/%05hu/Prof.dsc", ".",      profile_ID );
    } else {
      sprintf ( pd_fn, "%s/%05hu/Prof.dsc", data_dir, profile_ID );
    }
  }

  return pd_fn;
}

static int profile_description_mkdir ( const char* const dir ) {

  if ( !dir ) return 1;

  //  If there is a '/' in the current path name,
  //  first make the higher level dir
  //
  if ( strchr ( dir, '/' ) ) {

    char* dir_copy = strdup ( dir );
    char* high_dir = dirname ( dir_copy );

    if ( profile_description_mkdir ( high_dir ) ) {
      //  The higher level dir could not be created.
      //  BAD! - Error message created inside callee
      free ( dir_copy );
      return 1;
    }

    free ( dir_copy );
  }

  //  Check if the directory exists.
  //
  struct stat statbuf;
  if ( stat( dir, &statbuf ) ) {
    //  dir does not exist ...
    if ( mkdir ( dir, S_IRWXU | S_IRWXG | S_IRWXO ) ) {
      //  ... and cannot be created
      perror ( "Cannot create directory" );
      fprintf ( stderr, "Cannot mkdir(%s)\n", dir );
      return 1;
    } else {
      //  ... and was successfully created
      return 0;
    }
  } else {
    //  dir exists ...
    if ( ! ( statbuf.st_mode &= S_IFDIR ) ) {
      //  ... but is not a directory
      fprintf ( stderr, "%s is not a directory\n", dir );
      return 1;
    } else {
      //  ... and is a directory
      return 0;
    }
  }

  //  Never reaching this point
  return 0;

}

int profile_description_init ( Profile_Description_t* pd, const char* const data_dir, uint16_t profile_ID ) {

  //  If the profile ID is zero, use current YYDDD as ID.
  //
  if ( 0 == profile_ID ) {
    time_t time_now = time((time_t*)0);
    struct tm* tm_now = gmtime( &time_now );
    profile_ID = 1000*(tm_now->tm_year-100) + tm_now->tm_yday;
  }

  //  If data_dir does not exist, create it
  //  If directory cannot be created, return FAIL
  //
  if ( profile_description_mkdir ( data_dir ) ) {
    return -1;
  }

  //  If there is already a profile under profile_ID, return FAIL.
  //
  char dname[strlen(data_dir)+32];
  sprintf ( dname, "%s/%05hu", data_dir, profile_ID );

  struct stat statbuf;
  if ( 0 == stat( dname, &statbuf ) ) {
    fprintf ( stderr, "Profile %s already exists\n", dname );
    return -1;
  }

  //  Create a new directory
  //
  if ( profile_description_mkdir ( dname ) ) {
    return -1;
  }

  //  Initialize Profile_Description
  //
  pd->profiler_sn   = 0;
  pd->profile_yyddd = (uint16_t)profile_ID;
  pd->nSBRD = pd->nPORT = pd->nOCR = pd->nMCOMS = 0;

  //  Success
  //
  return 0;
}

int profile_description_update( Profile_Description_t* pd, char measurement_type ) {

  if ( 0 == pd ) {
    return -1;
  }

  switch( measurement_type ) {
  case 'S': pd->nSBRD ++; break;
  case 'P': pd->nPORT ++; break;
  case 'O': pd->nOCR  ++; break;
  case 'M': pd->nMCOMS++; break;
  default : /* do nothing */ break;
  }

  return 0;
}

int profile_description_save( Profile_Description_t* pd, const char* data_dir ) {

  char* fname = profile_description_filename( data_dir, pd->profile_yyddd );
  FILE* fp = fopen ( fname, "w" );
  free ( fname );

  if ( fp ) {
    fprintf ( fp, "%hu\r\n", pd->profiler_sn );
    fprintf ( fp, "%hu\r\n", pd->profile_yyddd );
    fprintf ( fp, "%hu\r\n", pd->nSBRD );
    fprintf ( fp, "%hu\r\n", pd->nPORT );
    fprintf ( fp, "%hu\r\n", pd->nOCR );
    fprintf ( fp, "%hu\r\n", pd->nMCOMS );
    fclose  ( fp );
    return 0;
  } else {
    return -1;
  }
}

int profile_description_retrieve( Profile_Description_t* pd, const char* data_dir, uint16_t profile_ID ) {

  char* fname = profile_description_filename( data_dir, profile_ID );
  FILE* fp = fopen ( fname, "r" );
  free ( fname );

  if ( fp ) {
    fscanf ( fp, "%hu\r\n", &(pd->profiler_sn) );
    fscanf ( fp, "%hu\r\n", &(pd->profile_yyddd) );
    fscanf ( fp, "%hu\r\n", &(pd->nSBRD) );
    fscanf ( fp, "%hu\r\n", &(pd->nPORT) );
    fscanf ( fp, "%hu\r\n", &(pd->nOCR) );
    fscanf ( fp, "%hu\r\n", &(pd->nMCOMS) );
    fclose ( fp );
    return 0;
  } else {
    return -1;
  }
}

int profile_packet_definition_to_info_packet ( Profile_Packet_Definition_t* ppd, Profile_Info_Packet_t* pip ) {

  if ( !ppd ) return 1;
  if ( !pip ) return 1;

  char numString[16];

  pip->HYNV_num = ppd->profiler_sn;
  pip->PROF_num = ppd->profile_yyddd;
  pip->PCKT_num =   0;

  snprintf ( numString, 5, "% 4hu", ppd->numData_SBRD  ); memcpy ( pip->num_dat_SBRD,  numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numData_PORT  ); memcpy ( pip->num_dat_PORT,  numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numData_OCR   ); memcpy ( pip->num_dat_OCR,   numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numData_MCOMS ); memcpy ( pip->num_dat_MCOMS, numString, 4 );

  snprintf ( numString, 5, "% 4hu", ppd->numPackets_SBRD  ); memcpy ( pip->num_pck_SBRD,  numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numPackets_PORT  ); memcpy ( pip->num_pck_PORT,  numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numPackets_OCR   ); memcpy ( pip->num_pck_OCR,   numString, 4 );
  snprintf ( numString, 5, "% 4hu", ppd->numPackets_MCOMS ); memcpy ( pip->num_pck_MCOMS, numString, 4 );

  return 0;
}

int profile_packet_definition_from_info_packet ( Profile_Packet_Definition_t* ppd, Profile_Info_Packet_t* pip ) {

  if ( !ppd ) return 1;
  if ( !pip ) return 1;

  char numString[16];

  ppd->profiler_sn = pip->HYNV_num;
  ppd->profile_yyddd = pip->PROF_num;

  sscanf ( pip->num_dat_SBRD,  "%4hu", &(ppd->numData_SBRD) );
  sscanf ( pip->num_dat_PORT,  "%4hu", &(ppd->numData_PORT) );
  sscanf ( pip->num_dat_OCR,   "%4hu", &(ppd->numData_OCR) );
  sscanf ( pip->num_dat_MCOMS, "%4hu", &(ppd->numData_MCOMS) );

  //  This information is not part of Profile_Info_Packet_t
  ppd->nPerPacket_SBRD  = 0;
  ppd->nPerPacket_PORT  = 0;
  ppd->nPerPacket_OCR   = 0;
  ppd->nPerPacket_MCOMS = 0;

  sscanf ( pip->num_pck_SBRD,  "%4hu", &(ppd->numPackets_SBRD) );
  sscanf ( pip->num_pck_PORT,  "%4hu", &(ppd->numPackets_PORT) );
  sscanf ( pip->num_pck_OCR,   "%4hu", &(ppd->numPackets_OCR) );
  sscanf ( pip->num_pck_MCOMS, "%4hu", &(ppd->numPackets_MCOMS) );

  return 0;
}

