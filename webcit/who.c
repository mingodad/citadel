/* $Id$ */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

struct whouser {
	struct whouser *next;
	int sessionnum;
	char username[256];
	char roomname[256];
	char hostname[256];
	char clientsoftware[256];
	};
	
/*
 * who is on?
 */
void whobbs(void) {
	struct whouser *wlist = NULL;
	struct whouser *wptr = NULL;
	char buf[256],sess,user[256],room[256],host[256];
	int foundit;

        printf("HTTP/1.0 200 OK\n");
        output_headers(1);

        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>Users currently on ");
	escputs(serv_info.serv_humannode);
        wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER><TABLE border><TR>");
	wprintf("<TH>Session ID</TH><TH>User Name</TH><TH>Room</TH>");
	wprintf("<TH>From host</TH></TR>\n");
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0]=='1') {
		while(serv_gets(buf), strcmp(buf,"000")) {
			sess = extract_int(buf, 0);
			extract(user, buf, 1);
			extract(room, buf, 2);
			extract(host, buf, 3);

			foundit = 0;
			for (wptr = wlist; wptr != NULL; wptr = wptr -> next) {
				if (wptr->sessionnum == sess) {
					foundit = 1;
					if (strcasecmp(user, wptr->username)) {
						sprintf(buf, "%cBR%c%s", 
							LB, RB, user);
						strcat(wptr->username, buf);
						}
					if (strcasecmp(room, wptr->roomname)) {
						sprintf(buf, "%cBR%c%s", 
							LB, RB, room);
						strcat(wptr->roomname, buf);
						}
					if (strcasecmp(host, wptr->hostname)) {
						sprintf(buf, "%cBR%c%s", 
							LB, RB, host);
						strcat(wptr->hostname, buf);
						}
					}
				}

			if (foundit == 0) {
				wptr = (struct whouser *)
					malloc(sizeof(struct whouser));
				wptr->next = wlist;
				wlist = wptr;
				strcpy(wlist->username, user);
				strcpy(wlist->roomname, room);
				strcpy(wlist->hostname, host);
				wlist->sessionnum = sess;
				}
			}

		while (wlist != NULL) {
			wprintf("<TR><TD>%d</TD><TD>", wlist->sessionnum);
			escputs(wlist->username);
			wprintf("</TD><TD>");
			escputs(wlist->roomname);
			wprintf("</TD><TD>");
			escputs(wlist->hostname);
			wprintf("</TD></TR>\n");
			wptr = wlist->next;
			free(wlist);
			wlist = wptr;
			}
		}
	wprintf("</TABLE></CENTER>\n");
        wprintf("</BODY></HTML>\n");
        wDumpContent();
	}


