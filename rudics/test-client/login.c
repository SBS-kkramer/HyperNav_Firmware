#ifndef LOGIN_H
#define LOGIN_H

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * $Id: //Firmware/Radiometers/Hyperspectral/Source/HyperNAV/rudics/test-client/login.c#1 $
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
/** RCS log of revisions to the C source code.
 *
 * \begin{verbatim}
 * $Log: login.c,v $
 * Revision 1.1  2004/12/29 23:11:27  swift
 * Modified LogEntry() to use strings stored in the CODE segment.  This saves
 * lots of space in the DATA segment and significantly speeds code start-up.
 *
 * \end{verbatim}
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define loginChangeLog "$RCSfile: login.c,v $  $Revision: #1 $   $Date: 2016/01/20 $"

#include "serial.h"

/* function prototypes for external functions */
int login(const struct SerialPort *port, const char *user, const char *pwd);
int logout(const struct SerialPort *port);

#endif /* LOGIN_H */

#include <ctype.h>
#include <string.h>
#include "chat.h"
#include "expect.h"
#include "logger.h"

/* define the prompt to set on the host computer */
#define CMD "cmd>"

/* function prototypes */
unsigned int sleep(unsigned int seconds);
int snprintf(char *str, size_t size, const char *format, ...);

/*------------------------------------------------------------------------*/
/* function to log into a remote computer via the serial port             */
/*------------------------------------------------------------------------*/
/**
   This function logs into a remote computer connected via a serial port.
   It reads from the serial port searching for the log-in and password
   prompts.  Each prompt is responded to with a user-specified response
   strings.

      \begin{verbatim}
      input:
         
         port...A structure that contains pointers to machine dependent
                primitive IO functions.  See the comment section of the
                SerialPort structure for details.  The function checks to be
                sure this pointer is not NULL.

         user...The string transmitted when responding to the login prompt.

         pwd....The string transmitted when responding to the password prompt.

      output:

         This function returns a positive number if the login was
         successful.  Zero is returned if the login failed.  A negative
         number is returned if the function arguments are determined to be
         ill-defined. 
      
      \end{verbatim}
*/
int login(const struct SerialPort *port,const char *user, const char *pwd)
{
   /* function name for log entries */
   static cc FuncName[] = "login()";
   
   /* initialize the return value of this function */
   int status = -1;
  
   if (!port) {                                             /* validate the serialport */

      LogEntry(Funcname,"Serial port not ready.\n");

   } else if (!user) {                                      /* verify user name */

      LofEntry(FuncName,"NULL pointer to the user name.\n";

   } else if (!pwd) {                                       /* verify the password */

      LogEntry(FuncName,"NULL pointer to the password.\n";

   } else if (!port->putb) {                                /* validate the serial port's putb() function */

      LogEntry(FuncName,"NULL putb() function for serial port.\n");

   } else {

      const time_t timeout=30;                              /* specify timeout period (sec) */
      const char *login_prompt="login:";                    /* specify the login prompt */
      const char *pwd_prompt="Password:";                   /* specify the password prompt */
      
      status=0;                                             /* re-initialize the function's return value */

      if ((status=pflushio(port))<=0) {                     /* flush the IO buffers as an initialization step */

         LogEntry(FuncName,"Attempt to flush IO buffers failed.\n");

      } else {

         int i;

         char User[32]; snprintf(User,sizeof(User)-1,"%s\r",user);	/*  Copy & terminate user name  */
         char Pwd [32]; snprintf(Pwd ,sizeof(Pwd )-1,"%s\r",pwd );      /*  Copy & terminate password   */
         
         for (status=0, i=0; status<=0 && i<3; i++) {                   /* 3 attempts to login before aborting */

            if (port->cd && !port->cd()) {                              /* check if carrier-dectect enabled and CD line not asserted */

               LogEntry(FuncName,"No carrier detected.\n");
               break;
            }
 
            if (port->putb('\n')<=0) {                                  /* write line-feed to serial port to initiate the login sequence */

               LogEntry(FuncName,"Attempt to write to serial port failed.\n");

            } else if (expect(port,login_prompt,User,timeout,"\n")<=0) {/* look for login prompt & enter user name */

               LogEntry(FuncName,"Login prompt not received.\n");

            } else if (expect(port,pwd_prompt,Pwd,timeout,"\n")<=0) {   /* look for the password prompt */

               LogEntry(FuncName,"Password prompt not received.\n");

            } else if (expect(port,"Last login:","",timeout,"\n")<=0) { /* verify that the login was successful */

               LogEntry(FuncName,"Login failed.\n");

            } else {

               LogEntry(FuncName,"Login successful.\n");
               status=1;                                                 /* indicate success via return value*/
            }
         }

         for (i=0; status>0 && i<3; i++) {                               /* 3 attempts to reset the command prompt */

            if (port->cd && !port->cd()) {                              /* check if carrier-dectect enabled and CD line not asserted */

               LogEntry(FuncName,"No carrier detected.\n");
               status=0; break;                                         /* indicate failure */
            }

            if (chat(port,"set prompt = \"" CMD "\"\n",CMD,5,"\n")>0) {

               sleep(1); break;

            } else {

               LogEntry(FuncName,"Attempt to set the command prompt failed.\n");

            }
         }
      }
   }
   
   return status;
}

/*------------------------------------------------------------------------*/
/* function to logout of the remote computer                              */
/*------------------------------------------------------------------------*/
/**
   This function logs out of the remote computer attached to the serial
   port.  Unfortunately, there is no way to verify that the logout was
   actually successful.

      \begin{verbatim}
      input:
               
         port...A structure that contains pointers to machine dependent
                primitive IO functions.  See the comment section of the
                SerialPort structure for details.  The function checks to be
                sure this pointer is not NULL.

      output:

         This function returns a positive number if successful.  Zero is
         returned if the attempt failed.  A negative number is returned if
         the function argument was determined to be ill-defined.
      
      \end{verbatim}
*/
int logout(const struct SerialPort *port)
{
   /* function name for log entries */
   static cc FuncName[] = "logout()";

   /* initialize return value */
   int status = -1;
   
   /* validate the serialport */
   if (!port)
   {
      /* create the message */
      static cc msg[]="Serial port not ready.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }

   /* validate the serial port's pputb() function */
   else if (!port->putb)
   {
      /* create the message */
      static cc msg[]="NULL pputb() function for serial port.\n";

      /* make the logentry */
      LogEntry(FuncName,msg);
   }
   
   /* write the logout command */
   else
   {
      int i;

      /* define the timeout period (sec) */
      const time_t timeout=5;

      for (status=0,i=0; i<3; i++)
      {
         /* check if carrier-dectect enabled and CD line not asserted */
         if (port->cd && !port->cd())
         {
            /* create the message */
            static cc msg[]="No carrier detected.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);

            /* indicate failure */
            status=0; break;
         }
         
         if (chat(port,"\003\003\003\003\003\r",CMD,timeout,"\n")<=0)
         {
            /* create the message */
            static cc msg[]="Can't get command prompt.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);
         }
         
         if (chat(port,"exit\r","logout",timeout,"\n")<=0)
         {
            /* create the message */
            static cc msg[]="Attempt to log-out failed.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);
         }
         else
         {
            /* create the message */
            static cc msg[]="Log-out successful.\n";

            /* make the logentry */
            LogEntry(FuncName,msg);

            /* indicate success */
            status=1; sleep(1); break;
         }
      }
   }
   
   return status;
}
