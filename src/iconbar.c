/*
 * $Id$
 */
/**
 * \defgroup IconBar Displays and customizes the iconbar.
 * \ingroup MenuInfrastructure
 */
/*@{*/
#include "webcit.h"


/** Values for ib_displayas */
#define IB_PICTEXT	0 /**< picture and text */
#define IB_PICONLY	1 /**< just a picture */
#define IB_TEXTONLY	2 /**< just text */


/**
 * \brief draw the icon bar?????
 */
void do_selected_iconbar(void) {
	if (WC->current_iconbar == current_iconbar_roomlist) {
		do_iconbar_roomlist();
	}
	else {
		do_iconbar();
	}
}

/**
 * \brief draw the icon bar???
 */
void do_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;

	WC->current_iconbar = current_iconbar_menu;

	/**
	 * The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_displayas = 0;	/**< pictures and text, pictures, text */
	int ib_logo = 0;	/**< Site logo */
	int ib_summary = 1;	/**< Summary page icon */
	int ib_inbox = 1;	/**< Inbox icon */
	int ib_calendar = 1;	/**< Calendar icon */
	int ib_contacts = 1;	/**< Contacts icon */
	int ib_notes = 1;	/**< Notes icon */
	int ib_tasks = 1;	/**< Tasks icon */
	int ib_rooms = 1;	/**< Rooms icon */
	int ib_users = 1;	/**< Users icon */
	int ib_chat = 1;	/**< Chat icon */
	int ib_advanced = 1;	/**< Advanced Options icon */
	int ib_citadel = 1;	/**< 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar, sizeof iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',', sizeof buf);
		extract_token(key, buf, 0, '=', sizeof key);
		extract_token(value, buf, 1, '=', sizeof value);

		if (!strcasecmp(key, "ib_displayas")) ib_displayas = atoi(value);
		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_summary")) ib_summary = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_contacts")) ib_contacts = atoi(value);
		if (!strcasecmp(key, "ib_notes")) ib_notes = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_chat")) ib_chat = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	wprintf("<div id=\"button\">\n"
		"<ul>\n"
	);

	if (ib_logo) {
		wprintf("<li>");
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" "
				"HEIGHT=\"32\" src=\"image&name=hello\" ALT=\"&nbsp;\">\n"
			);
		}
		wprintf("</li>\n");
	}

	if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
		"<li><div align=\"center\">"
		"<a href=\"http://www.citadel.org\" "
		"title=\"%s\" target=\"aboutcit\">"
		"<img border=\"0\" "
		"src=\"static/citadel-logo.gif\" ALT=\"%s\"></a>"
		"</div></li>\n",
		_("Find out more about Citadel"),
		_("CITADEL")
	);

	wprintf("<li><div align=\"center\"><a href=\"javascript:switch_to_room_list()\">");
	wprintf(_("switch to room list"));
	wprintf("</a></div>");

	if (ib_summary) {
		wprintf("<li><a href=\"summary\" "
			"TITLE=\"%s\" "
			">", _("Your summary page")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"src=\"static/summscreen_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Summary"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_inbox) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_MAIL_\" "
			"TITLE=\"%s\" "
			">",
			_("Go to your email inbox")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
				"src=\"static/privatemess_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Mail"));
			if (WC->new_mail != WC->remember_new_mail) {
/*
				if (WC->new_mail > 0) {
					wprintf(" <b>(%d)</b>", WC->new_mail);
				}
*/
				WC->remember_new_mail = WC->new_mail;
			}
		}
		wprintf("</A></li>\n");
	}

	if (ib_calendar) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_CALENDAR_\" "
			"TITLE=\"%s\" "
			">",
			_("Go to your personal calendar")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/calarea_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Calendar"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_contacts) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_CONTACTS_\" "
			"TITLE=\"%s\" "
			">",
			_("Go to your personal address book")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/viewcontacts_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Contacts"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_notes) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_NOTES_\" "
			"TITLE=\"%s\" "
			">",
			_("Go to your personal notes")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/storenotes_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Notes"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_tasks)  {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_TASKS_\" "
			"TITLE=\"%s\" "
			">",
			_("Go to your personal task list")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/taskmanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Tasks"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_rooms) {
		wprintf("<li>"
			"<a href=\"knrooms\" TITLE=\"%s\" >",
			_("List all of your accessible rooms")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/chatrooms_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Rooms"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_users) {
		wprintf("<li>"
			"<a href=\"who\" TITLE=\"%s\" "
			">",
			_("See who is online right now")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/usermanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Who is online?"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_chat) {
		wprintf("<li>"
			"<a href=\"#\" onClick=\"window.open('chat', "
			"'ctdl_chat_window', "
			"'toolbar=no,location=no,directories=no,copyhistory=no,"
			"status=no,scrollbars=yes,resizable=yes');\""
			">"
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/citadelchat_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Chat"));
		}
		wprintf("</A></li>\n");
	}

	if (ib_advanced) {
		wprintf("<li>"
			"<a href=\"display_main_menu\" "
			"TITLE=\"%s\" "
			">",
			_("Advanced Options Menu: Advanced Room commands, Account Info, and Chat")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Advanced"));
		}
		wprintf("</A></li>\n");
	}

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<li>"
			"<a href=\"display_aide_menu\" "
			"TITLE=\"%s\" "
			">",
			_("Room and system administration functions")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
			"src=\"static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Administration"));
		}
		wprintf("</A></li>\n");
	}

	wprintf("<li>"
		"<a href=\"termquit\" TITLE=\"%s\" "
		"onClick=\"return confirm('%s');\">",
		_("Log off"),
		_("Log off now?")
		
	);
	if (ib_displayas != IB_TEXTONLY) {
	wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"src=\"static/logoff_32x.gif\">");
	}
	if (ib_displayas != IB_PICONLY) {
		wprintf(_("Log off"));
	}
	wprintf("</A></li>\n");

	wprintf(
		"<li><div align=\"center\">"
		"<a href=\"display_customize_iconbar\" "
		"TITLE=\"%s\" "
		">%s"
		"</A></div></li>\n",
		_("Customize this menu"),
		_("customize this menu")
	);

	wprintf("</ul></div>\n");
}


