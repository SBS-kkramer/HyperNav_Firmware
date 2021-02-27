# include "network.h"

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <time.h>
# include <sys/time.h>
# include <unistd.h>

# include "syslog.h"
# include "modem.h"

# ifdef FW_SIMULATION
# include <unistd.h>
# define portTickType useconds_t
# define TASK_DELAY_MS(M) (M)
# define vTaskDelay(X) usleep(X*1000)
# endif

static uint8_t ntw_is_connected = 0;

int16_t NTW_IsConnected() {
  return ntw_is_connected;
}

/*------------------------------------------------------------------------*/
/* function to get modem attention (power cycle when failing initially    */
/*------------------------------------------------------------------------*/
/**
   This function powers-up the iridium modem and ensures that it will
   respond to AT commands.  It was created because experience shows that
   under some circumstances the LBT must be power-cycled before it will
   respond to commands.  On success (ie., the modem responds to AT
   commands), then this function returns a positive value.  On failure (ie.,
   the modem fails to respond to AT commands), then this function returns
   zero.
*/
static int16_t NTW_Attention() {

  int i,j,status = -1;

  /* fault-tolerance loop to get the LBT9522's attention */
  for( status=0,i=1; !status && i<=4; i++) {

    /* allow for autobaud detection */
    for( j=0; j<3; j++) {
      MDM_putb('\r');
      vTaskDelay( (portTickType)(TASK_DELAY_MS(100)) );
    }

    for( j=0; j<3; j++) {
      /* get the LBT9522's attention */
      if( MDM_chat("AT","OK",2,"\r") > 0 ) {
        status=1;
        break;
      }
    }

    /* cycle power if modem does not respond */
    if( !status && i<4) {
      MDM_Disable();
      sleep(i*15);
      MDM_Enable(19200);
    }
  }
   
  return status;
}


/*------------------------------------*/
/* function to search for the network */
/*------------------------------------*/
/**
   This function implements a method for ensuring that the network can be reached.
   The modem is powered up and an attempt is made to register with the network.

   The power is cycled up, down, and then back up again in order to
   work around a bug in the Sebring and Daytona LBTs.  After being powered
   down for a long time (days), most units will not respond to AT commands
   on the first power-up.  However, the modem will respond if the LBT is
   powered down for 2 seconds and then powered up a second time.
   
   This function returns a positive value if the network is reached. (ie.,
   the LBT is able to register).  Zero is returned if the registration
   attempt failed.
*/

int16_t NTW_Search()
{
  /* enable the modem serial port */
  MDM_Enable(19200);

  /* use modem registration as proof of sky visibility */
  int16_t status = NTW_Register(); 
       
  /* deactivate the modem serial port */
  MDM_Disable();
   
  return status;
}

int16_t NTW_ConnectionFailed() {

  return (1==MDM_cd()) ? 0 : -1;
}


/*------------------------------------------------------------------------*/
/* function to register the modem with the iridium system                 */
/*------------------------------------------------------------------------*/
/**
   This function attempts to register the LBT with the iridium system.  On
   success, this function returns a positive value.  Zero is returned if the
   registration attempt failed.  A negative return value indicates an
   invalid serial port.
*/

