/*
 * $Id$
 *
 * Displays the "Summary Page"
 */

#include "webcit.h"
#include "calendar.h"

/*
 * Display today's date in a friendly format
 */
void output_date(void) {
	struct tm tm;
	time_t now;
	char buf[128];

	time(&now);
	localtime_r(&now, &tm);

	wc_strftime(buf, 32, "%A, %x", &tm);
	wc_printf("%s", buf);
}




/*
 * Dummy section
 */
void dummy_section(void) {
	svput("BOXTITLE", WCS_STRING, "(dummy&nbsp;section)");
	do_template("beginboxx", NULL);
	wc_printf(_("(nothing)"));
	do_template("endbox", NULL);
}


/*
 * New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";


	number_of_rooms_to_check = num_tokens(rooms_to_check, '|');
	if (number_of_rooms_to_check == 0) return;

	wc_printf("<table border=0 width=100%%>\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract_token(room, rooms_to_check, i, '|', sizeof room);

		serv_printf("GOTO %s", room);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			extract_token(room, &buf[4], 0, '|', sizeof room);
			wc_printf("<tr><td><a href=\"dotgoto?room=");
			urlescputs(room);
			wc_printf("\">");
			escputs(room);
			wc_printf("</a></td><td>%d/%d</td></tr>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wc_printf("</table>\n");

}


/*
 * Task list section
 */
void tasks_section(void) {
	int num_msgs = 0;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wcsession *WCC = WC;
	StrBuf *Buf;
	SharedMessageStatus Stat;

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);

	Buf = NewStrBufPlain(HKEY("_TASKS_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if (WCC->wc_view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
	}

	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			tasks_LoadMsgFromServer(NULL, NULL, Msg, 0, 0);
		}
		DeleteHashPos(&at);
	}

	if (calendar_summary_view() < 1) {
		wc_printf("<i>");
		wc_printf(_("(None)"));
		wc_printf("</i><br />\n");
	}
}


/*
 * Calendar section
 */
void calendar_section(void) {
	char cmd[SIZ];
	int num_msgs = 0;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wcsession *WCC = WC;
	StrBuf *Buf;
	void *v = NULL;
	SharedMessageStatus Stat;

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	
	Buf = NewStrBufPlain(HKEY("_CALENDAR_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if ( (WC->wc_view != VIEW_CALENDAR) && (WC->wc_view != VIEW_CALBRIEF) ) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
	}
	calendar_GetParamsGetServerCall(&Stat, 
					&v,
					readnew, 
					cmd, 
					sizeof(cmd));

	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			calendar_LoadMsgFromServer(NULL, &v, Msg, 0, 0);
		}
		DeleteHashPos(&at);
	}
	if (calendar_summary_view() < 1) {
		wc_printf("<i>");
		wc_printf(_("(Nothing)"));
		wc_printf("</i><br />\n");
	}
	__calendar_Cleanup(&v);
}

/*
 * Server info section (fluff, really)
 */
void server_info_section(void) {
	char message[512];
	wcsession *WCC = WC;

	snprintf(message, sizeof message,
		_("You are connected to %s, running %s with %s, server build %s and located in %s.  Your system administrator is %s."),
		 ChrPtr(WCC->serv_info->serv_humannode),
		 ChrPtr(WCC->serv_info->serv_software),
		 PACKAGE_STRING,
		 ChrPtr(WCC->serv_info->serv_svn_revision),
		 ChrPtr(WCC->serv_info->serv_bbs_city),
		 ChrPtr(WCC->serv_info->serv_sysadm));
	escputs(message);
}

/*
 * Now let's do three columns of crap.  All portals and all groupware
 * clients seem to want to do three columns, so we'll do three
 * columns too.  Conformity is not inherently a virtue, but there are
 * a lot of really shallow people out there, and even though they're
 * not people I consider worthwhile, I still want them to use WebCit.
 */
