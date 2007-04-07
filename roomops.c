/*
 * $Id$
 */
/**
 * \defgroup RoomOps Lots of different room-related operations.
 * \ingroup CitadelCommunitacion
 */
/*@{*/
#include "webcit.h"

char floorlist[128][SIZ]; /**< list of our floor names */

char *viewdefs[8]; /**< the different kinds of available views */

/**
 * \brief initialize the viewdefs with localized strings
 */
void initialize_viewdefs(void) {
	viewdefs[0] = _("Bulletin Board");
	viewdefs[1] = _("Mail Folder");
	viewdefs[2] = _("Address Book");
	viewdefs[3] = _("Calendar");
	viewdefs[4] = _("Task List");
	viewdefs[5] = _("Notes List");
	viewdefs[6] = _("Wiki");
	viewdefs[7] = _("Calendar List");
}

/**
 * \brief	Determine which views are allowed as the default for creating a new room.
 *
 * \param	which_view	The view ID being queried.
 */
int is_view_allowed_as_default(int which_view)
{
	switch(which_view) {
		case VIEW_BBS:		return(1);
		case VIEW_MAILBOX:	return(1);
		case VIEW_ADDRESSBOOK:	return(1);
		case VIEW_CALENDAR:	return(1);
		case VIEW_TASKS:	return(1);
		case VIEW_NOTES:	return(1);
		case VIEW_WIKI:		return(0);	/**< because it isn't finished yet */
		case VIEW_CALBRIEF:	return(0);
		default:		return(0);	/**< should never get here */
	}
}


/**
 * \brief load the list of floors
 */
void load_floorlist(void)
{
	int a;
	char buf[SIZ];

	for (a = 0; a < 128; ++a)
		floorlist[a][0] = 0;

	serv_puts("LFLR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		strcpy(floorlist[0], "Main Floor");
		return;
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(floorlist[extract_int(buf, 0)], buf, 1, '|', sizeof floorlist[0]);
	}
}


/**
 * \brief	Free a session's march list
 *
 * \param	wcf		Pointer to session being cleared
 */
void free_march_list(struct wcsession *wcf)
{
	struct march *mptr;

	while (wcf->march != NULL) {
		mptr = wcf->march->next;
		free(wcf->march);
		wcf->march = mptr;
	}

}



/**
 * \brief remove a room from the march list
 */
void remove_march(char *aaa)
{
	struct march *mptr, *mptr2;

	if (WC->march == NULL)
		return;

	if (!strcasecmp(WC->march->march_name, aaa)) {
		mptr = WC->march->next;
		free(WC->march);
		WC->march = mptr;
		return;
	}
	mptr2 = WC->march;
	for (mptr = WC->march; mptr != NULL; mptr = mptr->next) {
		if (!strcasecmp(mptr->march_name, aaa)) {
			mptr2->next = mptr->next;
			free(mptr);
			mptr = mptr2;
		} else {
			mptr2 = mptr;
		}
	}
}




/**
 * \brief display rooms in tree structure???
 * \param rp the roomlist to build a tree from
 */
void room_tree_list(struct roomlisting *rp)
{
	char rmname[64];
	int f;

	if (rp == NULL) {
		return;
	}

	room_tree_list(rp->lnext);

	strcpy(rmname, rp->rlname);
	f = rp->rlflags;

	wprintf("<a href=\"dotgoto&room=");
	urlescputs(rmname);
	wprintf("\"");
	wprintf(">");
	escputs1(rmname, 1, 1);
	if ((f & QR_DIRECTORY) && (f & QR_NETWORK))
		wprintf("}");
	else if (f & QR_DIRECTORY)
		wprintf("]");
	else if (f & QR_NETWORK)
		wprintf(")");
	else
		wprintf("&gt;");
	wprintf("</A><TT> </TT>\n");

	room_tree_list(rp->rnext);
	free(rp);
}


/** 
 * \brief Room ordering stuff (compare first by floor, then by order)
 * \param r1 first roomlist to compare
 * \param r2 second roomlist co compare
 * \return are they the same???
 */
int rordercmp(struct roomlisting *r1, struct roomlisting *r2)
{
	if ((r1 == NULL) && (r2 == NULL))
		return (0);
	if (r1 == NULL)
		return (-1);
	if (r2 == NULL)
		return (1);
	if (r1->rlfloor < r2->rlfloor)
		return (-1);
	if (r1->rlfloor > r2->rlfloor)
		return (1);
	if (r1->rlorder < r2->rlorder)
		return (-1);
	if (r1->rlorder > r2->rlorder)
		return (1);
	return (0);
}


/**
 * \brief Common code for all room listings
 * \param variety what???
 */
void listrms(char *variety)
{
	char buf[SIZ];
	int num_rooms = 0;

	struct roomlisting *rl = NULL;
	struct roomlisting *rp;
	struct roomlisting *rs;

	/** Ask the server for a room list */
	serv_puts(variety);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("&nbsp;");
		return;
	}

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		++num_rooms;
		rp = malloc(sizeof(struct roomlisting));
		extract_token(rp->rlname, buf, 0, '|', sizeof rp->rlname);
		rp->rlflags = extract_int(buf, 1);
		rp->rlfloor = extract_int(buf, 2);
		rp->rlorder = extract_int(buf, 3);
		rp->lnext = NULL;
		rp->rnext = NULL;

		rs = rl;
		if (rl == NULL) {
			rl = rp;
		} else
			while (rp != NULL) {
				if (rordercmp(rp, rs) < 0) {
					if (rs->lnext == NULL) {
						rs->lnext = rp;
						rp = NULL;
					} else {
						rs = rs->lnext;
					}
				} else {
					if (rs->rnext == NULL) {
						rs->rnext = rp;
						rp = NULL;
					} else {
						rs = rs->rnext;
					}
				}
			}
	}

	room_tree_list(rl);

	/**
	 * If no rooms were listed, print an nbsp to make the cell
	 * borders show up anyway.
	 */
	if (num_rooms == 0) wprintf("&nbsp;");
}


/**
 * \brief list all forgotten rooms
 */
void zapped_list(void)
{
	output_headers(1, 1, 0, 0, 0, 0);

	svprintf("BOXTITLE", WCS_STRING, _("Zapped (forgotten) rooms"));
	do_template("beginbox");

	listrms("LZRM -1");

	wprintf("<br /><br />\n");
	wprintf(_("Click on any room to un-zap it and goto that room.\n"));
	do_template("endbox");
	wDumpContent(1);
}


/**
 * \brief read this room's info file (set v to 1 for verbose mode)
 */
void readinfo(void)
{
	char buf[SIZ];

	serv_puts("RINF");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
                wprintf("<div class=\"infos\" "
                "onclick=\"javascript:Effect.Appear('room_infos', { duration: 0.5 });\" "
                ">");
                wprintf(_("Room info"));
                wprintf("</div><div id=\"room_infos\" style=\"display:none;\">"
                "<p class=\"close_infos\" "
                "onclick=\"javascript:Effect.Fade('room_infos', { duration: 0.5 });\" "
                ">");
		wprintf(_("Close window"));
		wprintf("</p>");
                fmout("CENTER");
                wprintf("</div>");
	}
	else {
		wprintf("&nbsp;");
	}
}




/**
 * \brief Display room banner icon.  
 * The server doesn't actually
 * need the room name, but we supply it in order to
 * keep the browser from using a cached icon from 
 * another room.
 */
