/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ** NOTE **   A word on the S_NETCONFIGS semaphore:
 * This is a fairly high-level type of critical section.  It ensures that no
 * two threads work on the netconfigs files at the same time.  Since we do
 * so many things inside these, here are the rules:
 *  1. begin_critical_section(S_NETCONFIGS) *before* begin_ any others.
 *  2. Do *not* perform any I/O with the client during these sections.
 *
 */

/*
 * Duration of time (in seconds) after which pending list subscribe/unsubscribe
 * requests that have not been confirmed will be deleted.
 */
#define EXP	259200	/* three days */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
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
#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else 
# if HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"
#include "citadel_dirs.h"
#include "threads.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "context.h"
#include "netconfig.h"
#include "netspool.h"
#include "netmail.h"
#include "ctdl_module.h"

/*
 * Learn topology from path fields
 */
static void network_learn_topology(char *node, char *path, NetMap **the_netmap, int *netmap_changed)
{
	char nexthop[256];
	NetMap *nmptr;

	*nexthop = '\0';

	if (num_tokens(path, '!') < 3) return;
	for (nmptr = *the_netmap; nmptr != NULL; nmptr = nmptr->next) {
		if (!strcasecmp(nmptr->nodename, node)) {
			extract_token(nmptr->nexthop, path, 0, '!', sizeof nmptr->nexthop);
			nmptr->lastcontact = time(NULL);
			(*netmap_changed) ++;
			return;
		}
	}

	/* If we got here then it's not in the map, so add it. */
	nmptr = (NetMap *) malloc(sizeof (NetMap));
	strcpy(nmptr->nodename, node);
	nmptr->lastcontact = time(NULL);
	extract_token(nmptr->nexthop, path, 0, '!', sizeof nmptr->nexthop);
	nmptr->next = *the_netmap;
	*the_netmap = nmptr;
	(*netmap_changed) ++;
}


	

int read_spoolcontrol_file(SpoolControl **scc, char *filename)
{
	FILE *fp;
	char instr[SIZ];
	char buf[SIZ];
	char nodename[256];
	char roomname[ROOMNAMELEN];
	size_t miscsize = 0;
	size_t linesize = 0;
	int skipthisline = 0;
	namelist *nptr = NULL;
	maplist *mptr = NULL;
	SpoolControl *sc;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		return 0;
	}
	sc = malloc(sizeof(SpoolControl));
	memset(sc, 0, sizeof(SpoolControl));
	*scc = sc;

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, strof(lastsent))) {
			sc->lastsent = extract_long(buf, 1);
		}
		else if (!strcasecmp(instr, strof(listrecp))) {
			nptr = (namelist *)
				malloc(sizeof(namelist));
			nptr->next = sc->listrecps;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc->listrecps = nptr;
		}
		else if (!strcasecmp(instr, strof(participate))) {
			nptr = (namelist *)
				malloc(sizeof(namelist));
			nptr->next = sc->participates;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc->participates = nptr;
		}
		else if (!strcasecmp(instr, strof(digestrecp))) {
			nptr = (namelist *)
				malloc(sizeof(namelist));
			nptr->next = sc->digestrecps;
			extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
			sc->digestrecps = nptr;
		}
		else if (!strcasecmp(instr, strof(ignet_push_share))) {
			extract_token(nodename, buf, 1, '|', sizeof nodename);
			extract_token(roomname, buf, 2, '|', sizeof roomname);
			mptr = (maplist *) malloc(sizeof(maplist));
			mptr->next = sc->ignet_push_shares;
			strcpy(mptr->remote_nodename, nodename);
			strcpy(mptr->remote_roomname, roomname);
			sc->ignet_push_shares = mptr;
		}
		else {
			/* Preserve 'other' lines ... *unless* they happen to
			 * be subscribe/unsubscribe pendings with expired
			 * timestamps.
			 */
			skipthisline = 0;
			if (!strncasecmp(buf, strof(subpending)"|", 11)) {
				if (time(NULL) - extract_long(buf, 4) > EXP) {
					skipthisline = 1;
				}
			}
			if (!strncasecmp(buf, strof(unsubpending)"|", 13)) {
				if (time(NULL) - extract_long(buf, 3) > EXP) {
					skipthisline = 1;
				}
			}

			if (skipthisline == 0) {
				linesize = strlen(buf);
				sc->misc = realloc(sc->misc,
					(miscsize + linesize + 2) );
				sprintf(&sc->misc[miscsize], "%s\n", buf);
				miscsize = miscsize + linesize + 1;
			}
		}


	}
	fclose(fp);
	return 1;
}

