/*
 *  File: schedule.h
 *  Isus Firmware version 2
 *  Copyright 2004 Satlantic Inc.
 */

/*
 *  All functions related to interpreting the schedule file,
 *  and finding the next scheduled event.
 *
 *  2004-Jul-30 BP: Initial version
 */

# ifndef _SCHEDULE_H_
# define _SCHEDULE_H_

# if 0
# include <time.h>
# include "config.h"

typedef enum {
    SCHED_ACTION_POWER            = -1,  /*  only a temporary value  */
    SCHED_ACTION_NOTHING          =  0,
    SCHED_ACTION_POWER_ON         =  1,
    SCHED_ACTION_POWER_OFF        =  2,
    SCHED_ACTION_POWER_ON_ACQUIRE =  3,
    SCHED_ACTION_ACQUIRE          =  4
 # if 0
    SCHED_ACTION_SEND             5
    SCHED_ACTION_RECEIVE          6
 # endif
} SCHED_action;

typedef struct {

    /*  hour, minute, second are the time-of-day values
     *  of the RTC immediately after the event has
     *  been accepted.
     *  time is calculated from the RTC-derived hh:mm:ss
     *  via gmtime(), to get the easier to handle value
     *  (seconds since 1970).
     */

    time_t			start_time;
    time_t			duration [ MAX_DEVS ];
    SCHED_action	action   [ MAX_DEVS ];

} SCHED_event;

typedef enum {
    SCHED_STATE_OFF = 0,
    SCHED_STATE_ON  = 1,
    SCHED_STATE_ACQ = 2
} SCHED_state;

typedef struct _instrument_state {

    time_t		end_time [ MAX_DEVS ];
    SCHED_state	state    [ MAX_DEVS ];

} SCHED_instrument_state;

void SCHED_retrieveNextEvent ( time_t req_time, SCHED_event* event );

# endif

# endif