int16_t NTW_Register() {

  static const char FuncName[] = "IrModemRegister";

  //  Start communicating with modem
  //
  if( (status=NTW_Attention()) <=0 ) {

    syslog_out( SYSLOG_ERROR, FuncName, "Modem not responding." );
    return -2;
  } 
  
  //  Check if the modem is an iridium modem
  //
  if( MDM_chat("AT I4","IRIDIUM",10,"\r") <= 0 ) {

    syslog_out( SYSLOG_ERROR, FuncName, "Modem is not an Iridium LBT." );
    return -3;
  }

  //  Initialize parameters of the timeout mechanism
  //
  const time_t TimeOut=120, To = time(NULL);

  /* reinitialize the return value */
  int16_t registration_status=0;

  do {

      MDM_IOFlush();                  /* flush the serial port buffers */

      /*
       *  Request the LBT9522's registration status
       *  Response will be: +CREG: <n>,<stat>[,<lac>,<ci>]
       *     <n> 0  disabled
       *         1  enabled
       *         2  enabled with location information (???)
       *  <stat> 0  not registered
       *         1      registered, home network
       *         2  not registered, searching to register
       *         3  denied registration
       *         4  unknown
       *         5  registered, roaming
       */
      if( MDM_puts("AT+CREG?",2,"\r") >0 ) {

        char buf[32];

        while (MDM_gets(buf,sizeof(buf)-1,2,"\r\n")>0) {               /* read the next line from the serial port */

//             syslog_out(SYSLOG_DEBUG,FuncName,"Received: %s",buf);

	       char* p;
               if ((p=strstr(buf,"+CREG:")) && (p=strchr(p,',')))      /* search for the second field of the expected response string */
               {
                  int s = atoi(++p);                                   /* get the value of the second field */

                  if( s==1 || s==5 ) {                                 /* check for registration */
			  registration_status=1;
			  break;
		  }
               }
        }

      }

      sleep(5);                                                        /* pause before retry */

  } while (!registration_status && difftime(time(NULL),To)<TimeOut);   /* check termination criteria */

  return registration_status ? 0 : -4;
}

