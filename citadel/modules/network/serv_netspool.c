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
#include "file_ops.h"
#include "citadel_dirs.h"
#include "threads.h"
#include "context.h"
#include "netconfig.h"
#include "netspool.h"
#include "netmail.h"
#include "ctdl_module.h"


HashList *CfgTypeHash = NULL;
typedef struct CfgLineType CfgLineType;
typedef void (*CfgLineParser)(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc);
typedef void (*CfgLineSerializer)(const CfgLineType *ThisOne, StrBuf *OuptputBuffer, SpoolControl *sc, namelist *data);
typedef void (*CfgLineDeAllocator)(const CfgLineType *ThisOne, namelist **data);
struct CfgLineType {
	RoomNetCfg C;
	CfgLineParser Parser;
	CfgLineSerializer Serializer;
	CfgLineDeAllocator DeAllocator;
	ConstStr Str;
	int IsSingleLine;
};
#define REGISTERRoomCfgType(a, p, uniq, s, d) RegisterRoomCfgType(#a, sizeof(#a) - 1, a, p, uniq, s, d);

void RegisterRoomCfgType(const char* Name, long len, RoomNetCfg eCfg, CfgLineParser p, int uniq, CfgLineSerializer s, CfgLineDeAllocator d)
{
	CfgLineType *pCfg;

	pCfg = (CfgLineType*) malloc(sizeof(CfgLineType));
	pCfg->Parser = p;
	pCfg->Serializer = s;
	pCfg->C = eCfg;
	pCfg->Str.Key = Name;
	pCfg->Str.len = len;
	pCfg->IsSingleLine = uniq;

	if (CfgTypeHash == NULL)
		CfgTypeHash = NewHash(1, NULL);
	Put(CfgTypeHash, Name, len, pCfg, NULL);
}

const CfgLineType *GetCfgTypeByStr(const char *Key, long len)
{
	void *pv;
	
	if (GetHash(CfgTypeHash, Key, len, &pv) && (pv != NULL))
	{
		return (const CfgLineType *) pv;
	}
	else
	{
		return NULL;
	}
}

