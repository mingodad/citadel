/*
 * $Id$
 *
 * 
 * Client-side functions which perform room operations
 *
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
#include "citadel.h"
#include "citadel_ipc.h"
#include "rooms.h"
#include "commands.h"
#include "tools.h"
#include "messages.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "screen.h"

#define IFNEXPERT if ((userflags&US_EXPERT)==0)


void sttybbs(int cmd);
void hit_any_key(void);
int yesno(void);
void strprompt(char *prompt, char *str, int len);
void newprompt(char *prompt, char *str, int len);
void dotgoto(char *towhere, int display_name, int fromungoto);
void serv_read(char *buf, int bytes);
void formout(char *name);
int inkey(void);
void progress(long int curr, long int cmax);
int pattern(char *search, char *patn);
int file_checksum(char *filename);
int nukedir(char *dirname);

extern unsigned room_flags;
extern char room_name[];
extern char temp[];
extern char tempdir[];
extern int editor_pid;
extern char editor_path[];
extern int screenwidth;
extern int screenheight;
extern char fullname[];
extern int userflags;
extern char sigcaught;
extern char floor_mode;
extern char curr_floor;


extern int ugnum;
extern long uglsn;
extern char *uglist[];
extern long uglistlsn[];
extern int uglistsize;

extern char floorlist[128][SIZ];


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


void room_tree_list(struct roomlisting *rp)
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
			pprintf("\n");
			c = 1;
		}
		if (f & QR_MAILBOX) {
			color(BRIGHT_YELLOW);
		} else if (f & QR_PRIVATE) {
			color(BRIGHT_RED);
		} else {
			color(DIM_WHITE);
		}
		pprintf("%s", rmname);
		if ((f & QR_DIRECTORY) && (f & QR_NETWORK))
			pprintf("}  ");
		else if (f & QR_DIRECTORY)
			pprintf("]  ");
		else if (f & QR_NETWORK)
			pprintf(")  ");
		else
			pprintf(">  ");
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

	struct roomlisting *rl = NULL;
	struct roomlisting *rp;
	struct roomlisting *rs;


	/* Ask the server for a room list */
	serv_puts(variety);
	serv_gets(buf);
	if (buf[0] != '1') {
		return;
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
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
				pprintf("\n");
				c = 1;
			}
			pprintf("%s:  ", floorlist[a]);
			c = c + strlen(floorlist[a]) + 3;
		}
	}
}


/*
 * List known rooms.  kn_floor_mode should be set to 0 for a 'flat' listing,
 * 1 to list rooms on the current floor, or 1 to list rooms on all floors.
 */
void knrooms(int kn_floor_mode)
{
	char buf[SIZ];
	int a;

	load_floorlist();

	if (kn_floor_mode == 0) {
		color(BRIGHT_CYAN);
		pprintf("\n   Rooms with unread messages:\n");
		listrms("LKRN");
		color(BRIGHT_CYAN);
		pprintf("\n\n   No unseen messages in:\n");
		listrms("LKRO");
		pprintf("\n");
	}

	if (kn_floor_mode == 1) {
		color(BRIGHT_CYAN);
		pprintf("\n   Rooms with unread messages on %s:\n",
			floorlist[(int) curr_floor]);
		snprintf(buf, sizeof buf, "LKRN %d", curr_floor);
		listrms(buf);
		color(BRIGHT_CYAN);
		pprintf("\n\n   Rooms with no new messages on %s:\n",
			floorlist[(int) curr_floor]);
		snprintf(buf, sizeof buf, "LKRO %d", curr_floor);
		listrms(buf);
		color(BRIGHT_CYAN);
		pprintf("\n\n   Other floors:\n");
		list_other_floors();
		pprintf("\n");
	}

	if (kn_floor_mode == 2) {
		for (a = 0; a < 128; ++a) {
			if (floorlist[a][0] != 0) {
				color(BRIGHT_CYAN);
				pprintf("\n   Rooms on %s:\n",
					floorlist[a]);
				snprintf(buf, sizeof buf, "LKRA %d", a);
				listrms(buf);
				pprintf("\n");
			}
		}
	}

	color(DIM_WHITE);
	IFNEXPERT hit_any_key();
}


