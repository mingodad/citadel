/*
 * $Id$
 * Lots of different room-related operations.
 */

#include "webcit.h"
#include "webserver.h"
#define MAX_FLOORS 128
char floorlist[MAX_FLOORS][SIZ]; /**< list of our floor names */

char *viewdefs[9]; /**< the different kinds of available views */

void display_whok(void);

/*
 * Initialize the viewdefs with localized strings
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
	viewdefs[8] = _("Journal");
}

/*
 * Determine which views are allowed as the default for creating a new room.
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

#ifdef TECH_PREVIEW
		case VIEW_WIKI:		return(1);
#else /* TECH_PREVIEW */
		case VIEW_WIKI:		return(0);	/* because it isn't finished yet */
#endif /* TECH_PREVIEW */

		case VIEW_CALBRIEF:	return(0);
		case VIEW_JOURNAL:	return(0);
		default:		return(0);	/* should never get here */
	}
}


/*
 * load the list of floors
 */
void load_floorlist(void)
{
	int a;
	char buf[SIZ];

	for (a = 0; a < MAX_FLOORS; ++a)
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


/*
 * Free a session's march list
 */
void free_march_list(wcsession *wcf)
{
	struct march *mptr;

	while (wcf->march != NULL) {
		mptr = wcf->march->next;
		free(wcf->march);
		wcf->march = mptr;
	}

}



/*
 * remove a room from the march list
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




/*
 * display rooms in tree structure
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
	wprintf("</a><tt> </tt>\n");

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
	StrBuf *Buf;
	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Zapped (forgotten) rooms"), -1);
	DoTemplate(HKEY("beginbox"), NULL, Buf, CTX_STRBUF);

	FreeStrBuf(&Buf);

	listrms("LZRM -1");

	wprintf("<br /><br />\n");
	wprintf(_("Click on any room to un-zap it and goto that room.\n"));
	do_template("endbox", NULL);
	wDumpContent(1);
}


/**
 * \brief read this room's info file (set v to 1 for verbose mode)
 */
void readinfo(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	char buf[256];
	char briefinfo[128];
	char fullinfo[8192];
	int fullinfo_len = 0;

	serv_puts("RINF");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {

		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (fullinfo_len < (sizeof fullinfo - sizeof buf)) {
				strcpy(&fullinfo[fullinfo_len], buf);
				fullinfo_len += strlen(buf);
			}
		}

		safestrncpy(briefinfo, fullinfo, sizeof briefinfo);
		strcpy(&briefinfo[50], "...");

                wprintf("<div class=\"infos\" "
                "onclick=\"javascript:Effect.Appear('room_infos', { duration: 0.5 });\" "
                ">");
		escputs(briefinfo);
                wprintf("</div><div id=\"room_infos\" style=\"display:none;\">");
		wprintf("<img class=\"close_infos\" "
                	"onclick=\"javascript:Effect.Fade('room_infos', { duration: 0.5 });\" "
			"src=\"static/closewindow.gif\" alt=\"%s\">",
			_("Close window")
		);
		escputs(fullinfo);
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
void embed_room_graphic(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType) {
	char buf[SIZ];

	serv_puts("OIMG _roompic_");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		wprintf("<img height=\"64px\" src=\"image&name=_roompic_&room=");
		urlescputs(WC->wc_roomname);
		wprintf("\">");
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
	}
	else if (WC->wc_view == VIEW_ADDRESSBOOK) {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/viewcontacts_48x.gif"
			"\">"
		);
	}
	else if ( (WC->wc_view == VIEW_CALENDAR) || (WC->wc_view == VIEW_CALBRIEF) ) {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/calarea_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_TASKS) {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/taskmanag_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_NOTES) {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/storenotes_48x.gif"
			"\">"
		);
	}
	else if (WC->wc_view == VIEW_MAILBOX) {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/privatemess_48x.gif"
			"\">"
		);
	}
	else {
		wprintf("<img class=\"roompic\" alt=\"\" src=\""
			"static/chatrooms_48x.gif"
			"\">"
		);
	}

}



/**
 * \brief Display the current view and offer an option to change it
 */
void embed_view_o_matic(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType) {
	int i;

	wprintf("<form name=\"viewomatic\" action=\"changeview\">\n");
	wprintf("\t<div style=\"display: inline;\">\n\t<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<label for=\"view_name\">");
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
	wprintf("</select></div></form>\n");
}


/**
 * \brief Display a search box
 */
void embed_search_o_matic(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType) {
	wprintf("<form name=\"searchomatic\" action=\"do_search\">\n");
	wprintf("<div style=\"display: inline;\"><input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<label for=\"search_name\">");
	wprintf(_("Search: "));
	wprintf("</label><input ");
	wprintf("%s", serv_info.serv_fulltext_enabled ? "" : "disabled ");
	wprintf("type=\"text\" name=\"query\" size=\"15\" maxlength=\"128\" "
		"id=\"search_name\" class=\"inputbox\">\n"
	);
	wprintf("</div></form>\n");
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
	char buf2[1024];
	char with_files[256];
	int file_count=0;
	
	/**
	 * We need to have the information returned by a GOTO server command.
	 * If it isn't supplied, we fake it by issuing our own GOTO.
	 */
	if (got == NULL) {
		memset(buf, 20, '0');
		buf[20] = '\0';
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

	/* Is this a directory room and does it contain files and how many? */
	if ((WC->room_flags & QR_DIRECTORY) && (WC->room_flags & QR_VISDIR))
	{
		serv_puts("RDIR");
		serv_getln(buf2, sizeof buf2);
		if (buf2[0] == '1') while (serv_getln(buf2, sizeof buf2), strcmp(buf2, "000"))
			file_count++;
		snprintf (with_files, sizeof with_files, 
			  "; <a href=\"display_room_directory\"> %d %s </a>", 
			  file_count, 
			  ((file_count>1) || (file_count == 0)  ? _("files") : _("file")));
	}
	else
		strcpy (with_files, "");
	
	svprintf(HKEY("NUMMSGS"), WCS_STRING,
		_("%d new of %d messages%s"),
		extract_int(&got[4], 1),
		extract_int(&got[4], 2),
		with_files
	);
	svcallback("ROOMPIC", embed_room_graphic);
	svcallback("ROOMINFO", readinfo);
	svcallback("VIEWOMATIC", embed_view_o_matic); 
	svcallback("SEARCHOMATIC", embed_search_o_matic);
	svcallback("START", offer_start_page); 
 
	do_template("roombanner", NULL);
	// roombanner contains this for mobile
	if (navbar_style != navbar_none && !WC->is_mobile) { 

		wprintf("<div id=\"navbar\"><ul>");

		if (navbar_style == navbar_default) wprintf(
			"<li class=\"ungoto\">"
			"<a href=\"ungoto\">"
			"<img src=\"static/ungoto2_24x.gif\" alt=\"\">"
			"<span class=\"navbar_link\">%s</span></A>"
			"</li>\n", _("Ungoto")
		);

		if ( (navbar_style == navbar_default) && (WC->wc_view == VIEW_BBS) ) {
			wprintf(
				"<li class=\"newmess\">"
				"<a href=\"readnew\">"
				"<img src=\"static/newmess2_24x.gif\" alt=\"\">"
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
						"<img src=\"static/viewcontacts_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View contacts")
					);
					break;
				case VIEW_CALENDAR:
					wprintf(
						"<li class=\"staskday\">"
						"<a href=\"readfwd?calview=day\">"
						"<img src=\"static/taskday2_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Day view")
					);
					wprintf(
						"<li class=\"monthview\">"
						"<a href=\"readfwd?calview=month\">"
						"<img src=\"static/monthview2_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Month view")
					);
					break;
				case VIEW_CALBRIEF:
					wprintf(
						"<li class=\"monthview\">"
						"<a href=\"readfwd?calview=month\">"
						"<img src=\"static/monthview2_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Calendar list")
					);
					break;
				case VIEW_TASKS:
					wprintf(
						"<li class=\"taskmanag\">"
						"<a href=\"readfwd\">"
						"<img src=\"static/taskmanag_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View tasks")
					);
					break;
				case VIEW_NOTES:
					wprintf(
						"<li class=\"viewnotes\">"
						"<a href=\"readfwd\">"
						"<img src=\"static/viewnotes_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View notes")
					);
					break;
				case VIEW_MAILBOX:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img src=\"static/readallmess3_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("View message list")
					);
					break;
				case VIEW_WIKI:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img src=\"static/readallmess3_24x.gif\" "
						"alt=\"\">"
						"<span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Wiki home")
					);
					break;
				default:
					wprintf(
						"<li class=\"readallmess\">"
						"<a href=\"readfwd\">"
						"<img src=\"static/readallmess3_24x.gif\" "
						"alt=\"\">"
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
						"<img src=\"static/addnewcontact_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new contact")
					);
					break;
				case VIEW_CALENDAR:
				case VIEW_CALBRIEF:
					wprintf("<li class=\"addevent\"><a href=\"display_enter");
					if (havebstr("year" )) wprintf("?year=%s", bstr("year"));
					if (havebstr("month")) wprintf("?month=%s", bstr("month"));
					if (havebstr("day"  )) wprintf("?day=%s", bstr("day"));
					wprintf("\">"
						"<img  src=\"static/addevent_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new event")
					);
					break;
				case VIEW_TASKS:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img  src=\"static/newmess3_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Add new task")
					);
					break;
				case VIEW_NOTES:
					wprintf(
						"<li class=\"enternewnote\">"
						"<a href=\"add_new_note\">"
						"<img  src=\"static/enternewnote_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
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
						"<img  src=\"static/newmess3_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", buf, _("Edit this page")
					);
					break;
				case VIEW_MAILBOX:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img  src=\"static/newmess3_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
						"%s"
						"</span></a></li>\n", _("Write mail")
					);
					break;
				default:
					wprintf(
						"<li class=\"newmess\">"
						"<a href=\"display_enter\">"
						"<img  src=\"static/newmess3_24x.gif\" "
						"alt=\"\"><span class=\"navbar_link\">"
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
			"<img  src=\"static/skipthisroom_24x.gif\" alt=\"\">"
			"<span class=\"navbar_link\">%s</span></a>"
			"</li>\n",
			_("Leave all messages marked as unread, go to next room with unread messages"),
			_("Skip this room")
		);

		if (navbar_style == navbar_default) wprintf(
			"<li class=\"markngo\">"
			"<a href=\"gotonext\" "
			"title=\"%s\">"
			"<img  src=\"static/markngo_24x.gif\" alt=\"\">"
			"<span class=\"navbar_link\">%s</span></a>"
			"</li>\n",
			_("Mark all messages as read, go to next room with unread messages"),
			_("Goto next room")
		);

		wprintf("</ul></div>\n");
	}

}


