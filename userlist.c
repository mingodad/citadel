#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"




struct namelist {
	struct namelist *next;
	char name[32];
};

/*
 * display the userlist
 */
void userlist(void)
{
	char buf[256];
	char fl[256];
	struct tm *tmbuf;
	long lc;
	struct namelist *bio = NULL;
	struct namelist *bptr;
	int has_bio;

	serv_puts("LBIO");
	serv_gets(buf);
	if (buf[0] == '1')
		while (serv_gets(buf), strcmp(buf, "000")) {
			bptr = (struct namelist *) malloc(sizeof(struct namelist));
			bptr->next = bio;
			strcpy(bptr->name, buf);
			bio = bptr;
		}
	output_headers(1);

	serv_puts("LIST");
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		goto DONE;
	}
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>User list for ");
	escputs(serv_info.serv_humannode);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER><TABLE border>");
	wprintf("<TR><TH>User Name</TH><TH>Number</TH><TH>Access Level</TH>");
	wprintf("<TH>Last Call</TH><TH>Total Calls</TH><TH>Total Posts</TH></TR>\n");

	while (serv_gets(buf), strcmp(buf, "000")) {
		extract(fl, buf, 0);
		has_bio = 0;
		for (bptr = bio; bptr != NULL; bptr = bptr->next) {
			if (!strcasecmp(fl, bptr->name))
				has_bio = 1;
		}
		wprintf("<TR><TD>");
		if (has_bio) {
			wprintf("<A HREF=\"/showuser&who=");
			urlescputs(fl);
			wprintf("\">");
			escputs(fl);
			wprintf("</A>");
		} else {
			escputs(fl);
		}
		wprintf("</TD><TD>%ld</TD><TD>%d</TD><TD>",
			extract_long(buf, 2),
			extract_int(buf, 1));
		lc = extract_long(buf, 3);
		tmbuf = (struct tm *) localtime(&lc);
		wprintf("%02d/%02d/%04d ",
			(tmbuf->tm_mon + 1),
			tmbuf->tm_mday,
			(tmbuf->tm_year + 1900));


		wprintf("</TD><TD>%ld</TD><TD>%5ld</TD></TR>\n",
			extract_long(buf, 4), extract_long(buf, 5));

	}
	wprintf("</TABLE></CENTER>\n");
      DONE:wDumpContent(1);
}


/*
 * Display (non confidential) information about a particular user
 */
void showuser(void)
{
	char who[256];
	char buf[256];
	int have_pic;

	output_headers(1);


	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>User profile");
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	strcpy(who, bstr("who"));
	serv_printf("OIMG _userpic_|%s", who);
	serv_gets(buf);
	if (buf[0] == '2') {
		have_pic = 1;
		serv_puts("CLOS");
		serv_gets(buf);
	} else {
		have_pic = 0;
	}

	wprintf("<CENTER><TABLE><TR><TD>");
	if (have_pic == 1) {
		wprintf("<IMG SRC=\"/image&name=_userpic_&parm=");
		urlescputs(who);
		wprintf("\">");
	}
	wprintf("</TD><TD><H1>%s</H1></TD></TR></TABLE></CENTER>\n", who);
	serv_printf("RBIO %s", who);
	serv_gets(buf);
	if (buf[0] == '1')
		fmout(NULL);
	wDumpContent(1);
}
