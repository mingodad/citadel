/* $Id$ */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

struct folder {
	int floor;
	char room[SIZ];
	char name[SIZ];
	int hasnewmsgs;
	int is_mailbox;
	int selectable;
};

char *viewdefs[] = {
	"Messages",
	"Summary",
	"Address Book",
	"Calendar",
	"Tasks",
	"Notes"
};

char floorlist[128][SIZ];

/*
 * load the list of floors
 */
void load_floorlist(void)
{
	int a;
	char buf[SIZ];

	for (a = 0; a < 128; ++a)
		floorlist[a][0] = 0;

	serv_puts("LFLR");
	serv_gets(buf);
	if (buf[0] != '1') {
		strcpy(floorlist[0], "Main Floor");
		return;
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		extract(floorlist[extract_int(buf, 0)], buf, 1);
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

	wprintf("<A HREF=\"/dotgoto&room=");
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


/* 
 * Room ordering stuff (compare first by floor, then by order)
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


/*
 * Common code for all room listings
 */
void listrms(char *variety)
{
	char buf[SIZ];
	int num_rooms = 0;

	struct roomlisting *rl = NULL;
	struct roomlisting *rp;
	struct roomlisting *rs;


	/* Ask the server for a room list */
	serv_puts(variety);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("&nbsp;");
		return;
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		++num_rooms;
		rp = malloc(sizeof(struct roomlisting));
		extract(rp->rlname, buf, 0);
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

	/* If no rooms were listed, print an nbsp to make the cell
	 * borders show up anyway.
	 */
	if (num_rooms == 0) wprintf("&nbsp;");
}


/*
 * list all forgotten rooms
 */
void zapped_list(void)
{
	output_headers(1, 1, 0, 0, 0, 0, 0);

	svprintf("BOXTITLE", WCS_STRING, "Zapped (forgotten) rooms");
	do_template("beginbox");

	listrms("LZRM -1");

	wprintf("<br /><br />\n");
	wprintf("Click on any room to un-zap it and goto that room.\n");
	do_template("endbox");
	wDumpContent(1);
}


/*
 * read this room's info file (set v to 1 for verbose mode)
 */
void readinfo(void)
{
	char buf[SIZ];

	serv_puts("RINF");
	serv_gets(buf);
	if (buf[0] == '1') {
		fmout(NULL, "CENTER");
	}
}




/* Display room graphic.  The server doesn't actually
 * need the room name, but we supply it in order to
 * keep the browser from using a cached graphic from 
 * another room.
 */
void embed_room_graphic(void) {
	char buf[SIZ];

	serv_puts("OIMG _roompic_");
	serv_gets(buf);
	if (buf[0] == '2') {
		wprintf("<TD BGCOLOR=\"#444455\">");
		wprintf("<IMG HEIGHT=64 SRC=\"/image&name=_roompic_&room=");
		urlescputs(WC->wc_roomname);
		wprintf("\"></TD>");
		serv_puts("CLOS");
		serv_gets(buf);
	}

}


/* Let the user know if new mail has arrived 
 */
void embed_newmail_button(void) {
	if ( (WC->new_mail > WC->remember_new_mail) && (WC->new_mail>0) ) {
		wprintf(
			"<A HREF=\"/dotgoto?room=_MAIL_\">"
			"<IMG SRC=\"/static/mail.gif\" border=0 "
			"ALT=\"You have new mail\">"
			"<br /><SPAN CLASS=\"youhavemail\">"
			"%d new mail</SPAN></A>", WC->new_mail);
		WC->remember_new_mail = WC->new_mail;
	}
}



/*
 * Display the current view and offer an option to change it
 */
void embed_view_o_matic(void) {
	int i;

	wprintf("<FORM NAME=\"viewomatic\">\n"
		"<SELECT NAME=\"newview\" SIZE=\"1\" "
		"OnChange=\"location.href=viewomatic.newview.options"
		"[selectedIndex].value\">\n");

	for (i=0; i<(sizeof viewdefs / sizeof (char *)); ++i) {
		/*
		 * Only offer the views that make sense, given the default
		 * view for the room.  For example, don't offer a Calendar
		 * view in a non-Calendar room.
		 */
		if (
			(i == WC->wc_view)
		   ||	(i == WC->wc_default_view)
		   ||	( (i == 0) && (WC->wc_default_view == 1) )
		   ||	( (i == 1) && (WC->wc_default_view == 0) )
		) {

			wprintf("<OPTION %s VALUE=\"/changeview?view=%d\">",
				((i == WC->wc_view) ? "SELECTED" : ""),
				i );
			escputs(viewdefs[i]);
			wprintf("</OPTION>\n");
		}
	}
	wprintf("</SELECT></FORM>\n");
}



void embed_room_banner(char *got, int navbar_style) {
	char fakegot[SIZ];

	/* We need to have the information returned by a GOTO server command.
	 * If it isn't supplied, we fake it by issuing our own GOTO.
	 */
	if (got == NULL) {
		serv_printf("GOTO %s", WC->wc_roomname);
		serv_gets(fakegot);
		got = fakegot;
	}

	/* If the user happens to select the "make this my start page" link,
	 * we want it to remember the URL as a "/dotskip" one instead of
	 * a "skip" or "gotonext" or something like that.
	 */
	snprintf(WC->this_page, sizeof(WC->this_page), "/dotskip&room=%s",
		WC->wc_roomname);

	/* Check for new mail. */
	WC->new_mail = extract_int(&got[4], 9);
	WC->wc_view = extract_int(&got[4], 11);

	svprintf("ROOMNAME", WCS_STRING, "%s", WC->wc_roomname);
	svprintf("NEWMSGS", WCS_STRING, "%d", extract_int(&got[4], 1));
	svprintf("TOTALMSGS", WCS_STRING, "%d", extract_int(&got[4], 2));
	svcallback("ROOMPIC", embed_room_graphic);
	svcallback("ROOMINFO", readinfo);
	svcallback("YOUHAVEMAIL", embed_newmail_button);
	svcallback("VIEWOMATIC", embed_view_o_matic);
	svcallback("START", offer_start_page);

	do_template("roombanner");
	if (navbar_style != navbar_none) {

		wprintf("<div style=\"position:absolute; bottom:0px; left:0px\">\n"
			"<table border=\"0\" cellspacing=\"0\" cellpadding=\"0\" width=\"100%%\"><tr>\n");


		if (navbar_style == navbar_default) wprintf(
			"<td>"
			"<a href=\"/ungoto\">"
			"<img align=\"middle\" src=\"/static/back.gif\" border=\"0\">"
			"<span class=\"navbar_link\">Ungoto</span></A>"
			"</td>\n"
		);

		if ( (navbar_style == navbar_default)
		   && (WC->wc_view != VIEW_CALENDAR) 
		   && (WC->wc_view != VIEW_ADDRESSBOOK) 
		   && (WC->wc_view != VIEW_NOTES) 
		   && (WC->wc_view != VIEW_TASKS) 
		   ) {
			wprintf(
				"<td>"
				"<A HREF=\"/readnew\">"
				"<img align=\"middle\" src=\"/static/readmsgs.gif\" border=\"0\">"
				"<span class=\"navbar_link\">New messages</span></A>"
				"</td>\n"
			);
		}

		if (navbar_style == navbar_default) {
			switch(WC->wc_view) {
				case VIEW_ADDRESSBOOK:
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd\">"
						"<img align=\"middle\" src=\"/static/readmsgs.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"View contacts"
						"</span></a></td>\n"
					);
					break;
				case VIEW_CALENDAR:
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd?calview=day\">"
						"<img align=\"middle\" src=\"/static/day_view.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"Day view"
						"</span></a></td>\n"
					);
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd?calview=month\">"
						"<img align=\"middle\" src=\"/static/month_view.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"Month view"
						"</span></a></td>\n"
					);
					break;
				case VIEW_TASKS:
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd\">"
						"<img align=\"middle\" src=\"/static/readmsgs.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"View tasks"
						"</span></a></td>\n"
					);
					break;
				case VIEW_NOTES:
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd\">"
						"<img align=\"middle\" src=\"/static/readmsgs.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"View notes"
						"</span></a></td>\n"
					);
					break;
				default:
					wprintf(
						"<td>"
						"<A HREF=\"/readfwd\">"
						"<img align=\"middle\" src=\"/static/readmsgs.gif\" "
						"border=\"0\">"
						"<span class=\"navbar_link\">"
						"</span></a></td>\n"
					);
					break;
			}
		}

		if (navbar_style == navbar_default) {
			wprintf(
				"<td>"
				"<A HREF=\"/display_enter\">"
				"<img align=\"middle\" src=\"/static/enter.gif\" border=\"0\">"
				"<span class=\"navbar_link\">"
			);
			switch(WC->wc_view) {
				case VIEW_ADDRESSBOOK:
					wprintf("Add new contact");
					break;
				case VIEW_CALENDAR:
					wprintf("Add new event");
					break;
				case VIEW_TASKS:
					wprintf("Add new task");
					break;
				case VIEW_NOTES:
					wprintf("Add new note");
					break;
				default:
					wprintf("Enter a message");
					break;
			}
			wprintf("</span></a></td>\n");
		}

		if (navbar_style == navbar_default) wprintf(
			"<td>"
			"<A HREF=\"/skip\">"
			"<span class=\"navbar_link\">Skip this room</span>"
			"<img align=\"middle\" src=\"/static/forward.gif\" border=\"0\"></A>"
			"</td>\n"
		);

		if (navbar_style == navbar_default) wprintf(
			"<td>"
			"<A HREF=\"/gotonext\">"
			"<span class=\"navbar_link\">Goto next room</span>"
			"<img align=\"middle\" src=\"/static/forward.gif\" border=\"0\"></A>"
			"</td>\n"
		);

		wprintf("</tr></table></div>\n");
	}

}





