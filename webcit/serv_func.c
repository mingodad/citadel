#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "webcit.h"

struct serv_info serv_info;

/*
 * get info about the server we've connected to
 */
void get_serv_info() {
	char buf[256];
	int a;

	serv_printf("IDEN %d|%d|%d|%s|%s",
		DEVELOPER_ID,
		CLIENT_ID,
		CLIENT_VERSION,
		SERVER,
		""		/* FIX find out where the user is */
		);
	serv_gets(buf);

	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0]!='1') return;

	a = 0;
	while(serv_gets(buf), strcmp(buf,"000")) {
	    switch(a) {
		case 0:		serv_info.serv_pid = atoi(buf);
				break;
		case 1:		strcpy(serv_info.serv_nodename,buf);
				break;
		case 2:		strcpy(serv_info.serv_humannode,buf);
				break;
		case 3:		strcpy(serv_info.serv_fqdn,buf);
				break;
		case 4:		strcpy(serv_info.serv_software,buf);
				break;
		case 5:		serv_info.serv_rev_level = atoi(buf);
				break;
		case 6:		strcpy(serv_info.serv_bbs_city,buf);
				break;
		case 7:		strcpy(serv_info.serv_sysadm,buf);
				break;
		case 9:		strcpy(serv_info.serv_moreprompt,buf);
				break;
		}
	    ++a;
	    }
	}

