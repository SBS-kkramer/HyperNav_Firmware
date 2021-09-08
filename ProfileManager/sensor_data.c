# include "sensor_data.h"

# include <stdio.h>

void generate_fake_hyper ( Spectrometer_Data_t* h, uint16_t side ) {

  int p;
  for ( p=0; p<N_SPEC_PIX; p++ ) {
    h->light_minus_dark_spectrum[p] = 0x0040 + ( 0xFF00 & random() );
  }
  h->aux.integration_time = 128;
  h->aux.sample_number    = 1;
  h->aux.dark_average     = 0x0100 + ( 0x002F & random() );
  h->aux.dark_noise       = 0x0010 + ( 0x0004 & random() );
  h->aux.spectrometer_temperature = 1230;
  gettimeofday ( &(h->aux.acquisition_time), (void*) 0 );
  h->aux.pressure         = 123.456F;
  h->aux.float_heading    = -8989;
  h->aux.float_yaw        =   111;
  h->aux.float_roll       =  -222;
  h->aux.radiometer_yaw   =   -99;
  h->aux.radiometer_roll  =   188;
  h->aux.tag              =     0;
  h->aux.side             =  side;
}



void  generate_fake_ocr (OCR_Data_t* o)
{
  int p;
  for  (p = 0;  p < 4;  p++)
  {
    o->channel[p] = 0x0040 + ( 0x1FFF & random() );
  }
  o->aux.sample_number = 1;
  gettimeofday ( &(o->aux.acquisition_time), (void*) 0 );
}



void generate_fake_mcoms( MCOMS_Data_t* m )
{
  int p, q;
  for  (p = 0;  p < 4;  p++)
  {
    for  (q = 0;  q < 3;  q++)
    {
      m->rawdata[q][p] = 0x0040 + (0x1FFF & random ());
    }
  }

  m->aux.sample_number    = 1;
  gettimeofday (&(m->aux.acquisition_time), (void*) 0 );
}



// FIXME ??
void  save_measurement (Measurement_t* m, const char* data_dir, uint16_t profile_yyddd )
{
  char fname[32];

  switch  (m->type)
  {
  case 'S': sprintf (fname, "%05hu/M.sbd", profile_yyddd); break;
  case 'P': sprintf (fname, "%05hu/M.prt", profile_yyddd); break;
  case 'O': sprintf (fname, "%05hu/M.ocr", profile_yyddd); break;
  case 'M': sprintf (fname, "%05hu/M.mcm", profile_yyddd); break;
  default : fname[0] = 0;
  }

  if  (fname[0])
  {
    FILE* fp = fopen (fname, "a");
    if  (fp)
    {
      switch (m->type)
      {
      case 'S': fwrite (&(m->data.hyper), sizeof (Spectrometer_Data_t), 1, fp); break;
      case 'P': fwrite (&(m->data.hyper), sizeof (Spectrometer_Data_t), 1, fp); break;
      case 'O': fwrite (&(m->data.ocr  ), sizeof (OCR_Data_t),          1, fp); break;
      case 'M': fwrite (&(m->data.mcoms), sizeof (MCOMS_Data_t),        1, fp); break;
      }
      fclose  (fp);
    }
  }
}