void listzrooms(void)
{				/* list public forgotten rooms */
	color(BRIGHT_CYAN);
	pprintf("\n   Forgotten public rooms:\n");
	listrms("LZRM");
	pprintf("\n");
	color(DIM_WHITE);
	IFNEXPERT hit_any_key();
}


int set_room_attr(int ibuf, char *prompt, unsigned int sbit)
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
int select_floor(int rfloor)
{
	int a, newfloor;
	char floorstr[SIZ];

	if (floor_mode == 1) {
		if (floorlist[(int) curr_floor][0] == 0) {
			load_floorlist();
		}

		do {
			newfloor = (-1);
			safestrncpy(floorstr, floorlist[rfloor],
				    sizeof floorstr);
			strprompt("Which floor", floorstr, SIZ);
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
	return (rfloor);
}




/*
 * .<A>ide <E>dit room
 */
void editthisroom(void)
{
	char rname[ROOMNAMELEN];
	char rpass[10];
	char rdir[15];
	unsigned rflags;
	int rbump;
	char raide[32];
	char buf[SIZ];
	int rfloor;
	int rorder;
	int expire_mode = 0;
	int expire_value = 0;

	/* Fetch the existing room config */
	serv_puts("GETR");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}

	extract(rname, &buf[4], 0);
	extract(rpass, &buf[4], 1);
	extract(rdir, &buf[4], 2);
	rflags = extract_int(&buf[4], 3);
	rfloor = extract_int(&buf[4], 4);
	rorder = extract_int(&buf[4], 5);
	rbump = 0;

	/* Fetch the name of the current room aide */
	serv_puts("GETA");
	serv_gets(buf);
	if (buf[0] == '2') {
		safestrncpy(raide, &buf[4], sizeof raide);
	}
	else {
		strcpy(raide, "");
	}
	if (strlen(raide) == 0) {
		strcpy(raide, "none");
	}

	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	serv_puts("GPEX room");
	serv_gets(buf);
	if (buf[0] == '2') {
		expire_mode = extract_int(&buf[4], 0);
		expire_value = extract_int(&buf[4], 1);
	}

	/* Now interact with the user. */
	strprompt("Room name", rname, ROOMNAMELEN - 1);

	rfloor = select_floor(rfloor);
	rflags = set_room_attr(rflags, "Private room", QR_PRIVATE);
	if (rflags & QR_PRIVATE) {
		rflags = set_room_attr(rflags,
				       "Accessible by guessing room name",
				       QR_GUESSNAME);
	}

	/* if it's public, clear the privacy classes */
	if ((rflags & QR_PRIVATE) == 0) {
		if (rflags & QR_GUESSNAME) {
			rflags = rflags - QR_GUESSNAME;
		}
		if (rflags & QR_PASSWORDED) {
			rflags = rflags - QR_PASSWORDED;
		}
	}

	/* if it's private, choose the privacy classes */
	if ((rflags & QR_PRIVATE)
	    && ((rflags & QR_GUESSNAME) == 0)) {
		rflags = set_room_attr(rflags,
				       "Accessible by entering a password",
				       QR_PASSWORDED);
	}
	if ((rflags & QR_PRIVATE)
	    && ((rflags & QR_PASSWORDED) == QR_PASSWORDED)) {
		strprompt("Room password", rpass, 9);
	}

	if ((rflags & QR_PRIVATE) == QR_PRIVATE) {
		rbump =
		    boolprompt("Cause current users to forget room", 0);
	}

	rflags =
	    set_room_attr(rflags, "Preferred users only", QR_PREFONLY);
	rflags = set_room_attr(rflags, "Read-only room", QR_READONLY);
	rflags = set_room_attr(rflags, "Directory room", QR_DIRECTORY);
	rflags = set_room_attr(rflags, "Permanent room", QR_PERMANENT);
	if (rflags & QR_DIRECTORY) {
		strprompt("Directory name", rdir, 14);
		rflags =
		    set_room_attr(rflags, "Uploading allowed", QR_UPLOAD);
		rflags =
		    set_room_attr(rflags, "Downloading allowed",
				  QR_DOWNLOAD);
		rflags =
		    set_room_attr(rflags, "Visible directory", QR_VISDIR);
	}
	rflags = set_room_attr(rflags, "Network shared room", QR_NETWORK);
	rflags = set_room_attr(rflags,
			       "Automatically make all messages anonymous",
			       QR_ANONONLY);
	if ((rflags & QR_ANONONLY) == 0) {
		rflags = set_room_attr(rflags,
				       "Ask users whether to make messages anonymous",
				       QR_ANONOPT);
	}
	rorder = intprompt("Listing order", rorder, 1, 127);

	/* Ask about the room aide */
	do {
		strprompt("Room aide (or 'none')", raide, 29);
		if (!strcasecmp(raide, "none")) {
			strcpy(raide, "");
			strcpy(buf, "200");
		} else {
			snprintf(buf, sizeof buf, "QUSR %s", raide);
			serv_puts(buf);
			serv_gets(buf);
			if (buf[0] != '2')
				scr_printf("%s\n", &buf[4]);
		}
	} while (buf[0] != '2');

	if (!strcasecmp(raide, "none")) {
		strcpy(raide, "");
	}


	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", expire_mode);
		strprompt("Message expire policy (? for list)", buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"0. Use the default for this floor\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < 48) || (buf[0] > 51));
	expire_mode = buf[0] - 48;

	/* ...lunatics and monsters underneath my bed */
	if (expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		expire_value = atol(buf);
	}

	if (expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		expire_value = atol(buf);
	}

	/* Give 'em a chance to change their minds */
	scr_printf("Save changes (y/n)? ");

	if (yesno() == 1) {
		snprintf(buf, sizeof buf, "SETA %s", raide);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			scr_printf("%s\n", &buf[4]);
		}

		snprintf(buf, sizeof buf, "SPEX room|%d|%d",
			 expire_mode, expire_value);
		serv_puts(buf);
		serv_gets(buf);

		snprintf(buf, sizeof buf, "SETR %s|%s|%s|%d|%d|%d|%d",
			 rname, rpass, rdir, rflags, rbump, rfloor,
			 rorder);
		serv_puts(buf);
		serv_gets(buf);
		scr_printf("%s\n", &buf[4]);
		if (buf[0] == '2')
			dotgoto(rname, 2, 0);
	}
}


/*
 * un-goto the previous room
 */
void ungoto(void)
{
	char buf[SIZ];
    if (uglistsize == 0)
      return;
	snprintf(buf, sizeof buf, "GOTO %s", uglist[uglistsize-1]); 
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	snprintf(buf, sizeof buf, "SLRP %ld", uglistlsn[uglistsize-1]); 
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
	}
    safestrncpy (buf, uglist[uglistsize-1], sizeof(buf));
    uglistsize--;
    free(uglist[uglistsize]);
	dotgoto(buf, 0, 1); /* Don't queue ungoto info or we end up in a loop */
}


