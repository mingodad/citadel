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

	if (rp == NULL)
		return;

	if (rp->lnext != NULL) {
		room_tree_list(rp->lnext);
	}
	strcpy(rmname, rp->rlname);
	f = rp->rlflags;

	wprintf("<A HREF=\"/dotgoto&room=");
	urlescputs(rmname);
	wprintf("\"");
	wprintf(">");
	escputs1(rmname, 1);
	if ((f & QR_DIRECTORY) && (f & QR_NETWORK))
		wprintf("}");
	else if (f & QR_DIRECTORY)
		wprintf("]");
	else if (f & QR_NETWORK)
		wprintf(")");
	else
		wprintf("&gt;");
	wprintf("</A><TT> </TT>\n");

	if (rp->rnext != NULL) {
		room_tree_list(rp->rnext);
	}
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
 * list all rooms by floor
 */
void list_all_rooms_by_floor(void)
{
	int a;
	char buf[SIZ];

	load_floorlist();

	output_headers(1);

	wprintf("<TABLE width=100%% border><TR><TH>Floor</TH>");
	wprintf("<TH>Rooms with new messages</TH>");
	wprintf("<TH>Rooms with no new messages</TH></TR>\n");

	for (a = 0; a < 128; ++a)
		if (floorlist[a][0] != 0) {

			/* Floor name column */
			wprintf("<TR><TD>");

			serv_printf("OIMG _floorpic_|%d", a);
			serv_gets(buf);
			if (buf[0] == '2') {
				serv_puts("CLOS");
				serv_gets(buf);
				wprintf("<IMG SRC=\"/image&name=_floorpic_&parm=%d\" ALT=\"%s\">",
					a, &floorlist[a][0]);
			} else {
				escputs(&floorlist[a][0]);
			}

			wprintf("</TD>");

			/* Rooms with new messages column */
			wprintf("<TD>");
			sprintf(buf, "LKRN %d", a);
			listrms(buf);
			wprintf("</TD>\n<TD>");

			/* Rooms with old messages column */
			sprintf(buf, "LKRO %d", a);
			listrms(buf);
			wprintf("</TD></TR>\n");
		}
	wprintf("</TABLE>\n");
	wDumpContent(1);
}


/*
 * list all forgotten rooms
 */
void zapped_list(void)
{
	output_headers(1);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Zapped (forgotten) rooms</B>\n");
	wprintf("</TD></TR></TABLE><BR>\n");
	listrms("LZRM -1");
	wprintf("<BR><BR>\n");
	wprintf("Click on any room to un-zap it and goto that room.\n");
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
		wprintf("<FONT SIZE=-1>");
		fmout(NULL);
		wprintf("</FONT>");
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
		wprintf("<TD>");
		wprintf("<IMG SRC=\"/image&name=_roompic_&room=");
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
		wprintf("<TD VALIGN=TOP>"
			"<IMG SRC=\"/static/mail.gif\" border=0 "
			"ALT=\"You have new mail\">"
			"<BR><BLINK>%d</BLINK>", WC->new_mail);
		wprintf("<FONT SIZE=-2> new mail messages</FONT></TD>");
		WC->remember_new_mail = WC->new_mail;
	}
}



void embed_room_banner(char *got) {
	char fakegot[SIZ];

	/* We need to have the information returned by a GOTO server command.
	 * If it isn't supplied, we fake it by issuing our own GOTO.
	 */
	if (got == NULL) {
		serv_printf("GOTO %s", WC->wc_roomname);
		serv_gets(fakegot);
		got = fakegot;
	}

	/* Check for new mail. */
	WC->new_mail = extract_int(&got[4], 9);

	svprintf("ROOMNAME", WCS_STRING, "%s", WC->wc_roomname);
	svprintf("NEWMSGS", WCS_STRING, "%d", extract_int(&got[4], 1));
	svprintf("TOTALMSGS", WCS_STRING, "%d", extract_int(&got[4], 2));
	svcallback("ROOMPIC", embed_room_graphic);
	svcallback("ROOMINFO", readinfo);
	svcallback("YOUHAVEMAIL", embed_newmail_button);

	do_template("roombanner.html");
	clear_local_substs();
}





/*
 * generic routine to take the session to a new room
 *
 * display_name values:  0 = goto only
 *                       1 = goto and display
 *                       2 = display only
 */