void free_spoolcontrol_struct(SpoolControl **scc)
{
	SpoolControl *sc;
	namelist *nptr = NULL;
	maplist *mptr = NULL;

	sc = *scc;
	while (sc->listrecps != NULL) {
		nptr = sc->listrecps->next;
		free(sc->listrecps);
		sc->listrecps = nptr;
	}
	/* Do the same for digestrecps */
	while (sc->digestrecps != NULL) {
		nptr = sc->digestrecps->next;
		free(sc->digestrecps);
		sc->digestrecps = nptr;
	}
	/* Do the same for participates */
	while (sc->participates != NULL) {
		nptr = sc->participates->next;
		free(sc->participates);
		sc->participates = nptr;
	}
	while (sc->ignet_push_shares != NULL) {
		mptr = sc->ignet_push_shares->next;
		free(sc->ignet_push_shares);
		sc->ignet_push_shares = mptr;
	}
	free(sc->misc);
	free(sc);
	*scc=NULL;
}

int writenfree_spoolcontrol_file(SpoolControl **scc, char *filename)
{
	char tempfilename[PATH_MAX];
	int TmpFD;
	SpoolControl *sc;
	namelist *nptr = NULL;
	maplist *mptr = NULL;
	long len;
	time_t unixtime;
	struct timeval tv;
	long reltid; /* if we don't have SYS_gettid, use "random" value */
	StrBuf *Cfg;
	int rc;

	len = strlen(filename);
	memcpy(tempfilename, filename, len + 1);


#if defined(HAVE_SYSCALL_H) && defined (SYS_gettid)
	reltid = syscall(SYS_gettid);
#endif
	gettimeofday(&tv, NULL);
	/* Promote to time_t; types differ on some OSes (like darwin) */
	unixtime = tv.tv_sec;

	sprintf(tempfilename + len, ".%ld-%ld", reltid, unixtime);
	sc = *scc;
	errno = 0;
	TmpFD = open(tempfilename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
	Cfg = NewStrBuf();
	if ((TmpFD < 0) || (errno != 0)) {
		syslog(LOG_CRIT, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
		free_spoolcontrol_struct(scc);
		unlink(tempfilename);
	}
	else {
		fchown(TmpFD, config.c_ctdluid, 0);
		StrBufAppendPrintf(Cfg, "lastsent|%ld\n", sc->lastsent);
		
		/* Write out the listrecps while freeing from memory at the
		 * same time.  Am I clever or what?  :)
		 */
		while (sc->listrecps != NULL) {
		    StrBufAppendPrintf(Cfg, "listrecp|%s\n", sc->listrecps->name);
			nptr = sc->listrecps->next;
			free(sc->listrecps);
			sc->listrecps = nptr;
		}
		/* Do the same for digestrecps */
		while (sc->digestrecps != NULL) {
			StrBufAppendPrintf(Cfg, "digestrecp|%s\n", sc->digestrecps->name);
			nptr = sc->digestrecps->next;
			free(sc->digestrecps);
			sc->digestrecps = nptr;
		}
		/* Do the same for participates */
		while (sc->participates != NULL) {
			StrBufAppendPrintf(Cfg, "participate|%s\n", sc->participates->name);
			nptr = sc->participates->next;
			free(sc->participates);
			sc->participates = nptr;
		}
		while (sc->ignet_push_shares != NULL) {
			StrBufAppendPrintf(Cfg, "ignet_push_share|%s", sc->ignet_push_shares->remote_nodename);
			if (!IsEmptyStr(sc->ignet_push_shares->remote_roomname)) {
				StrBufAppendPrintf(Cfg, "|%s", sc->ignet_push_shares->remote_roomname);
			}
			StrBufAppendPrintf(Cfg, "\n");
			mptr = sc->ignet_push_shares->next;
			free(sc->ignet_push_shares);
			sc->ignet_push_shares = mptr;
		}
		if (sc->misc != NULL) {
			StrBufAppendBufPlain(Cfg, sc->misc, -1, 0);
		}
		free(sc->misc);

		rc = write(TmpFD, ChrPtr(Cfg), StrLength(Cfg));
		if ((rc >=0 ) && (rc == StrLength(Cfg))) 
		{
			close(TmpFD);
			rename(tempfilename, filename);
		}
		else {
			syslog(LOG_EMERG, 
				      "unable to write %s; [%s]; not enough space on the disk?\n", 
				      tempfilename, 
				      strerror(errno));
			close(TmpFD);
			unlink(tempfilename);
		}
		FreeStrBuf(&Cfg);
		free(sc);
		*scc=NULL;
	}
	return 1;
}
int is_recipient(SpoolControl *sc, const char *Name)
{
	namelist *nptr;
	size_t len;

	len = strlen(Name);
	nptr = sc->listrecps;
	while (nptr != NULL) {
		if (strncmp(Name, nptr->name, len)==0)
			return 1;
		nptr = nptr->next;
	}
	/* Do the same for digestrecps */
	nptr = sc->digestrecps;
	while (nptr != NULL) {
		if (strncmp(Name, nptr->name, len)==0)
			return 1;
		nptr = nptr->next;
	}
	/* Do the same for participates */
	nptr = sc->participates;
	while (nptr != NULL) {
		if (strncmp(Name, nptr->name, len)==0)
			return 1;
		nptr = nptr->next;
	}
	return 0;
}


/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(RoomProcList *room_to_spool, 		       
			   char *working_ignetcfg,
			   NetMap *the_netmap)
{
	char buf[SIZ];
	char filename[PATH_MAX];
	SpoolControl *sc;
	int i;

	/*
	 * If the room doesn't exist, don't try to perform its networking tasks.
	 * Normally this should never happen, but once in a while maybe a room gets
	 * queued for networking and then deleted before it can happen.
	 */
	if (CtdlGetRoom(&CC->room, room_to_spool->name) != 0) {
		syslog(LOG_CRIT, "ERROR: cannot load <%s>\n", room_to_spool->name);
		return;
	}

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_netcfg_dir);
	begin_critical_section(S_NETCONFIGS);

	/* Only do net processing for rooms that have netconfigs */
	if (!read_spoolcontrol_file(&sc, filename))
	{
		end_critical_section(S_NETCONFIGS);
		return;
	}
	syslog(LOG_INFO, "Networking started for <%s>\n", CC->room.QRname);

	sc->working_ignetcfg = working_ignetcfg;
	sc->the_netmap = the_netmap;

	/* If there are digest recipients, we have to build a digest */
	if (sc->digestrecps != NULL) {
		sc->digestfp = tmpfile();
		fprintf(sc->digestfp, "Content-type: text/plain\n\n");
	}

	/* Do something useful */
	CtdlForEachMessage(MSGS_GT, sc->lastsent, NULL, NULL, NULL,
		network_spool_msg, sc);

	/* If we wrote a digest, deliver it and then close it */
	snprintf(buf, sizeof buf, "room_%s@%s",
		CC->room.QRname, config.c_fqdn);
	for (i=0; buf[i]; ++i) {
		buf[i] = tolower(buf[i]);
		if (isspace(buf[i])) buf[i] = '_';
	}
	if (sc->digestfp != NULL) {
		fprintf(sc->digestfp,	" -----------------------------------"
					"------------------------------------"
					"-------\n"
					"You are subscribed to the '%s' "
					"list.\n"
					"To post to the list: %s\n",
					CC->room.QRname, buf
		);
		network_deliver_digest(sc);	/* deliver and close */
	}

	/* Now rewrite the config file */
	writenfree_spoolcontrol_file(&sc, filename);
	end_critical_section(S_NETCONFIGS);
}

/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size, char *working_ignetcfg, NetMap **the_netmap, int *netmap_changed)
{
	struct CtdlMessage *msg = NULL;
	long pos;
	int field;
	struct recptypes *recp = NULL;
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char *oldpath = NULL;
	char filename[PATH_MAX];
	FILE *fp;
	char nexthop[SIZ];
	unsigned char firstbyte;
	unsigned char lastbyte;

	syslog(LOG_DEBUG, "network_process_buffer() processing %ld bytes\n", size);

	/* Validate just a little bit.  First byte should be FF and * last byte should be 00. */
	firstbyte = buffer[0];
	lastbyte = buffer[size-1];
	if ( (firstbyte != 255) || (lastbyte != 0) ) {
		syslog(LOG_ERR, "Corrupt message ignored.  Length=%ld, firstbyte = %d, lastbyte = %d\n",
			size, firstbyte, lastbyte);
		return;
	}

	/* Set default target room to trash */
	strcpy(target_room, TWITROOM);

	/* Load the message into memory */
	msg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = buffer[1];
	msg->cm_format_type = buffer[2];

	for (pos = 3; pos < size; ++pos) {
		field = buffer[pos];
		msg->cm_fields[field] = strdup(&buffer[pos+1]);
		pos = pos + strlen(&buffer[(int)pos]);
	}

	/* Check for message routing */
	if (msg->cm_fields['D'] != NULL) {
		if (strcasecmp(msg->cm_fields['D'], config.c_nodename)) {

			/* route the message */
			strcpy(nexthop, "");
			if (is_valid_node(nexthop, 
					  NULL, 
					  msg->cm_fields['D'], 
					  working_ignetcfg, 
					  *the_netmap) == 0) 
			{
				/* prepend our node to the path */
				if (msg->cm_fields['P'] != NULL) {
					oldpath = msg->cm_fields['P'];
					msg->cm_fields['P'] = NULL;
				}
				else {
					oldpath = strdup("unknown_user");
				}
				size = strlen(oldpath) + SIZ;
				msg->cm_fields['P'] = malloc(size);
				snprintf(msg->cm_fields['P'], size, "%s!%s",
					config.c_nodename, oldpath);
				free(oldpath);

				/* serialize the message */
				serialize_message(&sermsg, msg);

				/* now send it */
				if (IsEmptyStr(nexthop)) {
					strcpy(nexthop, msg->cm_fields['D']);
				}
				snprintf(filename, 
					sizeof filename,
					"%s/%s@%lx%x",
					ctdl_netout_dir,
					nexthop,
					time(NULL),
					rand()
				);
				syslog(LOG_DEBUG, "Appending to %s\n", filename);
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser, sermsg.len, 1, fp);
					fclose(fp);
				}
				else {
					syslog(LOG_ERR, "%s: %s\n", filename, strerror(errno));
				}
				free(sermsg.ser);
				CtdlFreeMessage(msg);
				return;
			}
			
			else {	/* invalid destination node name */

				network_bounce(msg,
"A message you sent could not be delivered due to an invalid destination node"
" name.  Please check the address and try sending the message again.\n");
				msg = NULL;
				return;

			}
		}
	}

	/*
	 * Check to see if we already have a copy of this message, and
	 * abort its processing if so.  (We used to post a warning to Aide>
	 * every time this happened, but the network is now so densely
	 * connected that it's inevitable.)
	 */
	if (network_usetable(msg) != 0) {
		CtdlFreeMessage(msg);
		return;
	}

	/* Learn network topology from the path */
	if ((msg->cm_fields['N'] != NULL) && (msg->cm_fields['P'] != NULL)) {
		network_learn_topology(msg->cm_fields['N'], 
				       msg->cm_fields['P'], 
				       the_netmap, 
				       netmap_changed);
	}

	/* Is the sending node giving us a very persuasive suggestion about
	 * which room this message should be saved in?  If so, go with that.
	 */
	if (msg->cm_fields['C'] != NULL) {
		safestrncpy(target_room, msg->cm_fields['C'], sizeof target_room);
	}

	/* Otherwise, does it have a recipient?  If so, validate it... */
	else if (msg->cm_fields['R'] != NULL) {
		recp = validate_recipients(msg->cm_fields['R'], NULL, 0);
		if (recp != NULL) if (recp->num_error != 0) {
			network_bounce(msg,
				"A message you sent could not be delivered due to an invalid address.\n"
				"Please check the address and try sending the message again.\n");
			msg = NULL;
			free_recipients(recp);
			syslog(LOG_DEBUG, "Bouncing message due to invalid recipient address.\n");
			return;
		}
		strcpy(target_room, "");	/* no target room if mail */
	}

	/* Our last shot at finding a home for this message is to see if
	 * it has the O field (Originating room) set.
	 */
	else if (msg->cm_fields['O'] != NULL) {
		safestrncpy(target_room, msg->cm_fields['O'], sizeof target_room);
	}

	/* Strip out fields that are only relevant during transit */
	if (msg->cm_fields['D'] != NULL) {
		free(msg->cm_fields['D']);
		msg->cm_fields['D'] = NULL;
	}
	if (msg->cm_fields['C'] != NULL) {
		free(msg->cm_fields['C']);
		msg->cm_fields['C'] = NULL;
	}

	/* save the message into a room */
	if (PerformNetprocHooks(msg, target_room) == 0) {
		msg->cm_flags = CM_SKIP_HOOKS;
		CtdlSubmitMsg(msg, recp, target_room, 0);
	}
	CtdlFreeMessage(msg);
	free_recipients(recp);
}