void embed_room_graphic(void) {
	char buf[SIZ];

	serv_puts("OIMG _roompic_");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wprintf("<IMG HEIGHT=64 src=\"image&name=_roompic_&room=");
		urlescputs(WC->wc_roomname);
		wprintf("\">");
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
	}
	else if (WC->wc_view == VIEW_ADDRESSBOOK) {
		wprintf("<img height=48 width=48 src=\""
			"static/viewcontacts_48x.gif"
			"\">"
		);
	}
	else if ( (WC->wc_view == VIEW_CALENDAR) || (WC->wc_view == VIEW_CALBRIEF) ) {
		wprintf("<img height=48 width=48 src=\""
			"static/calarea_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_TASKS) {
		wprintf("<img height=48 width=48 src=\""
			"static/taskmanag_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_NOTES) {
		wprintf("<img height=48 width=48 src=\""
			"static/storenotes_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_MAILBOX) {
		wprintf("<img height=48 width=48 src=\""
			"static/privatemess_48x.gif"
			"\">"
		);
	}
	else {
		wprintf("<img height=48 width=48 src=\""
			"static/chatrooms_48x.gif"
			"\">"
		);
	}

}



/**
 * \brief Display the current view and offer an option to change it
 */
void embed_view_o_matic(void) {
	int i;

	wprintf("<form name=\"viewomatic\" action=\"changeview\">\n"
		"<label for=\"view_name\">");
	wprintf(_("View as:"));
	wprintf("</label> "
		"<select name=\"newview\" size=\"1\" "
		"id=\"view_name\" class=\"selectbox\" "
		"OnChange=\"location.href=viewomatic.newview.options"
		"[selectedIndex].value\">\n");

	for (i=0; i<(sizeof viewdefs / sizeof (char *)); ++i) {
		/**
		 * Only offer the views that make sense, given the default
		 * view for the room.  For example, don't offer a Calendar
		 * view in a non-Calendar room.
		 */
		if (
			(i == WC->wc_view)
			||	(i == WC->wc_default_view)			/**< default */
			||	( (i == 0) && (WC->wc_default_view == 1) )	/**< mail or bulletin */
			||	( (i == 1) && (WC->wc_default_view == 0) )	/**< mail or bulletin */
			/** ||	( (i == 7) && (WC->wc_default_view == 3) )	(calendar list temporarily disabled) */
		) {

			wprintf("<option %s value=\"changeview?view=%d\">",
				((i == WC->wc_view) ? "selected" : ""),
				i );
			escputs(viewdefs[i]);
			wprintf("</option>\n");
		}
	}
	wprintf("</select></form>\n");
}


/**
 * \brief Display a search box
 */
void embed_search_o_matic(void) {
	wprintf("<form name=\"searchomatic\" action=\"do_search\">\n"
		"<label for=\"search_name\">");
	wprintf(_("Search: "));
	wprintf("</label> <input "
		"type=\"text\" name=\"query\" size=\"15\" maxlength=\"128\" "
		"id=\"search_name\" class=\"inputbox\">\n"
	);
	wprintf("</select></form>\n");
}


/**
 * \brief		Embed the room banner
 *
 * \param got		The information returned from a GOTO server command
 * \param navbar_style 	Determines which navigation buttons to display
 *
 */

void embed_room_banner(char *got, int navbar_style) {
	char buf[256];

	/**
	 * We need to have the information returned by a GOTO server command.
	 * If it isn't supplied, we fake it by issuing our own GOTO.
	 */
	if (got == NULL) {
		serv_printf("GOTO %s", WC->wc_roomname);
		serv_getln(buf, sizeof buf);
		got = buf;
	}

	/** The browser needs some information for its own use */
	wprintf("<script type=\"text/javascript\">	\n"
		"	room_is_trash = %d;		\n"
		"</script>\n",
		WC->wc_is_trash
	);

	/**
	 * If the user happens to select the "make this my start page" link,
	 * we want it to remember the URL as a "/dotskip" one instead of
	 * a "skip" or "gotonext" or something like that.
	 */
	snprintf(WC->this_page, sizeof(WC->this_page), "dotskip&room=%s",
		WC->wc_roomname);

	/** Check for new mail. */
	WC->new_mail = extract_int(&got[4], 9);
	WC->wc_view = extract_int(&got[4], 11);

	svprintf("ROOMNAME", WCS_STRING, "%s", WC->wc_roomname);
	svprintf("NUMMSGS", WCS_STRING,
		_("%d new of %d messages"),
		extract_int(&got[4], 1),
		extract_int(&got[4], 2)
	);
	svcallback("ROOMPIC", embed_room_graphic);
	svcallback("ROOMINFO", readinfo);
	svcallback("VIEWOMATIC", embed_view_o_matic);
	svcallback("SEARCHOMATIC", embed_search_o_matic);
	svcallback("START", offer_start_page);

	do_template("roombanner");
	if (navbar_style != navbar_none) {

		wprintf("<div id=\"navbar\">\n"
			"<ul>");

		

		if (navbar_style == navbar_default) wprintf(
			"<li class=\"ungoto\">"
			"<a href=\"ungoto\">"
			"<img align=\"middle\" src=\"static/ungoto2_24x.gif\" border=\"0\">"
			"<span class=\"navbar_link\">%s</span></A>"
			"</li>\n", _("Ungoto")
		);

		if ( (navbar_style == navbar_default) && (WC->wc_view == VIEW_BBS) ) {
			wprintf(
				"<li class=\"newmess\">"
				"<a href=\"readnew\">"
				"<img align=\"middle\" src=\"static/newmess2_24x.gif\" border=\"0\">"
				"<span class=\"navbar_link\">%s</span></A>"
				"</li>\n", _("Read new messages")
			);
		}

		if (navbar_style == navbar_default) {
			switch(WC->wc_view) {
				case VIEW_ADDRESSBOOK:
					wprintf(
						"<li class=\"viewcontacts\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/viewcontacts_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View contacts")
					);
					break;
				case VIEW_CALENDAR:
					wprintf(
						"<li class=\"staskday\">"
						"<a href=\"readfwd?calview=day\">"
						"<img align=\"middle\" src=\"static/taskday2_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Day view")
					);
					wprintf(
						"<li class=\"monthview\">"
						"<a href=\"readfwd?calview=month\">"
						"<img align=\"middle\" src=\"static/monthview2_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Month view")
					);
					break;
				case VIEW_CALBRIEF:
					wprintf(
						"<li class=\"monthview\">"
						"<a href=\"readfwd?calview=month\">"
						"<img align=\"middle\" src=\"static/monthview2_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Calendar list")
					);
					break;
				case VIEW_TASKS:
					wprintf(
						"<li class=\"taskmanag\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/taskmanag_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View tasks")
					);
					break;
				case VIEW_NOTES:
					wprintf(
						"<li class=\"viewnotes\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/viewnotes_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View notes")
					);
					break;
				case VIEW_MAILBOX:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/readallmess3_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View message list")
					);
					break;
				case VIEW_WIKI:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/readallmess3_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Wiki home")
					);
					break;
				default:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img align=\"middle\" src=\"static/readallmess3_24x.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Read all messages")
					);
					break;
			}
		}

		if (navbar_style == navbar_default) {
			switch(WC->wc_view) {
				case VIEW_ADDRESSBOOK:
					wprintf(
						"<li class=\"addnewcontact\">"
						"<a href=\"display_enter\">"
						"<img align=\"middle\" src=\"static/addnewcontact_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new contact")
					);
					break;
				case VIEW_CALENDAR:
				case VIEW_CALBRIEF:
					wprintf("<li class=\"addevent\"><a href=\"display_enter");
					if (strlen(bstr("year")) > 0) wprintf("?year=%s", bstr("year"));
					if (strlen(bstr("month")) > 0) wprintf("?month=%s", bstr("month"));
					if (strlen(bstr("day")) > 0) wprintf("?day=%s", bstr("day"));
					wprintf("\">"
						"<img align=\"middle\" src=\"static/addevent_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new event")
					);
					break;
				case VIEW_TASKS:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img align=\"middle\" src=\"static/newmess3_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new task")
					);
					break;
				case VIEW_NOTES:
					wprintf(
						"<li class=\"enternewnote\">"
						"<a href=\"javascript:add_new_note();\">"
						"<img align=\"middle\" src=\"static/enternewnote_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new note")
					);
					break;
				case VIEW_WIKI:
					safestrncpy(buf, bstr("page"), sizeof buf);
					str_wiki_index(buf);
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter?wikipage=%s\">"
						"<img align=\"middle\" src=\"static/newmess3_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", buf, _("Edit this page")
					);
					break;
				case VIEW_MAILBOX:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img align=\"middle\" src=\"static/newmess3_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Write mail")
					);
					break;
				default:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img align=\"middle\" src=\"static/newmess3_24x.gif\" "
						"border=\"0\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Enter a message")
					);
					break;
			}
		}

		if (navbar_style == navbar_default) wprintf(
			"<li class=\"skipthisroom\">"
			"<a href=\"skip\" "
			"title=\"%s\">"
			"<img align=\"middle\" src=\"static/skipthisroom_24x.gif\" border=\"0\">"
			"<span class=\"navbar_link\">%s</span></a>"
			"</li>\n",
			_("Leave all messages marked as unread, go to next room with unread messages"),
			_("Skip this room")
		);

		if (navbar_style == navbar_default) wprintf(
			"<li class=\"markngo\">"
			"<a href=\"gotonext\" "
			"title=\"%s\">"
			"<img align=\"middle\" src=\"static/markngo_24x.gif\" border=\"0\">"
			"<span class=\"navbar_link\">%s</span></a>"
			"</li>\n",
			_("Mark all messages as read, go to next room with unread messages"),
			_("Goto next room")
		);

		wprintf("</ul></div>\n");
	}

}


/**
 * \brief back end routine to take the session to a new room
 * \param gname room to go to
 *
 */
int gotoroom(char *gname)
{
	char buf[SIZ];
	static long ls = (-1L);
	int err = 0;

	/** store ungoto information */
	strcpy(WC->ugname, WC->wc_roomname);
	WC->uglsn = ls;

	/** move to the new room */
	serv_printf("GOTO %s", gname);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		buf[3] = 0;
		err = atoi(buf);
		serv_puts("GOTO _BASEROOM_");
		serv_getln(buf, sizeof buf);
	}
	if (buf[0] != '2') {
		buf[3] = 0;
		err = atoi(buf);
		return err;
	}
	extract_token(WC->wc_roomname, &buf[4], 0, '|', sizeof WC->wc_roomname);
	WC->room_flags = extract_int(&buf[4], 4);
	/* highest_msg_read = extract_int(&buf[4],6);
	   maxmsgnum = extract_int(&buf[4],5);
	 */
	WC->is_mailbox = extract_int(&buf[4],7);
	ls = extract_long(&buf[4], 6);
	WC->wc_floor = extract_int(&buf[4], 10);
	WC->wc_view = extract_int(&buf[4], 11);
	WC->wc_default_view = extract_int(&buf[4], 12);
	WC->wc_is_trash = extract_int(&buf[4], 13);
	WC->room_flags2 = extract_int(&buf[4], 14);

	if (WC->is_aide)
		WC->is_room_aide = WC->is_aide;
	else
		WC->is_room_aide = (char) extract_int(&buf[4], 8);

	remove_march(WC->wc_roomname);
	if (!strcasecmp(gname, "_BASEROOM_"))
		remove_march(gname);

	return err;
}


/**
 * \brief Locate the room on the march list which we most want to go to.  
 * Each room
 * is measured given a "weight" of preference based on various factors.
 * \param desired_floor the room number on the citadel server
 * \return the roomname
 */
char *pop_march(int desired_floor)
{
	static char TheRoom[128];
	int TheFloor = 0;
	int TheOrder = 32767;
	int TheWeight = 0;
	int weight;
	struct march *mptr = NULL;

	strcpy(TheRoom, "_BASEROOM_");
	if (WC->march == NULL)
		return (TheRoom);

	for (mptr = WC->march; mptr != NULL; mptr = mptr->next) {
		weight = 0;
		if ((strcasecmp(mptr->march_name, "_BASEROOM_")))
			weight = weight + 10000;
		if (mptr->march_floor == desired_floor)
			weight = weight + 5000;

		weight = weight + ((128 - (mptr->march_floor)) * 128);
		weight = weight + (128 - (mptr->march_order));

		if (weight > TheWeight) {
			TheWeight = weight;
			strcpy(TheRoom, mptr->march_name);
			TheFloor = mptr->march_floor;
			TheOrder = mptr->march_order;
		}
	}
	return (TheRoom);
}



/**
 *\brief Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 * We start the search in the current room rather than the beginning to prevent
 * two or more concurrent users from dragging each other back to the same room.
 */
