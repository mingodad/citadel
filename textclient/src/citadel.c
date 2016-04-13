/*
 * Main source module for the client program.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <stdarg.h>
#include <errno.h>
#include <libcitadel.h>
///#include "citadel.h"
#include "citadel_ipc.h"
//#include "axdefs.h"
#include "routines.h"
#include "routines2.h"
#include "tuiconfig.h"
#include "rooms.h"
#include "messages.h"
#include "commands.h"
#include "client_chat.h"
#include "client_passwords.h"
#include "citadel_decls.h"
#include "sysdep.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"
///#include "citadel_dirs.h"

#include "ecrash.h"
#include "md5.h"

#define IFEXPERT if (userflags&US_EXPERT)
#define IFNEXPERT if ((userflags&US_EXPERT)==0)
#define IFAIDE if (axlevel>=AxAideU)
#define IFNAIDE if (axlevel<AxAideU)

int rordercmp(struct ctdlroomlisting *r1, struct ctdlroomlisting *r2);
march *marchptr = NULL;
extern char *moreprompt;

/* globals associated with the client program */
char temp[PATH_MAX];		/* Name of general-purpose temp file */
char temp2[PATH_MAX];		/* Name of general-purpose temp file */
char tempdir[PATH_MAX];		/* Name of general-purpose temp directory */
char printcmd[SIZ];		/* print command */
int editor_pid = (-1);
char fullname[USERNAME_SIZE];
unsigned room_flags;
unsigned room_flags2;
int entmsg_ok = 0;
char room_name[ROOMNAMELEN];
char *uglist[UGLISTLEN]; /* size of the ungoto list */
long uglistlsn[UGLISTLEN]; /* current read position for all the ungoto's. Not going to make any friends with this one. */
int uglistsize = 0;
char is_mail = 0;		/* nonzero when we're in a mail room */
char axlevel = AxDeleted;		/* access level */
char is_room_aide = 0;		/* boolean flag, 1 if room admin */
int timescalled;
int posted;
unsigned userflags;
long usernum = 0L;		/* user number */
time_t lastcall = 0L;		/* Date/time of previous login */
char newnow;
long highest_msg_read;		/* used for <A>bandon room cmd */
long maxmsgnum;			/* used for <G>oto */
char sigcaught = 0;
char rc_username[USERNAME_SIZE];
char rc_password[32];
char hostbuf[SIZ];
char portbuf[SIZ];
char rc_floor_mode;
char floor_mode;
char curr_floor = 0;		/* number of current floor */
char floorlist[128][SIZ];	/* names of floors */
int termn8 = 0;			/* Set to nonzero to cause a logoff */
int secure;			/* Set to nonzero when wire is encrypted */

extern char instant_msgs;	/* instant messages waiting! */
extern int rc_ansi_color;	/* ansi color value from citadel.rc */
extern int next_lazy_cmd;

CtdlIPC *ipc_for_signal_handlers;	/* KLUDGE cover your eyes */
int enable_syslog = 0;


/*
 * here is our 'clean up gracefully and exit' routine
 */
void ctdl_logoff(char *file, int line, CtdlIPC *ipc, int code)
{
	int lp;

	if (editor_pid > 0) {	/* kill the editor if it's running */
		kill(editor_pid, SIGHUP);
	}

	/* Free the ungoto list */
	for (lp = 0; lp < uglistsize; lp++) {
		free(uglist[lp]);
	}

/* Shut down the server connection ... but not if the logoff code is 3,
 * because that means we're exiting because we already lost the server.
 */
	if (code != 3) {
		CtdlIPCQuit(ipc);
	}

/*
 * now clean up various things
 */
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
	color(ORIGINAL_PAIR);	/* Restore the old color settings */
	stty_ctdl(SB_RESTORE);	/* return the old terminal settings */
	/* 
	 * uncomment the following if you need to know why Citadel exited
	printf("*** Exit code %d at %s:%d\n", code, file, line);
	sleep(2);
	 */
	exit(code);		/* exit with the proper exit code */
}



/*
 * signal catching function for hangups...
 */
void dropcarr(int signum)
{
	logoff(NULL, 3);	/* No IPC when server's already gone! */
}



/*
 * catch SIGCONT to reset terminal modes when were are put back into the
 * foreground.
 */
void catch_sigcont(int signum)
{
	stty_ctdl(SB_LAST);
	signal(SIGCONT, catch_sigcont);
}


/* general purpose routines */