/**
 * \brief roomtree view of the iconbar
 * If the user has toggled the icon bar over to a room list, here's where
 * we generate its innerHTML...
 */
void do_iconbar_roomlist(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;

	WC->current_iconbar = current_iconbar_roomlist;

	/**
	 * The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_displayas = 0;	/* pictures and text, pictures, text */
	int ib_logo = 0;	/* Site logo */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar, sizeof iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',', sizeof buf);
		extract_token(key, buf, 0, '=', sizeof key);
		extract_token(value, buf, 1, '=', sizeof value);

		if (!strcasecmp(key, "ib_displayas")) ib_displayas = atoi(value);
		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	wprintf("<div id=\"button\">\n"
		"<ul>\n"
	);

	if (ib_logo) {
		wprintf("<li>");
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" "
				"HEIGHT=\"32\" src=\"image&name=hello\" ALT=\"&nbsp;\">\n"
			);
		}
		wprintf("</li>\n");
	}

	if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
		"<li><div align=\"center\">"
		"<a href=\"http://www.citadel.org\" "
		"title=\"%s\" target=\"aboutcit\">"
		"<img border=\"0\" "
		"src=\"static/citadel-logo.gif\" ALT=\"%s\"></a>"
		"</div></li>\n",
		_("Find out more about Citadel"),
		_("CITADEL")
	);

	wprintf("<li><div align=\"center\"><a href=\"javascript:switch_to_menu_buttons()\">");
	wprintf(_("switch to menu"));
	wprintf("</a></div>");

	wprintf("<li>"
		"<a href=\"termquit\" TITLE=\"%s\" "
		"onClick=\"return confirm('%s');\">",
		_("Log off"),
		_("Log off now?")
		
	);
	if (ib_displayas != IB_TEXTONLY) {
	wprintf("<IMG BORDER=\"0\" WIDTH=\"32\" HEIGHT=\"32\" "
		"src=\"static/logoff_32x.gif\">");
	}
	if (ib_displayas != IB_PICONLY) {
		wprintf(_("Log off"));
	}
	wprintf("</A></li>\n");

	wprintf("</ul></div>\n");

	/** embed the room list */
	list_all_rooms_by_floor("iconbar");

	wprintf("</div>\n");
}


