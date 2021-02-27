# ifndef _PM_PROFILE_ACQUIRE_H_
# define _PM_PROFILE_ACQUIRE_H_

# include <unistd.h>
# include <stdint.h>

int profile_acquire ( const char* data_dir, uint16_t profile_ID );

# endif
