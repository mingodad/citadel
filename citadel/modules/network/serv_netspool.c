/*
 * This module handles shared rooms, inter-Citadel mail, and outbound
 * mailing list processing.
 *
 * Copyright (c) 2000-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
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
#include "citadel_dirs.h"
#include "threads.h"
#include "context.h"

#include "ctdl_module.h"

#include "netspool.h"
#include "netmail.h"


#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif


void ParseLastSent(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCFG)
{
	RoomNetCfgLine *nptr;
	nptr = (RoomNetCfgLine *)
		malloc(sizeof(RoomNetCfgLine));
	memset(nptr, 0, sizeof(RoomNetCfgLine));
	OneRNCFG->lastsent = extract_long(LinePos, 0);
	OneRNCFG->NetConfigs[ThisOne->C] = nptr;
}

void ParseRoomAlias(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *rncfg)
{
	if (rncfg->Sender != NULL)
		return;

	ParseGeneric(ThisOne, Line, LinePos, rncfg);
	rncfg->Sender = NewStrBufDup(rncfg->NetConfigs[roommailalias]->Value[0]);
}

void ParseSubPendingLine(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCFG)
{
	if (time(NULL) - extract_long(LinePos, 3) > EXP) 
		return; /* expired subscription... */

	ParseGeneric(ThisOne, Line, LinePos, OneRNCFG);
}
void ParseUnSubPendingLine(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCFG)
{
	if (time(NULL) - extract_long(LinePos, 2) > EXP)
		return; /* expired subscription... */

	ParseGeneric(ThisOne, Line, LinePos, OneRNCFG);
}


void SerializeLastSent(const CfgLineType *ThisOne, StrBuf *OutputBuffer, OneRoomNetCfg *RNCfg, RoomNetCfgLine *data)
{
	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendPrintf(OutputBuffer, "|%ld\n", RNCfg->lastsent);
}

void DeleteLastSent(const CfgLineType *ThisOne, RoomNetCfgLine **data)
{
	free(*data);
	*data = NULL;
}

static const RoomNetCfg SpoolCfgs [4] = {
	listrecp,
	digestrecp,
	participate,
	ignet_push_share
};

static const long SpoolCfgsCopyN [4] = {
	1, 1, 1, 2
};

int HaveSpoolConfig(OneRoomNetCfg* RNCfg)
{
	int i;
	int interested = 0;
	for (i=0; i < 4; i++) if (RNCfg->NetConfigs[SpoolCfgs[i]] == NULL) interested = 1;
	return interested;
}

void Netmap_AddMe(struct CtdlMessage *msg, const char *defl, long defllen)
{
	long node_len;
	char buf[SIZ];

	/* prepend our node to the path */
	if (CM_IsEmpty(msg, eMessagePath)) {
		CM_SetField(msg, eMessagePath, defl, defllen);
	}
	node_len = strlen(config.c_nodename);
	if (node_len >= SIZ) 
		node_len = SIZ - 1;
	memcpy(buf, config.c_nodename, node_len);
	buf[node_len] = '!';
	buf[node_len + 1] = '\0';
	CM_PrependToField(msg, eMessagePath, buf, node_len + 1);
}