/**
 * \brief display a customized version of the iconbar
 */
void display_customize_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;
	int bar = 0;

	/**
	 * The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_displayas = IB_PICTEXT;	/**< pictures and text, pictures, text */
	int ib_logo = 0;	/**< Site logo */
	int ib_summary = 1;	/**< Summary page icon */
	int ib_inbox = 1;	/**< Inbox icon */
	int ib_calendar = 1;	/**< Calendar icon */
	int ib_contacts = 1;	/**< Contacts icon */
	int ib_notes = 1;	/**< Notes icon */
	int ib_tasks = 1;	/**< Tasks icon */
	int ib_rooms = 1;	/**< Rooms icon */
	int ib_users = 1;	/**< Users icon */
	int ib_chat = 1;	/**< Chat icon */
	int ib_advanced = 1;	/**< Advanced Options icon */
	int ib_citadel = 1;	/**< 'Powered by Citadel' logo */
	/*
	 */

	get_preference("iconbar", iconbar, sizeof iconbar);
	for (i=0; i<num_tokens(iconbar, ','); ++i) {
		extract_token(buf, iconbar, i, ',', sizeof buf);
		extract_token(key, buf, 0, '=', sizeof key);
		extract_token(value, buf, 1, '=', sizeof value);

		if (!strcasecmp(key, "ib_displayas")) ib_displayas = atoi(value);
		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_summary")) ib_summary = atoi(value);
		if (!strcasecmp(key, "ib_inbox")) ib_inbox = atoi(value);
		if (!strcasecmp(key, "ib_calendar")) ib_calendar = atoi(value);
		if (!strcasecmp(key, "ib_contacts")) ib_contacts = atoi(value);
		if (!strcasecmp(key, "ib_notes")) ib_notes = atoi(value);
		if (!strcasecmp(key, "ib_tasks")) ib_tasks = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_chat")) ib_chat = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);
	}

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Customize the icon bar"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>");

	wprintf("<FORM METHOD=\"POST\" action=\"commit_iconbar\">\n");

	wprintf("<CENTER>");
	wprintf(_("Display icons as:"));
	wprintf(" ");
	for (i=0; i<=2; ++i) {
		wprintf("<INPUT TYPE=\"radio\" NAME=\"ib_displayas\" VALUE=\"%d\"", i);
		if (ib_displayas == i) wprintf(" CHECKED");
		wprintf(">");
		if (i == IB_PICTEXT)	wprintf(_("pictures and text"));
		if (i == IB_PICONLY)	wprintf(_("pictures only"));
		if (i == IB_TEXTONLY)	wprintf(_("text only"));
		wprintf("\n");
	}
	wprintf("<br /><br />\n");

	wprintf(_("Select the icons you would like to see displayed "
		"in the 'icon bar' menu on the left side of the "
		"screen."));
	wprintf("</CENTER><br />\n");

	wprintf("<TABLE border=0 cellspacing=0 cellpadding=3 width=100%%>\n");

	wprintf("<TR BGCOLOR=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_logo\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"image&name=hello\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_logo ? "CHECKED" : ""),
		_("Site logo"),
		_("An icon describing this site")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_summary\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/summscreen_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_summary ? "CHECKED" : ""),
		_("Summary"),
		_("Your summary page")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_inbox\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/privatemess_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_inbox ? "CHECKED" : ""),
		_("Mail (inbox)"),
		_("A shortcut to your email Inbox")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_contacts\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/viewcontacts_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_contacts ? "CHECKED" : ""),
		_("Contacts"),
		_("Your personal address book")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_notes\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/storenotes_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_notes ? "CHECKED" : ""),
		_("Notes"),
		_("Your personal notes")
	);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_calendar\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/calarea_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_calendar ? "CHECKED" : ""),
		_("Calendar"),
		_("A shortcut to your personal calendar")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_tasks\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/taskmanag_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_tasks ? "CHECKED" : ""),
		_("Tasks"),
		_("A shortcut to your personal task list")
	);
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_rooms\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/chatrooms_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_rooms ? "CHECKED" : ""),
		_("Rooms"),
		_("Clicking this icon displays a list of all accessible "
		"rooms (or folders) available.")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_users\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/usermanag_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_users ? "CHECKED" : ""),
		_("Who is online?"),
		_("Clicking this icon displays a list of all users "
		"currently logged in.")
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_chat\" VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/citadelchat_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_chat ? "CHECKED" : ""),
		_("Chat"),
		_("Clicking this icon enters real-time chat mode "
		"with other users in the same room.")
		
	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_advanced\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/advanpage2_48x.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_advanced ? "CHECKED" : ""),
		_("Advanced options"),
		_("Access to the complete menu of Citadel functions.")

	);

	wprintf("<TR bgcolor=%s><TD>"
		"<INPUT TYPE=\"checkbox\" NAME=\"ib_citadel\" "
		"VALUE=\"yes\" %s>"
		"</TD><TD>"
		"<IMG BORDER=\"0\" WIDTH=\"48\" HEIGHT=\"48\" "
		"src=\"static/citadel-logo.gif\" ALT=\"&nbsp;\">"
		"</TD><TD>"
		"<B>%s</B><br />"
		"%s"
		"</TD></TR>\n",
		((bar = 1 - bar), (bar ? "\"#CCCCCC\"" : "\"#FFFFFF\"")),
		(ib_citadel ? "CHECKED" : ""),
		_("Citadel logo"),
		_("Displays the 'Powered by Citadel' icon")
	);

	wprintf("</TABLE><br />\n"
		"<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">"
		"</CENTER></FORM>\n",
		_("Save changes"),
		_("Cancel")
	);

	wprintf("</td></tr></table></div>\n");
	wDumpContent(2);
}