/*
 * back end routine to take the session to a new room
 */
int gotoroom(char *gname)
{
	char buf[SIZ];
	static long ls = (-1L);
	int err = 0;

	/* store ungoto information */
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



/*
 * Goto next room having unread messages.
 *
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 * We start the search in the current room rather than the beginning to prevent
 * two or more concurrent users from dragging each other back to the same room.
 */
void gotonext(void)
{
	char buf[256];
	struct march *mptr = NULL;
	struct march *mptr2 = NULL;
	char room_name[128];
	char next_room[128];
	int ELoop = 0;

	/*
	 * First check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */

	if (WC->march == NULL) {
		serv_puts("LKRN");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1')
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (IsEmptyStr(buf)) {
					if (ELoop > 10000)
						return;
					if (ELoop % 100 == 0)
						sleeeeeeeeeep(1);
					ELoop ++;
					continue;					
				}
				extract_token(room_name, buf, 0, '|', sizeof room_name);
				if (strcasecmp(room_name, WC->wc_roomname)) {
					mptr = (struct march *) malloc(sizeof(struct march));
					mptr->next = NULL;
					safestrncpy(mptr->march_name, room_name, sizeof mptr->march_name);
					mptr->march_floor = extract_int(buf, 2);
					mptr->march_order = extract_int(buf, 3);
					if (WC->march == NULL) 
						WC->march = mptr;
					else 
						mptr2->next = mptr;
					mptr2 = mptr;
				}
				buf[0] = '\0';
			}
		/*
		 * add _BASEROOM_ to the end of the march list, so the user will end up
		 * in the system base room (usually the Lobby>) at the end of the loop
		 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		mptr->march_order = 0;
	    	mptr->march_floor = 0;
		strcpy(mptr->march_name, "_BASEROOM_");
		if (WC->march == NULL) {
			WC->march = mptr;
		} else {
			mptr2 = WC->march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
		}
		/*
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


/*
 * goto next room
 */
void smart_goto(char *next_room) {
	gotoroom(next_room);
	readloop(readnew);
}



/*
 * mark all messages in current room as having been read
 */
void slrp_highest(void)
{
	char buf[256];

	serv_puts("SLRP HIGHEST");
	serv_getln(buf, sizeof buf);
}


/*
 * un-goto the previous room
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

typedef struct __room_states {
	char password[SIZ];
	char dirname[SIZ];
	char name[SIZ];
	int flags;
	int floor;
	int order;
	int view;
	int flags2;
} room_states;




/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * set newval to 0 to clear, 1 to set, any other value to leave unchanged.
 * returns the new value.
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

int is_selflist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_SELFLIST) != 0);
}

int is_publiclist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_SMTP_PUBLIC) != 0);
}

int is_moderatedlist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_MODERATED) != 0);
}

/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * set newval to 0 to clear, 1 to set, any other value to leave unchanged.
 * returns the new value.
 */

int get_roomflags(room_states *RoomOps) 
{
	char buf[SIZ];
	
	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return(0);

	extract_token(RoomOps->name, &buf[4], 0, '|', sizeof RoomOps->name);
	extract_token(RoomOps->password, &buf[4], 1, '|', sizeof RoomOps->password);
	extract_token(RoomOps->dirname, &buf[4], 2, '|', sizeof RoomOps->dirname);
	RoomOps->flags = extract_int(&buf[4], 3);
	RoomOps->floor = extract_int(&buf[4], 4);
	RoomOps->order = extract_int(&buf[4], 5);
	RoomOps->view = extract_int(&buf[4], 6);
	RoomOps->flags2 = extract_int(&buf[4], 7);
	return (1);
}

int set_roomflags(room_states *RoomOps)
{
	char buf[SIZ];

	serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
		    RoomOps->name, 
		    RoomOps->password, 
		    RoomOps->dirname, 
		    RoomOps->flags,
		    RoomOps->floor, 
		    RoomOps->order, 
		    RoomOps->view, 
		    RoomOps->flags2);
	serv_getln(buf, sizeof buf);
	return (1);
}






/*
 * display the form for editing a room
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
	char pop3_host[128];
	char pop3_user[32];
	int bg = 0;

	tab = bstr("tab");
	if (IsEmptyStr(tab)) tab = "admin";

	load_floorlist();
	output_headers(1, 1, 1, 0, 0, 0);

	wprintf("<div class=\"fix_scrollbar_bug\">");

	wprintf("<br />\n");

	/* print the tabbed dialog */
	wprintf("<div align=\"center\">");
	wprintf("<table id=\"AdminTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\""
		"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
		);

	wprintf("<td class=\"");
	if (!strcmp(tab, "admin")) {
		wprintf(" tab_cell_label\">");
		wprintf(_("Administration"));
	}
	else {
		wprintf("< tab_cell_edit\"><a href=\"display_editroom&tab=admin\">");
		wprintf(_("Administration"));
		wprintf("</a>");
	}
	wprintf("</td>\n");
	wprintf("<td>&nbsp;</td>\n");

	if ( (WC->axlevel >= 6) || (WC->is_room_aide) ) {

		wprintf("<td class=\"");
		if (!strcmp(tab, "config")) {
			wprintf(" tab_cell_label\">");
			wprintf(_("Configuration"));
		}
		else {
			wprintf(" tab_cell_edit\"><a href=\"display_editroom&tab=config\">");
			wprintf(_("Configuration"));
			wprintf("</a>");
		}
		wprintf("</td>\n");
		wprintf("<td>&nbsp;</td>\n");

		wprintf("<td class=\"");
		if (!strcmp(tab, "expire")) {
			wprintf(" tab_cell_label\">");
			wprintf(_("Message expire policy"));
		}
		else {
			wprintf(" tab_cell_edit\"><a href=\"display_editroom&tab=expire\">");
			wprintf(_("Message expire policy"));
			wprintf("</a>");
		}
		wprintf("</td>\n");
		wprintf("<td>&nbsp;</td>\n");
	
		wprintf("<td class=\"");
		if (!strcmp(tab, "access")) {
			wprintf(" tab_cell_label\">");
			wprintf(_("Access controls"));
		}
		else {
			wprintf(" tab_cell_edit\"><a href=\"display_editroom&tab=access\">");
			wprintf(_("Access controls"));
			wprintf("</a>");
		}
		wprintf("</td>\n");
		wprintf("<td>&nbsp;</td>\n");

		wprintf("<td class=\"");
		if (!strcmp(tab, "sharing")) {
			wprintf(" tab_cell_label\">");
			wprintf(_("Sharing"));
		}
		else {
			wprintf(" tab_cell_edit\"><a href=\"display_editroom&tab=sharing\">");
			wprintf(_("Sharing"));
			wprintf("</a>");
		}
		wprintf("</td>\n");
		wprintf("<td>&nbsp;</td>\n");

		wprintf("<td class=\"");
		if (!strcmp(tab, "listserv")) {
			wprintf(" tab_cell_label\">");
			wprintf(_("Mailing list service"));
		}
		else {
			wprintf("< tab_cell_edit\"><a href=\"display_editroom&tab=listserv\">");
			wprintf(_("Mailing list service"));
			wprintf("</a>");
		}
		wprintf("</td>\n");
		wprintf("<td>&nbsp;</td>\n");

	}

	wprintf("<td class=\"");
	if (!strcmp(tab, "feeds")) {
		wprintf(" tab_cell_label\">");
		wprintf(_("Remote retrieval"));
	}
	else {
		wprintf("< tab_cell_edit\"><a href=\"display_editroom&tab=feeds\">");
		wprintf(_("Remote retrieval"));
		wprintf("</a>");
	}
	wprintf("</td>\n");
	wprintf("<td>&nbsp;</td>\n");

	wprintf("</tr></table>\n");
	wprintf("</div>\n");
	/* end tabbed dialog */	

	wprintf("<script type=\"text/javascript\">"
		" Nifty(\"table#AdminTabs td\", \"small transparent top\");"
		"</script>"
	);

	/* begin content of whatever tab is open now */

	if (!strcmp(tab, "admin")) {
		wprintf("<div class=\"tabcontent\">");
		wprintf("<ul>"
			"<li><a href=\"delete_room\" "
			"onClick=\"return confirm('");
		wprintf(_("Are you sure you want to delete this room?"));
		wprintf("');\">\n");
		wprintf(_("Delete this room"));
		wprintf("</a>\n"
			"<li><a href=\"display_editroompic\">\n");
		wprintf(_("Set or change the icon for this room's banner"));
		wprintf("</a>\n"
			"<li><a href=\"display_editinfo\">\n");
		wprintf(_("Edit this room's Info file"));
		wprintf("</a>\n"
			"</ul>");
		wprintf("</div>");
	}

	if (!strcmp(tab, "config")) {
		wprintf("<div class=\"tabcontent\">");
		serv_puts("GETR");
		serv_getln(buf, sizeof buf);

		if (!strncmp(buf, "550", 3)) {
			wprintf("<br><br><div align=center>%s</div><br><br>\n",
				_("Higher access is required to access this function.")
			);
		}
		else if (buf[0] != '2') {
			wprintf("<br><br><div align=center>%s</div><br><br>\n", &buf[4]);
		}
		else {
			extract_token(er_name, &buf[4], 0, '|', sizeof er_name);
			extract_token(er_password, &buf[4], 1, '|', sizeof er_password);
			extract_token(er_dirname, &buf[4], 2, '|', sizeof er_dirname);
			er_flags = extract_int(&buf[4], 3);
			er_floor = extract_int(&buf[4], 4);
			er_flags2 = extract_int(&buf[4], 7);
	
			wprintf("<form method=\"POST\" action=\"editroom\">\n");
			wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		
			wprintf("<ul><li>");
			wprintf(_("Name of room: "));
			wprintf("<input type=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\"%d\">\n",
				er_name,
				(sizeof(er_name)-1)
			);
		
			wprintf("<li>");
			wprintf(_("Resides on floor: "));
			wprintf("<select NAME=\"er_floor\" SIZE=\"1\"");
			if (er_flags & QR_MAILBOX)
				wprintf("disabled >\n");
			for (i = 0; i < 128; ++i)
				if (!IsEmptyStr(floorlist[i])) {
					wprintf("<OPTION ");
					if (i == er_floor )
						wprintf("SELECTED ");
					wprintf("VALUE=\"%d\">", i);
					escputs(floorlist[i]);
					wprintf("</OPTION>\n");
				}
			wprintf("</select>\n");

			wprintf("<li>");
			wprintf(_("Type of room:"));
			wprintf("<ul>\n");
	
			wprintf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"public\" ");
			if ((er_flags & (QR_PRIVATE + QR_MAILBOX)) == 0)
				wprintf("CHECKED ");
			wprintf("OnChange=\""
				"	if (this.form.type[0].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wprintf(_("Public (automatically appears to everyone)"));
			wprintf("\n");
	
			wprintf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"hidden\" ");
			if ((er_flags & QR_PRIVATE) &&
		    	(er_flags & QR_GUESSNAME))
				wprintf("CHECKED ");
			wprintf(" OnChange=\""
				"	if (this.form.type[1].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wprintf(_("Private - hidden (accessible to anyone who knows its name)"));
		
			wprintf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
			if ((er_flags & QR_PRIVATE) &&
		    	(er_flags & QR_PASSWORDED))
				wprintf("CHECKED ");
			wprintf(" OnChange=\""
				"	if (this.form.type[2].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wprintf(_("Private - require password: "));
			wprintf("\n<input type=\"text\" NAME=\"er_password\" VALUE=\"%s\" MAXLENGTH=\"9\">\n",
				er_password);
		
			wprintf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
			if ((er_flags & QR_PRIVATE)
		    	&& ((er_flags & QR_GUESSNAME) == 0)
		    	&& ((er_flags & QR_PASSWORDED) == 0))
				wprintf("CHECKED ");
			wprintf(" OnChange=\""
				"	if (this.form.type[3].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wprintf(_("Private - invitation only"));
		
			wprintf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"personal\" ");
			if (er_flags & QR_MAILBOX)
				wprintf("CHECKED ");
			wprintf (" OnChange=\""
				"	if (this.form.type[4].checked == true) {	"
				"		this.form.er_floor.disabled = true;	"
				"	}						"
				"\"> ");
			wprintf(_("Personal (mailbox for you only)"));
			
			wprintf("\n<li><input type=\"checkbox\" NAME=\"bump\" VALUE=\"yes\" ");
			wprintf("> ");
			wprintf(_("If private, cause current users to forget room"));
		
			wprintf("\n</ul>\n");
		
			wprintf("<li><input type=\"checkbox\" NAME=\"prefonly\" VALUE=\"yes\" ");
			if (er_flags & QR_PREFONLY)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Preferred users only"));
		
			wprintf("\n<li><input type=\"checkbox\" NAME=\"readonly\" VALUE=\"yes\" ");
			if (er_flags & QR_READONLY)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Read-only room"));
		
			wprintf("\n<li><input type=\"checkbox\" NAME=\"collabdel\" VALUE=\"yes\" ");
			if (er_flags2 & QR2_COLLABDEL)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("All users allowed to post may also delete messages"));
		
			/** directory stuff */
			wprintf("\n<li><input type=\"checkbox\" NAME=\"directory\" VALUE=\"yes\" ");
			if (er_flags & QR_DIRECTORY)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("File directory room"));
	
			wprintf("\n<ul><li>");
			wprintf(_("Directory name: "));
			wprintf("<input type=\"text\" NAME=\"er_dirname\" VALUE=\"%s\" MAXLENGTH=\"14\">\n",
				er_dirname);
	
			wprintf("<li><input type=\"checkbox\" NAME=\"ulallowed\" VALUE=\"yes\" ");
			if (er_flags & QR_UPLOAD)
			wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Uploading allowed"));
		
			wprintf("\n<li><input type=\"checkbox\" NAME=\"dlallowed\" VALUE=\"yes\" ");
			if (er_flags & QR_DOWNLOAD)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Downloading allowed"));
		
			wprintf("\n<li><input type=\"checkbox\" NAME=\"visdir\" VALUE=\"yes\" ");
			if (er_flags & QR_VISDIR)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Visible directory"));
			wprintf("</ul>\n");
		
			/** end of directory stuff */
	
			wprintf("<li><input type=\"checkbox\" NAME=\"network\" VALUE=\"yes\" ");
			if (er_flags & QR_NETWORK)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Network shared room"));
	
			wprintf("\n<li><input type=\"checkbox\" NAME=\"permanent\" VALUE=\"yes\" ");
			if (er_flags & QR_PERMANENT)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Permanent (does not auto-purge)"));
	
			wprintf("\n<li><input type=\"checkbox\" NAME=\"subjectreq\" VALUE=\"yes\" ");
			if (er_flags2 & QR2_SUBJECTREQ)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Subject Required (Force users to specify a message subject)"));
	
			/** start of anon options */
		
			wprintf("\n<li>");
			wprintf(_("Anonymous messages"));
			wprintf("<ul>\n");
		
			wprintf("<li><input type=\"radio\" NAME=\"anon\" VALUE=\"no\" ");
			if (((er_flags & QR_ANONONLY) == 0)
		    	&& ((er_flags & QR_ANONOPT) == 0))
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("No anonymous messages"));
	
			wprintf("\n<li><input type=\"radio\" NAME=\"anon\" VALUE=\"anononly\" ");
			if (er_flags & QR_ANONONLY)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("All messages are anonymous"));
		
			wprintf("\n<li><input type=\"radio\" NAME=\"anon\" VALUE=\"anon2\" ");
			if (er_flags & QR_ANONOPT)
				wprintf("CHECKED ");
			wprintf("> ");
			wprintf(_("Prompt user when entering messages"));
			wprintf("</ul>\n");
		
		/* end of anon options */
		
			wprintf("<li>");
			wprintf(_("Room aide: "));
			serv_puts("GETA");
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				wprintf("<em>%s</em>\n", &buf[4]);
			} else {
				extract_token(er_roomaide, &buf[4], 0, '|', sizeof er_roomaide);
				wprintf("<input type=\"text\" NAME=\"er_roomaide\" VALUE=\"%s\" MAXLENGTH=\"25\">\n", er_roomaide);
			}
		
			wprintf("</ul><CENTER>\n");
			wprintf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"config\">\n"
				"<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">"
				"&nbsp;"
				"<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">"
				"</CENTER>\n",
				_("Save changes"),
				_("Cancel")
			);
		}
		wprintf("</div>");
	}


	/* Sharing the room with other Citadel nodes... */
	if (!strcmp(tab, "sharing")) {
		wprintf("<div class=\"tabcontent\">");

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
				if (!IsEmptyStr(remote_room)) {
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

		/* Display the stuff */
		wprintf("<CENTER><br />"
			"<table border=1 cellpadding=5><tr>"
			"<td><B><I>");
		wprintf(_("Shared with"));
		wprintf("</I></B></td>"
			"<td><B><I>");
		wprintf(_("Not shared with"));
		wprintf("</I></B></td></tr>\n"
			"<tr><td VALIGN=TOP>\n");

		wprintf("<table border=0 cellpadding=5><tr class=\"tab_cell\"><td>");
		wprintf(_("Remote node name"));
		wprintf("</td><td>");
		wprintf(_("Remote room name"));
		wprintf("</td><td>");
		wprintf(_("Actions"));
		wprintf("</td></tr>\n");

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n', sizeof buf);
			extract_token(node, buf, 0, '|', sizeof node);
			extract_token(remote_room, buf, 1, '|', sizeof remote_room);
			if (!IsEmptyStr(node)) {
				wprintf("<form method=\"POST\" action=\"netedit\">");
				wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
				wprintf("<tr><td>%s</td>\n", node);

				wprintf("<td>");
				if (!IsEmptyStr(remote_room)) {
					escputs(remote_room);
				}
				wprintf("</td>");

				wprintf("<td>");
		
				wprintf("<input type=\"hidden\" NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				if (!IsEmptyStr(remote_room)) {
					wprintf("|");
					urlescputs(remote_room);
				}
				wprintf("\">");
				wprintf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"sharing\">\n");
				wprintf("<input type=\"hidden\" NAME=\"cmd\" VALUE=\"remove\">\n");
				wprintf("<input type=\"submit\" "
					"NAME=\"unshare_button\" VALUE=\"%s\">", _("Unshare"));
				wprintf("</td></tr></form>\n");
			}
		}

		wprintf("</table>\n");
		wprintf("</td><td VALIGN=TOP>\n");
		wprintf("<table border=0 cellpadding=5><tr class=\"tab_cell\"><td>");
		wprintf(_("Remote node name"));
		wprintf("</td><td>");
		wprintf(_("Remote room name"));
		wprintf("</td><td>");
		wprintf(_("Actions"));
		wprintf("</td></tr>\n");

		for (i=0; i<num_tokens(not_shared_with, '\n'); ++i) {
			extract_token(node, not_shared_with, i, '\n', sizeof node);
			if (!IsEmptyStr(node)) {
				wprintf("<form method=\"POST\" action=\"netedit\">");
				wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
				wprintf("<tr><td>");
				escputs(node);
				wprintf("</td><td>"
					"<input type=\"INPUT\" "
					"NAME=\"suffix\" "
					"MAXLENGTH=128>"
					"</td><td>");
				wprintf("<input type=\"hidden\" "
					"NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				wprintf("|\">");
				wprintf("<input type=\"hidden\" NAME=\"tab\" "
					"VALUE=\"sharing\">\n");
				wprintf("<input type=\"hidden\" NAME=\"cmd\" "
					"VALUE=\"add\">\n");
				wprintf("<input type=\"submit\" "
					"NAME=\"add_button\" VALUE=\"%s\">", _("Share"));
				wprintf("</td></tr></form>\n");
			}
		}

		wprintf("</table>\n");
		wprintf("</td></tr>"
			"</table></CENTER><br />\n"
			"<I><B>%s</B><ul><li>", _("Notes:"));
		wprintf(_("When sharing a room, "
			"it must be shared from both ends.  Adding a node to "
			"the 'shared' list sends messages out, but in order to"
			" receive messages, the other nodes must be configured"
			" to send messages out to your system as well. "
			"<li>If the remote room name is blank, it is assumed "
			"that the room name is identical on the remote node."
			"<li>If the remote room name is different, the remote "
			"node must also configure the name of the room here."
			"</ul></I><br />\n"
		));

		wprintf("</div>");
	}

	/* Mailing list management */
	if (!strcmp(tab, "listserv")) {
		room_states RoomFlags;
		wprintf("<div class=\"tabcontent\">");

		wprintf("<br /><center>"
			"<table BORDER=0 WIDTH=100%% CELLPADDING=5>"
			"<tr><td VALIGN=TOP>");

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
				wprintf(" <a href=\"netedit&cmd=remove&tab=listserv&line=listrecp|");
				urlescputs(recp);
				wprintf("\">");
				wprintf(_("(remove)"));
				wprintf("</A><br />");
			}
		}
		wprintf("<br /><form method=\"POST\" action=\"netedit\">\n"
			"<input type=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<input type=\"hidden\" NAME=\"prefix\" VALUE=\"listrecp|\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<input type=\"text\" id=\"add_as_listrecp\" NAME=\"line\">\n");
		wprintf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wprintf("</form>\n");

		wprintf("</td><td VALIGN=TOP>\n");
		
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
				wprintf(" <a href=\"netedit&cmd=remove&tab=listserv&line="
					"digestrecp|");
				urlescputs(recp);
				wprintf("\">");
				wprintf(_("(remove)"));
				wprintf("</A><br />");
			}
		}
		wprintf("<br /><form method=\"POST\" action=\"netedit\">\n"
			"<input type=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<input type=\"hidden\" NAME=\"prefix\" VALUE=\"digestrecp|\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<input type=\"text\" id=\"add_as_digestrecp\" NAME=\"line\">\n");
		wprintf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wprintf("</form>\n");
		
		wprintf("</td></tr></table>\n");

		/** Pop open an address book -- begin **/
		wprintf("<div align=right>"
			"<a href=\"javascript:PopOpenAddressBook('add_as_listrecp|%s|add_as_digestrecp|%s');\" "
			"title=\"%s\">"
			"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
			"&nbsp;%s</a>"
			"</div>",
			_("List"),
			_("Digest"),
			_("Add recipients from Contacts or other address books"),
			_("Add recipients from Contacts or other address books")
		);
		/* Pop open an address book -- end **/

		wprintf("<br />\n<form method=\"GET\" action=\"toggle_self_service\">\n");

		get_roomflags (&RoomFlags);
		
		/* Self Service subscription? */
		wprintf("<table><tr><td>\n");
		wprintf(_("Allow self-service subscribe/unsubscribe requests."));
		wprintf("</td><td><input type=\"checkbox\" name=\"QR2_SelfList\" value=\"yes\" %s></td></tr>\n"
			" <tr><td colspan=\"2\">\n",
			(is_selflist(&RoomFlags))?"checked":"");
		wprintf(_("The URL for subscribe/unsubscribe is: "));
		wprintf("<TT>%s://%s/listsub</TT></td></tr>\n",
			(is_https ? "https" : "http"),
			WC->http_host);
		/* Public posting? */
		wprintf("<tr><td>");
		wprintf(_("Allow non-subscribers to mail to this room."));
		wprintf("</td><td><input type=\"checkbox\" name=\"QR2_SubsOnly\" value=\"yes\" %s></td></tr>\n",
			(is_publiclist(&RoomFlags))?"checked":"");
		
		/* Moderated List? */
		wprintf("<tr><td>");
		wprintf(_("Room post publication needs Aide permission."));
		wprintf("</td><td><input type=\"checkbox\" name=\"QR2_Moderated\" value=\"yes\" %s></td></tr>\n",
			(is_moderatedlist(&RoomFlags))?"checked":"");


		wprintf("<tr><td colspan=\"2\" align=\"center\">"
			"<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\"></td></tr>", _("Save changes"));
		wprintf("</table></form>");
			

		wprintf("</CENTER>\n");
		wprintf("</div>");
	}


	/* Configuration of The Dreaded Auto-Purger */
	if (!strcmp(tab, "expire")) {
		wprintf("<div class=\"tabcontent\">");

		serv_puts("GPEX room");
		serv_getln(buf, sizeof buf);
		if (!strncmp(buf, "550", 3)) {
			wprintf("<br><br><div align=center>%s</div><br><br>\n",
				_("Higher access is required to access this function.")
			);
		}
		else if (buf[0] != '2') {
			wprintf("<br><br><div align=center>%s</div><br><br>\n", &buf[4]);
		}
		else {
			roompolicy = extract_int(&buf[4], 0);
			roomvalue = extract_int(&buf[4], 1);
		
			serv_puts("GPEX floor");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				floorpolicy = extract_int(&buf[4], 0);
				floorvalue = extract_int(&buf[4], 1);
			}
			
			wprintf("<br /><form method=\"POST\" action=\"set_room_policy\">\n");
			wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
			wprintf("<table border=0 cellspacing=5>\n");
			wprintf("<tr><td>");
			wprintf(_("Message expire policy for this room"));
			wprintf("<br />(");
			escputs(WC->wc_roomname);
			wprintf(")</td><td>");
			wprintf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"0\" %s>",
				((roompolicy == 0) ? "CHECKED" : "") );
			wprintf(_("Use the default policy for this floor"));
			wprintf("<br />\n");
			wprintf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"1\" %s>",
				((roompolicy == 1) ? "CHECKED" : "") );
			wprintf(_("Never automatically expire messages"));
			wprintf("<br />\n");
			wprintf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"2\" %s>",
				((roompolicy == 2) ? "CHECKED" : "") );
			wprintf(_("Expire by message count"));
			wprintf("<br />\n");
			wprintf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"3\" %s>",
				((roompolicy == 3) ? "CHECKED" : "") );
			wprintf(_("Expire by message age"));
			wprintf("<br />");
			wprintf(_("Number of messages or days: "));
			wprintf("<input type=\"text\" NAME=\"roomvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", roomvalue);
			wprintf("</td></tr>\n");
	
			if (WC->axlevel >= 6) {
				wprintf("<tr><td COLSPAN=2><hr /></td></tr>\n");
				wprintf("<tr><td>");
				wprintf(_("Message expire policy for this floor"));
				wprintf("<br />(");
				escputs(floorlist[WC->wc_floor]);
				wprintf(")</td><td>");
				wprintf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"0\" %s>",
					((floorpolicy == 0) ? "CHECKED" : "") );
				wprintf(_("Use the system default"));
				wprintf("<br />\n");
				wprintf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"1\" %s>",
					((floorpolicy == 1) ? "CHECKED" : "") );
				wprintf(_("Never automatically expire messages"));
				wprintf("<br />\n");
				wprintf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"2\" %s>",
					((floorpolicy == 2) ? "CHECKED" : "") );
				wprintf(_("Expire by message count"));
				wprintf("<br />\n");
				wprintf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"3\" %s>",
					((floorpolicy == 3) ? "CHECKED" : "") );
				wprintf(_("Expire by message age"));
				wprintf("<br />");
				wprintf(_("Number of messages or days: "));
				wprintf("<input type=\"text\" NAME=\"floorvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">",
					floorvalue);
			}
	
			wprintf("<CENTER>\n");
			wprintf("<tr><td COLSPAN=2><hr /><CENTER>\n");
			wprintf("<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Save changes"));
			wprintf("&nbsp;");
			wprintf("<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
			wprintf("</CENTER></td><tr>\n");
	
			wprintf("</table>\n"
				"<input type=\"hidden\" NAME=\"tab\" VALUE=\"expire\">\n"
				"</form>\n"
			);
		}

		wprintf("</div>");
	}

	/* Access controls */
	if (!strcmp(tab, "access")) {
		wprintf("<div class=\"tabcontent\">");
		display_whok();
		wprintf("</div>");
	}

	/* Fetch messages from remote locations */
	if (!strcmp(tab, "feeds")) {
		wprintf("<div class=\"tabcontent\">");

		wprintf("<i>");
		wprintf(_("Retrieve messages from these remote POP3 accounts and store them in this room:"));
		wprintf("</i><br />\n");

		wprintf("<table class=\"altern\" border=0 cellpadding=5>"
			"<tr class=\"even\"><th>");
		wprintf(_("Remote host"));
		wprintf("</th><th>");
		wprintf(_("User name"));
		wprintf("</th><th>");
		wprintf(_("Password"));
		wprintf("</th><th>");
		wprintf(_("Keep messages on server?"));
		wprintf("</th><th>");
		wprintf(_("Interval"));
		wprintf("</th><th> </th></tr>");

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		bg = 1;
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			if (!strcasecmp(cmd, "pop3client")) {
				safestrncpy(recp, &buf[11], sizeof recp);

                                bg = 1 - bg;
                                wprintf("<tr class=\"%s\">",
                                        (bg ? "even" : "odd")
                                );

				wprintf("<td>");
				extract_token(pop3_host, buf, 1, '|', sizeof pop3_host);
				escputs(pop3_host);
				wprintf("</td>");

				wprintf("<td>");
				extract_token(pop3_user, buf, 2, '|', sizeof pop3_user);
				escputs(pop3_user);
				wprintf("</td>");

				wprintf("<td>*****</td>");		/* Don't show the password */

				wprintf("<td>%s</td>", extract_int(buf, 4) ? _("Yes") : _("No"));

				wprintf("<td>%ld</td>", extract_long(buf, 5));	// Fetching interval
			
				wprintf("<td class=\"button_link\">");
				wprintf(" <a href=\"netedit&cmd=remove&tab=feeds&line=pop3client|");
				urlescputs(recp);
				wprintf("\">");
				wprintf(_("(remove)"));
				wprintf("</a></td>");
			
				wprintf("</tr>");
			}
		}

		wprintf("<form method=\"POST\" action=\"netedit\">\n"
			"<tr>"
			"<input type=\"hidden\" name=\"tab\" value=\"feeds\">"
			"<input type=\"hidden\" name=\"prefix\" value=\"pop3client|\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<td>");
		wprintf("<input type=\"text\" id=\"add_as_pop3host\" NAME=\"line_pop3host\">\n");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"text\" id=\"add_as_pop3user\" NAME=\"line_pop3user\">\n");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"password\" id=\"add_as_pop3pass\" NAME=\"line_pop3pass\">\n");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"checkbox\" id=\"add_as_pop3keep\" NAME=\"line_pop3keep\" VALUE=\"1\">");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"text\" id=\"add_as_pop3int\" NAME=\"line_pop3int\" MAXLENGTH=\"5\">");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wprintf("</td></tr>");
		wprintf("</form></table>\n");

		wprintf("<hr>\n");

		wprintf("<i>");
		wprintf(_("Fetch the following RSS feeds and store them in this room:"));
		wprintf("</i><br />\n");

		wprintf("<table class=\"altern\" border=0 cellpadding=5>"
			"<tr class=\"even\"><th>");
		wprintf("<img src=\"static/rss_16x.png\" width=\"16\" height=\"16\" alt=\" \"> ");
		wprintf(_("Feed URL"));
		wprintf("</th><th>");
		wprintf("</th></tr>");

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		bg = 1;
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			if (!strcasecmp(cmd, "rssclient")) {
				safestrncpy(recp, &buf[10], sizeof recp);

                                bg = 1 - bg;
                                wprintf("<tr class=\"%s\">",
                                        (bg ? "even" : "odd")
                                );

				wprintf("<td>");
				extract_token(pop3_host, buf, 1, '|', sizeof pop3_host);
				escputs(pop3_host);
				wprintf("</td>");

				wprintf("<td class=\"button_link\">");
				wprintf(" <a href=\"netedit&cmd=remove&tab=feeds&line=rssclient|");
				urlescputs(recp);
				wprintf("\">");
				wprintf(_("(remove)"));
				wprintf("</a></td>");
			
				wprintf("</tr>");
			}
		}

		wprintf("<form method=\"POST\" action=\"netedit\">\n"
			"<tr>"
			"<input type=\"hidden\" name=\"tab\" value=\"feeds\">"
			"<input type=\"hidden\" name=\"prefix\" value=\"rssclient|\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<td>");
		wprintf("<input type=\"text\" id=\"add_as_pop3host\" size=\"72\" "
			"maxlength=\"256\" name=\"line_pop3host\">\n");
		wprintf("</td>");
		wprintf("<td>");
		wprintf("<input type=\"submit\" name=\"add_button\" value=\"%s\">", _("Add"));
		wprintf("</td></tr>");
		wprintf("</form></table>\n");

		wprintf("</div>");
	}


	/* end content of whatever tab is open now */
	wprintf("</div>\n");

	address_book_popup();
	wDumpContent(1);
}


