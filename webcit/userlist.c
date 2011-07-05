
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
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<h1>");
	snprintf(title, sizeof title, _("User list for %s"), ChrPtr(WC->serv_info->serv_humannode));
	escputs(title);
	wc_printf("</h1>");
        wc_printf("</div>");

        wc_printf("<div id=\"content\" class=\"service\">\n");

	serv_puts("LIST");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wc_printf("<em>%s</em><br>\n", &buf[4]);
		goto DONE;
	}

	wc_printf("<table class=\"userlist_background\"><tr><td>\n");
	wc_printf("<tr><th>%s</th><th>%s</th><th>%s</th>"
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
		wc_printf("<tr bgcolor=\"#%s\"><td>",
			(bg ? "DDDDDD" : "FFFFFF")
		);
		if (has_bio) {
			wc_printf("<a href=\"showuser?who=");
			urlescputs(fl);
			wc_printf("\">");
			escputs(fl);
			wc_printf("</A>");
		} else {
			escputs(fl);
		}
		wc_printf("</td><td>%ld</td><td>%d</td><td>",
			extract_long(buf, 2),
			extract_int(buf, 1));
		lc = extract_long(buf, 3);
		localtime_r(&lc, &tmbuf);
		wc_printf("%02d/%02d/%04d ",
			(tmbuf.tm_mon + 1),
			tmbuf.tm_mday,
			(tmbuf.tm_year + 1900));


		wc_printf("</td><td>%ld</td><td>%5ld</td></tr>\n",
			extract_long(buf, 4), extract_long(buf, 5));

	}
	wc_printf("</table>\n");
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
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<img src=\"static/webcit_icons/essen/32x32/account.png\" alt\"\">");
        wc_printf("<h1>");
	wc_printf(_("User profile"));
        wc_printf("</h1>");
	wc_printf("<div id=\"navbar\">\n");
	wc_printf("<ul><li><a href=\"display_page?recp=");
	urlescputs(who);
        wc_printf("\">"
                "<img src=\"static/webcit_icons/essen/16x16/chat.png\" alt=\"\">"
		"<span class=\"navbar_link\">");
        snprintf(buf, sizeof buf, _("Click here to send an instant message to %s"), who);
        escputs(buf);
        wc_printf("</span></li></a>\n");
	wc_printf("</div>");
        wc_printf("</div>");

        wc_printf("<div id=\"content\" class=\"service bio\">\n");

	wc_printf("<table class=\"userlist_background\"><tr><td>\n");

	serv_printf("OIMG _userpic_|%s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		have_pic = 1;
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
	} else {
		have_pic = 0;
	}

	wc_printf("<center><table><tr><td>");
	if (have_pic == 1) {
		wc_printf("<img src=\"image?name=_userpic_&amp;parm=");
		urlescputs(who);
		wc_printf("\" alt=\"\">");
	}
	wc_printf("</td><td><h1>");
	escputs(who);
	wc_printf("</h1></td></tr></table></center>\n");
	serv_printf("RBIO %s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("JUSTIFY");
	}
	wc_printf("</td></tr></table>\n");
	wDumpContent(1);
}

void
InitModule_USERLIST
(void)
{
	WebcitAddUrlHandler(HKEY("userlist"), "", 0, userlist, 0);
	WebcitAddUrlHandler(HKEY("showuser"), "", 0, showuser, 0);
}
