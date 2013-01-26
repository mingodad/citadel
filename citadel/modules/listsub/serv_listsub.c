/*
 * This module handles self-service subscription/unsubscription to mail lists.
 *
 * Copyright (c) 2002-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
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
#include "clientsocket.h"
#include "file_ops.h"
#include "ctdl_module.h"

/*
 * Generate a randomizationalisticized token to use for authentication of
 * a subscribe or unsubscribe request.
 */
void listsub_generate_token(char *buf) {
	char sourcebuf[SIZ];
	static int seq = 0;

	/* Theo, please sit down and shut up.  This key doesn't have to be
	 * tinfoil-hat secure, it just needs to be reasonably unguessable
	 * and unique.
	 */
	sprintf(sourcebuf, "%lx",
		(long) (++seq + getpid() + time(NULL))
	);

	/* Convert it to base64 so it looks cool */	
	CtdlEncodeBase64(buf, sourcebuf, strlen(sourcebuf), 0);
}

const RoomNetCfg ActiveSubscribers[] = {listrecp, digestrecp};

int CountThisSubscriber(OneRoomNetCfg *OneRNCfg, StrBuf *email)
{
	RoomNetCfgLine *Line;
	int found_sub = 0;
	int i;

	for (i = 0; i < sizeof (ActiveSubscribers); i++)
	{
		Line = OneRNCfg->NetConfigs[ActiveSubscribers[i]];
		while (Line != NULL)
		{
			if (!strcmp(ChrPtr(email),
				    ChrPtr(Line->Value[0])))
			{
				++found_sub;
				break;					
			}
			Line = Line->next;
		}
	}
	return found_sub;
}

/*
	subpending,
	unsubpending,
	ignet_push_share,
	listrecp,
	digestrecp,
	pop3client,
	rssclient,
	participate,
	roommailalias,
	maxRoomNetCfg

/ *
 * Enter a subscription request
 */
void do_subscribe(StrBuf **room, StrBuf **email, StrBuf **subtype, StrBuf **webpage) {
	struct ctdlroom qrbuf;
	char token[256];
	char *pcf_req;
	StrBuf *cf_req;
	StrBuf *UrlRoom;
	int found_sub = 0;
	const char *RoomMailAddress;
	OneRoomNetCfg *OneRNCfg;
	RoomNetCfgLine *Line;
	long RoomMailAddressLen;

	if (CtdlGetRoom(&qrbuf, ChrPtr(*room)) != 0) {
		cprintf("%d There is no list called '%s'\n", ERROR + ROOM_NOT_FOUND, ChrPtr(*room));
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	/* 
	 * Make sure the requested address isn't already subscribed
	 */
	begin_critical_section(S_NETCONFIGS);

	RoomMailAddress = qrbuf.QRname;
	OneRNCfg = CtdlGetNetCfgForRoom(qrbuf.QRnumber);
	if (OneRNCfg!=NULL) {
		found_sub = CountThisSubscriber(OneRNCfg, *email);
		if (StrLength(OneRNCfg->Sender) > 0)
			RoomMailAddress = ChrPtr(OneRNCfg->Sender);
	}

	if (found_sub != 0) {
		cprintf("%d '%s' is already subscribed to '%s'.\n",
			ERROR + ALREADY_EXISTS,
			ChrPtr(*email),
			RoomMailAddress);

		end_critical_section(S_NETCONFIGS);
		return;
	}

	/*
	 * Now add it to the config
	 */	
	
	RoomMailAddressLen = strlen(RoomMailAddress);
	listsub_generate_token(token);
	Line = (RoomNetCfgLine*)malloc(sizeof(RoomNetCfgLine));
	memset(Line, 0, sizeof(RoomNetCfgLine));

	Line->Value = (StrBuf**) malloc(sizeof(StrBuf*) * 5);
	
	Line->Value[0] = *email; *email = NULL;
	Line->Value[1] = *subtype; *subtype = NULL;
	Line->Value[2] = NewStrBufPlain(token, -1);
	Line->Value[3] = NewStrBufPlain(NULL, 10);
	StrBufPrintf(Line->Value[3], "%ld", time(NULL));
	Line->Value[4] = *webpage; *webpage = NULL;
	Line->nValues = 5;

	AddRoomCfgLine(OneRNCfg, &qrbuf, subpending, Line);

	/* Generate and send the confirmation request */
	UrlRoom = NewStrBuf();
	StrBufUrlescAppend(UrlRoom, NULL, qrbuf.QRname);

	cf_req = NewStrBufPlain(NULL, 2048);
	StrBufAppendBufPlain(
		cf_req,
		HKEY("MIME-Version: 1.0\n"
		     "Content-Type: multipart/alternative; boundary=\"__ctdlmultipart__\"\n"
		     "\n"
		     "This is a multipart message in MIME format.\n"
		     "\n"
		     "--__ctdlmultipart__\n"
		     "Content-type: text/plain\n"
		     "\n"
		     "Someone (probably you) has submitted a request to subscribe\n"
		     "<"), 0);
	StrBufAppendBuf(cf_req, Line->Value[0], 0);

	StrBufAppendBufPlain(cf_req, HKEY("> to the '"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list.\n"
		     "\n"
		     "Please go here to confirm this request:\n"
		     "  "), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);

	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);

	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("&cmd=confirm  \n"
		     "\n"
		     "If this request has been submitted in error and you do not\n"
		     "wish to receive the '"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list, simply do nothing,\n"
		     "and you will not receive any further mailings.\n"
		     "\n"
		     "--__ctdlmultipart__\n"
		     "Content-type: text/html\n"
		     "\n"
		     "<HTML><BODY>\n"
		     "Someone (probably you) has submitted a request to subscribe\n"
		     "&lt;"), 0);
	StrBufAppendBuf(cf_req, Line->Value[0], 0);

	StrBufAppendBufPlain(cf_req, HKEY( "&gt; to the <B>"), 0);

	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("'</B> mailing list.<BR><BR>\n"
		     "Please click here to confirm this request:<BR>\n"
		     "<A HREF=\""), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);

	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);

	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);

	StrBufAppendBufPlain(cf_req, HKEY("&cmd=confirm\">"), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);

	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);

	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("&cmd=confirm</A><BR><BR>\n"
		     "If this request has been submitted in error and you do not\n"
		     "wish to receive the '"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);
	
	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list, simply do nothing,\n"
		     "and you will not receive any further mailings.\n"
		     "</BODY></HTML>\n"
		     "\n"
		     "--__ctdlmultipart__--\n"), 0);

	end_critical_section(S_NETCONFIGS);

	pcf_req = SmashStrBuf(&cf_req);
	quickie_message(	/* This delivers the message */
		"Citadel",
		NULL,
		ChrPtr(*email),
		NULL,
		pcf_req,
		FMT_RFC822,
		"Please confirm your list subscription"
		);
	free(cf_req);
	cprintf("%d Subscription entered; confirmation request sent\n", CIT_OK);

	FreeStrBuf(&UrlRoom);
}


