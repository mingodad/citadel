/*
 * $Id$
 */
/**
 * \defgroup SymaryFuncs Displays the "Summary Page"
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"

/**
 * \brief Display today's date in a friendly format
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




/**
 * \brief Dummy section
 */
void dummy_section(void) {
	svprintf("BOXTITLE", WCS_STRING, "(dummy&nbsp;section)");
	do_template("beginbox");
	wprintf(_("(nothing)"));
	do_template("endbox");
}


/**
 * \brief New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";

	svprintf("BOXTITLE", WCS_STRING, _("Messages"));
	do_template("beginbox");

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
	do_template("endbox");

}


/**
 * \brief Wholist section
 */
void wholist_section(void) {
	char buf[SIZ];
	char user[SIZ];

	svprintf("BOXTITLE", WCS_STRING, _("Who's&nbsp;online&nbsp;now"));
	do_template("beginbox");
	serv_puts("RWHO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(user, buf, 1, '|', sizeof user);
		escputs(user);
		wprintf("<br />\n");
	}
	do_template("endbox");
}


/**
 * \brief Task list section
 */
void tasks_section(void) {
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	int num_msgs = 0;
	int i;
#endif

	svprintf("BOXTITLE", WCS_STRING, _("Tasks"));
	do_template("beginbox");
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	gotoroom("_TASKS_");
	if (WC->wc_view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>");
		wprintf(_("(None)"));
		wprintf("</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_task(WC->msgarr[i]);
		}
	}

	calendar_summary_view();

#else /* WEBCIT_WITH_CALENDAR_SERVICE */
	wprintf("<i>");
	wprintf(_("(This server does not support task lists)"));
	wprintf("</i>\n");
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
	do_template("endbox");
}


/**
 * \brief Calendar section
 */
void calendar_section(void) {
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	int num_msgs = 0;
	int i;
#endif

	svprintf("BOXTITLE", WCS_STRING, _("Today&nbsp;on&nbsp;your&nbsp;calendar"));
	do_template("beginbox");
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	gotoroom("_CALENDAR_");
	if ( (WC->wc_view != VIEW_CALENDAR) && (WC->wc_view != VIEW_CALBRIEF) ) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", 0);
	}

	if (num_msgs < 1) {
		wprintf("<i>");
		wprintf(_("(Nothing)"));
		wprintf("</i><br />\n");
	}
	else {
		for (i=0; i<num_msgs; ++i) {
			display_calendar(WC->msgarr[i]);
		}
		calendar_summary_view();
	}

#else /* WEBCIT_WITH_CALENDAR_SERVICE */
	wprintf("<i>");
	wprintf(_("(This server does not support calendars)"));
	wprintf("</i>\n");
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
	do_template("endbox");
}

/**
 * \brief Server info section (fluff, really)
 */
void server_info_section(void) {
	char message[512];

	svprintf("BOXTITLE", WCS_STRING, _("About&nbsp;this&nbsp;server"));
	do_template("beginbox");

	snprintf(message, sizeof message,
		_("You are connected to %s, running %s with %s, and located in %s.  Your system administrator is %s."),
		serv_info.serv_humannode,
		serv_info.serv_software,
		SERVER,
		serv_info.serv_bbs_city,
		serv_info.serv_sysadm);
	escputs(message);
	do_template("endbox");
}

/**
 * \brief summary of inner div????
 */
void summary_inner_div(void) {
	/**
	 * Now let's do three columns of crap.  All portals and all groupware
	 * clients seem to want to do three columns, so we'll do three
	 * columns too.  Conformity is not inherently a virtue, but there are
	 * a lot of really shallow people out there, and even though they're
	 * not people I consider worthwhile, I still want them to use WebCit.
	 */

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%%><tr valign=top>");

	/**
	 * Column One
	 */
	wprintf("<td width=33%%>");
	wholist_section();

	/**
	 * Column Two
	 */
	wprintf("</td><td width=33%%>");
	server_info_section();
	wprintf("<br />");
	tasks_section();

	/**
	 * Column Three
	 */
	wprintf("</td><td width=33%%>");
	new_messages_section();
	wprintf("<br />");
	calendar_section();

	/**
	 * End of columns
	 */
	wprintf("</td></tr></table>");
}


/**
 * \brief Display this user's summary page
 */
void summary(void) {
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<table width=100%% border=0 bgcolor=#444455><tr>"
		"<td><img src=\"static/summscreen_48x.gif\"></td><td>"
		"<span class=\"titlebar\">"
	);

	snprintf(title, sizeof title, _("Summary page for %s"), WC->wc_fullname);
	escputs(title);
	wprintf("</span></td><td>\n");
	wprintf("</td><td aling=right><span class=\"titlebar\">");
	output_date();
	wprintf("</span><br />");
	offer_start_page();
	wprintf("</td></tr></table>\n");

	/**
	 * You guessed it ... we're going to refresh using ajax.
	 * In the future we might consider updating individual sections of the summary
	 * instead of the whole thing.
	 */
	wprintf("</div>\n<div id=\"content\">\n");
	summary_inner_div();
	wprintf("</div>\n");

	wprintf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('content', 'summary_inner_div',		"
		"                            { method: 'get', frequency: 60 }  );	"
		"</script>							 	\n"
	);

	wDumpContent(1);
}


/*@}*/