void gotoroom(char *gname, int display_name)
{
	char buf[SIZ];
	static long ls = (-1L);


	if (display_name) {
		output_headers(0);
                wprintf("Pragma: no-cache\n");
                wprintf("Cache-Control: no-store\n");

		wprintf("<HTML><HEAD>\n"
			"<META HTTP-EQUIV=\"refresh\" CONTENT=\"500363689;\">\n"
			"<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">\n"
			"<META HTTP-EQUIV=\"expired\" CONTENT=\"28-May-1971 18:10:00 GMT\">\n"
			"<meta name=\"MSSmartTagsPreventParsing\" content=\"TRUE\">\n"
			"</HEAD>\n");
		do_template("background.html");
	}
	if (display_name != 2) {
		/* store ungoto information */
		strcpy(WC->ugname, WC->wc_roomname);
		WC->uglsn = ls;
	}
	/* move to the new room */
	serv_printf("GOTO %s", gname);
	serv_gets(buf);
	if (buf[0] != '2') {
		serv_puts("GOTO _BASEROOM_");
		serv_gets(buf);
	}
	if (buf[0] != '2') {
		if (display_name) {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
			wDumpContent(1);
		}
		return;
	}
	extract(WC->wc_roomname, &buf[4], 0);
	WC->room_flags = extract_int(&buf[4], 4);
	/* highest_msg_read = extract_int(&buf[4],6);
	   maxmsgnum = extract_int(&buf[4],5);
	   is_mail = (char) extract_int(&buf[4],7); */
	ls = extract_long(&buf[4], 6);

	if (WC->is_aide)
		WC->is_room_aide = WC->is_aide;
	else
		WC->is_room_aide = (char) extract_int(&buf[4], 8);

	remove_march(WC->wc_roomname);
	if (!strcasecmp(gname, "_BASEROOM_"))
		remove_march(gname);

	/* Display the room banner */
	if (display_name) {
		embed_room_banner(buf);
		wDumpContent(1);
	}
	strcpy(WC->wc_roomname, WC->wc_roomname);
}


/*
 * Locate the room on the march list which we most want to go to.  Each room
 * is measured given a "weight" of preference based on various factors.
 */
char *pop_march(int desired_floor)
{
	static char TheRoom[64];
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
	char next_room[32];

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
	gotoroom(next_room, 0);
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
		wprintf("<EM>%s</EM><BR>\n", &buf[4]);
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
 * display the form for editing a room
 */
void display_editroom(void)
{
	char buf[SIZ];
	char er_name[20];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;
	int er_floor;
	int i;

	serv_puts("GETR");
	serv_gets(buf);

	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
	}
	extract(er_name, &buf[4], 0);
	extract(er_password, &buf[4], 1);
	extract(er_dirname, &buf[4], 2);
	er_flags = extract_int(&buf[4], 3);
	er_floor = extract_int(&buf[4], 4);


	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Room administration</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<UL>"
		"<LI><A HREF=\"/confirm_delete_room\">\n"
		"Delete this room</A>\n"
		"<LI><A HREF=\"/display_editroompic\">\n"
		"Set or change the graphic for this room's banner</A>\n"
		"<LI><A HREF=\"/display_editinfo\">\n"
		"Edit this room's Info file</A>\n"
		"</UL>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Room editing</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/editroom\">\n");

	wprintf("<UL><LI>Name of room: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\"19\">\n", er_name);

	wprintf("<LI>Resides on floor: ");
	load_floorlist();
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
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER>\n");

	wDumpContent(1);
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
		display_error("Cancelled.  Changes were not saved.");
		return;
	}
	serv_puts("GETR");
	serv_gets(buf);

	if (buf[0] != '2') {
		display_error(&buf[4]);
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
		display_error(&buf[4]);
		return;
	}
	gotoroom(er_name, 0);

	if (strlen(er_roomaide) > 0) {
		sprintf(buf, "SETA %s", er_roomaide);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			display_error(&buf[4]);
			return;
		}
	}
	smart_goto(er_name);
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
                display_error(&buf[4]);
                return;
        }
        extract(room, &buf[4], 0);

        strcpy(username, bstr("username"));

        output_headers(1);

        if(!strcmp(bstr("sc"), "Kick")) {
                sprintf(buf, "KICK %s", username);
                serv_puts(buf);
                serv_gets(buf);

                if (buf[0] != '2') {
                        display_error(&buf[4]);
                        return;
                } else {
                        wprintf("User %s kicked out of room %s.\n", 
                                username, room);
                }
        } else if(!strcmp(bstr("sc"), "Invite")) {
                sprintf(buf, "INVT %s", username);
                serv_puts(buf);
                serv_gets(buf);

                if (buf[0] != '2') {
                        display_error(&buf[4]);
                        return;
                } else {
                        wprintf("User %s invited to room %s.\n", 
                                username, room);
                }
        }
        

        wprintf("<FORM METHOD=\"POST\" ACTION=\"/display_whok\">\n");
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
        wprintf("</SELECT>\n");

        wprintf("<CENTER>\n");
        wprintf("<input type=submit name=sc value=\"Kick\">");
        wprintf("</CENTER>\n");
        wprintf("</FORM>\n");
        wprintf("<FORM METHOD=\"POST\" ACTION=\"/display_whok\">\n");
        wprintf("Invite: ");
        wprintf("<input type=text name=username>\n");
        wprintf("<input type=hidden name=sc value=\"Invite\">");
        wprintf("<input type=submit value=\"Invite\">");
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
		display_error(&buf[4]);
		return;
	}
	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Enter (create) a new room</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/entroom\">\n");

	wprintf("<UL><LI>Name of room: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"er_name\" MAXLENGTH=\"19\">\n");

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
	wprintf("</UL>\n");

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
	wprintf("</UL>\n");


	wprintf("<CENTER>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER>\n");
	wprintf("</FORM>\n<HR>");
	serv_printf("MESG roomaccess");
	serv_gets(buf);
	if (buf[0] == '1') {
		fmout(NULL);
	}
	wDumpContent(1);
}



