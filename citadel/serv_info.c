/*
 * serv_info.c
 *
 * The CtdlGetServerInfo() function is useful for 
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

void CtdlInternalGetServInfo(struct CtdlServInfo *infobuf) {
	char buf[256];
	int a;

	/* fetch info */	
	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0]!='1') return;

	a = 0;
	while(serv_gets(buf), strcmp(buf,"000")) {
	    switch(a) {
		case 0:		infobuf->serv_pid = atoi(buf);
				break;
		case 1:		strcpy(infobuf->serv_nodename,buf);
				break;
		case 2:		strcpy(infobuf->serv_humannode,buf);
				break;
		case 3:		strcpy(infobuf->serv_fqdn,buf);
				break;
		case 4:		strcpy(infobuf->serv_software,buf);
				break;
		case 5:		infobuf->serv_rev_level = atoi(buf);
				break;
		case 6:		strcpy(infobuf->serv_bbs_city,buf);
				break;
		case 7:		strcpy(infobuf->serv_sysadm,buf);
				break;
		case 9:		strcpy(infobuf->serv_moreprompt,buf);
				break;
		case 10:	infobuf->serv_ok_floors = atoi(buf);
				break;
		}
	    ++a;
	    }

	}