void gotonext(void)
{
	char buf[256];
	struct march *mptr, *mptr2;
	char room_name[128];
	char next_room[128];

	/**
	 * First check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */

	if (WC->march == NULL) {
		serv_puts("LKRN");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1')
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(room_name, buf, 0, '|', sizeof room_name);
				if (strcasecmp(room_name, WC->wc_roomname)) {
					mptr = (struct march *) malloc(sizeof(struct march));
					mptr->next = NULL;
					safestrncpy(mptr->march_name, room_name, sizeof mptr->march_name);
					mptr->march_floor = extract_int(buf, 2);
					mptr->march_order = extract_int(buf, 3);
					if (WC->march == NULL) {
						WC->march = mptr;
					} else {
						mptr2 = WC->march;
						while (mptr2->next != NULL)
							mptr2 = mptr2->next;
						mptr2->next = mptr;
					}
				}
			}
		/**
		 * add _BASEROOM_ to the end of the march list, so the user will end up
		 * in the system base room (usually the Lobby>) at the end of the loop
		 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		strcpy(mptr->march_name, "_BASEROOM_");
		if (WC->march == NULL) {
			WC->march = mptr;
		} else {
			mptr2 = WC->march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
		}
		/**
		 * ...and remove the room we're currently in, so a <G>oto doesn't make us
		 * walk around in circles
		 */
		remove_march(WC->wc_roomname);
	}
	if (WC->march != NULL) {
		strcpy(next_room, pop_march(-1));
	} else {
		strcpy(next_room, "_BASEROOM_");
	}


	smart_goto(next_room);
}


/**
 * \brief goto next room
 * \param next_room next room to go to
 */
void smart_goto(char *next_room) {
	gotoroom(next_room);
	readloop("readnew");
}



/**
 * \brief mark all messages in current room as having been read
 */
void slrp_highest(void)
{
	char buf[256];

	serv_puts("SLRP HIGHEST");
	serv_getln(buf, sizeof buf);
}


/**
 * \brief un-goto the previous room
 */
void ungoto(void)
{
	char buf[SIZ];

	if (!strcmp(WC->ugname, "")) {
		smart_goto(WC->wc_roomname);
		return;
	}
	serv_printf("GOTO %s", WC->ugname);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		smart_goto(WC->wc_roomname);
		return;
	}
	if (WC->uglsn >= 0L) {
		serv_printf("SLRP %ld", WC->uglsn);
		serv_getln(buf, sizeof buf);
	}
	strcpy(buf, WC->ugname);
	strcpy(WC->ugname, "");
	smart_goto(buf);
}





/**
 * \brief Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * \param newval set to 0 to clear, 1 to set, any other value to leave unchanged.
 * \return return the new value.
 */

int self_service(int newval) {
	int current_value = 0;
	char buf[SIZ];
	
	char name[SIZ];
	char password[SIZ];
	char dirname[SIZ];
	int flags, floor, order, view, flags2;

	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return(0);

	extract_token(name, &buf[4], 0, '|', sizeof name);
	extract_token(password, &buf[4], 1, '|', sizeof password);
	extract_token(dirname, &buf[4], 2, '|', sizeof dirname);
	flags = extract_int(&buf[4], 3);
	floor = extract_int(&buf[4], 4);
	order = extract_int(&buf[4], 5);
	view = extract_int(&buf[4], 6);
	flags2 = extract_int(&buf[4], 7);

	if (flags2 & QR2_SELFLIST) {
		current_value = 1;
	}
	else {
		current_value = 0;
	}

	if (newval == 1) {
		flags2 = flags2 | QR2_SELFLIST;
	}
	else if (newval == 0) {
		flags2 = flags2 & ~QR2_SELFLIST;
	}
	else {
		return(current_value);
	}

	if (newval != current_value) {
		serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
			name, password, dirname, flags,
			floor, order, view, flags2);
		serv_getln(buf, sizeof buf);
	}

	return(newval);

}






/**
 * \brief display the form for editing a room
 */
