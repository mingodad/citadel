/*
 * Administrative screens for floor maintenance
 *
 */


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
#include "webserver.h"




/*
 * Display floor configuration.  If prepend_html is not NULL, its contents
 * will be displayed at the top of the screen.
 */
void display_floorconfig(char *prepend_html)
{
	char buf[SIZ];

	int floornum;
	char floorname[SIZ];
	int refcount;

	output_headers(1, 1, 2, 0, 0, 0, 0);

	if (prepend_html != NULL) {
		client_write(prepend_html, strlen(prepend_html));
	}

	serv_printf("LFLR");	/* FIXME put a real test here */
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("<div id=\"banner\">\n");
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#770000\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">Error</SPAN>\n");
        	wprintf("</TD></TR></TABLE>\n");
		wprintf("</div><div id=\"text\">\n");
        	wprintf("%s<br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	svprintf("BOXTITLE", WCS_STRING, "Floor configuration");
	do_template("beginbox");

	wprintf("<TABLE BORDER=1 WIDTH=100%>\n"
		"<TR><TH>Floor number</TH>"
		"<TH>Floor name</TH>"
		"<TH>Number of rooms</TH></TR>\n"
	);

	while (serv_gets(buf), strcmp(buf, "000")) {
		floornum = extract_int(buf, 0);
		extract(floorname, buf, 1);
		refcount = extract_int(buf, 2);

		wprintf("<TR><TD><TABLE border=0><TR><TD>%d", floornum);
		if (refcount == 0) {
			wprintf("</TD><TD>"
				"<A HREF=\"/delete_floor?floornum=%d\">"
				"<FONT SIZE=-1>(delete floor)</A>"
				"</FONT><br />", floornum
			);
		}
		wprintf("<FONT SIZE=-1>"
			"<A HREF=\"/display_editfloorpic&"
			"which_floor=%d\">(edit graphic)</A>",
			floornum);
		wprintf("</TD></TR></TABLE>");
		wprintf("</TD>");

		wprintf("<TD>"
			"<FORM METHOD=\"POST\" ACTION=\"/rename_floor\">"
			"<INPUT TYPE=\"hidden\" NAME=\"floornum\" "
			"VALUE=\"%d\">"
			"<INPUT TYPE=\"text\" NAME=\"floorname\" "
			"VALUE=\"%s\" MAXLENGTH=\"250\">\n",
			floornum, floorname);
		wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"Change name\">"
			"</FORM></TD>");

		wprintf("<TD>%d</TD></TR>\n", refcount);
	}

	wprintf("<TR><TD>&nbsp;</TD>"
		"<TD><FORM METHOD=\"POST\" ACTION=\"/create_floor\">"
		"<INPUT TYPE=\"text\" NAME=\"floorname\" "
		"MAXLENGTH=\"250\">\n"
		"<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
		"VALUE=\"Create new floor\">"
		"</FORM></TD>"
		"<TD>&nbsp;</TD></TR>\n");

	wprintf("</TABLE>\n");
	do_template("endbox");
	wDumpContent(1);
}



void delete_floor(void) {
	int floornum;
	char buf[SIZ];
	char message[SIZ];

	floornum = atoi(bstr("floornum"));

	serv_printf("KFLR %d|1", floornum);
	serv_gets(buf);

	if (buf[0] == '2') {
		sprintf(message, "<B><I>Floor has been deleted."
				"</I></B><br /><br />\n");
	}
	else {
		sprintf(message, "<B><I>%s</I></B>><br />", &buf[4]);
	}

	display_floorconfig(message);
}


void create_floor(void) {
	char buf[SIZ];
	char message[SIZ];
	char floorname[SIZ];

	strcpy(floorname, bstr("floorname"));

	serv_printf("CFLR %s|1", floorname);
	serv_gets(buf);

	sprintf(message, "<B><I>%s</I></B>><br />", &buf[4]);

	display_floorconfig(message);
}


void rename_floor(void) {
	int floornum;
	char buf[SIZ];
	char message[SIZ];
	char floorname[SIZ];

	floornum = atoi(bstr("floornum"));
	strcpy(floorname, bstr("floorname"));

	serv_printf("EFLR %d|%s", floornum, floorname);
	serv_gets(buf);

	sprintf(message, "<B><I>%s</I></B>><br />", &buf[4]);

	display_floorconfig(message);
}


