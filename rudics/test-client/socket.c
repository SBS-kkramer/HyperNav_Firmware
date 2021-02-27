#ifndef SOCKET_H
#define SOCKET_H

#include "serial.h"

int OpenRudicsPort(const char *hostname);
int CloseRudicsPort(void);

extern struct SerialPort tcp;
extern time_t RefTime;

#endif /* SOCKET_H */

#include "logger.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

time_t itimer(void);

static int cd(void) { return 1;}
static int na(void) { return -1;}
static int na_(int i) {return -1;}

static int iflush(void);
static int oflush(void);
static int ioflush(void);
static int getb(unsigned char *byte);
static int putb(unsigned char byte);

struct SerialPort tcp={getb,putb,iflush,ioflush,oflush,na,cd,na_,na,na_,na};

static int sock=-1;
time_t RefTime=0L;
time_t itimer(void) {return (time_t)((RefTime)?difftime(time(NULL),RefTime):-1);}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*------------------------------------------------------------------------*/
int iflush(void)
{
   int status=0; unsigned char byte;
   
   while (1) {

      int err=tcp.getb(&byte);

      if (err<0 && errno==EWOULDBLOCK) {

         status=1;
         break;

      } else if (!err) {

         status=0;
         break;
      }
   }

   return status;
}

int ioflush(void) {
   return iflush();
}

int oflush(void) {
   return 1;
}
 
/*------------------------------------------------------------------------*/
/*                                                                        */
/*------------------------------------------------------------------------*/
int CloseRudicsPort(void)
{
   int status=-1;
   
   if (sock>=0) status=shutdown(sock,SHUT_RDWR);

   return status;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*------------------------------------------------------------------------*/
int getb(unsigned char *byte)
{
   /* define logentry signature */
   static cc FuncName[] = "tcp.getb()";

   int status=-1;
   
   if (!byte) {

      LogEntry(FuncName,"NULL function argument.\n");

   } else if (sock<0) {

      LogEntry(FuncName,"Invalid RUDICS port.\n");

   } else if ((status=read(sock,byte,1))<0 && errno!=EWOULDBLOCK) {

      LogEntry(FuncName,"Error[%d]: %s\n",errno,strerror(errno));
   }

   return status;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*------------------------------------------------------------------------*/
int putb(unsigned char byte)
{
   /* define logentry signature */
   static cc FuncName[] = "tcp.putb()";

   int status=-1;
   
   if (sock<0) {

      LogEntry(FuncName,"Invalid RUDICS port.\n");
   
   } else if ((status=write(sock,&byte,1))<0) {

      LogEntry(FuncName,"Error[%d]: %s\n",status,strerror(errno));
   }

   return status;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*------------------------------------------------------------------------*/
int OpenRudicsPort(const char *hostname)
{
   /* define logentry signature */
   static cc FuncName[] = "OpenRudicsPort()";

   const char *service="rudics", *protocol="tcp";

   int status = 0;

   struct hostent *host;
   
   if (!hostname) {

      LogEntry(FuncName,"Invalid hostname.\n"); status=-1;

   } else if (!(host=gethostbyname(hostname))) {

      LogEntry(FuncName,"Hostname not found: %s\n",hostname);

   } else {

      int flags,err;
# if FFF
      const struct servent *serv=getservbyname(service,protocol);
# else
      int serv = 37999;
# endif
      struct sockaddr_in addr; memset(&addr,0,sizeof(addr));
      
      addr.sin_family=AF_INET;
      memcpy(&addr.sin_addr,host->h_addr_list[0],host->h_length);

# if FFF
      if (serv) addr.sin_port=serv->s_port;
# else
                addr.sin_port=serv;
# endif

      if (!serv) {

         LogEntry(FuncName,"/etc/services entry: %s/%s not found.\n",service,protocol);

      } else if (!(sock=socket(AF_INET,SOCK_STREAM,0))) {

         LogEntry(FuncName,"Attempt to open socket failed.\n");

      } else if ((err=connect(sock,(struct sockaddr *)&addr,sizeof(addr)))<0) {

         LogEntry(FuncName,"Socket connection failed: err=%d\n",err);

      } else if ((flags=fcntl(sock,F_GETFL,0))<0) {

         LogEntry(FuncName,"Attempt to get socket configuration failed.\n");

      } else {

         flags |= O_NONBLOCK;

         if (fcntl(sock,F_SETFL,flags)<0) {

            LogEntry(FuncName,"Attempt to set nonblocking IO on socket failed.\n");

         } else {

            status=1;
         }
      }
   }
   
   return status;
}
