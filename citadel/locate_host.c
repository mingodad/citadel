/*
 * locate the originating host
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include "sysdep.h"
#include "citadel.h"
#include "server.h"
#include "locate_host.h"
#include "config.h"

void locate_host(char *tbuf)
{
	struct sockaddr_in      cs;     
	struct hostent      *   ch;        
	int                     len;     
	char *i;
	int a1,a2,a3,a4;
       
    len = sizeof(cs);   
    if (getpeername(CC->client_socket, (struct sockaddr *)&cs,&len) < 0){   
	strcpy(tbuf,config.c_fqdn);
	return;
	}
     
    if((ch = gethostbyaddr((char *) &cs.sin_addr, sizeof(cs.sin_addr),  
	  AF_INET)) == NULL) { 
		i = (char *) &cs.sin_addr;
		a1 = ((*i++)&0xff);
		a2 = ((*i++)&0xff);
		a3 = ((*i++)&0xff);
		a4 = ((*i++)&0xff);
		sprintf(tbuf,"%d.%d.%d.%d",a1,a2,a3,a4);
		return;
		}

	strncpy(tbuf,ch->h_name, 24);
	tbuf[24] = 0;
	}
