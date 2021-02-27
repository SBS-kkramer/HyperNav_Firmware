/*! \file frames.h *******************************************************
 *
 * \brief generate data frames
 *
 *
 * @author      BP, Satlantic LP
 * @date        2015-07-08
 *
  ***************************************************************************/

# ifndef _FRAMES_H_
# define _FRAMES_H_

# include <stdint.h>
# include "spectrometer_data.h"


# define FRAME_OK	 0
# define FRAME_FAIL	-1

int16_t frm_out_or_log (
        Spectrometer_Data_t* hsd,
        uint8_t tlm_frame_subsampling,
        uint8_t log_frame_subsampling );


//  The frames are defined in a doc called SUNA-LC-Frame-Definitions.ods
//
//  Frame elements are grouped into, and included depending on frame type.
//
//      General:    Header, Date, Time
//      Fit:        Nitrate, other species & baselines, RMSE
//      Auxiliary:  Bromide-trace, Absorbances at 254, 350; maybe more.
//      Spec Stats: Spec AVG (obsolete?), Dark Avg (as used in this processing)
//      IntFactor:  make part of Spec Stats
//      Spectrum:   Some/all channles
//      System:     Temperatures, Humidity, Voltages, Current
//      CheckSum:   1-byte check-sum - TODO in the future: Replace by CRC
//      Terminator: "\r\n" [ASCII only]
//
//

int16_t frm_buildFrame (
        uint8_t frame_subsampling,
        uint8_t frameBuffer[], uint16_t bufferSize, uint16_t* filledSize,
        Spectrometer_Data_t* hsd );

# endif /* __FRAMES */