/*
 * back end routine to take the session to a new room
 *
 */
void gotoroom(char *gname)
{
	char buf[SIZ];
	static long ls = (-1L);

	/* store ungoto information */
	strcpy(WC->ugname, WC->wc_roomname);
	WC->uglsn = ls;

	/* move to the new room */
	serv_printf("GOTO %s", gname);
	serv_gets(buf);
	if (buf[0] != '2') {
		serv_puts("GOTO _BASEROOM_");
		serv_gets(buf);
	}
	if (buf[0] != '2') {
		return;
	}
	extract(WC->wc_roomname, &buf[4], 0);
	WC->room_flags = extract_int(&buf[4], 4);
	/* highest_msg_read = extract_int(&buf[4],6);
	   maxmsgnum = extract_int(&buf[4],5);
	   is_mail = (char) extract_int(&buf[4],7); */
	ls = extract_long(&buf[4], 6);
	WC->wc_floor = extract_int(&buf[4], 10);
	WC->wc_view = extract_int(&buf[4], 11);
	WC->wc_default_view = extract_int(&buf[4], 12);

	if (WC->is_aide)
		WC->is_room_aide = WC->is_aide;
	else
		WC->is_room_aide = (char) extract_int(&buf[4], 8);

	remove_march(WC->wc_roomname);
	if (!strcasecmp(gname, "_BASEROOM_"))
		remove_march(gname);
}


/*
 * Locate the room on the march list which we most want to go to.  Each room
 * is measured given a "weight" of preference based on various factors.
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



/* Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 * We start the search in the current room rather than the beginning to prevent
 * two or more concurrent users from dragging each other back to the same room.
 */