void InspectQueuedRoom(SpoolControl **pSC,
		       RoomProcList *room_to_spool,     
		       HashList *working_ignetcfg,
		       HashList *the_netmap)
{
	SpoolControl *sc;
	int i = 0;

	sc = (SpoolControl*)malloc(sizeof(SpoolControl));
	memset(sc, 0, sizeof(SpoolControl));
	sc->RNCfg = room_to_spool->OneRNCfg;
	sc->lastsent = room_to_spool->lastsent;
	sc->working_ignetcfg = working_ignetcfg;
	sc->the_netmap = the_netmap;

	/*
	 * If the room doesn't exist, don't try to perform its networking tasks.
	 * Normally this should never happen, but once in a while maybe a room gets
	 * queued for networking and then deleted before it can happen.
	 */
	if (CtdlGetRoom(&sc->room, room_to_spool->name) != 0) {
		syslog(LOG_CRIT, "ERROR: cannot load <%s>\n", room_to_spool->name);
		free(sc);
		return;
	}
	if (sc->room.QRhighest <= sc->lastsent)
	{
		syslog(LOG_DEBUG, "nothing to do for <%s>\n", room_to_spool->name);
		free(sc);
		return;
	}

	if (sc->RNCfg == NULL)
		sc->RNCfg = CtdlGetNetCfgForRoom(sc->room.QRnumber);

	if (!HaveSpoolConfig(sc->RNCfg))
	{
		free(sc);
		/* nothing to do for this room... */
		return;
	}

	/* Now lets remember whats needed for the actual work... */

	for (i=0; i < 4; i++)
	{
		aggregate_recipients(&sc->Users[SpoolCfgs[i]],
				     SpoolCfgs[i],
				     sc->RNCfg,
				     SpoolCfgsCopyN[i]);
	}
	
	if (StrLength(sc->RNCfg->Sender) > 0)
		sc->Users[roommailalias] = NewStrBufDup(sc->RNCfg->Sender);

	sc->next = *pSC;
	*pSC = sc;

}

void CalcListID(SpoolControl *sc)
{
	StrBuf *RoomName;
	const char *err;
	int fd;
	struct CitContext *CCC = CC;
	char filename[PATH_MAX];
#define MAX_LISTIDLENGTH 150

	assoc_file_name(filename, sizeof filename, &sc->room, ctdl_info_dir);
	fd = open(filename, 0);

	if (fd > 0) {
		struct stat stbuf;

		if ((fstat(fd, &stbuf) == 0) &&
		    (stbuf.st_size > 0))
		{
			sc->RoomInfo = NewStrBufPlain(NULL, stbuf.st_size + 1);
			StrBufReadBLOB(sc->RoomInfo, &fd, 0, stbuf.st_size, &err);
		}
		close(fd);
	}

	sc->ListID = NewStrBufPlain(NULL, 1024);
	if (StrLength(sc->RoomInfo) > 0)
	{
		const char *Pos = NULL;
		StrBufSipLine(sc->ListID, sc->RoomInfo, &Pos);

		if (StrLength(sc->ListID) > MAX_LISTIDLENGTH)
		{
			StrBufCutAt(sc->ListID, MAX_LISTIDLENGTH, NULL);
			StrBufAppendBufPlain(sc->ListID, HKEY("..."), 0);
		}
		StrBufAsciify(sc->ListID, ' ');
	}
	else
	{
		StrBufAppendBufPlain(sc->ListID, CCC->room.QRname, -1, 0);
	}

	StrBufAppendBufPlain(sc->ListID, HKEY("<"), 0);
	RoomName = NewStrBufPlain (sc->room.QRname, -1);
	StrBufAsciify(RoomName, '_');
	StrBufReplaceChars(RoomName, ' ', '_');

	if (StrLength(sc->Users[roommailalias]) > 0)
	{
		long Pos;
		const char *AtPos;

		Pos = StrLength(sc->ListID);
		StrBufAppendBuf(sc->ListID, sc->Users[roommailalias], 0);
		AtPos = strchr(ChrPtr(sc->ListID) + Pos, '@');

		if (AtPos != NULL)
		{
			StrBufPeek(sc->ListID, AtPos, 0, '.');
		}
	}
	else
	{
		StrBufAppendBufPlain(sc->ListID, HKEY("room_"), 0);
		StrBufAppendBuf(sc->ListID, RoomName, 0);
		StrBufAppendBufPlain(sc->ListID, HKEY("."), 0);
		StrBufAppendBufPlain(sc->ListID, config.c_fqdn, -1, 0);
		/*
		 * this used to be:
		 * roomname <Room-Number.list-id.fqdn>
		 * according to rfc2919.txt it only has to be a uniq identifier
		 * under the domain of the system; 
		 * in general MUAs use it to calculate the reply address nowadays.
		 */
	}
	StrBufAppendBufPlain(sc->ListID, HKEY(">"), 0);

	if (StrLength(sc->Users[roommailalias]) == 0)
	{
		sc->Users[roommailalias] = NewStrBuf();
		
		StrBufAppendBufPlain(sc->Users[roommailalias], HKEY("room_"), 0);
		StrBufAppendBuf(sc->Users[roommailalias], RoomName, 0);
		StrBufAppendBufPlain(sc->Users[roommailalias], HKEY("@"), 0);
		StrBufAppendBufPlain(sc->Users[roommailalias], config.c_fqdn, -1, 0);

		StrBufLowerCase(sc->Users[roommailalias]);
	}

	FreeStrBuf(&RoomName);
}


