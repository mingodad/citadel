/* 
 * Server functions which perform operations on room objects.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <dirent.h>	/* for cmd_rdir to read contents of the directory */
#include <libcitadel.h>

#include "citserver.h"
#include "ctdl_module.h"
#include "room_ops.h"
#include "config.h"

/*
 * Back-back-end for all room listing commands
 */
void list_roomname(struct ctdlroom *qrbuf, int ra, int current_view, int default_view)
{
	char truncated_roomname[ROOMNAMELEN];

	/* For my own mailbox rooms, chop off the owner prefix */
	if ( (qrbuf->QRflags & QR_MAILBOX)
	     && (atol(qrbuf->QRname) == CC->user.usernum) ) {
		safestrncpy(truncated_roomname, qrbuf->QRname, sizeof truncated_roomname);
		safestrncpy(truncated_roomname, &truncated_roomname[11], sizeof truncated_roomname);
		cprintf("%s", truncated_roomname);
	}
	/* For all other rooms, just display the name in its entirety */
	else {
		cprintf("%s", qrbuf->QRname);
	}

	/* ...and now the other parameters */
	cprintf("|%u|%d|%d|%d|%d|%d|%d|%ld|\n",
		qrbuf->QRflags,
		(int) qrbuf->QRfloor,
		(int) qrbuf->QRorder,
		(int) qrbuf->QRflags2,
		ra,
		current_view,
		default_view,
		qrbuf->QRmtime
	);
}


/* 
 * cmd_lrms()   -  List all accessible rooms, known or forgotten
 */