/*
 * enter a new room
 */
void entroom(void)
{
	char buf[SIZ];
	char er_name[20];
	char er_type[20];
	char er_password[10];
	int er_floor;
	int er_num_type;

	if (strcmp(bstr("sc"), "OK")) {
		display_error("Cancelled.  No new room was created.");
		return;
	}
	strcpy(er_name, bstr("er_name"));
	strcpy(er_type, bstr("type"));
	strcpy(er_password, bstr("er_password"));
	er_floor = atoi(bstr("er_floor"));

	er_num_type = 0;
	if (!strcmp(er_type, "guessname"))
		er_num_type = 1;
	if (!strcmp(er_type, "passworded"))
		er_num_type = 2;
	if (!strcmp(er_type, "invonly"))
		er_num_type = 3;

	sprintf(buf, "CRE8 1|%s|%d|%s|%d", 
		er_name, er_num_type, er_password, er_floor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
	}
	smart_goto(er_name);
}


/*
 * display the screen to enter a private room
 */
void display_private(char *rname, int req_pass)
{

	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Goto a private room</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n");
	wprintf("If you know the name of a hidden (guess-name) or\n");
	wprintf("passworded room, you can enter that room by typing\n");
	wprintf("its name below.  Once you gain access to a private\n");
	wprintf("room, it will appear in your regular room listings\n");
	wprintf("so you don't have to keep returning here.\n");
	wprintf("<BR><BR>");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/goto_private\">\n");

	wprintf("<TABLE border><TR><TD>");
	wprintf("Enter room name:</TD><TD>");
	wprintf("<INPUT TYPE=\"text\" NAME=\"gr_name\" VALUE=\"%s\" MAXLENGTH=\"19\">\n", rname);

	if (req_pass) {
		wprintf("</TD></TR><TR><TD>");
		wprintf("Enter room password:</TD><TD>");
		wprintf("<INPUT TYPE=\"password\" NAME=\"gr_pass\" MAXLENGTH=\"9\">\n");
	}
	wprintf("</TD></TR></TABLE>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</FORM>\n");
	wDumpContent(1);
}

/* 
 * goto a private room
 */
void goto_private(void)
{
	char hold_rm[32];
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
	output_headers(1);
	wprintf("%s\n", &buf[4]);
	wDumpContent(1);
	return;
}


/*
 * display the screen to zap a room
 */
void display_zap(void)
{
	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Zap (forget) the current room</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("If you select this option, <em>%s</em> will ", WC->wc_roomname);
	wprintf("disappear from your room list.  Is this what you wish ");
	wprintf("to do?<BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/zap\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
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
		if (buf[0] != '2') {
			/* ExpressMessageCat(&buf[4]);   FIXME    */
		} else {
			serv_puts("FORG");
			serv_gets(buf);
			if (buf[0] != '2') {
				/* ExpressMessageCat(&buf[4]);  FIXME   */
			} else {
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
		display_error(&buf[4]);
		return;
	}
	output_headers(1);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Confirm deletion of room</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/delete_room\">\n");

	wprintf("Are you sure you want to delete <FONT SIZE=+1>");
	escputs(WC->wc_roomname);
	wprintf("</FONT>?<BR>\n");

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
		display_error("Cancelled.  This room was not deleted.");
		return;
	}
	serv_puts("KILL 1");
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
	} else {
		smart_goto("_BASEROOM_");
	}
}
