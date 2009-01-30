/*
 * $Id$
 *
 * Displays the "Summary Page"
 */

#include "webcit.h"

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
	wprintf("%s", buf);
}




/*
 * Dummy section
 */
void dummy_section(void) {
	svput("BOXTITLE", WCS_STRING, "(dummy&nbsp;section)");
	do_template("beginboxx", NULL);
	wprintf(_("(nothing)"));
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

	wprintf("<table border=0 width=100%%>\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract_token(room, rooms_to_check, i, '|', sizeof room);

		serv_printf("GOTO %s", room);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			extract_token(room, &buf[4], 0, '|', sizeof room);
			wprintf("<tr><td><a href=\"dotgoto?room=");
			urlescputs(room);
			wprintf("\">");
			escputs(room);
			wprintf("</a></td><td>%d/%d</td></tr>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wprintf("</table>\n");

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

	Buf = NewStrBufPlain(HKEY("_TASKS_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if (WCC->wc_view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			display_task(Msg, 0);
		}
	}

	if (calendar_summary_view() < 1) {
		wprintf("<i>");
		wprintf(_("(None)"));
		wprintf("</i><br />\n");
	}
}


/*
 * Calendar section
 */
void calendar_section(void) {
	int num_msgs = 0;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wcsession *WCC = WC;
	struct calview c;
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("_CALENDAR_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if ( (WC->wc_view != VIEW_CALENDAR) && (WC->wc_view != VIEW_CALBRIEF) ) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	parse_calendar_view_request(&c);

	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			load_calendar_item(Msg, 0, &c);
		}
	}
	if (calendar_summary_view() < 1) {
		wprintf("<i>");
		wprintf(_("(Nothing)"));
		wprintf("</i><br />\n");
	}
}

/*
 * Server info section (fluff, really)
 */
void server_info_section(void) {
	char message[512];

	snprintf(message, sizeof message,
		_("You are connected to %s, running %s with %s, server build %s and located in %s.  Your system administrator is %s."),
		 ChrPtr(serv_info.serv_humannode),
		 ChrPtr(serv_info.serv_software),
		 PACKAGE_STRING,
		 ChrPtr(serv_info.serv_svn_revision),
		 ChrPtr(serv_info.serv_bbs_city),
		 ChrPtr(serv_info.serv_sysadm));
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
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table width=\"100%%\" cellspacing=\"10\" cellpadding=\"0\">"
		"<tr valign=top>");

	/*
	 * Column One
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Messages"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"msg_inner\">");	
	new_messages_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	/*
	 * Column Two 
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Tasks"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"tasks_inner\">");	
	tasks_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	/*
	 * Column Three
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Today&nbsp;on&nbsp;your&nbsp;calendar"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"calendar_inner\">");	
	calendar_section();
	wprintf("</div></div></div>");
	wprintf("</td>");

	wprintf("</tr><tr valign=top>");

	/*
	 * Row Two - Column One
	 */
	wprintf("<td colspan=2>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("Who's&nbsp;online&nbsp;now"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"who_inner\">");	
	do_template("wholistsummarysection", NULL);
	wprintf("</div></div></div>");
	wprintf("</td>");

	/*
	 * Row Two - Column Two
	 */
	wprintf("<td width=33%%>");
	wprintf("<div class=\"box\">");	
	wprintf("<div class=\"boxlabel\">");	
	wprintf(_("About&nbsp;this&nbsp;server"));
	wprintf("</div><div class=\"boxcontent\">");	
	wprintf("<div id=\"info_inner\">");	
	server_info_section();
	wprintf("</div></div></div>");
	wprintf("</td>");


	/*
	 * End of columns
	 */
	wprintf("</tr></table>");
}


/*
 * Display this user's summary page
 */
void summary(void) {
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<div class=\"room_banner\">");
        wprintf("<img src=\"static/summscreen_48x.gif\">");
        wprintf("<h1>");
        snprintf(title, sizeof title, _("Summary page for %s"), ChrPtr(WC->wc_fullname));
        escputs(title);
        wprintf("</h1><h2>");
        output_date();
        wprintf("</h2></div>");
	wprintf("<div id=\"actiondiv\">");
	wprintf("<ul class=\"room_actions\">\n");
	wprintf("<li class=\"start_page\">");
	offer_start_page(NULL, &NoCtx);
        wprintf("</li></ul>");
        wprintf("</div>");
        wprintf("</div>");

	/*
	 * You guessed it ... we're going to refresh using ajax.
	 * In the future we might consider updating individual sections of the summary
	 * instead of the whole thing.
	 */
	wprintf("<div id=\"content\" class=\"service\">\n");
	summary_inner_div();
	wprintf("</div>\n");

	wprintf(
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
	WebcitAddUrlHandler(HKEY("new_messages_html"), new_messages_section, AJAX);
	WebcitAddUrlHandler(HKEY("tasks_inner_html"), tasks_section, AJAX);
	WebcitAddUrlHandler(HKEY("calendar_inner_html"), calendar_section, AJAX);
	WebcitAddUrlHandler(HKEY("mini_calendar"), ajax_mini_calendar, AJAX);
	WebcitAddUrlHandler(HKEY("summary"), summary, 0);
	WebcitAddUrlHandler(HKEY("summary_inner_div"), summary_inner_div, AJAX);
}