void gotonext(void)
{
	char buf[SIZ];
	struct march *mptr, *mptr2;
	char next_room[128];

	/* First check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */

	if (WC->march == NULL) {
		serv_puts("LKRN");
		serv_gets(buf);
		if (buf[0] == '1')
			while (serv_gets(buf), strcmp(buf, "000")) {
				mptr = (struct march *) malloc(sizeof(struct march));
				mptr->next = NULL;
				extract(mptr->march_name, buf, 0);
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
/* add _BASEROOM_ to the end of the march list, so the user will end up
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


void smart_goto(char *next_room) {
	gotoroom(next_room);
	readloop("readnew");
}



/*
 * mark all messages in current room as having been read
 */
void slrp_highest(void)
{
	char buf[SIZ];

	/* set pointer */
	serv_puts("SLRP HIGHEST");
	serv_gets(buf);
	if (buf[0] != '2') {
		wprintf("<EM>%s</EM><br />\n", &buf[4]);
		return;
	}
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
	serv_gets(buf);
	if (buf[0] != '2') {
		smart_goto(WC->wc_roomname);
		return;
	}
	if (WC->uglsn >= 0L) {
		serv_printf("SLRP %ld", WC->uglsn);
		serv_gets(buf);
	}
	strcpy(buf, WC->ugname);
	strcpy(WC->ugname, "");
	smart_goto(buf);
}





/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * Set 'newval' to 0 to clear, 1 to set, any other value to leave unchanged.
 * Always returns the new value.
 */

int self_service(int newval) {
	int current_value = 0;
	char buf[SIZ];
	
	char name[SIZ];
	char password[SIZ];
	char dirname[SIZ];
	int flags, floor, order, view, flags2;

	serv_puts("GETR");
	serv_gets(buf);
	if (buf[0] != '2') return(0);

	extract(name, &buf[4], 0);
	extract(password, &buf[4], 1);
	extract(dirname, &buf[4], 2);
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
		serv_gets(buf);
	}

	return(newval);

}






/*
 * display the form for editing a room
 */
void display_editroom(void)
{
	char buf[SIZ];
	char cmd[SIZ];
	char node[SIZ];
	char remote_room[SIZ];
	char recp[SIZ];
	char er_name[20];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;
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
	serv_gets(buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	extract(er_name, &buf[4], 0);
	extract(er_password, &buf[4], 1);
	extract(er_dirname, &buf[4], 2);
	er_flags = extract_int(&buf[4], 3);
	er_floor = extract_int(&buf[4], 4);

	output_headers(1, 1, 1, 0, 0, 0, 0);

	/* print the tabbed dialog */
	wprintf("<br /><TABLE border=0 cellspacing=0 cellpadding=0 width=100%%>"
		"<TR ALIGN=CENTER>"
		"<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "admin")) {
		wprintf("<TD BGCOLOR=\"#FFFFFF\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD BGCOLOR=\"#CCCCCC\"><A HREF=\"/display_editroom&tab=admin\">");
	}
	wprintf("Administration");
	if (!strcmp(tab, "admin")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "config")) {
		wprintf("<TD BGCOLOR=\"#FFFFFF\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD BGCOLOR=\"#CCCCCC\"><A HREF=\"/display_editroom&tab=config\">");
	}
	wprintf("Configuration");
	if (!strcmp(tab, "config")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "expire")) {
		wprintf("<TD BGCOLOR=\"#FFFFFF\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD BGCOLOR=\"#CCCCCC\"><A HREF=\"/display_editroom&tab=expire\">");
	}
	wprintf("Message expire policy");
	if (!strcmp(tab, "expire")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "sharing")) {
		wprintf("<TD BGCOLOR=\"#FFFFFF\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD BGCOLOR=\"#CCCCCC\"><A HREF=\"/display_editroom&tab=sharing\">");
	}
	wprintf("Sharing");
	if (!strcmp(tab, "sharing")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	if (!strcmp(tab, "listserv")) {
		wprintf("<TD BGCOLOR=\"#FFFFFF\"><SPAN CLASS=\"tablabel\">");
	}
	else {
		wprintf("<TD BGCOLOR=\"#CCCCCC\"><A HREF=\"/display_editroom&tab=listserv\">");
	}
	wprintf("Mailing list service");
	if (!strcmp(tab, "listserv")) {
		wprintf("</SPAN></TD>\n");
	}
	else {
		wprintf("</A></TD>\n");
	}

	wprintf("<TD>&nbsp;</TD>\n");

	wprintf("</TR></TABLE>\n");
	/* end tabbed dialog */	

	/* begin content of whatever tab is open now */
	wprintf("<TABLE border=0 width=100%% bgcolor=\"#FFFFFF\">\n"
		"<TR><TD>\n");

	if (!strcmp(tab, "admin")) {
		wprintf("<UL>"
			"<LI><A HREF=\"/confirm_delete_room\">\n"
			"Delete this room</A>\n"
			"<LI><A HREF=\"/display_editroompic\">\n"
			"Set or change the graphic for this room's banner</A>\n"
			"<LI><A HREF=\"/display_editinfo\">\n"
			"Edit this room's Info file</A>\n"
			"</UL>");
	}

	if (!strcmp(tab, "config")) {
		wprintf("<FORM METHOD=\"POST\" ACTION=\"/editroom\">\n");
	
		wprintf("<UL><LI>Name of room: ");
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\"19\">\n", er_name);
	
		wprintf("<LI>Resides on floor: ");
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
	
		wprintf("<LI>Type of room:<UL>\n");

		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
		if ((er_flags & QR_PRIVATE) == 0)
		wprintf("CHECKED ");
		wprintf("> Public room\n");

		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"guessname\" ");
		if ((er_flags & QR_PRIVATE) &&
		    (er_flags & QR_GUESSNAME))
			wprintf("CHECKED ");
		wprintf("> Private - guess name\n");
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
		if ((er_flags & QR_PRIVATE) &&
		    (er_flags & QR_PASSWORDED))
			wprintf("CHECKED ");
		wprintf("> Private - require password:\n");
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_password\" VALUE=\"%s\" MAXLENGTH=\"9\">\n", er_password);
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
		if ((er_flags & QR_PRIVATE)
		    && ((er_flags & QR_GUESSNAME) == 0)
		    && ((er_flags & QR_PASSWORDED) == 0))
			wprintf("CHECKED ");
		wprintf("> Private - invitation only\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"bump\" VALUE=\"yes\" ");
		wprintf("> If private, cause current users to forget room\n");
	
		wprintf("</UL>\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"prefonly\" VALUE=\"yes\" ");
		if (er_flags & QR_PREFONLY)
			wprintf("CHECKED ");
		wprintf("> Preferred users only\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"readonly\" VALUE=\"yes\" ");
		if (er_flags & QR_READONLY)
			wprintf("CHECKED ");
		wprintf("> Read-only room\n");
	
	/* directory stuff */
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"directory\" VALUE=\"yes\" ");
		if (er_flags & QR_DIRECTORY)
			wprintf("CHECKED ");
		wprintf("> File directory room\n");

		wprintf("<UL><LI>Directory name: ");
		wprintf("<INPUT TYPE=\"text\" NAME=\"er_dirname\" VALUE=\"%s\" MAXLENGTH=\"14\">\n", er_dirname);

		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"ulallowed\" VALUE=\"yes\" ");
		if (er_flags & QR_UPLOAD)
			wprintf("CHECKED ");
		wprintf("> Uploading allowed\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"dlallowed\" VALUE=\"yes\" ");
		if (er_flags & QR_DOWNLOAD)
			wprintf("CHECKED ");
		wprintf("> Downloading allowed\n");
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"visdir\" VALUE=\"yes\" ");
		if (er_flags & QR_VISDIR)
			wprintf("CHECKED ");
		wprintf("> Visible directory</UL>\n");
	
	/* end of directory stuff */
	
		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"network\" VALUE=\"yes\" ");
		if (er_flags & QR_NETWORK)
			wprintf("CHECKED ");
		wprintf("> Network shared room\n");

		wprintf("<LI><INPUT TYPE=\"checkbox\" NAME=\"permanent\" VALUE=\"yes\" ");
		if (er_flags & QR_PERMANENT)
			wprintf("CHECKED ");
		wprintf("> Permanent (does not auto-purge)\n");

	/* start of anon options */
	
		wprintf("<LI>Anonymous messages<UL>\n");
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"no\" ");
		if (((er_flags & QR_ANONONLY) == 0)
		    && ((er_flags & QR_ANONOPT) == 0))
			wprintf("CHECKED ");
		wprintf("> No anonymous messages\n");
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anononly\" ");
		if (er_flags & QR_ANONONLY)
			wprintf("CHECKED ");
		wprintf("> All messages are anonymous\n");
	
		wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"anon\" VALUE=\"anon2\" ");
		if (er_flags & QR_ANONOPT)
			wprintf("CHECKED ");
		wprintf("> Prompt user when entering messages</UL>\n");
	
	/* end of anon options */
	
		wprintf("<LI>Room aide: \n");
		serv_puts("GETA");
		serv_gets(buf);
		if (buf[0] != '2') {
			wprintf("<EM>%s</EM>\n", &buf[4]);
		} else {
			extract(er_roomaide, &buf[4], 0);
			wprintf("<INPUT TYPE=\"text\" NAME=\"er_roomaide\" VALUE=\"%s\" MAXLENGTH=\"25\">\n", er_roomaide);
		}
	
		wprintf("</UL><CENTER>\n");
		wprintf("<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"config\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">"
			"&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">"
			"</CENTER>\n"
		);
	}


	/* Sharing the room with other Citadel nodes... */
	if (!strcmp(tab, "sharing")) {

		shared_with = strdup("");
		not_shared_with = strdup("");

		/* Learn the current configuration */
		serv_puts("CONF getsys|application/x-citadel-ignet-config");
		serv_gets(buf);
		if (buf[0]=='1') while (serv_gets(buf), strcmp(buf, "000")) {
			extract(node, buf, 0);
			not_shared_with = realloc(not_shared_with,
					strlen(not_shared_with) + 32);
			strcat(not_shared_with, node);
			strcat(not_shared_with, "\n");
		}

		serv_puts("GNET");
		serv_gets(buf);
		if (buf[0]=='1') while (serv_gets(buf), strcmp(buf, "000")) {
			extract(cmd, buf, 0);
			extract(node, buf, 1);
			extract(remote_room, buf, 2);
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
			extract_token(buf, shared_with, i, '\n');
			extract_token(node, buf, 0, '|');
			for (j=0; j<num_tokens(not_shared_with, '\n'); ++j) {
				extract_token(cmd, not_shared_with, j, '\n');
				if (!strcasecmp(node, cmd)) {
					remove_token(not_shared_with, j, '\n');
				}
			}
		}

		/* Display the stuff */
		wprintf("<CENTER><br />"
			"<TABLE border=1 cellpadding=5><TR>"
			"<TD><B><I>Shared with</I></B></TD>"
			"<TD><B><I>Not shared with</I></B></TD></TR>\n"
			"<TR><TD VALIGN=TOP>\n");

		wprintf("<TABLE border=0 cellpadding=5><TR BGCOLOR=\"#CCCCCC\">"
			"<TD>Remote node name</TD>"
			"<TD>Remote room name</TD>"
			"<TD>Actions</TD>"
			"</TR>\n"
		);

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n');
			extract_token(node, buf, 0, '|');
			extract_token(remote_room, buf, 1, '|');
			if (strlen(node) > 0) {
				wprintf("<FORM METHOD=\"POST\" "
					"ACTION=\"/netedit\">"
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
					"NAME=\"sc\" VALUE=\"Unshare\">");
				wprintf("</TD></TR></FORM>\n");
			}
		}

		wprintf("</TABLE>\n");
		wprintf("</TD><TD VALIGN=TOP>\n");
		wprintf("<TABLE border=0 cellpadding=5><TR BGCOLOR=\"#CCCCCC\">"
			"<TD>Remote node name</TD>"
			"<TD>Remote room name</TD>"
			"<TD>Actions</TD>"
			"</TR>\n"
		);

		for (i=0; i<num_tokens(not_shared_with, '\n'); ++i) {
			extract_token(node, not_shared_with, i, '\n');
			if (strlen(node) > 0) {
				wprintf("<FORM METHOD=\"POST\" "
					"ACTION=\"/netedit\">"
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
					"NAME=\"sc\" VALUE=\"Share\">");
				wprintf("</TD></TR></FORM>\n");
			}
		}

		wprintf("</TABLE>\n");
		wprintf("</TD></TR>"
			"</TABLE></CENTER><br />\n"
			"<I><B>Notes:</B><UL><LI>When sharing a room, "
			"it must be shared from both ends.  Adding a node to "
			"the 'shared' list sends messages out, but in order to"
			" receive messages, the other nodes must be configured"
			" to send messages out to your system as well.\n"
			"<LI>If the remote room name is blank, it is assumed "
			"that the room name is identical on the remote node."
			"<LI>If the remote room name is different, the remote "
			"node must also configure the name of the room here."
			"</UL></I><br />\n"
		);

	}

	/* Mailing list management */
	if (!strcmp(tab, "listserv")) {

		wprintf("<br /><center>"
			"<TABLE BORDER=0 WIDTH=100%% CELLPADDING=5>"
			"<TR><TD VALIGN=TOP>");

		wprintf("<i>The contents of this room are being "
			"mailed <b>as individual messages</b> "
			"to the following list recipients:"
			"</i><br /><br />\n");

		serv_puts("GNET");
		serv_gets(buf);
		if (buf[0]=='1') while (serv_gets(buf), strcmp(buf, "000")) {
			extract(cmd, buf, 0);
			if (!strcasecmp(cmd, "listrecp")) {
				extract(recp, buf, 1);
			
				escputs(recp);
				wprintf(" <A HREF=\"/netedit&cmd=remove&line="
					"listrecp|");
				urlescputs(recp);
				wprintf("&tab=listserv\">(remove)</A><br />");

			}
		}
		wprintf("<br /><FORM METHOD=\"POST\" ACTION=\"/netedit\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"prefix\" VALUE=\"listrecp|\">\n");
		wprintf("<INPUT TYPE=\"text\" NAME=\"line\">\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"cmd\" VALUE=\"Add\">");
		wprintf("</FORM>\n");

		wprintf("</TD><TD VALIGN=TOP>\n");
		
		wprintf("<i>The contents of this room are being "
			"mailed <b>in digest form</b> "
			"to the following list recipients:"
			"</i><br /><br />\n");

		serv_puts("GNET");
		serv_gets(buf);
		if (buf[0]=='1') while (serv_gets(buf), strcmp(buf, "000")) {
			extract(cmd, buf, 0);
			if (!strcasecmp(cmd, "digestrecp")) {
				extract(recp, buf, 1);
			
				escputs(recp);
				wprintf(" <A HREF=\"/netedit&cmd=remove&line="
					"digestrecp|");
				urlescputs(recp);
				wprintf("&tab=listserv\">(remove)</A><br />");

			}
		}
		wprintf("<br /><FORM METHOD=\"POST\" ACTION=\"/netedit\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"prefix\" VALUE=\"digestrecp|\">\n");
		wprintf("<INPUT TYPE=\"text\" NAME=\"line\">\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"cmd\" VALUE=\"Add\">");
		wprintf("</FORM>\n");
		
		wprintf("</TD></TR></TABLE><hr />\n");

		if (self_service(999) == 1) {
			wprintf("This room is configured to allow "
				"self-service subscribe/unsubscribe requests."
				" <A HREF=\"/toggle_self_service?newval=0&"
				"tab=listserv\">"
				"Click to disable.</A><br />\n"
				"The URL for subscribe/unsubscribe is: "
				"<TT>http://%s/listsub</TT><br />\n",
				WC->http_host
			);
		}
		else {
			wprintf("This room is <i>not</i> configured to allow "
				"self-service subscribe/unsubscribe requests."
				" <A HREF=\"/toggle_self_service?newval=1&"
				"tab=listserv\">"
				"Click to enable.</A><br />\n"
			);
		}


		wprintf("</CENTER>\n");
	}


	/* Mailing list management */
	if (!strcmp(tab, "expire")) {

		serv_puts("GPEX room");
		serv_gets(buf);
		if (buf[0] == '2') {
			roompolicy = extract_int(&buf[4], 0);
			roomvalue = extract_int(&buf[4], 1);
		}
		
		serv_puts("GPEX floor");
		serv_gets(buf);
		if (buf[0] == '2') {
			floorpolicy = extract_int(&buf[4], 0);
			floorvalue = extract_int(&buf[4], 1);
		}
		
		wprintf("<br /><FORM METHOD=\"POST\" ACTION=\"/set_room_policy\">\n");
		wprintf("<TABLE border=0 cellspacing=5>\n");
		wprintf("<TR><TD>Message expire policy for this room<br />(");
		escputs(WC->wc_roomname);
		wprintf(")</TD><TD>");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"0\" %s>",
			((roompolicy == 0) ? "CHECKED" : "") );
		wprintf("Use the default policy for this floor<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"1\" %s>",
			((roompolicy == 1) ? "CHECKED" : "") );
		wprintf("Never automatically expire messages<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"2\" %s>",
			((roompolicy == 2) ? "CHECKED" : "") );
		wprintf("Expire by message count<br />\n");
		wprintf("<INPUT TYPE=\"radio\" NAME=\"roompolicy\" VALUE=\"3\" %s>",
			((roompolicy == 3) ? "CHECKED" : "") );
		wprintf("Expire by message age<br />");
		wprintf("Number of messages or days: ");
		wprintf("<INPUT TYPE=\"text\" NAME=\"roomvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", roomvalue);
		wprintf("</TD></TR>\n");

		if (WC->axlevel >= 6) {
			wprintf("<TR><TD COLSPAN=2><hr /></TD></TR>\n");
			wprintf("<TR><TD>Message expire policy for this floor<br />(");
			escputs(floorlist[WC->wc_floor]);
			wprintf(")</TD><TD>");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"0\" %s>",
				((floorpolicy == 0) ? "CHECKED" : "") );
			wprintf("Use the system default<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"1\" %s>",
				((floorpolicy == 1) ? "CHECKED" : "") );
			wprintf("Never automatically expire messages<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"2\" %s>",
				((floorpolicy == 2) ? "CHECKED" : "") );
			wprintf("Expire by message count<br />\n");
			wprintf("<INPUT TYPE=\"radio\" NAME=\"floorpolicy\" VALUE=\"3\" %s>",
				((floorpolicy == 3) ? "CHECKED" : "") );
			wprintf("Expire by message age<br />");
			wprintf("Number of messages or days: ");
			wprintf("<INPUT TYPE=\"text\" NAME=\"floorvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">",
				floorvalue);
		}

		wprintf("<CENTER>\n");
		wprintf("<TR><TD COLSPAN=2><hr /><CENTER>\n");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
		wprintf("&nbsp;");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
		wprintf("</CENTER></TD><TR>\n");

		wprintf("</TABLE>\n"
			"<INPUT TYPE=\"hidden\" NAME=\"tab\" VALUE=\"expire\">\n"
			"</FORM>\n"
		);

	}


	/* end content of whatever tab is open now */
	wprintf("</TD></TR></TABLE>\n");

	wDumpContent(1);
}


