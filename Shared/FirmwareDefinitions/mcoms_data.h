/*! \file mcoms_data.h
 *
 *  \brief Define the data structure
 *         for MCOMS
 *         and its auxiliary data
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef   _MCOMS_DATA_H_
# define   _MCOMS_DATA_H_

# include <stdint.h>

# include <time.h>
# include <sys/time.h>

//  There is a bit of fudging here:
//  The 3 MCOMS sensors need 3 x 16 bit + 1 x 32 bit each.
//  Hence 3 x 5 x 16 bit
//
//  Provide two access methods:
//  (1) by 12 individual identifiers
//  (2) as a 16bit[15] array
//
# define N_MCOMS_PIX 15

typedef uint16_t MCOMS_Pixel;

typedef struct MCOMS_Aux_Data {

  struct timeval acquisition_time;

} MCOMS_Aux_Data_t;

typedef struct MCOMS_Data {

  int16_t  chl_led;
  int16_t  chl_low;
  int16_t  chl_hgh;
  int32_t  chl_value;

  int16_t   bb_led;
  int16_t   bb_low;
  int16_t   bb_hgh;
  int32_t   bb_value;

  int16_t fdom_led;
  int16_t fdom_low;
  int16_t fdom_hgh;
  int32_t fdom_value;

  MCOMS_Aux_Data_t aux;

} MCOMS_Data_t;

# define MCOMS_FRAME_LENGTH 128
typedef struct MCOMS_Frame {

  //  MCOMS frames are variable length
  uint8_t  frame[MCOMS_FRAME_LENGTH];

  MCOMS_Aux_Data_t aux;

} MCOMS_Frame_t;

# endif // _MCOMS_DATA_H_

