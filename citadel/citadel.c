/*
 * $Id$
 *
 * Main source module for the client program.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <pwd.h>
#include <setjmp.h>
#include <stdarg.h>

#include "citadel.h"
#include "axdefs.h"
#include "routines.h"
#include "routines2.h"
#include "rooms.h"
#include "messages.h"
#include "commands.h"
#include "ipc.h"
#include "citadel_ipc.h"
#include "client_chat.h"
#include "client_passwords.h"
#include "citadel_decls.h"
#include "tools.h"
#include "acconfig.h"
#include "client_crypto.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"

#include "md5.h"

#define IFEXPERT if (userflags&US_EXPERT)
#define IFNEXPERT if ((userflags&US_EXPERT)==0)
#define IFAIDE if (axlevel>=6)
#define IFNAIDE if (axlevel<6)

struct march *march = NULL;

/* globals associated with the client program */
char temp[PATH_MAX];		/* Name of general temp file */
char temp2[PATH_MAX];		/* Name of general temp file */
char tempdir[PATH_MAX];		/* Name of general temp dir */
char editor_path[SIZ];		/* path to external editor */
char printcmd[SIZ];		/* print command */
int editor_pid = (-1);
char fullname[32];
jmp_buf nextbuf;
struct CtdlServInfo serv_info;	/* Info on the server connected */
int screenwidth;
int screenheight;
unsigned room_flags;
char room_name[ROOMNAMELEN];
char *uglist[UGLISTLEN]; /* size of the ungoto list */
long uglistlsn[UGLISTLEN]; /* current read position for all the ungoto's. Not going to make any friends with this one. */
int uglistsize = 0;
char is_mail = 0;		/* nonzero when we're in a mail room */
char axlevel = 0;		/* access level */
char is_room_aide = 0;		/* boolean flag, 1 if room aide */
int timescalled;
int posted;
unsigned userflags;
long usernum = 0L;		/* user number */
time_t lastcall = 0L;		/* Date/time of previous login */
char newnow;
long highest_msg_read;		/* used for <A>bandon room cmd */
long maxmsgnum;			/* used for <G>oto */
char sigcaught = 0;
char have_xterm = 0;		/* are we running on an xterm? */
char rc_username[32];
char rc_password[32];
char hostbuf[SIZ];
char portbuf[SIZ];
char rc_floor_mode;
char floor_mode;
char curr_floor = 0;		/* number of current floor */
char floorlist[128][SIZ];	/* names of floors */
int termn8 = 0;			/* Set to nonzero to cause a logoff */
int secure;			/* Set to nonzero when wire is encrypted */

extern char express_msgs;	/* express messages waiting! */
extern int rc_ansi_color;	/* ansi color value from citadel.rc */
extern int next_lazy_cmd;

/*
 * here is our 'clean up gracefully and exit' routine
 */
void logoff(int code)
{
    int lp;
	if (editor_pid > 0) {	/* kill the editor if it's running */
		kill(editor_pid, SIGHUP);
	}

    /* Free the ungoto list */
    for (lp = 0; lp < uglistsize; lp++)
      free (uglist[lp]);

/* shut down the server... but not if the logoff code is 3, because
 * that means we're exiting because we already lost the server
 */
	if (code != 3)
		CtdlIPCQuit();

/*
 * now clean up various things
 */

	screen_delete();

	unlink(temp);
	unlink(temp2);
	nukedir(tempdir);

	/* Violently kill off any child processes if Citadel is
	 * the login shell. 
	 */
	if (getppid() == 1) {
		kill(0 - getpgrp(), SIGTERM);
		sleep(1);
		kill(0 - getpgrp(), SIGKILL);
	}
	sttybbs(SB_RESTORE);	/* return the old terminal settings */
	exit(code);		/* exit with the proper exit code */
}



/*
 * signal catching function for hangups...
 */
void dropcarr(int signum)
{
	logoff(SIGHUP);
}



/*
 * catch SIGCONT to reset terminal modes when were are put back into the
 * foreground.
 */
void catch_sigcont(int signum)
{
	sttybbs(SB_LAST);
	signal(SIGCONT, catch_sigcont);
}



