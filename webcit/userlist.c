/*
 * $Id$
 */

#include "webcit.h"

/* 
 * structure to keep namelists in
 */
struct namelist {
	struct namelist *next; /**< next item of the linked list */
	char name[32];         /**< name of the userentry */
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
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	snprintf(title, sizeof title, _("User list for %s"), serv_info.serv_humannode);
	escputs(title);
	wprintf("</h1>");
        wprintf("</div>");

        wprintf("<div id=\"content\" class=\"service\">\n");

	serv_puts("LIST");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("<em>%s</em><br />\n", &buf[4]);
		goto DONE;
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"userlist_background\"><tr><td>\n");
	wprintf("<tr><th>%s</th><th>%s</th><th>%s</th>"
			"<th>%s</th><th>%s</th><th>%s</th></tr>",
			_("User Name"),
			_("Number"),
			_("Access Level"),
			_("Last Login"),
			_("Total Logins"),
			_("Total Posts"));

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(fl, buf, 0, '|', sizeof fl);
		has_bio = 0;
		for (bptr = bio; bptr != NULL; bptr = bptr->next) {
			if (!strcasecmp(fl, bptr->name))
				has_bio = 1;
		}
		bg = 1 - bg;
		wprintf("<tr bgcolor=\"#%s\"><td>",
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
		wprintf("</td><td>%ld</td><td>%d</td><td>",
			extract_long(buf, 2),
			extract_int(buf, 1));
		lc = extract_long(buf, 3);
		localtime_r(&lc, &tmbuf);
		wprintf("%02d/%02d/%04d ",
			(tmbuf.tm_mon + 1),
			tmbuf.tm_mday,
			(tmbuf.tm_year + 1900));


		wprintf("</td><td>%ld</td><td>%5ld</td></tr>\n",
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
	wprintf("<div id=\"banner\">\n");
	wprintf("<img src=\"static/usermanag_48x.gif\">");
        wprintf("<h1>");
	wprintf(_("User profile"));
        wprintf("</h1>");
        wprintf("</div>");

        wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"userlist_background\"><tr><td>\n");

	serv_printf("OIMG _userpic_|%s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		have_pic = 1;
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
	} else {
		have_pic = 0;
	}

	wprintf("<center><table><tr><td>");
	if (have_pic == 1) {
		wprintf("<img src=\"image&name=_userpic_&parm=");
		urlescputs(who);
		wprintf("\">");
	}
	wprintf("</td><td><h1>");
	escputs(who);
	wprintf("</h1></td></tr></table></center>\n");
	serv_printf("RBIO %s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("JUSTIFY");
	}
	wprintf("<br /><a href=\"display_page?recp=");
	urlescputs(who);
	wprintf("\">"
		"<img src=\"static/citadelchat_24x.gif\" "
		"align=middle border=0>&nbsp;&nbsp;");
	snprintf(buf, sizeof buf, _("Click here to send an instant message to %s"), who);
	escputs(buf);
	wprintf("</a>\n");

	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

void 
InitModule_USERLIST
(void)
{
	WebcitAddUrlHandler(HKEY("userlist"), userlist, 0);
	WebcitAddUrlHandler(HKEY("showuser"), showuser, 0);
}