void summary_inner_div(void) {
	wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<table width=\"100%%\" cellspacing=\"10\" cellpadding=\"0\">"
		"<tr valign=top>");

	/*
	 * Column One
	 */
	wc_printf("<td width=33%%>");
	wc_printf("<div class=\"box\">");	
	wc_printf("<div class=\"boxlabel\">");	
	wc_printf(_("Messages"));
	wc_printf("</div><div class=\"boxcontent\">");	
	wc_printf("<div id=\"msg_inner\">");	
	new_messages_section();
	wc_printf("</div></div></div>");
	wc_printf("</td>");

	/*
	 * Column Two 
	 */
	wc_printf("<td width=33%%>");
	wc_printf("<div class=\"box\">");	
	wc_printf("<div class=\"boxlabel\">");	
	wc_printf(_("Tasks"));
	wc_printf("</div><div class=\"boxcontent\">");	
	wc_printf("<div id=\"tasks_inner\">");	
	tasks_section();
	wc_printf("</div></div></div>");
	wc_printf("</td>");

	/*
	 * Column Three
	 */
	wc_printf("<td width=33%%>");
	wc_printf("<div class=\"box\">");	
	wc_printf("<div class=\"boxlabel\">");	
	wc_printf(_("Today&nbsp;on&nbsp;your&nbsp;calendar"));
	wc_printf("</div><div class=\"boxcontent\">");	
	wc_printf("<div id=\"calendar_inner\">");	
	calendar_section();
	wc_printf("</div></div></div>");
	wc_printf("</td>");

	wc_printf("</tr><tr valign=top>");

	/*
	 * Row Two - Column One
	 */
	wc_printf("<td colspan=2>");
	wc_printf("<div class=\"box\">");	
	wc_printf("<div class=\"boxlabel\">");	
	wc_printf(_("Who's&nbsp;online&nbsp;now"));
	wc_printf("</div><div class=\"boxcontent\">");	
	wc_printf("<div id=\"who_inner\">");	
	do_template("wholistsummarysection", NULL);
	wc_printf("</div></div></div>");
	wc_printf("</td>");

	/*
	 * Row Two - Column Two
	 */
	wc_printf("<td width=33%%>");
	wc_printf("<div class=\"box\">");	
	wc_printf("<div class=\"boxlabel\">");	
	wc_printf(_("About&nbsp;this&nbsp;server"));
	wc_printf("</div><div class=\"boxcontent\">");	
	wc_printf("<div id=\"info_inner\">");	
	server_info_section();
	wc_printf("</div></div></div>");
	wc_printf("</td>");


	/*
	 * End of columns
	 */
	wc_printf("</tr></table>");
}


/*
 * Display this user's summary page
 */
void summary(void) {
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<div class=\"room_banner\">");
        wc_printf("<img src=\"static/summscreen_48x.gif\">");
        wc_printf("<h1>");
        snprintf(title, sizeof title, _("Summary page for %s"), ChrPtr(WC->wc_fullname));
        escputs(title);
        wc_printf("</h1><h2>");
        output_date();
        wc_printf("</h2></div>");
	wc_printf("<div id=\"actiondiv\">");
	wc_printf("<ul class=\"room_actions\">\n");
	wc_printf("<li class=\"start_page\">");
	offer_start_page(NULL, &NoCtx);
        wc_printf("</li></ul>");
        wc_printf("</div>");
        wc_printf("</div>");

	/*
	 * You guessed it ... we're going to refresh using ajax.
	 * In the future we might consider updating individual sections of the summary
	 * instead of the whole thing.
	 */
	wc_printf("<div id=\"content\" class=\"service\">\n");
	summary_inner_div();
	wc_printf("</div>\n");

	wc_printf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('msg_inner', 'new_messages_html',		"
		"                            { method: 'get', frequency: 60 }  );	"
		" new Ajax.PeriodicalUpdater('tasks_inner', 'tasks_inner_html',		"
		"                            { method: 'get', frequency: 120 }  );	"
		" new Ajax.PeriodicalUpdater('calendar_inner', 'calendar_inner_html',		"
		"                            { method: 'get', frequency: 90 }  );	"
		" new Ajax.PeriodicalUpdater('do_template', 'template=wholistsummarysection',	"
		"                            { method: 'get', frequency: 30 }  );	"
		"</script>							 	\n"
	);

	wDumpContent(1);
}

void 
InitModule_SUMMARY
(void)
{
	WebcitAddUrlHandler(HKEY("new_messages_html"), "", 0, new_messages_section, AJAX);
	WebcitAddUrlHandler(HKEY("tasks_inner_html"), "", 0, tasks_section, AJAX);
	WebcitAddUrlHandler(HKEY("calendar_inner_html"), "", 0, calendar_section, AJAX);
	WebcitAddUrlHandler(HKEY("mini_calendar"), "", 0, ajax_mini_calendar, AJAX);
	WebcitAddUrlHandler(HKEY("summary"), "", 0, summary, 0);
	WebcitAddUrlHandler(HKEY("summary_inner_div"), "", 0, summary_inner_div, AJAX);
}