int16_t NTW_Connect( char const* dialString,
                     char const* username,
                     char const* password ) {

  const int modemCommandTimeout =  5;  //  seconds
  const int         dialTimeout = 20;  //  seconds
  const int        loginTimeout = 20;  //  seconds

  //  If the connection was already established,
  //  do nothing.
  //
  if ( NTW_IsConnected() ) return 1;

  //  Toggle DRT line (calls include proper delays)
  //
  MDM_DTR_clear();
  MDM_DTR_assert();

  //  Restore modem to factory configuration
  //
  char const* factoryReset = "AT&F0";
  if ( MDM_chat( factoryReset, "OK", modemCommandTimeout, "\r" ) <= 0 ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Modem factory reset '%s' failed.", factoryReset );
    return -1;
  }

  //  Initialize modem
  //
  //    &C1          DCD Option: 1 DCD indicates the connection status
  //    &D2          DTR Option
  //
  //                 DTR must be ON during on-hook command mode.
  //                 If DTR transitions from ON to OFF during onhook command mode,
  //                 operation will be locked after approximately 10 seconds.
  //                 On-hook command mode operation will resume when DTR is restored ON.
  //
  //                 DTR must be ON at call connection
  //
  //                 DTR must be ON during both in-call command mode and in-call data mode.
  //                 Reaction to DTR ON to OFF transitions during in-call command mode
  //                 and in-call data mode is determined by the &Dn setting as shown below.
  //                 Note that the +CVHU command can be set to override these specified reactions.
  //
  //                   1  If DTR transitions from ON to OFF during in-call command mode,
  //                      and DTR is restored ON within approximately 10 seconds,
  //                      the call will remain up. If DTR is not restored ON
  //                      within approximately 10 seconds, the call will drop to on-hook
  //                      command mode.
  //
  //                      If DTR transitions from ON to OFF during in-call data mode,
  //                      the mode will change to incall command mode. If DTR is restored
  //                      ON within approximately 10 seconds, the call will remain up.
  //                      If DTR is not restored ON within approximately 10 seconds,
  //                      the call will drop to on-hook command mode.
  //
  //                   2  If DTR transitions from ON to OFF during either in-call command
  //                      mode or in-call data mode, the call will drop to on-hook command
  //                      mode (default).
  //
  //                   3  If DTR transitions from ON to OFF during either in-call command
  //                      mode or in-call data mode, the call will drop to on-hook command
  //                      mode and the ISU will reset to AT command profile 0.
  //
  //    &K0          Flow Control           0 == Disable flow control
  //    &R1          RTS/CTS Option         Compatibility mode, no action taken
  //    &S1          DSR Override         0,1 == DSR always active (default) [1 same as 0]
  //    E1           Echo                   1 == Characters are echoed to the DTE
  //    Q0           Quiet                  0 == ISU responses are sent to thte DTE
  //    S0=0         Auto Answer            0 == Disable auto-answer
  //    S7=45        S7: Communication Standard used by ISU -- No Action, compatibility
  //    S10=100      S10: Carrier loss time                 -- No Action, compatibility
  //    V1           Verbose Mode           1 == Textual responses
  //    X4           Extended Result Codes  4 == Responses sent by ISU to DTE:
  //                                             OK, CONNECT, RING, NO CARRIER, NO ANSWER, ERROR
  //                                             CONNECT x  [x==DTE speed]
  //                                             NO DIALTONE
  //                                             BUSY
  //                                             CARRIER x, PROTOCOL, COMPRESSION  [x==line speed]
  //
  char const* modemInit = "AT &C1 &D2 &K0 &R1 &S1 E1 Q0 S0=0 S7=45 S10=100 V1 X4";
  if ( MDM_chat( modemInit, "OK", modemCommandTimeout, "\r" ) <= 0 ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Modem initialization '%s' failed.", modemInit );
    return -1;
  }

  //  Assert IRIDIUM service
  //
  if ( MDM_chat( "AT I4", "IRIDIUM", modemCommandTimeout, "\r" ) <= 0 ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Modem service check '%s' failed.", "AT I4" );
    return -1;
  }

  //  Original code: Navis Firmware modem.c :: m9500cbst()
  //  Purpose: Select the Bearer Service Type to be 4800 baud on the
  //  remote computer.  I [Dana Swift]  have found this command to be necessary in order for
  //  logins to be successful.  Motorola's ISU AT Command Reference
  //  (SSP-ISU-CPSW-USER-005 Version 1.3) was used as the guide document.
  //
  //  Command syntax: +CBST=[<speed>[,<name>[,<ce>]]]
  //   speed: 6  4800 bps V.32
  //   name : 0  data circuit asynchronous [only permitted value]
  //   ce   : 1  non-transparent           [only permitted value]
  //
  char const* cbstCommand = "AT +CBST=6,0,1";
  if ( MDM_chat( cbstCommand, "OK", modemCommandTimeout, "\r" ) <= 0 ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Modem remote configuration '%s' failed.", cbstCommand );
    return -1;
  }


  //  Dial into the network
  //
  if ( MDM_chat( dialString, "CONNECT", dialTimeout, "\r" ) <= 0 ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Modem dial failed." );
    return -1;
  }

  //  Make sure modem is responsive ???
  //
  if ( !MDM_cd() ) return -1;

  //
  //  At this point, the iridium connection is established.
  //  All further commands are interpreted on the other side
  //  of the rudics daemon.
  //

  char UserName[32]; snprintf( UserName, sizeof(UserName)-1, "%s\r", username );
  char PassWord[32]; snprintf( PassWord, sizeof(PassWord)-1, "%s\r", password );

  //  Wait for "login:" prompt, then send UserName
  //
  if ( MDM_expect ( "login:", UserName, loginTimeout, "\n" ) ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Login prompt not received." );
    return -1;
  }

  //  Wait for "Password:" prompt, then send PassWord
  //
  if ( MDM_expect ( "Password:", PassWord, loginTimeout, "\n" ) ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Password prompt not received." );
    return -1;
  }

  //  Wait for "Last login:" message, as proof of successful login
  //
  if ( MDM_expect ( "Last login:", "", loginTimeout, "\n" ) ) {

    syslog_out( SYSLOG_ERROR, "NTW_Connect", "Login failed." );
    return -1;
  }

  //  Host is running C-Shell (csh) compatible shell.
  //  Change default prompt to "cmd>"
  //
  //  retry 3 times
  //
  //  This is non-critical;
  //  failure to change the prompt is not a connection failure
  //
  int prompt_changed = 0;
  int retries = 3;
  do {

    if ( !MDM_cd() ) return -1;

    if ( MDM_chat ( "set prompt = \"cmd>\"\n", "cmd>", loginTimeout, "\n" ) <= 0 ) {

      syslog_out( SYSLOG_ERROR, "NTW_Connect", "Change prompt failed." );
      retries--;

    } else {

      prompt_changed = 1;
    }

  } while ( !prompt_changed && retries>0 );

  ntw_is_connected = 1;

  return 0;
}

int16_t NTW_Disconnect() {

  if ( !ntw_is_connected ) return 1;

//  TODO !!!

  ntw_is_connected = 0;

  return 0;
}
