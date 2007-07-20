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

        if (ib_logo) {
                if (ib_displayas != IB_TEXTONLY) {
                        wprintf("<div class=\"logo\"> <img "
                                "src=\"image&name=hello\" alt=\"&nbsp;\"> "
                                "</div>\n"
                        );
                }
                wprintf("\n");
        }

        if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
                "<div class=\"logo_citadel\"> "
                "<a href=\"http://www.citadel.org\" "
                "title=\"%s\" target=\"aboutcit\"> "
                "<img "
                "src=\"static/citadel-logo.gif\" alt=\"%s\"></a> "
                "</div>\n",
                _("Find out more about Citadel"),
                _("CITADEL")
        );

	wprintf("<ul id=\"button\">\n");

	wprintf("<li class=\"switch\"><a href=\"javascript:switch_to_room_list()\">");
	wprintf(_("switch to room list"));
	wprintf("</a></li>");

	if (ib_summary) {
		wprintf("<li><a href=\"summary\" "
			"title=\"%s\" "
			">", _("Your summary page")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
				"src=\"static/summscreen_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Summary"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_inbox) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_MAIL_\" "
			"title=\"%s\" "
			">",
			_("Go to your email inbox")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
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
		wprintf("</a></li>\n");
	}

	if (ib_calendar) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_CALENDAR_\" "
			"title=\"%s\" "
			">",
			_("Go to your personal calendar")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/calarea_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Calendar"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_contacts) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_CONTACTS_\" "
			"title=\"%s\" "
			">",
			_("Go to your personal address book")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/viewcontacts_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Contacts"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_notes) {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_NOTES_\" "
			"title=\"%s\" "
			">",
			_("Go to your personal notes")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/storenotes_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Notes"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_tasks)  {
		wprintf("<li>"
			"<a href=\"dotgoto?room=_TASKS_\" "
			"title=\"%s\" "
			">",
			_("Go to your personal task list")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/taskmanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Tasks"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_rooms) {
		wprintf("<li>"
			"<a href=\"knrooms\" title=\"%s\" >",
			_("List all of your accessible rooms")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/chatrooms_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Rooms"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_users) {
		wprintf("<li>"
			"<a href=\"who\" title=\"%s\" "
			">",
			_("See who is online right now")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/usermanag_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Who is online?"));
		}
		 
		wprintf("</a>\n");

		if (ib_users > 1) {
			wprintf("<ul id=\"wholist\">");
			wprintf("</ul></li>\n");
		}
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
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/citadelchat_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Chat"));
		}
		wprintf("</a></li>\n");
	}

	if (ib_advanced) {
		wprintf("<li>"
			"<a href=\"display_main_menu\" "
			"title=\"%s\" "
			">",
			_("Advanced Options Menu: Advanced Room commands, Account Info, and Chat")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Advanced"));
		}
		wprintf("</a></li>\n");
	}

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<li>"
			"<a href=\"display_aide_menu\" "
			"title=\"%s\" "
			">",
			_("Room and system administration functions")
		);
		if (ib_displayas != IB_TEXTONLY) {
			wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
			"src=\"static/advanpage2_32x.gif\">");
		}
		if (ib_displayas != IB_PICONLY) {
			wprintf(_("Administration"));
		}
		wprintf("</a></li>\n");
	}

	wprintf("<li>"
		"<a href=\"termquit\" title=\"%s\" "
		"onClick=\"return confirm('%s');\">",
		_("Log off"),
		_("Log off now?")
		
	);
	if (ib_displayas != IB_TEXTONLY) {
	wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
		"src=\"static/logoff_32x.gif\">");
	}
	if (ib_displayas != IB_PICONLY) {
		wprintf(_("Log off"));
	}
	wprintf("</a></li>\n");

	wprintf(
		"<li class=\"switch\">"
		"<a href=\"display_customize_iconbar\" "
		"title=\"%s\" "
		">%s"
		"</a></li>\n",
		_("Customize this menu"),
		_("customize this menu")
	);

	wprintf("</ul>\n");

	if (ib_users > 1) {
        	wprintf(
                	"<script type=\"text/javascript\"> "
                	" new Ajax.PeriodicalUpdater('wholist', 'wholist_section', { method: 'get', frequency: 30 } );"
                "</script> \n"
        	);
	}

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

	if (ib_logo) {
		if (ib_displayas != IB_TEXTONLY) {
                        wprintf("<div class=\"logo\"> <img "
                                "src=\"image&name=hello\" alt=\"&nbsp;\"> "
                                "</div>\n"
			);
		}
	}

        if (ib_citadel) if (ib_displayas != IB_TEXTONLY) wprintf(
                "<div class=\"logo_citadel\"> "
                "<a href=\"http://www.citadel.org\" "
                "title=\"%s\" target=\"aboutcit\"> "
                "<img "
                "src=\"static/citadel-logo.gif\" alt=\"%s\"></a> "
                "</div>\n",
                _("Find out more about Citadel"),
                _("CITADEL")
        );

	wprintf("<ul id=\"button\">\n");

	wprintf("<li class=\"switch\"><a href=\"javascript:switch_to_menu_buttons()\">");
	wprintf(_("switch to menu"));
	wprintf("</a></li>");

	wprintf("<li>"
		"<a href=\"termquit\" title=\"%s\" "
		"onClick=\"return confirm('%s');\">",
		_("Log off"),
		_("Log off now?")
		
	);
	if (ib_displayas != IB_TEXTONLY) {
	wprintf("<img border=\"0\" width=\"32\" height=\"32\" "
		"src=\"static/logoff_32x.gif\">");
	}
	if (ib_displayas != IB_PICONLY) {
		wprintf(_("Log off"));
	}
	wprintf("</a></li>\n");

	wprintf("</ul>\n");

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
	wprintf("<div id=\"banner\" class=\"service\">\n");
	wprintf("<h1>");
	wprintf(_("Customize the icon bar"));
	wprintf("</h1></div>\n");
	wprintf("<div id=\"content\" class=\"customize_menu\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">");

	wprintf("<form method=\"post\" action=\"commit_iconbar\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);

	wprintf("<table class=\"altern\" >\n");
	wprintf("<tr><td></td><td colspan=\"2\"><b>");
	wprintf(_("Display icons as:"));
	wprintf("</b>");
	for (i=0; i<=2; ++i) {
		wprintf("<input type=\"radio\" name=\"ib_displayas\" value=\"%d\"", i);
		if (ib_displayas == i) wprintf(" CHECKED");
		wprintf(">");
		if (i == IB_PICTEXT)	wprintf(_("pictures and text"));
		if (i == IB_PICONLY)	wprintf(_("pictures only"));
		if (i == IB_TEXTONLY)	wprintf(_("text only"));
		wprintf("\n");
	}
	wprintf("<br />\n");

	wprintf(_("Select the icons you would like to see displayed "
		"in the 'icon bar' menu on the left side of the "
		"screen."));
	wprintf("</td></tr>\n");

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_logo\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_logo\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"image&name=hello\" width=\"48\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_logo ? "CHECKED" : ""),_("Yes"),
		(!ib_logo ? "CHECKED" : ""),_("No"),
		_("Site logo"),
		_("An icon describing this site")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_summary\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_summary\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/summscreen_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_summary ? "CHECKED" : ""),_("Yes"),
		(!ib_summary ? "CHECKED" : ""),_("No"),
		_("Summary"),
		_("Your summary page")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_inbox\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_inbox\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/privatemess_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_inbox ? "CHECKED" : ""),_("Yes"),
		(!ib_inbox ? "CHECKED" : ""),_("No"),
		_("Mail (inbox)"),
		_("A shortcut to your email Inbox")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_contacts\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_contacts\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/viewcontacts_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_contacts ? "CHECKED" : ""),_("Yes"),
		(!ib_contacts ? "CHECKED" : ""),_("No"),
		_("Contacts"),
		_("Your personal address book")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_notes\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_notes\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/storenotes_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_notes ? "CHECKED" : ""),_("Yes"),
		(!ib_notes ? "CHECKED" : ""),_("No"),
		_("Notes"),
		_("Your personal notes")
	);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_calendar\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_calendar\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/calarea_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_calendar ? "CHECKED" : ""),_("Yes"),
		(!ib_calendar ? "CHECKED" : ""),_("No"),
		_("Calendar"),
		_("A shortcut to your personal calendar")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_tasks\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_tasks\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/taskmanag_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_tasks ? "CHECKED" : ""),_("Yes"),
		(!ib_tasks ? "CHECKED" : ""),_("No"),
		_("Tasks"),
		_("A shortcut to your personal task list")
	);