/* display a file */
void formout(CtdlIPC *ipc, char *name)
{
	int r;			/* IPC return code */
	char buf[SIZ];
	char *text = NULL;

	r = CtdlIPCSystemMessage(ipc, name, &text, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	if (text) {
		fmout(screenwidth, NULL, text, NULL, 1);
		free(text);
	}
}


void userlist(CtdlIPC *ipc, char *patn)
{
	char buf[SIZ];
	char fl[SIZ];
	struct tm tmbuf;
	time_t lc;
	int r;				/* IPC response code */
	char *listing = NULL;

	r = CtdlIPCUserListing(ipc, patn, &listing, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}

	scr_printf("       User Name           Num  L Last Visit Logins Messages\n");
	scr_printf("------------------------- ----- - ---------- ------ --------\n");
	if (listing != NULL) while (!IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');

		if (sigcaught == 0) {
		    extract_token(fl, buf, 0, '|', sizeof fl);
		    if (pattern(fl, patn) >= 0) {
			scr_printf("%-25s ", fl);
			scr_printf("%5ld %d ", extract_long(buf, 2),
			       extract_int(buf, 1));
			lc = extract_long(buf, 3);
			localtime_r(&lc, &tmbuf);
			scr_printf("%02d/%02d/%04d ",
			       (tmbuf.tm_mon + 1),
			       tmbuf.tm_mday,
			       (tmbuf.tm_year + 1900));
			scr_printf("%6ld %8ld\n", extract_long(buf, 4), extract_long(buf, 5));
		    }

		}
	}
	free(listing);
	scr_printf("\n");
}


/*
 * grab assorted info about the user...
 */
void load_user_info(char *params)
{
	extract_token(fullname, params, 0, '|', sizeof fullname);
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

	if (marchptr == NULL)
		return;

	if ((!strcasecmp(marchptr->march_name, roomname))
	    || ((!strcasecmp(roomname, "_FLOOR_")) && (marchptr->march_floor == floornum))) {
		mptr = marchptr->next;
		free(marchptr);
		marchptr = mptr;
		return;
	}
	mptr2 = marchptr;
	for (mptr = marchptr; mptr != NULL; mptr = mptr->next) {

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
char *pop_march(int desired_floor, struct march *_march)
{
	static char TheRoom[ROOMNAMELEN];
	int TheWeight = 0;
	int weight;
	struct march *mptr = NULL;

	strcpy(TheRoom, "_BASEROOM_");
	if (_march == NULL)
		return (TheRoom);

	for (mptr = _march; mptr != NULL; mptr = mptr->next) {
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
		}
	}
	return (TheRoom);
}


/*
 * jump directly to a room
 */
void dotgoto(CtdlIPC *ipc, char *towhere, int display_name, int fromungoto)
{
	char aaa[SIZ], bbb[SIZ];
	static long ls = 0L;
	int newmailcount = 0;
	int partial_match, best_match;
	char from_floor;
	int ugpos = uglistsize;
	int r;				/* IPC result code */
	struct ctdlipcroom *room = NULL;
	int rv = 0;

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
		} else {
			uglistsize++;
		}
        
		uglist[ugpos] = malloc(strlen(room_name)+1);
		strcpy(uglist[ugpos], room_name);
		uglistlsn[ugpos] = ls;
	}
      
	/* first try an exact match */
	r = CtdlIPCGotoRoom(ipc, towhere, "", &room, aaa);
	if (r / 10 == 54) {
		newprompt("Enter room password: ", bbb, 9);
		r = CtdlIPCGotoRoom(ipc, towhere, bbb, &room, aaa);
		if (r / 10 == 54) {
			scr_printf("Wrong password.\n");
			return;
		}
	}	

	/*
	 * If a match is not found, try a partial match.
	 * Partial matches anywhere in the string carry a weight of 1,
	 * left-aligned matches carry a weight of 2.  Pick the room that
	 * has the highest-weighted match.  Do not match on forgotten
	 * rooms.
	 */
	if (r / 100 != 2) {
		struct march *march = NULL;

		best_match = 0;
		strcpy(bbb, "");

		r = CtdlIPCKnownRooms(ipc, SubscribedRooms, AllFloors, &march, aaa);
		if (r / 100 == 1) {
			/* Run the roomlist; free the data as we go */
			struct march *mp = march;	/* Current */

			while (mp) {
				partial_match = 0;
				if (pattern(mp->march_name, towhere) >= 0) {
					partial_match = 1;
				}
				if (!strncasecmp(mp->march_name, towhere, strlen(towhere))) {
					partial_match = 2;
				}
				if (partial_match > best_match) {
					strcpy(bbb, mp->march_name);
					best_match = partial_match;
				}
				/* Both pointers are NULL at end of list */
				march = mp->next;
				free(mp);
				mp = march;
			}
		}

		if (IsEmptyStr(bbb)) {
			scr_printf("No room '%s'.\n", towhere);
			return;
		}
		r = CtdlIPCGotoRoom(ipc, bbb, "", &room, aaa);
	}
	if (r / 100 != 1 && r / 100 != 2) {
		scr_printf("%s\n", aaa);
		return;
	}
	safestrncpy(room_name, room->RRname, ROOMNAMELEN);
	room_flags = room->RRflags;
	room_flags2 = room->RRflags2;
	from_floor = curr_floor;
	curr_floor = room->RRfloor;

	/* Determine, based on the room's default view, whether an <E>nter message
	 * command will be valid here.
	 */
	switch(room->RRdefaultview) {
		case VIEW_BBS:
		case VIEW_MAILBOX:
		case VIEW_BLOG:
					entmsg_ok = 1;
					break;
		default:
					entmsg_ok = 0;
					break;
	}

	remove_march(room_name, 0);
	if (!strcasecmp(towhere, "_BASEROOM_"))
		remove_march(towhere, 0);
	if (!room->RRunread)
		next_lazy_cmd = 5;	/* Don't read new if no new msgs */
	if ((from_floor != curr_floor) && (display_name > 0) && (floor_mode == 1)) {
		if (floorlist[(int) curr_floor][0] == 0)
			load_floorlist(ipc);
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
		scr_printf("%d ", room->RRunread);
		color(DIM_WHITE);
		scr_printf("new of ");
		color(BRIGHT_YELLOW);
		scr_printf("%d ", room->RRtotal);
		color(DIM_WHITE);
		scr_printf("messages.\n");
	}
	highest_msg_read = room->RRlastread;
	maxmsgnum = room->RRhighest;
	is_mail = room->RRismailbox;
	is_room_aide = room->RRaide;
	ls = room->RRlastread;

	/* read info file if necessary */
	if (room->RRinfoupdated > 0)
		readinfo(ipc);

	/* check for newly arrived mail if we can */
	newmailcount = room->RRnewmail;
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
		if (!IsEmptyStr(rc_gotmail_cmd)) {
			rv = system(rc_gotmail_cmd);
			if (rv) 
				scr_printf("*** failed to check for mail calling %s Reason %d.\n",
					   rc_gotmail_cmd, rv);
		}
	}
	free(room);

	if (screenwidth>5) snprintf(&status_line[1], screenwidth-1, "%s  |  %s  |  %s  |  %s  |  %d new mail  |",
		(secure ? "Encrypted" : "Unencrypted"),
		ipc->ServInfo.humannode,
		ipc->ServInfo.site_location,
		room_name,
		newmailcount
	);
}

/* Goto next room having unread messages.
 * We want to skip over rooms that the user has already been to, and take the
 * user back to the lobby when done.  The room we end up in is placed in
 * newroom - which is set to 0 (the lobby) initially.
 */