/*
 * Process a single message from a single file from the inbound queue 
 */
void network_process_message(FILE *fp, 
			     long msgstart, 
			     long msgend,
			     char *working_ignetcfg,
			     NetMap **the_netmap, 
			     int *netmap_changed)
{
	long hold_pos;
	long size;
	char *buffer;

	hold_pos = ftell(fp);
	size = msgend - msgstart + 1;
	buffer = malloc(size);
	if (buffer != NULL) {
		fseek(fp, msgstart, SEEK_SET);
		if (fread(buffer, size, 1, fp) > 0) {
			network_process_buffer(buffer, 
					       size, 
					       working_ignetcfg, 
					       the_netmap, 
					       netmap_changed);
		}
		free(buffer);
	}

	fseek(fp, hold_pos, SEEK_SET);
}


/*
 * Process a single file from the inbound queue 
 */
void network_process_file(char *filename,
			  char *working_ignetcfg,
			  NetMap **the_netmap, 
			  int *netmap_changed)
{
	FILE *fp;
	long msgstart = (-1L);
	long msgend = (-1L);
	long msgcur = 0L;
	int ch;


	fp = fopen(filename, "rb");
	if (fp == NULL) {
		syslog(LOG_CRIT, "Error opening %s: %s\n", filename, strerror(errno));
		return;
	}

	fseek(fp, 0L, SEEK_END);
	syslog(LOG_INFO, "network: processing %ld bytes from %s\n", ftell(fp), filename);
	rewind(fp);

	/* Look for messages in the data stream and break them out */
	while (ch = getc(fp), ch >= 0) {
	
		if (ch == 255) {
			if (msgstart >= 0L) {
				msgend = msgcur - 1;
				network_process_message(fp,
							msgstart,
							msgend,
							working_ignetcfg,
							the_netmap,
							netmap_changed);
			}
			msgstart = msgcur;
		}

		++msgcur;
	}

	msgend = msgcur - 1;
	if (msgstart >= 0L) {
		network_process_message(fp,
					msgstart,
					msgend,
					working_ignetcfg,
					the_netmap,
					netmap_changed);
	}

	fclose(fp);
	unlink(filename);
}