const CfgLineType *GetCfgTypeByEnum(RoomNetCfg eCfg, HashPos *It)
{
	const char *Key;
	long len;
	void *pv;
	CfgLineType *pCfg;

	RewindHashPos(CfgTypeHash, It, 1);
	while (GetNextHashPos(CfgTypeHash, It, &len, &Key, &pv) && (pv != NULL))
	{
		pCfg = (CfgLineType*) pv;
		if (pCfg->C == eCfg)
			return pCfg;
	}
	return NULL;
}
void ParseDefault(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{
	namelist *nptr;

	nptr = (namelist *)
		malloc(sizeof(namelist));
	nptr->next = sc->NetConfigs[ThisOne->C];
	nptr->Value = NewStrBufPlain(LinePos, StrLength(Line) - ( LinePos - ChrPtr(Line)) );
	sc->NetConfigs[ThisOne->C] = nptr;
}

void ParseLastSent(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{
	sc->lastsent = extract_long(LinePos, 0);
}
void ParseIgnetPushShare(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{
/*
	extract_token(nodename, LinePos, 0, '|', sizeof nodename);
	extract_token(roomname, LinePos, 1, '|', sizeof roomname);
	mptr = (maplist *) malloc(sizeof(maplist));
	mptr->next = sc->ignet_push_shares;
	strcpy(mptr->remote_nodename, nodename);
	strcpy(mptr->remote_roomname, roomname);
	sc->ignet_push_shares = mptr;
*/
}

void ParseRoomAlias(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{
/*
	if (sc->sender != NULL)
		continue; / * just one alowed... * /
	extract_token(nptr->name, buf, 1, '|', sizeof nptr->name);
	sc->sender = nptr;
*/
}

void ParseSubPendingLine(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{

	if (time(NULL) - extract_long(LinePos, 3) > EXP) {
		//	skipthisline = 1;
	}
	else
	{
	}

}
void ParseUnSubPendingLine(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, SpoolControl *sc)
{
	///int skipthisline = 0;
	if (time(NULL) - extract_long(LinePos, 2) > EXP) {
		//	skipthisline = 1;
	}

}

void DeleteGenericCfgLine(const CfgLineType *ThisOne, namelist **data)
{
	FreeStrBuf(&(*data)->Value);
	free(*data);
	*data = NULL;
}
int read_spoolcontrol_file(SpoolControl **scc, char *filename)
{
	int fd;
	const char *ErrStr = NULL;
	const char *Pos;
	const CfgLineType *pCfg;
	StrBuf *Line;
	StrBuf *InStr;
	SpoolControl *sc;

	fd = open(filename, O_NONBLOCK|O_RDONLY);
	if (fd == -1) {
		*scc = NULL;
		return 0;
	}
	sc = malloc(sizeof(SpoolControl));
	memset(sc, 0, sizeof(SpoolControl));
	*scc = sc;

	while (StrBufTCP_read_line(Line, &fd, 0, &ErrStr) >= 0) {
		if (StrLength(Line) == 0)
			continue;
		Pos = NULL;
		InStr = NewStrBufPlain(NULL, StrLength(Line));
		StrBufExtract_NextToken(InStr, Line, &Pos, '|');

		pCfg = GetCfgTypeByStr(SKEY(InStr));
		if (pCfg != NULL)
		{
			pCfg->Parser(pCfg, Line, Pos, sc);
		}
		else
		{
			if (sc->misc == NULL)
			{
				sc->misc = NewStrBufDup(Line);
			}
			else
			{
				if(StrLength(sc->misc) > 0)
					StrBufAppendBufPlain(sc->misc, HKEY("\n"), 0);
				StrBufAppendBuf(sc->misc, Line, 0);
			}
		}
	}
	if (fd > 0)
		close(fd);
	FreeStrBuf(&InStr);
	FreeStrBuf(&Line);
	return 1;
}

void SerializeLastSent(const CfgLineType *ThisOne, StrBuf *OutputBuffer, SpoolControl *sc, namelist *data)
{
	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendPrintf(OutputBuffer, "|%ld\n", sc->lastsent);
}

void SerializeGeneric(const CfgLineType *ThisOne, StrBuf *OutputBuffer, SpoolControl *sc, namelist *data)
{
	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendBuf(OutputBuffer, data->Value, 0);
	StrBufAppendBufPlain(OutputBuffer, HKEY("\n"), 0);
}
void SerializeIgnetPushShare(const CfgLineType *ThisOne, StrBuf *OutputBuffer, SpoolControl *sc, namelist *data)
{
	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
/*
			StrBufAppendPrintf(Cfg, "ignet_push_share|%s", sc->ignet_push_shares->remote_nodename);
			if (!IsEmptyStr(sc->ignet_push_shares->remote_roomname)) {
				StrBufAppendPrintf(Cfg, "|%s", sc->ignet_push_shares->remote_roomname);
			}
			StrBufAppendPrintf(Cfg, "\n");
			mptr = sc->ignet_push_shares->next;
			free(sc->ignet_push_shares);
			sc->ignet_push_shares = mptr;
*/
	StrBufAppendBuf(OutputBuffer, data->Value, 0);
	StrBufAppendBufPlain(OutputBuffer, HKEY("\n"), 0);
}

int save_spoolcontrol_file(SpoolControl *sc, char *filename)
{
	RoomNetCfg eCfg;
	StrBuf *Cfg;
	char tempfilename[PATH_MAX];
	int TmpFD;
	long len;
	time_t unixtime;
	struct timeval tv;
	long reltid; /* if we don't have SYS_gettid, use "random" value */
	StrBuf *OutBuffer;
	int rc;
	HashPos *CfgIt;

	len = strlen(filename);
	memcpy(tempfilename, filename, len + 1);


#if defined(HAVE_SYSCALL_H) && defined (SYS_gettid)
	reltid = syscall(SYS_gettid);
#endif
	gettimeofday(&tv, NULL);
	/* Promote to time_t; types differ on some OSes (like darwin) */
	unixtime = tv.tv_sec;

	sprintf(tempfilename + len, ".%ld-%ld", reltid, unixtime);
	errno = 0;
	TmpFD = open(tempfilename, O_CREAT|O_EXCL|O_RDWR, S_IRUSR|S_IWUSR);
	Cfg = NewStrBuf();
	if ((TmpFD < 0) || (errno != 0)) {
		syslog(LOG_CRIT, "ERROR: cannot open %s: %s\n",
			filename, strerror(errno));
		unlink(tempfilename);
		return 0;
	}
	else {
		CfgIt = GetNewHashPos(CfgTypeHash, 1);
		fchown(TmpFD, config.c_ctdluid, 0);
		for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
		{
			const CfgLineType *pCfg;
			pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
			if (pCfg->IsSingleLine)
			{
				pCfg->Serializer(pCfg, OutBuffer, sc, NULL);
			}
			else
			{
				namelist *pName = sc->NetConfigs[pCfg->C];
				while (pName != NULL)
				{
					pCfg->Serializer(pCfg, OutBuffer, sc, pName);
					pName = pName->next;
				}
				
				
			}

		}
		DeleteHashPos(&CfgIt);


		if (sc->misc != NULL) {
			StrBufAppendBuf(OutBuffer, sc->misc, 0);
		}

		rc = write(TmpFD, ChrPtr(OutBuffer), StrLength(OutBuffer));
		if ((rc >=0 ) && (rc == StrLength(Cfg))) 
		{
			close(TmpFD);
			rename(tempfilename, filename);
			rc = 1;
		}
		else {
			syslog(LOG_EMERG, 
				      "unable to write %s; [%s]; not enough space on the disk?\n", 
				      tempfilename, 
				      strerror(errno));
			close(TmpFD);
			unlink(tempfilename);
			rc = 0;
		}
		FreeStrBuf(&OutBuffer);
		
	}
	return rc;
}



void free_spoolcontrol_struct(SpoolControl **scc)
{
	RoomNetCfg eCfg;
	HashPos *CfgIt;
	SpoolControl *sc;

	sc = *scc;
	CfgIt = GetNewHashPos(CfgTypeHash, 1);
	for (eCfg = subpending; eCfg < maxRoomNetCfg; eCfg ++)
	{
		const CfgLineType *pCfg;
		namelist *pNext, *pName;
		
		pCfg = GetCfgTypeByEnum(eCfg, CfgIt);
		pName= sc->NetConfigs[pCfg->C];
		while (pName != NULL)
		{
			pNext = pName->next;
			pCfg->DeAllocator(pCfg, &pName);
			pName = pNext;
		}
	}
	DeleteHashPos(&CfgIt);

	FreeStrBuf(&sc->Sender);
	FreeStrBuf(&sc->RoomInfo);
	FreeStrBuf(&sc->misc);
	free(sc);
	*scc=NULL;
}

#if 0
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
#endif
int is_recipient(SpoolControl *sc, const char *Name)
{
	const RoomNetCfg RecipientCfgs[] = {
		listrecp,
		digestrecp,
		participate,
		maxRoomNetCfg
	};
	int i;
	namelist *nptr;
	size_t len;
	
	len = strlen(Name);
	i = 0;
	while (RecipientCfgs[i] != maxRoomNetCfg)
	{
		nptr = sc->NetConfigs[RecipientCfgs[i]];
		
		while (nptr != NULL)
		{
			if ((StrLength(nptr->Value) == len) && 
			    (!strcmp(Name, ChrPtr(nptr->Value))))
			{
				return 1;
			}
			nptr = nptr->next;
		}
	}
	return 0;
}


/*
 * Batch up and send all outbound traffic from the current room
 */
void network_spoolout_room(RoomProcList *room_to_spool, 		       
			   HashList *working_ignetcfg,
			   HashList *the_netmap)
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
	if (sc->NetConfigs[digestrecp] != NULL) {
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
	//// todo writenfree_spoolcontrol_file(&sc, filename);
	end_critical_section(S_NETCONFIGS);
}

/*
 * Process a buffer containing a single message from a single file
 * from the inbound queue 
 */
void network_process_buffer(char *buffer, long size, HashList *working_ignetcfg, HashList *the_netmap, int *netmap_changed)
{
	struct CitContext *CCC = CC;
	StrBuf *Buf = NULL;
	struct CtdlMessage *msg = NULL;
	long pos;
	int field;
	struct recptypes *recp = NULL;
	char target_room[ROOMNAMELEN];
	struct ser_ret sermsg;
	char *oldpath = NULL;
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
		msg->cm_fields[field] = strdup(&buffer[pos+1]);
		pos = pos + strlen(&buffer[(int)pos]);
	}

	/* Check for message routing */
	if (msg->cm_fields['D'] != NULL) {
		if (strcasecmp(msg->cm_fields['D'], config.c_nodename)) {

			/* route the message */
			Buf = NewStrBufPlain(msg->cm_fields['D'], -1);
			if (is_valid_node(&nexthop, 
					  NULL, 
					  Buf, 
					  working_ignetcfg, 
					  the_netmap) == 0) 
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
				CtdlFreeMessage(msg);
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
			QNM_syslog(LOG_DEBUG, "Bouncing message due to invalid recipient address.\n");
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
void network_do_spoolin(HashList *working_ignetcfg, HashList *the_netmap, int *netmap_changed)
{
	struct CitContext *CCC = CC;
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
#ifdef _DIRENT_HAVE_D_NAMELEN
		d_namelen = filedir_entry->d_namelen;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_namelen = strlen(filedir_entry->d_name);
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
		if (network_talking_to(SKEY(NextHop), NTT_CHECK)) {
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
			network_talking_to(SKEY(NextHop), NTT_ADD);

			infd = open(filename, O_RDONLY);
			if (infd == -1) {
				nFailed++;
				QN_syslog(LOG_ERR,
					  "failed to open %s for reading due to %s; skipping.\n",
					  filename, strerror(errno)
					);
				network_talking_to(SKEY(NextHop), NTT_REMOVE);
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
				network_talking_to(SKEY(NextHop), NTT_REMOVE);
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
			network_talking_to(SKEY(NextHop), NTT_REMOVE);
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
#ifdef _DIRENT_HAVE_D_NAMELEN
		d_namelen = filedir_entry->d_namelen;
		d_type = filedir_entry->d_type;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_namelen = strlen(filedir_entry->d_name);
#endif
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

		i = is_valid_node(&nexthop,
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
//		REGISTERRoomCfgType(subpending, ParseSubPendingLine, 0, SerializeSubPendingLine, DeleteSubPendingLine); /// todo: move this to mailinglist manager
//		REGISTERRoomCfgType(unsubpending, ParseUnSubPendingLine0, SerializeUnSubPendingLine, DeleteUnSubPendingLine); /// todo: move this to mailinglist manager
//		REGISTERRoomCfgType(lastsent, ParseLastSent, 1, SerializeLastSent, DeleteLastSent);
///		REGISTERRoomCfgType(ignet_push_share, ParseIgnetPushShare, 0, SerializeIgnetPushShare, DeleteIgnetPushShare); // todo: move this to the ignet client
		REGISTERRoomCfgType(listrecp, ParseDefault, 0, SerializeGeneric, DeleteGenericCfgLine);
		REGISTERRoomCfgType(digestrecp, ParseDefault, 0, SerializeGeneric, DeleteGenericCfgLine);
		REGISTERRoomCfgType(pop3client, ParseDefault, 0, SerializeGeneric, DeleteGenericCfgLine);/// todo: implement pop3 specific parser
		REGISTERRoomCfgType(rssclient, ParseDefault, 0, SerializeGeneric, DeleteGenericCfgLine); /// todo: implement rss specific parser
		REGISTERRoomCfgType(participate, ParseDefault, 0, SerializeGeneric, DeleteGenericCfgLine);
		REGISTERRoomCfgType(roommailalias, ParseRoomAlias, 0, SerializeGeneric, DeleteGenericCfgLine);

		create_spool_dirs();
//////todo		CtdlRegisterCleanupHook(destroy_network_queue_room);
	}
	return "network_spool";
}