void gotonext(CtdlIPC *ipc)
{
	char buf[SIZ];
	struct march *mptr, *mptr2;
	char next_room[ROOMNAMELEN];

	/* Check to see if the march-mode list is already allocated.
	 * If it is, pop the first room off the list and go there.
	 */
	if (marchptr == NULL) {
		CtdlIPCKnownRooms(ipc, SubscribedRoomsWithNewMessages,
				  AllFloors, &marchptr, buf);

/* add _BASEROOM_ to the end of the march list, so the user will end up
 * in the system base room (usually the Lobby>) at the end of the loop
 */
		mptr = (struct march *) malloc(sizeof(struct march));
		mptr->next = NULL;
		mptr->march_order = 0;
 	   	mptr->march_floor = 0;
		strcpy(mptr->march_name, "_BASEROOM_");
		if (marchptr == NULL) {
			marchptr = mptr;
		} else {
			mptr2 = marchptr;
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
	if (marchptr != NULL) {
		strcpy(next_room, pop_march(curr_floor, marchptr));
	} else {
		strcpy(next_room, "_BASEROOM_");
	}
	remove_march(next_room, 0);
	dotgoto(ipc, next_room, 1, 0);
}

/*
 * forget all rooms on a given floor
 */
void forget_all_rooms_on(CtdlIPC *ipc, int ffloor)
{
	char buf[SIZ];
	struct march *flist = NULL;
	struct march *fptr = NULL;
	struct ctdlipcroom *room = NULL;
	int r;				/* IPC response code */

	scr_printf("Forgetting all rooms on %s...\n", &floorlist[ffloor][0]);
	remove_march("_FLOOR_", ffloor);
	r = CtdlIPCKnownRooms(ipc, AllAccessibleRooms, ffloor, &flist, buf);
	if (r / 100 != 1) {
		scr_printf("Error %d: %s\n", r, buf);
		return;
	}
	while (flist) {
		r = CtdlIPCGotoRoom(ipc, flist->march_name, "", &room, buf);
		if (r / 100 == 2) {
			r = CtdlIPCForgetRoom(ipc, buf);
			if (r / 100 != 2) {
				scr_printf("Error %d: %s\n", r, buf);
			}

		}
		fptr = flist;
		flist = flist->next;
		free(fptr);
	}
	if (room) free(room);
}


/*
 * routine called by gotofloor() to move to a new room on a new floor
 */
void gf_toroom(CtdlIPC *ipc, char *towhere, int mode)
{
	int floor_being_left;

	floor_being_left = curr_floor;

	if (mode == GF_GOTO) {		/* <;G>oto mode */
		updatels(ipc);
		dotgoto(ipc, towhere, 1, 0);
	}
	else if (mode == GF_SKIP) {	/* <;S>kip mode */
		dotgoto(ipc, towhere, 1, 0);
		remove_march("_FLOOR_", floor_being_left);
	}
	else if (mode == GF_ZAP) {	/* <;Z>ap mode */
		dotgoto(ipc, towhere, 1, 0);
		remove_march("_FLOOR_", floor_being_left);
		forget_all_rooms_on(ipc, floor_being_left);
	}
}


/*
 * go to a new floor
 */
void gotofloor(CtdlIPC *ipc, char *towhere, int mode)
{
	int a, tofloor;
	int r;		/* IPC response code */
	struct march *mptr;
	char buf[SIZ], targ[SIZ];

	if (floorlist[0][0] == 0)
		load_floorlist(ipc);
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
	for (mptr = marchptr; mptr != NULL; mptr = mptr->next) {
		if ((mptr->march_floor) == tofloor) {
			gf_toroom(ipc, mptr->march_name, mode);
			return;
		}
	}

	/* Find first known room on the floor */

	strcpy(targ, "");
	mptr = NULL;
	r = CtdlIPCKnownRooms(ipc, SubscribedRooms, tofloor, &mptr, buf);
	if (r / 100 == 1) {
		struct march *tmp = mptr;

		/*. . . according to room order */
		if (mptr)
    	    strcpy(targ, pop_march(tofloor, mptr));
		while (mptr) {
			tmp = mptr->next;
			free(mptr);
			mptr = tmp;
		}
	}
	if (!IsEmptyStr(targ)) {
		gf_toroom(ipc, targ, mode);
		return;
	}

	/* No known rooms on the floor; unzap the first one then */

	strcpy(targ, "");
	mptr = NULL;
	r = CtdlIPCKnownRooms(ipc, AllAccessibleRooms, tofloor, &mptr, buf);
	if (r / 100 == 1) {
		struct march *tmp = mptr;
		
        /*. . . according to room order */
		if (mptr)
			strcpy(targ, pop_march(tofloor, mptr));
		while (mptr) {
			tmp = mptr->next;
			free(mptr);
			mptr = tmp;
		}
	}
	if (!IsEmptyStr(targ)) {
		gf_toroom(ipc, targ, mode);
	} else {
		scr_printf("There are no rooms on '%s'.\n", &floorlist[tofloor][0]);
	}
}

/*
 * Indexing mechanism for a room list, called by gotoroomstep()
 */
void room_tree_list_query(struct ctdlroomlisting *rp, char *findrmname, int findrmslot, char *rmname, int *rmslot, int *rmtotal)
{
	char roomname[ROOMNAMELEN];
	static int cur_rmslot = 0;

	if (rp == NULL) {
		cur_rmslot = 0;
		return;
	}

	if (rp->lnext != NULL) {
		room_tree_list_query(rp->lnext, findrmname, findrmslot, rmname, rmslot, rmtotal);
	}

	if (sigcaught == 0) {
		strcpy(roomname, rp->rlname);

		if (rmname != NULL) {
			if (cur_rmslot == findrmslot) {
				strcpy(rmname, roomname);
			}
		}
		if (rmslot != NULL) {
			if (!strcmp(roomname, findrmname)) {
				*rmslot = cur_rmslot;
			}
		}
		cur_rmslot++;
	}

	if (rp->rnext != NULL) {
		room_tree_list_query(rp->rnext, findrmname, findrmslot, rmname, rmslot, rmtotal);
	}

	if ((rmname == NULL) && (rmslot == NULL))
		free(rp);

	if (rmtotal != NULL) {
		*rmtotal = cur_rmslot;
	}
}

/*
 * step through rooms on current floor
 */
void  gotoroomstep(CtdlIPC *ipc, int direction, int mode)
{
	struct march *listing = NULL;
	struct march *mptr;
	int r;		/* IPC response code */
	char buf[SIZ];
	struct ctdlroomlisting *rl = NULL;
	struct ctdlroomlisting *rp;
	struct ctdlroomlisting *rs;
	int list_it;
	char rmname[ROOMNAMELEN];
	int rmslot = 0;
	int rmtotal;

	/* Ask the server for a room list */
	r = CtdlIPCKnownRooms(ipc, SubscribedRooms, (-1), &listing, buf);
	if (r / 100 != 1) {
		listing = NULL;
	}

	load_floorlist(ipc);

	for (mptr = listing; mptr != NULL; mptr = mptr->next) {
		list_it = 1;

		if ( floor_mode 
			 && (mptr->march_floor != curr_floor))
			list_it = 0;

		if (list_it) {
			rp = malloc(sizeof(struct ctdlroomlisting));
			strncpy(rp->rlname, mptr->march_name, ROOMNAMELEN);
			rp->rlflags = mptr->march_flags;
			rp->rlfloor = mptr->march_floor;
			rp->rlorder = mptr->march_order;
			rp->lnext = NULL;
			rp->rnext = NULL;

			rs = rl;
			if (rl == NULL) {
				rl = rp;
			} else {
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
		}
	}

	/* Find position of current room */
	room_tree_list_query(NULL, NULL, 0, NULL, NULL, NULL);
	room_tree_list_query(rl,  room_name, 0, NULL, &rmslot, &rmtotal);

	if (direction == 0) { /* Previous room */
		/* If we're at the first room, wrap to the last room */
		if (rmslot == 0) {
			rmslot = rmtotal - 1;
		} else {
			rmslot--;
		}
	} else {		 /* Next room */
		/* If we're at the last room, wrap to the first room */
		if (rmslot == rmtotal - 1) {
			rmslot = 0; 
		} else {
			rmslot++;
		}
	}

	/* Get name of next/previous room */
	room_tree_list_query(NULL, NULL, 0, NULL, NULL, NULL);
	room_tree_list_query(rl,  NULL, rmslot, rmname, NULL, NULL);

	/* Free the tree */
	room_tree_list_query(rl, NULL, 0, NULL, NULL, NULL);

	if (mode == 0) { /* not skipping */
	    updatels(ipc);
	}

	/* Free the room list */
	while (listing) {
		mptr = listing->next;
		free(listing);
		listing = mptr;
	};

	dotgoto(ipc, rmname, 1, 0);
}


/*
 * step through floors on system
 */
void  gotofloorstep(CtdlIPC *ipc, int direction, int mode)
{
	int  tofloor;

	if (floorlist[0][0] == 0)
		load_floorlist(ipc);

	empty_keep_going:

	if (direction == 0) { /* Previous floor */
		if (curr_floor)	tofloor = curr_floor - 1;
		else tofloor = 127;

		while (!floorlist[tofloor][0]) tofloor--;
	} else {		   /* Next floor */
		if (curr_floor < 127) tofloor = curr_floor + 1;
		else tofloor = 0;

		while (!floorlist[tofloor][0] && tofloor < 127)	tofloor++;
		if (!floorlist[tofloor][0])	tofloor = 0;
	}
	/* ;g works when not in floor mode so . . . */
	if (!floor_mode) {
		scr_printf("(%s)\n", floorlist[tofloor] );
	}

	gotofloor(ipc, floorlist[tofloor], mode);
	if (curr_floor != tofloor) { /* gotofloor failed */
	     curr_floor = tofloor;
	     goto empty_keep_going;
	}	     
}

/* 
 * Display user 'preferences'.
 */
extern int rc_prompt_control;
void read_config(CtdlIPC *ipc)
{
	char buf[SIZ];
	char *resp = NULL;
	int r;			/* IPC response code */
    char _fullname[USERNAME_SIZE];
	long _usernum;
	int _axlevel, _timescalled, _posted;
	time_t _lastcall;
	struct ctdluser *user = NULL;

	/* get misc user info */   
	r = CtdlIPCGetBio(ipc, fullname, &resp, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	extract_token(_fullname, buf, 1, '|', sizeof fullname);
	_usernum = extract_long(buf, 2);
	_axlevel = extract_int(buf, 3);
	_lastcall = extract_long(buf, 4);
    _timescalled = extract_int(buf, 5);
	_posted = extract_int(buf, 6);
	free(resp);
	resp = NULL;
   
	/* get preferences */
	r = CtdlIPCGetConfig(ipc, &user, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		free(user);
		return;
	}

	/* show misc user info */
	scr_printf("%s\nAccess level: %d (%s)\n"
		   "User #%ld / %d Calls / %d Posts",
		   _fullname, _axlevel, axdefs[(int) _axlevel],
		   _usernum, _timescalled, _posted);
	if (_lastcall > 0L) {
		scr_printf(" / Curr login: %s",
			   asctime(localtime(&_lastcall)));
	}
	scr_printf("\n");

	/* show preferences */
	scr_printf("Are you an experienced Citadel user: ");                   color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_EXPERT) ? "Yes" : "No");     color(DIM_WHITE);
	scr_printf("Print last old message on New message request: ");         color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_LASTOLD)? "Yes" : "No");     color(DIM_WHITE);
	scr_printf("Prompt after each message: ");                             color(BRIGHT_CYAN); scr_printf("%s\n", (!(user->flags & US_NOPROMPT))? "Yes" : "No"); color(DIM_WHITE);
	if ((user->flags & US_NOPROMPT) == 0) {
    	scr_printf("Use 'disappearing' prompts: ");                        color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_DISAPPEAR)? "Yes" : "No");   color(DIM_WHITE);
	}
	scr_printf("Pause after each screenful of text: ");                    color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_PAGINATOR)? "Yes" : "No");   color(DIM_WHITE);
    if (rc_prompt_control == 3 && (user->flags & US_PAGINATOR)) {
    	scr_printf("<N>ext and <S>top work at paginator prompt: ");        color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_PROMPTCTL)? "Yes" : "No");   color(DIM_WHITE);
	}
    if (rc_floor_mode == RC_DEFAULT) {
    	scr_printf("View rooms by floor: ");                               color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_FLOORS)? "Yes" : "No");	     color(DIM_WHITE);
	}
	if (rc_ansi_color == 3)	{
	    scr_printf("Enable color support: ");                              color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_COLOR)? "Yes" : "No");	     color(DIM_WHITE);
	}
	scr_printf("Be unlisted in userlog: ");                                color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_UNLISTED)? "Yes" : "No");    color(DIM_WHITE);
	if (!IsEmptyStr(editor_path)) {
    	scr_printf("Always enter messages with the full-screen editor: "); color(BRIGHT_CYAN); scr_printf("%s\n", (user->flags & US_EXTEDIT)? "Yes" : "No");     color(DIM_WHITE);
	}
	free(user);
}