/*
 * Process anything in the inbound queue
 */
void network_do_spoolin(char *working_ignetcfg, NetMap **the_netmap, int *netmap_changed)
{
	DIR *dp;
	struct dirent *d;
	struct stat statbuf;
	char filename[PATH_MAX];
	static time_t last_spoolin_mtime = 0L;

	/*
	 * Check the spoolin directory's modification time.  If it hasn't
	 * been touched, we don't need to scan it.
	 */
	if (stat(ctdl_netin_dir, &statbuf)) return;
	if (statbuf.st_mtime == last_spoolin_mtime) {
		syslog(LOG_DEBUG, "network: nothing in inbound queue\n");
		return;
	}
	last_spoolin_mtime = statbuf.st_mtime;
	syslog(LOG_DEBUG, "network: processing inbound queue\n");

	/*
	 * Ok, there's something interesting in there, so scan it.
	 */
	dp = opendir(ctdl_netin_dir);
	if (dp == NULL) return;

	while (d = readdir(dp), d != NULL) {
		if ((strcmp(d->d_name, ".")) && (strcmp(d->d_name, ".."))) {
			snprintf(filename, 
				sizeof filename,
				"%s/%s",
				ctdl_netin_dir,
				d->d_name
			);
			network_process_file(filename,
					     working_ignetcfg,
					     the_netmap,
					     netmap_changed);
		}
	}

	closedir(dp);
}