void cmd_lrms_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((( ra & (UA_KNOWN | UA_ZAPPED)))
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lrms(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Accessible rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lrms_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkra()   -  List all known rooms
 */
void cmd_lkra_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((( ra & (UA_KNOWN)))
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkra(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Known rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkra_backend, &FloorBeingSearched);
	cprintf("000\n");
}



void cmd_lprm_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if (   ((qrbuf->QRflags & QR_PRIVATE) == 0)
		&& ((qrbuf->QRflags & QR_MAILBOX) == 0)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lprm(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	cprintf("%d Public rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lprm_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkrn()   -  List all known rooms with new messages
 */
void cmd_lkrn_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_KNOWN)
	    && (ra & UA_HASNEWMSGS)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkrn(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Rooms w/ new msgs:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkrn_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkro()   -  List all known rooms
 */
void cmd_lkro_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_KNOWN)
	    && ((ra & UA_HASNEWMSGS) == 0)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkro(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Rooms w/o new msgs:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkro_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lzrm()   -  List all forgotten rooms
 */
void cmd_lzrm_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_GOTOALLOWED)
	    && (ra & UA_ZAPPED)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lzrm(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Zapped rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lzrm_backend, &FloorBeingSearched);
	cprintf("000\n");
}


/* 
 * cmd_goto()  -  goto a new room
 */
void cmd_goto(char *gargs)
{
	struct ctdlroom QRscratch;
	int c;
	int ok = 0;
	int ra;
	char augmented_roomname[ROOMNAMELEN];
	char towhere[ROOMNAMELEN];
	char password[32];
	int transiently = 0;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	extract_token(towhere, gargs, 0, '|', sizeof towhere);
	extract_token(password, gargs, 1, '|', sizeof password);
	transiently = extract_int(gargs, 2);

	CtdlGetUser(&CC->user, CC->curr_user);

	/*
	 * Handle some of the macro named rooms
	 */
	convert_room_name_macros(towhere, sizeof towhere);

	/* First try a regular match */
	c = CtdlGetRoom(&QRscratch, towhere);

	/* Then try a mailbox name match */
	if (c != 0) {
		CtdlMailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->user, towhere);
		c = CtdlGetRoom(&QRscratch, augmented_roomname);
		if (c == 0)
			safestrncpy(towhere, augmented_roomname, sizeof towhere);
	}

	/* And if the room was found... */
	if (c == 0) {

		/* Let internal programs go directly to any room. */
		if (CC->internal_pgm) {
			memcpy(&CC->room, &QRscratch,
				sizeof(struct ctdlroom));
			CtdlUserGoto(NULL, 1, transiently, NULL, NULL, NULL, NULL);
			return;
		}

		/* See if there is an existing user/room relationship */
		CtdlRoomAccess(&QRscratch, &CC->user, &ra, NULL);

		/* normal clients have to pass through security */
		if (ra & UA_GOTOALLOWED) {
			ok = 1;
		}

		if (ok == 1) {
			if ((QRscratch.QRflags & QR_MAILBOX) &&
			    ((ra & UA_GOTOALLOWED))) {
				memcpy(&CC->room, &QRscratch,
					sizeof(struct ctdlroom));
				CtdlUserGoto(NULL, 1, transiently, NULL, NULL, NULL, NULL);
				return;
			} else if ((QRscratch.QRflags & QR_PASSWORDED) &&
			    ((ra & UA_KNOWN) == 0) &&
			    (strcasecmp(QRscratch.QRpasswd, password)) &&
			    (CC->user.axlevel < AxAideU)
			    ) {
				cprintf("%d wrong or missing passwd\n",
					ERROR + PASSWORD_REQUIRED);
				return;
			} else if ((QRscratch.QRflags & QR_PRIVATE) &&
				   ((QRscratch.QRflags & QR_PASSWORDED) == 0) &&
				   ((QRscratch.QRflags & QR_GUESSNAME) == 0) &&
				   ((ra & UA_KNOWN) == 0) &&
			           (CC->user.axlevel < AxAideU)
                                  ) {
				syslog(LOG_DEBUG, "Failed to acquire private room\n");
			} else {
				memcpy(&CC->room, &QRscratch,
					sizeof(struct ctdlroom));
				CtdlUserGoto(NULL, 1, transiently, NULL, NULL, NULL, NULL);
				return;
			}
		}
	}

	cprintf("%d room '%s' not found\n", ERROR + ROOM_NOT_FOUND, towhere);
}


void cmd_whok(char *cmdbuf)
{
	struct ctdluser temp;
	struct cdbdata *cdbus;
	int ra;

	cprintf("%d Who knows room:\n", LISTING_FOLLOWS);
	cdb_rewind(CDB_USERS);
	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(&temp, 0, sizeof temp);
		memcpy(&temp, cdbus->ptr, sizeof temp);
		cdb_free(cdbus);

		CtdlRoomAccess(&CC->room, &temp, &ra, NULL);
		if ((!IsEmptyStr(temp.fullname)) && 
		    (CC->room.QRflags & QR_INUSE) &&
		    (ra & UA_KNOWN)
			)
			cprintf("%s\n", temp.fullname);
	}
	cprintf("000\n");
}


/*
 * RDIR command for room directory
 */
void cmd_rdir(char *cmdbuf)
{
	char buf[256];
	char comment[256];
	FILE *fd;
	struct stat statbuf;
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	int d_namelen;
	char buf2[SIZ];
	char mimebuf[64];
	long len;
	
	if (CtdlAccessCheck(ac_logged_in)) return;
	
	CtdlGetRoom(&CC->room, CC->room.QRname);
	CtdlGetUser(&CC->user, CC->curr_user);

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d not here.\n", ERROR + NOT_HERE);
		return;
	}
	if (((CC->room.QRflags & QR_VISDIR) == 0)
	    && (CC->user.axlevel < AxAideU)
	    && (CC->user.usernum != CC->room.QRroomaide)) {
		cprintf("%d not here.\n", ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	snprintf(buf, sizeof buf, "%s/%s", ctdl_file_dir, CC->room.QRdirname);
	filedir = opendir (buf);
	
	if (filedir == NULL) {
		cprintf("%d not here.\n", ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	cprintf("%d %s|%s/%s\n", LISTING_FOLLOWS, CtdlGetConfigStr("c_fqdn"), ctdl_file_dir, CC->room.QRdirname);
	
	snprintf(buf, sizeof buf, "%s/%s/filedir", ctdl_file_dir, CC->room.QRdirname);
	fd = fopen(buf, "r");
	if (fd == NULL)
		fd = fopen("/dev/null", "r");
	while ((filedir_entry = readdir(filedir)))
	{
		if (strcasecmp(filedir_entry->d_name, "filedir") && filedir_entry->d_name[0] != '.')
		{
#ifdef _DIRENT_HAVE_D_NAMELEN
			d_namelen = filedir_entry->d_namlen;
#else
			d_namelen = strlen(filedir_entry->d_name);
#endif
			snprintf(buf, sizeof buf, "%s/%s/%s", ctdl_file_dir, CC->room.QRdirname, filedir_entry->d_name);
			stat(buf, &statbuf);	/* stat the file */
			if (!(statbuf.st_mode & S_IFREG))
			{
				snprintf(buf2, sizeof buf2,
					"\"%s\" appears in the file directory for room \"%s\" but is not a regular file.  Directories, named pipes, sockets, etc. are not usable in Citadel room directories.\n",
					buf, CC->room.QRname
				);
				CtdlAideMessage(buf2, "Unusable data found in room directory");
				continue;	/* not a useable file type so don't show it */
			}
			safestrncpy(comment, "", sizeof comment);
			fseek(fd, 0L, 0);	/* rewind descriptions file */
			/* Get the description from the descriptions file */
			while ((fgets(buf, sizeof buf, fd) != NULL) && (IsEmptyStr(comment))) 
			{
				buf[strlen(buf) - 1] = 0;
				if ((!strncasecmp(buf, filedir_entry->d_name, d_namelen)) && (buf[d_namelen] == ' '))
					safestrncpy(comment, &buf[d_namelen + 1], sizeof comment);
			}
			len = extract_token (mimebuf, comment, 0,' ', 64);
			if ((len <0) || strchr(mimebuf, '/') == NULL)
			{
				snprintf (mimebuf, 64, "application/octetstream");
				len = 0;
			}
			cprintf("%s|%ld|%s|%s\n", 
				filedir_entry->d_name, 
				(long)statbuf.st_size, 
				mimebuf, 
				&comment[len]);
		}
	}
	fclose(fd);
	closedir(filedir);
	
	cprintf("000\n");
}

/*
 * get room parameters (admin or room admin command)
 */
void cmd_getr(char *cmdbuf)
{
	if (CtdlAccessCheck(ac_room_aide)) return;

	CtdlGetRoom(&CC->room, CC->room.QRname);
	cprintf("%d%c%s|%s|%s|%d|%d|%d|%d|%d|\n",
		CIT_OK,
		CtdlCheckExpress(),

		((CC->room.QRflags & QR_MAILBOX) ?
			&CC->room.QRname[11] : CC->room.QRname),

		((CC->room.QRflags & QR_PASSWORDED) ?
			CC->room.QRpasswd : ""),

		((CC->room.QRflags & QR_DIRECTORY) ?
			CC->room.QRdirname : ""),

		CC->room.QRflags,
		(int) CC->room.QRfloor,
		(int) CC->room.QRorder,

		CC->room.QRdefaultview,
		CC->room.QRflags2
		);
}

/*
 * set room parameters (admin or room admin command)
 */
void cmd_setr(char *args)
{
	char buf[256];
	int new_order = 0;
	int r;
	int new_floor;
	char new_name[ROOMNAMELEN];

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (num_parms(args) >= 6) {
		new_floor = extract_int(args, 5);
	} else {
		new_floor = (-1);	/* don't change the floor */
	}

	/* When is a new name more than just a new name?  When the old name
	 * has a namespace prefix.
	 */
	if (CC->room.QRflags & QR_MAILBOX) {
		sprintf(new_name, "%010ld.", atol(CC->room.QRname) );
	} else {
		safestrncpy(new_name, "", sizeof new_name);
	}
	extract_token(&new_name[strlen(new_name)], args, 0, '|', (sizeof new_name - strlen(new_name)));

	r = CtdlRenameRoom(CC->room.QRname, new_name, new_floor);

	if (r == crr_room_not_found) {
		cprintf("%d Internal error - room not found?\n", ERROR + INTERNAL_ERROR);
	} else if (r == crr_already_exists) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_name);
	} else if (r == crr_noneditable) {
		cprintf("%d Cannot edit this room.\n", ERROR + NOT_HERE);
	} else if (r == crr_invalid_floor) {
		cprintf("%d Target floor does not exist.\n",
			ERROR + INVALID_FLOOR_OPERATION);
	} else if (r == crr_access_denied) {
		cprintf("%d You do not have permission to edit '%s'\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			CC->room.QRname);
	} else if (r != crr_ok) {
		cprintf("%d Error: CtdlRenameRoom() returned %d\n",
			ERROR + INTERNAL_ERROR, r);
	}

	if (r != crr_ok) {
		return;
	}

	CtdlGetRoom(&CC->room, new_name);

	/* Now we have to do a bunch of other stuff */

	if (num_parms(args) >= 7) {
		new_order = extract_int(args, 6);
		if (new_order < 1)
			new_order = 1;
		if (new_order > 127)
			new_order = 127;
	}

	CtdlGetRoomLock(&CC->room, CC->room.QRname);

	/* Directory room */
	extract_token(buf, args, 2, '|', sizeof buf);
	buf[15] = 0;
	safestrncpy(CC->room.QRdirname, buf,
		sizeof CC->room.QRdirname);

	/* Default view */
	if (num_parms(args) >= 8) {
		CC->room.QRdefaultview = extract_int(args, 7);
	}

	/* Second set of flags */
	if (num_parms(args) >= 9) {
		CC->room.QRflags2 = extract_int(args, 8);
	}

	/* Misc. flags */
	CC->room.QRflags = (extract_int(args, 3) | QR_INUSE);
	/* Clean up a client boo-boo: if the client set the room to
	 * guess-name or passworded, ensure that the private flag is
	 * also set.
	 */
	if ((CC->room.QRflags & QR_GUESSNAME)
	    || (CC->room.QRflags & QR_PASSWORDED))
		CC->room.QRflags |= QR_PRIVATE;

	/* Some changes can't apply to BASEROOM */
	if (!strncasecmp(CC->room.QRname, CtdlGetConfigStr("c_baseroom"), ROOMNAMELEN)) {
		CC->room.QRorder = 0;
		CC->room.QRpasswd[0] = '\0';
		CC->room.QRflags &= ~(QR_PRIVATE & QR_PASSWORDED &
			QR_GUESSNAME & QR_PREFONLY & QR_MAILBOX);
		CC->room.QRflags |= QR_PERMANENT;
	} else {	
		/* March order (doesn't apply to AIDEROOM) */
		if (num_parms(args) >= 7)
			CC->room.QRorder = (char) new_order;
		/* Room password */
		extract_token(buf, args, 1, '|', sizeof buf);
		buf[10] = 0;
		safestrncpy(CC->room.QRpasswd, buf,
			    sizeof CC->room.QRpasswd);
		/* Kick everyone out if the client requested it
		 * (by changing the room's generation number)
		 */
		if (extract_int(args, 4)) {
			time(&CC->room.QRgen);
		}
	}
	/* Some changes can't apply to AIDEROOM */
	if (!strncasecmp(CC->room.QRname, CtdlGetConfigStr("c_baseroom"), ROOMNAMELEN)) {
		CC->room.QRorder = 0;
		CC->room.QRflags &= ~QR_MAILBOX;
		CC->room.QRflags |= QR_PERMANENT;
	}

	/* Write the room record back to disk */
	CtdlPutRoomLock(&CC->room);

	/* Create a room directory if necessary */
	if (CC->room.QRflags & QR_DIRECTORY) {
		snprintf(buf, sizeof buf,"%s/%s",
				 ctdl_file_dir,
				 CC->room.QRdirname);
		mkdir(buf, 0755);
	}
	snprintf(buf, sizeof buf, "The room \"%s\" has been edited by %s.\n",
		CC->room.QRname,
		(CC->logged_in ? CC->curr_user : "an administrator")
	);
	CtdlAideMessage(buf, "Room modification Message");
	cprintf("%d Ok\n", CIT_OK);
}



/* 
 * get the name of the room admin for this room
 */
void cmd_geta(char *cmdbuf)
{
	struct ctdluser usbuf;

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (CtdlGetUserByNumber(&usbuf, CC->room.QRroomaide) == 0) {
		cprintf("%d %s\n", CIT_OK, usbuf.fullname);
	} else {
		cprintf("%d \n", CIT_OK);
	}
}


/* 
 * set the room admin for this room
 */
void cmd_seta(char *new_ra)
{
	struct ctdluser usbuf;
	long newu;
	char buf[SIZ];
	int post_notice;

	if (CtdlAccessCheck(ac_room_aide)) return;

	if (CtdlGetUser(&usbuf, new_ra) != 0) {
		newu = (-1L);
	} else {
		newu = usbuf.usernum;
	}

	CtdlGetRoomLock(&CC->room, CC->room.QRname);
	post_notice = 0;
	if (CC->room.QRroomaide != newu) {
		post_notice = 1;
	}
	CC->room.QRroomaide = newu;
	CtdlPutRoomLock(&CC->room);

	/*
	 * We have to post the change notice _after_ writing changes to 
	 * the room table, otherwise it would deadlock!
	 */
	if (post_notice == 1) {
		if (!IsEmptyStr(usbuf.fullname))
			snprintf(buf, sizeof buf,
				"%s is now the room admin for \"%s\".\n",
				usbuf.fullname, CC->room.QRname);
		else
			snprintf(buf, sizeof buf,
				"There is now no room admin for \"%s\".\n",
				CC->room.QRname);
		CtdlAideMessage(buf, "Admin Room Modification");
	}
	cprintf("%d Ok\n", CIT_OK);
}

/* 
 * retrieve info file for this room
 */
void cmd_rinf(char *gargs)
{
	char filename[PATH_MAX];
	char buf[SIZ];
	FILE *info_fp;

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_info_dir);
	info_fp = fopen(filename, "r");

	if (info_fp == NULL) {
		cprintf("%d No info file.\n", ERROR + FILE_NOT_FOUND);
		return;
	}
	cprintf("%d Info:\n", LISTING_FOLLOWS);
	while (fgets(buf, sizeof buf, info_fp) != NULL) {
		if (!IsEmptyStr(buf))
			buf[strlen(buf) - 1] = 0;
		cprintf("%s\n", buf);
	}
	cprintf("000\n");
	fclose(info_fp);
}


/*
 * admin command: kill the current room
 */
void cmd_kill(char *argbuf)
{
	char deleted_room_name[ROOMNAMELEN];
	char msg[SIZ];
	int kill_ok;

	kill_ok = extract_int(argbuf, 0);

	if (CtdlDoIHavePermissionToDeleteThisRoom(&CC->room) == 0) {
		cprintf("%d Can't delete this room.\n", ERROR + NOT_HERE);
		return;
	}
	if (kill_ok) {
		if (CC->room.QRflags & QR_MAILBOX) {
			safestrncpy(deleted_room_name, &CC->room.QRname[11], sizeof deleted_room_name);
		}
		else {
			safestrncpy(deleted_room_name, CC->room.QRname, sizeof deleted_room_name);
		}

		/* Do the dirty work */
		CtdlScheduleRoomForDeletion(&CC->room);

		/* Return to the Lobby */
		CtdlUserGoto(CtdlGetConfigStr("c_baseroom"), 0, 0, NULL, NULL, NULL, NULL);

		/* tell the world what we did */
		snprintf(msg, sizeof msg, "The room \"%s\" has been deleted by %s.\n",
			 deleted_room_name,
			(CC->logged_in ? CC->curr_user : "an administrator")
		);
		CtdlAideMessage(msg, "Room Purger Message");
		cprintf("%d '%s' deleted.\n", CIT_OK, deleted_room_name);
	} else {
		cprintf("%d ok to delete.\n", CIT_OK);
	}
}


/*
 * create a new room
 */
void cmd_cre8(char *args)
{
	int cre8_ok;
	char new_room_name[ROOMNAMELEN];
	int new_room_type;
	char new_room_pass[32];
	int new_room_floor;
	int new_room_view;
	char *notification_message = NULL;
	unsigned newflags;
	struct floor *fl;
	int avoid_access = 0;

	cre8_ok = extract_int(args, 0);
	extract_token(new_room_name, args, 1, '|', sizeof new_room_name);
	new_room_name[ROOMNAMELEN - 1] = 0;
	new_room_type = extract_int(args, 2);
	extract_token(new_room_pass, args, 3, '|', sizeof new_room_pass);
	avoid_access = extract_int(args, 5);
	new_room_view = extract_int(args, 6);
	new_room_pass[9] = 0;
	new_room_floor = 0;

	if ((IsEmptyStr(new_room_name)) && (cre8_ok == 1)) {
		cprintf("%d Invalid room name.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (!strcasecmp(new_room_name, MAILROOM)) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_room_name);
		return;
	}

	if (num_parms(args) >= 5) {
		fl = CtdlGetCachedFloor(extract_int(args, 4));
		if (fl == NULL) {
			cprintf("%d Invalid floor number.\n",
				ERROR + INVALID_FLOOR_OPERATION);
			return;
		}
		else if ((fl->f_flags & F_INUSE) == 0) {
			cprintf("%d Invalid floor number.\n",
				ERROR + INVALID_FLOOR_OPERATION);
			return;
		} else {
			new_room_floor = extract_int(args, 4);
		}
	}

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (CC->user.axlevel < CtdlGetConfigInt("c_createax") && !CC->internal_pgm) {
		cprintf("%d You need higher access to create rooms.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if ((IsEmptyStr(new_room_name)) && (cre8_ok == 0)) {
		cprintf("%d Ok to create rooms.\n", CIT_OK);
		return;
	}

	if ((new_room_type < 0) || (new_room_type > 5)) {
		cprintf("%d Invalid room type.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (new_room_type == 5) {
		if (CC->user.axlevel < AxAideU) {
			cprintf("%d Higher access required\n", 
				ERROR + HIGHER_ACCESS_REQUIRED);
			return;
		}
	}

	/* Check to make sure the requested room name doesn't already exist */
	newflags = CtdlCreateRoom(new_room_name,
				new_room_type, new_room_pass, new_room_floor,
				0, avoid_access, new_room_view);
	if (newflags == 0) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_room_name);
		return;
	}

	if (cre8_ok == 0) {
		cprintf("%d OK to create '%s'\n", CIT_OK, new_room_name);
		return;
	}

	/* If we reach this point, the room needs to be created. */

	newflags = CtdlCreateRoom(new_room_name,
			   new_room_type, new_room_pass, new_room_floor, 1, 0,
			   new_room_view);

	/* post a message in Aide> describing the new room */
	notification_message = malloc(1024);
	snprintf(notification_message, 1024,
		"A new room called \"%s\" has been created by %s%s%s%s%s%s\n",
		new_room_name,
		(CC->logged_in ? CC->curr_user : "an administrator"),
		((newflags & QR_MAILBOX) ? " [personal]" : ""),
		((newflags & QR_PRIVATE) ? " [private]" : ""),
		((newflags & QR_GUESSNAME) ? " [hidden]" : ""),
		((newflags & QR_PASSWORDED) ? " Password: " : ""),
		((newflags & QR_PASSWORDED) ? new_room_pass : "")
	);
	CtdlAideMessage(notification_message, "Room Creation Message");
	free(notification_message);

	cprintf("%d '%s' has been created.\n", CIT_OK, new_room_name);
}



void cmd_einf(char *ok)
{				/* enter info file for current room */
	FILE *fp;
	char infofilename[SIZ];
	char buf[SIZ];

	unbuffer_output();

	if (CtdlAccessCheck(ac_room_aide)) return;

	if (atoi(ok) == 0) {
		cprintf("%d Ok.\n", CIT_OK);
		return;
	}
	assoc_file_name(infofilename, sizeof infofilename, &CC->room, ctdl_info_dir);
	syslog(LOG_DEBUG, "opening\n");
	fp = fopen(infofilename, "w");
	syslog(LOG_DEBUG, "checking\n");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
		  ERROR + INTERNAL_ERROR, infofilename, strerror(errno));
		return;
	}
	cprintf("%d Send info...\n", SEND_LISTING);

	do {
		client_getln(buf, sizeof buf);
		if (strcmp(buf, "000"))
			fprintf(fp, "%s\n", buf);
	} while (strcmp(buf, "000"));
	fclose(fp);

	/* now update the room index so people will see our new info */
	CtdlGetRoomLock(&CC->room, CC->room.QRname);		/* lock so no one steps on us */
	CC->room.QRinfo = CC->room.QRhighest + 1L;
	CtdlPutRoomLock(&CC->room);
}


/* 
 * cmd_lflr()   -  List all known floors
 */
void cmd_lflr(char *gargs)
{
	int a;
	struct floor flbuf;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	cprintf("%d Known floors:\n", LISTING_FOLLOWS);

	for (a = 0; a < MAXFLOORS; ++a) {
		CtdlGetFloor(&flbuf, a);
		if (flbuf.f_flags & F_INUSE) {
			cprintf("%d|%s|%d\n",
				a,
				flbuf.f_name,
				flbuf.f_ref_count);
		}
	}
	cprintf("000\n");
}



/*
 * create a new floor
 */
void cmd_cflr(char *argbuf)
{
	char new_floor_name[256];
	struct floor flbuf;
	int cflr_ok;
	int free_slot = (-1);
	int a;

	extract_token(new_floor_name, argbuf, 0, '|', sizeof new_floor_name);
	cflr_ok = extract_int(argbuf, 1);

	if (CtdlAccessCheck(ac_aide)) return;

	if (IsEmptyStr(new_floor_name)) {
		cprintf("%d Blank floor name not allowed.\n",
			ERROR + ILLEGAL_VALUE);
		return;
	}

	for (a = 0; a < MAXFLOORS; ++a) {
		CtdlGetFloor(&flbuf, a);

		/* note any free slots while we're scanning... */
		if (((flbuf.f_flags & F_INUSE) == 0)
		    && (free_slot < 0))
			free_slot = a;

		/* check to see if it already exists */
		if ((!strcasecmp(flbuf.f_name, new_floor_name))
		    && (flbuf.f_flags & F_INUSE)) {
			cprintf("%d Floor '%s' already exists.\n",
				ERROR + ALREADY_EXISTS,
				flbuf.f_name);
			return;
		}
	}

	if (free_slot < 0) {
		cprintf("%d There is no space available for a new floor.\n",
			ERROR + INVALID_FLOOR_OPERATION);
		return;
	}
	if (cflr_ok == 0) {
		cprintf("%d ok to create...\n", CIT_OK);
		return;
	}
	lgetfloor(&flbuf, free_slot);
	flbuf.f_flags = F_INUSE;
	flbuf.f_ref_count = 0;
	safestrncpy(flbuf.f_name, new_floor_name, sizeof flbuf.f_name);
	lputfloor(&flbuf, free_slot);
	cprintf("%d %d\n", CIT_OK, free_slot);
}



/*
 * delete a floor
 */
void cmd_kflr(char *argbuf)
{
	struct floor flbuf;
	int floor_to_delete;
	int kflr_ok;
	int delete_ok;

	floor_to_delete = extract_int(argbuf, 0);
	kflr_ok = extract_int(argbuf, 1);

	if (CtdlAccessCheck(ac_aide)) return;

	lgetfloor(&flbuf, floor_to_delete);

	delete_ok = 1;
	if ((flbuf.f_flags & F_INUSE) == 0) {
		cprintf("%d Floor %d not in use.\n",
			ERROR + INVALID_FLOOR_OPERATION, floor_to_delete);
		delete_ok = 0;
	} else {
		if (flbuf.f_ref_count != 0) {
			cprintf("%d Cannot delete; floor contains %d rooms.\n",
				ERROR + INVALID_FLOOR_OPERATION,
				flbuf.f_ref_count);
			delete_ok = 0;
		} else {
			if (kflr_ok == 1) {
				cprintf("%d Ok\n", CIT_OK);
			} else {
				cprintf("%d Ok to delete...\n", CIT_OK);
			}

		}

	}

	if ((delete_ok == 1) && (kflr_ok == 1))
		flbuf.f_flags = 0;
	lputfloor(&flbuf, floor_to_delete);
}

/*
 * edit a floor
 */
void cmd_eflr(char *argbuf)
{
	struct floor flbuf;
	int floor_num;
	int np;

	np = num_parms(argbuf);
	if (np < 1) {
		cprintf("%d Usage error.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (CtdlAccessCheck(ac_aide)) return;

	floor_num = extract_int(argbuf, 0);
	lgetfloor(&flbuf, floor_num);
	if ((flbuf.f_flags & F_INUSE) == 0) {
		lputfloor(&flbuf, floor_num);
		cprintf("%d Floor %d is not in use.\n",
			ERROR + INVALID_FLOOR_OPERATION, floor_num);
		return;
	}
	if (np >= 2)
		extract_token(flbuf.f_name, argbuf, 1, '|', sizeof flbuf.f_name);
	lputfloor(&flbuf, floor_num);

	cprintf("%d Ok\n", CIT_OK);
}



/* 
 * cmd_stat()  -  return the modification time of the current room (maybe other things in the future)
 */
void cmd_stat(char *gargs)
{
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	CtdlGetRoom(&CC->room, CC->room.QRname);
	cprintf("%d %s|%ld|\n", CIT_OK, CC->room.QRname, CC->room.QRmtime);
}




/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(rooms)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_lrms, "LRMS", "List rooms");
		CtdlRegisterProtoHook(cmd_lkra, "LKRA", "List all known rooms");
		CtdlRegisterProtoHook(cmd_lkrn, "LKRN", "List known rooms with new messages");
		CtdlRegisterProtoHook(cmd_lkro, "LKRO", "List known rooms without new messages");
		CtdlRegisterProtoHook(cmd_lzrm, "LZRM", "List zapped rooms");
		CtdlRegisterProtoHook(cmd_lprm, "LPRM", "List public rooms");
		CtdlRegisterProtoHook(cmd_goto, "GOTO", "Goto a named room");
		CtdlRegisterProtoHook(cmd_stat, "STAT", "Get mtime of the current room");
		CtdlRegisterProtoHook(cmd_whok, "WHOK", "List users who know this room");
		CtdlRegisterProtoHook(cmd_rdir, "RDIR", "List files in room directory");
		CtdlRegisterProtoHook(cmd_getr, "GETR", "Get room parameters");
		CtdlRegisterProtoHook(cmd_setr, "SETR", "Set room parameters");
		CtdlRegisterProtoHook(cmd_geta, "GETA", "Get the room admin name");
		CtdlRegisterProtoHook(cmd_seta, "SETA", "Set the room admin for this room");
		CtdlRegisterProtoHook(cmd_rinf, "RINF", "Fetch room info file");
		CtdlRegisterProtoHook(cmd_kill, "KILL", "Kill (delete) the current room");
		CtdlRegisterProtoHook(cmd_cre8, "CRE8", "Create a new room");
		CtdlRegisterProtoHook(cmd_einf, "EINF", "Enter info file for the current room");
		CtdlRegisterProtoHook(cmd_lflr, "LFLR", "List all known floors");
		CtdlRegisterProtoHook(cmd_cflr, "CFLR", "Create a new floor");
		CtdlRegisterProtoHook(cmd_kflr, "KFLR", "Kill a floor");
		CtdlRegisterProtoHook(cmd_eflr, "EFLR", "Edit a floor");
	}
        /* return our Subversion id for the Log */
	return "rooms";
}
