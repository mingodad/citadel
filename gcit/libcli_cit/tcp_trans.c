/*

Citadel TCP/IP transport layer
Brian Costello
btx@calyx.net

*/

#include "config.h"
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "client_api.h"
#include "transport.h"

int citadel_connect(char *host, u_short port)
{
   int sock, ret;
   struct sockaddr_in sin;
   struct hostent *he;
 
   if ((he = gethostbyname(host)) == NULL)
      return -1;

   if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
   {
      perror("Citadel_connect");
      return sock;
   }
   
   sin.sin_family = AF_INET;
   sin.sin_port = htons(port);
   memcpy(&sin.sin_addr, he->h_addr, he->h_length);
   
   ret = connect(sock, &sin, sizeof(sin));
   
   if (ret < 0)
   {
      perror("Citadel_connect - connect()");
      return(ret);
   }
   return(sock);
}

int citadel_disconnect(int sd)
{
   int n;
   
   if ((n=close(sd))<0)
      return(n);
   return(0);
}

int citadel_send(int sd, char *buf, int buflen)
{
   int n;
   
   if (buflen < 1)
      return(0);
   
   n = write(sd, buf, buflen);
   
#ifdef DEBUG
   printf("Wrote: %s\n", buf);
#endif 

   return(n);
}

int citadel_recv_line(int sd, char *buf, int buflen)
{
   int cl = 0;
   int n;
   
   if (!buf)
   {
      fprintf(stderr, "Error: BUF in citadel_recv is NULL!\n");
      return -1;
   }

#ifdef DEBUG
   fprintf(stderr,"Receiving in citadel_recv_line...\n");
#endif   

   do
   {
      n = read(sd, &buf[cl], 1);
      if (n)
      {
         if (buf[cl] == '\n')
            buf[cl] = '\0';
         cl++;
      }
   } while ((n) && (cl < buflen) && (buf[cl-1]));
   
   if (cl == buflen)
      buf[buflen-1] = '\0';
   
   if (!n)
      buf[cl] = '\0';

#ifdef DEBUG
      printf("Received: %s\n", buf);
#endif
   return(cl);
   
}

int citadel_recv(int sd, char *buf, int buflen)
{
   int n;
   
   if (!buf)
   {
      fprintf(stderr, "Error: BUF in citadel_recv is NULL!\n");
      return -1;
   }

#ifdef DEBUG
   fprintf(stderr,"Receiving...\n");
#endif   
   n = read(sd, buf, buflen);
#ifdef DEBUG
   buf[n] = '\0';
   printf("Received: %s\n", buf);
#endif 
   return(n);
}

