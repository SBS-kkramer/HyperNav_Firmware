/*
 *  File: gplp_trace.spectrometer.h
 *  HyperNav Firmware
 *  Copyright 2010 Satlantic Inc.
 */

#ifndef _GPLP_TRACE_SPECTROMETER_H_
#define _GPLP_TRACE_SPECTROMETER_H_

# include "power_clocks_lib.h"

# if 0
# define  FLAG_CMS   0x00000001
# define   Run_CMS() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_CMS; pcl_write_gplp ( 0, gplp0 ); }
# define Sleep_CMS() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_CMS; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ   0x00000010
# define   Run_DAQ() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ; pcl_write_gplp ( 0, gplp0 ); }
# define Sleep_DAQ() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_TX   0x00010000
# define Enter_DAQ_TX() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_TX; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_TX() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_TX; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_AM   0x00020000
# define Enter_DAQ_AM() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_AM; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_AM() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_AM; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_PR   0x00040000
# define Enter_DAQ_PR() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_PR; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_PR() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_PR; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_SP0   0x00080000
# define Enter_DAQ_SP0() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_SP0; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_SP0() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_SP0; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_SP1   0x00100000
# define Enter_DAQ_SP1() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_SP1; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_SP1() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_SP1; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_MX   0x00200000
# define Enter_DAQ_MX() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_MX; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_MX() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_MX; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DAQ_PCK   0x00400000
# define Enter_DAQ_PCK() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DAQ_PCK; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_DAQ_PCK() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DAQ_PCK; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_DXS   0x00000100
# define   Run_DXS() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_DXS; pcl_write_gplp ( 0, gplp0 ); }
# define Sleep_DXS() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_DXS; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_PPM   0x00001000
# define   Run_PPM() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_PPM; pcl_write_gplp ( 0, gplp0 ); }
# define Sleep_PPM() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_PPM; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_lsm303_get_accel_raw   0x04000000
# define Enter_lsm303_get_accel_raw() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_foo; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_lsm303_get_accel_raw() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_foo; pcl_write_gplp ( 0, gplp0 ); }
 
# define  FLAG_foo   0x80000000
# define Enter_foo() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 |=  FLAG_foo; pcl_write_gplp ( 0, gplp0 ); }
# define  Exit_foo() { uint32_t gplp0 = pcl_read_gplp(0); gplp0 &= ~FLAG_foo; pcl_write_gplp ( 0, gplp0 ); }
 
# endif

#endif /* _GPLP_TRACE_SPECTROMETER_H_ */