/* Here's the code for simply transferring the file to the client,
 * for folks who have their own clientware.  It's a lot simpler than
 * the [XYZ]modem code below...
 * (This function assumes that a download file is already open on the server)
 */
void download_to_local_disk(char *supplied_filename, long total_bytes)
{
	char buf[SIZ];
	char dbuf[4096];
	long transmitted_bytes = 0L;
	long aa, bb;
	FILE *savefp;
	int broken = 0;
	int packet;
	char filename[SIZ];

	strcpy(filename, supplied_filename);
	if (strlen(filename) == 0) {
		newprompt("Filename: ", filename, 250);
	}

	scr_printf("Enter the name of the directory to save '%s'\n"
		"to, or press return for the current directory.\n", filename);
	newprompt("Directory: ", dbuf, sizeof dbuf);
	if (strlen(dbuf) == 0)
		strcpy(dbuf, ".");
	strcat(dbuf, "/");
	strcat(dbuf, filename);

	savefp = fopen(dbuf, "w");
	if (savefp == NULL) {
		scr_printf("Cannot open '%s': %s\n", dbuf, strerror(errno));
		/* close the download file at the server */
		serv_puts("CLOS");
		serv_gets(buf);
		if (buf[0] != '2') {
			scr_printf("%s\n", &buf[4]);
		}
		return;
	}
	progress(0, total_bytes);
	while ((transmitted_bytes < total_bytes) && (broken == 0)) {
		bb = total_bytes - transmitted_bytes;
		aa = ((bb < 4096) ? bb : 4096);
		snprintf(buf, sizeof buf, "READ %ld|%ld", transmitted_bytes, aa);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '6') {
			scr_printf("%s\n", &buf[4]);
			return;
		}
		packet = extract_int(&buf[4], 0);
		serv_read(dbuf, packet);
		if (fwrite(dbuf, packet, 1, savefp) < 1)
			broken = 1;
		transmitted_bytes = transmitted_bytes + (long) packet;
		progress(transmitted_bytes, total_bytes);
	}
	fclose(savefp);
	/* close the download file at the server */
	serv_puts("CLOS");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
	}
	return;
}