/* 
 * Toggle self-service list subscription
 */
void toggle_self_service(void) {
	int newval = 0;

	newval = atoi(bstr("newval"));
	self_service(newval);
	display_editroom();
}



/*
 * save new parameters for a room
 */
void editroom(void)
{
	char buf[SIZ];
	char er_name[20];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	int er_floor;
	unsigned er_flags;
	int bump;


	if (strcmp(bstr("sc"), "OK")) {
		strcpy(WC->ImportantMessage,
			"Cancelled.  Changes were not saved.");
		display_editroom();
		return;
	}
	serv_puts("GETR");
	serv_gets(buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_editroom();
		return;
	}
	extract(er_name, &buf[4], 0);
	extract(er_password, &buf[4], 1);
	extract(er_dirname, &buf[4], 2);
	er_flags = extract_int(&buf[4], 3);

	strcpy(er_roomaide, bstr("er_roomaide"));
	if (strlen(er_roomaide) == 0) {
		serv_puts("GETA");
		serv_gets(buf);
		if (buf[0] != '2') {
			strcpy(er_roomaide, "");
		} else {
			extract(er_roomaide, &buf[4], 0);
		}
	}
	strcpy(buf, bstr("er_name"));
	buf[20] = 0;
	if (strlen(buf) > 0)
		strcpy(er_name, buf);

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
	if (!strcmp(buf, "guessname")) {
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

	sprintf(buf, "SETR %s|%s|%s|%u|%d|%d",
	     er_name, er_password, er_dirname, er_flags, bump, er_floor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_editroom();
		return;
	}
	gotoroom(er_name);

	if (strlen(er_roomaide) > 0) {
		sprintf(buf, "SETA %s", er_roomaide);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			strcpy(WC->ImportantMessage, &buf[4]);
			display_main_menu();
			return;
		}
	}
	gotoroom(er_name);
	strcpy(WC->ImportantMessage, "Your changes have been saved.");
	display_editroom();
	return;
}

