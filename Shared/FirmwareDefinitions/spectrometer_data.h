/*! \file spectrometer_data.h
 *
 *  \brief Define the data structure
 *         for spectrometer and its auxiliary data
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef   _SPECTROMETER_DATA_H_
# define   _SPECTROMETER_DATA_H_

# include <stdint.h>

# include <time.h>
# include <sys/time.h>

//  The number of spectrometer pixels
//
# define N_SPEC_PIX 2048

//  Defines for the tag
# define SAD_TAG_DATA_MASK         0x000F
# define SAD_TAG_DARK              0x0001  //  Dark Frame (Cal Mode)
# define SAD_TAG_LIGHT             0x0002  //  Light Frame (Cal Mode)
# define SAD_TAG_CHAR_DARK         0x0004  //  Dark Frame (Dark Characterization)
# define SAD_TAG_LIGHT_MINUS_DARK  0x0008  //  Light-minus-Dark Frame (Free falling profiler)

# define SAD_TAG_QUAL_S_MASK       0x00F0
# define SAD_TAG_QUAL_SUNNY        0x0010  //  Radiometer was sun side during acquisition
# define SAD_TAG_QUAL_SHADE_NO     0x0020  //  Radiometer was shade side during acquisition (-90..-45, +45..+90 degree)
# define SAD_TAG_QUAL_SHADE_PART   0x0040  //  Radiometer was in partial shade during acquisition (-45..-10, +10..+45 degree)
# define SAD_TAG_QUAL_SHADE_FULL   0x0080  //  Radiometer was in full shade during acquisition (-10..+10 degree)

# define SAD_TAG_QUAL_F_MASK       0x0F00
# define SAD_TAG_QUAL_F_TILT_NO    0x0100  //  Float tilt < x deg
# define SAD_TAG_QUAL_F_TILT_LOW   0x0200  //  Float tilt > x deg, < y deg
# define SAD_TAG_QUAL_F_TILT_MED   0x0400  //  Float tilt > y deg, < z deg
# define SAD_TAG_QUAL_F_TILT_HIGH  0x0800  //  Float tilt > z deg

# define SAD_TAG_QUAL_R_MASK       0xF000
# define SAD_TAG_QUAL_R_TILT_NO    0x1000  //  Radiometer head tilt < x deg
# define SAD_TAG_QUAL_R_TILT_LOW   0x2000  //  Radiometer head tilt > x deg, < y deg
# define SAD_TAG_QUAL_R_TILT_MED   0x4000  //  Radiometer head tilt > y deg, < z deg
# define SAD_TAG_QUAL_R_TILT_HIGH  0x8000  //  Radiometer head tilt > z deg

# define SAD_TAG_READOUT_NO    0x00010000  //  No readout 
# define SAD_TAG_READOUT_PART  0x00020000  //  Partial readout 

# define SAD_TAG_PT_TEMP_NO    0x10000000
# define SAD_TAG_PT_TEMP_NS    0x20000000
# define SAD_TAG_PT_TEMP_NA    0x40000000
# define SAD_TAG_PT_TEMP_OFF   0x80000000
# define SAD_TAG_PT_PRES_NO    0x01000000
# define SAD_TAG_PT_PRES_NS    0x02000000
# define SAD_TAG_PT_PRES_NA    0x04000000
# define SAD_TAG_PT_PRES_OFF   0x08000000

typedef struct Spec_Aux_Data {

  struct timeval acquisition_time;

  uint16_t       integration_time;
  uint16_t       sample_number;
  uint16_t       dark_average;
  uint16_t       dark_noise;
   int16_t       light_minus_dark_up_shift;
   int16_t       spectrometer_temperature;
   int32_t       pressure;
   int32_t       pressure_T_counts;
  uint32_t       pressure_T_duration;
   int32_t       pressure_P_counts;
  uint32_t       pressure_P_duration;
  uint16_t       sun_azimuth;
  uint16_t       housing_heading;
   int16_t       housing_pitch;
   int16_t       housing_roll;
   int16_t       spectrometer_pitch;
   int16_t       spectrometer_roll;
  uint32_t       tag;                //  Bit field - see SAD_TAG_* defines above
  uint16_t       side;
  uint16_t       spec_min;
  uint16_t       spec_max;

} Spec_Aux_Data_t;

typedef struct Spectrometer_Data {

  uint16_t        hnv_spectrum[N_SPEC_PIX];
  Spec_Aux_Data_t aux;

} Spectrometer_Data_t;

# endif // _SPECTROMETER_DATA_H_
