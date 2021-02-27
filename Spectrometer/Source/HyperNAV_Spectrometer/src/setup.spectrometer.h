/*
 *  File: mode_periodic.h
 *  HyperNav Sensor Firmware
 *  Copyright 2010 Satlantic Inc.
 */

# ifndef MODE_SETUP_H_
# define MODE_SETUP_H_

# include "compiler.h"

# define SETUP_OK		 0
# define SETUP_FAILED	-1

S16 setup_system(void);
S16 characterization(bool what);

# endif
