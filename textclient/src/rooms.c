/*
 * Client-side functions which perform room operations
 *
 * Copyright (c) 1987-2012 by the citadel.org team
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
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <libcitadel.h>
//#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "rooms.h"
#include "commands.h"
#include "messages.h"
#include "tuiconfig.h"
//#ifndef HAVE_SNPRINTF
//#include "snprintf.h"
//#endif
#include "screen.h"
//#include "citadel_dirs.h"

#define IFNEXPERT if ((userflags&US_EXPERT)==0)


void stty_ctdl(int cmd);
void dotgoto(CtdlIPC *ipc, char *towhere, int display_name, int fromungoto);
void progress(CtdlIPC* ipc, unsigned long curr, unsigned long cmax);
int pattern(char *search, char *patn);
int file_checksum(char *filename);
int nukedir(char *dirname);

extern unsigned room_flags;
extern char room_name[];
extern char temp[];
extern char tempdir[];
extern int editor_pid;
extern int screenwidth;
extern int screenheight;
extern char fullname[];
extern char sigcaught;
extern char floor_mode;
extern char curr_floor;


extern int ugnum;
extern long uglsn;
extern char *uglist[];
extern long uglistlsn[];
extern int uglistsize;

extern char floorlist[128][SIZ];


void load_floorlist(CtdlIPC *ipc)
{
	int a;
	char buf[SIZ];
	char *listing = NULL;
	int r;			/* IPC response code */

	for (a = 0; a < 128; ++a)
		floorlist[a][0] = 0;

	r = CtdlIPCFloorListing(ipc, &listing, buf);
	if (r / 100 != 1) {
		strcpy(floorlist[0], "Main Floor");
		return;
	}
	while (*listing && !IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');
		extract_token(floorlist[extract_int(buf, 0)], buf, 1, '|', SIZ);
	}
	free(listing);
}


void room_tree_list(struct ctdlroomlisting *rp)
{
	static int c = 0;
	char rmname[ROOMNAMELEN];
	int f;

	if (rp == NULL) {
		c = 1;
		return;
	}

	if (rp->lnext != NULL) {
		room_tree_list(rp->lnext);
	}

	if (sigcaught == 0) {
		strcpy(rmname, rp->rlname);
		f = rp->rlflags;
		if ((c + strlen(rmname) + 4) > screenwidth) {

			/* line break, check the paginator */
			scr_printf("\n");
			c = 1;
		}
		if (f & QR_MAILBOX) {
			color(BRIGHT_YELLOW);
		} else if (f & QR_PRIVATE) {
			color(BRIGHT_RED);
		} else {
			color(DIM_WHITE);
		}
		scr_printf("%s", rmname);
		if ((f & QR_DIRECTORY) && (f & QR_NETWORK))
			scr_printf("}  ");
		else if (f & QR_DIRECTORY)
			scr_printf("]  ");
		else if (f & QR_NETWORK)
			scr_printf(")  ");
		else
			scr_printf(">  ");
		c = c + strlen(rmname) + 3;
	}

	if (rp->rnext != NULL) {
		room_tree_list(rp->rnext);
	}

	free(rp);
}


/* 
 * Room ordering stuff (compare first by floor, then by order)
 */