/*
 * Invite, Kick, and show Who Knows a room
 */
void display_whok(void)
{
        char buf[SIZ], room[SIZ], username[SIZ];

        serv_puts("GETR");
        serv_gets(buf);

        if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
        }
        extract(room, &buf[4], 0);

        strcpy(username, bstr("username"));

        if(!strcmp(bstr("sc"), "Kick")) {
                sprintf(buf, "KICK %s", username);
                serv_puts(buf);
                serv_gets(buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
				"<B><I>User %s kicked out of room %s.</I></B>\n", 
                                username, room);
                }
        } else if(!strcmp(bstr("sc"), "Invite")) {
                sprintf(buf, "INVT %s", username);
                serv_puts(buf);
                serv_gets(buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
                        	"<B><I>User %s invited to room %s.</I></B>\n", 
                                username, room);
                }
        }
        
        output_headers(1, 1, 1, 0, 0, 0, 0);
	stresc(buf, WC->wc_roomname, 1, 1);
	svprintf("BOXTITLE", WCS_STRING, "Access control list for %s", buf);
	do_template("beginbox");

	wprintf("<TABLE border=0 CELLSPACING=10><TR VALIGN=TOP>"
		"<TD>The users listed below have access to this room.  "
		"To remove a user from the access list, select the user "
		"name from the list and click 'Kick'.<br /><br />");
	
        wprintf("<CENTER><FORM METHOD=\"POST\" ACTION=\"/display_whok\">\n");
        wprintf("<SELECT NAME=\"username\" SIZE=10>\n");
        serv_puts("WHOK");
        serv_gets(buf);
        if (buf[0] == '1') {
                while (serv_gets(buf), strcmp(buf, "000")) {
                        extract(username, buf, 0);
                        wprintf("<OPTION>");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</SELECT><br />\n");

        wprintf("<input type=submit name=sc value=\"Kick\">");
        wprintf("</FORM></CENTER>\n");

	wprintf("</TD><TD>"
		"To grant another user access to this room, enter the "
		"user name in the box below and click 'Invite'.<br /><br />");

        wprintf("<CENTER><FORM METHOD=\"POST\" ACTION=\"/display_whok\">\n");
        wprintf("Invite: ");
        wprintf("<input type=text name=username><br />\n"
        	"<input type=hidden name=sc value=\"Invite\">"
        	"<input type=submit value=\"Invite\">"
		"</FORM></CENTER>\n");

	wprintf("</TD></TR></TABLE>\n");
	do_template("endbox");
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
	serv_gets(buf);

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	output_headers(1, 1, 0, 0, 0, 0, 0);
	svprintf("BOXTITLE", WCS_STRING, "Create a new room");
	do_template("beginbox");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/entroom\">\n");

	wprintf("<UL><LI>Name of room: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" MAXLENGTH=\"127\">\n");

        wprintf("<LI>Resides on floor: ");
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

	wprintf("<LI>Type of room:<UL>\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"public\" ");
	wprintf("CHECKED > Public room\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"guessname\" ");
	wprintf("> Private - guess name\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
	wprintf("> Private - require password:\n");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_password\" MAXLENGTH=\"9\">\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
	wprintf("> Private - invitation only\n");

	wprintf("<LI><INPUT TYPE=\"radio\" NAME=\"type\" VALUE=\"personal\" ");
	wprintf("> Personal (mailbox for you only)\n");
	wprintf("</UL>\n");

	wprintf("<LI>Default view for room: "); /* FOO */
        wprintf("<SELECT NAME=\"er_view\" SIZE=\"1\">\n");
	for (i=0; i<(sizeof viewdefs / sizeof (char *)); ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">",
			((i == 0) ? "SELECTED" : ""), i );
		escputs(viewdefs[i]);
		wprintf("</OPTION>\n");
	}
	wprintf("</SELECT>\n");

	wprintf("<CENTER>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER>\n");
	wprintf("</FORM>\n<hr />");
	serv_printf("MESG roomaccess");
	serv_gets(buf);
	if (buf[0] == '1') {
		fmout(NULL, "CENTER");
	}
	do_template("endbox");
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
	serv_gets(buf);
	if (buf[0] != '2') return;

	extract(rm_name, &buf[4], 0);
	extract(rm_pass, &buf[4], 1);
	extract(rm_dir, &buf[4], 2);
	rm_bits1 = extract_int(&buf[4], 3);
	rm_floor = extract_int(&buf[4], 4);
	rm_listorder = extract_int(&buf[4], 5);
	rm_bits2 = extract_int(&buf[4], 7);

	serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
		rm_name, rm_pass, rm_dir, rm_bits1, rm_floor,
		rm_listorder, newview, rm_bits2
	);
	serv_gets(buf);
}