/* 
 * Toggle self-service list subscription
 */
void toggle_self_service(void) {
	room_states RoomFlags;

	get_roomflags (&RoomFlags);

	if (yesbstr("QR2_SelfList")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SELFLIST;
	else 
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SELFLIST;

	if (yesbstr("QR2_SMTP_PUBLIC")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SMTP_PUBLIC;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SMTP_PUBLIC;

	if (yesbstr("QR2_Moderated")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_MODERATED;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_MODERATED;
	if (yesbstr("QR2_SubsOnly")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SMTP_PUBLIC;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SMTP_PUBLIC;

	set_roomflags (&RoomFlags);
	
	display_editroom();
}



/*
 * save new parameters for a room
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


	if (!havebstr("ok_button")) {
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
	if (IsEmptyStr(er_roomaide)) {
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
	if (!IsEmptyStr(buf)) {
		strcpy(er_name, buf);
	}

	strcpy(buf, bstr("er_password"));
	buf[10] = 0;
	if (!IsEmptyStr(buf))
		strcpy(er_password, buf);

	strcpy(buf, bstr("er_dirname"));
	buf[15] = 0;
	if (!IsEmptyStr(buf))
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
	if (!strcmp(buf, "personal")) {
		er_flags |= QR_MAILBOX;
	} else {
		er_flags &= ~QR_MAILBOX;
	}
	
	if (yesbstr("prefonly")) {
		er_flags |= QR_PREFONLY;
	} else {
		er_flags &= ~QR_PREFONLY;
	}

	if (yesbstr("readonly")) {
		er_flags |= QR_READONLY;
	} else {
		er_flags &= ~QR_READONLY;
	}

	
	if (yesbstr("collabdel")) {
		er_flags2 |= QR2_COLLABDEL;
	} else {
		er_flags2 &= ~QR2_COLLABDEL;
	}

	if (yesbstr("permanent")) {
		er_flags |= QR_PERMANENT;
	} else {
		er_flags &= ~QR_PERMANENT;
	}

	if (yesbstr("subjectreq")) {
		er_flags2 |= QR2_SUBJECTREQ;
	} else {
		er_flags2 &= ~QR2_SUBJECTREQ;
	}

	if (yesbstr("network")) {
		er_flags |= QR_NETWORK;
	} else {
		er_flags &= ~QR_NETWORK;
	}

	if (yesbstr("directory")) {
		er_flags |= QR_DIRECTORY;
	} else {
		er_flags &= ~QR_DIRECTORY;
	}

	if (yesbstr("ulallowed")) {
		er_flags |= QR_UPLOAD;
	} else {
		er_flags &= ~QR_UPLOAD;
	}

	if (yesbstr("dlallowed")) {
		er_flags |= QR_DOWNLOAD;
	} else {
		er_flags &= ~QR_DOWNLOAD;
	}

	if (yesbstr("visdir")) {
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

	er_floor = ibstr("er_floor");

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

	if (!IsEmptyStr(er_roomaide)) {
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


/*
 * Display form for Invite, Kick, and show Who Knows a room
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

        if (havebstr("kick_button")) {
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

	if (havebstr("invite_button")) {
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



/*
 * Display form for Invite, Kick, and show Who Knows a room
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

        
	wprintf("<table border=0 CELLSPACING=10><tr VALIGN=TOP><td>");
	wprintf(_("The users listed below have access to this room.  "
		"To remove a user from the access list, select the user "
		"name from the list and click 'Kick'."));
	wprintf("<br /><br />");
	
        wprintf("<CENTER><form method=\"POST\" action=\"do_invt_kick\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
        wprintf("<select NAME=\"username\" SIZE=\"10\" style=\"width:100%%\">\n");
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
        wprintf("</select><br />\n");

        wprintf("<input type=\"submit\" name=\"kick_button\" value=\"%s\">", _("Kick"));
        wprintf("</form></CENTER>\n");

	wprintf("</td><td>");
	wprintf(_("To grant another user access to this room, enter the "
		"user name in the box below and click 'Invite'."));
	wprintf("<br /><br />");

        wprintf("<CENTER><form method=\"POST\" action=\"do_invt_kick\">\n");
	wprintf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wprintf(_("Invite:"));
	wprintf(" ");
        wprintf("<input type=\"text\" name=\"username\" id=\"username_id\" style=\"width:100%%\"><br />\n"
        	"<input type=\"hidden\" name=\"invite_button\" value=\"Invite\">"
        	"<input type=\"submit\" value=\"%s\">"
		"</form></CENTER>\n", _("Invite"));
		/* Pop open an address book -- begin **/
		wprintf(
			"<a href=\"javascript:PopOpenAddressBook('username_id|%s');\" "
			"title=\"%s\">"
			"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
			"&nbsp;%s</a>",
			_("User"), 
			_("Users"), _("Users")
		);
		/* Pop open an address book -- end **/

	wprintf("</td></tr></table>\n");
	address_book_popup();
        wDumpContent(1);
}



/*
 * display the form for entering a new room
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

	output_headers(1, 1, 1, 0, 0, 0);

	svprintf(HKEY("BOXTITLE"), WCS_STRING, _("Create a new room"));
	do_template("beginbox", NULL);

	wprintf("<form name=\"create_room_form\" method=\"POST\" action=\"entroom\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf("<table class=\"altern\"> ");

	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Name of room: "));
	wprintf("</td><td>");
	wprintf("<input type=\"text\" NAME=\"er_name\" MAXLENGTH=\"127\">\n");
        wprintf("</td></tr>");

	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Resides on floor: "));
	wprintf("</td><td>");
        load_floorlist(); 
        wprintf("<select name=\"er_floor\" size=\"1\">\n");
        for (i = 0; i < 128; ++i)
                if (!IsEmptyStr(floorlist[i])) {
                        wprintf("<option ");
                        wprintf("value=\"%d\">", i);
                        escputs(floorlist[i]);
                        wprintf("</option>\n");
                }
        wprintf("</select>\n");
        wprintf("</td></tr>");

		/*
		 * Our clever little snippet of JavaScript automatically selects
		 * a public room if the view is set to Bulletin Board or wiki, and
		 * it selects a mailbox room otherwise.  The user can override this,
		 * of course.  We also disable the floor selector for mailboxes.
		 */
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Default view for room: "));
	wprintf("</td><td>");
        wprintf("<select name=\"er_view\" size=\"1\" OnChange=\""
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
			wprintf("<option %s value=\"%d\">",
				((i == 0) ? "selected" : ""), i );
			escputs(viewdefs[i]);
			wprintf("</option>\n");
		}
	}
	wprintf("</select>\n");
	wprintf("</td></tr>");

	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Type of room:"));
	wprintf("</td><td>");
	wprintf("<ul class=\"adminlist\">\n");

	wprintf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"public\" ");
	wprintf("CHECKED OnChange=\""
		"	if (this.form.type[0].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Public (automatically appears to everyone)"));
	wprintf("</li>");

	wprintf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"hidden\" OnChange=\""
		"	if (this.form.type[1].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - hidden (accessible to anyone who knows its name)"));
	wprintf("</li>");

	wprintf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"passworded\" OnChange=\""
		"	if (this.form.type[2].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - require password: "));
	wprintf("<input type=\"text\" NAME=\"er_password\" MAXLENGTH=\"9\">\n");
	wprintf("</li>");

	wprintf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"invonly\" OnChange=\""
		"	if (this.form.type[3].checked == true) {	"
		"		this.form.er_floor.disabled = false;	"
		"	}						"
		"\"> ");
	wprintf(_("Private - invitation only"));
	wprintf("</li>");

	wprintf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"personal\" "
		"OnChange=\""
		"	if (this.form.type[4].checked == true) {	"
		"		this.form.er_floor.disabled = true;	"
		"	}						"
		"\"> ");
	wprintf(_("Personal (mailbox for you only)"));
	wprintf("</li>");

	wprintf("\n</ul>\n");
	wprintf("</td></tr></table>\n");

	wprintf("<div class=\"buttons\">\n");
	wprintf("<input type=\"submit\" name=\"ok_button\" value=\"%s\">", _("Create new room"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">", _("Cancel"));
	wprintf("</div>\n");
	wprintf("</form>\n<hr />");
	serv_printf("MESG roomaccess");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		fmout("LEFT");
	}

	do_template("endbox", NULL);

	wDumpContent(1);
}




