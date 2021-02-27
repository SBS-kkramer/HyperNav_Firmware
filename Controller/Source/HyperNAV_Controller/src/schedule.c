/*
 *  File: schedule.c
 *  Isus Firmware version 2
 *  Copyright 2004 Satlantic Inc.
 */

/*
 *  All functions related to interpreting the schedule file,
 *  and finding the next scheduled event.
 *
 *  2004-Jul-30 BP: Initial version
 */


# include "schedule.h"

# if 0

# define SCHED_FILE "schedule.txt"

# include <ctype.h>
# include <limits.h>
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <time.h>

/*
 *  Driver APIs
 */

# include "prtc.h"

/*
 *  ISUS firmware
 */

# include "config.h"
# include "extern.h"
# include "isusutil.h"
//# include "measure.h"
# include "numerical.h"
# include "spectrometer.h"


# include "flashutil.h"
# include "io.h"
# include "settings.h"

# endif

# if 0


typedef struct _line_info {

	time_t	sec_of_day;
    short	action   [ MAX_DEVS ];
    time_t	duration;

} line_info;

static char* sched_eventCode ( SCHED_event* event ) {

    static char code [MAX_DEVS+1];
    short  dev;

    for ( dev=0; dev<MAX_DEVS; dev++ ) {

        switch ( event->action[dev] ) {
            case SCHED_ACTION_POWER_OFF: code [dev] = 'v'; break;
            case SCHED_ACTION_POWER_ON:  code [dev] = '^'; break;
            case SCHED_ACTION_ACQUIRE:   code [dev] = 'a'; break;
            default:                     code [dev] = ' '; break;
        }
    }
    
    code [MAX_PORTS] = 0;

    return code;
}

static void sched_makeLineEvent ( time_t req_time, ulong req_sec_of_day, line_info* info, SCHED_event* event ) {

# ifdef SCH_DBG
# undef SCH_DBG
# endif

    short port;
  # ifdef SCH_DBG
    char  tmp [128];
    char  msg [128];
    
    sprintf ( tmp, "Make event from sec-of-day %lu ", info->sec_of_day );
  # endif
    
    event->time = ( req_time + info->sec_of_day ) - req_sec_of_day;

    if ( req_sec_of_day > info->sec_of_day ) {
        event->time += 24L*3600L;
    }

    for ( port=0; port<MAX_PORTS; port++ ) {
        event->action[port] = info->action[port];
      # ifdef SCH_DBG
        switch ( info->action[port] ) {
            case ACTION_POWER_OFF: strcat ( tmp, "v" ); break;
            case ACTION_POWER_ON:  strcat ( tmp, "^" ); break;
            case ACTION_ACQUIRE:   strcat ( tmp, "a" ); break;
            default:               strcat ( tmp, " " ); break;
        }
      # endif
        event->duration[port] = info->duration;
    }
    
  # ifdef SCH_DBG
    sprintf ( msg, "%s for %ld sec @ %ld", tmp, info->duration, event->time );
    LogMessage ( msg, true );
  # endif
}

static void sched_makeDefaultEvent ( ulong req_time, SCHED_event* event ) {

    short port;
    
    event->time = 3600 * ( 1 + req_time % 3600 );

    for ( port=0; port<MAX_PORTS; port++ ) {
        event->action [port] = SCHED_ACTION_POWER_OFF;
        event->duration[port] = 300;
    }

}

static int sched_strncasecmp ( const char* s1, const char* s2, int len ) {

    int i;
    
    for ( i=0; i<len; i++ ) {
    
        int diff = toupper ( s1[i] )
                 - toupper ( s2[i] );
        
        if ( diff ) {
            return diff;
        }
    }
    
    return 0;
}