/*
 * enter a new room
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

	if (strcmp(bstr("sc"), "OK")) {
		strcpy(WC->ImportantMessage,
			"Cancelled.  No new room was created.");
		display_main_menu();
		return;
	}
	strcpy(er_name, bstr("er_name"));
	strcpy(er_type, bstr("type"));
	strcpy(er_password, bstr("er_password"));
	er_floor = atoi(bstr("er_floor"));
	er_view = atoi(bstr("er_view"));

	er_num_type = 0;
	if (!strcmp(er_type, "guessname"))
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
	serv_gets(buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	gotoroom(er_name);
	do_change_view(er_view);		/* Now go there */
}


/*
 * display the screen to enter a private room
 */
void display_private(char *rname, int req_pass)
{

	output_headers(1, 1, 0, 0, 0, 0, 0);

	svprintf("BOXTITLE", WCS_STRING, "Go to a hidden room");
	do_template("beginbox");

	wprintf("<CENTER>\n");
	wprintf("<br />If you know the name of a hidden (guess-name) or\n");
	wprintf("passworded room, you can enter that room by typing\n");
	wprintf("its name below.  Once you gain access to a private\n");
	wprintf("room, it will appear in your regular room listings\n");
	wprintf("so you don't have to keep returning here.\n");
	wprintf("<br /><br />");

	wprintf("<FORM METHOD=\"GET\" ACTION=\"/goto_private\">\n");

	wprintf("<table border=\"0\" cellspacing=\"5\" "
		"cellpadding=\"5\" BGCOLOR=\"#EEEEEE\">\n"
		"<TR><TD>"
		"Enter room name:</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"gr_name\" "
		"VALUE=\"%s\" MAXLENGTH=\"19\">\n", rname);

	if (req_pass) {
		wprintf("</TD></TR><TR><TD>");
		wprintf("Enter room password:</TD><TD>");
		wprintf("<INPUT TYPE=\"password\" NAME=\"gr_pass\" MAXLENGTH=\"9\">\n");
	}
	wprintf("</TD></TR></TABLE><br />\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM>\n");
	do_template("endbox");
	wDumpContent(1);
}

/* 
 * goto a private room
 */
void goto_private(void)
{
	char hold_rm[SIZ];
	char buf[SIZ];

	if (strcasecmp(bstr("sc"), "OK")) {
		display_main_menu();
		return;
	}
	strcpy(hold_rm, WC->wc_roomname);
	strcpy(buf, "GOTO ");
	strcat(buf, bstr("gr_name"));
	strcat(buf, "|");
	strcat(buf, bstr("gr_pass"));
	serv_puts(buf);
	serv_gets(buf);

	if (buf[0] == '2') {
		smart_goto(bstr("gr_name"));
		return;
	}
	if (!strncmp(buf, "540", 3)) {
		display_private(bstr("gr_name"), 1);
		return;
	}
	output_headers(1, 1, 1, 0, 0, 0, 0);
	wprintf("%s\n", &buf[4]);
	wDumpContent(1);
	return;
}


/*
 * display the screen to zap a room
 */
