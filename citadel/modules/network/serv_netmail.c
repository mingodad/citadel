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
#include "ctdl_module.h"
#include "netspool.h"
#include "netmail.h"

void network_deliver_list(struct CtdlMessage *msg, SpoolControl *sc, const char *RoomName);

void aggregate_recipients(StrBuf **recps, RoomNetCfg Which, OneRoomNetCfg *OneRNCfg, long nSegments)
{
	int i;
	size_t recps_len = 0;
	RoomNetCfgLine *nptr;
	struct CitContext *CCC = CC;

	*recps = NULL;
	/*
	 * Figure out how big a buffer we need to allocate
	 */
	for (nptr = OneRNCfg->NetConfigs[Which]; nptr != NULL; nptr = nptr->next) {
		recps_len = recps_len + StrLength(nptr->Value[0]) + 2;
	}

	/* Nothing todo... */
	if (recps_len == 0)
		return;

	*recps = NewStrBufPlain(NULL, recps_len);

	if (*recps == NULL) {
		QN_syslog(LOG_EMERG,
			  "Cannot allocate %ld bytes for recps...\n",
			  (long)recps_len);
		abort();
	}

	/* Each recipient */
	for (nptr = OneRNCfg->NetConfigs[Which]; nptr != NULL; nptr = nptr->next) {
		if (nptr != OneRNCfg->NetConfigs[Which]) {
			for (i = 0; i < nSegments; i++)
				StrBufAppendBufPlain(*recps, HKEY(","), i);
		}
		StrBufAppendBuf(*recps, nptr->Value[0], 0);
		if (Which == ignet_push_share)
		{
			StrBufAppendBufPlain(*recps, HKEY(","), 0);
			StrBufAppendBuf(*recps, nptr->Value[1], 0);

		}
	}
}

static void ListCalculateSubject(struct CtdlMessage *msg)
{
	struct CitContext *CCC = CC;
	StrBuf *Subject, *FlatSubject;
	int rlen;
	char *pCh;

	if (msg->cm_fields['U'] == NULL) {
		Subject = NewStrBufPlain(HKEY("(no subject)"));
	}
	else {
		Subject = NewStrBufPlain(
			msg->cm_fields['U'], -1);
	}
	FlatSubject = NewStrBufPlain(NULL, StrLength(Subject));
	StrBuf_RFC822_to_Utf8(FlatSubject, Subject, NULL, NULL);

	rlen = strlen(CCC->room.QRname);
	pCh  = strstr(ChrPtr(FlatSubject), CCC->room.QRname);
	if ((pCh == NULL) ||
	    (*(pCh + rlen) != ']') ||
	    (pCh == ChrPtr(FlatSubject)) ||
	    (*(pCh - 1) != '[')
		)
	{
		StrBuf *tmp;
		StrBufPlain(Subject, HKEY("["));
		StrBufAppendBufPlain(Subject,
				     CCC->room.QRname,
				     rlen, 0);
		StrBufAppendBufPlain(Subject, HKEY("] "), 0);
		StrBufAppendBuf(Subject, FlatSubject, 0);
		/* so we can free the right one swap them */
		tmp = Subject;
		Subject = FlatSubject;
		FlatSubject = tmp;
		StrBufRFC2047encode(&Subject, FlatSubject);
	}

	if (msg->cm_fields['U'] != NULL)
		free (msg->cm_fields['U']);
	msg->cm_fields['U'] = SmashStrBuf(&Subject);

	FreeStrBuf(&FlatSubject);
}

/*
 * Deliver digest messages
 */