/*
 * Display system statistics.
 */
void system_info(CtdlIPC *ipc)
{
	char buf[SIZ];
	char *resp = NULL;
	size_t bytes;
	int mrtg_users, mrtg_active_users; 
	char mrtg_server_uptime[40];
	long mrtg_himessage;

	/* get #users, #active & server uptime */
	CtdlIPCGenericCommand(ipc, "MRTG|users", NULL, 0, &resp, &bytes, buf);
	mrtg_users = extract_int(resp, 0);
	remove_token(resp, 0, '\n');
	mrtg_active_users = extract_int(resp, 0);
	remove_token(resp, 0, '\n');
	extract_token(mrtg_server_uptime, resp, 0, '\n', sizeof mrtg_server_uptime);
	free(resp);
	resp = NULL;

	/* get high message# */
	CtdlIPCGenericCommand(ipc, "MRTG|messages", NULL, 0, &resp, &bytes, buf);
	mrtg_himessage = extract_long(resp, 0);
	free(resp);
	resp = NULL;

	/* refresh server info just in case */
	CtdlIPCServerInfo(ipc, buf);

	scr_printf("You are connected to %s (%s) @%s\n", ipc->ServInfo.nodename, ipc->ServInfo.humannode, ipc->ServInfo.fqdn);
	scr_printf("running %s with text client v%.2f,\n", ipc->ServInfo.software, (float)CLIENT_VERSION/100);
	scr_printf("server build %s,\n", ipc->ServInfo.svn_revision, (float)CLIENT_VERSION/100);
	scr_printf("and located in %s.\n", ipc->ServInfo.site_location);
	scr_printf("Connected users %d / Active users %d / Highest message #%ld\n", mrtg_users, mrtg_active_users, mrtg_himessage);
	scr_printf("Server uptime: %s\n", mrtg_server_uptime);
	scr_printf("Your system administrator is %s.\n", ipc->ServInfo.sysadm);
}

/*
 * forget all rooms on current floor
 */
void forget_this_floor(CtdlIPC *ipc)
{
	if (curr_floor == 0) {
		scr_printf("Can't forget this floor.\n");
		return;
	}
	if (floorlist[0][0] == 0) {
		load_floorlist(ipc);
	}
	scr_printf("Are you sure you want to forget all rooms on %s? ",
	       &floorlist[(int) curr_floor][0]);
	if (yesno() == 0) {
		return;
	}

	gf_toroom(ipc, "_BASEROOM_", GF_ZAP);
}


/*
 * set floor mode depending on client, server, and user settings
 */
