
# include "profiles_list.h"

# include <dirent.h>
# include <errno.h>
# include <stdio.h>
# include <sys/stat.h>

# include "profile_description.h"

int profiles_list ( const char* data_dir ) {

  struct stat data_dir_info;
  if ( -1 == stat( data_dir, &data_dir_info ) ) {
    return -1;
  }

  printf ( "Listing %s\n", data_dir );
  DIR* dirp = opendir ( data_dir );

  struct dirent* entry;

  while ( entry = readdir ( dirp ) ) {
    if ( entry->d_type == DT_DIR
      && 0 != strcmp ( entry->d_name, "."  )
      && 0 != strcmp ( entry->d_name, ".." ) ) {

      //  Parse sub-directory name,
      //  and if it is a valid number,
      //  retrieve profile description (if available)
      errno = 0;
      char* end;
      long  profile_number = strtol ( entry->d_name, &end, 10 );

      if ( !errno && end != entry->d_name ) {
        if ( 0 < profile_number && profile_number <= 65535 ) {

          uint16_t profile_ID = (uint16_t)profile_number;
          Profile_Description_t pd;

          if ( 0 == profile_description_retrieve( &pd, data_dir, profile_ID ) ) {
            printf ( "%s %hu %hu %hu %hu\n", entry->d_name,
                          pd.nSBRD, pd.nPORT, pd.nOCR, pd.nMCOMS );
          } else {
            printf ( "%s NO-Info\n", entry->d_name );
          }
        }
      }
    }
  }

  closedir ( dirp );

  return 0;
}