/*
 * Enter an unsubscription request
 */
void do_unsubscribe(StrBuf **room, StrBuf **email, StrBuf **webpage) {
	struct ctdlroom qrbuf;
	char token[256];
	char *pcf_req;
	StrBuf *cf_req;
	StrBuf *UrlRoom;
	int found_sub = 0;
	const char *RoomMailAddress;
	OneRoomNetCfg *OneRNCfg;
	RoomNetCfgLine *Line;
	long RoomMailAddressLen;

	if (CtdlGetRoom(&qrbuf, ChrPtr(*room)) != 0) {
		cprintf("%d There is no list called '%s'\n",
			ERROR + ROOM_NOT_FOUND, ChrPtr(*room));
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	listsub_generate_token(token);

	/* 
	 * Make sure there's actually a subscription there to remove
	 */
	begin_critical_section(S_NETCONFIGS);
	RoomMailAddress = qrbuf.QRname;
	OneRNCfg = CtdlGetNetCfgForRoom(qrbuf.QRnumber);
	if (OneRNCfg!=NULL) {
		found_sub = CountThisSubscriber(OneRNCfg, *email);
		if (StrLength(OneRNCfg->Sender) > 0)
			RoomMailAddress = ChrPtr(OneRNCfg->Sender);
	}

	if (found_sub == 0) {
		cprintf("%d '%s' is not subscribed to '%s'.\n",
			ERROR + NO_SUCH_USER,
			ChrPtr(*email),
			qrbuf.QRname);

		end_critical_section(S_NETCONFIGS);
		return;
	}
	
	/* 
	 * Ok, now enter the unsubscribe-pending entry.
	 */
	RoomMailAddressLen = strlen(RoomMailAddress);
	listsub_generate_token(token);
	Line = (RoomNetCfgLine*)malloc(sizeof(RoomNetCfgLine));
	memset(Line, 0, sizeof(RoomNetCfgLine));

	Line->Value = (StrBuf**) malloc(sizeof(StrBuf*) * 5);
	
	Line->Value[0] = *email; *email = NULL;
	Line->Value[2] = NewStrBufPlain(token, -1);
	Line->Value[3] = NewStrBufPlain(NULL, 10);
	StrBufPrintf(Line->Value[3], "%ld", time(NULL));
	Line->Value[4] = *webpage; *webpage = NULL;
	Line->nValues = 5;

	AddRoomCfgLine(OneRNCfg, &qrbuf, unsubpending, Line);

	/* Generate and send the confirmation request */
	UrlRoom = NewStrBuf();
	StrBufUrlescAppend(UrlRoom, NULL, qrbuf.QRname);

	cf_req = NewStrBufPlain(NULL, 2048);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("MIME-Version: 1.0\n"
		     "Content-Type: multipart/alternative; boundary=\"__ctdlmultipart__\"\n"
		     "\n"
		     "This is a multipart message in MIME format.\n"
		     "\n"
		     "--__ctdlmultipart__\n"
		     "Content-type: text/plain\n"
		     "\n"
		     "Someone (probably you) has submitted a request to unsubscribe\n"
		     "<"), 0);
	StrBufAppendBuf(cf_req, Line->Value[0], 0);


	StrBufAppendBufPlain(
		cf_req,
		HKEY("> from the '"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list.\n"
		     "\n"
		     "Please go here to confirm this request:\n  "), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);
	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);
	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("&cmd=confirm  \n"
		     "\n"
		     "If this request has been submitted in error and you do not\n"
		     "wish to unsubscribe from the '"), 0);

	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list, simply do nothing,\n"
		     "and the request will not be processed.\n"
		     "\n"
		     "--__ctdlmultipart__\n"
		     "Content-type: text/html\n"
		     "\n"
		     "<HTML><BODY>\n"
		     "Someone (probably you) has submitted a request to unsubscribe\n"
		     "&lt;"), 0);
	StrBufAppendBuf(cf_req, Line->Value[0], 0);

	StrBufAppendBufPlain(cf_req, HKEY("&gt; from the <B>"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("</B> mailing list.<BR><BR>\n"
		     "Please click here to confirm this request:<BR>\n"
		     "<A HREF=\""), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);

	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);

	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);

	StrBufAppendBufPlain(cf_req, HKEY("&cmd=confirm\">"), 0);
	StrBufAppendBuf(cf_req, Line->Value[4], 0);

	StrBufAppendBufPlain(cf_req, HKEY("?room="), 0);
	StrBufAppendBuf(cf_req, UrlRoom, 0);

	StrBufAppendBufPlain(cf_req, HKEY("&token="), 0);
	StrBufAppendBuf(cf_req, Line->Value[2], 0);


	StrBufAppendBufPlain(
		cf_req,
		HKEY("&cmd=confirm</A><BR><BR>\n"
		     "If this request has been submitted in error and you do not\n"
		     "wish to unsubscribe from the '"), 0);
	StrBufAppendBufPlain(cf_req, RoomMailAddress, RoomMailAddressLen, 0);

	StrBufAppendBufPlain(
		cf_req,
		HKEY("' mailing list, simply do nothing,\n"
		     "and the request will not be processed.\n"
		     "</BODY></HTML>\n"
		     "\n"
		     "--__ctdlmultipart__--\n"), 0);

	end_critical_section(S_NETCONFIGS);

	pcf_req = SmashStrBuf(&cf_req);
	quickie_message(	/* This delivers the message */
		"Citadel",
		NULL,
		ChrPtr(*email),
		NULL,
		pcf_req,
		FMT_RFC822,
		"Please confirm your unsubscribe request"
	);

	cprintf("%d Unubscription noted; confirmation request sent\n", CIT_OK);
}