/*
 * support function for entroom() -- sets the default view 
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



/*
 * Create a new room
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

	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  No new room was created."));
		display_main_menu();
		return;
	}
	strcpy(er_name, bstr("er_name"));
	strcpy(er_type, bstr("type"));
	strcpy(er_password, bstr("er_password"));
	er_floor = ibstr("er_floor");
	er_view = ibstr("er_view");

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
	/** TODO: Room created, now udate the left hand icon bar for this user */
	burn_folder_cache(0);	/* burn the old folder cache */
	
	
	gotoroom(er_name);
	do_change_view(er_view);		/* Now go there */
}


/**
 * \brief display the screen to enter a private room
 */
void display_private(char *rname, int req_pass)
{
	StrBuf *Buf;
	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Go to a hidden room"), -1);
	DoTemplate(HKEY("beginbox"), NULL, Buf, CTX_STRBUF);

	FreeStrBuf(&Buf);

	wprintf("<p>");
	wprintf(_("If you know the name of a hidden (guess-name) or "
		"passworded room, you can enter that room by typing "
		"its name below.  Once you gain access to a private "
		"room, it will appear in your regular room listings "
		"so you don't have to keep returning here."));
	wprintf("</p>");

	wprintf("<form method=\"post\" action=\"goto_private\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf("<table class=\"altern\"> "
		"<tr class=\"even\"><td>");
	wprintf(_("Enter room name:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"gr_name\" "
		"value=\"%s\" maxlength=\"128\">\n", rname);

	if (req_pass) {
		wprintf("</td></tr><tr class=\"odd\"><td>");
		wprintf(_("Enter room password:"));
		wprintf("</td><td>");
		wprintf("<input type=\"password\" name=\"gr_pass\" maxlength=\"9\">\n");
	}
	wprintf("</td></tr></table>\n");

	wprintf("<div class=\"buttons\">\n");
	wprintf("<input type=\"submit\" name=\"ok_button\" value=\"%s\">"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">",
		_("Go there"),
		_("Cancel")
	);
	wprintf("</div></form>\n");

	do_template("endbox", NULL);

	wDumpContent(1);
}

/**
 * \brief goto a private room
 */
void goto_private(void)
{
	char hold_rm[SIZ];
	char buf[SIZ];

	if (!havebstr("ok_button")) {
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
	wprintf("<h1>");
	wprintf(_("Zap (forget/unsubscribe) the current room"));
	wprintf("</h1>\n");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf(_("If you select this option, <em>%s</em> will "
		"disappear from your room list.  Is this what you wish "
		"to do?<br />\n"), WC->wc_roomname);

	wprintf("<form method=\"POST\" action=\"zap\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Zap this room"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wprintf("</form>\n");
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

	if (havebstr("ok_button")) {
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
	burn_folder_cache(0);	/* Burn the cahce of known rooms to update the icon bar */
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
	int i, num_addrs;
	// TODO: do line dynamic!
	if (havebstr("line_pop3host")) {
		strcpy(line, bstr("prefix"));
		strcat(line, bstr("line_pop3host"));
		strcat(line, "|");
		strcat(line, bstr("line_pop3user"));
		strcat(line, "|");
		strcat(line, bstr("line_pop3pass"));
		strcat(line, "|");
		strcat(line, ibstr("line_pop3keep") ? "1" : "0" );
		strcat(line, "|");
		sprintf(&line[strlen(line)],"%ld", lbstr("line_pop3int"));
		strcat(line, bstr("suffix"));
	}
	else if (havebstr("line")) {
		strcpy(line, bstr("prefix"));
		strcat(line, bstr("line"));
		strcat(line, bstr("suffix"));
	}
	else {
		display_editroom();
		return;
	}


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

	if (havebstr("add_button")) {
		num_addrs = num_tokens(bstr("line"), ',');
		if (num_addrs < 2) {
			/* just adding one node or address */
			serv_puts(line);
		}
		else {
			/* adding multiple addresses separated by commas */
			for (i=0; i<num_addrs; ++i) {
				strcpy(line, bstr("prefix"));
				extract_token(buf, bstr("line"), i, ',', sizeof buf);
				striplt(buf);
				strcat(line, buf);
				strcat(line, bstr("suffix"));
				serv_puts(line);
			}
		}
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
	int i, len;

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
		if (floor > MAX_FLOORS) {
			wc_backtrace ();
			sprintf(folder, "%%%%%%|%s", room);
		}
		else {
			sprintf(folder, "%s|%s", floorlist[floor], room);
		}
	}

	/**
	 * Replace "\" characters with "|" for pseudo-folder-delimiting
	 */
	len = strlen (folder);
	for (i=0; i<len; ++i) {
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

	view = lbstr("view");
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
			int len;
			len = strlen(fold[i].name);
			if ( (!strncasecmp(fold[i].name, fold[i+1].name, len))
			   && (fold[i+1].name[len] == '|') ) {
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
			wprintf("<span class=\"roomlist_floor\">");
		}
		else if (fold[i].hasnewmsgs) {
			wprintf("<span class=\"roomlist_new\">");
		}
		else {
			wprintf("<span class=\"roomlist_old\">");
		}
		extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
		escputs(buf);
		wprintf("</span>");

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
	wprintf("<table BORDER=0 WIDTH=96%% CELLPADDING=5>"
		"<tr><td valign=top>");

	levels = 0;
	oldlevels = 0;
	for (i=0; i<max_folders; ++i) {

		levels = num_tokens(fold[i].name, '|');
		extract_token(floor_name, fold[i].name, 0,
			'|', sizeof floor_name);

		if ( (strcasecmp(floor_name, old_floor_name))
		   && (!IsEmptyStr(old_floor_name)) ) {
			/* End inner box */
			do_template("endbox", NULL);
			wprintf("<br>");

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
			StrBuf *Buf;
			
			Buf = NewStrBufPlain(floor_name, -1);
			DoTemplate(HKEY("beginbox"), NULL, Buf, CTX_STRBUF);
			
			FreeStrBuf(&Buf);
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
				wprintf("<span class=\"roomlist_new\">");
			}
			else {
				wprintf("<span class=\"roomlist_old\">");
			}
			extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
			escputs(buf);
			wprintf("</span>");
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
	do_template("endbox", NULL);

	wprintf("</td></tr></table>\n");
}

/**
 * \brief print a floor div???
 * \param which_floordiv name of the floordiv???
 */
void set_floordiv_expanded(void) {
	wcsession *WCC = WC;
	StrBuf *FloorDiv;
	
	FloorDiv = NewStrBuf();
	StrBufAppendBuf(FloorDiv, WCC->UrlFragment2, 0);
	set_preference("floordiv_expanded", FloorDiv, 1);
	WCC->floordiv_expanded = FloorDiv;
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
		   && (!IsEmptyStr(old_floor_name)) ) {
			/** End inner box */
			wprintf("<br>\n");
			wprintf("</div>\n");	/** floordiv */
		}
		strcpy(old_floor_name, floor_name);

		if (levels == 1) {
			/** Begin floor */
			stresc(floordivtitle, 256, floor_name, 0, 0);
			sprintf(floordiv_id, "floordiv%d", i);
			wprintf("<span class=\"ib_roomlist_floor\" "
				"onClick=\"expand_floor('%s')\">"
				"%s</span><br>\n", floordiv_id, floordivtitle);
			wprintf("<div id=\"%s\" style=\"display:%s\">",
				floordiv_id,
				(!strcasecmp(floordiv_id, ChrPtr(WC->floordiv_expanded)) ? "block" : "none")
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
				wprintf("<img  border=0 src=\"static/%s\" alt=\"\"> ", icon);
			}
			else {
				wprintf("<i>");
			}
			if (fold[i].hasnewmsgs) {
				wprintf("<span class=\"ib_roomlist_new\">");
			}
			else {
				wprintf("<span class=\"ib_roomlist_old\">");
			}
			extract_token(buf, fold[i].name, levels-1, '|', sizeof buf);
			escputs(buf);
			if (!strcasecmp(fold[i].name, "My Folders|Mail")) {
				wprintf(" (INBOX)");
			}
			wprintf("</span>");
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


}



/**
 * \brief Burn the cached folder list.  
 * \param age How old the cahce needs to be before we burn it.
 */

void burn_folder_cache(time_t age)
{
	/** If our cached folder list is very old, burn it. */
	if (WC->cache_fold != NULL) {
		if ((time(NULL) - WC->cache_timestamp) > age) {
			free(WC->cache_fold);
			WC->cache_fold = NULL;
		}
	}
}




/**
 * \brief Show the room list.  
 * (only should get called by
 * knrooms() because that's where output_headers() is called from)
 * \param viewpref the view preferences???
 */

void list_all_rooms_by_floor(const char *viewpref) {
	char buf[SIZ];
	int swap = 0;
	struct folder *fold = NULL;
	struct folder ftmp;
	int max_folders = 0;
	int alloc_folders = 0;
	int *floor_mapping;
	int IDMax;
	int i, j;
	int ShowEmptyFloors;
	int ra_flags = 0;
	int flags = 0;
	int num_floors = 1;	/** add an extra one for private folders */
	char buf3[SIZ];
	
	/** If our cached folder list is very old, burn it. */
	burn_folder_cache(300);
	
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
		extract_token(buf3, buf, 0, '|', SIZ);
		fold[max_folders].floor = atol (buf3);
		++max_folders;
		++num_floors;
	}
	IDMax = 0;
	for (i=0; i<num_floors; i++)
		if (IDMax < fold[i].floor)
			IDMax = fold[i].floor;
	floor_mapping = malloc (sizeof (int) * (IDMax + 1));
	memset (floor_mapping, 0, sizeof (int) * (IDMax + 1));
	for (i=0; i<num_floors; i++)
		floor_mapping[fold[i].floor]=i;
	
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
		/* Increase the room count for the associtaed floor */
		if (fold[max_folders].is_mailbox) {
			fold[0].num_rooms++;
		}
		else {
			i = floor_mapping[fold[max_folders].floor];
			fold[i].num_rooms++;
		}
		++max_folders;
	}
	
	/*
	 * Remove any floors that don't have rooms
	 */
	get_pref_yesno("emptyfloors", &ShowEmptyFloors, 0);
	if (ShowEmptyFloors)
	{
		for (i=0; i<num_floors; i++)
		{
        		if (fold[i].num_rooms == 0) {
                		for (j=i; j<max_folders; j++) {
                        		memcpy(&fold[j], &fold[j+1], sizeof(struct folder));
                		}
                		max_folders--;
                		num_floors--;
                		i--;
        		}
		}
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
	free(floor_mapping);
}


/**
 * \brief Do either a known rooms list or a folders list, depending on the
 * user's preference
 */
void knrooms(void)
{
	StrBuf *ListView = NULL;

	output_headers(1, 1, 2, 0, 0, 0);

	/** Determine whether the user is trying to change views */
	if (havebstr("view")) {
		ListView = NewStrBufPlain(bstr("view"), -1);
		set_preference("roomlistview", ListView, 1);
	}
	/** Sanitize the input so its safe */
	if(!get_preference("roomlistview", &ListView) ||
	   ((strcasecmp(ChrPtr(ListView), "folders") != 0) &&
	    (strcasecmp(ChrPtr(ListView), "table") != 0))) 
	{
		if (ListView == NULL) {
			ListView = NewStrBufPlain("rooms", sizeof("rooms") - 1);
			set_preference("roomlistview", ListView, 0);
		}
		else {
			StrBufPrintf(ListView, "rooms");
			save_preferences();
		}
	}

	/** title bar */
	wprintf("<div id=\"banner\">\n");
	wprintf("<div class=\"room_banner\">");
	wprintf("<h1>");
	if (!strcasecmp(ChrPtr(ListView), "rooms")) {
		wprintf(_("Room list"));
	}
	else if (!strcasecmp(ChrPtr(ListView), "folders")) {
		wprintf(_("Folder list"));
	}
	else if (!strcasecmp(ChrPtr(ListView), "table")) {
		wprintf(_("Room list"));
	}
	wprintf("</h1></div>\n");

	/** offer the ability to switch views */
	wprintf("<ul class=\"room_actions\">\n");
	wprintf("<li class=\"start_page\">");
	offer_start_page(NULL, 0, NULL, NULL, CTX_NONE);
	wprintf("</li>");
	wprintf("<li><form name=\"roomlistomatic\">\n"
		"<select name=\"newview\" size=\"1\" "
		"OnChange=\"location.href=roomlistomatic.newview.options"
		"[selectedIndex].value\">\n");

	wprintf("<option %s value=\"knrooms&view=rooms\">"
		"View as room list"
		"</option>\n",
		( !strcasecmp(ChrPtr(ListView), "rooms") ? "SELECTED" : "" )
	);

	wprintf("<option %s value=\"knrooms&view=folders\">"
		"View as folder list"
		"</option>\n",
		( !strcasecmp(ChrPtr(ListView), "folders") ? "SELECTED" : "" )
	);

	wprintf("</select>");
	wprintf("</form></li>");
	wprintf("</ul></div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	/** Display the room list in the user's preferred format */
	list_all_rooms_by_floor(ChrPtr(ListView));
	wDumpContent(1);
}



/**
 * \brief Set the message expire policy for this room and/or floor
 */
void set_room_policy(void) {
	char buf[SIZ];

	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  Changes were not saved."));
		display_editroom();
		return;
	}

	serv_printf("SPEX room|%d|%d", ibstr("roompolicy"), ibstr("roomvalue"));
	serv_getln(buf, sizeof buf);
	strcpy(WC->ImportantMessage, &buf[4]);

	if (WC->axlevel >= 6) {
		strcat(WC->ImportantMessage, "<br />\n");
		serv_printf("SPEX floor|%d|%d", ibstr("floorpolicy"), ibstr("floorvalue"));
		serv_getln(buf, sizeof buf);
		strcat(WC->ImportantMessage, &buf[4]);
	}

	display_editroom();
}


void tmplput_RoomName(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	StrBuf *tmp;
	tmp = NewStrBufPlain(WC->wc_roomname, -1);;
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, tmp, 0);
	FreeStrBuf(&tmp);
}

void _gotonext(void) { slrp_highest(); gotonext(); }
void dotskip(void) {smart_goto(bstr("room"));}
void _display_private(void) { display_private("", 0); }
void dotgoto(void) {
	if (WC->wc_view != VIEW_MAILBOX) {	/* dotgoto acts like dotskip when we're in a mailbox view */
		slrp_highest();
	}
	smart_goto(bstr("room"));
}

void tmplput_roombanner(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wprintf("<div id=\"banner\">\n");
	embed_room_banner(NULL, navbar_default);
	wprintf("</div>\n");
}


void tmplput_ungoto(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;

	if ((WCC!=NULL) && 
	    (!IsEmptyStr(WCC->ugname)))
		StrBufAppendBufPlain(Target, WCC->ugname, -1, 0);
}


int ConditionalHaveUngoto(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) && 
		(!IsEmptyStr(WCC->ugname)) && 
		(strcasecmp(WCC->ugname, WCC->wc_roomname) == 0));
}




int ConditionalRoomHas_QR_PERMANENT(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_PERMANENT) != 0));
}

