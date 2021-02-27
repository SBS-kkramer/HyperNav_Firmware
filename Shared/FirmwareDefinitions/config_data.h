/*! \file config_data.h
 *
 *  \brief Define the data structure
 *         for configuration data exchange.
 *
 *  @author BP, Satlantic
 *  @date   2016-01-29
 *
 ***************************************************************************/

# ifndef   _CONFIG_DATA_H_
# define   _CONFIG_DATA_H_

# include <compiler.h>
# include <stdint.h>

typedef struct Config_Data {

  enum { CD_Empty, CD_Full, CD_AccCal, CD_MagCal } content;

  uint32_t id;

  F32     acc_mounting;
  int16_t acc_x;
  int16_t acc_y;
  int16_t acc_z;

  int16_t mag_min_x;
  int16_t mag_max_x;
  int16_t mag_min_y;
  int16_t mag_max_y;
  int16_t mag_min_z;
  int16_t mag_max_z;

  F64   gps_lati;
  F64   gps_long;
  F64   magdecli;

  F64   digiqzu0;
  F64   digiqzy1;
  F64   digiqzy2;
  F64   digiqzy3;
  F64   digiqzc1;
  F64   digiqzc2;
  F64   digiqzc3;
  F64   digiqzd1;
  F64   digiqzd2;
  F64   digiqzt1;
  F64   digiqzt2;
  F64   digiqzt3;
  F64   digiqzt4;
  F64   digiqzt5;

  int16_t dqtempdv;
  int16_t dqpresdv;

  int16_t simdepth;
  int16_t simascnt;

  F32   puppival;
  F32   puppstrt;
  F32   pmidival;
  F32   pmidstrt;
  F32   plowival;
  F32   plowstrt;

  uint16_t frmprtsn;
  uint16_t frmsbdsn;

  uint16_t saturcnt;
  uint8_t  numclear;

} Config_Data_t;

# endif // _CONFIG_DATA_H_