static bool sched_parseCommand ( char* cmd, line_info* info ) {

    char* rem = cmd;
    /*
     *  First find out what to do:
     *  POWER
     *  ACQUIRE DURATION
     */

    short port;
    SCHED_action action = SCHED_ACTION_NOTHING;
    bool some_port_configured = false;  /*  events for only un-configured ports are ignored  */

    if ( 0 == sched_strncasecmp ( "POWER", rem, 5 ) ) {

        action = SCHED_ACTION_POWER;
        info->duration = 0;

        while ( *rem != ' '
             && *rem != '\t'
             && *rem != 0 ) {
            rem++;
        }

        while ( ( *rem == ' '
               || *rem == '\t' )
             &&   *rem != 0 ) {
            rem++;
        }

        if ( *rem == 0 ) {
            return false;
        }

    } else if ( 0 == sched_strncasecmp ( "ACQUIRE", rem, 7 ) ) {

        action = SCHED_ACTION_ACQUIRE;

        while ( *rem != ' '
             && *rem != '\t'
             && *rem != 0 ) {
            rem++;
        }

        while ( ( *rem == ' '
               || *rem == '\t' )
             &&   *rem != 0 ) {
            rem++;
        }

        if ( *rem == 0 ) {
            return false;
        }

        if ( 1 != sscanf ( rem, "%lu", &(info->duration) ) ) {
            return false;
        }

        while ( *rem != ' '
             && *rem != '\t'
             && *rem != 0 ) {
            rem++;
        }

        while ( ( *rem == ' '
               || *rem == '\t' )
             &&   *rem != 0 ) {
            rem++;
        }

        if ( *rem == 0 ) {
            return false;
        }

    } else {
        return false;
    }

    /*
     *  Second, find out where the command applies to.
     *  We expect a white-space or tab separated list,
     *  where the elements are port numbers or internal
     *  devices (ISUS only for now) (+/- prepended in case
     *  of the power command)
     *
     *  Thus, scan the rest of the line for tokens.
     */

    for ( port=0; port<MAX_PORTS; port++ ) {
        info->action [port] = SCHED_ACTION_NOTHING;
    }
    
    some_port_configured = false;

    do {
        char* begin_of_token = rem;
        char  was_end;
        
        short this_action = SCHED_ACTION_NOTHING;
        
        /*
         *  Skip to first space character or EOL (end of token)
         */
        
        while ( *rem != ' '
             && *rem != '\t'
             && *rem != 0 ) {
            rem++;
        }
        
        /*
         *  End the current token
         */

        was_end = *rem;
        *rem = 0;

        /*
         *  Make sure there is something left
         */

        if ( 0 == strlen ( begin_of_token ) ) {
            return false;
        }

        /*
         *  See if there are +/- chars
         */
        
        if ( action == SCHED_ACTION_POWER ) {
            if ( *begin_of_token == '+' ) {
                this_action = SCHED_ACTION_POWER_ON;
            } else if ( *begin_of_token == '-' ) {
                this_action = SCHED_ACTION_POWER_OFF;
            } else {
                char msg [128];
                sprintf ( msg, "%s command without '+' or '-' in schedule file ignored", cmd );
                LogMessage ( msg, true );
            }
            begin_of_token++;
        } else {
            this_action = action;
        }

        /*
         *  Determine the port# of this token
         */
        
        switch ( *begin_of_token ) {
        
            /* Serial ports */
            case '1': if ( IsPortConfigured(1) ) { 
                          info->action [1] = this_action;
                          some_port_configured = true;
                      }
                      break;
            case '2': if ( IsPortConfigured(2) ) {
                          info->action [2] = this_action;
                          some_port_configured = true;
                      }
                      break;
            case '3': if ( IsPortConfigured(3) ) {
                          info->action [3] = this_action;
                          some_port_configured = true;
                      }
                      break;
            case '4': if ( IsPortConfigured(4) ) {
                          info->action [4] = this_action;
                          some_port_configured = true;
                      }
                      break;
            case '5': if ( IsPortConfigured(5) ) {
                          info->action [5] = this_action;
                          some_port_configured = true;
                      }
                      break;
            case '6': if ( IsPortConfigured(6) ) {
                          info->action [6] = this_action;
                          some_port_configured = true;
                      }
                      break;
            
            /* Named ports */
            default:  if ( 0 == strncmp ( "ISUS", begin_of_token, 4 ) ) {
                          info->action [PORT_ISUS] = this_action;
                          some_port_configured = true;
                      } else {
                          char msg [128];
                          sprintf ( msg, "Unknown port '%s' in schedule file", begin_of_token );
                          LogMessage ( msg, true );
                      }
        }

        /*
         *  Find start of next token
         */

        *rem = was_end;
        
        while ( ( *rem == ' '
               || *rem == '\t' ) 
             &&   *rem != 0 ) {
            rem++;
        }

    } while ( *rem /*  the string is nul-terminated  */ );
    
    return some_port_configured;
}