int ConditionalRoomHas_QR_INUSE(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_INUSE) != 0));
}

int ConditionalRoomHas_QR_PRIVATE(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_PRIVATE) != 0));
}

int ConditionalRoomHas_QR_PASSWORDED(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_PASSWORDED) != 0));
}

int ConditionalRoomHas_QR_GUESSNAME(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_GUESSNAME) != 0));
}

int ConditionalRoomHas_QR_DIRECTORY(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_DIRECTORY) != 0));
}

int ConditionalRoomHas_QR_UPLOAD(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_UPLOAD) != 0));
}

int ConditionalRoomHas_QR_DOWNLOAD(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_DOWNLOAD) != 0));
}

int ConditionalRoomHas_QR_VISDIR(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_VISDIR) != 0));
}

int ConditionalRoomHas_QR_ANONONLY(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_ANONONLY) != 0));
}

int ConditionalRoomHas_QR_ANONOPT(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_ANONOPT) != 0));
}

int ConditionalRoomHas_QR_NETWORK(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_NETWORK) != 0));
}

int ConditionalRoomHas_QR_PREFONLY(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_PREFONLY) != 0));
}

int ConditionalRoomHas_QR_READONLY(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_READONLY) != 0));
}

int ConditionalRoomHas_QR_MAILBOX(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) &&
		((WCC->room_flags & QR_MAILBOX) != 0));
}