void set_floor_mode(CtdlIPC* ipc)
{
	if (ipc->ServInfo.ok_floors == 0) {
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
int set_password(CtdlIPC *ipc)
{
	char pass1[20];
	char pass2[20];
	char buf[SIZ];

	if (!IsEmptyStr(rc_password)) {
		strcpy(pass1, rc_password);
		strcpy(pass2, rc_password);
	} else {
		IFNEXPERT formout(ipc, "changepw");
		newprompt("Enter a new password: ", pass1, -19);
		newprompt("Enter it again to confirm: ", pass2, -19);
	}
	strproc(pass1);
	strproc(pass2);
	if (!strcasecmp(pass1, pass2)) {
		CtdlIPCChangePassword(ipc, pass1, buf);
		scr_printf("%s\n", buf);
		offer_to_remember_password(ipc, hostbuf, portbuf, fullname, pass1);
		return (0);
	} else {
		scr_printf("*** They don't match... try again.\n");
		return (1);
	}
}



/*
 * get info about the server we've connected to
 */
void get_serv_info(CtdlIPC *ipc, char *supplied_hostname)
{
	char buf[SIZ];

	CtdlIPCServerInfo(ipc, buf);
	moreprompt = ipc->ServInfo.moreprompt;

	/* be nice and identify ourself to the server */
	CtdlIPCIdentifySoftware(ipc, CLIENT_TYPE, 0, CLIENT_VERSION,
		 (ipc->isLocal ? "local" : PACKAGE_STRING),
		 (supplied_hostname) ? supplied_hostname : 
		 /* Look up the , in the bible if you're confused */
		 (locate_host(ipc, buf), buf), buf);

	/* Indicate to the server that we prefer to decode Base64 and
	 * quoted-printable on the client side.
	 */
	if ((CtdlIPCSpecifyPreferredFormats(ipc, buf, "dont_decode") / 100 ) != 2) {
		scr_printf("ERROR: Extremely old server; MSG4 framework not supported.\n");
		logoff(ipc, 0);
	}

	/*
	 * Tell the server what our preferred content formats are.
	 *
	 * Originally we preferred HTML over plain text because we can format
	 * it to the reader's screen width, but since our HTML-to-text parser
	 * isn't really all that great, it's probably better to just go with
	 * the plain text when we have it available.
	 */
	if ((CtdlIPCSpecifyPreferredFormats(ipc, buf, "text/plain|text/html") / 100 ) != 2) {
		scr_printf("ERROR: Extremely old server; MSG4 framework not supported.\n");
		logoff(ipc, 0);
	}
}



/*
 * Session username compare function for SortOnlineUsers()
 */
int rwho_username_cmp(const void *rec1, const void *rec2) {
	char *u1, *u2;

	u1 = strchr(rec1, '|');
	u2 = strchr(rec2, '|');

	return strcasecmp( (u1?++u1:"") , (u2?++u2:"") );
}


/*
 * Idle time compare function for SortOnlineUsers()
 */
int idlecmp(const void *rec1, const void *rec2) {
	time_t i1, i2;

	i1 = extract_long(rec1, 5);
	i2 = extract_long(rec2, 5);

	if (i1 < i2) return(1);
	if (i1 > i2) return(-1);
	return(0);
}


/*
 * Sort the list of online users by idle time.
 * This function frees the supplied buffer, and returns a new one
 * to the caller.  The caller is responsible for freeing the returned buffer.
 *
 * If 'condense' is nonzero, multiple sessions for the same user will be
 * combined into one for brevity.
 */
char *SortOnlineUsers(char *listing, int condense) {
	int rows;
	char *sortbuf;
	char *retbuf;
	char buf[SIZ];
	int i;

	rows = num_tokens(listing, '\n');
	sortbuf = malloc(rows * SIZ);
	if (sortbuf == NULL) return(listing);
	retbuf = malloc(rows * SIZ);
	if (retbuf == NULL) {
		free(sortbuf);
		return(listing);
	}

	/* Copy the list into a fixed-record-size array for sorting */
	for (i=0; i<rows; ++i) {
		memset(buf, 0, SIZ);
		extract_token(buf, listing, i, '\n', sizeof buf);
		memcpy(&sortbuf[i*SIZ], buf, (size_t)SIZ);
	}

	/* Sort by idle time */
	qsort(sortbuf, rows, SIZ, idlecmp);

	/* Combine multiple sessions for the same user */
	if (condense) {
		qsort(sortbuf, rows, SIZ, rwho_username_cmp);
		if (rows > 1) for (i=1; i<rows; ++i) if (i>0) {
			char u1[USERNAME_SIZE];
			char u2[USERNAME_SIZE];
			extract_token(u1, &sortbuf[(i-1)*SIZ], 1, '|', sizeof u1);
			extract_token(u2, &sortbuf[i*SIZ], 1, '|', sizeof u2);
			if (!strcasecmp(u1, u2)) {
				memcpy(&sortbuf[i*SIZ], &sortbuf[(i+1)*SIZ], (rows-i-1)*SIZ);
				--rows;
				--i;
			}
		}

		qsort(sortbuf, rows, SIZ, idlecmp);	/* idle sort again */
	}

	/* Copy back to a \n delimited list */
	strcpy(retbuf, "");
	for (i=0; i<rows; ++i) {
		if (!IsEmptyStr(&sortbuf[i*SIZ])) {
			strcat(retbuf, &sortbuf[i*SIZ]);
			if (i<(rows-1)) strcat(retbuf, "\n");
		}
	}
	free(listing);
	free(sortbuf);
	return(retbuf);
}



/*
 * Display list of users currently logged on to the server
 */
void who_is_online(CtdlIPC *ipc, int longlist)
{
	char buf[SIZ], username[SIZ], roomname[SIZ], fromhost[SIZ];
	char flags[SIZ];
	char actual_user[SIZ], actual_room[SIZ], actual_host[SIZ];
	char clientsoft[SIZ];
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
		scr_printf("           User Name               Room          ");
		if (screenwidth >= 80) scr_printf(" Idle        From host");
		scr_printf("\n");
		color(DIM_WHITE);
		scr_printf("   ------------------------- --------------------");
		if (screenwidth >= 80) scr_printf(" ---- ------------------------");
		scr_printf("\n");
	}
	r = CtdlIPCOnlineUsers(ipc, &listing, &timenow, buf);
	listing = SortOnlineUsers(listing, (!longlist));
	if (r / 100 == 1) {
		while (!IsEmptyStr(listing)) {
			int isidle = 0;
			
			/* Get another line */
			extract_token(buf, listing, 0, '\n', sizeof buf);
			remove_token(listing, 0, '\n');

			extract_token(username, buf, 1, '|', sizeof username);
			extract_token(roomname, buf, 2, '|', sizeof roomname);
			extract_token(fromhost, buf, 3, '|', sizeof fromhost);
			extract_token(clientsoft, buf, 4, '|', sizeof clientsoft);
			extract_token(flags, buf, 7, '|', sizeof flags);

			idletime = timenow - extract_long(buf, 5);
			idlehours = idletime / 3600;
			idlemins = (idletime - (idlehours * 3600)) / 60;
			idlesecs = (idletime - (idlehours * 3600) - (idlemins * 60));

			if (idletime > rc_idle_threshold) {
				if (skipidle) {
					isidle = 1;
				}
			}

			if (longlist) {
				extract_token(actual_user, buf, 8, '|', sizeof actual_user);
				extract_token(actual_room, buf, 9, '|', sizeof actual_room);
				extract_token(actual_host, buf, 10, '|', sizeof actual_host);

				scr_printf("  Flags: %s\n", flags);
				scr_printf("Session: %d\n", extract_int(buf, 0));
				scr_printf("   Name: %s\n", username);
				scr_printf("In room: %s\n", roomname);
				scr_printf("   Host: %s\n", fromhost);
				scr_printf(" Client: %s\n", clientsoft);
				scr_printf("   Idle: %ld:%02ld:%02ld\n",
					(long) idlehours,
					(long) idlemins,
					(long) idlesecs);

				if ( (!IsEmptyStr(actual_user)&&
				      !IsEmptyStr(actual_room)&&
				      !IsEmptyStr(actual_host))) {
					scr_printf("(really ");
					if (!IsEmptyStr(actual_user)) scr_printf("<%s> ", actual_user);
					if (!IsEmptyStr(actual_room)) scr_printf("in <%s> ", actual_room);
					if (!IsEmptyStr(actual_host)) scr_printf("from <%s> ", actual_host);
					scr_printf(")\n");
				}
				scr_printf("\n");

			} else {
				if (isidle == 0) {
    					if (extract_int(buf, 0) == last_session) {
    						scr_printf("        ");
    					}
					else {
    						color(BRIGHT_MAGENTA);
    						scr_printf("%-3s", flags);
    					}
    					last_session = extract_int(buf, 0);
    					color(BRIGHT_CYAN);
    					scr_printf("%-25s ", username);
    					color(BRIGHT_MAGENTA);
    					roomname[20] = 0;
    					scr_printf("%-20s", roomname);

					if (screenwidth >= 80) {
						scr_printf(" ");
						if (idletime > rc_idle_threshold) {
							/* over 1000d, must be gone fishing */
							if (idlehours > 23999) {
								scr_printf("fish");
							/* over 10 days */
							} else if (idlehours > 239) {
								scr_printf("%3ldd", idlehours / 24);
							/* over 10 hours */
							} else if (idlehours > 9) {
								scr_printf("%1ldd%02ld",
									idlehours / 24,
									idlehours % 24);
							/* less than 10 hours */
							}
							else {
								scr_printf("%1ld:%02ld", idlehours, idlemins);
							}
						}
						else {
							scr_printf("    ");
						}
						scr_printf(" ");
    						color(BRIGHT_CYAN);
    						fromhost[24] = '\0';
    						scr_printf("%-24s", fromhost);
					}
					scr_printf("\n");
    					color(DIM_WHITE);
    	  			}
			}
		}
	}
	free(listing);
}

void enternew(CtdlIPC *ipc, char *desc, char *buf, int maxlen)
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
	scr_printf(s);
}

