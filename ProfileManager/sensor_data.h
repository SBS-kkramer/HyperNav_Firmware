# ifndef _PM_SENSOR_DATA_H_
# define _PM_SENSOR_DATA_H_

# include <time.h>
# include <sys/time.h>
# include <unistd.h>
# include <stdint.h>

# include "spectrometer_data.h"
# include "ocr_data.h"
# include "mcoms_data.h"



typedef struct Measurement {
  uint16_t done;
  char     type;
  union {
    Spectrometer_Data_t hyper;
    OCR_Data_t            ocr;
    MCOMS_Data_t        mcoms;
  } data;
} Measurement_t;

void generate_fake_hyper ( Spectrometer_Data_t* h, uint16_t side );
void generate_fake_ocr( OCR_Data_t* o );
void generate_fake_mcoms( MCOMS_Data_t* m );

void save_measurement( Measurement_t* m, const char* data_dir, uint16_t profile_yyddd );

# endif