/**
 * \brief commit the changes of an edited iconbar ????
 */
void commit_iconbar(void) {
	char iconbar[SIZ];
	int i;

	char *boxen[] = {
		"ib_logo",
		"ib_summary",
		"ib_inbox",
		"ib_calendar",
		"ib_contacts",
		"ib_notes",
		"ib_tasks",
		"ib_rooms",
		"ib_users",
		"ib_chat",
		"ib_advanced",
		"ib_logoff",
		"ib_citadel"
	};

	if (strlen(bstr("ok_button")) == 0) {
		display_main_menu();
		return;
	}

	sprintf(iconbar, "ib_displayas=%d", atoi(bstr("ib_displayas")));

	for (i=0; i<(sizeof(boxen)/sizeof(char *)); ++i) {
		sprintf(&iconbar[strlen(iconbar)], ",%s=", boxen[i]);
		if (!strcasecmp(bstr(boxen[i]), "yes")) {
			sprintf(&iconbar[strlen(iconbar)], "1");
		}
		else {
			sprintf(&iconbar[strlen(iconbar)], "0");
		}
	}

	set_preference("iconbar", iconbar, 1);

	output_headers(1, 1, 0, 0, 0, 0);
	wprintf(
		"<center><table border=1 bgcolor=\"#ffffff\"><tr><td>"
		"<img src=\"static/advanpage2_48x.gif\">"
		"&nbsp;");
	wprintf(_("Your icon bar has been updated.  Please select any of its "
		"choices to continue."));
	wprintf("</td></tr></table>\n");
	wDumpContent(2);
}



/*@}*/