const RoomNetCfg ConfirmSubscribers[] = {subpending, unsubpending};

/*
 * Confirm a subscribe/unsubscribe request.
 */
void do_confirm(StrBuf **room, StrBuf **token) {
	struct ctdlroom qrbuf;
	OneRoomNetCfg *OneRNCfg;
	RoomNetCfgLine *Line;
	RoomNetCfgLine *ConfirmLine;
	RoomNetCfgLine **PrevLine;
	int success = 0;
	RoomNetCfg ConfirmType;
	int i;
	
	if (CtdlGetRoom(&qrbuf, ChrPtr(*room)) != 0) {
		cprintf("%d There is no list called '%s'\n",
			ERROR + ROOM_NOT_FOUND, ChrPtr(*room));
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	/*
	 * Now start scanning this room's netconfig file for the
	 * specified token.
	 */
	begin_critical_section(S_NETCONFIGS);
	OneRNCfg = CtdlGetNetCfgForRoom(qrbuf.QRnumber);
	if (OneRNCfg==NULL) {
////TODO
	}


	ConfirmType = maxRoomNetCfg;
	for (i = 0; i < sizeof (ConfirmSubscribers); i++)
	{
		PrevLine = &OneRNCfg->NetConfigs[ConfirmSubscribers[i]];
		Line = *PrevLine;
		while (Line != NULL)
		{
			if (!strcasecmp(ChrPtr(*token),
					ChrPtr(Line->Value[2])))
			{
				*PrevLine = Line->next; /* Remove it from the list */
				ConfirmLine = Line;
				ConfirmType = ConfirmSubscribers[i];
				i += 100; 
				break;
			}
			PrevLine = &Line;
			Line = Line->next;
		}
	}

	if (ConfirmType == subpending) {
		if (!strcasecmp(ChrPtr(ConfirmLine->Value[2]), 
				("digest")))
		{
			ConfirmType = digestrecp;
		}
		else
		{
			ConfirmType = listrecp;
		}

		syslog(LOG_NOTICE, 
		       "Mailing list: %s subscribed to %s with token %s\n", 
		       ChrPtr(ConfirmLine->Value[0]), 
		       qrbuf.QRname,
		       ChrPtr(*token));

		FreeStrBuf(&ConfirmLine->Value[1]);
		FreeStrBuf(&ConfirmLine->Value[2]);
		FreeStrBuf(&ConfirmLine->Value[3]);
		FreeStrBuf(&ConfirmLine->Value[4]);
		ConfirmLine->nValues = 5;

		AddRoomCfgLine(OneRNCfg, &qrbuf, ConfirmType, ConfirmLine);
		success = 1;
	}
	else if (ConfirmType == unsubpending) {
		for (i = 0; i < sizeof (ActiveSubscribers); i++)
		{
			PrevLine = &OneRNCfg->NetConfigs[ActiveSubscribers[i]];
			Line = *PrevLine;
			while (Line != NULL)
			{
				if (!strcasecmp(ChrPtr(ConfirmLine->Value[0]),
						ChrPtr(Line->Value[2])))
				{
					*PrevLine = Line->next; /* Remove it from the list */
					i += 100; 
					break;
				}
				PrevLine = &Line;
				Line = Line->next;
			}
		}


		syslog(LOG_NOTICE, 
		       "Mailing list: %s unsubscribed to %s with token %s\n", 
		       ChrPtr(ConfirmLine->Value[0]), 
		       qrbuf.QRname,
		       ChrPtr(*token));


		if (Line != NULL) DeleteGenericCfgLine(NULL/*TODO*/, &Line);
		DeleteGenericCfgLine(NULL/*TODO*/, &ConfirmLine);
		AddRoomCfgLine(OneRNCfg, &qrbuf, ConfirmType, NULL);
		success = 1;
	}

	end_critical_section(S_NETCONFIGS);
	
	/*
	 * Did we do anything useful today?
	 */
	if (success) {
		cprintf("%d %d operation(s) confirmed.\n", CIT_OK, success);
	}
	else {
		cprintf("%d Invalid token.\n", ERROR + ILLEGAL_VALUE);
	}

}



/* 
 * process subscribe/unsubscribe requests and confirmations
 */
void cmd_subs(char *cmdbuf)
{
	const char *Pos = NULL;
	StrBuf *Segments[20];
	int i=1;

	memset(Segments, 0, sizeof(StrBuf*) * 20);
	Segments[0] = NewStrBufPlain(cmdbuf, -1);
	while ((Pos != StrBufNOTNULL) && (i < 20))
	{
		Segments[i] = NewStrBufPlain(NULL, StrLength(Segments[0]));
		StrBufExtract_NextToken(Segments[i], Segments[0], &Pos, '|');
	}

	if (!strcasecmp(ChrPtr(Segments[1]), "subscribe")) {
		if ( (strcasecmp(ChrPtr(Segments[4]), "list"))
		   && (strcasecmp(ChrPtr(Segments[4]), "digest")) ) {
			cprintf("%d Invalid subscription type '%s'\n",
				ERROR + ILLEGAL_VALUE, ChrPtr(Segments[4]));
		}
		else {
			do_subscribe(&Segments[2], &Segments[3], &Segments[4], &Segments[5]);
		}
	}
	else if (!strcasecmp(ChrPtr(Segments[1]), "unsubscribe")) {
		do_unsubscribe(&Segments[2], &Segments[3], &Segments[4]);
	}
	else if (!strcasecmp(ChrPtr(Segments[1]), "confirm")) {
		do_confirm(&Segments[2], &Segments[3]);
	}
	else {
		cprintf("%d Invalid command\n", ERROR + ILLEGAL_VALUE);
	}
}


/*
 * Module entry point
 */
CTDL_MODULE_INIT(listsub)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_subs, "SUBS", "List subscribe/unsubscribe");
	}
	
	/* return our module name for the log */
	return "listsub";
}