/*
 * main
 */
int main(int argc, char **argv)
{
	int a, b, mcmd;
	char aaa[100], bbb[100];/* general purpose variables */
	char argbuf[64];	/* command line buf */
	char nonce[NONCE_SIZE];
	char *telnet_client_host = NULL;
	char *sptr, *sptr2;	/* USed to extract the nonce */
	char hexstring[MD5_HEXSTRING_SIZE];
	char password[SIZ];
	struct ctdlipcmisc chek;
	struct ctdluser *myself = NULL;
	CtdlIPC* ipc;			/* Our server connection */
	int r;				/* IPC result code */
	int rv = 0;			/* fetch but ignore syscall return value to suppress warnings */

	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
    int lp; 
#ifdef HAVE_BACKTRACE
	eCrashParameters params;
//	eCrashSymbolTable symbol_table;
#endif
	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);

#ifdef HAVE_BACKTRACE
	bzero(&params, sizeof(params));
//	params.filename = file_pid_paniclog;
//	panic_fd=open(file_pid_paniclog, O_APPEND|O_CREAT|O_DIRECT);
///	params.filep = fopen(file_pid_paniclog, "a+");
	params.debugLevel = ECRASH_DEBUG_VERBOSE;
	params.dumpAllThreads = TRUE;
	params.useBacktraceSymbols = 1;
///	BuildSymbolTable(&symbol_table);
//	params.symbolTable = &symbol_table;
	params.signals[0]=SIGSEGV;
	params.signals[1]=SIGILL;
	params.signals[2]=SIGBUS;
	params.signals[3]=SIGABRT;

///	eCrash_Init(&params);
#endif	
	setIPCErrorPrintf(scr_printf);
	setCryptoStatusHook(statusHook);
	
	/* Permissions sanity check - don't run citadel setuid/setgid */
	if (getuid() != geteuid()) {
		scr_printf("Please do not run citadel setuid!\n");
		logoff(NULL, 3);
	} else if (getgid() != getegid()) {
		scr_printf("Please do not run citadel setgid!\n");
		logoff(NULL, 3);
	}

	stty_ctdl(SB_SAVE);	/* Store the old terminal parameters */
	load_command_set();	/* parse the citadel.rc file */
	stty_ctdl(SB_NO_INTR);	/* Install the new ones */
	/* signal(SIGHUP, dropcarr);FIXME */	/* Cleanup gracefully if carrier is dropped */
	signal(SIGPIPE, dropcarr);	/* Cleanup gracefully if local conn. dropped */
	signal(SIGTERM, dropcarr);	/* Cleanup gracefully if terminated */
	signal(SIGCONT, catch_sigcont);	/* Catch SIGCONT so we can reset terminal */
#ifdef SIGWINCH
	signal(SIGWINCH, scr_winch);	/* Window resize signal */
#endif