/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(SpoolControl *sc)
{
	struct CitContext *CCC = CC;
	char buf[SIZ];
	int i;
	long lastsent;

	/*
	 * If the room doesn't exist, don't try to perform its networking tasks.
	 * Normally this should never happen, but once in a while maybe a room gets
	 * queued for networking and then deleted before it can happen.
	 */
	memcpy (&CCC->room, &sc->room, sizeof(ctdlroom));

	syslog(LOG_INFO, "Networking started for <%s>\n", CCC->room.QRname);

	/* If there are digest recipients, we have to build a digest */
	if (sc->Users[digestrecp] != NULL) {
		sc->digestfp = tmpfile();
		fprintf(sc->digestfp, "Content-type: text/plain\n\n");
	}

	CalcListID(sc);

	/* remember where we started... */
	lastsent = sc->lastsent;

	/* Do something useful */
	CtdlForEachMessage(MSGS_GT, sc->lastsent, NULL, NULL, NULL,
		network_spool_msg, sc);

	/* If we wrote a digest, deliver it and then close it */
	if (StrLength(sc->Users[roommailalias]) > 0)
	{
		long len;
		len = StrLength(sc->Users[roommailalias]);
		if (len + 1 > sizeof(buf))
			len = sizeof(buf) - 1;
		memcpy(buf, ChrPtr(sc->Users[roommailalias]), len);
		buf[len] = '\0';
	}
	else
	{
		snprintf(buf, sizeof buf, "room_%s@%s",
			 CCC->room.QRname, config.c_fqdn);
	}

	for (i=0; buf[i]; ++i) {
		buf[i] = tolower(buf[i]);
		if (isspace(buf[i])) buf[i] = '_';
	}
	if (sc->digestfp != NULL) {
		fprintf(sc->digestfp,
			" -----------------------------------"
			"------------------------------------"
			"-------\n"
			"You are subscribed to the '%s' "
			"list.\n"
			"To post to the list: %s\n",
			CCC->room.QRname, buf
		);
		network_deliver_digest(sc);	/* deliver and close */
	}

	/* Now rewrite the config file */
	if (sc->lastsent != lastsent)
	{
		sc->RNCfg = CtdlGetNetCfgForRoom(sc->room.QRnumber);

		sc->RNCfg->lastsent = sc->lastsent;
		sc->RNCfg->changed = 1;
	}
	end_critical_section(S_NETCONFIGS);
}

