/*
 * $Id$
 * 
 * 5.80 to 5.90 migration utility for network files
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "citadel.h"
#include "ipc.h"
#include "tools.h"

extern struct config config;

void logoff(int code)
{
	exit(code);
}

int main(int argc, char **argv)
{
	char buf[SIZ];
	char hostbuf[SIZ];
	char portbuf[SIZ];

	printf("\n\n\n\n\n"
"This utility migrates your network settings (room sharing with other\n"
"Citadel systems) from 5.80 to 5.90.  You should only do this ONCE.  It\n"
"will ERASE your 5.80 configuration files when it is finished, and it will\n"
"ERASE any 5.90 configuration files that you have already set up.\n\n"
"Are you sure you want to do this? ");

	gets(buf);
	if (tolower(buf[0]) != 'y') exit(0);

	get_config();

	attach_to_server(argc, argv, hostbuf, portbuf);
	serv_gets(buf);
	printf("%s\n", &buf[4]);
	if ((buf[0]!='2')&&(strncmp(buf,"551",3))) {
		fprintf(stderr,"%s: %s\n",argv[0],&buf[4]);
		logoff(atoi(buf));
	}

	printf("%s\n", buf);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", &buf[4]);
	if (buf[0] != '2') {
		exit(2);
	}


	printf("FIXME not finished yet\n");
}