/*
 * Step 1: consolidate files in the outbound queue into one file per neighbor node
 * Step 2: delete any files in the outbound queue that were for neighbors who no longer exist.
 */
void network_consolidate_spoolout(char *working_ignetcfg, NetMap *the_netmap)
{
	DIR *dp;
	struct dirent *d;
	char filename[PATH_MAX];
	char cmd[PATH_MAX];
	char nexthop[256];
	long nexthoplen;
	int i;
	char *ptr;

	/* Step 1: consolidate files in the outbound queue into one file per neighbor node */
	dp = opendir(ctdl_netout_dir);
	if (dp == NULL) return;
	while (d = readdir(dp), d != NULL) {
		if (
			(strcmp(d->d_name, "."))
			&& (strcmp(d->d_name, ".."))
			&& (strchr(d->d_name, '@') != NULL)
		) {
			nexthoplen = safestrncpy(nexthop, d->d_name, sizeof nexthop);
			ptr = strchr(nexthop, '@');
			if (ptr) {
				*ptr = 0;
				nexthoplen = ptr - nexthop;
			}				
	
			snprintf(filename, 
				sizeof filename,
				"%s/%s",
				ctdl_netout_dir,
				d->d_name
			);
	
			syslog(LOG_DEBUG, "Consolidate %s to %s\n", filename, nexthop);
			if (network_talking_to(nexthop, nexthoplen, NTT_CHECK)) {
				syslog(LOG_DEBUG,
					"Currently online with %s - skipping for now\n",
					nexthop
				);
			}
			else {
				network_talking_to(nexthop, nexthoplen, NTT_ADD);
				snprintf(cmd, sizeof cmd, "/bin/cat %s >>%s/%s && /bin/rm -f %s",
					filename,
					ctdl_netout_dir, nexthop,
					filename
				);
				system(cmd);
				network_talking_to(nexthop, nexthoplen, NTT_REMOVE);
			}
		}
	}
	closedir(dp);

	/* Step 2: delete any files in the outbound queue that were for neighbors who no longer exist */

	dp = opendir(ctdl_netout_dir);
	if (dp == NULL) return;

	while (d = readdir(dp), d != NULL) {
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;

		snprintf(filename, 
			sizeof filename,
			"%s/%s",
			ctdl_netout_dir,
			d->d_name
		);

		strcpy(nexthop, "");
		i = is_valid_node(nexthop,
				  NULL,
				  d->d_name,
				  working_ignetcfg,
				  the_netmap);
	
		if ( (i != 0) || !IsEmptyStr(nexthop) ) {
			unlink(filename);
		}
	}


	closedir(dp);
}




