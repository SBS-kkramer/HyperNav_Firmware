#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdarg.h>
#include <unistd.h>

extern "C" {
#include "socket.h"
#include "logger.h"
#include "login.h"
#include "upload.h"
#include "download.h"
}

/*========================================================================*/
/*                                                                        */
/*========================================================================*/
int main(int argc,char *argv[])
{
   // const char *hostname="127.0.0.1";
   //const char *hostname="192.168.2.7";
   const char *hostname="52.27.58.207";

   if (argc<=1) {

      printf("usage: %s file1 file2 ...\n",argv[0]);
      exit(0);
   }
   
   // set-up logging facilities
   debuglevel=0xffff; MaxLogSize=LONG_MAX-CHAR_MAX; RefTime=time(NULL); LogOpenAuto(NULL);
   
   if (OpenRudicsPort(hostname)>0) {

      if (login(&tcp,"hnv-test","hnv")>0) {

         if (RxConfig(&tcp)<=0) {

            LogEntry("main()","RxConfig() failed.\n");

         } else for (int i=1; i<argc; i++) {

            sleep(2);

            if (UpLoadFile(&tcp,argv[i],argv[i])<=0) {

               LogEntry("main()","UpLoadFile failed: %s\n",argv[i]);
               break;
            }
         }
      }
      
      logout(&tcp);
   }

   LogClose(); CloseRudicsPort();
   
   return 0;
}