static bool sched_parseEventLine ( char* line, line_info* info ) {
    /*
     *  A line is of the form
     *  HH:MM:SS  Remainder
     */

    char* command = line;
    short hh;
    short mm;
    short ss;
    
    /*
     *  Get the first token, and terminate it with a '\0'-char
     */

    while ( *command != ' '
         && *command != '\t'
         && *command != 0 ) {
        command++;
    }
  
    if ( 0 == *command ) {
        return false;
    }

    *command = 0;
    command ++;
    
    /*
     *  Go to the start of the next token
     */

    while ( ( *command == ' '
           || *command == '\t' )
         &&   *command != 0 ) {
        command++;
    }
    
    if ( 0 == *command ) {
        return false;
    }

    /*
     *  Scan the first token, and parse the rest of the line
     */

    if ( 3 == sscanf ( line, "%hd:%hd:%hd", &hh, &mm, &ss ) ) {
        //  Ignore lines where any time value is out of bound
        if ( hh < 0  || mm < 0  || ss < 0
          || hh > 23 || mm > 59 || ss > 59 ) {
            return false;
        }
        if ( sched_parseCommand ( command, info ) ) {
            info->sec_of_day = (long)ss + 60L*(long)mm + 3600L*(long)hh;
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}


static int sched_stripComments (

    char str[] ) {

    char* tmp1;
# if 0
    char* tmp2;
# endif

    /*
     *  If the string contains a comment mark,
     *  end the string there.
     */

    tmp1 = strchr ( str, '#' );

    if ( tmp1 ) {
        *tmp1 = 0;
    }

# if 0
    /*
     *  Push non-spaces to the left
     */

    tmp1 = tmp2 = str;

    while ( *tmp1 ) {

        if ( !isspace ( *tmp1) ) {

            *tmp2 = *tmp1;
            tmp2++;
        }

        tmp1++;

    }

    *tmp2 = 0;
# endif

    return (int) strlen ( str );
}



static void sched_readNextEvent ( FILE* fp, ulong req_after, SCHED_event* next_event ) {

    /*
     *  Go line-by-line through the file.
     *  If the line is valid, compare time of line with requested time.
     *  Use the line if it refers to a future time.
     */
     
    char msg[128];
    
    bool dbg = GetDebugFlag();

    line_info first_line;
    bool   have_first = false;

    line_info try_line;
    bool   have_event = false;

	struct tm* req_tm = gmtime ( &req_after );
    ulong  req_sec_of_day = 3600L * (long)req_tm->tm_hour
                          +   60L * (long)req_tm->tm_min
                          +         (long)req_tm->tm_sec;

    do {
        char  line [128];
        short llen;
        
        do {
            fgets ( line, (int)(sizeof(line)-2), fp );
        } while ( line && line[0] && 0 == sched_stripComments(line) );

        llen = strlen ( line );
        while ( llen > 0 && ( '\n' == line [llen-1] || '\r' == line [llen-1] ) ) {
            line [llen-1] = 0;
        }

        strupr ( line );
        
        if ( sched_parseEventLine ( line, &try_line ) ) {

            /*
             *  Schedule file works on a daily schedule.
             *  If requested time is past last event,
             *  we will use this (the first) event,
             *  which will be executed on the following day.
             */

            if ( !have_first ) {
                memcpy ( &first_line, &try_line, sizeof(line_info) );
                have_first = true;
            }

            /*
             *  Current line refers to the future
             *  Break the loop. Use this entry.
             */

            if ( try_line.sec_of_day > req_sec_of_day ) {
                have_event = true;
            }
        }

    } while ( !have_event && !feof ( fp ) );

    /*
     *  Either we found something,
     *  or we read the whole file finding nothing
     */

    if ( have_event ) {
        sched_makeLineEvent ( req_after, req_sec_of_day, &try_line, next_event );
        if ( dbg ) {
            sprintf ( msg, "Found event '%s' in schedule file.", sched_eventCode(next_event) );
            LogMessage ( msg, true );
        }
    } else {
        if ( have_first ) {
            sched_makeLineEvent ( req_after, req_sec_of_day, &first_line, next_event );
            if ( dbg ) {
                sprintf ( msg, "Found first event '%s' in schedule file.", sched_eventCode(next_event) );
                LogMessage ( msg, true );
            }
        } else {
            sched_makeDefaultEvent ( req_after, next_event );
            sprintf ( msg, "Found no event in schedule file. Using default '%s'.", sched_eventCode(next_event) );
            LogError ( msg );
        }
    }

    return;
}


void SCHED_retrieveNextEvent ( ulong req_time, SCHED_event* event ) {

    sch_file = safe_fopen ( sch_file, FOLDER_CONFIG FOLDER_SEPARATOR SCHED_FILE, "r" );

    if ( !sch_file ) {

        LogError ( "Schedule file not found. Turning off all instruments." );
        sched_makeDefaultEvent ( req_time, event );

    } else {

        sched_readNextEvent ( sch_file, req_time, event );

        safe_fclose ( sch_file );
        sch_file = NULL;
    }

    return;
}

# endif