#ifdef HAVE_OPENSSL
	arg_encrypt = RC_DEFAULT;
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
		if (!strcmp(argv[a], "-p")) {
			struct stat st;
		
			if (chdir(CTDLDIR) < 0) {
				perror("can't change to " CTDLDIR);
				logoff(NULL, 3);
			}

			/*
			 * Drop privileges if necessary. We stat
			 * citadel.config to get the uid/gid since it's
			 * guaranteed to have the uid/gid we want.
			 */
			if (!getuid() || !getgid()) {
				if (stat(file_citadel_config, &st) < 0) {
					perror("couldn't stat citadel.config");
					logoff(NULL, 3);
				}
				if (!getgid() && (setgid(st.st_gid) < 0)) {
					perror("couldn't change gid");
					logoff(NULL, 3);
				}
				if (!getuid() && (setuid(st.st_uid) < 0)) {
					perror("couldn't change uid");
					logoff(NULL, 3);
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
	/* Get screen dimensions.  First we go to a default of 80x24.
	 * Then attempt to read the actual screen size from the terminal.
	 */
	check_screen_dims();


#ifdef __CYGWIN__
	newprompt("Connect to (return for local server): ", hostbuf, 64);
#endif

	scr_printf("Attaching to server...\n");
	ipc = CtdlIPC_new(argc, argv, hostbuf, portbuf);
	if (!ipc) {
		error_printf("Can't connect: %s\n", strerror(errno));
		logoff(NULL, 3);
	}

	CtdlIPC_SetNetworkStatusCallback(ipc, scr_wait_indicator);

	if (!(ipc->isLocal)) {
		scr_printf("Connected to %s [%s].\n", ipc->ip_hostname, ipc->ip_address);
	}

	ipc_for_signal_handlers = ipc;	/* KLUDGE cover your eyes */

	CtdlIPC_chat_recv(ipc, aaa);
	if (aaa[0] != '2') {
		scr_printf("%s\n", &aaa[4]);
		logoff(ipc, atoi(aaa));
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
					strncpy(nonce, sptr, (size_t)NONCE_SIZE);
				}
		}

#ifdef HAVE_OPENSSL
	/* Evaluate encryption preferences */
	if (arg_encrypt != RC_NO && rc_encrypt != RC_NO) {
		if (!ipc->isLocal || arg_encrypt == RC_YES || rc_encrypt == RC_YES) {
			secure = (CtdlIPCStartEncryption(ipc, aaa) / 100 == 2) ? 1 : 0;
			if (!secure)
				error_printf("Can't encrypt: %s\n", aaa);
		}
	}
#endif

	get_serv_info(ipc, telnet_client_host);
	scr_printf("%-24s\n%s\n%s\n", ipc->ServInfo.software, ipc->ServInfo.humannode,
		   ipc->ServInfo.site_location);

	scr_printf(" pause    next    stop\n");
	scr_printf(" ctrl-s  ctrl-o  ctrl-c\n\n");
	formout(ipc, "hello");	/* print the opening greeting */
	scr_printf("\n");

 GSTA:	/* See if we have a username and password on disk */
	if (rc_remember_passwords) {
		get_stored_password(hostbuf, portbuf, fullname, password);
		if (!IsEmptyStr(fullname)) {
			r = CtdlIPCTryLogin(ipc, fullname, aaa);
			if (r / 100 == 3) {
				if (*nonce) {
					r = CtdlIPCTryApopPassword(ipc, make_apop_string(password, nonce, hexstring, sizeof hexstring), aaa);
				} else {
					r = CtdlIPCTryPassword(ipc, password, aaa);
				}
			}

			if (r / 100 == 2) {
				load_user_info(aaa);
				goto PWOK;
			} else {
				set_stored_password(hostbuf, portbuf, "", "");
			}
		}
	}

	termn8 = 0;
	newnow = 0;
	do {
		if (!IsEmptyStr(rc_username)) {
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
		 || (IsEmptyStr(fullname)));

	if (!strcasecmp(fullname, "off")) {
		mcmd = 29;
		goto TERMN8;
	}

	/* FIXME this is a stupid way to do guest mode but it's a reasonable test harness FIXME */
	if ( (ipc->ServInfo.guest_logins) && (!strcasecmp(fullname, "guest")) ) {
		goto PWOK;
	}

	/* sign on to the server */
	r = CtdlIPCTryLogin(ipc, fullname, aaa);
	if (r / 100 != 3)
		goto NEWUSR;

	/* password authentication */
	if (!IsEmptyStr(rc_password)) {
		strcpy(password, rc_password);
	} else {
		newprompt("\rPlease enter your password: ", password, -(SIZ-1));
	}

	if (*nonce) {
		r = CtdlIPCTryApopPassword(ipc, make_apop_string(password, nonce, hexstring, sizeof hexstring), aaa);
		if (r / 100 != 2) {
			strproc(password);
			r = CtdlIPCTryApopPassword(ipc, make_apop_string(password, nonce, hexstring, sizeof hexstring), aaa);
		}
	} else {
		r = CtdlIPCTryPassword(ipc, password, aaa);
		if (r / 100 != 2) {
			strproc(password);
			r = CtdlIPCTryPassword(ipc, password, aaa);
		}
	}
	
	if (r / 100 == 2) {
		load_user_info(aaa);
		offer_to_remember_password(ipc, hostbuf, portbuf,
					   fullname, password);
		goto PWOK;
	}
	scr_printf("<< wrong password >>\n");
	if (!IsEmptyStr(rc_password))
		logoff(ipc, 2);
	goto GSTA;

NEWUSR:	if (IsEmptyStr(rc_password)) {
		scr_printf("'%s' not found.\n", fullname);
		scr_printf("Type 'off' if you would like to exit.\n");
		if (ipc->ServInfo.newuser_disabled == 1) {
			goto GSTA;
		}
		scr_printf("Do you want to create a new user account called '%s'? ",
			fullname);
		if (yesno() == 0) {
			goto GSTA;
		}
	}

	r = CtdlIPCCreateUser(ipc, fullname, 1, aaa);
	if (r / 100 != 2) {
		scr_printf("%s\n", aaa);
		goto GSTA;
	}
	load_user_info(aaa);

	while (set_password(ipc) != 0);
	newnow = 1;

	enter_config(ipc, 1);

 PWOK:
	/* Switch color support on or off if we're in user mode */
	if (rc_ansi_color == 3) {
		if (userflags & US_COLOR)
			enable_color = 1;
		else
			enable_color = 0;
	}

	color(BRIGHT_WHITE);
	scr_printf("\n%s\nAccess level: %d (%s)\n"
		   "User #%ld / Login #%d",
		   fullname, axlevel, axdefs[(int) axlevel],
		   usernum, timescalled);
	if (lastcall > 0L) {
		scr_printf(" / Last login: %s",
			   asctime(localtime(&lastcall)));
	}
	scr_printf("\n");

	r = CtdlIPCMiscCheck(ipc, &chek, aaa);
	if (r / 100 == 2) {
		b = chek.newmail;
		if (b > 0) {
			color(BRIGHT_RED);
			if (b == 1)
				scr_printf("*** You have a new private message in Mail>\n");
			if (b > 1)
				scr_printf("*** You have %d new private messages in Mail>\n", b);
			color(DIM_WHITE);
			if (!IsEmptyStr(rc_gotmail_cmd)) {
				rv = system(rc_gotmail_cmd);
				if (rv)
					scr_printf("*** failed to check for mail calling %s Reason %d.\n",
						   rc_gotmail_cmd, rv);

			}
		}
		if ((axlevel >= AxAideU) && (chek.needvalid > 0)) {
			scr_printf("*** Users need validation\n");
		}
		if (chek.needregis > 0) {
			scr_printf("*** Please register.\n");
			formout(ipc, "register");
			entregis(ipc);
		}
	}
	/* Make up some temporary filenames for use in various parts of the
	 * program.  Don't mess with these once they've been set, because we
	 * will be unlinking them later on in the program and we don't
	 * want to delete something that we didn't create. */
	CtdlMakeTempFileName(temp, sizeof temp);
	CtdlMakeTempFileName(temp2, sizeof temp2);
	CtdlMakeTempFileName(tempdir, sizeof tempdir);

	r = CtdlIPCGetConfig(ipc, &myself, aaa);
	set_floor_mode(ipc);

	/* Enter the lobby */
	dotgoto(ipc, "_BASEROOM_", 1, 0);

	/* Main loop for the system... user is logged in. */
    free(uglist[0]);
	uglistsize = 0;

	if (newnow == 1)
		readmsgs(ipc, LastMessages, ReadForward, 5);
	else
		readmsgs(ipc, NewMessages, ReadForward, 0);

	/* MAIN COMMAND LOOP */
	do {
		mcmd = getcmd(ipc, argbuf);	/* Get keyboard command */

#ifdef TIOCGWINSZ
		check_screen_dims();		/* get screen size */
#endif

		if (termn8 == 0)
			switch (mcmd) {
			case 1:
				formout(ipc, "help");
				break;
			case 4:
				entmsg(ipc, 0, ((userflags & US_EXTEDIT) ? 2 : 0), 0);
				break;
			case 36:
				entmsg(ipc, 0, 1, 0);
				break;
			case 46:
				entmsg(ipc, 0, 2, 0);
				break;
			case 78:
				entmsg(ipc, 0, ((userflags & US_EXTEDIT) ? 2 : 0), 1);
				break;
			case 5:				/* <G>oto */
				updatels(ipc);
				gotonext(ipc);
				break;
			case 47:			/* <A>bandon */
				gotonext(ipc);
				break;
			case 90:			/* <.A>bandon */
				dotgoto(ipc, argbuf, 0, 0);
				break;
			case 58:			/* <M>ail */
				updatelsa(ipc);
				dotgoto(ipc, "_MAIL_", 1, 0);
				break;
			case 20:
				if (!IsEmptyStr(argbuf)) {
					updatels(ipc);
					dotgoto(ipc, argbuf, 0, 0);
				}
				break;
			case 52:
				if (!IsEmptyStr(argbuf)) {
					dotgoto(ipc, argbuf, 0, 0);
				}
				break;
			case 95: /* what exactly is the numbering scheme supposed to be anyway? --Ford, there isn't one. -IO */
				dotungoto(ipc, argbuf);
				break;
			case 10:
				readmsgs(ipc, AllMessages, ReadForward, 0);
				break;
			case 9:
				readmsgs(ipc, LastMessages, ReadForward, 5);
				break;
			case 13:
				readmsgs(ipc, NewMessages, ReadForward, 0);
				break;
			case 11:
				readmsgs(ipc, AllMessages, ReadReverse, 0);
				break;
			case 12:
				readmsgs(ipc, OldMessages, ReadReverse, 0);
				break;
			case 71:
				readmsgs(ipc, LastMessages, ReadForward,
						atoi(argbuf));
				break;
			case 7:
				forget(ipc);
				break;
			case 18:
				subshell();
				break;
			case 38:
				updatels(ipc);
				entroom(ipc);
				break;
			case 22:
				killroom(ipc);
				break;
			case 32:
				userlist(ipc, argbuf);
				break;
			case 27:
				invite(ipc);
				break;
			case 28:
				kickout(ipc);
				break;
			case 23:
				editthisroom(ipc);
				break;
			case 14:
				roomdir(ipc);
				break;
			case 33:
				download(ipc, 0);
				break;
			case 34:
				download(ipc, 1);
				break;
			case 31:
				download(ipc, 2);
				break;
			case 43:
				download(ipc, 3);
				break;
			case 45:
				download(ipc, 4);
				break;
			case 55:
				download(ipc, 5);
				break;
			case 39:
				upload(ipc, 0);
				break;
			case 40:
				upload(ipc, 1);
				break;
			case 42:
				upload(ipc, 2);
				break;
			case 44:
				upload(ipc, 3);
				break;
			case 57:
				cli_upload(ipc);
				break;
			case 16:
				ungoto(ipc);
				break;
			case 24:
				whoknows(ipc);
				break;
			case 26:
				validate(ipc);
				break;
			case 29:
			case 30:
				updatels(ipc);
				termn8 = 1;
				break;
			case 48:
				enterinfo(ipc);
				break;
			case 49:
				readinfo(ipc);
				break;
			case 72:
				cli_image_upload(ipc, "_userpic_");
				break;
			case 73:
				cli_image_upload(ipc, "_roompic_");
				break;
			case 75:
				enternew(ipc, "roomname", aaa, 20);
				r = CtdlIPCChangeRoomname(ipc, aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", bbb);
				break;
			case 76:
				enternew(ipc, "hostname", aaa, 25);
				r = CtdlIPCChangeHostname(ipc, aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", bbb);
				break;
			case 77:
				enternew(ipc, "username", aaa, 32);
				r = CtdlIPCChangeUsername(ipc, aaa, bbb);
				if (r / 100 != 2)
					scr_printf("\n%s\n", bbb);
				break;

			case 35:
				set_password(ipc);
				break;

			case 21:
				if (argbuf[0] == 0)
					strcpy(aaa, "?");
				display_help(ipc, argbuf);
				break;

			case 41:
				formout(ipc, "register");
				entregis(ipc);
				break;

			case 15:
				scr_printf("Are you sure (y/n)? ");
				if (yesno() == 1) {
					updatels(ipc);
					a = 0;
					termn8 = 1;
				}
				break;

			case 85:
				scr_printf("All users will be disconnected!  "
					   "Really terminate the server? ");
				if (yesno() == 1) {
					updatels(ipc);
					r = CtdlIPCTerminateServerNow(ipc, aaa);
					scr_printf("%s\n", aaa);
					if (r / 100 == 2) {
						a = 0;
						termn8 = 1;
					}
				}
				break;

			case 86:
				scr_printf("Do you really want to schedule a "
					   "server shutdown? ");
				if (yesno() == 1) {
					r = CtdlIPCTerminateServerScheduled(ipc, 1, aaa);
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
				network_config_management(ipc, "listrecp",
							  "Message-by-message mailing list recipients");
				break;

			case 94:
				network_config_management(ipc, "digestrecp",
							  "Digest mailing list recipients");
				break;

			case 89:
				network_config_management(ipc, "ignet_push_share",
							  "Nodes with which we share this room");
				break;

			case 88:
				do_ignet_configuration(ipc);
				break;

			case 92:
				do_filterlist_configuration(ipc);
				break;

			case 6:
				gotonext(ipc);
				break;

			case 3:
				chatmode(ipc);
				break;

			case 17:
				who_is_online(ipc, 0);
				break;

			case 79:
				who_is_online(ipc, 1);
				break;

			case 91:
				who_is_online(ipc, 2);
				break;
                
			case 80:
				do_system_configuration(ipc);
				break;

			case 82:
				do_internet_configuration(ipc);
				break;

			case 84:
				quiet_mode(ipc);
				break;

			case 93:
				stealth_mode(ipc);
				break;

			case 50:
				enter_config(ipc, 2);
				break;

			case 37:
				enter_config(ipc, 0);
				set_floor_mode(ipc);
				break;

			case 59:
				enter_config(ipc, 3);
				set_floor_mode(ipc);
				break;

			case 60:
				gotofloor(ipc, argbuf, GF_GOTO);
				break;

			case 61:
				gotofloor(ipc, argbuf, GF_SKIP);
				break;

			case 62:
				forget_this_floor(ipc);
				break;

			case 63:
				create_floor(ipc);
				break;

			case 64:
				edit_floor(ipc);
				break;

			case 65:
				kill_floor(ipc);
				break;

			case 66:
				enter_bio(ipc);
				break;

			case 67:
				read_bio(ipc);
				break;

			case 25:
				edituser(ipc, 25);
				break;

			case 96:
				edituser(ipc, 96);
				break;

			case 8:
				knrooms(ipc, floor_mode);
				scr_printf("\n");
				break;

			case 68:
				knrooms(ipc, 2);
				scr_printf("\n");
				break;

			case 69:
				misc_server_cmd(ipc, argbuf);
				break;

			case 70:
				edit_system_message(ipc, argbuf);
				break;

			case 19:
				listzrooms(ipc);
				scr_printf("\n");
				break;

			case 51:
				deletefile(ipc);
				break;

			case 54:
				movefile(ipc);
				break;

			case 56:
				page_user(ipc);
				break;

            case 110:           /* <+> Next room */
   				 gotoroomstep(ipc, 1, 0);
			     break;

            case 111:           /* <-> Previous room */
                 gotoroomstep(ipc, 0, 0);
			     break;

			case 112:           /* <>> Next floor */
                 gotofloorstep(ipc, 1, GF_GOTO);
			     break;

			case 113:           /* <<> Previous floor */
                 gotofloorstep(ipc, 0, GF_GOTO);
				 break;

            case 116:           /* <.> skip to <+> Next room */
                 gotoroomstep(ipc, 1, 1);
			     break;

            case 117:           /* <.> skip to <-> Previous room */
                 gotoroomstep(ipc, 0, 1);
			     break;

			case 118:           /* <.> skip to <>> Next floor */
                 gotofloorstep(ipc, 1, GF_SKIP);
			     break;

			case 119:           /* <.> skip to <<> Previous floor */
                 gotofloorstep(ipc, 0, GF_SKIP);
				 break;

			case 114:           
                 read_config(ipc);
				 break;

			case 115:           
                 system_info(ipc);
				 break;

			case 120:           /* .KAnonymous */
    			 dotknown(ipc, 0, NULL);
				 break;

			case 121:           /* .KDirectory */
    			 dotknown(ipc, 1, NULL);
				 break;

			case 122:           /* .KMatch */
    			 dotknown(ipc, 2, argbuf);
				 break;

			case 123:           /* .KpreferredOnly */
    			 dotknown(ipc, 3, NULL);
				 break;

			case 124:           /* .KPrivate */
    			 dotknown(ipc, 4, NULL);
				 break;

			case 125:           /* .KRead only */
    			 dotknown(ipc, 5, NULL);
				 break;

			case 126:           /* .KShared */
    			 dotknown(ipc, 6, NULL);
				 break;

			case 127:           /* Configure POP3 aggregation */
				do_pop3client_configuration(ipc);
				break;

			case 128:           /* Configure XML/RSS feed retrieval */
				do_rssclient_configuration(ipc);
				break;

			default:
				break;
			}	/* end switch */
	} while (termn8 == 0);

TERMN8:	scr_printf("%s logged out.", fullname);
	termn8 = 0;
	color(ORIGINAL_PAIR);
	scr_printf("\n");
	while (marchptr != NULL) {
		remove_march(marchptr->march_name, 0);
	}
	if (mcmd == 30) {
		scr_printf("\n\nType 'off' to disconnect, or next user...\n");
	}
	CtdlIPCLogout(ipc);
	if ((mcmd == 29) || (mcmd == 15)) {
		stty_ctdl(SB_RESTORE);
		formout(ipc, "goodbye");
		logoff(ipc, 0);
	}
	/* Free the ungoto list */
	for (lp = 0; lp < uglistsize; lp++) {
		free(uglist[lp]);
	}
    uglistsize = 0;
	goto GSTA;

}	/* end main() */