void display_editroom(void)
{
	char buf[SIZ];
	char cmd[1024];
	char node[256];
	char remote_room[128];
	char recp[1024];
	char er_name[128];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;
	unsigned er_flags2;
	int er_floor;
	int i, j;
	char *tab;
	char *shared_with;
	char *not_shared_with;
	int roompolicy = 0;
	int roomvalue = 0;
	int floorpolicy = 0;
	int floorvalue = 0;

	tab = bstr("tab");
	if (strlen(tab) == 0) tab = "admin";

	load_floorlist();
	serv_puts("GETR");
	serv_getln(buf, sizeof buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	extract_token(er_name, &buf[4], 0, '|', sizeof er_name);
	extract_token(er_password, &buf[4], 1, '|', sizeof er_password);
	extract_token(er_dirname, &buf[4], 2, '|', sizeof er_dirname);
	er_flags = extract_int(&buf[4], 3);
	er_floor = extract_int(&buf[4], 4);
	er_flags2 = extract_int(&buf[4], 7);

	output_headers(1, 1, 1, 0, 0, 0);

	/** print the tabbed dialog */
	wprintf("<br />"
		"<div class=\"fix_scrollbar_bug\">"
		"<TABLE border=0 cellspacing=0 cellpadding=0 width=100%%>"
		"<TR ALIGN=CENTER>"
		"<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "admin")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=admin\">");
	}
	wprintf(_("Administration"));
	if (!strcmp(tab, "admin")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "config")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=config\">");
	}
	wprintf(_("Configuration"));
	if (!strcmp(tab, "config")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "expire")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=expire\">");
	}
	wprintf(_("Message expire policy"));
	if (!strcmp(tab, "expire")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "access")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=access\">");
	}
	wprintf(_("Access controls"));
	if (!strcmp(tab, "access")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "sharing")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=sharing\">");
	}
	wprintf(_("Sharing"));
	if (!strcmp(tab, "sharing")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "listserv")) {
		wprintf("<TD class=\"roomops_cell_label\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD class=\"roomops_cell_edit\"><a href=\"display_editroom&tab=listserv\">");
	}
	wprintf(_("Mailing list service"));
	if (!strcmp(tab, "listserv")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	wprintf("</TR></TABLE></div>\n");
	/** end tabbed dialog */	

	/** begin content of whatever tab is open now */
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<TABLE class=\"roomops_background\">\n"
		"<TR><TD>\n");

	if (!strcmp(tab, "admin")) {
		wprintf("<UL>"
			"<LI><a href=\"delete_room\" "
			"onClick=\"return confirm('");
		wprintf(_("Are you sure you want to delete this room?"));
		wprintf("');\">\n");
		wprintf(_("Delete this room"));
		wprintf("</A>\n"
			"<LI><a href=\"display_editroompic\">\n");
		wprintf(_("Set or change the icon for this room's banner"));
		wprintf("</A>\n"
			"<LI><a href=\"display_editinfo\">\n");
		wprintf(_("Edit this room's Info file"));
		wprintf("</A>\n"
			"</UL>");
	}

	if (!strcmp(tab, "config")) {
		wprintf("<FORM METHOD=\"POST\" action=\"editroom\">\n");
	
		wprintf("<UL><LI>");
		wprintf(_("Name of room: "));
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\"%d\">\n",
			er_name,
			(sizeof(er_name)-1)
		);
	
		wprintf("<LI>");
		wprintf(_("Resides on floor: "));
		wprintf("<SELECT NAME=\"er_floor\" SIZE=\"1\">\n");
		for (i = 0; i < 128; ++i)
			if (strlen(floorlist[i]) > 0) {
				wprintf("<OPTION ");
				if (i == er_floor)
					wprintf("SELECTED ");
				wprintf("VALUE=\"%d\">", i);
				escputs(floorlist[i]);
				wprintf("</OPTION>\n");
			}
		wprintf("</SELECT>\n");
	
		wprintf("<LI>");
		wprintf(_("Type of room:"));
		wprintf("<UL>\n");

		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
		if ((er_flags & QR_PRIVATE) == 0)
		wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Public room"));
		wprintf("\n");

		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"hidden\" ");
		if ((er_flags & QR_PRIVATE) &&
		    (er_flags & QR_GUESSNAME))
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Private - guess name"));
	
		wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
		if ((er_flags & QR_PRIVATE) &&
		    (er_flags & QR_PASSWORDED))
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Private - require password:"));
		wprintf("\n<INPUT TYPE=\"text\" NAME=\"er_password\" VALUE=\"%s\" MAXLENGTH=\"9\">\n",
			er_password);
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
		if ((er_flags & QR_PRIVATE)
		    && ((er_flags & QR_GUESSNAME) == 0)
		    && ((er_flags & QR_PASSWORDED) == 0))
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Private - invitation only"));
	
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"bump\" VALUE=\"yes\" ");
		wprintf("> ");
		wprintf(_("If private, cause current users to forget room"));
	
		wprintf("\n</UL>\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"prefonly\" VALUE=\"yes\" ");
		if (er_flags & QR_PREFONLY)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Preferred users only"));
	
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"readonly\" VALUE=\"yes\" ");
		if (er_flags & QR_READONLY)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Read-only room"));
	
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"collabdel\" VALUE=\"yes\" ");
		if (er_flags2 & QR2_COLLABDEL)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("All users allowed to post may also delete messages"));
	
		/** directory stuff */
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"directory\" VALUE=\"yes\" ");
		if (er_flags & QR_DIRECTORY)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("File directory room"));

		wprintf("\n<UL><LI>");
		wprintf(_("Directory name: "));
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_dirname\" VALUE=\"%s\" MAXLENGTH=\"14\">\n",
			er_dirname);

		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"ulallowed\" VALUE=\"yes\" ");
		if (er_flags & QR_UPLOAD)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Uploading allowed"));
	
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"dlallowed\" VALUE=\"yes\" ");
		if (er_flags & QR_DOWNLOAD)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Downloading allowed"));
	
		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"visdir\" VALUE=\"yes\" ");
		if (er_flags & QR_VISDIR)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Visible directory"));
		wprintf("</UL>\n");
	
		/** end of directory stuff */
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"network\" VALUE=\"yes\" ");
		if (er_flags & QR_NETWORK)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Network shared room"));

		wprintf("\n<LI><INPUT TYPE=\"checkbox\" NAME=\"permanent\" VALUE=\"yes\" ");
		if (er_flags & QR_PERMANENT)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Permanent (does not auto-purge)"));

		/** start of anon options */
	
		wprintf("\n<LI>");
		wprintf(_("Anonymous messages"));
		wprintf("<UL>\n");
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"no\" ");
		if (((er_flags & QR_ANONONLY) == 0)
		    && ((er_flags & QR_ANONOPT) == 0))
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("No anonymous messages"));
	
		wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anononly\" ");
		if (er_flags & QR_ANONONLY)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("All messages are anonymous"));
	
		wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anon2\" ");
		if (er_flags & QR_ANONOPT)
			wprintf("CHECKED ");
		wprintf("> ");
		wprintf(_("Prompt user when entering messages"));
		wprintf("</UL>\n");
	
	/* end of anon options */
	
		wprintf("<LI>");
		wprintf(_("Room aide: "));
		serv_puts("GETA");
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			wprintf("<em>%s</em>\n", &buf[4]);
		} else {
			extract_token(er_roomaide, &buf[4], 0, '|', sizeof er_roomaide);
			wprintf("<INPUT TYPE=\"text\" NAME=\"er_roomaide\" VALUE=\"%s\" MAXLENGTH=\"25\">\n", er_roomaide);
		}
	
		wprintf("</UL><CENTER>\n");
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"config\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">"
			"&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">"
			"</CENTER>\n",
			_("Save changes"),
			_("Cancel")
		);
	}


	/** Sharing the room with other Citadel nodes... */
	if (!strcmp(tab, "sharing")) {

		shared_with = strdup("");
		not_shared_with = strdup("");

		/** Learn the current configuration */
		serv_puts("CONF getsys|application/x-citadel-ignet-config");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(node, buf, 0, '|', sizeof node);
			not_shared_with = realloc(not_shared_with,
					strlen(not_shared_with) + 32);
			strcat(not_shared_with, node);
			strcat(not_shared_with, "\n");
		}

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			extract_token(node, buf, 1, '|', sizeof node);
			extract_token(remote_room, buf, 2, '|', sizeof remote_room);
			if (!strcasecmp(cmd, "ignet_push_share")) {
				shared_with = realloc(shared_with,
						strlen(shared_with) + 32);
				strcat(shared_with, node);
				if (strlen(remote_room) > 0) {
					strcat(shared_with, "|");
					strcat(shared_with, remote_room);
				}
				strcat(shared_with, "\n");
			}
		}

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n', sizeof buf);
			extract_token(node, buf, 0, '|', sizeof node);
			for (j=0; j<num_tokens(not_shared_with, '\n'); ++j) {
				extract_token(cmd, not_shared_with, j, '\n', sizeof cmd);
				if (!strcasecmp(node, cmd)) {
					remove_token(not_shared_with, j, '\n');
				}
			}
		}

		/** Display the stuff */
		wprintf("<CENTER><br />"
			"<TABLE border=1 cellpadding=5><TR>"
			"<TD><B><I>");
		wprintf(_("Shared with"));
		wprintf("</I></B></TD>"
			"<TD><B><I>");
		wprintf(_("Not shared with"));
		wprintf("</I></B></TD></TR>\n"
			"<TR><TD VALIGN=TOP>\n");

		wprintf("<TABLE border=0 cellpadding=5><TR class=\"roomops_cell\"><TD>");
		wprintf(_("Remote node name"));
		wprintf("</TD><TD>");
		wprintf(_("Remote room name"));
		wprintf("</TD><TD>");
		wprintf(_("Actions"));
		wprintf("</TD></TR>\n");

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n', sizeof buf);
			extract_token(node, buf, 0, '|', sizeof node);
			extract_token(remote_room, buf, 1, '|', sizeof remote_room);
			if (strlen(node) > 0) {
				wprintf("<FORM METHOD=\"POST\" "
					"action=\"netedit\">"
					"<TR><TD>%s</TD>\n", node);

				wprintf("<TD>");
				if (strlen(remote_room) > 0) {
					escputs(remote_room);
				}
				wprintf("</TD>");

				wprintf("<TD>");
		
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				if (strlen(remote_room) > 0) {
					wprintf("|");
					urlescputs(remote_room);
				}
				wprintf("\">");
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" "
					"VALUE=\"sharing\">\n");
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"cmd\" "
					"VALUE=\"remove\">\n");
				wprintf("<INPUT TYPE=\"submit\" "
					"NAME=\"unshare_button\" VALUE=\"%s\">", _("Unshare"));
				wprintf("</TD></TR></FORM>\n");
			}
		}

		wprintf("</TABLE>\n");
		wprintf("</TD><TD VALIGN=TOP>\n");
		wprintf("<TABLE border=0 cellpadding=5><TR class=\"roomops_cell\"><TD>");
		wprintf(_("Remote node name"));
		wprintf("</TD><TD>");
		wprintf(_("Remote room name"));
		wprintf("</TD><TD>");
		wprintf(_("Actions"));
		wprintf("</TD></TR>\n");

		for (i=0; i<num_tokens(not_shared_with, '\n'); ++i) {
			extract_token(node, not_shared_with, i, '\n', sizeof node);
			if (strlen(node) > 0) {
				wprintf("<FORM METHOD=\"POST\" "
					"action=\"netedit\">"
					"<TR><TD>");
				escputs(node);
				wprintf("</TD><TD>"
					"<INPUT TYPE=\"INPUT\" "
					"NAME=\"suffix\" "
					"MAXLENGTH=128>"
					"</TD><TD>");
				wprintf("<INPUT TYPE=\"hidden\" "
					"NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				wprintf("|\">");
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" "
					"VALUE=\"sharing\">\n");
				wprintf("<INPUT TYPE=\"hidden\" NAME=\"cmd\" "
					"VALUE=\"add\">\n");
				wprintf("<INPUT TYPE=\"submit\" "
					"NAME=\"add_button\" VALUE=\"%s\">", _("Share"));
				wprintf("</TD></TR></FORM>\n");
			}
		}

		wprintf("</TABLE>\n");
		wprintf("</TD></TR>"
			"</TABLE></CENTER><br />\n"
			"<I><B>%s</B><UL><LI>", _("Notes:"));
		wprintf(_("When sharing a room, "
			"it must be shared from both ends.  Adding a node to "
			"the 'shared' list sends messages out, but in order to"
			" receive messages, the other nodes must be configured"
			" to send messages out to your system as well. "
			"<LI>If the remote room name is blank, it is assumed "
			"that the room name is identical on the remote node."
			"<LI>If the remote room name is different, the remote "
			"node must also configure the name of the room here."
			"</UL></I><br />\n"
		));

	}

	/** Mailing list management */
	if (!strcmp(tab, "listserv")) {

		wprintf("<br /><center>"
			"<TABLE BORDER=0 WIDTH=100%% CELLPADDING=5>"
			"<TR><TD VALIGN=TOP>");

		wprintf(_("<i>The contents of this room are being "
			"mailed <b>as individual messages</b> "
			"to the following list recipients:"
			"</i><br /><br />\n"));

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			if (!strcasecmp(cmd, "listrecp")) {
				extract_token(recp, buf, 1, '|', sizeof recp);
			
				escputs(recp);
				wprintf(" <a href=\"netedit&cmd=remove&line="
					"listrecp|");
				urlescputs(recp);
				wprintf("&tab=listserv\">");
				wprintf(_("(remove)"));
				wprintf("</A><br />");
			}
		}
		wprintf("<br /><FORM METHOD=\"POST\" action=\"netedit\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"prefix\" VALUE=\"listrecp|\">\n");
		wprintf("<INPUT TYPE=\"text\" NAME=\"line\">\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wprintf("</FORM>\n");

		wprintf("</TD><TD VALIGN=TOP>\n");
		
		wprintf(_("<i>The contents of this room are being "
			"mailed <b>in digest form</b> "
			"to the following list recipients:"
			"</i><br /><br />\n"));

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			if (!strcasecmp(cmd, "digestrecp")) {
				extract_token(recp, buf, 1, '|', sizeof recp);
			
				escputs(recp);
				wprintf(" <a href=\"netedit&cmd=remove&line="
					"digestrecp|");
				urlescputs(recp);
				wprintf("&tab=listserv\">");
				wprintf(_("(remove)"));
				wprintf("</A><br />");
			}
		}
		wprintf("<br /><FORM METHOD=\"POST\" action=\"netedit\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"prefix\" VALUE=\"digestrecp|\">\n");
		wprintf("<INPUT TYPE=\"text\" NAME=\"line\">\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wprintf("</FORM>\n");
		
		wprintf("</TD></TR></TABLE><hr />\n");

		if (self_service(999) == 1) {
			wprintf(_("This room is configured to allow "
				"self-service subscribe/unsubscribe requests."));
			wprintf("<a href=\"toggle_self_service?newval=0&tab=listserv\">");
			wprintf(_("Click to disable."));
			wprintf("</A><br />\n");
			wprintf(_("The URL for subscribe/unsubscribe is: "));
			wprintf("<TT>%s://%s/listsub</TT><br />\n",
				(is_https ? "https" : "http"),
				WC->http_host);
		}
		else {
			wprintf(_("This room is <i>not</i> configured to allow "
				"self-service subscribe/unsubscribe requests."));
			wprintf(" <a href=\"toggle_self_service?newval=1&"
				"tab=listserv\">");
			wprintf(_("Click to enable."));
			wprintf("</A><br />\n");
		}


		wprintf("</CENTER>\n");
	}


	/** Mailing list management */
	if (!strcmp(tab, "expire")) {

		serv_puts("GPEX room");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			roompolicy = extract_int(&buf[4], 0);
			roomvalue = extract_int(&buf[4], 1);
		}
		
		serv_puts("GPEX floor");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			floorpolicy = extract_int(&buf[4], 0);
			floorvalue = extract_int(&buf[4], 1);
		}
		
		wprintf("<br /><FORM METHOD=\"POST\" action=\"set_room_policy\">\n");
		wprintf("<TABLE border=0 cellspacing=5>\n");
		wprintf("<TR><TD>");
		wprintf(_("Message expire policy for this room"));
		wprintf("<br />(");
		escputs(WC->wc_roomname);
		wprintf(")</TD><TD>");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"0\" %s>",
			((roompolicy == 0) ? "CHECKED" : "") );
		wprintf(_("Use the default policy for this floor"));
		wprintf("<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"1\" %s>",
			((roompolicy == 1) ? "CHECKED" : "") );
		wprintf(_("Never automatically expire messages"));
		wprintf("<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"2\" %s>",
			((roompolicy == 2) ? "CHECKED" : "") );
		wprintf(_("Expire by message count"));
		wprintf("<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"3\" %s>",
			((roompolicy == 3) ? "CHECKED" : "") );
		wprintf(_("Expire by message age"));
		wprintf("<br />");
		wprintf(_("Number of messages or days: "));
		wprintf("<INPUT TYPE=\"text\" NAME=\"roomvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", roomvalue);
		wprintf("</TD></TR>\n");

		if (WC->axlevel >= 6) {
			wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");
			wprintf("<TR><TD>");
			wprintf(_("Message expire policy for this floor"));
			wprintf("<br />(");
			escputs(floorlist[WC->wc_floor]);
			wprintf(")</TD><TD>");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"0\" %s>",
				((floorpolicy == 0) ? "CHECKED" : "") );
			wprintf(_("Use the system default"));
			wprintf("<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"1\" %s>",
				((floorpolicy == 1) ? "CHECKED" : "") );
			wprintf(_("Never automatically expire messages"));
			wprintf("<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"2\" %s>",
				((floorpolicy == 2) ? "CHECKED" : "") );
			wprintf(_("Expire by message count"));
			wprintf("<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"3\" %s>",
				((floorpolicy == 3) ? "CHECKED" : "") );
			wprintf(_("Expire by message age"));
			wprintf("<br />");
			wprintf(_("Number of messages or days: "));
			wprintf("<INPUT TYPE=\"text\" NAME=\"floorvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">",
				floorvalue);
		}

		wprintf("<CENTER>\n");
		wprintf("<TR><TD COLSPAN=2><hr /><CENTER>\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Save changes"));
		wprintf("&nbsp;");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
		wprintf("</CENTER></TD><TR>\n");

		wprintf("</TABLE>\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"expire\">\n"
			"</FORM>\n"
		);

	}

	/** Mailing list management */
	if (!strcmp(tab, "access")) {
		display_whok();
	}

	/** end content of whatever tab is open now */
	wprintf("</TD></TR></TABLE></div>\n");

	wDumpContent(1);
}