void network_deliver_digest(SpoolControl *sc)
{
	char buf[SIZ];
	struct CtdlMessage *msg = NULL;
	long msglen;
	struct recptypes *valid;
	char bounce_to[256];

	if (sc->Users[listrecp] == NULL)
		return;

	if (sc->num_msgs_spooled < 1) {
		fclose(sc->digestfp);
		sc->digestfp = NULL;
		return;
	}

	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_format_type = FMT_RFC822;
	msg->cm_anon_type = MES_NORMAL;

	sprintf(buf, "%ld", time(NULL));
	msg->cm_fields['T'] = strdup(buf);
	msg->cm_fields['A'] = strdup(CC->room.QRname);
	snprintf(buf, sizeof buf, "[%s]", CC->room.QRname);
	msg->cm_fields['U'] = strdup(buf);

	CtdlMsgSetCM_Fields(msg, 'F', SKEY(sc->Users[roommailalias]));
	CtdlMsgSetCM_Fields(msg, 'R', SKEY(sc->Users[roommailalias]));

	/* Set the 'List-ID' header */
	CtdlMsgSetCM_Fields(msg, 'L', SKEY(sc->ListID));

	/*
	 * Go fetch the contents of the digest
	 */
	fseek(sc->digestfp, 0L, SEEK_END);
	msglen = ftell(sc->digestfp);

	msg->cm_fields['M'] = malloc(msglen + 1);
	fseek(sc->digestfp, 0L, SEEK_SET);
	fread(msg->cm_fields['M'], (size_t)msglen, 1, sc->digestfp);
	msg->cm_fields['M'][msglen] = '\0';

	fclose(sc->digestfp);
	sc->digestfp = NULL;

	/* Now generate the delivery instructions */
	if (sc->Users[listrecp] == NULL)
		return;

	/* Where do we want bounces and other noise to be heard?
	 *Surely not the list members! */
	snprintf(bounce_to, sizeof bounce_to, "room_aide@%s", config.c_fqdn);

	/* Now submit the message */
	valid = validate_recipients(ChrPtr(sc->Users[listrecp]), NULL, 0);
	if (valid != NULL) {
		valid->bounce_to = strdup(bounce_to);
		valid->envelope_from = strdup(bounce_to);
		CtdlSubmitMsg(msg, valid, NULL, 0);
	}
	CtdlFreeMessage(msg);
	free_recipients(valid);
}


void network_process_digest(SpoolControl *sc, struct CtdlMessage *omsg, long *delete_after_send)
{

	struct CtdlMessage *msg = NULL;

	/*
	 * Process digest recipients
	 */
	if ((sc->Users[digestrecp] == NULL)||
	    (sc->digestfp == NULL))
		return;

	msg = CtdlDuplicateMessage(omsg);
	if (msg != NULL) {
		fprintf(sc->digestfp,
			" -----------------------------------"
			"------------------------------------"
			"-------\n");
		fprintf(sc->digestfp, "From: ");
		if (msg->cm_fields['A'] != NULL) {
			fprintf(sc->digestfp,
				"%s ",
				msg->cm_fields['A']);
		}
		if (msg->cm_fields['F'] != NULL) {
			fprintf(sc->digestfp,
				"<%s> ",
				msg->cm_fields['F']);
		}
		else if (msg->cm_fields['N'] != NULL) {
			fprintf(sc->digestfp,
				"@%s ",
				msg->cm_fields['N']);
		}
		fprintf(sc->digestfp, "\n");
		if (msg->cm_fields['U'] != NULL) {
			fprintf(sc->digestfp,
				"Subject: %s\n",
				msg->cm_fields['U']);
		}

		CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);

		safestrncpy(CC->preferred_formats,
			    "text/plain",
			    sizeof CC->preferred_formats);

		CtdlOutputPreLoadedMsg(msg,
				       MT_CITADEL,
				       HEADERS_NONE,
				       0, 0, 0);

		StrBufTrim(CC->redirect_buffer);
		fwrite(HKEY("\n"), 1, sc->digestfp);
		fwrite(SKEY(CC->redirect_buffer), 1, sc->digestfp);
		fwrite(HKEY("\n"), 1, sc->digestfp);

		FreeStrBuf(&CC->redirect_buffer);

		sc->num_msgs_spooled += 1;
		CtdlFreeMessage(msg);
	}
}


void network_process_list(SpoolControl *sc, struct CtdlMessage *omsg, long *delete_after_send)
{
	struct CtdlMessage *msg = NULL;

	/*
	 * Process mailing list recipients
	 */
	if (sc->Users[listrecp] == NULL)
		return;

	/* create our own copy of the message.
	 *  We're going to need to modify it
	 * in order to insert the [list name] in it, etc.
	 */

	msg = CtdlDuplicateMessage(omsg);


	CtdlMsgSetCM_Fields(msg, 'K', SKEY(sc->Users[roommailalias]));

	/* if there is no other recipient, Set the recipient
	 * of the list message to the email address of the
	 * room itself.
	 */
	if ((msg->cm_fields['R'] == NULL) ||
	    IsEmptyStr(msg->cm_fields['R']))
	{
		CtdlMsgSetCM_Fields(msg, 'R', SKEY(sc->Users[roommailalias]));
	}

	/* Set the 'List-ID' header */
	CtdlMsgSetCM_Fields(msg, 'L', SKEY(sc->ListID));


	/* Prepend "[List name]" to the subject */
	ListCalculateSubject(msg);

	/* Handle delivery */
	network_deliver_list(msg, sc, CC->room.QRname);
	CtdlFreeMessage(msg);
}

