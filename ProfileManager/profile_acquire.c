# include "profile_acquire.h"

# include <stdio.h>

# include "profile_description.h"
# include "sensor_data.h"

static void get_measurement( Measurement_t* m ) {
  m->done = 0;
  m->type = ' ';
  char line[32];
  fgets ( line, 31, stdin );
  switch ( line[0] ) {
  case 'S': generate_fake_hyper( &(m->data.hyper), 1 ); m->type = line[0]; break;
  case 'P': generate_fake_hyper( &(m->data.hyper), 2 ); m->type = line[0]; break;
  case 'O': generate_fake_ocr  ( &(m->data.ocr  ) ); m->type = line[0]; break;
  case 'M': generate_fake_mcoms( &(m->data.mcoms) ); m->type = line[0]; break;
  case 'x':
  case 'X': m->done =      1 ; break;
  default : m->type = ' ';
            m->done =  0 ;     break;
  }
  printf ( "Got %c\n", m->type );
}

int profile_acquire ( const char* data_dir, uint16_t profile_ID ) {
  const char* const function_name = "profile_acquire()";

  Profile_Description_t profile_description;

  if ( profile_description_init ( &profile_description, data_dir, profile_ID ) ) {
     return 1;
  }

  profile_description_save( &profile_description, data_dir );

  Measurement_t a_measurement;
  do {
    get_measurement( &a_measurement );
    save_measurement( &a_measurement, data_dir, profile_description.profile_yyddd );
    profile_description_update( &profile_description, a_measurement.type );
    profile_description_save( &profile_description, data_dir );
  } while ( !a_measurement.done );


  return 0;
}