int ConditionalHaveRoomeditRights(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	wcsession *WCC = WC;

	return ( (WCC!= NULL) && 
		 ((WCC->axlevel >= 6) || 
		  (WCC->is_room_aide) || 
		  (WCC->is_mailbox) ));
}

void 
InitModule_ROOMOPS
(void)
{
	RegisterNamespace("ROOMNAME", 0, 1, tmplput_RoomName, 0);

	WebcitAddUrlHandler(HKEY("knrooms"), knrooms, 0);
	WebcitAddUrlHandler(HKEY("gotonext"), _gotonext, 0);
	WebcitAddUrlHandler(HKEY("skip"), gotonext, 0);
	WebcitAddUrlHandler(HKEY("ungoto"), ungoto, 0);
	WebcitAddUrlHandler(HKEY("dotgoto"), dotgoto, 0);
	WebcitAddUrlHandler(HKEY("dotskip"), dotskip, 0);
	WebcitAddUrlHandler(HKEY("display_private"), _display_private, 0);
	WebcitAddUrlHandler(HKEY("goto_private"), goto_private, 0);
	WebcitAddUrlHandler(HKEY("zapped_list"), zapped_list, 0);
	WebcitAddUrlHandler(HKEY("display_zap"), display_zap, 0);
	WebcitAddUrlHandler(HKEY("zap"), zap, 0);
	WebcitAddUrlHandler(HKEY("display_entroom"), display_entroom, 0);
	WebcitAddUrlHandler(HKEY("entroom"), entroom, 0);
	WebcitAddUrlHandler(HKEY("display_whok"), display_whok, 0);
	WebcitAddUrlHandler(HKEY("do_invt_kick"), do_invt_kick, 0);
	WebcitAddUrlHandler(HKEY("display_editroom"), display_editroom, 0);
	WebcitAddUrlHandler(HKEY("netedit"), netedit, 0);
	WebcitAddUrlHandler(HKEY("editroom"), editroom, 0);
	WebcitAddUrlHandler(HKEY("delete_room"), delete_room, 0);
	WebcitAddUrlHandler(HKEY("set_room_policy"), set_room_policy, 0);
	WebcitAddUrlHandler(HKEY("set_floordiv_expanded"), set_floordiv_expanded, NEED_URL|AJAX);
	WebcitAddUrlHandler(HKEY("changeview"), change_view, 0);
	WebcitAddUrlHandler(HKEY("toggle_self_service"), toggle_self_service, 0);
	RegisterNamespace("ROOMBANNER", 0, 1, tmplput_roombanner, 0);

	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_PERMANENT"), 0, ConditionalRoomHas_QR_PERMANENT, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_INUSE"), 0, ConditionalRoomHas_QR_INUSE, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_PRIVATE"), 0, ConditionalRoomHas_QR_PRIVATE, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_PASSWORDED"), 0, ConditionalRoomHas_QR_PASSWORDED, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_GUESSNAME"), 0, ConditionalRoomHas_QR_GUESSNAME, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_DIRECTORY"), 0, ConditionalRoomHas_QR_DIRECTORY, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_UPLOAD"), 0, ConditionalRoomHas_QR_UPLOAD, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_DOWNLOAD"), 0, ConditionalRoomHas_QR_DOWNLOAD, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_VISIDIR"), 0, ConditionalRoomHas_QR_VISDIR, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_ANONONLY"), 0, ConditionalRoomHas_QR_ANONONLY, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_ANONOPT"), 0, ConditionalRoomHas_QR_ANONOPT, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_NETWORK"), 0, ConditionalRoomHas_QR_NETWORK, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_PREFONLY"), 0, ConditionalRoomHas_QR_PREFONLY, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_READONLY"), 0, ConditionalRoomHas_QR_READONLY, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:QR_MAILBOX"), 0, ConditionalRoomHas_QR_MAILBOX, CTX_NONE);

	RegisterConditional(HKEY("COND:UNGOTO"), 0, ConditionalHaveUngoto, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:EDITACCESS"), 0, ConditionalHaveRoomeditRights, CTX_NONE);

	RegisterNamespace("ROOM:UNGOTO", 0, 0, tmplput_ungoto, 0);
}

/*@}*/