int rordercmp(struct ctdlroomlisting *r1, struct ctdlroomlisting *r2)
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
static void listrms(struct march *listing, int new_only, int floor_only, unsigned int flags, char *match)
{
	struct march *mptr;
	struct ctdlroomlisting *rl = NULL;
	struct ctdlroomlisting *rp;
	struct ctdlroomlisting *rs;
	int list_it;

	for (mptr = listing; mptr != NULL; mptr = mptr->next) {
		list_it = 1;

		if ( (new_only == LISTRMS_NEW_ONLY)
		   && ((mptr->march_access & UA_HASNEWMSGS) == 0)) 
			list_it = 0;

		if ( (new_only == LISTRMS_OLD_ONLY)
		   && ((mptr->march_access & UA_HASNEWMSGS) != 0)) 
			list_it = 0;

		if ( (floor_only >= 0)
		   && (mptr->march_floor != floor_only))
			list_it = 0;

		if (flags && (mptr->march_flags & flags) == 0)
		    list_it = 0;

	    if (match && (pattern(mptr->march_name, match) == -1))
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

	room_tree_list(NULL);
	room_tree_list(rl);
	color(DIM_WHITE);
}


void list_other_floors(void)
{
	int a, c;

	c = 1;
	for (a = 0; a < 128; ++a) {
		if ((strlen(floorlist[a]) > 0) && (a != curr_floor)) {
			if ((c + strlen(floorlist[a]) + 4) > screenwidth) {
				scr_printf("\n");
				c = 1;
			}
			scr_printf("%s:  ", floorlist[a]);
			c = c + strlen(floorlist[a]) + 3;
		}
	}
}


/*
 * List known rooms.  kn_floor_mode should be set to 0 for a 'flat' listing,
 * 1 to list rooms on the current floor, or 2 to list rooms on all floors.
 */
void knrooms(CtdlIPC *ipc, int kn_floor_mode)
{
	int a;
	struct march *listing = NULL;
	struct march *mptr;
	int r;		/* IPC response code */
	char buf[SIZ];


	/* Ask the server for a room list */
	r = CtdlIPCKnownRooms(ipc, SubscribedRooms, (-1), &listing, buf);
	if (r / 100 != 1) {
		listing = NULL;
	}

	load_floorlist(ipc);


	if (kn_floor_mode == 0) {
		color(BRIGHT_CYAN);
		scr_printf("\n   Rooms with unread messages:\n");
		listrms(listing, LISTRMS_NEW_ONLY, -1, 0, NULL);
		color(BRIGHT_CYAN);
		scr_printf("\n\n   No unseen messages in:\n");
		listrms(listing, LISTRMS_OLD_ONLY, -1, 0, NULL);
		scr_printf("\n");
	}

	if (kn_floor_mode == 1) {
		color(BRIGHT_CYAN);
		scr_printf("\n   Rooms with unread messages on %s:\n",
			floorlist[(int) curr_floor]);
		listrms(listing, LISTRMS_NEW_ONLY, curr_floor, 0, NULL);
		color(BRIGHT_CYAN);
		scr_printf("\n\n   Rooms with no new messages on %s:\n",
			floorlist[(int) curr_floor]);
		listrms(listing, LISTRMS_OLD_ONLY, curr_floor, 0, NULL);
		color(BRIGHT_CYAN);
		scr_printf("\n\n   Other floors:\n");
		list_other_floors();
		scr_printf("\n");
	}

	if (kn_floor_mode == 2) {
		for (a = 0; a < 128; ++a) {
			if (floorlist[a][0] != 0) {
				color(BRIGHT_CYAN);
				scr_printf("\n   Rooms on %s:\n",
					floorlist[a]);
				listrms(listing, LISTRMS_ALL, a, 0, NULL);
				scr_printf("\n");
			}
		}
	}

	/* Free the room list */
	while (listing) {
		mptr = listing->next;
		free(listing);
		listing = mptr;
	};

	color(DIM_WHITE);
}


void listzrooms(CtdlIPC *ipc)
{				/* list public forgotten rooms */
	struct march *listing = NULL;
	struct march *mptr;
	int r;		/* IPC response code */
	char buf[SIZ];


	/* Ask the server for a room list */
	r = CtdlIPCKnownRooms(ipc, UnsubscribedRooms, (-1), &listing, buf);
	if (r / 100 != 1) {
		listing = NULL;
	}

	color(BRIGHT_CYAN);
	scr_printf("\n   Forgotten public rooms:\n");
	listrms(listing, LISTRMS_ALL, -1, 0, NULL);
	scr_printf("\n");

	/* Free the room list */
	while (listing) {
		mptr = listing->next;
		free(listing);
		listing = mptr;
	};

	color(DIM_WHITE);
}

void dotknown(CtdlIPC *ipc, int what, char *match)
{				/* list rooms according to attribute */
	struct march *listing = NULL;
	struct march *mptr;
	int r;		/* IPC response code */
	char buf[SIZ];

	/* Ask the server for a room list */
	r = CtdlIPCKnownRooms(ipc, AllAccessibleRooms, (-1), &listing, buf);
	if (r / 100 != 1) {
		listing = NULL;
	}

	color(BRIGHT_CYAN);

	switch (what) {
    case 0:
     	scr_printf("\n   Anonymous rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_ANONONLY|QR_ANONOPT, NULL);
    	scr_printf("\n");
		break;
    case 1:
     	scr_printf("\n   Directory rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_DIRECTORY, NULL);
    	scr_printf("\n");
		break;
    case 2:
     	scr_printf("\n   Matching \"%s\" rooms:\n", match);
	    listrms(listing, LISTRMS_ALL, -1, 0, match);
    	scr_printf("\n");
		break;
    case 3:
     	scr_printf("\n   Preferred only rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_PREFONLY, NULL);
    	scr_printf("\n");
		break;
    case 4:
     	scr_printf("\n   Private rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_PRIVATE, NULL);
    	scr_printf("\n");
		break;
    case 5:
     	scr_printf("\n   Read only rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_READONLY, NULL);
    	scr_printf("\n");
		break;
    case 6:
     	scr_printf("\n   Shared rooms:\n");
	    listrms(listing, LISTRMS_ALL, -1, QR_NETWORK, NULL);
    	scr_printf("\n");
		break;
	}

	/* Free the room list */
	while (listing) {
		mptr = listing->next;
		free(listing);
		listing = mptr;
	};

	color(DIM_WHITE);
}


int set_room_attr(CtdlIPC *ipc, unsigned int ibuf, char *prompt, unsigned int sbit)
{
	int a;

	a = boolprompt(prompt, (ibuf & sbit));
	ibuf = (ibuf | sbit);
	if (!a) {
		ibuf = (ibuf ^ sbit);
	}
	return (ibuf);
}



/*
 * Select a floor (used in several commands)
 * The supplied argument is the 'default' floor number.
 * This function returns the selected floor number.
 */
int select_floor(CtdlIPC *ipc, int rfloor)
{
	int a, newfloor;
	char floorstr[SIZ];

	if (floor_mode == 1) {
		if (floorlist[(int) curr_floor][0] == 0) {
			load_floorlist(ipc);
		}

		do {
			newfloor = (-1);
			safestrncpy(floorstr, floorlist[rfloor],
				    sizeof floorstr);
			strprompt("Which floor", floorstr, 255);
			for (a = 0; a < 128; ++a) {
				if (!strcasecmp
				    (floorstr, &floorlist[a][0]))
					newfloor = a;
				if ((newfloor < 0)
				    &&
				    (!strncasecmp
				     (floorstr, &floorlist[a][0],
				      strlen(floorstr))))
					newfloor = a;
				if ((newfloor < 0)
				    && (pattern(&floorlist[a][0], floorstr)
					>= 0))
					newfloor = a;
			}
			if (newfloor < 0) {
				scr_printf("\n One of:\n");
				for (a = 0; a < 128; ++a) {
					if (floorlist[a][0] != 0) {
						scr_printf("%s\n",
						       &floorlist[a][0]);
					}
				}
			}
		} while (newfloor < 0);
		return (newfloor);
	}

	else {
		scr_printf("Floor selection bypassed because you have "
			"floor mode disabled.\n");
	}

	return (rfloor);
}




/*
 * .<A>ide <E>dit room
 */
void editthisroom(CtdlIPC *ipc)
{
	int rbump = 0;
	char raide[USERNAME_SIZE];
	char buf[SIZ];
	struct ctdlroom *attr = NULL;
	struct ExpirePolicy *eptr = NULL;
	int r;				/* IPC response code */

	/* Fetch the existing room config */
	r = CtdlIPCGetRoomAttributes(ipc, &attr, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}
	eptr = &(attr->QRep);

	/* Fetch the name of the current room aide */
	r = CtdlIPCGetRoomAide(ipc, buf);
	if (r / 100 == 2) {
		safestrncpy(raide, buf, sizeof raide);
	} else {
		strcpy(raide, "");
	}
	if (IsEmptyStr(raide)) {
		strcpy(raide, "none");
	}

	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	r = CtdlIPCGetMessageExpirationPolicy(ipc, 0, &eptr, buf);

	/* Now interact with the user. */

	strprompt("Room name", attr->QRname, ROOMNAMELEN-1);
	attr->QRfloor = select_floor(ipc, attr->QRfloor);
	attr->QRflags = set_room_attr(ipc, attr->QRflags, "Private room", QR_PRIVATE);
	if (attr->QRflags & QR_PRIVATE) {
		attr->QRflags = set_room_attr(ipc, attr->QRflags,
				       "Hidden room (accessible to anyone who knows the room name)",
				       QR_GUESSNAME);
	}

	/* if it's public, clear the privacy classes */
	if ((attr->QRflags & QR_PRIVATE) == 0) {
		if (attr->QRflags & QR_GUESSNAME) {
			attr->QRflags = attr->QRflags - QR_GUESSNAME;
		}
		if (attr->QRflags & QR_PASSWORDED) {
			attr->QRflags = attr->QRflags - QR_PASSWORDED;
		}
	}

	/* if it's private, choose the privacy classes */
	if ((attr->QRflags & QR_PRIVATE)
	    && ((attr->QRflags & QR_GUESSNAME) == 0)) {
		attr->QRflags = set_room_attr(ipc, attr->QRflags,
				       "Accessible by entering a password",
				       QR_PASSWORDED);
	}
	if ((attr->QRflags & QR_PRIVATE)
	    && ((attr->QRflags & QR_PASSWORDED) == QR_PASSWORDED)) {
		strprompt("Room password", attr->QRpasswd, 9);
	}

	if ((attr->QRflags & QR_PRIVATE) == QR_PRIVATE) {
		rbump = boolprompt("Cause current users to forget room", 0);
	}

	attr->QRflags = set_room_attr(ipc, attr->QRflags,
					"Preferred users only", QR_PREFONLY);
	attr->QRflags = set_room_attr(ipc, attr->QRflags,
					"Read-only room", QR_READONLY);
	attr->QRflags2 = set_room_attr(ipc, attr->QRflags2,
				"Allow message deletion by anyone who can post",
				QR2_COLLABDEL);
	attr->QRflags = set_room_attr(ipc, attr->QRflags,
					"Permanent room", QR_PERMANENT);
	attr->QRflags2 = set_room_attr(ipc, attr->QRflags2,
								   "Subject Required (Force "
								   "users to specify a message "
                                   "subject)", QR2_SUBJECTREQ);
	attr->QRflags = set_room_attr(ipc, attr->QRflags,
					"Directory room", QR_DIRECTORY);
	if (attr->QRflags & QR_DIRECTORY) {
		strprompt("Directory name", attr->QRdirname, 14);
		attr->QRflags =
		    set_room_attr(ipc, attr->QRflags,
						"Uploading allowed", QR_UPLOAD);
		attr->QRflags =
		    set_room_attr(ipc, attr->QRflags, "Downloading allowed",
				  QR_DOWNLOAD);
		attr->QRflags =
		    set_room_attr(ipc, attr->QRflags,
						"Visible directory", QR_VISDIR);
	}
	attr->QRflags = set_room_attr(ipc, attr->QRflags,
					"Network shared room", QR_NETWORK);
	attr->QRflags2 = set_room_attr(ipc, attr->QRflags2,
				"Self-service list subscribe/unsubscribe",
				QR2_SELFLIST);
	attr->QRflags2 = set_room_attr(ipc, attr->QRflags2,
				"public posting to this room via room_roomname@yourcitadel.org",
				QR2_SMTP_PUBLIC);
	attr->QRflags2 = set_room_attr(ipc, attr->QRflags2,
				"moderated mailinglist",
				QR2_MODERATED);
	attr->QRflags = set_room_attr(ipc, attr->QRflags,
			       "Automatically make all messages anonymous",
			       QR_ANONONLY);
	if ((attr->QRflags & QR_ANONONLY) == 0) {
		attr->QRflags = set_room_attr(ipc, attr->QRflags,
				       "Ask users whether to make messages anonymous",
				       QR_ANONOPT);
	}
	attr->QRorder = intprompt("Listing order", attr->QRorder, 0, 127);

	/* Ask about the room aide */
	do {
		strprompt("Room aide (or 'none')", raide, 29);
		if (!strcasecmp(raide, "none")) {
			strcpy(raide, "");
			break;
		} else {
			r = CtdlIPCQueryUsername(ipc, raide, buf);
			if (r / 100 != 2)
				scr_printf("%s\n", buf);
		}
	} while (r / 100 != 2);

	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", attr->QRep.expire_mode);
		strprompt("Message expire policy (? for list)", buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"0. Use the default for this floor\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < 48) || (buf[0] > 51));
	attr->QRep.expire_mode = buf[0] - 48;

	/* ...lunatics and monsters underneath my bed */
	if (attr->QRep.expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", attr->QRep.expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		attr->QRep.expire_value = atol(buf);
	}

	if (attr->QRep.expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", attr->QRep.expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		attr->QRep.expire_value = atol(buf);
	}

	/* Give 'em a chance to change their minds */
	scr_printf("Save changes (y/n)? ");

	if (yesno() == 1) {
		r = CtdlIPCSetRoomAide(ipc, raide, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
		}

		r = CtdlIPCSetMessageExpirationPolicy(ipc, 0, eptr, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
		}

		r = CtdlIPCSetRoomAttributes(ipc, rbump, attr, buf);
		scr_printf("%s\n", buf);
		strncpy(buf, attr->QRname, ROOMNAMELEN);
		free(attr);
		if (r / 100 == 2)
			dotgoto(ipc, buf, 2, 0);
	}
	else free(attr);
}


/*
 * un-goto the previous room, or a specified room
 */
void dotungoto(CtdlIPC *ipc, char *towhere)
  {
    /* Find this 'towhere' room in the list ungoto from this room to
       that at the messagepointer position in that room in our ungoto list.
       I suppose I could be a real dick and just ungoto that many places
       in our list. */
    int found = -1;
    int lp;
	char buf[SIZ];
	struct ctdlipcroom *rret = NULL;	/* ignored */
	int r;

	if (uglistsize == 0)
      {
		scr_printf("No rooms to ungoto.\n");
		return;
      }
	if (towhere == NULL)
      {
		scr_printf("Must specify a room to ungoto.\n");
		return;
      }
	if (IsEmptyStr(towhere))
      {
		scr_printf("Must specify a room to ungoto.\n");
		return;
      }
    for (lp = uglistsize-1; lp >= 0; lp--)
      {
        if (strcasecmp(towhere, uglist[lp]) == 0)
          {
            found = lp;
            break;
          }
      }
    if (found == -1)
      {
		scr_printf("Room: %s not in ungoto list.\n", towhere);
    	return;
      }

	r = CtdlIPCGotoRoom(ipc, uglist[found], "", &rret, buf);
	if (rret) free(rret);	/* ignored */
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}
	r = CtdlIPCSetLastRead(ipc, uglistlsn[found] ? uglistlsn[found] : 1, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
	}
	safestrncpy(buf, uglist[found], sizeof(buf));
    /* we queue ungoto information here, because we're not really
       ungotoing, we're really going to a random spot in some arbitrary
       room list. */
	dotgoto(ipc, buf, 0, 0);
  }

void ungoto(CtdlIPC *ipc)
{
	char buf[SIZ];
	struct ctdlipcroom *rret = NULL;	/* ignored */
	int r;

	if (uglistsize == 0)
		return;

	r = CtdlIPCGotoRoom(ipc, uglist[uglistsize-1], "", &rret, buf);
	if (rret) free(rret);	/* ignored */
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}
	r = CtdlIPCSetLastRead(ipc, uglistlsn[uglistsize-1] ? uglistlsn[uglistsize-1] : 1, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
	}
	safestrncpy(buf, uglist[uglistsize-1], sizeof(buf));
	uglistsize--;
	free(uglist[uglistsize]);
	/* Don't queue ungoto info or we end up in a loop */
	dotgoto(ipc, buf, 0, 1);
}


/*
 * saves filelen bytes from file at pathname
 */
int save_buffer(void *file, size_t filelen, const char *pathname)
{
	size_t block = 0;
	size_t bytes_written = 0;
	FILE *fp;

	fp = fopen(pathname, "w");
	if (!fp) {
		scr_printf("Cannot open '%s': %s\n", pathname, strerror(errno));
		return 0;
	}
	do {
		block = fwrite((char *)file + bytes_written, 1,
				filelen - bytes_written, fp);
		bytes_written += block;
	} while (errno == EINTR && bytes_written < filelen);
	fclose(fp);

	if (bytes_written < filelen) {
		scr_printf("Trouble saving '%s': %s\n", pathname,
				strerror(errno));
		return 0;
	}
	return 1;
}


/*
 * Save supplied_filename in dest directory; gets the name only
 */
void destination_directory(char *dest, const char *supplied_filename)
{
	static char save_dir[SIZ] = { 0 };

	if (IsEmptyStr(save_dir)) {
		if (getenv("HOME") == NULL) {
			strcpy(save_dir, ".");
		}
		else {
			sprintf(save_dir, "%s/Desktop", getenv("HOME"));
			if (access(save_dir, W_OK) != 0) {
				sprintf(save_dir, "%s", getenv("HOME"));
				if (access(save_dir, W_OK) != 0) {
					sprintf(save_dir, ".");
				}
			}
		}
	}

	sprintf(dest, "%s/%s", save_dir, supplied_filename);
	strprompt("Save as", dest, PATH_MAX);

	/* Remember the directory for next time */
	strcpy(save_dir, dest);
	if (strrchr(save_dir, '/') != NULL) {
		strcpy(strrchr(save_dir, '/'), "");
	}
	else {
		strcpy(save_dir, ".");
	}
}


/*
 * download()  -  download a file or files.  The argument passed to this
 *                function determines which protocol to use.
 *  proto - 0 = paginate, 1 = xmodem, 2 = raw, 3 = ymodem, 4 = zmodem, 5 = save
 */
void download(CtdlIPC *ipc, int proto)
{
	char buf[SIZ];
	char filename[PATH_MAX];
	char tempname[PATH_MAX];
	char transmit_cmd[SIZ];
	FILE *tpipe = NULL;
/*	int broken = 0;*/
	int r;
	int rv = 0;
	void *file = NULL;	/* The downloaded file */
	size_t filelen = 0L;	/* The downloaded file length */

	if ((room_flags & QR_DOWNLOAD) == 0) {
		scr_printf("*** You cannot download from this room.\n");
		return;
	}

	newprompt("Enter filename: ", filename, PATH_MAX);

	/* Save to local disk, for folks with their own copy of the client */
	if (proto == 5) {
		destination_directory(tempname, filename);
		r = CtdlIPCFileDownload(ipc, filename, &file, 0, progress, buf);
		if (r / 100 != 2) {
			scr_printf("%s\n", buf);
			return;
		}
		save_buffer(file, (size_t)extract_long(buf, 0), tempname);
		free(file);
		return;
	}

	r = CtdlIPCFileDownload(ipc, filename, &file, 0, progress, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}
	filelen = extract_unsigned_long(buf, 0);

	/* Meta-download for public clients */
	/* scr_printf("Fetching file from Citadel server...\n"); */
	mkdir(tempdir, 0700);
	snprintf(tempname, sizeof tempname, "%s/%s", tempdir, filename);
	tpipe = fopen(tempname, "wb");
	if (fwrite(file, filelen, 1, tpipe) < filelen) {
		/* FIXME: restart syscall on EINTR 
		   broken = 1;*/
	}
	fclose(tpipe);
	if (file) free(file);

	if (proto == 0) {
		/* FIXME: display internally instead */
		snprintf(transmit_cmd, sizeof transmit_cmd,
			"SHELL=/dev/null; export SHELL; TERM=dumb; export TERM; exec more -d <%s",
			tempname);
	}
	else if (proto == 1)
		snprintf(transmit_cmd, sizeof transmit_cmd, "exec sx %s", tempname);
	else if (proto == 3)
		snprintf(transmit_cmd, sizeof transmit_cmd, "exec sb %s", tempname);
	else if (proto == 4)
		snprintf(transmit_cmd, sizeof transmit_cmd, "exec sz %s", tempname);
	else
		/* FIXME: display internally instead */
		snprintf(transmit_cmd, sizeof transmit_cmd, "exec cat %s", tempname);

	stty_ctdl(SB_RESTORE);
	rv = system(transmit_cmd);
	if (rv != 0)
		scr_printf("failed to download '%s': %d\n", transmit_cmd, rv);
	stty_ctdl(SB_NO_INTR);

	/* clean up the temporary directory */
	nukedir(tempdir);
	ctdl_beep();	/* Beep beep! */
}


/*
 * read directory of this room
 */
void roomdir(CtdlIPC *ipc)
{
	char flnm[256];
	char flsz[32];
	char comment[256];
	char mimetype[256];
	char buf[256];
	char *listing = NULL;	/* Returned directory listing */
	int r;

	r = CtdlIPCReadDirectory(ipc, &listing, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}

	extract_token(comment, buf, 0, '|', sizeof comment);
	extract_token(flnm, buf, 1, '|', sizeof flnm);
	scr_printf("\nDirectory of %s on %s\n", flnm, comment);
	scr_printf("-----------------------\n");
	while (listing && *listing && !IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');

		extract_token(flnm, buf, 0, '|', sizeof flnm);
		extract_token(flsz, buf, 1, '|', sizeof flsz);
		extract_token(mimetype, buf, 2, '|', sizeof mimetype);
		extract_token(comment, buf, 3, '|', sizeof comment);
		if (strlen(flnm) <= 14)
			scr_printf("%-14s %8s %s [%s]\n", flnm, flsz, comment, mimetype);
		else
			scr_printf("%s\n%14s %8s %s [%s]\n", flnm, "", flsz,
				comment, mimetype);
	}
	if (listing) free(listing);
}


/*
 * add a user to a private room
 */
void invite(CtdlIPC *ipc)
{
	char username[USERNAME_SIZE];
	char buf[SIZ];

	newprompt("Name of user? ", username, USERNAME_SIZE);
	if (username[0] == 0)
		return;

	CtdlIPCInviteUserToRoom(ipc, username, buf);
	scr_printf("%s\n", buf);
}


/*
 * kick a user out of a room
 */
void kickout(CtdlIPC *ipc)
{
	char username[USERNAME_SIZE];
	char buf[SIZ];

	newprompt("Name of user? ", username, USERNAME_SIZE);
	if (username[0] == 0)
		return;

	CtdlIPCKickoutUserFromRoom(ipc, username, buf);
	scr_printf("%s\n", buf);
}


/*
 * aide command: kill the current room
 */
void killroom(CtdlIPC *ipc)
{
	char aaa[100];
	int r;

	r = CtdlIPCDeleteRoom(ipc, 0, aaa);
	if (r / 100 != 2) {
		scr_printf("%s\n", aaa);
		return;
	}

	scr_printf("Are you sure you want to kill this room? ");
	if (yesno() == 0)
		return;

	r = CtdlIPCDeleteRoom(ipc, 1, aaa);
	scr_printf("%s\n", aaa);
	if (r / 100 != 2)
		return;
	dotgoto(ipc, "_BASEROOM_", 0, 0);
}

void forget(CtdlIPC *ipc)
{				/* forget the current room */
	char buf[SIZ];

	scr_printf("Are you sure you want to forget this room? ");
	if (yesno() == 0)
		return;

	remove_march(room_name, 0);
	if (CtdlIPCForgetRoom(ipc, buf) / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}

	/* now return to the lobby */
	dotgoto(ipc, "_BASEROOM_", 0, 0);
}


/*
 * create a new room
 */
void entroom(CtdlIPC *ipc)
{
	char buf[SIZ];
	char new_room_name[ROOMNAMELEN];
	int new_room_type;
	char new_room_pass[10];
	int new_room_floor;
	int a, b;
	int r;				/* IPC response code */

	/* Check permission to create room */
	r = CtdlIPCCreateRoom(ipc, 0, "", 1, "", 0, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}

	newprompt("Name for new room? ", new_room_name, ROOMNAMELEN - 1);
	if (IsEmptyStr(new_room_name)) {
		return;
	}
	for (a = 0; !IsEmptyStr(&new_room_name[a]); ++a) {
		if (new_room_name[a] == '|') {
			new_room_name[a] = '_';
		}
	}

	new_room_floor = select_floor(ipc, (int) curr_floor);

	IFNEXPERT formout(ipc, "roomaccess");
	do {
		scr_printf("<?>Help\n"
			"<1>Public room (shown to all users by default)\n"
			"<2>Hidden room (accessible to anyone who knows the room name)\n"
			"<3>Passworded room (hidden, plus requires a password to enter)\n"
			"<4>Invitation-only room (requires access to be granted by an Aide)\n"
		       	"<5>Personal room (accessible to you only)\n"
			"Enter room type: "
		);
		do {
			b = inkey();
		} while (((b < '1') || (b > '5')) && (b != '?'));
		if (b == '?') {
			scr_printf("?\n");
			formout(ipc, "roomaccess");
		}
	} while ((b < '1') || (b > '5'));
	b -= '0';			/* Portable */
	scr_printf("%d\n", b);
	new_room_type = b - 1;
	if (new_room_type == 2) {
		newprompt("Enter a room password: ", new_room_pass, 9);
		for (a = 0; !IsEmptyStr(&new_room_pass[a]); ++a)
			if (new_room_pass[a] == '|')
				new_room_pass[a] = '_';
	} else {
		strcpy(new_room_pass, "");
	}

	scr_printf("\042%s\042, a", new_room_name);
	if (b == 1)
		scr_printf(" public room.");
	if (b == 2)
		scr_printf(" hidden room.");
	if (b == 3)
		scr_printf(" passworded room, password: %s", new_room_pass);
	if (b == 4)
		scr_printf("n invitation-only room.");
	if (b == 5)
		scr_printf(" personal room.");
	scr_printf("\nInstall it? (y/n) : ");
	if (yesno() == 0) {
		return;
	}

	r = CtdlIPCCreateRoom(ipc, 1, new_room_name, new_room_type,
			      new_room_pass, new_room_floor, buf);
	if (r / 100 != 2) {
		scr_printf("%s\n", buf);
		return;
	}

	/* command succeeded... now GO to the new room! */
	dotgoto(ipc, new_room_name, 0, 0);
}



void readinfo(CtdlIPC *ipc)
{				/* read info file for current room */
	char buf[SIZ];
	char raide[64];
	int r;			/* IPC response code */
	char *text = NULL;

	/* Name of currernt room aide */
	r = CtdlIPCGetRoomAide(ipc, buf);
	if (r / 100 == 2)
		safestrncpy(raide, buf, sizeof raide);
	else
		strcpy(raide, "");

	if (!IsEmptyStr(raide))
		scr_printf("Room aide is %s.\n\n", raide);

	r = CtdlIPCRoomInfo(ipc, &text, buf);
	if (r / 100 != 1)
		return;

	if (text) {
		fmout(screenwidth, NULL, text, NULL, 1);
		free(text);
	}
}


/*
 * <W>ho knows room...
 */
void whoknows(CtdlIPC *ipc)
{
	char buf[256];
	char *listing = NULL;
	int r;

	r = CtdlIPCWhoKnowsRoom(ipc, &listing, buf);
	if (r / 100 != 1) {
		scr_printf("%s\n", buf);
		return;
	}
	while (!IsEmptyStr(listing)) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');
		if (sigcaught == 0)
			scr_printf("%s\n", buf);
	}
	free(listing);
}


void do_edit(CtdlIPC *ipc,
		char *desc, char *read_cmd, char *check_cmd, char *write_cmd)
{
	FILE *fp;
	char cmd[SIZ];
	int b, cksum, editor_exit;

	if (IsEmptyStr(editor_path)) {
		scr_printf("Do you wish to re-enter %s? ", desc);
		if (yesno() == 0)
			return;
	}

	fp = fopen(temp, "w");
	fclose(fp);

	CtdlIPC_chat_send(ipc, check_cmd);
	CtdlIPC_chat_recv(ipc, cmd);
	if (cmd[0] != '2') {
		scr_printf("%s\n", &cmd[4]);
		return;
	}

	if (!IsEmptyStr(editor_path)) {
		CtdlIPC_chat_send(ipc, read_cmd);
		CtdlIPC_chat_recv(ipc, cmd);
		if (cmd[0] == '1') {
			fp = fopen(temp, "w");
			while (CtdlIPC_chat_recv(ipc, cmd), strcmp(cmd, "000")) {
				fprintf(fp, "%s\n", cmd);
			}
			fclose(fp);
		}
	}

	cksum = file_checksum(temp);

	if (!IsEmptyStr(editor_path)) {
		char tmp[SIZ];

		snprintf(tmp, sizeof tmp, "WINDOW_TITLE=%s", desc);
		putenv(tmp);
		stty_ctdl(SB_RESTORE);
		editor_pid = fork();
		if (editor_pid == 0) {
			chmod(temp, 0600);
			execlp(editor_path, editor_path, temp, NULL);
			exit(1);
		}
		if (editor_pid > 0)
			do {
				editor_exit = 0;
				b = ka_wait(&editor_exit);
			} while ((b != editor_pid) && (b >= 0));
		editor_pid = (-1);
		scr_printf("Executed %s\n", editor_path);
		stty_ctdl(0);
	} else {
		scr_printf("Entering %s.  Press return twice when finished.\n", desc);
		fp = fopen(temp, "r+");
		citedit(fp);
		fclose(fp);
	}

	if (file_checksum(temp) == cksum) {
		scr_printf("*** Aborted.\n");
	}

	else {
		CtdlIPC_chat_send(ipc, write_cmd);
		CtdlIPC_chat_recv(ipc, cmd);
		if (cmd[0] != '4') {
			scr_printf("%s\n", &cmd[4]);
			return;
		}

		fp = fopen(temp, "r");
		while (fgets(cmd, SIZ - 1, fp) != NULL) {
			cmd[strlen(cmd) - 1] = 0;
			CtdlIPC_chat_send(ipc, cmd);
		}
		fclose(fp);
		CtdlIPC_chat_send(ipc, "000");
	}

	unlink(temp);
}


void enterinfo(CtdlIPC *ipc)
{				/* edit info file for current room */
	do_edit(ipc, "the Info file for this room", "RINF", "EINF 0", "EINF 1");
}

void enter_bio(CtdlIPC *ipc)
{
	char cmd[SIZ];
	snprintf(cmd, sizeof cmd, "RBIO %s", fullname);
	do_edit(ipc, "your Bio", cmd, "NOOP", "EBIO");
}

/*
 * create a new floor
 */
void create_floor(CtdlIPC *ipc)
{
	char buf[SIZ];
	char newfloorname[SIZ];
	int r;			/* IPC response code */

	load_floorlist(ipc);

	r = CtdlIPCCreateFloor(ipc, 0, "", buf);
	if ( (r / 100 != 2) && (r != ERROR + ILLEGAL_VALUE) ) {
		scr_printf("%s\n", buf);
		return;
	}

	newprompt("Name for new floor: ", newfloorname, 255);
	if (!*newfloorname) return;
	r = CtdlIPCCreateFloor(ipc, 1, newfloorname, buf);
	if (r / 100 == 2) {
		scr_printf("Floor has been created.\n");
	} else {
		scr_printf("%s\n", buf);
	}

	load_floorlist(ipc);
}

/*
 * edit the current floor
 */
void edit_floor(CtdlIPC *ipc)
{
	char buf[SIZ];
	struct ExpirePolicy *ep = NULL;

	load_floorlist(ipc);

	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	CtdlIPCGetMessageExpirationPolicy(ipc, 1, &ep, buf);

	/* Interact with the user */
	scr_printf("You are editing the floor called \"%s\"\n", 
		&floorlist[(int) curr_floor][0] );
	strprompt("Floor name", &floorlist[(int) curr_floor][0], 255);

	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", ep->expire_mode);
		strprompt
		    ("Floor default message expire policy (? for list)",
		     buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"0. Use the system default\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < '0') || (buf[0] > '3'));
	ep->expire_mode = buf[0] - '0';

	/* ...lunatics and monsters underneath my bed */
	if (ep->expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", ep->expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		ep->expire_value = atol(buf);
	}

	if (ep->expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", ep->expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		ep->expire_value = atol(buf);
	}

	/* Save it */
	CtdlIPCSetMessageExpirationPolicy(ipc, 1, ep, buf);
	CtdlIPCEditFloor(ipc, curr_floor, &floorlist[(int)curr_floor][0], buf);
	scr_printf("%s\n", buf);
	load_floorlist(ipc);
}




/*
 * kill the current floor 
 */
void kill_floor(CtdlIPC *ipc)
{
	int floornum_to_delete, a;
	char buf[SIZ];

	load_floorlist(ipc);
	do {
		floornum_to_delete = (-1);
		scr_printf("(Press return to abort)\n");
		newprompt("Delete which floor? ", buf, 255);
		if (IsEmptyStr(buf))
			return;
		for (a = 0; a < 128; ++a)
			if (!strcasecmp(&floorlist[a][0], buf))
				floornum_to_delete = a;
		if (floornum_to_delete < 0) {
			scr_printf("No such floor.  Select one of:\n");
			for (a = 0; a < 128; ++a)
				if (floorlist[a][0] != 0)
					scr_printf("%s\n", &floorlist[a][0]);
		}
	} while (floornum_to_delete < 0);
	CtdlIPCDeleteFloor(ipc, 1, floornum_to_delete, buf);
	scr_printf("%s\n", buf);
	load_floorlist(ipc);
}