/*
 * download()  -  download a file or files.  The argument passed to this
 *                function determines which protocol to use.
 *  proto - 0 = paginate, 1 = xmodem, 2 = raw, 3 = ymodem, 4 = zmodem, 5 = save
 */
void download(int proto)
{
	char buf[SIZ];
	char filename[SIZ];
	char tempname[SIZ];
	char transmit_cmd[SIZ];
	long total_bytes = 0L;
	char dbuf[4096];
	long transmitted_bytes = 0L;
	long aa, bb;
	int packet;
	FILE *tpipe = NULL;
	int broken = 0;

	if ((room_flags & QR_DOWNLOAD) == 0) {
		scr_printf("*** You cannot download from this room.\n");
		return;
	}

	newprompt("Enter filename: ", filename, 255);

	snprintf(buf, sizeof buf, "OPEN %s", filename);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}
	total_bytes = extract_long(&buf[4], 0);

	/* Save to local disk, for folks with their own copy of the client */
	if (proto == 5) {
		download_to_local_disk(filename, total_bytes);
		return;
	}

	/* Meta-download for public clients */
	scr_printf("Fetching file from Citadel server...\n");
	mkdir(tempdir, 0700);
	snprintf(tempname, sizeof tempname, "%s/%s", tempdir, filename);
	tpipe = fopen(tempname, "wb");
	while ((transmitted_bytes < total_bytes) && (broken == 0)) {
		progress(transmitted_bytes, total_bytes);
		bb = total_bytes - transmitted_bytes;
		aa = ((bb < 4096) ? bb : 4096);
		snprintf(buf, sizeof buf, "READ %ld|%ld", transmitted_bytes, aa);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '6') {
			scr_printf("%s\n", &buf[4]);
		}
		packet = extract_int(&buf[4], 0);
		serv_read(dbuf, packet);
		if (fwrite(dbuf, packet, 1, tpipe) < 1) {
			broken = 1;
		}
		transmitted_bytes = transmitted_bytes + (long) packet;
	}
	fclose(tpipe);
	progress(transmitted_bytes, total_bytes);

	/* close the download file at the server */
	serv_puts("CLOS");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
	}

	if (proto == 0) {
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
		snprintf(transmit_cmd, sizeof transmit_cmd, "exec cat %s", tempname);

	screen_reset();
	sttybbs(SB_RESTORE);
	system(transmit_cmd);
	sttybbs(SB_NO_INTR);
	screen_set();

	/* clean up the temporary directory */
	nukedir(tempdir);
	scr_putc(7);
}


/*
 * read directory of this room
 */