void display_zap(void)
{
	output_headers(1, 1, 2, 0, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#770000\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Zap (forget/unsubscribe) the current room</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("If you select this option, <em>%s</em> will ", WC->wc_roomname);
	wprintf("disappear from your room list.  Is this what you wish ");
	wprintf("to do?<br />\n");

	wprintf("<FORM METHOD=\"GET\" ACTION=\"/zap\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM>\n");
	wDumpContent(1);
}


/* 
 * zap a room
 */
void zap(void)
{
	char buf[SIZ];
	char final_destination[SIZ];

	/* If the forget-room routine fails for any reason, we fall back
	 * to the current room; otherwise, we go to the Lobby
	 */
	strcpy(final_destination, WC->wc_roomname);

	if (!strcasecmp(bstr("sc"), "OK")) {
		serv_printf("GOTO %s", WC->wc_roomname);
		serv_gets(buf);
		if (buf[0] == '2') {
			serv_puts("FORG");
			serv_gets(buf);
			if (buf[0] == '2') {
				strcpy(final_destination, "_BASEROOM_");
			}
		}
	}
	smart_goto(final_destination);
}




/*
 * Confirm deletion of the current room
 */
void confirm_delete_room(void)
{
	char buf[SIZ];

	serv_puts("KILL 0");
	serv_gets(buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#770000\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Confirm deletion of room</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<CENTER>");
	wprintf("<FORM METHOD=\"GET\" ACTION=\"/delete_room\">\n");

	wprintf("Are you sure you want to delete <FONT SIZE=+1>");
	escputs(WC->wc_roomname);
	wprintf("</FONT>?<br />\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Delete\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);
}


/*
 * Delete the current room
 */
void delete_room(void)
{
	char buf[SIZ];
	char sc[SIZ];

	strcpy(sc, bstr("sc"));

	if (strcasecmp(sc, "Delete")) {
		strcpy(WC->ImportantMessage,
			"Cancelled.  This room was not deleted.");
		display_main_menu();
		return;
	}
	serv_puts("KILL 1");
	serv_gets(buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	} else {
		smart_goto("_BASEROOM_");
	}
}



/*
 * Perform changes to a room's network configuration
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
	serv_gets(buf);
	if (buf[0] != '1') {
		fclose(fp);
		display_editroom();
		return;
	}

	/* This loop works for add *or* remove.  Spiffy, eh? */
	while (serv_gets(buf), strcmp(buf, "000")) {
		extract(cmpa0, buf, 0);
		extract(cmpa1, buf, 1);
		extract(cmpb0, line, 0);
		extract(cmpb1, line, 1);
		if ( (strcasecmp(cmpa0, cmpb0)) 
		   || (strcasecmp(cmpa1, cmpb1)) ) {
			fprintf(fp, "%s\n", buf);
		}
	}

	rewind(fp);
	serv_puts("SNET");
	serv_gets(buf);
	if (buf[0] != '4') {
		fclose(fp);
		display_editroom();
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		serv_puts(buf);
	}

	if (!strcasecmp(bstr("cmd"), "add")) {
		serv_puts(line);
	}

	serv_puts("000");
	fclose(fp);
	display_editroom();
}



/*
 * Convert a room name to a folder-ish-looking name.
 */
void room_to_folder(char *folder, char *room, int floor, int is_mailbox)
{
	int i;

	/*
	 * For mailboxes, just do it straight...
	 */
	if (is_mailbox) {
		sprintf(folder, "My folders|%s", room);
	}

	/*
	 * Otherwise, prefix the floor name as a "public folders" moniker
	 */
	else {
		sprintf(folder, "%s|%s", floorlist[floor], room);
	}

	/*
	 * Replace "\" characters with "|" for pseudo-folder-delimiting
	 */
	for (i=0; i<strlen(folder); ++i) {
		if (folder[i] == '\\') folder[i] = '|';
	}
}




/*
 * Back end for change_view()
 */
void do_change_view(int newview) {
	char buf[SIZ];

	serv_printf("VIEW %d", newview);
	serv_gets(buf);
	WC->wc_view = newview;
	smart_goto(WC->wc_roomname);
}



/*
 * Change the view for this room
 */
void change_view(void) {
	int view;

	view = atol(bstr("view"));
	do_change_view(view);
}


/*
 * One big expanded tree list view --- like a folder list
 */
void do_folder_view(struct folder *fold, int max_folders, int num_floors) {
	char buf[SIZ];
	int levels, oldlevels;
	int i, t;
	int actnum = 0;
	int has_subfolders = 0;

	/* Include the menu expanding/collapsing code */
	wprintf("<script type=\"text/javascript\" src=\"/static/menuExpandable3.js\"></script>\n");

	/* BEGIN TREE MENU */
	wprintf("<div id=\"mainMenu\">\n");
	wprintf("<UL id=\"menuList\">\n");
	levels = 0;
	oldlevels = 0;

	for (i=0; i<max_folders; ++i) {

		has_subfolders = 0;
		if ((i+1) < max_folders) {
			if ( (!strncasecmp(fold[i].name, fold[i+1].name, strlen(fold[i].name)))
			   && (fold[i+1].name[strlen(fold[i].name)] == '|') ) {
				has_subfolders = 1;
			}
		}

		levels = num_tokens(fold[i].name, '|');

		if ( (levels < oldlevels) || ((levels==1)&&(i!=0)) ) {
			for (t=0; t<(oldlevels-levels); ++t) {
				wprintf("</UL>\n");
			}
		}

		if (has_subfolders) {
			wprintf("<LI");
			if (levels == 1) wprintf(" class=\"menubar\"");
			wprintf(">");
			wprintf("<A href=\"#\" id=\"actuator%d\" class=\"actuator\"></a>\n", actnum);
		}
		else {
			wprintf("<LI>");
		}

		if (fold[i].selectable) {
			wprintf("<A HREF=\"/dotgoto?room=");
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
		extract(buf, fold[i].name, levels-1);
		escputs(buf);
		wprintf("</SPAN>");

		if (!strcasecmp(fold[i].name, "My Folders|Mail")) {
			wprintf(" (INBOX)");
		}

		if (fold[i].selectable) {
			wprintf("</A>");
		}
		wprintf("\n");

		if (has_subfolders) {
			wprintf("<UL id=\"menu%d\" class=\"%s\">\n",
				actnum++,
				( (levels == 1) ? "menu" : "submenu")
			);
		}

		oldlevels = levels;
	}
	wprintf("</UL></UL>\n");
	wprintf("<img src=\"/static/blank.gif\" onLoad = ' \n");
	for (i=0; i<actnum; ++i) {
		wprintf(" initializeMenu(\"menu%d\", \"actuator%d\");\n", i, i);
	}
	wprintf(" ' > \n");
	wprintf("</DIV>\n");
	/* END TREE MENU */
}

/*
 * Boxes and rooms and lists ... oh my!
 */
void do_rooms_view(struct folder *fold, int max_folders, int num_floors) {
	char buf[SIZ];
	char boxtitle[SIZ];
	int levels, oldlevels;
	int i, t;
	int num_boxes = 0;
	static int columns = 3;
	int boxes_per_column = 0;
	int current_column = 0;
	int nf;

	nf = num_floors;
	while (nf % columns != 0) ++nf;
	boxes_per_column = (nf / columns);
	if (boxes_per_column < 1) boxes_per_column = 1;

	/* Outer table (for columnization) */
	wprintf("<TABLE BORDER=0 WIDTH=96%% CELLPADDING=5>"
		"<tr><td valign=top>");

	levels = 0;
	oldlevels = 0;
	for (i=0; i<max_folders; ++i) {

		levels = num_tokens(fold[i].name, '|');

		if ((levels == 1) && (oldlevels > 1)) {

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

		if (levels == 1) {

			/* Begin inner box */
			extract(buf, fold[i].name, levels-1);
			stresc(boxtitle, buf, 1, 0);
			svprintf("BOXTITLE", WCS_STRING, boxtitle);
			do_template("beginbox");

		}

		oldlevels = levels;

		if (levels > 1) {
			wprintf("&nbsp;");
			if (levels>2) for (t=0; t<(levels-2); ++t) wprintf("&nbsp;&nbsp;&nbsp;");
			if (fold[i].selectable) {
				wprintf("<A HREF=\"/dotgoto?room=");
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
			extract(buf, fold[i].name, levels-1);
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
	/* End the final inner box */
	do_template("endbox");

	wprintf("</TD></TR></TABLE>\n");
}


/*
 * Show the room list.  (only should get called by
 * knrooms() because that's where output_headers() is called from)
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
	int num_floors = 1;	/* add an extra one for private folders */

	/* Start with the mailboxes */
	max_folders = 1;
	alloc_folders = 1;
	fold = malloc(sizeof(struct folder));
	memset(fold, 0, sizeof(struct folder));
	strcpy(fold[0].name, "My folders");
	fold[0].is_mailbox = 1;

	/* Then add floors */
	serv_puts("LFLR");
	serv_gets(buf);
	if (buf[0]=='1') while(serv_gets(buf), strcmp(buf, "000")) {
		if (max_folders >= alloc_folders) {
			alloc_folders = max_folders + 100;
			fold = realloc(fold,
				alloc_folders * sizeof(struct folder));
		}
		memset(&fold[max_folders], 0, sizeof(struct folder));
		extract(fold[max_folders].name, buf, 1);
		++max_folders;
		++num_floors;
	}

	/* Now add rooms */
	serv_puts("LKRA");
	serv_gets(buf);
	if (buf[0]=='1') while(serv_gets(buf), strcmp(buf, "000")) {
		if (max_folders >= alloc_folders) {
			alloc_folders = max_folders + 100;
			fold = realloc(fold,
				alloc_folders * sizeof(struct folder));
		}
		memset(&fold[max_folders], 0, sizeof(struct folder));
		extract(fold[max_folders].room, buf, 0);
		ra_flags = extract_int(buf, 5);
		flags = extract_int(buf, 1);
		fold[max_folders].floor = extract_int(buf, 2);
		fold[max_folders].hasnewmsgs =
			((ra_flags & UA_HASNEWMSGS) ? 1 : 0 );
		if (flags & QR_MAILBOX) {
			fold[max_folders].is_mailbox = 1;
		}
		room_to_folder(fold[max_folders].name,
				fold[max_folders].room,
				fold[max_folders].floor,
				fold[max_folders].is_mailbox);
		fold[max_folders].selectable = 1;
		++max_folders;
	}

	/* Bubble-sort the folder list */
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

/* test only hackish view 
	wprintf("<table><TR><TD>A Table</TD></TR></table>\n");
	for (i=0; i<max_folders; ++i) {
		escputs(fold[i].name);
		wprintf("<br />\n");
	}
 */

	if (!strcasecmp(viewpref, "folders")) {
		do_folder_view(fold, max_folders, num_floors);
	}
	else {
		do_rooms_view(fold, max_folders, num_floors);
	}

	free(fold);
	wDumpContent(1);
}


/* Do either a known rooms list or a folders list, depending on the
 * user's preference
 */
void knrooms() {
	char listviewpref[SIZ];

	output_headers(1, 1, 2, 0, 0, 0, 0);
	load_floorlist();

	/* Determine whether the user is trying to change views */
	if (bstr("view") != NULL) {
		if (strlen(bstr("view")) > 0) {
			set_preference("roomlistview", bstr("view"));
		}
	}

	get_preference("roomlistview", listviewpref);

	if ( (strcasecmp(listviewpref, "folders"))
	   && (strcasecmp(listviewpref, "table")) ) {
		strcpy(listviewpref, "rooms");
	}

	/* title bar */
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">"
	);
	if (!strcasecmp(listviewpref, "rooms")) {
		wprintf("Room list");
	}
	if (!strcasecmp(listviewpref, "folders")) {
		wprintf("Folder list");
	}
	if (!strcasecmp(listviewpref, "table")) {
		wprintf("Room list");
	}
	wprintf("</SPAN></TD>\n");

	/* offer the ability to switch views */
	wprintf("<TD ALIGN=RIGHT><FORM NAME=\"roomlistomatic\">\n"
		"<SELECT NAME=\"newview\" SIZE=\"1\" "
		"OnChange=\"location.href=roomlistomatic.newview.options"
		"[selectedIndex].value\">\n");

	wprintf("<OPTION %s VALUE=\"/knrooms&view=rooms\">"
		"View as room list"
		"</OPTION>\n",
		( !strcasecmp(listviewpref, "rooms") ? "SELECTED" : "" )
	);

	wprintf("<OPTION %s VALUE=\"/knrooms&view=folders\">"
		"View as folder list"
		"</OPTION>\n",
		( !strcasecmp(listviewpref, "folders") ? "SELECTED" : "" )
	);

	wprintf("</SELECT><br />");
	offer_start_page();
	wprintf("</FORM></TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	/* Display the room list in the user's preferred format */
	list_all_rooms_by_floor(listviewpref);
}



/* 
 * Set the message expire policy for this room and/or floor
 */
void set_room_policy(void) {
	char buf[SIZ];

	if (strcmp(bstr("sc"), "OK")) {
		strcpy(WC->ImportantMessage,
			"Cancelled.  Changes were not saved.");
		display_editroom();
		return;
	}

	serv_printf("SPEX room|%d|%d", atoi(bstr("roompolicy")), atoi(bstr("roomvalue")));
	serv_gets(buf);
	strcpy(WC->ImportantMessage, &buf[4]);

	if (WC->axlevel >= 6) {
		strcat(WC->ImportantMessage, "<br />\n");
		serv_printf("SPEX floor|%d|%d", atoi(bstr("floorpolicy")), atoi(bstr("floorvalue")));
		serv_gets(buf);
		strcat(WC->ImportantMessage, &buf[4]);
	}

	display_editroom();
}
