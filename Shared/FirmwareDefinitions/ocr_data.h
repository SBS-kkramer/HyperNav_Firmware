/*! \file ocr_data.h
 *
 *  \brief Define the data structure
 *         for OCR (ocean color radiometer)
 *         and its auxiliary data
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef   _OCR_DATA_H_
# define   _OCR_DATA_H_

# include <stdint.h>

# include <time.h>
# include <sys/time.h>

# define N_OCR_PIX 4

typedef uint32_t OCR_Pixel;

typedef struct OCR_Aux_Data
{
  struct timeval acquisition_time;
} OCR_Aux_Data_t;

typedef struct OCR_Data
{
  OCR_Pixel      pixel[N_OCR_PIX];
  OCR_Aux_Data_t aux;
} OCR_Data_t;

# define OCR_FRAME_LENGTH 46
typedef struct OCR_Frame
{

  //  The frame is always fixed length
  uint8_t frame[OCR_FRAME_LENGTH];
  OCR_Aux_Data_t aux;

} OCR_Frame_t;

# endif // _OCR_DATA_H_