/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size, HashList *working_ignetcfg, HashList *the_netmap, int *netmap_changed)
{
	long len;
	struct CitContext *CCC = CC;
	StrBuf *Buf = NULL;
	struct CtdlMessage *msg = NULL;
	long pos;
	int field;
	recptypes *recp = NULL;
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char filename[PATH_MAX];
	FILE *fp;
	const StrBuf *nexthop = NULL;
	unsigned char firstbyte;
	unsigned char lastbyte;

	QN_syslog(LOG_DEBUG, "network_process_buffer() processing %ld bytes\n", size);

	/* Validate just a little bit.  First byte should be FF and * last byte should be 00. */
	firstbyte = buffer[0];
	lastbyte = buffer[size-1];
	if ( (firstbyte != 255) || (lastbyte != 0) ) {
		QN_syslog(LOG_ERR, "Corrupt message ignored.  Length=%ld, firstbyte = %d, lastbyte = %d\n",
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
		len = strlen(buffer + pos + 1);
		CM_SetField(msg, field, buffer + pos + 1, len);
		pos = pos + len + 1;
	}

	/* Check for message routing */
	if (!CM_IsEmpty(msg, eDestination)) {
		if (strcasecmp(msg->cm_fields[eDestination], config.c_nodename)) {

			/* route the message */
			Buf = NewStrBufPlain(CM_KEY(msg,eDestination));
			if (CtdlIsValidNode(&nexthop, 
					    NULL, 
					    Buf, 
					    working_ignetcfg, 
					    the_netmap) == 0) 
			{
				Netmap_AddMe(msg, HKEY("unknown_user"));

				/* serialize the message */
				CtdlSerializeMessage(&sermsg, msg);

				/* now send it */
				if (StrLength(nexthop) == 0) {
					nexthop = Buf;
				}
				snprintf(filename,
					 sizeof filename,
					 "%s/%s@%lx%x",
					 ctdl_netout_dir,
					 ChrPtr(nexthop),
					 time(NULL),
					 rand()
				);
				QN_syslog(LOG_DEBUG, "Appending to %s\n", filename);
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser, sermsg.len, 1, fp);
					fclose(fp);
				}
				else {
					QN_syslog(LOG_ERR, "%s: %s\n", filename, strerror(errno));
				}
				free(sermsg.ser);
				CM_Free(msg);
				FreeStrBuf(&Buf);
				return;
			}
			
			else {	/* invalid destination node name */
				FreeStrBuf(&Buf);

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
		CM_Free(msg);
		return;
	}

	/* Learn network topology from the path */
	if (!CM_IsEmpty(msg, eNodeName) && !CM_IsEmpty(msg, eMessagePath)) {
		NetworkLearnTopology(msg->cm_fields[eNodeName], 
				     msg->cm_fields[eMessagePath], 
				     the_netmap, 
				     netmap_changed);
	}

	/* Is the sending node giving us a very persuasive suggestion about
	 * which room this message should be saved in?  If so, go with that.
	 */
	if (!CM_IsEmpty(msg, eRemoteRoom)) {
		safestrncpy(target_room, msg->cm_fields[eRemoteRoom], sizeof target_room);
	}

	/* Otherwise, does it have a recipient?  If so, validate it... */
	else if (!CM_IsEmpty(msg, eRecipient)) {
		recp = validate_recipients(msg->cm_fields[eRecipient], NULL, 0);
		if (recp != NULL) if (recp->num_error != 0) {
			network_bounce(msg,
				"A message you sent could not be delivered due to an invalid address.\n"
				"Please check the address and try sending the message again.\n");
			msg = NULL;
			free_recipients(recp);
			QNM_syslog(LOG_DEBUG, "Bouncing message due to invalid recipient address.\n");
			return;
		}
		strcpy(target_room, "");	/* no target room if mail */
	}

	/* Our last shot at finding a home for this message is to see if
	 * it has the eOriginalRoom (O) field (Originating room) set.
	 */
	else if (!CM_IsEmpty(msg, eOriginalRoom)) {
		safestrncpy(target_room, msg->cm_fields[eOriginalRoom], sizeof target_room);
	}

	/* Strip out fields that are only relevant during transit */
	CM_FlushField(msg, eDestination);
	CM_FlushField(msg, eRemoteRoom);

	/* save the message into a room */
	if (PerformNetprocHooks(msg, target_room) == 0) {
		msg->cm_flags = CM_SKIP_HOOKS;
		CtdlSubmitMsg(msg, recp, target_room, 0);
	}
	CM_Free(msg);
	free_recipients(recp);
}


/*
 * Process a single message from a single file from the inbound queue 
 */
void network_process_message(FILE *fp, 
			     long msgstart, 
			     long msgend,
			     HashList *working_ignetcfg,
			     HashList *the_netmap, 
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
			  HashList *working_ignetcfg,
			  HashList *the_netmap, 
			  int *netmap_changed)
{
	struct CitContext *CCC = CC;
	FILE *fp;
	long msgstart = (-1L);
	long msgend = (-1L);
	long msgcur = 0L;
	int ch;
	int nMessages = 0;

	fp = fopen(filename, "rb");
	if (fp == NULL) {
		QN_syslog(LOG_CRIT, "Error opening %s: %s\n", filename, strerror(errno));
		return;
	}

	fseek(fp, 0L, SEEK_END);
	QN_syslog(LOG_INFO, "network: processing %ld bytes from %s\n", ftell(fp), filename);
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
		nMessages ++;
	}

	msgend = msgcur - 1;
	if (msgstart >= 0L) {
		network_process_message(fp,
					msgstart,
					msgend,
					working_ignetcfg,
					the_netmap,
					netmap_changed);
		nMessages ++;
	}

	if (nMessages > 0)
		QN_syslog(LOG_INFO,
			  "network: processed %d messages in %s\n",
			  nMessages,
			  filename);

	fclose(fp);
	unlink(filename);
}