/*
 * It's ok if these directories already exist.  Just fail silently.
 */
void create_spool_dirs(void) {
	if ((mkdir(ctdl_spool_dir, 0700) != 0) && (errno != EEXIST))
		syslog(LOG_EMERG, "unable to create directory [%s]: %s", ctdl_spool_dir, strerror(errno));
	if (chown(ctdl_spool_dir, CTDLUID, (-1)) != 0)
		syslog(LOG_EMERG, "unable to set the access rights for [%s]: %s", ctdl_spool_dir, strerror(errno));
	if ((mkdir(ctdl_netin_dir, 0700) != 0) && (errno != EEXIST))
		syslog(LOG_EMERG, "unable to create directory [%s]: %s", ctdl_netin_dir, strerror(errno));
	if (chown(ctdl_netin_dir, CTDLUID, (-1)) != 0)
		syslog(LOG_EMERG, "unable to set the access rights for [%s]: %s", ctdl_netin_dir, strerror(errno));
	if ((mkdir(ctdl_nettmp_dir, 0700) != 0) && (errno != EEXIST))
		syslog(LOG_EMERG, "unable to create directory [%s]: %s", ctdl_nettmp_dir, strerror(errno));
	if (chown(ctdl_nettmp_dir, CTDLUID, (-1)) != 0)
		syslog(LOG_EMERG, "unable to set the access rights for [%s]: %s", ctdl_nettmp_dir, strerror(errno));
	if ((mkdir(ctdl_netout_dir, 0700) != 0) && (errno != EEXIST))
		syslog(LOG_EMERG, "unable to create directory [%s]: %s", ctdl_netout_dir, strerror(errno));
	if (chown(ctdl_netout_dir, CTDLUID, (-1)) != 0)
		syslog(LOG_EMERG, "unable to set the access rights for [%s]: %s", ctdl_netout_dir, strerror(errno));
}

/*
 * Module entry point
 */
CTDL_MODULE_INIT(network_spool)
{
	if (!threading)
	{
		create_spool_dirs();
//////todo		CtdlRegisterCleanupHook(destroy_network_queue_room);
	}
	return "network_spool";
}