/** 
 * \brief Toggle self-service list subscription
 */
void toggle_self_service(void) {
	int newval = 0;

	newval = atoi(bstr("newval"));
	self_service(newval);
	display_editroom();
}



/**
 * \brief save new parameters for a room
 */
void editroom(void)
{
	char buf[SIZ];
	char er_name[128];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	int er_floor;
	unsigned er_flags;
	int er_listingorder;
	int er_defaultview;
	unsigned er_flags2;
	int bump;


	if (strlen(bstr("ok_button")) == 0) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  Changes were not saved."));
		display_editroom();
		return;
	}
	serv_puts("GETR");
	serv_getln(buf, sizeof buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_editroom();
		return;
	}
	extract_token(er_name, &buf[4], 0, '|', sizeof er_name);
	extract_token(er_password, &buf[4], 1, '|', sizeof er_password);
	extract_token(er_dirname, &buf[4], 2, '|', sizeof er_dirname);
	er_flags = extract_int(&buf[4], 3);
	er_listingorder = extract_int(&buf[4], 5);
	er_defaultview = extract_int(&buf[4], 6);
	er_flags2 = extract_int(&buf[4], 7);

	strcpy(er_roomaide, bstr("er_roomaide"));
	if (strlen(er_roomaide) == 0) {
		serv_puts("GETA");
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			strcpy(er_roomaide, "");
		} else {
			extract_token(er_roomaide, &buf[4], 0, '|', sizeof er_roomaide);
		}
	}
	strcpy(buf, bstr("er_name"));
	buf[128] = 0;
	if (strlen(buf) > 0) {
		strcpy(er_name, buf);
	}

	strcpy(buf, bstr("er_password"));
	buf[10] = 0;
	if (strlen(buf) > 0)
		strcpy(er_password, buf);

	strcpy(buf, bstr("er_dirname"));
	buf[15] = 0;
	if (strlen(buf) > 0)
		strcpy(er_dirname, buf);

	strcpy(buf, bstr("type"));
	er_flags &= !(QR_PRIVATE | QR_PASSWORDED | QR_GUESSNAME);

	if (!strcmp(buf, "invonly")) {
		er_flags |= (QR_PRIVATE);
	}
	if (!strcmp(buf, "hidden")) {
		er_flags |= (QR_PRIVATE | QR_GUESSNAME);
	}
	if (!strcmp(buf, "passworded")) {
		er_flags |= (QR_PRIVATE | QR_PASSWORDED);
	}
	if (!strcmp(bstr("prefonly"), "yes")) {
		er_flags |= QR_PREFONLY;
	} else {
		er_flags &= ~QR_PREFONLY;
	}

	if (!strcmp(bstr("readonly"), "yes")) {
		er_flags |= QR_READONLY;
	} else {
		er_flags &= ~QR_READONLY;
	}

	if (!strcmp(bstr("collabdel"), "yes")) {
		er_flags2 |= QR2_COLLABDEL;
	} else {
		er_flags2 &= ~QR2_COLLABDEL;
	}

	if (!strcmp(bstr("permanent"), "yes")) {
		er_flags |= QR_PERMANENT;
	} else {
		er_flags &= ~QR_PERMANENT;
	}

	if (!strcmp(bstr("network"), "yes")) {
		er_flags |= QR_NETWORK;
	} else {
		er_flags &= ~QR_NETWORK;
	}

	if (!strcmp(bstr("directory"), "yes")) {
		er_flags |= QR_DIRECTORY;
	} else {
		er_flags &= ~QR_DIRECTORY;
	}

	if (!strcmp(bstr("ulallowed"), "yes")) {
		er_flags |= QR_UPLOAD;
	} else {
		er_flags &= ~QR_UPLOAD;
	}

	if (!strcmp(bstr("dlallowed"), "yes")) {
		er_flags |= QR_DOWNLOAD;
	} else {
		er_flags &= ~QR_DOWNLOAD;
	}

	if (!strcmp(bstr("visdir"), "yes")) {
		er_flags |= QR_VISDIR;
	} else {
		er_flags &= ~QR_VISDIR;
	}

	strcpy(buf, bstr("anon"));

	er_flags &= ~(QR_ANONONLY | QR_ANONOPT);
	if (!strcmp(buf, "anononly"))
		er_flags |= QR_ANONONLY;
	if (!strcmp(buf, "anon2"))
		er_flags |= QR_ANONOPT;

	bump = 0;
	if (!strcmp(bstr("bump"), "yes"))
		bump = 1;

	er_floor = atoi(bstr("er_floor"));

	sprintf(buf, "SETR %s|%s|%s|%u|%d|%d|%d|%d|%u",
		er_name, er_password, er_dirname, er_flags, bump, er_floor,
		er_listingorder, er_defaultview, er_flags2);
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_editroom();
		return;
	}
	gotoroom(er_name);

	if (strlen(er_roomaide) > 0) {
		sprintf(buf, "SETA %s", er_roomaide);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			strcpy(WC->ImportantMessage, &buf[4]);
			display_main_menu();
			return;
		}
	}
	gotoroom(er_name);
	strcpy(WC->ImportantMessage, _("Your changes have been saved."));
	display_editroom();
	return;
}


/**
 * \brief Display form for Invite, Kick, and show Who Knows a room
 */
void do_invt_kick(void) {
        char buf[SIZ], room[SIZ], username[SIZ];

        serv_puts("GETR");
        serv_getln(buf, sizeof buf);

        if (buf[0] != '2') {
		escputs(&buf[4]);
		return;
        }
        extract_token(room, &buf[4], 0, '|', sizeof room);

        strcpy(username, bstr("username"));

        if (strlen(bstr("kick_button")) > 0) {
                sprintf(buf, "KICK %s", username);
                serv_puts(buf);
                serv_getln(buf, sizeof buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
				_("<B><I>User %s kicked out of room %s.</I></B>\n"), 
                                username, room);
                }
        }

	if (strlen(bstr("invite_button")) > 0) {
                sprintf(buf, "INVT %s", username);
                serv_puts(buf);
                serv_getln(buf, sizeof buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
                        	_("<B><I>User %s invited to room %s.</I></B>\n"), 
                                username, room);
                }
        }

	display_editroom();
}



/**
 * \brief Display form for Invite, Kick, and show Who Knows a room
 */