/*
 * Process anything in the inbound queue
 */
void network_do_spoolin(HashList *working_ignetcfg, HashList *the_netmap, int *netmap_changed)
{
	struct CitContext *CCC = CC;
	DIR *dp;
	struct dirent *d;
	struct dirent *filedir_entry;
	struct stat statbuf;
	char filename[PATH_MAX];
	static time_t last_spoolin_mtime = 0L;
	int d_type = 0;
        int d_namelen;

	/*
	 * Check the spoolin directory's modification time.  If it hasn't
	 * been touched, we don't need to scan it.
	 */
	if (stat(ctdl_netin_dir, &statbuf)) return;
	if (statbuf.st_mtime == last_spoolin_mtime) {
		QNM_syslog(LOG_DEBUG, "network: nothing in inbound queue\n");
		return;
	}
	last_spoolin_mtime = statbuf.st_mtime;
	QNM_syslog(LOG_DEBUG, "network: processing inbound queue\n");

	/*
	 * Ok, there's something interesting in there, so scan it.
	 */
	dp = opendir(ctdl_netin_dir);
	if (dp == NULL) return;

	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		closedir(dp);
		return;
	}

	while ((readdir_r(dp, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else
		d_type = DT_UNKNOWN;
#endif
		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		if (d_type == DT_UNKNOWN) {
			struct stat s;
			char path[PATH_MAX];

			snprintf(path,
				 PATH_MAX,
				 "%s/%s", 
				 ctdl_netin_dir,
				 filedir_entry->d_name);

			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}

		switch (d_type)
		{
		case DT_DIR:
			break;
		case DT_LNK: /* TODO: check whether its a file or a directory */
		case DT_REG:
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
	free(d);
}

/*
 * Step 1: consolidate files in the outbound queue into one file per neighbor node
 * Step 2: delete any files in the outbound queue that were for neighbors who no longer exist.
 */
void network_consolidate_spoolout(HashList *working_ignetcfg, HashList *the_netmap)
{
	struct CitContext *CCC = CC;
	IOBuffer IOB;
	FDIOBuffer FDIO;
        int d_namelen;
	DIR *dp;
	struct dirent *d;
	struct dirent *filedir_entry;
	const char *pch;
	char spooloutfilename[PATH_MAX];
	char filename[PATH_MAX];
	const StrBuf *nexthop;
	StrBuf *NextHop;
	int i;
	struct stat statbuf;
	int nFailed = 0;
	int d_type = 0;


	/* Step 1: consolidate files in the outbound queue into one file per neighbor node */
	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) 	return;

	dp = opendir(ctdl_netout_dir);
	if (dp == NULL) {
		free(d);
		return;
	}

	NextHop = NewStrBuf();
	memset(&IOB, 0, sizeof(IOBuffer));
	memset(&FDIO, 0, sizeof(FDIOBuffer));
	FDIO.IOB = &IOB;

	while ((readdir_r(dp, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else
		d_type = DT_UNKNOWN;
#endif
		if (d_type == DT_DIR)
			continue;

		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		pch = strchr(filedir_entry->d_name, '@');
		if (pch == NULL)
			continue;

		snprintf(filename, 
			 sizeof filename,
			 "%s/%s",
			 ctdl_netout_dir,
			 filedir_entry->d_name);

		StrBufPlain(NextHop,
			    filedir_entry->d_name,
			    pch - filedir_entry->d_name);

		snprintf(spooloutfilename,
			 sizeof spooloutfilename,
			 "%s/%s",
			 ctdl_netout_dir,
			 ChrPtr(NextHop));

		QN_syslog(LOG_DEBUG, "Consolidate %s to %s\n", filename, ChrPtr(NextHop));
		if (CtdlNetworkTalkingTo(SKEY(NextHop), NTT_CHECK)) {
			nFailed++;
			QN_syslog(LOG_DEBUG,
				  "Currently online with %s - skipping for now\n",
				  ChrPtr(NextHop)
				);
		}
		else {
			size_t dsize;
			size_t fsize;
			int infd, outfd;
			const char *err = NULL;
			CtdlNetworkTalkingTo(SKEY(NextHop), NTT_ADD);

			infd = open(filename, O_RDONLY);
			if (infd == -1) {
				nFailed++;
				QN_syslog(LOG_ERR,
					  "failed to open %s for reading due to %s; skipping.\n",
					  filename, strerror(errno)
					);
				CtdlNetworkTalkingTo(SKEY(NextHop), NTT_REMOVE);
				continue;				
			}
			
			outfd = open(spooloutfilename,
				  O_EXCL|O_CREAT|O_NONBLOCK|O_WRONLY, 
				  S_IRUSR|S_IWUSR);
			if (outfd == -1)
			{
				outfd = open(spooloutfilename,
					     O_EXCL|O_NONBLOCK|O_WRONLY, 
					     S_IRUSR | S_IWUSR);
			}
			if (outfd == -1) {
				nFailed++;
				QN_syslog(LOG_ERR,
					  "failed to open %s for reading due to %s; skipping.\n",
					  spooloutfilename, strerror(errno)
					);
				close(infd);
				CtdlNetworkTalkingTo(SKEY(NextHop), NTT_REMOVE);
				continue;
			}

			dsize = lseek(outfd, 0, SEEK_END);
			lseek(outfd, -dsize, SEEK_SET);

			fstat(infd, &statbuf);
			fsize = statbuf.st_size;
/*
			fsize = lseek(infd, 0, SEEK_END);
*/			
			IOB.fd = infd;
			FDIOBufferInit(&FDIO, &IOB, outfd, fsize + dsize);
			FDIO.ChunkSendRemain = fsize;
			FDIO.TotalSentAlready = dsize;
			err = NULL;
			errno = 0;
			do {} while ((FileMoveChunked(&FDIO, &err) > 0) && (err == NULL));
			if (err == NULL) {
				unlink(filename);
				QN_syslog(LOG_DEBUG,
					  "Spoolfile %s now "SIZE_T_FMT" k\n",
					  spooloutfilename,
					  (dsize + fsize)/1024
					);				
			}
			else {
				nFailed++;
				QN_syslog(LOG_ERR,
					  "failed to append to %s [%s]; rolling back..\n",
					  spooloutfilename, strerror(errno)
					);
				/* whoops partial append?? truncate spooloutfilename again! */
				ftruncate(outfd, dsize);
			}
			FDIOBufferDelete(&FDIO);
			close(infd);
			close(outfd);
			CtdlNetworkTalkingTo(SKEY(NextHop), NTT_REMOVE);
		}
	}
	closedir(dp);

	if (nFailed > 0) {
		FreeStrBuf(&NextHop);
		QN_syslog(LOG_INFO,
			  "skipping Spoolcleanup because of %d files unprocessed.\n",
			  nFailed
			);

		return;
	}

	/* Step 2: delete any files in the outbound queue that were for neighbors who no longer exist */
	dp = opendir(ctdl_netout_dir);
	if (dp == NULL) {
		FreeStrBuf(&NextHop);
		free(d);
		return;
	}

	while ((readdir_r(dp, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else
		d_type = DT_UNKNOWN;
#endif
		if (d_type == DT_DIR)
			continue;

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		pch = strchr(filedir_entry->d_name, '@');
		if (pch == NULL) /* no @ in name? consolidated file. */
			continue;

		StrBufPlain(NextHop,
			    filedir_entry->d_name,
			    pch - filedir_entry->d_name);

		snprintf(filename, 
			sizeof filename,
			"%s/%s",
			ctdl_netout_dir,
			filedir_entry->d_name
		);

		i = CtdlIsValidNode(&nexthop,
				    NULL,
				    NextHop,
				    working_ignetcfg,
				    the_netmap);
	
		if ( (i != 0) || (StrLength(nexthop) > 0) ) {
			unlink(filename);
		}
	}
	FreeStrBuf(&NextHop);
	free(d);
	closedir(dp);
}

void free_spoolcontrol_struct(SpoolControl **sc)
{
	free_spoolcontrol_struct_members(*sc);
	free(*sc);
	*sc = NULL;
}

void free_spoolcontrol_struct_members(SpoolControl *sc)
{
	int i;
	FreeStrBuf(&sc->RoomInfo);
	FreeStrBuf(&sc->ListID);
	for (i = 0; i < maxRoomNetCfg; i++)
		FreeStrBuf(&sc->Users[i]);
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
		CtdlREGISTERRoomCfgType(subpending,       ParseSubPendingLine,   0, 5, SerializeGeneric,  DeleteGenericCfgLine); /// todo: move this to mailinglist manager
		CtdlREGISTERRoomCfgType(unsubpending,     ParseUnSubPendingLine, 0, 4, SerializeGeneric,  DeleteGenericCfgLine); /// todo: move this to mailinglist manager
		CtdlREGISTERRoomCfgType(lastsent,         ParseLastSent,         1, 1, SerializeLastSent, DeleteLastSent);
		CtdlREGISTERRoomCfgType(ignet_push_share, ParseGeneric,          0, 2, SerializeGeneric,  DeleteGenericCfgLine); // [remotenode|remoteroomname (optional)]// todo: move this to the ignet client
		CtdlREGISTERRoomCfgType(listrecp,         ParseGeneric,          0, 1, SerializeGeneric,  DeleteGenericCfgLine);
		CtdlREGISTERRoomCfgType(digestrecp,       ParseGeneric,          0, 1, SerializeGeneric,  DeleteGenericCfgLine);
		CtdlREGISTERRoomCfgType(participate,      ParseGeneric,          0, 1, SerializeGeneric,  DeleteGenericCfgLine);
		CtdlREGISTERRoomCfgType(roommailalias,    ParseRoomAlias,        0, 1, SerializeGeneric,  DeleteGenericCfgLine);

		create_spool_dirs();
//////todo		CtdlRegisterCleanupHook(destroy_network_queue_room);
	}
	return "network_spool";
}
