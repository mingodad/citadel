/* $Id$ */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "citadel.h"
#include "ipc.h"
#include "tools.h"

void logoff(int code)
{
	exit(code);
	}

int main(int argc, char **argv)
{
	char buf[256];
	char nodetitle[256];
	int a;
	int s_pid = 0;
	int my_pid = 0;
	char s_user[256];
	char s_room[256];
	char s_host[256];

	attach_to_server(argc,argv);
	serv_gets(buf);
	if ((buf[0]!='2')&&(strncmp(buf,"551",3))) {
		fprintf(stderr,"%s: %s\n",argv[0],&buf[4]);
		logoff(atoi(buf));
		}
	strcpy(nodetitle, "this BBS");
	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0]=='1') {
		a = 0;
		while (serv_gets(buf), strcmp(buf,"000")) {
			if (a==0) my_pid = atoi(buf);
			if (a==2) strcpy(nodetitle, buf);
			++a;
			}
		}
	printf("            Users currently logged on to %s\n", nodetitle);
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0]!='1') {
		fprintf(stderr,"%s: %s\n",argv[0],&buf[4]);
		logoff(atoi(buf));
		}

	printf("Session         User name               Room                  From host\n");
	printf("------- ------------------------- -------------------- ------------------------\n");
	while (serv_gets(buf), strcmp(buf,"000")) {
		s_pid = extract_int(buf,0);
		extract(s_user,buf,1);
		extract(s_room,buf,2);
		extract(s_host,buf,3);
		if (s_pid != my_pid) {
			printf("%-7d%c%-25s %-20s %-24s\n",
				s_pid,
				((s_pid == my_pid) ? '*' : ' '),
				s_user,s_room,s_host);
			}
		}

	serv_puts("QUIT");
	serv_gets(buf);
	return 0;
	}


#ifndef HAVE_STRERROR
/*
 * replacement strerror() for systems that don't have it
 */
char *strerror(int e)
{
	static char buf[32];

	sprintf(buf,"errno = %d",e);
	return(buf);
	}
#endif
