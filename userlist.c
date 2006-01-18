/*
 * $Id$
 *
 * Display a list of all accounts on a Citadel system.
 *
 */

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
	char title[256];
	struct tm tmbuf;
	time_t lc;
	struct namelist *bio = NULL;
	struct namelist *bptr;
	int has_bio;
	int bg = 0;

	serv_puts("LBIO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1')
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			bptr = (struct namelist *) malloc(sizeof(struct namelist));
			bptr->next = bio;
			strcpy(bptr->name, buf);
			bio = bptr;
		}
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	snprintf(title, sizeof title, _("User list for %s"), serv_info.serv_humannode);
	escputs(title);
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	serv_puts("LIST");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("<EM>%s</EM><br />\n", &buf[4]);
		goto DONE;
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");
	wprintf(_("<TR><TH>User Name</TH><TH>Number</TH><TH>Access Level</TH>"
		"<TH>Last Login</TH><TH>Total Logins</TH><TH>Total Posts</TH></TR>"));

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(fl, buf, 0, '|', sizeof fl);
		has_bio = 0;
		for (bptr = bio; bptr != NULL; bptr = bptr->next) {
			if (!strcasecmp(fl, bptr->name))
				has_bio = 1;
		}
		bg = 1 - bg;
		wprintf("<TR BGCOLOR=\"#%s\"><TD>",
			(bg ? "DDDDDD" : "FFFFFF")
		);
		if (has_bio) {
			wprintf("<a href=\"showuser&who=");
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
		localtime_r(&lc, &tmbuf);
		wprintf("%02d/%02d/%04d ",
			(tmbuf.tm_mon + 1),
			tmbuf.tm_mday,
			(tmbuf.tm_year + 1900));


		wprintf("</TD><TD>%ld</TD><TD>%5ld</TD></TR>\n",
			extract_long(buf, 4), extract_long(buf, 5));

	}
	wprintf("</table></div>\n");
DONE:	wDumpContent(1);
}


/*
 * Display (non confidential) information about a particular user
 */
void showuser(void)
{
	char who[256];
	char buf[256];
	int have_pic;

	strcpy(who, bstr("who"));

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR>"
		"<TD><img src=\"static/usermanag_48x.gif\"></TD>"
		"<td align=left><SPAN CLASS=\"titlebar\">");
	wprintf(_("User profile"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	serv_printf("OIMG _userpic_|%s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		have_pic = 1;
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
	} else {
		have_pic = 0;
	}

	wprintf("<CENTER><TABLE><TR><TD>");
	if (have_pic == 1) {
		wprintf("<img src=\"image&name=_userpic_&parm=");
		urlescputs(who);
		wprintf("\">");
	}
	wprintf("</TD><TD><H1>%s</H1></TD></TR></TABLE></CENTER>\n", who);
	serv_printf("RBIO %s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("JUSTIFY");
	}
	wprintf("<br /><a href=\"display_page?recp=");
	urlescputs(who);
	wprintf("\">"
		"<img src=\"static/citadelchat_24x.gif\" "
		"ALIGN=MIDDLE BORDER=0>&nbsp;&nbsp;");
	snprintf(buf, sizeof buf, _("Click here to send an instant message to %s"), who);
	escputs(buf);
	wprintf("</A>\n");

	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}
