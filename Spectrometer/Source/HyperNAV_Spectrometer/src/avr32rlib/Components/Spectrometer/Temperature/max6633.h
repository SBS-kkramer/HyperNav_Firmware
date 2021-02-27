/*
 * max6633.h
 *
 * Created: 5/19/2017 12:40:47 PM
 *  Author: bplache
 */ 


#ifndef MAX6633_H_
#define MAX6633_H_

# include <stdint.h>

# define MAX6633_OK 0
# define MAX6633_ERR -1

# define MAX6633_ERR_SETUP_MUX -2
# define MAX6633_ERR_WRITE_CFG -3
# define MAX6633_ERR_WRITE_TEM -4
# define MAX6633_ERR_READ_TEM  -5

int max6633_measure ( uint8_t twi_mux_channel, float* T_C /* temperature in C */ );

#endif /* MAX6633_H_ */