/*
 * Deliver list messages to everyone on the list ... efficiently
 */
void network_deliver_list(struct CtdlMessage *msg, SpoolControl *sc, const char *RoomName)
{
	struct recptypes *valid;
	char bounce_to[256];

	/* Don't do this if there were no recipients! */
	if (sc->Users[listrecp] == NULL)
		return;

	/* Now generate the delivery instructions */

	/* Where do we want bounces and other noise to be heard?
	 *  Surely not the list members! */
	snprintf(bounce_to, sizeof bounce_to, "room_aide@%s", config.c_fqdn);

	/* Now submit the message */
	valid = validate_recipients(ChrPtr(sc->Users[listrecp]), NULL, 0);
	if (valid != NULL) {
		valid->bounce_to = strdup(bounce_to);
		valid->envelope_from = strdup(bounce_to);
		valid->sending_room = strdup(RoomName);
		CtdlSubmitMsg(msg, valid, NULL, 0);
		free_recipients(valid);
	}
	/* Do not call CtdlFreeMessage(msg) here; the caller will free it. */
}


void network_process_participate(SpoolControl *sc, struct CtdlMessage *omsg, long *delete_after_send)
{
	struct CtdlMessage *msg = NULL;
	int ok_to_participate = 0;
	StrBuf *Buf = NULL;
	struct recptypes *valid;

	/*
	 * Process client-side list participations for this room
	 */
	if (sc->Users[participate] == NULL)
		return;

	msg = CtdlDuplicateMessage(omsg);

	/* Only send messages which originated on our own
	 * Citadel network, otherwise we'll end up sending the
	 * remote mailing list's messages back to it, which
	 * is rude...
	 */
	ok_to_participate = 0;
	if (msg->cm_fields['N'] != NULL) {
		if (!strcasecmp(msg->cm_fields['N'],
				config.c_nodename)) {
			ok_to_participate = 1;
		}
		
		Buf = NewStrBufPlain(msg->cm_fields['N'], -1);
		if (CtdlIsValidNode(NULL,
				    NULL,
				    Buf,
				    sc->working_ignetcfg,
				    sc->the_netmap) == 0)
		{
			ok_to_participate = 1;
		}
	}
	if (ok_to_participate)
	{
		/* Replace the Internet email address of the
		 * actual author with the email address of the
		 * room itself, so the remote listserv doesn't
		 * reject us.
		 */
		CtdlMsgSetCM_Fields(msg, 'F', SKEY(sc->Users[roommailalias]));

		valid = validate_recipients(ChrPtr(sc->Users[participate]) , NULL, 0);

		CtdlMsgSetCM_Fields(msg, 'R', SKEY(sc->Users[roommailalias]));
		CtdlSubmitMsg(msg, valid, "", 0);
		free_recipients(valid);
	}
	FreeStrBuf(&Buf);
	CtdlFreeMessage(msg);
}