void roomdir(void)
{
	char flnm[SIZ];
	char flsz[32];
	char comment[SIZ];
	char buf[SIZ];

	serv_puts("RDIR");
	serv_gets(buf);
	if (buf[0] != '1') {
		pprintf("%s\n", &buf[4]);
		return;
	}

	extract(comment, &buf[4], 0);
	extract(flnm, &buf[4], 1);
	pprintf("\nDirectory of %s on %s\n", flnm, comment);
	pprintf("-----------------------\n");
	while (serv_gets(buf), strcmp(buf, "000")) {
		extract(flnm, buf, 0);
		extract(flsz, buf, 1);
		extract(comment, buf, 2);
		if (strlen(flnm) <= 14)
			pprintf("%-14s %8s %s\n", flnm, flsz, comment);
		else
			pprintf("%s\n%14s %8s %s\n", flnm, "", flsz,
				comment);
	}
}


/*
 * add a user to a private room
 */
void invite(void)
{
	char aaa[31], bbb[SIZ];

	/* Because kicking people out of public rooms now sets a LOCKOUT
	 * flag, we need to be able to invite people into public rooms
	 * in order to let them back in again.
	 *        - cough
	 */

	/*
	 * if ((room_flags & QR_PRIVATE)==0) {
	 *         scr_printf("This is not a private room.\n");
	 *         return;
	 * }
	 */

	newprompt("Name of user? ", aaa, 30);
	if (aaa[0] == 0)
		return;

	snprintf(bbb, sizeof bbb, "INVT %s", aaa);
	serv_puts(bbb);
	serv_gets(bbb);
	scr_printf("%s\n", &bbb[4]);
}


/*
 * kick a user out of a room
 */
void kickout(void)
{
	char username[31], cmd[SIZ];

	newprompt("Name of user? ", username, 30);
	if (strlen(username) == 0) {
		return;
	}

	snprintf(cmd, sizeof cmd, "KICK %s", username);
	serv_puts(cmd);
	serv_gets(cmd);
	scr_printf("%s\n", &cmd[4]);
}


/*
 * aide command: kill the current room
 */
void killroom(void)
{
	char aaa[100];

	serv_puts("KILL 0");
	serv_gets(aaa);
	if (aaa[0] != '2') {
		scr_printf("%s\n", &aaa[4]);
		return;
	}

	scr_printf("Are you sure you want to kill this room? ");
	if (yesno() == 0)
		return;

	serv_puts("KILL 1");
	serv_gets(aaa);
	scr_printf("%s\n", &aaa[4]);
	if (aaa[0] != '2')
		return;
	dotgoto("_BASEROOM_", 0, 0);
}

void forget(void)
{				/* forget the current room */
	char cmd[SIZ];

	scr_printf("Are you sure you want to forget this room? ");
	if (yesno() == 0)
		return;

	serv_puts("FORG");
	serv_gets(cmd);
	if (cmd[0] != '2') {
		scr_printf("%s\n", &cmd[4]);
		return;
	}

	/* now return to the lobby */
	dotgoto("_BASEROOM_", 0, 0);
}


/*
 * create a new room
 */
void entroom(void)
{
	char cmd[SIZ];
	char new_room_name[ROOMNAMELEN];
	int new_room_type;
	char new_room_pass[10];
	int new_room_floor;
	int a, b;

	serv_puts("CRE8 0");
	serv_gets(cmd);

	if (cmd[0] != '2') {
		scr_printf("%s\n", &cmd[4]);
		return;
	}

	newprompt("Name for new room? ", new_room_name, ROOMNAMELEN - 1);
	if (strlen(new_room_name) == 0) {
		return;
	}
	for (a = 0; a < strlen(new_room_name); ++a) {
		if (new_room_name[a] == '|') {
			new_room_name[a] = '_';
		}
	}

	new_room_floor = select_floor((int) curr_floor);

	IFNEXPERT formout("roomaccess");
	do {
		scr_printf("<?>Help\n<1>Public room\n<2>Guess-name room\n"
		       "<3>Passworded room\n<4>Invitation-only room\n"
		       "<5>Personal room\n"
			"Enter room type: ");
		do {
			b = inkey();
		} while (((b < '1') || (b > '5')) && (b != '?'));
		if (b == '?') {
			scr_printf("?\n");
			formout("roomaccess");
		}
	} while ((b < '1') || (b > '5'));
	b = b - 48;
	scr_printf("%d\n", b);
	new_room_type = b - 1;
	if (new_room_type == 2) {
		newprompt("Enter a room password: ", new_room_pass, 9);
		for (a = 0; a < strlen(new_room_pass); ++a)
			if (new_room_pass[a] == '|')
				new_room_pass[a] = '_';
	} else {
		strcpy(new_room_pass, "");
	}

	scr_printf("\042%s\042, a", new_room_name);
	if (b == 1)
		scr_printf(" public room.");
	if (b == 2)
		scr_printf(" guess-name room.");
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

	snprintf(cmd, sizeof cmd, "CRE8 1|%s|%d|%s|%d", new_room_name,
		 new_room_type, new_room_pass, new_room_floor);
	serv_puts(cmd);
	serv_gets(cmd);
	if (cmd[0] != '2') {
		scr_printf("%s\n", &cmd[4]);
		return;
	}

	/* command succeeded... now GO to the new room! */
	dotgoto(new_room_name, 0, 0);
}