void display_whok(void)
{
        char buf[SIZ], room[SIZ], username[SIZ];

        serv_puts("GETR");
        serv_getln(buf, sizeof buf);

        if (buf[0] != '2') {
		escputs(&buf[4]);
		return;
        }
        extract_token(room, &buf[4], 0, '|', sizeof room);

        
	wprintf("<TABLE border=0 CELLSPACING=10><TR VALIGN=TOP><TD>");
	wprintf(_("The users listed below have access to this room.  "
		"To remove a user from the access list, select the user "
		"name from the list and click 'Kick'."));
	wprintf("<br /><br />");
	
        wprintf("<CENTER><FORM METHOD=\"POST\" action=\"do_invt_kick\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
        wprintf("<SELECT NAME=\"username\" SIZE=\"10\" style=\"width:100%%\">\n");
        serv_puts("WHOK");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(username, buf, 0, '|', sizeof username);
                        wprintf("<OPTION>");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</SELECT><br />\n");

        wprintf("<input type=\"submit\" name=\"kick_button\" value=\"%s\">", _("Kick"));
        wprintf("</FORM></CENTER>\n");

	wprintf("</TD><TD>");
	wprintf(_("To grant another user access to this room, enter the "
		"user name in the box below and click 'Invite'."));
	wprintf("<br /><br />");

        wprintf("<CENTER><FORM METHOD=\"POST\" action=\"do_invt_kick\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
        wprintf(_("Invite:"));
	wprintf(" ");
        wprintf("<input type=\"text\" name=\"username\" style=\"width:100%%\"><br />\n"
        	"<input type=\"hidden\" name=\"invite_button\" value=\"Invite\">"
        	"<input type=\"submit\" value=\"%s\">"
		"</FORM></CENTER>\n", _("Invite"));

	wprintf("</TD></TR></TABLE>\n");
        wDumpContent(1);
}



/**
 * \brief display the form for entering a new room
 */
void display_entroom(void)
{
	int i;
	char buf[SIZ];

	serv_puts("CRE8 0");
	serv_getln(buf, sizeof buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE class=\"roomops_banner\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Create a new room"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"roomops_background\"><tr><td>\n");

	wprintf("<form name=\"create_room_form\" method=\"POST\" action=\"entroom\">\n");

	wprintf("<UL><LI>");
	wprintf(_("Name of room: "));
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" MAXLENGTH=\"127\">\n");

        wprintf("<LI>");
	wprintf(_("Resides on floor: "));
        load_floorlist(); 
        wprintf("<SELECT NAME=\"er_floor\" SIZE=\"1\">\n");
        for (i = 0; i < 128; ++i)
                if (strlen(floorlist[i]) > 0) {
                        wprintf("<OPTION ");
                        wprintf("VALUE=\"%d\">", i);
                        escputs(floorlist[i]);
                        wprintf("</OPTION>\n");
                }
        wprintf("</SELECT>\n");

		/**
		 * Our clever little snippet of JavaScript automatically selects
		 * a public room if the view is set to Bulletin Board or wiki, and
		 * it selects a mailbox room otherwise.  The user can override this,
		 * of course.  We also disable the floor selector for mailboxes.
		 */
		wprintf("<LI>");
		wprintf(_("Default view for room: "));
        wprintf("<SELECT NAME=\"er_view\" SIZE=\"1\" OnChange=\""
		"	if ( (this.form.er_view.value == 0)		"
		"	   || (this.form.er_view.value == 6) ) {	"
		"		this.form.type[0].checked=true;		"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"	else {						"
		"		this.form.type[4].checked=true;		"
		"		this.form.er_floor.disabled = true;	"
		"	}						"
		"\">\n");
	for (i=0; i<(sizeof viewdefs / sizeof (char *)); ++i) {
		if (is_view_allowed_as_default(i)) {
			wprintf("<OPTION %s VALUE=\"%d\">",
				((i == 0) ? "SELECTED" : ""), i );
			escputs(viewdefs[i]);
			wprintf("</OPTION>\n");
		}
	}
	wprintf("</SELECT>\n");

	wprintf("<LI>");
	wprintf(_("Type of room:"));
	wprintf("<UL>\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
	wprintf("CHECKED OnChange=\""
		"	if (this.form.type[0].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Public (automatically appears to everyone)"));

	wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"hidden\" OnChange=\""
		"	if (this.form.type[1].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - hidden (accessible to anyone who knows its name)"));

	wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" OnChange=\""
		"	if (this.form.type[2].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - require password: "));
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_password\" MAXLENGTH=\"9\">\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" OnChange=\""
		"	if (this.form.type[3].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - invitation only"));

	wprintf("\n<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"personal\" "
		"OnChange=\""
		"	if (this.form.type[4].checked == true) {	"
		"		this.form.er_floor.disabled = true;	"
		"	}						"
		"\"> ");
	wprintf(_("Personal (mailbox for you only)"));

	wprintf("\n</UL>\n");

	wprintf("<CENTER>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Create new room"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wprintf("</CENTER>\n");
	wprintf("</FORM>\n<hr />");
	serv_printf("MESG roomaccess");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("CENTER");
	}
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}




/**
 * \brief support function for entroom() -- sets the default view 
 */
void er_set_default_view(int newview) {

	char buf[SIZ];

	char rm_name[SIZ];
	char rm_pass[SIZ];
	char rm_dir[SIZ];
	int rm_bits1;
	int rm_floor;
	int rm_listorder;
	int rm_bits2;

	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return;

	extract_token(rm_name, &buf[4], 0, '|', sizeof rm_name);
	extract_token(rm_pass, &buf[4], 1, '|', sizeof rm_pass);
	extract_token(rm_dir, &buf[4], 2, '|', sizeof rm_dir);
	rm_bits1 = extract_int(&buf[4], 3);
	rm_floor = extract_int(&buf[4], 4);
	rm_listorder = extract_int(&buf[4], 5);
	rm_bits2 = extract_int(&buf[4], 7);

	serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
		rm_name, rm_pass, rm_dir, rm_bits1, rm_floor,
		rm_listorder, newview, rm_bits2
	);
	serv_getln(buf, sizeof buf);
}



/**
 * \brief enter a new room
 */
void entroom(void)
{
	char buf[SIZ];
	char er_name[SIZ];
	char er_type[SIZ];
	char er_password[SIZ];
	int er_floor;
	int er_num_type;
	int er_view;

	if (strlen(bstr("ok_button")) == 0) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  No new room was created."));
		display_main_menu();
		return;
	}
	strcpy(er_name, bstr("er_name"));
	strcpy(er_type, bstr("type"));
	strcpy(er_password, bstr("er_password"));
	er_floor = atoi(bstr("er_floor"));
	er_view = atoi(bstr("er_view"));

	er_num_type = 0;
	if (!strcmp(er_type, "hidden"))
		er_num_type = 1;
	if (!strcmp(er_type, "passworded"))
		er_num_type = 2;
	if (!strcmp(er_type, "invonly"))
		er_num_type = 3;
	if (!strcmp(er_type, "personal"))
		er_num_type = 4;

	sprintf(buf, "CRE8 1|%s|%d|%s|%d|%d|%d", 
		er_name, er_num_type, er_password, er_floor, 0, er_view);
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	gotoroom(er_name);
	do_change_view(er_view);		/* Now go there */
}


/**
 * \brief display the screen to enter a private room
 */
void display_private(char *rname, int req_pass)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE class=\"roomops_banner\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Go to a hidden room"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"roomops_background\"><tr><td>\n");

	wprintf("<CENTER>\n");
	wprintf("<br />");
	wprintf(_("If you know the name of a hidden (guess-name) or "
		"passworded room, you can enter that room by typing "
		"its name below.  Once you gain access to a private "
		"room, it will appear in your regular room listings "
		"so you don't have to keep returning here."));
	wprintf("\n<br /><br />");

	wprintf("<FORM METHOD=\"POST\" action=\"goto_private\">\n");

	wprintf("<table border=\"0\" cellspacing=\"5\" "
		"cellpadding=\"5\" class=\"roomops_background_alt\">\n"
		"<TR><TD>");
	wprintf(_("Enter room name:"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"gr_name\" "
		"VALUE=\"%s\" MAXLENGTH=\"128\">\n", rname);

	if (req_pass) {
		wprintf("</TD></TR><TR><TD>");
		wprintf(_("Enter room password:"));
		wprintf("</TD><TD>");
		wprintf("<INPUT TYPE=\"password\" NAME=\"gr_pass\" MAXLENGTH=\"9\">\n");
	}
	wprintf("</TD></TR></TABLE><br />\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">",
		_("Go there"),
		_("Cancel")
	);
	wprintf("</FORM>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/**
 * \brief goto a private room
 */
void goto_private(void)
{
	char hold_rm[SIZ];
	char buf[SIZ];

	if (strlen(bstr("ok_button")) == 0) {
		display_main_menu();
		return;
	}
	strcpy(hold_rm, WC->wc_roomname);
	strcpy(buf, "GOTO ");
	strcat(buf, bstr("gr_name"));
	strcat(buf, "|");
	strcat(buf, bstr("gr_pass"));
	serv_puts(buf);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		smart_goto(bstr("gr_name"));
		return;
	}
	if (!strncmp(buf, "540", 3)) {
		display_private(bstr("gr_name"), 1);
		return;
	}
	output_headers(1, 1, 1, 0, 0, 0);
	wprintf("%s\n", &buf[4]);
	wDumpContent(1);
	return;
}


/**
 * \brief display the screen to zap a room
 */
void display_zap(void)
{
	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE class=\"roomops_zap\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Zap (forget/unsubscribe) the current room"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf(_("If you select this option, <em>%s</em> will "
		"disappear from your room list.  Is this what you wish "
		"to do?<br />\n"), WC->wc_roomname);

	wprintf("<FORM METHOD=\"POST\" action=\"zap\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Zap this room"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wprintf("</FORM>\n");
	wDumpContent(1);
}


/**
 * \brief zap a room
 */
void zap(void)
{
	char buf[SIZ];
	char final_destination[SIZ];

	/**
	 * If the forget-room routine fails for any reason, we fall back
	 * to the current room; otherwise, we go to the Lobby
	 */
	strcpy(final_destination, WC->wc_roomname);

	if (strlen(bstr("ok_button")) > 0) {
		serv_printf("GOTO %s", WC->wc_roomname);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			serv_puts("FORG");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				strcpy(final_destination, "_BASEROOM_");
			}
		}
	}
	smart_goto(final_destination);
}



/**
 * \brief Delete the current room
 */
void delete_room(void)
{
	char buf[SIZ];

	serv_puts("KILL 1");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	} else {
		smart_goto("_BASEROOM_");
	}
}



/**
 * \brief Perform changes to a room's network configuration
 */
void netedit(void) {
	FILE *fp;
	char buf[SIZ];
	char line[SIZ];
	char cmpa0[SIZ];
	char cmpa1[SIZ];
	char cmpb0[SIZ];
	char cmpb1[SIZ];

	if (strlen(bstr("line"))==0) {
		display_editroom();
		return;
	}

	strcpy(line, bstr("prefix"));
	strcat(line, bstr("line"));
	strcat(line, bstr("suffix"));

	fp = tmpfile();
	if (fp == NULL) {
		display_editroom();
		return;
	}

	serv_puts("GNET");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		fclose(fp);
		display_editroom();
		return;
	}

	/** This loop works for add *or* remove.  Spiffy, eh? */
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(cmpa0, buf, 0, '|', sizeof cmpa0);
		extract_token(cmpa1, buf, 1, '|', sizeof cmpa1);
		extract_token(cmpb0, line, 0, '|', sizeof cmpb0);
		extract_token(cmpb1, line, 1, '|', sizeof cmpb1);
		if ( (strcasecmp(cmpa0, cmpb0)) 
		   || (strcasecmp(cmpa1, cmpb1)) ) {
			fprintf(fp, "%s\n", buf);
		}
	}

	rewind(fp);
	serv_puts("SNET");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		fclose(fp);
		display_editroom();
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		serv_puts(buf);
	}

	if (strlen(bstr("add_button")) > 0) {
		serv_puts(line);
	}

	serv_puts("000");
	fclose(fp);
	display_editroom();
}