/* general purpose routines */
/* display a file */
void formout(char *name)
{
	int r;			/* IPC return code */
	char buf[SIZ];
	char *text = NULL;

	r = CtdlIPCSystemMessage(name, &text, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	if (text) {
		fmout(screenwidth, NULL, text, NULL,
		      ((userflags & US_PAGINATOR) ? 1 : 0),
		      screenheight, 1, 1);
		free(text);
	}
}


void userlist(char *patn)
{
	char buf[SIZ];
	char fl[SIZ];
	struct tm *tmbuf;
	time_t lc;
	int r;				/* IPC response code */
	char *listing = NULL;

	r = CtdlIPCUserListing(&listing, buf);
	if (r / 100 != 1) {
		pprintf("%s\n", buf);
		return;
	}

	pprintf("       User Name           Num  L  LastCall  Calls Posts\n");
	pprintf("------------------------- ----- - ---------- ----- -----\n");
	while (strlen(listing) > 0) {
		extract_token(buf, listing, 0, '\n');
		remove_token(listing, 0, '\n');

		if (sigcaught == 0) {
		    extract(fl, buf, 0);
		    if (pattern(fl, patn) >= 0) {
			pprintf("%-25s ", fl);
			pprintf("%5ld %d ", extract_long(buf, 2),
			       extract_int(buf, 1));
			lc = extract_long(buf, 3);
			tmbuf = (struct tm *) localtime(&lc);
			pprintf("%02d/%02d/%04d ",
			       (tmbuf->tm_mon + 1),
			       tmbuf->tm_mday,
			       (tmbuf->tm_year + 1900));
			pprintf("%5ld %5ld\n", extract_long(buf, 4), extract_long(buf, 5));
		    }

		}
	}
	free(listing);
	pprintf("\n");
}


/*
 * grab assorted info about the user...
 */
void load_user_info(char *params)
{
	extract(fullname, params, 0);
	axlevel = extract_int(params, 1);
	timescalled = extract_int(params, 2);
	posted = extract_int(params, 3);
	userflags = extract_int(params, 4);
	usernum = extract_long(params, 5);
	lastcall = extract_long(params, 6);
}


/*
 * Remove a room from the march list.  'floornum' is ignored unless
 * 'roomname' is set to _FLOOR_, in which case all rooms on the requested
 * floor will be removed from the march list.
 */
void remove_march(char *roomname, int floornum)
{
	struct march *mptr, *mptr2;

	if (march == NULL)
		return;

	if ((!strcasecmp(march->march_name, roomname))
	    || ((!strcasecmp(roomname, "_FLOOR_")) && (march->march_floor == floornum))) {
		mptr = march->next;
		free(march);
		march = mptr;
		return;
	}
	mptr2 = march;
	for (mptr = march; mptr != NULL; mptr = mptr->next) {

		if ((!strcasecmp(mptr->march_name, roomname))
		    || ((!strcasecmp(roomname, "_FLOOR_"))
			&& (mptr->march_floor == floornum))) {

			mptr2->next = mptr->next;
			free(mptr);
			mptr = mptr2;
		} else {
			mptr2 = mptr;
		}
	}
}


/*
 * Locate the room on the march list which we most want to go to.  Each room
 * is measured given a "weight" of preference based on various factors.
 */
char *pop_march(int desired_floor)
{
	static char TheRoom[ROOMNAMELEN];
	int TheFloor = 0;
	int TheOrder = 32767;
	int TheWeight = 0;
	int weight;
	struct march *mptr = NULL;

	strcpy(TheRoom, "_BASEROOM_");
	if (march == NULL)
		return (TheRoom);

	for (mptr = march; mptr != NULL; mptr = mptr->next) {
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
 * jump directly to a room
 */
void dotgoto(char *towhere, int display_name, int fromungoto)
{
	char aaa[SIZ], bbb[SIZ], psearch[SIZ];
	static long ls = 0L;
	int newmailcount = 0;
	int partial_match, best_match;
	char from_floor;
	int ugpos = uglistsize;
	int r;				/* IPC result code */
	struct ctdlipcroom *roomrec = NULL;

	/* store ungoto information */
	if (fromungoto == 0) {
		/* sloppy slide them all down, hey it's the client, who cares. :-) */
        	if (uglistsize >= (UGLISTLEN-1)) {
			int lp;
			free (uglist[0]);
			for (lp = 0; lp < (UGLISTLEN-1); lp++) {
				uglist[lp] = uglist[lp+1];
				uglistlsn[lp] = uglistlsn[lp+1];
			}
			ugpos--;
		} else
			uglistsize++;
        
		uglist[ugpos] = malloc(strlen(room_name)+1);
		strcpy(uglist[ugpos], room_name);
		uglistlsn[ugpos] = ls;
	}
      
	/* first try an exact match */
	r = CtdlIPCGotoRoom(towhere, "", &roomrec, aaa);
	if (r / 10 == 54) {
		newprompt("Enter room password: ", bbb, 9);
		r = CtdlIPCGotoRoom(towhere, bbb, &roomrec, aaa);
		if (r / 10 == 54) {
			scr_printf("Wrong password.\n");
			return;
		}
	}	

	/*
	 * If a match is not found, try a partial match.
	 * Partial matches anywhere in the string carry a weight of 1,
	 * left-aligned matches carry a weight of 2.  Pick the room that
	 * has the highest-weighted match.
	 */
	if (r / 100 != 2) {
		best_match = 0;
		strcpy(bbb, "");
		serv_puts("LKRA");
		serv_gets(aaa);
		if (aaa[0] == '1')
			while (serv_gets(aaa), strcmp(aaa, "000")) {
				extract(psearch, aaa, 0);
				partial_match = 0;
				if (pattern(psearch, towhere) >= 0) {
					partial_match = 1;
				}
				if (!strncasecmp(towhere, psearch, strlen(towhere))) {
					partial_match = 2;
				}
				if (partial_match > best_match) {
					strcpy(bbb, psearch);
					best_match = partial_match;
				}
			}
		if (strlen(bbb) == 0) {
			scr_printf("No room '%s'.\n", towhere);
			return;
		}
		roomrec = NULL;
		r = CtdlIPCGotoRoom(bbb, "", &roomrec, aaa);
	}
	if (r / 100 != 2) {
		scr_printf("%s\n", aaa);
		return;
	}
	safestrncpy(room_name, roomrec->RRname, ROOMNAMELEN);
	room_flags = roomrec->RRflags;
	from_floor = curr_floor;
	curr_floor = roomrec->RRfloor;

	remove_march(room_name, 0);
	if (!strcasecmp(towhere, "_BASEROOM_"))
		remove_march(towhere, 0);
	if (!roomrec->RRunread)
		next_lazy_cmd = 5;	/* Don't read new if no new msgs */
	if ((from_floor != curr_floor) && (display_name > 0) && (floor_mode == 1)) {
		if (floorlist[(int) curr_floor][0] == 0)
			load_floorlist();
		scr_printf("(Entering floor: %s)\n", &floorlist[(int) curr_floor][0]);
	}
	if (display_name == 1) {
		color(BRIGHT_WHITE);
		scr_printf("%s ", room_name);
		color(DIM_WHITE);
		scr_printf("- ");
	}
	if (display_name != 2) {
		color(BRIGHT_YELLOW);
		scr_printf("%d ", roomrec->RRunread);
		color(DIM_WHITE);
		scr_printf("new of ");
		color(BRIGHT_YELLOW);
		scr_printf("%d ", roomrec->RRtotal);
		color(DIM_WHITE);
		scr_printf("messages.\n");
	}
	highest_msg_read = roomrec->RRlastread;
	maxmsgnum = roomrec->RRhighest;
	is_mail = roomrec->RRismailbox;
	is_room_aide = roomrec->RRaide;
	ls = roomrec->RRlastread;

	/* read info file if necessary */
	if (roomrec->RRinfoupdated > 0)
		readinfo();

	/* check for newly arrived mail if we can */
	newmailcount = roomrec->RRnewmail;
	if (newmailcount > 0) {
		color(BRIGHT_RED);
		if (newmailcount == 1) {
			scr_printf("*** A new mail message has arrived.\n");
		}
		else {
			scr_printf("*** %d new mail messages have arrived.\n",
					newmailcount);
		}
		color(DIM_WHITE);
		if (strlen(rc_gotmail_cmd) > 0) {
			system(rc_gotmail_cmd);
		}
	}
	status_line(serv_info.serv_humannode, serv_info.serv_bbs_city,
			room_name, secure, newmailcount);
}

/* Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 */
void gotonext(void)
{
	char buf[SIZ];
	struct march *mptr, *mptr2;
	char next_room[ROOMNAMELEN];
	int r;				/* IPC response code */

	/* Check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */
	if (march == NULL) {
		r = CtdlIPCKnownRooms(1, -1, &march, buf);
/* add _BASEROOM_ to the end of the march list, so the user will end up
 * in the system base room (usually the Lobby>) at the end of the loop
 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		strcpy(mptr->march_name, "_BASEROOM_");
		if (march == NULL) {
			march = mptr;
		} else {
			mptr2 = march;
			while (mptr2->next != NULL)
				mptr2 = mptr2->next;
			mptr2->next = mptr;
		}
/*
 * ...and remove the room we're currently in, so a <G>oto doesn't make us
 * walk around in circles
 */
		remove_march(room_name, 0);
	}
	if (march != NULL) {
		strcpy(next_room, pop_march(curr_floor));
	} else {
		strcpy(next_room, "_BASEROOM_");
	}
	remove_march(next_room, 0);
	dotgoto(next_room, 1, 0);
}

/*
 * forget all rooms on a given floor
 */
void forget_all_rooms_on(int ffloor)
{
	char buf[SIZ];
	struct march *flist, *fptr;
	struct ctdlipcroom *roomrec;	/* Ignored */
	int r;				/* IPC response code */

	scr_printf("Forgetting all rooms on %s...\r", &floorlist[ffloor][0]);
	scr_flush();
	snprintf(buf, sizeof buf, "LKRA %d", ffloor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') {
		scr_printf("%-72s\n", &buf[4]);
		return;
	}
	flist = NULL;
	while (serv_gets(buf), strcmp(buf, "000")) {
		fptr = (struct march *) malloc(sizeof(struct march));
		fptr->next = flist;
		flist = fptr;
		extract(fptr->march_name, buf, 0);
	}
	while (flist != NULL) {
		r = CtdlIPCGotoRoom(flist->march_name, "", &roomrec, buf);
		if (r / 100 == 2) {
			r = CtdlIPCForgetRoom(buf);
		}
		fptr = flist;
		flist = flist->next;
		free(fptr);
	}
	scr_printf("%-72s\r", "");
}


/*
 * routine called by gotofloor() to move to a new room on a new floor
 */
void gf_toroom(char *towhere, int mode)
{
	int floor_being_left;

	floor_being_left = curr_floor;

	if (mode == GF_GOTO) {	/* <;G>oto mode */
		updatels();
		dotgoto(towhere, 1, 0);
	}
	if (mode == GF_SKIP) {	/* <;S>kip mode */
		dotgoto(towhere, 1, 0);
		remove_march("_FLOOR_", floor_being_left);
	}
	if (mode == GF_ZAP) {	/* <;Z>ap mode */
		dotgoto(towhere, 1, 0);
		remove_march("_FLOOR_", floor_being_left);
		forget_all_rooms_on(floor_being_left);
	}
}


/*
 * go to a new floor
 */
void gotofloor(char *towhere, int mode)
{
	int a, tofloor;
	struct march *mptr;
	char buf[SIZ], targ[SIZ];

	if (floorlist[0][0] == 0)
		load_floorlist();
	tofloor = (-1);
	for (a = 0; a < 128; ++a)
		if (!strcasecmp(&floorlist[a][0], towhere))
			tofloor = a;

	if (tofloor < 0) {
		for (a = 0; a < 128; ++a) {
			if (!strncasecmp(&floorlist[a][0], towhere, strlen(towhere))) {
				tofloor = a;
			}
		}
	}
	if (tofloor < 0) {
		for (a = 0; a < 128; ++a)
			if (pattern(towhere, &floorlist[a][0]) > 0)
				tofloor = a;
	}
	if (tofloor < 0) {
		scr_printf("No floor '%s'.\n", towhere);
		return;
	}
	for (mptr = march; mptr != NULL; mptr = mptr->next) {
		if ((mptr->march_floor) == tofloor)
			gf_toroom(mptr->march_name, mode);
		return;
	}

	strcpy(targ, "");
	snprintf(buf, sizeof buf, "LKRA %d", tofloor);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1')
		while (serv_gets(buf), strcmp(buf, "000")) {
			if ((extract_int(buf, 2) == tofloor) && (strlen(targ) == 0))
				extract(targ, buf, 0);
		}
	if (strlen(targ) > 0) {
		gf_toroom(targ, mode);
	} else {
		scr_printf("There are no rooms on '%s'.\n", &floorlist[tofloor][0]);
	}
}


/*
 * forget all rooms on current floor
 */
void forget_this_floor(void)
{

	if (curr_floor == 0) {
		scr_printf("Can't forget this floor.\n");
		return;
	}
	if (floorlist[0][0] == 0)
		load_floorlist();
	scr_printf("Are you sure you want to forget all rooms on %s? ",
	       &floorlist[(int) curr_floor][0]);
	if (yesno() == 0)
		return;

	gf_toroom("_BASEROOM_", GF_ZAP);
}


/* 
 * Figure out the physical screen dimensions, if we can
 * WARNING:  this is now called from a signal handler!
 */
void check_screen_dims(void)
{
#ifdef TIOCGWINSZ
	struct {
		unsigned short height;	/* rows */
		unsigned short width;	/* columns */
		unsigned short xpixels;
		unsigned short ypixels;		/* pixels */
	} xwinsz;

	if (have_xterm) {	/* dynamically size screen if on an xterm */
		if (ioctl(0, TIOCGWINSZ, &xwinsz) == 0) {
			if (xwinsz.height)
				screenheight = is_curses_enabled() ? (int)xwinsz.height - 1 : (int) xwinsz.height;
			if (xwinsz.width)
				screenwidth = (int) xwinsz.width;
		}
	}
#endif
}


/*
 * set floor mode depending on client, server, and user settings
 */
void set_floor_mode(void)
{
	if (serv_info.serv_ok_floors == 0) {
		floor_mode = 0;	/* Don't use floors if the server */
	}
	/* doesn't support them!          */
	else {
		if (rc_floor_mode == RC_NO) {	/* never use floors */
			floor_mode = 0;
		}
		if (rc_floor_mode == RC_YES) {	/* always use floors */
			floor_mode = 1;
		}
		if (rc_floor_mode == RC_DEFAULT) {	/* user choice */
			floor_mode = ((userflags & US_FLOORS) ? 1 : 0);
		}
	}
}

/*
 * Set or change the user's password
 */
int set_password(void)
{
	char pass1[20];
	char pass2[20];
	char buf[SIZ];

	if (strlen(rc_password) > 0) {
		strcpy(pass1, rc_password);
		strcpy(pass2, rc_password);
	} else {
		IFNEXPERT formout("changepw");
		newprompt("Enter a new password: ", pass1, -19);
		newprompt("Enter it again to confirm: ", pass2, -19);
	}
	strproc(pass1);
	strproc(pass2);
	if (!strcasecmp(pass1, pass2)) {
		CtdlIPCChangePassword(pass1, buf);
		scr_printf("%s\n", buf);
		offer_to_remember_password(hostbuf, portbuf, fullname, pass1);
		return (0);
	} else {
		scr_printf("*** They don't match... try again.\n");
		return (1);
	}
}



/*
 * get info about the server we've connected to
 */
void get_serv_info(char *supplied_hostname)
{
	char buf[SIZ];

	CtdlIPCServerInfo(&serv_info, buf);

	/* be nice and identify ourself to the server */
	CtdlIPCIdentifySoftware(SERVER_TYPE, 0, REV_LEVEL,
		 (server_is_local ? "local" : CITADEL),
		 (supplied_hostname) ? supplied_hostname : "", buf);
		 /* (locate_host(buf), buf)); */
}





/*
 * Display list of users currently logged on to the server
 */
void who_is_online(int longlist)
{
	char buf[SIZ], username[SIZ], roomname[SIZ], fromhost[SIZ];
	char flags[SIZ];
	char actual_user[SIZ], actual_room[SIZ], actual_host[SIZ];
	char tbuf[SIZ], clientsoft[SIZ];
	time_t timenow = 0;
	time_t idletime, idlehours, idlemins, idlesecs;
	int last_session = (-1);
	int skipidle = 0;
	char *listing = NULL;
	int r;				/* IPC response code */
    
	if (longlist == 2) {
		longlist = 0;
		skipidle = 1;
	}

	if (!longlist) {
		color(BRIGHT_WHITE);
		pprintf("FLG ###        User Name                 Room                 From host\n");
		color(DIM_WHITE);
		pprintf("--- --- ------------------------- -------------------- ------------------------\n");
	}
	r = CtdlIPCOnlineUsers(&listing, &timenow, buf);
	if (r / 100 == 1) {
		while (strlen(listing) > 0) {
			int isidle = 0;
			
			/* Get another line */
			extract_token(buf, listing, 0, '\n');
			remove_token(listing, 0, '\n');

			extract(username, buf, 1);
			extract(roomname, buf, 2);
			extract(fromhost, buf, 3);
			extract(clientsoft, buf, 4);
			extract(flags, buf, 7);

			idletime = timenow - extract_long(buf, 5);
			idlehours = idletime / 3600;
			idlemins = (idletime - (idlehours * 3600)) / 60;
			idlesecs = (idletime - (idlehours * 3600) - (idlemins * 60));

			if (idletime > rc_idle_threshold) {
				while (strlen(roomname) < 20) {
					strcat(roomname, " ");
				}
				strcpy(&roomname[14], "[idle]");
				if (skipidle)
					isidle = 1;
			}

			if (longlist) {
				extract(actual_user, buf, 8);
				extract(actual_room, buf, 9);
				extract(actual_host, buf, 10);

				pprintf("  Flags: %s\n", flags);
				pprintf("Session: %d\n", extract_int(buf, 0));
				pprintf("   Name: %s\n", username);
				pprintf("In room: %s\n", roomname);
				pprintf("   Host: %s\n", fromhost);
				pprintf(" Client: %s\n", clientsoft);
				pprintf("   Idle: %ld:%02ld:%02ld\n",
					(long) idlehours,
					(long) idlemins,
					(long) idlesecs);

				if ( (strlen(actual_user)+strlen(actual_room)+strlen(actual_host)) > 0) {
					pprintf("(really ");
					if (strlen(actual_user)>0) pprintf("<%s> ", actual_user);
					if (strlen(actual_room)>0) pprintf("in <%s> ", actual_room);
					if (strlen(actual_host)>0) pprintf("from <%s> ", actual_host);
					pprintf(")\n");
				}
				pprintf("\n");

			} else {
	            if (isidle == 0) {
    				if (extract_int(buf, 0) == last_session) {
    					pprintf("        ");
    				} else {
    					color(BRIGHT_MAGENTA);
    					pprintf("%-3s ", flags);
    					color(DIM_WHITE);
    					pprintf("%-3d ", extract_int(buf, 0));
    				}
    				last_session = extract_int(buf, 0);
    				color(BRIGHT_CYAN);
    				pprintf("%-25s ", username);
    				color(BRIGHT_MAGENTA);
    				roomname[20] = 0;
    				pprintf("%-20s ", roomname);
    				color(BRIGHT_CYAN);
    				fromhost[24] = '\0';
    				pprintf("%-24s\n", fromhost);
    				color(DIM_WHITE);
    	  		}
			}
		}
	}
	free(listing);
}

void enternew(char *desc, char *buf, int maxlen)
{
	char bbb[128];
	snprintf(bbb, sizeof bbb, "Enter in your new %s: ", desc);
	newprompt(bbb, buf, maxlen);
}



int shift(int argc, char **argv, int start, int count) {
	int i;

	for (i=start; i<(argc-count); ++i) {
		argv[i] = argv[i+count];
	}
	argc = argc - count;
	return argc;
}

static void statusHook(char *s) {
	sln_printf(s);
	sln_flush();
}

/*
 * main
 */
int main(int argc, char **argv)
{
	int a, b, mcmd;
	char aaa[100], bbb[100];/* general purpose variables */
	char argbuf[32];	/* command line buf */
	char nonce[NONCE_SIZE];
	char *telnet_client_host = NULL;
	char *sptr, *sptr2;	/* USed to extract the nonce */
	char hexstring[MD5_HEXSTRING_SIZE];
	int stored_password = 0;
	char password[SIZ];
	struct ctdlipcmisc chek;
	struct usersupp *myself = NULL;
	int r;				/* IPC result code */

	setIPCDeathHook(screen_delete);
	setIPCErrorPrintf(err_printf);
	setCryptoStatusHook(statusHook);
	
	/* Permissions sanity check - don't run citadel setuid/setgid */
	if (getuid() != geteuid()) {
		err_printf("Please do not run citadel setuid!\n");
		logoff(3);
	} else if (getgid() != getegid()) {
		err_printf("Please do not run citadel setgid!\n");
		logoff(3);
	}

	sttybbs(SB_SAVE);	/* Store the old terminal parameters */
	load_command_set();	/* parse the citadel.rc file */
	sttybbs(SB_NO_INTR);	/* Install the new ones */
	signal(SIGHUP, dropcarr);	/* Cleanup gracefully if carrier is dropped */
	signal(SIGTERM, dropcarr);	/* Cleanup gracefully if terminated */
	signal(SIGCONT, catch_sigcont);	/* Catch SIGCONT so we can reset terminal */
#ifdef SIGWINCH
	signal(SIGWINCH, scr_winch);	/* Window resize signal */
#endif

#ifdef HAVE_OPENSSL
	arg_encrypt = RC_DEFAULT;
#endif
#ifdef HAVE_CURSES_H
	arg_screen = RC_DEFAULT;
#endif

	/* 
	 * Handle command line options as if we were called like /bin/login
	 * (i.e. from in.telnetd)
	 */
	for (a=0; a<argc; ++a) {
		if ((argc > a+1) && (!strcmp(argv[a], "-h")) ) {
			telnet_client_host = argv[a+1];
			argc = shift(argc, argv, a, 2);
		}
		if (!strcmp(argv[a], "-x")) {
#ifdef HAVE_OPENSSL
			arg_encrypt = RC_NO;
#endif
			argc = shift(argc, argv, a, 1);
		}
		if (!strcmp(argv[a], "-X")) {
#ifdef HAVE_OPENSSL
			arg_encrypt = RC_YES;
                        argc = shift(argc, argv, a, 1);
#else
			fprintf(stderr, "Not compiled with encryption support");
			return 1;
#endif
		}
		if (!strcmp(argv[a], "-s")) {
#ifdef HAVE_CURSES_H
			arg_screen = RC_NO;
#endif
			argc = shift(argc, argv, a, 1);
		}
		if (!strcmp(argv[a], "-S")) {
#ifdef HAVE_CURSES_H
			arg_screen = RC_YES;
#endif
			argc = shift(argc, argv, a, 1);
		}
		if (!strcmp(argv[a], "-p")) {
			struct stat st;
		
			if (chdir(BBSDIR) < 0) {
				perror("can't change to " BBSDIR);
				logoff(3);
			}

			/*
			 * Drop privileges if necessary. We stat
			 * citadel.config to get the uid/gid since it's
			 * guaranteed to have the uid/gid we want.
			 */
			if (!getuid() || !getgid()) {
				if (stat(BBSDIR "/citadel.config", &st) < 0) {
					perror("couldn't stat citadel.config");
					logoff(3);
				}
				if (!getgid() && (setgid(st.st_gid) < 0)) {
					perror("couldn't change gid");
					logoff(3);
				}
				if (!getuid() && (setuid(st.st_uid) < 0)) {
					perror("couldn't change uid");
					logoff(3);
				}
				/*
				scr_printf("Privileges changed to uid %d gid %d\n",
						getuid(), getgid());
				*/
			}
			argc = shift(argc, argv, a, 1);
		}
	}

	screen_new();

	sln_printf("Attaching to server... \r");
	sln_flush();
	attach_to_server(argc, argv, hostbuf, portbuf);

	serv_gets(aaa);
	if (aaa[0] != '2') {
		scr_printf("%s\n", &aaa[4]);
		logoff(atoi(aaa));
	}

/* If there is a [nonce] at the end, put the nonce in <nonce>, else nonce
 * is zeroized.
 */
	
	if ((sptr = strchr(aaa, '<')) == NULL)
	{
	   nonce[0] = '\0';
	}
	else
	{
	   if ((sptr2 = strchr(sptr, '>')) == NULL)
	   {
	      nonce[0] = '\0';
	   }
	   else
	   {
	      sptr2++;
	      *sptr2 = '\0';
	      strncpy(nonce, sptr, NONCE_SIZE);
	   }
	}

	get_serv_info(telnet_client_host);

	scr_printf("%-24s\n%s\n%s\n", serv_info.serv_software, serv_info.serv_humannode,
		serv_info.serv_bbs_city);
	scr_flush();

	secure = starttls();
	status_line(serv_info.serv_humannode, serv_info.serv_bbs_city, NULL,
			secure, -1);

	screenwidth = 80;	/* default screen dimensions */
	screenheight = 24;
	
	scr_printf(" pause    next    stop\n");
	scr_printf(" ctrl-s  ctrl-o  ctrl-c\n\n");
	formout("hello");	/* print the opening greeting */
	scr_printf("\n");

GSTA:	/* See if we have a username and password on disk */
	if (rc_remember_passwords) {
		get_stored_password(hostbuf, portbuf, fullname, password);
		if (strlen(fullname) > 0) {
			snprintf(aaa, sizeof(aaa)-1, "USER %s", fullname);
			serv_puts(aaa);
			serv_gets(aaa);
			if (nonce[0])
			{
				snprintf(aaa, sizeof aaa, "PAS2 %s", make_apop_string(password, nonce, hexstring, sizeof hexstring));
			}
			else	/* Else no APOP */
			{
	   			snprintf(aaa, sizeof(aaa)-1, "PASS %s", password);
	   		}
	   		
			serv_puts(aaa);
			serv_gets(aaa);
			if (aaa[0] == '2') {
				load_user_info(&aaa[4]);
				stored_password = 1;
				goto PWOK;
			}
			else {
				set_stored_password(hostbuf, portbuf, "", "");
			}
		}
	}

	termn8 = 0;
	newnow = 0;
	do {
		if (strlen(rc_username) > 0) {
			strcpy(fullname, rc_username);
		} else {
			newprompt("Enter your name: ", fullname, 29);
		}
		strproc(fullname);
		if (!strcasecmp(fullname, "new")) {	/* just in case */
			scr_printf("Please enter the name you wish to log in with.\n");
		}
	} while (
			(!strcasecmp(fullname, "bbs"))
			|| (!strcasecmp(fullname, "new"))
			|| (strlen(fullname) == 0));

	if (!strcasecmp(fullname, "off")) {
		mcmd = 29;
		goto TERMN8;
	}
	/* sign on to the server */
	r = CtdlIPCTryLogin(fullname, aaa);
	if (r / 100 != 3)
		goto NEWUSR;

	/* password authentication */
	if (strlen(rc_password) > 0) {
		strcpy(password, rc_password);
	} else {
		newprompt("\rPlease enter your password: ", password, -19);
	}
	strproc(password);

	if (nonce[0])
	{
		snprintf(aaa, sizeof aaa, "PAS2 %s", make_apop_string(password, nonce, hexstring, sizeof hexstring));
	}
	else	/* Else no APOP */
	{
		snprintf(aaa, sizeof aaa, "PASS %s", password);
	}
	
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0] == '2') {
		load_user_info(&aaa[4]);
		offer_to_remember_password(hostbuf, portbuf,
					fullname, password);
		goto PWOK;
	}
	scr_printf("<< wrong password >>\n");
	if (strlen(rc_password) > 0)
		logoff(0);
	goto GSTA;

NEWUSR:	if (strlen(rc_password) == 0) {
		scr_printf("No record. Enter as new user? ");
		if (yesno() == 0)
			goto GSTA;
	}
	snprintf(aaa, sizeof aaa, "NEWU %s", fullname);
	serv_puts(aaa);
	serv_gets(aaa);
	if (aaa[0] != '2') {
		scr_printf("%s\n", aaa);
		goto GSTA;
	}
	load_user_info(&aaa[4]);

	while (set_password() != 0);;
	newnow = 1;

	enter_config(1);

PWOK:
	/* Switch color support on or off if we're in user mode */
	if (rc_ansi_color == 3) {
		if (userflags & US_COLOR)
			enable_color = 1;
		else
			enable_color = 0;
	}

	scr_printf("%s\nAccess level: %d (%s)\n"
		"User #%ld / Login #%d",
		fullname, axlevel, axdefs[(int) axlevel],
		usernum, timescalled);
	if (lastcall > 0L) {
		scr_printf(" / Last login: %s\n",
			asctime(localtime(&lastcall)) );
	}
	scr_printf("\n");

	r = CtdlIPCMiscCheck(&chek, aaa);
	if (r / 100 == 2) {
		b = chek.newmail;
		if (b > 0) {
			color(BRIGHT_RED);
			if (b == 1)
				scr_printf("*** You have a new private message in Mail>\n");
			if (b > 1)
				scr_printf("*** You have %d new private messages in Mail>\n", b);
			color(DIM_WHITE);
			if (strlen(rc_gotmail_cmd) > 0) {
				system(rc_gotmail_cmd);
			}
		}
		if ((axlevel >= 6) && (chek.needvalid > 0)) {
			scr_printf("*** Users need validation\n");
		}
		if (chek.needregis > 0) {
			scr_printf("*** Please register.\n");
			formout("register");
			entregis();
		}
	}
	/* Make up some temporary filenames for use in various parts of the
	 * program.  Don't mess with these once they've been set, because we
	 * will be unlinking them later on in the program and we don't
	 * want to delete something that we didn't create. */
	snprintf(temp, sizeof temp, tmpnam(NULL));
	snprintf(temp2, sizeof temp2, tmpnam(NULL));
	snprintf(tempdir, sizeof tempdir, tmpnam(NULL));

	/* Get screen dimensions.  First we go to a default of 80x24.  Then
	 * we try to get the user's actual screen dimensions off the server.
	 * However, if we're running on an xterm, all this stuff is
	 * irrelevant because we're going to dynamically size the screen
	 * during the session.
	 */
	screenwidth = 80;
	screenheight = 24;
	r = CtdlIPCGetConfig(&myself, aaa);
	if (r == 2) {
		screenwidth = myself->USscreenwidth;
		screenheight = myself->USscreenheight;
	}
	if (getenv("TERM") != NULL)
		if (!strcmp(getenv("TERM"), "xterm")) {
			have_xterm = 1;
		}
#ifdef TIOCGWINSZ
	check_screen_dims();
#endif

	set_floor_mode();


	/* Enter the lobby */
	dotgoto("_BASEROOM_", 1, 0);

/* Main loop for the system... user is logged in. */
    uglistsize = 0;

	if (newnow == 1)
		readmsgs(3, 1, 5);
	else
		readmsgs(1, 1, 0);

	/* MAIN COMMAND LOOP */
	do {
		mcmd = getcmd(argbuf);		/* Get keyboard command */

#ifdef TIOCGWINSZ
		check_screen_dims();		/* if xterm, get screen size */
#endif

		if (termn8 == 0)
			switch (mcmd) {
			case 1:
				formout("help");
				break;
			case 4:
				entmsg(0, 0);
				break;
			case 36:
				entmsg(0, 1);
				break;
			case 46:
				entmsg(0, 2);
				break;
			case 78:
				newprompt("What do you want your username to be? ", aaa, 32);
				snprintf(bbb, sizeof bbb, "ENT0 2|0|0|0|%s", aaa);
				serv_puts(bbb);
				serv_gets(aaa);
				if (strncmp("200", aaa, 3))
					scr_printf("\n%s\n", aaa);
				else
					entmsg(0, 0);
				break;
			case 5:
				updatels();
				gotonext();
				break;
			case 47:
				if (!rc_alt_semantics)
					updatelsa();
				gotonext();
				break;
			case 90:
				if (!rc_alt_semantics)
					updatelsa();
				dotgoto(argbuf, 0, 0);
				break;
			case 58:
				updatelsa();
				dotgoto("_MAIL_", 1, 0);
				break;
			case 20:
				updatels();
				dotgoto(argbuf, 0, 0);
				break;
			case 52:
				if (rc_alt_semantics)
					updatelsa();
				dotgoto(argbuf, 0, 0);
				break;
			case 10:
				readmsgs(0, 1, 0);
				break;
			case 9:
				readmsgs(3, 1, 5);
				break;
			case 13:
				readmsgs(1, 1, 0);
				break;
			case 11:
				readmsgs(0, (-1), 0);
				break;
			case 12:
				readmsgs(2, (-1), 0);
				break;
			case 71:
				readmsgs(3, 1, atoi(argbuf));
				break;
			case 7:
				forget();
				break;
			case 18:
				subshell();
				break;
			case 38:
				updatels();
				entroom();
				break;
			case 22:
				killroom();
				break;
			case 32:
				userlist(argbuf);
				break;
			case 27:
				invite();
				break;
			case 28:
				kickout();
				break;
			case 23:
				editthisroom();
				break;
			case 14:
				roomdir();
				break;
			case 33:
				download(0);
				break;
			case 34:
				download(1);
				break;
			case 31:
				download(2);
				break;
			case 43:
				download(3);
				break;
			case 45:
				download(4);
				break;
			case 55:
				download(5);
				break;
			case 39:
				upload(0);
				break;
			case 40:
				upload(1);
				break;
			case 42:
				upload(2);
				break;
			case 44:
				upload(3);
				break;
			case 57:
				cli_upload();
				break;
			case 16:
				ungoto();
				break;
			case 24:
				whoknows();
				break;
			case 26:
				validate();
				break;
			case 29:
				if (!rc_alt_semantics)
					updatels();
				termn8 = 1;
				break;
			case 30:
				if (!rc_alt_semantics)
					updatels();
				termn8 = 1;
				break;
			case 48:
				enterinfo();
				break;
			case 49:
				readinfo();
				break;
			case 72:
				cli_image_upload("_userpic_");
				break;
			case 73:
				cli_image_upload("_roompic_");
				break;

			case 74:
				snprintf(aaa, sizeof aaa, "_floorpic_|%d", curr_floor);
				cli_image_upload(aaa);
				break;

			case 75:
				enternew("roomname", aaa, 20);
				r = CtdlIPCChangeRoomname(aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", aaa);
				break;
			case 76:
				enternew("hostname", aaa, 25);
				r = CtdlIPCChangeHostname(aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", aaa);
				break;
			case 77:
				enternew("username", aaa, 32);
				r = CtdlIPCChangeUsername(aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", aaa);
				break;

			case 35:
				set_password();
				break;

			case 21:
				if (argbuf[0] == 0)
					strcpy(aaa, "?");
				display_help(argbuf);
				break;

			case 41:
				formout("register");
				entregis();
				break;

			case 15:
				scr_printf("Are you sure (y/n)? ");
				if (yesno() == 1) {
					updatels();
					a = 0;
					termn8 = 1;
				}
				break;

			case 85:
				scr_printf("All users will be disconnected!  "
					"Really terminate the server? ");
				if (yesno() == 1) {
					r = CtdlIPCTerminateServerNow(aaa);
					scr_printf("%s\n", aaa);
					if (r / 100 == 2) {
						updatels();
						a = 0;
						termn8 = 1;
					}
				}
				break;

			case 86:
				scr_printf("Do you really want to schedule a "
					"server shutdown? ");
				if (yesno() == 1) {
					r = CtdlIPCTerminateServerScheduled(1, aaa);
					if (r / 100 == 2) {
						if (atoi(aaa)) {
							scr_printf(
"The Citadel server will terminate when all users are logged off.\n"
								);
						} else {
							scr_printf(
"The Citadel server will not terminate.\n"
								);
						}
					}
				}
				break;

			case 87:
				network_config_management("listrecp",
					"Mailing list recipients");
				break;

			case 89:
				network_config_management("ignet_push_share",
					"Nodes with which we share this room");
				break;

			case 88:
				do_ignet_configuration();
				break;

			case 92:
				do_filterlist_configuration();
				break;

			case 6:
				if (rc_alt_semantics)
					updatelsa();
				gotonext();
				break;

			case 3:
				chatmode();
				break;

			case 2:
				if (server_is_local) {
					screen_reset();
					sttybbs(SB_RESTORE);
					snprintf(aaa, sizeof aaa, "USERNAME=\042%s\042; export USERNAME;"
						 "exec ./subsystem %ld %d %d", fullname,
					  usernum, screenwidth, axlevel);
					ka_system(aaa);
					sttybbs(SB_NO_INTR);
					screen_set();
				} else {
					scr_printf("*** Can't run doors when server is not local.\n");
				}
				break;

			case 17:
				who_is_online(0);
				break;

			case 79:
				who_is_online(1);
				break;

            case 91:
                who_is_online(2);
                break;
                
			case 80:
				do_system_configuration();
				break;

			case 82:
				do_internet_configuration();
				break;

			case 83:
				check_message_base();
				break;

			case 84:
				quiet_mode();
				break;

			case 93:
				stealth_mode();
				break;

			case 50:
				enter_config(2);
				break;

			case 37:
				enter_config(0);
				set_floor_mode();
				break;

			case 59:
				enter_config(3);
				set_floor_mode();
				break;

			case 60:
				gotofloor(argbuf, GF_GOTO);
				break;

			case 61:
				gotofloor(argbuf, GF_SKIP);
				break;

			case 62:
				forget_this_floor();
				break;

			case 63:
				create_floor();
				break;

			case 64:
				edit_floor();
				break;

			case 65:
				kill_floor();
				break;

			case 66:
				enter_bio();
				break;

			case 67:
				read_bio();
				break;

			case 25:
				edituser();
				break;

			case 8:
				knrooms(floor_mode);
				scr_printf("\n");
				break;

			case 68:
				knrooms(2);
				scr_printf("\n");
				break;

			case 69:
				misc_server_cmd(argbuf);
				break;

			case 70:
				edit_system_message(argbuf);
				break;

			case 19:
				listzrooms();
				scr_printf("\n");
				break;

			case 51:
				deletefile();
				break;

			case 53:
				netsendfile();
				break;

			case 54:
				movefile();
				break;

			case 56:
				page_user();
				break;

			}	/* end switch */
	} while (termn8 == 0);

TERMN8:	scr_printf("%s logged out.\n", fullname);
	while (march != NULL) {
		remove_march(march->march_name, 0);
	}
	if (mcmd == 30) {
		sln_printf("\n\nType 'off' to disconnect, or next user...\n");
	}
	CtdlIPCLogout();
	screen_delete();
	sttybbs(SB_RESTORE);
	if ((mcmd == 29) || (mcmd == 15)) {
		formout("goodbye");
		logoff(0);
	}
	goto GSTA;

}	/* end main() */