#endif /* WEBCIT_WITH_CALENDAR_SERVICE */

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_rooms\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_rooms\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/chatrooms_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_rooms ? "CHECKED" : ""),_("Yes"),
		(!ib_rooms ? "CHECKED" : ""),_("No"),
		_("Rooms"),
		_("Clicking this icon displays a list of all accessible "
		"rooms (or folders) available.")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_users\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_users\" value=\"no\" %s> %s <br />"
		"<input type=\"radio\" name=\"ib_users\" value=\"yeslist\" %s> %s"
		"</td><td>"
		"<img src=\"static/usermanag_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b>"
		"<br />%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_users ? "CHECKED" : ""),_("Yes"),
		(!ib_users ? "CHECKED" : ""),_("No"),
		((ib_users > 1) ? "CHECKED" : ""),_("Yes with users list"),
		_("Who is online?"),
		_("Clicking this icon displays a list of all users "
		"currently logged in.")
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_chat\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_chat\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/citadelchat_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_chat ? "CHECKED" : ""),_("Yes"),
		(!ib_chat ? "CHECKED" : ""),_("No"),
		_("Chat"),
		_("Clicking this icon enters real-time chat mode "
		"with other users in the same room.")
		
	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_advanced\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_advanced\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img src=\"static/advanpage2_48x.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_advanced ? "CHECKED" : ""),_("Yes"),
		(!ib_advanced ? "CHECKED" : ""),_("No"),
		_("Advanced options"),
		_("Access to the complete menu of Citadel functions.")

	);

	bar = 1 - bar;
	wprintf("<tr class=\"%s\"><td>"
		"<input type=\"radio\" name=\"ib_citadel\" value=\"yes\" %s> %s &nbsp;&nbsp;&nbsp;"
		"<input type=\"radio\" name=\"ib_citadel\" value=\"no\" %s> %s <br />"
		"</td><td>"
		"<img border=\"0\" width=\"48\" height=\"48\" "
		"src=\"static/citadel-logo.gif\" alt=\"&nbsp;\">"
		"</td><td>"
		"<b>%s</b><br />"
		"%s"
		"</td></tr>\n",
		(bar ? "even" : "odd"),
		(ib_citadel ? "CHECKED" : ""),_("Yes"),
		(!ib_citadel ? "CHECKED" : ""),_("No"),
		_("Citadel logo"),
		_("Displays the 'Powered by Citadel' icon")
	);

	wprintf("</table><br />\n"
		"<center>"
		"<input type=\"submit\" name=\"ok_button\" value=\"%s\">"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">"
		"</center>\n",
		_("Save changes"),
		_("Cancel")
	);

	wprintf("</form></div>\n");
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

	if (IsEmptyStr(bstr("ok_button"))) {
		display_main_menu();
		return;
	}

	sprintf(iconbar, "ib_displayas=%d", atoi(bstr("ib_displayas")));

	for (i=0; i<(sizeof(boxen)/sizeof(char *)); ++i) {
		char *Val;
		if (!strcasecmp(bstr(boxen[i]), "yes")) {
			Val = "1";
		}
		else if (!strcasecmp(bstr(boxen[i]), "yeslist")) {
			Val = "2";
		}
		else {
			Val = "0";
		}
		sprintf(&iconbar[strlen(iconbar)], ",%s=%s", boxen[i], Val);
	}

	set_preference("iconbar", iconbar, 1);

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\" class=\"service\">\n");
	wprintf("<h1>");
	wprintf(_("Customize the icon bar"));
	wprintf("</h1></div>\n");
	wprintf("<div id=\"content\" class=\"customize_menu\">\n");
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