void readinfo(void)
{				/* read info file for current room */
	char buf[SIZ];
	char raide[64];
	int r;			/* IPC response code */
	char *text = NULL;

	/* Name of currernt room aide */
	r = CtdlIPCGetRoomAide(buf);
	if (r / 100 == 2)
		safestrncpy(raide, buf, sizeof raide);
	else
		strcpy(raide, "");

	if (strlen(raide) > 0)
		scr_printf("Room aide is %s.\n\n", raide);

	r = CtdlIPCRoomInfo(&text, buf);
	if (r / 100 != 1)
		return;

	if (text) {
		fmout(screenwidth, NULL, text, NULL,
		      ((userflags & US_PAGINATOR) ? 1 : 0), screenheight, 
		      (*raide) ? 2 : 0, 1);
		free(text);
	}
}


/*
 * <W>ho knows room...
 */
void whoknows(void)
{
	char buf[SIZ];
	serv_puts("WHOK");
	serv_gets(buf);
	if (buf[0] != '1') {
		pprintf("%s\n", &buf[4]);
		return;
	}
	while (serv_gets(buf), strncmp(buf, "000", 3)) {
		if (sigcaught == 0)
			pprintf("%s\n", buf);
	}
}


void do_edit(char *desc, char *read_cmd, char *check_cmd, char *write_cmd)
{
	FILE *fp;
	char cmd[SIZ];
	int b, cksum, editor_exit;


	if (strlen(editor_path) == 0) {
		scr_printf("Do you wish to re-enter %s? ", desc);
		if (yesno() == 0)
			return;
	}

	fp = fopen(temp, "w");
	fclose(fp);

	serv_puts(check_cmd);
	serv_gets(cmd);
	if (cmd[0] != '2') {
		scr_printf("%s\n", &cmd[4]);
		return;
	}

	if (strlen(editor_path) > 0) {
		serv_puts(read_cmd);
		serv_gets(cmd);
		if (cmd[0] == '1') {
			fp = fopen(temp, "w");
			while (serv_gets(cmd), strcmp(cmd, "000")) {
				fprintf(fp, "%s\n", cmd);
			}
			fclose(fp);
		}
	}

	cksum = file_checksum(temp);

	if (strlen(editor_path) > 0) {
		char tmp[SIZ];

		snprintf(tmp, sizeof tmp, "WINDOW_TITLE=%s", desc);
		putenv(tmp);
		editor_pid = fork();
		if (editor_pid == 0) {
			chmod(temp, 0600);
			screen_reset();
			sttybbs(SB_RESTORE);
			execlp(editor_path, editor_path, temp, NULL);
			exit(1);
		}
		if (editor_pid > 0)
			do {
				editor_exit = 0;
				b = wait(&editor_exit);
			} while ((b != editor_pid) && (b >= 0));
		editor_pid = (-1);
		scr_printf("Executed %s\n", editor_path);
		sttybbs(0);
		screen_set();
	} else {
		scr_printf("Entering %s.  "
			"Press return twice when finished.\n", desc);
		fp = fopen(temp, "r+");
		citedit(fp);
		fclose(fp);
	}

	if (file_checksum(temp) == cksum) {
		scr_printf("*** Aborted.\n");
	}

	else {
		serv_puts(write_cmd);
		serv_gets(cmd);
		if (cmd[0] != '4') {
			scr_printf("%s\n", &cmd[4]);
			return;
		}

		fp = fopen(temp, "r");
		while (fgets(cmd, SIZ - 1, fp) != NULL) {
			cmd[strlen(cmd) - 1] = 0;
			serv_puts(cmd);
		}
		fclose(fp);
		serv_puts("000");
	}

	unlink(temp);
}