void network_process_ignetpush(SpoolControl *sc, struct CtdlMessage *omsg, long *delete_after_send)
{
	StrBuf *Recipient;
	StrBuf *RemoteRoom;
	const char *Pos = NULL;
	struct CtdlMessage *msg = NULL;
	struct CitContext *CCC = CC;
	struct ser_ret sermsg;
	char buf[SIZ];
	char filename[PATH_MAX];
	FILE *fp;
	size_t newpath_len;
	char *newpath = NULL;
	StrBuf *Buf = NULL;
	int i;
	int bang = 0;
	int send = 1;

	if (sc->Users[ignet_push_share] == NULL)
		return;

	/*
	 * Process IGnet push shares
	 */
	msg = CtdlDuplicateMessage(omsg);

	/* Prepend our node name to the Path field whenever
	 * sending a message to another IGnet node
	 */
	if (msg->cm_fields['P'] == NULL)
	{
		msg->cm_fields['P'] = strdup("username");
	}
	newpath_len = strlen(msg->cm_fields['P']) +
		strlen(config.c_nodename) + 2;
	newpath = malloc(newpath_len);
	snprintf(newpath, newpath_len, "%s!%s",
		 config.c_nodename, msg->cm_fields['P']);
	free(msg->cm_fields['P']);
	msg->cm_fields['P'] = newpath;
	
	/*
	 * Determine if this message is set to be deleted
	 * after sending out on the network
	 */
	if (msg->cm_fields['S'] != NULL) {
		if (!strcasecmp(msg->cm_fields['S'], "CANCEL")) {
			*delete_after_send = 1;
		}
	}

	/* Now send it to every node */
	Recipient = NewStrBufPlain(NULL, StrLength(sc->Users[ignet_push_share]));
	RemoteRoom = NewStrBufPlain(NULL, StrLength(sc->Users[ignet_push_share]));
	while ((Pos != StrBufNOTNULL) &&
	       StrBufExtract_NextToken(Recipient, sc->Users[ignet_push_share], &Pos, ','))
	{
		StrBufExtract_NextToken(RemoteRoom, sc->Users[ignet_push_share], &Pos, ',');
		send = 1;
		NewStrBufDupAppendFlush(&Buf, Recipient, NULL, 1);
			
		/* Check for valid node name */
		if (CtdlIsValidNode(NULL,
				    NULL,
				    Buf,
				    sc->working_ignetcfg,
				    sc->the_netmap) != 0)
		{
			QN_syslog(LOG_ERR,
				  "Invalid node <%s>\n",
				  ChrPtr(Recipient));
			
			send = 0;
		}
		
		/* Check for split horizon */
		QN_syslog(LOG_DEBUG, "Path is %s\n", msg->cm_fields['P']);
		bang = num_tokens(msg->cm_fields['P'], '!');
		if (bang > 1) {
			for (i=0; i<(bang-1); ++i) {
				extract_token(buf,
					      msg->cm_fields['P'],
					      i, '!',
					      sizeof buf);

				QN_syslog(LOG_DEBUG, "Compare <%s> to <%s>\n",
					  buf, ChrPtr(Recipient)) ;
				if (!strcasecmp(buf, ChrPtr(Recipient))) {
					send = 0;
					break;
				}
			}
			
			QN_syslog(LOG_INFO,
				  " %sSending to %s\n",
				  (send)?"":"Not ",
				  ChrPtr(Recipient));
		}
		
		/* Send the message */
		if (send == 1)
		{
			/*
			 * Force the message to appear in the correct
			 * room on the far end by setting the C field
			 * correctly
			 */
			if (msg->cm_fields['C'] != NULL) {
				free(msg->cm_fields['C']);
			}
			if (StrLength(RemoteRoom) > 0) {
				msg->cm_fields['C'] =
					strdup(ChrPtr(RemoteRoom));
			}
			else {
				msg->cm_fields['C'] =
					strdup(CC->room.QRname);
			}
			
			/* serialize it for transmission */
			serialize_message(&sermsg, msg);
			if (sermsg.len > 0) {
				
				/* write it to a spool file */
				snprintf(filename,
					 sizeof(filename),
					 "%s/%s@%lx%x",
					 ctdl_netout_dir,
					 ChrPtr(Recipient),
					 time(NULL),
					 rand()
					);
					
				QN_syslog(LOG_DEBUG,
					  "Appending to %s\n",
					  filename);
				
				fp = fopen(filename, "ab");
				if (fp != NULL) {
					fwrite(sermsg.ser,
					       sermsg.len, 1, fp);
					fclose(fp);
				}
				else {
					QN_syslog(LOG_ERR,
						  "%s: %s\n",
						  filename,
						  strerror(errno));
				}

				/* free the serialized version */
				free(sermsg.ser);
			}
		}
	}
	FreeStrBuf(&Buf);
	FreeStrBuf(&Recipient);
	FreeStrBuf(&RemoteRoom);
	CtdlFreeMessage(msg);
}


/*
 * Spools out one message from the list.
 */
void network_spool_msg(long msgnum,
		       void *userdata)
{
	struct CitContext *CCC = CC;
	struct CtdlMessage *msg = NULL;
	long delete_after_send = 0;	/* Set to 1 to delete after spooling */
	SpoolControl *sc;

	sc = (SpoolControl *)userdata;

	msg = CtdlFetchMessage(msgnum, 1);

	if (msg == NULL)
	{
		QN_syslog(LOG_ERR,
			  "failed to load Message <%ld> from disk\n",
			  msgnum);
		return;
	}
	network_process_list(sc, msg, &delete_after_send);
	network_process_digest(sc, msg, &delete_after_send);
	network_process_participate(sc, msg, &delete_after_send);
	network_process_ignetpush(sc, msg, &delete_after_send);
	
	CtdlFreeMessage(msg);

	/* update lastsent */
	sc->lastsent = msgnum;

	/* Delete this message if delete-after-send is set */
	if (delete_after_send) {
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
	}
}