/**
 * \brief Convert a room name to a folder-ish-looking name.
 * \param folder the folderish name
 * \param room the room name
 * \param floor the floor name
 * \param is_mailbox is it a mailbox?
 */
void room_to_folder(char *folder, char *room, int floor, int is_mailbox)
{
	int i;

	/**
	 * For mailboxes, just do it straight...
	 */
	if (is_mailbox) {
		sprintf(folder, "My folders|%s", room);
	}

	/**
	 * Otherwise, prefix the floor name as a "public folders" moniker
	 */
	else {
		sprintf(folder, "%s|%s", floorlist[floor], room);
	}

	/**
	 * Replace "\" characters with "|" for pseudo-folder-delimiting
	 */
	for (i=0; i<strlen(folder); ++i) {
		if (folder[i] == '\\') folder[i] = '|';
	}
}




/**
 * \brief Back end for change_view()
 * \param newview set newview???
 */
void do_change_view(int newview) {
	char buf[SIZ];

	serv_printf("VIEW %d", newview);
	serv_getln(buf, sizeof buf);
	WC->wc_view = newview;
	smart_goto(WC->wc_roomname);
}



/**
 * \brief Change the view for this room
 */
void change_view(void) {
	int view;

	view = atol(bstr("view"));
	do_change_view(view);
}


/**
 * \brief One big expanded tree list view --- like a folder list
 * \param fold the folder to view
 * \param max_folders how many folders???
 * \param num_floors hom many floors???
 */
void do_folder_view(struct folder *fold, int max_folders, int num_floors) {
	char buf[SIZ];
	int levels;
	int i;
	int has_subfolders = 0;
	int *parents;

	parents = malloc(max_folders * sizeof(int));

	/** BEGIN TREE MENU */
	wprintf("<div id=\"roomlist_div\">Loading folder list...</div>\n");

	/** include NanoTree */
	wprintf("<script type=\"text/javascript\" src=\"static/nanotree.js\"></script>\n");

	/** initialize NanoTree */
	wprintf("<script type=\"text/javascript\">			\n"
		"	showRootNode = false;				\n"
		"	sortNodes = false;				\n"
		"	dragable = false;				\n"
		"							\n"
		"	function standardClick(treeNode) {		\n"
		"	}						\n"
		"							\n"
		"	var closedGif = 'static/folder_closed.gif';	\n"
		"	var openGif = 'static/folder_open.gif';		\n"
		"							\n"
		"	rootNode = new TreeNode(1, 'root node - hide');	\n"
	);

	levels = 0;
	for (i=0; i<max_folders; ++i) {

		has_subfolders = 0;
		if ((i+1) < max_folders) {
			if ( (!strncasecmp(fold[i].name, fold[i+1].name, strlen(fold[i].name)))
			   && (fold[i+1].name[strlen(fold[i].name)] == '|') ) {
				has_subfolders = 1;
			}
		}

		levels = num_tokens(fold[i].name, '|');
		parents[levels] = i;

		wprintf("var node%d = new TreeNode(%d, '", i, i);

		if (fold[i].selectable) {
			wprintf("<a href=\"dotgoto?room=");
			urlescputs(fold[i].room);
			wprintf("\">");
		}

		if (levels == 1) {
			wprintf("<SPAN CLASS=\"roomlist_floor\">");
		}
		else if (fold[i].hasnewmsgs) {
			wprintf("<SPAN CLASS=\"roomlist_new\">");
		}
		else {
			wprintf("<SPAN CLASS=\"roomlist_old\">");
		}
		extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
		escputs(buf);
		wprintf("</SPAN>");

		wprintf("</a>', ");
		if (has_subfolders) {
			wprintf("new Array(closedGif, openGif)");
		}
		else if (fold[i].view == VIEW_ADDRESSBOOK) {
			wprintf("'static/viewcontacts_16x.gif'");
		}
		else if (fold[i].view == VIEW_CALENDAR) {
			wprintf("'static/calarea_16x.gif'");
		}
		else if (fold[i].view == VIEW_CALBRIEF) {
			wprintf("'static/calarea_16x.gif'");
		}
		else if (fold[i].view == VIEW_TASKS) {
			wprintf("'static/taskmanag_16x.gif'");
		}
		else if (fold[i].view == VIEW_NOTES) {
			wprintf("'static/storenotes_16x.gif'");
		}
		else if (fold[i].view == VIEW_MAILBOX) {
			wprintf("'static/privatemess_16x.gif'");
		}
		else {
			wprintf("'static/chatrooms_16x.gif'");
		}
		wprintf(", '");
		urlescputs(fold[i].name);
		wprintf("');\n");

		if (levels < 2) {
			wprintf("rootNode.addChild(node%d);\n", i);
		}
		else {
			wprintf("node%d.addChild(node%d);\n", parents[levels-1], i);
		}
	}

	wprintf("container = document.getElementById('roomlist_div');	\n"
		"showTree('');	\n"
		"</script>\n"
	);

	free(parents);
	/** END TREE MENU */
}

/**
 * \brief Boxes and rooms and lists ... oh my!
 * \param fold the folder to view
 * \param max_folders how many folders???
 * \param num_floors hom many floors???
 */
void do_rooms_view(struct folder *fold, int max_folders, int num_floors) {
	char buf[256];
	char floor_name[256];
	char old_floor_name[256];
	char boxtitle[256];
	int levels, oldlevels;
	int i, t;
	int num_boxes = 0;
	static int columns = 3;
	int boxes_per_column = 0;
	int current_column = 0;
	int nf;

	strcpy(floor_name, "");
	strcpy(old_floor_name, "");

	nf = num_floors;
	while (nf % columns != 0) ++nf;
	boxes_per_column = (nf / columns);
	if (boxes_per_column < 1) boxes_per_column = 1;

	/** Outer table (for columnization) */
	wprintf("<TABLE BORDER=0 WIDTH=96%% CELLPADDING=5>"
		"<tr><td valign=top>");

	levels = 0;
	oldlevels = 0;
	for (i=0; i<max_folders; ++i) {

		levels = num_tokens(fold[i].name, '|');
		extract_token(floor_name, fold[i].name, 0,
			'|', sizeof floor_name);

		if ( (strcasecmp(floor_name, old_floor_name))
		   && (strlen(old_floor_name) > 0) ) {
			/* End inner box */
			do_template("endbox");

			++num_boxes;
			if ((num_boxes % boxes_per_column) == 0) {
				++current_column;
				if (current_column < columns) {
					wprintf("</td><td valign=top>\n");
				}
			}
		}
		strcpy(old_floor_name, floor_name);

		if (levels == 1) {
			/** Begin inner box */
			stresc(boxtitle, floor_name, 1, 0);
			svprintf("BOXTITLE", WCS_STRING, boxtitle);
			do_template("beginbox");
		}

		oldlevels = levels;

		if (levels > 1) {
			wprintf("&nbsp;");
			if (levels>2) for (t=0; t<(levels-2); ++t) wprintf("&nbsp;&nbsp;&nbsp;");
			if (fold[i].selectable) {
				wprintf("<a href=\"dotgoto?room=");
				urlescputs(fold[i].room);
				wprintf("\">");
			}
			else {
				wprintf("<i>");
			}
			if (fold[i].hasnewmsgs) {
				wprintf("<SPAN CLASS=\"roomlist_new\">");
			}
			else {
				wprintf("<SPAN CLASS=\"roomlist_old\">");
			}
			extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
			escputs(buf);
			wprintf("</SPAN>");
			if (fold[i].selectable) {
				wprintf("</A>");
			}
			else {
				wprintf("</i>");
			}
			if (!strcasecmp(fold[i].name, "My Folders|Mail")) {
				wprintf(" (INBOX)");
			}
			wprintf("<br />\n");
		}
	}
	/** End the final inner box */
	do_template("endbox");

	wprintf("</TD></TR></TABLE>\n");
}

/**
 * \brief print a floor div???
 * \param which_floordiv name of the floordiv???
 */
void set_floordiv_expanded(char *which_floordiv) {
	begin_ajax_response();
	safestrncpy(WC->floordiv_expanded, which_floordiv, sizeof WC->floordiv_expanded);
	end_ajax_response();
}

/**
 * \brief view the iconbar
 * \param fold the folder to view
 * \param max_folders how many folders???
 * \param num_floors hom many floors???
 */
