/*
 *  File     sn74v283.c
 * 
 *  API for FIFO to receive & briefly store data generated by ADC
 *
 *  Created  2017-May-30
 *  Author   bplache
 */ 

#include "sn74v283.h"

# include <string.h>

# include "board.h"
# include "gpio.h"

//  These two includes needed to access vTaskDelay()
//
# include "FreeRTOS.h"
# include "task.h"

# include "task.h"

# include "smc_fifo_A.h"
# include "smc_fifo_B.h"

# define SN74V283_MAX 15  //  Size-of-FIFO/Size-of-Spectrum = 65536/2069

//  These count the total number of spectra asquired after initialization
//
static uint16_t sn74v283_spectra[2] = { 0, 0 };
static uint16_t sn74v283_cleared[2] = { 0, 0 };

//  These count the spectra currently in the pipeline.
//  The spectrometer data are in the FIFO,
//  auxililary data are in static local memory.
//
//  Keep track of auxiliary data
//
static uint16_t sn74v283_start[2] = { 0, 0 };
static uint16_t sn74v283_count[2] = { 0, 0 };
static Spec_Aux_Data_t sn74v283_aux[2][SN74V283_MAX];

int16_t SN74V283_InitGpio ()
{
  //  The sn74v283 FIFOs each have a total of 80 pins.
  //  Initialization order is not by pin number, but by pin purpose.
  //
  //  Note: gpio_enable_gpio_pin () sets up for I/O.
  //                                Do not use for OUTPUT pins.
  //                                Seems to be acceptable for INPUT pins.
  //
  //  FIFO A
  //
  //  Configuration input
  //
  //  /MRS  (Master Reset)        set initially to low, to keep it in reset
  //
  //  All other configurations are hard wired:
  //  /PRS  (Partial Reset)           disable == VCC
  //  /LD   (Offset load)             disable == VCC
  //   FWFT (First Word Fall Through) disable == GND
  //   IW   (Input width)             18 bit  == GND
  //   OW   (Output width)            18 bit  == GND
  //   IP   (Interspersed parity)     Non-IP  == GND
  //   FSEL0                                  == GND
  //   FSEL1                                  == GND
  //  /SEN  (serial mode)             disable == VCC
  //
  gpio_configure_pin  (FIFO_A_RESETN, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);

  //  Status output
  //
  //  /EF (Empty Flag)
  //  /HF (Half-Full Flag)
  //  /FF (Full Flag)
  //  /PAE (Programmable Almost Empty Flag) - Not used - Ends at Test Point
  //  /PAF (Programmable Almost Full Flag) - Not used - Ends at Test Point
  //
  gpio_configure_pin     (FIFO_A_EMPTYN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_A_EMPTYN);
  gpio_enable_gpio_pin   (FIFO_A_EMPTYN);

  gpio_configure_pin     (FIFO_A_HFN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_A_NFN);
  gpio_enable_gpio_pin   (FIFO_A_HFN);

  gpio_configure_pin     (FIFO_A_FULLN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_A_FULLN);
  gpio_enable_gpio_pin   (FIFO_A_FULLN);

  //  Write control
  //
  //  /WEN (Write enable)          tied to ADC_PD line -- Check ADC initialization!
  //   WCLK (Write clock)          from ADC
  //   D0-D17 (Data lines)         from ADC
  //  

  //  Read control
  //
  //  /REN (Read enable)       via chip select line, the data bus has two FIFOs and SRAM
  //  /OE  (Output enable)     tied to /REN
  //
  gpio_configure_pin (AVR32_EBI_NCS_2, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
//gpio_enable_pin_pull_up(AVR32_EBI_NCS_2);

  //  RCLK (Read clock)        Same for both A and B
  //
  gpio_configure_pin (AVR32_EBI_NRD, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
//gpio_enable_pin_pull_up(AVR32_EBI_NRD);

  //  Take FIFO outpf reset
  //
  gpio_set_pin_high (FIFO_A_RESETN);

  //  FIFO B
  //
  //  Configuration input
  //
  //  /MRS  (Master Reset)
  //
  //  All other configurations are hard wired:
  //  /PRS  (Partial Reset)           disable == VCC
  //  /LD   (Offset load)             disable == VCC
  //   FWFT (First Word Fall Through) disable == GND
  //   IW   (Input width)             18 bit  == GND
  //   OW   (Output width)            18 bit  == GND
  //   IP   (Interspersed parity)     Non-IP  == GND
  //   FSEL0                                  == GND
  //   FSEL1                                  == GND
  //  /SEN  (serial mode)             disable == VCC
  //
  gpio_configure_pin (FIFO_B_RESETN, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);

  //  Status output
  //
  //  /EF (Empty Flag)
  //  /HF (Half-Full Flag)
  //  /FF (Full Flag)
  //  /PAE (Programmable Almost Empty Flag) - Not used - Ends at Test Point
  //  /PAF (Programmable Almost Full Flag) - Not used - Ends at Test Point
  //
  gpio_configure_pin     (FIFO_B_EMPTYN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_B_EMPTYN);
  gpio_enable_gpio_pin   (FIFO_B_EMPTYN);

  gpio_configure_pin     (FIFO_B_HFN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_B_NFN);
  gpio_enable_gpio_pin   (FIFO_B_HFN);

  gpio_configure_pin     (FIFO_B_FULLN, GPIO_DIR_INPUT);
//gpio_enable_pin_pull_up(FIFO_B_FULLN);
  gpio_enable_gpio_pin   (FIFO_B_FULLN);

  //  Write control
  //
  //  /WEN (Write enable)          tied to ADC_PD line -- Check ADC initialization!
  //   WCLK (Write clock)          from ADC
  //   D0-D17 (Data lines)         from ADC
  //  

  //  Read control
  //
  //  /REN (Read enable)       via chip select line, the data bus has two FIFOs and SRAM
  //  /OE  (Output enable)     tied to /REN
  //
  gpio_configure_pin (AVR32_EBI_NCS_3, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
//gpio_enable_pin_pull_up (AVR32_EBI_NCS_3);

  //  RCLK (Read clock)        Same for both A and B
  //
  gpio_configure_pin (AVR32_EBI_NRD, GPIO_DIR_OUTPUT | GPIO_INIT_LOW);
//gpio_enable_pin_pull_up (AVR32_EBI_NRD);

  //  Take FIFO outpf reset
  //
  gpio_set_pin_high (FIFO_B_RESETN);


  //WARNING "FIXME - after smc configured properly"
  //
  gpio_configure_pin (AVR32_EBI_NCS_3, GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);

  return SN74V283_OK;
}



int16_t SN74V283_Start (component_selection_t which)
{

  //  Execute Master Reset (MRS) by taking pin briefly low.
  //  Because FWFT/SI is tied to low, SN74V283 enters standard mode.
  //  In standard mode, /EF -> low (isEmpty) and /FF -> high (isFull)
  //
  //  Then, initialize local variables.

  switch  (which)
  {
  case component_A:
       gpio_set_pin_high (FIFO_A_RESETN);
       
       vTaskDelay ((portTickType)TASK_DELAY_MS(2));
       gpio_set_pin_low (FIFO_A_RESETN);
       
       vTaskDelay ((portTickType)TASK_DELAY_MS(2));
       gpio_set_pin_high (FIFO_A_RESETN);
       
       sn74v283_spectra [0] = 0;
       sn74v283_cleared[0] = 0;
       sn74v283_start  [0] = 0;
       sn74v283_count  [0] = 0;

       {
         int rot = 32;
         int cnt = 0;
         int zro = 0;
         do
         {
           U16 v = FIFO_DATA_A[0];
           if (v) cnt++; else zro++;
           rot--;
         }
         while ( rot  ||  (cnt < 32768  &&  !SN74V283_IsEmpty (component_A) ) );
       }

       break;

  case component_B: gpio_set_pin_high(FIFO_B_RESETN);
                    vTaskDelay((portTickType)TASK_DELAY_MS(2));
                    gpio_set_pin_low (FIFO_B_RESETN);
                    vTaskDelay((portTickType)TASK_DELAY_MS(2));
                    gpio_set_pin_high(FIFO_B_RESETN);
                    sn74v283_spectra[1] = 0;
                    sn74v283_cleared[1] = 0;
                    sn74v283_start  [1] = 0;
                    sn74v283_count  [1] = 0;

                    {
                    int rot = 32;
                    int cnt = 0;
                    int zro = 0;
                    do {
                      U16 v = FIFO_DATA_B[0];
                      if (v) cnt++; else zro++;
                      rot--;
                    } while ( rot || ( cnt<32768 && !SN74V283_IsEmpty(component_B) ) );
                    }

                    break;
  default:          //  Code design should make it impossible to get here.
                    //  If this is executed, a coding error has occurred.
                    //  Currently, ignoring.
                    return SN74V283_FAIL;
  }

  return SN74V283_OK;
}

# if 0
int16_t SN74V283_WriteEnable  ( component_selection_t which ) {

//  ALERT: Not available.
//  The WriteEnable line is tied with the ADC Power Down line.

  //  Pin low == Active
  //
  //  In standard mode (as used in HyperNav),
  //  when the FIFO is full, /FF goes to low,
  //  inhibiting further write opertions.
  //  When /FF is low, /WEN is ignored.
  //
  switch ( which ) {
  case sn74v283_A:  gpio_set_pin_low  ( ADC_A_PD ); break;
  case sn74v283_B:  gpio_set_pin_low  ( ADC_B_PD ); break;
  default:          //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            return SN74V283_FAIL;
  }

  return SN74V283_OK;
}

int16_t SN74V283_WriteDisable ( component_selection_t which ) {

//  ALERT: Not available.
//  The WriteEnable line is tied with the ADC Power Down line.

  //  Pin high == Off
  //
  switch ( which ) {
  case sn74v283_A:  gpio_set_pin_high ( ADC_A_PD ); break;
  case sn74v283_B:  gpio_set_pin_high ( ADC_B_PD ); break;
  default:          //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            return SN74V283_FAIL;
  }

  return SN74V283_OK;
}
# endif

# if 0
int16_t SN74V283_ReadEnable  ( component_selection_t which ) {

  //  Pin low == Active
  //
  switch ( which ) {
  case component_A:  gpio_set_pin_low  ( AVR32_EBI_NCS_2 ); break;
  case component_B:  gpio_set_pin_low  ( AVR32_EBI_NCS_3 ); break;
  default:          //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            return SN74V283_FAIL;
  }

  return SN74V283_OK;
}

int16_t SN74V283_ReadDisable ( component_selection_t which ) {

  //  Pin high == Off
  //
  switch ( which ) {
  case component_A:  gpio_set_pin_high ( AVR32_EBI_NCS_2 ); break;
  case component_B:  gpio_set_pin_high ( AVR32_EBI_NCS_3 ); break;
  default:          //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            return SN74V283_FAIL;
  }

  return SN74V283_OK;
}
# endif

int16_t SN74V283_IsEmpty ( component_selection_t which ) {

  //  /EF pin low == Active
  //
  switch ( which ) {
  case component_A:  return gpio_pin_is_low( FIFO_A_EMPTYN ); break;
  case component_B:  return gpio_pin_is_low( FIFO_B_EMPTYN ); break;
  default:
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }

  return 0;
}



int16_t SN74V283_IsHalfFull ( component_selection_t which )
{
  //  /hF pin low == Active
  //
  switch ( which )
  {
  case component_A:  return gpio_pin_is_low( FIFO_A_HFN ); break;
  case component_B:  return gpio_pin_is_low( FIFO_B_HFN ); break;
  default:  
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }

  return 0;
}



int16_t SN74V283_IsFull  ( component_selection_t which ) {

  //  /FF pin low == Active
  //
  switch ( which ) {
  case component_A:  return gpio_pin_is_low( FIFO_A_FULLN ); break;
  case component_B:  return gpio_pin_is_low( FIFO_B_FULLN ); break;
  default:          //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }

  return 0;
}

void SN74V283_AddedSpectrum ( component_selection_t which, Spec_Aux_Data_t *aux )
{
  //  The FIFO does not accept new data when it is full.
  //  In order to maintain control over the data,
  //  prevent writing to FIFO when SN74V283_MAX are in.
  //

  switch  (which)
  {
  case component_A:
    if  (sn74v283_count[0] < SN74V283_MAX)
    {
      int addIdx = ( sn74v283_count[0]+sn74v283_start[0] ) % SN74V283_MAX;
      memcpy (&(sn74v283_aux[0][addIdx]), aux, sizeof (Spec_Aux_Data_t) );
      sn74v283_count  [0] ++;
      sn74v283_spectra[0] ++;
    }
    break;

  case component_B:
    if  (sn74v283_count[1] < SN74V283_MAX)
    {
      int addIdx = (sn74v283_count[1] + sn74v283_start[1] ) % SN74V283_MAX;
      memcpy (&(sn74v283_aux[1][addIdx]), aux, sizeof (Spec_Aux_Data_t));
      sn74v283_count  [1] ++;
      sn74v283_spectra[1] ++;
    }

  default:  
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }
}



uint16_t SN74V283_GetNumOfSpectra (component_selection_t  which)
{
  switch ( which )
  {
  case component_A:  return sn74v283_count[0]; break;
  case component_B:  return sn74v283_count[1]; break;
  default:
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }
  return 0;
}



void SN74V283_ReportNewClearout (component_selection_t  which)
{
  //  The FIFO does not accept new data when it is full.
  //  In order to maintain control over the data,
  //  prevent writing to FIFO when SN74V283_MAX are in.
  //
  switch  (which)
  {
  case component_A:  sn74v283_cleared[0] ++; break;
  case component_B:  sn74v283_cleared[1] ++; break;
  default:          
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }
}



uint16_t SN74V283_GetNumOfCleared (component_selection_t  which)
{
  switch (which)
  {
  case component_A:  return sn74v283_cleared[0]; break;
  case component_B:  return sn74v283_cleared[1]; break;
  default:  
            //  Code design should make it impossible to get here.
            //  If this is executed, a coding error has occurred.
            //  Currently, ignoring.
            break;
  }
  return 0;
}



# if 0
int16_t SN74V283_ReadSpectrum (component_selection_t which, 
                               uint16_t              sram_destination[],
                               int                   sizeofDestination
                              )
{
  if  (sizeofDestination != 4096 + sizeof (Spec_Aux_Data_t))
  {
    return SN74V283_FAIL;
  } 

  if  (which != component_A  &&  which != component_B)
  {
    return SN74V283_FAIL;
  }

  int theIndex;
  switch  (which)
  {
  case component_A:  theIndex = 0; break;
  case component_B:  theIndex = 1; break;
  default: return SN74V283_FAIL;
  }

  if  (sn74v283_count[theIndex] == 0)
  {
    return SN74V283_FAIL;
  }

  //  Get spectrometer data from FIFO
  //
  //  1. Discard 10 channels
  //  2. Copy  2048 channels.
  //  3. Discard 11 channels. (10+one spurious BUSY signal)
  //
  //  SET READ PIN A|B
  //
  int nRead;
  for  (nRead = 0;  nRead < 10;  nRead++)
  {
    // SET RCLK PIN A|B
    // delay a bit
  }

  for  (nRead = 0;  nRead < 2048;  nRead++)
  {
    // SET RCLK PIN A|B
    // delay a bit
    sram_destination[nRead] = 111; //FIFO_out;
  }

  for  (nRead = 0;  nRead < 11;  nRead++)
  {
    // SET RCLK PIN A|B
    // delay a bit
  }

  // UNSET READ PIN A|B

  uint16_t* auxAsWord = (uint16_t*) (sn74v283_aux[theIndex]+sn74v283_start[theIndex]);

  int auxWord;
  for ( auxWord=0; auxWord<sizeof(Spec_Aux_Data_t); auxWord++ ) {
    sram_destination[2048+auxWord] = auxAsWord[auxWord];
  }

  sn74v283_start[theIndex]++;
  sn74v283_start[theIndex]%=SN74V283_MAX;
  sn74v283_count[theIndex]--;

  return SN74V283_OK;
}
# endif