void enterinfo(void)
{				/* edit info file for current room */
	do_edit("the Info file for this room", "RINF", "EINF 0", "EINF 1");
}

void enter_bio(void)
{
	char cmd[SIZ];
	snprintf(cmd, sizeof cmd, "RBIO %s", fullname);
	do_edit("your Bio", cmd, "NOOP", "EBIO");
}

/*
 * create a new floor
 */
void create_floor(void)
{
	char buf[SIZ];
	char newfloorname[SIZ];

	load_floorlist();

	serv_puts("CFLR xx|0");
	serv_gets(buf);
	if (buf[0] != '2') {
		scr_printf("%s\n", &buf[4]);
		return;
	}

	newprompt("Name for new floor: ", newfloorname, 255);
	snprintf(buf, sizeof buf, "CFLR %s|1", newfloorname);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '2') {
		scr_printf("Floor has been created.\n");
	} else {
		scr_printf("%s\n", &buf[4]);
	}

	load_floorlist();
}

/*
 * edit the current floor
 */
void edit_floor(void)
{
	char buf[SIZ];
	int expire_mode = 0;
	int expire_value = 0;

	load_floorlist();

	/* Fetch the expire policy (this will silently fail on old servers,
	 * resulting in "default" policy)
	 */
	serv_puts("GPEX floor");
	serv_gets(buf);
	if (buf[0] == '2') {
		expire_mode = extract_int(&buf[4], 0);
		expire_value = extract_int(&buf[4], 1);
	}

	/* Interact with the user */
	strprompt("Floor name", &floorlist[(int) curr_floor][0], 255);

	/* Angels and demons dancing in my head... */
	do {
		snprintf(buf, sizeof buf, "%d", expire_mode);
		strprompt
		    ("Floor default essage expire policy (? for list)",
		     buf, 1);
		if (buf[0] == '?') {
			scr_printf("\n"
				"0. Use the system default\n"
				"1. Never automatically expire messages\n"
				"2. Expire by message count\n"
				"3. Expire by message age\n");
		}
	} while ((buf[0] < 48) || (buf[0] > 51));
	expire_mode = buf[0] - 48;

	/* ...lunatics and monsters underneath my bed */
	if (expire_mode == 2) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep how many messages online?", buf, 10);
		expire_value = atol(buf);
	}

	if (expire_mode == 3) {
		snprintf(buf, sizeof buf, "%d", expire_value);
		strprompt("Keep messages for how many days?", buf, 10);
		expire_value = atol(buf);
	}

	/* Save it */
	snprintf(buf, sizeof buf, "SPEX floor|%d|%d",
		 expire_mode, expire_value);
	serv_puts(buf);
	serv_gets(buf);

	snprintf(buf, sizeof buf, "EFLR %d|%s", curr_floor,
		 &floorlist[(int) curr_floor][0]);
	serv_puts(buf);
	serv_gets(buf);
	scr_printf("%s\n", &buf[4]);
	load_floorlist();
}




/*
 * kill the current floor 
 */
void kill_floor(void)
{
	int floornum_to_delete, a;
	char buf[SIZ];

	load_floorlist();
	do {
		floornum_to_delete = (-1);
		scr_printf("(Press return to abort)\n");
		newprompt("Delete which floor? ", buf, 255);
		if (strlen(buf) == 0)
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
	snprintf(buf, sizeof buf, "KFLR %d|1", floornum_to_delete);
	serv_puts(buf);
	serv_gets(buf);
	scr_printf("%s\n", &buf[4]);
	load_floorlist();
}