void do_iconbar_view(struct folder *fold, int max_folders, int num_floors) {
	char buf[256];
	char floor_name[256];
	char old_floor_name[256];
	char floordivtitle[256];
	char floordiv_id[32];
	int levels, oldlevels;
	int i, t;
	int num_drop_targets = 0;
	char *icon = NULL;

	strcpy(floor_name, "");
	strcpy(old_floor_name, "");

	levels = 0;
	oldlevels = 0;
	for (i=0; i<max_folders; ++i) {

		levels = num_tokens(fold[i].name, '|');
		extract_token(floor_name, fold[i].name, 0,
			'|', sizeof floor_name);

		if ( (strcasecmp(floor_name, old_floor_name))
		   && (strlen(old_floor_name) > 0) ) {
			/** End inner box */
			wprintf("<br>\n");
			wprintf("</div>\n");	/** floordiv */
		}
		strcpy(old_floor_name, floor_name);

		if (levels == 1) {
			/** Begin floor */
			stresc(floordivtitle, floor_name, 0, 0);
			sprintf(floordiv_id, "floordiv%d", i);
			wprintf("<span class=\"ib_roomlist_floor\" "
				"onClick=\"expand_floor('%s')\">"
				"%s</span><br>\n", floordiv_id, floordivtitle);
			wprintf("<div id=\"%s\" style=\"display:%s\">",
				floordiv_id,
				(!strcasecmp(floordiv_id, WC->floordiv_expanded) ? "block" : "none")
			);
		}

		oldlevels = levels;

		if (levels > 1) {
			wprintf("<div id=\"roomdiv%d\">", i);
			wprintf("&nbsp;");
			if (levels>2) for (t=0; t<(levels-2); ++t) wprintf("&nbsp;");

			/** choose the icon */
			if (fold[i].view == VIEW_ADDRESSBOOK) {
				icon = "viewcontacts_16x.gif" ;
			}
			else if (fold[i].view == VIEW_CALENDAR) {
				icon = "calarea_16x.gif" ;
			}
			else if (fold[i].view == VIEW_CALBRIEF) {
				icon = "calarea_16x.gif" ;
			}
			else if (fold[i].view == VIEW_TASKS) {
				icon = "taskmanag_16x.gif" ;
			}
			else if (fold[i].view == VIEW_NOTES) {
				icon = "storenotes_16x.gif" ;
			}
			else if (fold[i].view == VIEW_MAILBOX) {
				icon = "privatemess_16x.gif" ;
			}
			else {
				icon = "chatrooms_16x.gif" ;
			}

			if (fold[i].selectable) {
				wprintf("<a href=\"dotgoto?room=");
				urlescputs(fold[i].room);
				wprintf("\">");
				wprintf("<img align=\"middle\" border=0 src=\"static/%s\" alt=\"\"> ", icon);
			}
			else {
				wprintf("<i>");
			}
			if (fold[i].hasnewmsgs) {
				wprintf("<SPAN CLASS=\"ib_roomlist_new\">");
			}
			else {
				wprintf("<SPAN CLASS=\"ib_roomlist_old\">");
			}
			extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
			escputs(buf);
			if (!strcasecmp(fold[i].name, "My Folders|Mail")) {
				wprintf(" (INBOX)");
			}
			wprintf("</SPAN>");
			if (fold[i].selectable) {
				wprintf("</A>");
			}
			else {
				wprintf("</i>");
			}
			wprintf("<br />");
			wprintf("</div>\n");	/** roomdiv */
		}
	}
	wprintf("</div>\n");	/** floordiv */


	/** BEGIN: The old invisible pixel trick, to get our JavaScript to initialize */
	wprintf("<img src=\"static/blank.gif\" onLoad=\"\n");

	num_drop_targets = 0;

	for (i=0; i<max_folders; ++i) {
		levels = num_tokens(fold[i].name, '|');
		if (levels > 1) {
			wprintf("drop_targets_elements[%d]=$('roomdiv%d');\n", num_drop_targets, i);
			wprintf("drop_targets_roomnames[%d]='", num_drop_targets);
			jsescputs(fold[i].room);
			wprintf("';\n");
			++num_drop_targets;
		}
	}

	wprintf("num_drop_targets = %d;\n", num_drop_targets);
	if (strlen(WC->floordiv_expanded) > 1) {
		wprintf("which_div_expanded = '%s';\n", WC->floordiv_expanded);
	}

	wprintf("\">\n");
	/** END: The old invisible pixel trick, to get our JavaScript to initialize */
}



/**
 * \brief Show the room list.  
 * (only should get called by
 * knrooms() because that's where output_headers() is called from)
 * \param viewpref the view preferences???
 */

void list_all_rooms_by_floor(char *viewpref) {
	char buf[SIZ];
	int swap = 0;
	struct folder *fold = NULL;
	struct folder ftmp;
	int max_folders = 0;
	int alloc_folders = 0;
	int i, j;
	int ra_flags = 0;
	int flags = 0;
	int num_floors = 1;	/** add an extra one for private folders */

	/** If our cached folder list is very old, burn it. */
	if (WC->cache_fold != NULL) {
		if ((time(NULL) - WC->cache_timestamp) > 300) {
			free(WC->cache_fold);
			WC->cache_fold = NULL;
		}
	}

	/** Can we do the iconbar roomlist from cache? */
	if ((WC->cache_fold != NULL) && (!strcasecmp(viewpref, "iconbar"))) {
		do_iconbar_view(WC->cache_fold, WC->cache_max_folders, WC->cache_num_floors);
		return;
	}

	/** Grab the floor table so we know how to build the list... */
	load_floorlist();

	/** Start with the mailboxes */
	max_folders = 1;
	alloc_folders = 1;
	fold = malloc(sizeof(struct folder));
	memset(fold, 0, sizeof(struct folder));
	strcpy(fold[0].name, "My folders");
	fold[0].is_mailbox = 1;

	/** Then add floors */
	serv_puts("LFLR");
	serv_getln(buf, sizeof buf);
	if (buf[0]=='1') while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (max_folders >= alloc_folders) {
			alloc_folders = max_folders + 100;
			fold = realloc(fold,
				alloc_folders * sizeof(struct folder));
		}
		memset(&fold[max_folders], 0, sizeof(struct folder));
		extract_token(fold[max_folders].name, buf, 1, '|', sizeof fold[max_folders].name);
		++max_folders;
		++num_floors;
	}

	/** refresh the messages index for this room */
//	serv_puts("GOTO ");
//	while (serv_getln(buf, sizeof buf), strcmp(buf, "000"));
	/** Now add rooms */
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0]=='1') while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (max_folders >= alloc_folders) {
			alloc_folders = max_folders + 100;
			fold = realloc(fold,
				alloc_folders * sizeof(struct folder));
		}
		memset(&fold[max_folders], 0, sizeof(struct folder));
		extract_token(fold[max_folders].room, buf, 0, '|', sizeof fold[max_folders].room);
		ra_flags = extract_int(buf, 5);
		flags = extract_int(buf, 1);
		fold[max_folders].floor = extract_int(buf, 2);
		fold[max_folders].hasnewmsgs =
			((ra_flags & UA_HASNEWMSGS) ? 1 : 0 );
		if (flags & QR_MAILBOX) {
			fold[max_folders].is_mailbox = 1;
		}
		fold[max_folders].view = extract_int(buf, 6);
		room_to_folder(fold[max_folders].name,
				fold[max_folders].room,
				fold[max_folders].floor,
				fold[max_folders].is_mailbox);
		fold[max_folders].selectable = 1;
		++max_folders;
	}

	/** Bubble-sort the folder list */
	for (i=0; i<max_folders; ++i) {
		for (j=0; j<(max_folders-1)-i; ++j) {
			if (fold[j].is_mailbox == fold[j+1].is_mailbox) {
				swap = strcasecmp(fold[j].name, fold[j+1].name);
			}
			else {
				if ( (fold[j+1].is_mailbox)
				   && (!fold[j].is_mailbox)) {
					swap = 1;
				}
				else {
					swap = 0;
				}
			}
			if (swap > 0) {
				memcpy(&ftmp, &fold[j], sizeof(struct folder));
				memcpy(&fold[j], &fold[j+1],
							sizeof(struct folder));
				memcpy(&fold[j+1], &ftmp,
							sizeof(struct folder));
			}
		}
	}


	if (!strcasecmp(viewpref, "folders")) {
		do_folder_view(fold, max_folders, num_floors);
	}
	else if (!strcasecmp(viewpref, "hackish_view")) {
		for (i=0; i<max_folders; ++i) {
			escputs(fold[i].name);
			wprintf("<br />\n");
		}
	}
	else if (!strcasecmp(viewpref, "iconbar")) {
		do_iconbar_view(fold, max_folders, num_floors);
	}
	else {
		do_rooms_view(fold, max_folders, num_floors);
	}

	/* Don't free the folder list ... cache it for future use! */
	if (WC->cache_fold != NULL) {
		free(WC->cache_fold);
	}
	WC->cache_fold = fold;
	WC->cache_max_folders = max_folders;
	WC->cache_num_floors = num_floors;
	WC->cache_timestamp = time(NULL);
}


/**
 * \brief Do either a known rooms list or a folders list, depending on the
 * user's preference
 */
void knrooms(void)
{
	char listviewpref[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);

	/** Determine whether the user is trying to change views */
	if (bstr("view") != NULL) {
		if (strlen(bstr("view")) > 0) {
			set_preference("roomlistview", bstr("view"), 1);
		}
	}

	get_preference("roomlistview", listviewpref, sizeof listviewpref);

	if ( (strcasecmp(listviewpref, "folders"))
	   && (strcasecmp(listviewpref, "table")) ) {
		strcpy(listviewpref, "rooms");
	}

	/** title bar */
	wprintf("<div id=\"banner\">\n"
		"<TABLE class=\"roomops_banner\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">"
	);
	if (!strcasecmp(listviewpref, "rooms")) {
		wprintf(_("Room list"));
	}
	if (!strcasecmp(listviewpref, "folders")) {
		wprintf(_("Folder list"));
	}
	if (!strcasecmp(listviewpref, "table")) {
		wprintf(_("Room list"));
	}
	wprintf("</SPAN></TD>\n");

	/** offer the ability to switch views */
	wprintf("<TD ALIGN=RIGHT><FORM NAME=\"roomlistomatic\">\n"
		"<SELECT NAME=\"newview\" SIZE=\"1\" "
		"OnChange=\"location.href=roomlistomatic.newview.options"
		"[selectedIndex].value\">\n");

	wprintf("<OPTION %s VALUE=\"knrooms&view=rooms\">"
		"View as room list"
		"</OPTION>\n",
		( !strcasecmp(listviewpref, "rooms") ? "SELECTED" : "" )
	);

	wprintf("<OPTION %s VALUE=\"knrooms&view=folders\">"
		"View as folder list"
		"</OPTION>\n",
		( !strcasecmp(listviewpref, "folders") ? "SELECTED" : "" )
	);

	wprintf("</SELECT><br />");
	offer_start_page();
	wprintf("</FORM></TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"</div>\n"
		"<div id=\"content\">\n");

	/** Display the room list in the user's preferred format */
	list_all_rooms_by_floor(listviewpref);
	wDumpContent(1);
}



/**
 * \brief Set the message expire policy for this room and/or floor
 */
void set_room_policy(void) {
	char buf[SIZ];

	if (strlen(bstr("ok_button")) == 0) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  Changes were not saved."));
		display_editroom();
		return;
	}

	serv_printf("SPEX room|%d|%d", atoi(bstr("roompolicy")), atoi(bstr("roomvalue")));
	serv_getln(buf, sizeof buf);
	strcpy(WC->ImportantMessage, &buf[4]);

	if (WC->axlevel >= 6) {
		strcat(WC->ImportantMessage, "<br />\n");
		serv_printf("SPEX floor|%d|%d", atoi(bstr("floorpolicy")), atoi(bstr("floorvalue")));
		serv_getln(buf, sizeof buf);
		strcat(WC->ImportantMessage, &buf[4]);
	}

	display_editroom();
}


/*@}*/
