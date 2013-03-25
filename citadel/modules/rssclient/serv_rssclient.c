/*
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#include <sys/time.h>
# else
#include <time.h>
# endif
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <expat.h>
#include <curl/curl.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "threads.h"
#include "ctdl_module.h"
#include "msgbase.h"
#include "parsedate.h"
#include "database.h"
#include "citadel_dirs.h"
#include "md5.h"
#include "context.h"
#include "event_client.h"
#include "rss_atom_parser.h"


#define TMP_MSGDATA 0xFF
#define TMP_SHORTER_URL_OFFSET 0xFE
#define TMP_SHORTER_URLS 0xFD

time_t last_run = 0L;

pthread_mutex_t RSSQueueMutex; /* locks the access to the following vars: */
HashList *RSSQueueRooms = NULL; /* rss_room_counter */
HashList *RSSFetchUrls = NULL; /*->rss_aggregator;->RefCount access locked*/

eNextState RSSAggregator_Terminate(AsyncIO *IO);
eNextState RSSAggregator_TerminateDB(AsyncIO *IO);
eNextState RSSAggregator_ShutdownAbort(AsyncIO *IO);
struct CitContext rss_CC;

struct rssnetcfg *rnclist = NULL;
int RSSClientDebugEnabled = 0;
#define N ((rss_aggregator*)IO->Data)->QRnumber

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (RSSClientDebugEnabled != 0))

#define EVRSSC_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "IO[%ld]CC[%d][%ld]RSS" FORMAT,		\
			     IO->ID, CCID, N, __VA_ARGS__)

#define EVRSSCM_syslog(LEVEL, FORMAT)					\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "IO[%ld]CC[%d][%ld]RSS" FORMAT,		\
			     IO->ID, CCID, N)

#define EVRSSQ_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL, "RSS" FORMAT,			\
			     __VA_ARGS__)
#define EVRSSQM_syslog(LEVEL, FORMAT)			\
	DBGLOG(LEVEL) syslog(LEVEL, "RSS" FORMAT)

#define EVRSSCSM_syslog(LEVEL, FORMAT)					\
	DBGLOG(LEVEL) syslog(LEVEL, "IO[%ld][%ld]RSS" FORMAT,		\
			     IO->ID, N)

void DeleteRoomReference(long QRnumber)
{
	HashPos *At;
	long HKLen;
	const char *HK;
	void *vData = NULL;
	rss_room_counter *pRoomC;

	At = GetNewHashPos(RSSQueueRooms, 0);

	if (GetHashPosFromKey(RSSQueueRooms, LKEY(QRnumber), At))
	{
		GetHashPos(RSSQueueRooms, At, &HKLen, &HK, &vData);
		if (vData != NULL)
		{
			pRoomC = (rss_room_counter *) vData;
			pRoomC->count --;
			if (pRoomC->count == 0)
				DeleteEntryFromHash(RSSQueueRooms, At);
		}
	}
	DeleteHashPos(&At);
}

void UnlinkRooms(rss_aggregator *RSSAggr)
{
	DeleteRoomReference(RSSAggr->QRnumber);
	if (RSSAggr->OtherQRnumbers != NULL)
	{
		long HKLen;
		const char *HK;
		HashPos *At;
		void *vData;

		At = GetNewHashPos(RSSAggr->OtherQRnumbers, 0);
		while (! server_shutting_down &&
		       GetNextHashPos(RSSAggr->OtherQRnumbers,
				      At,
				      &HKLen, &HK,
				      &vData) &&
		       (vData != NULL))
		{
			long *lData = (long*) vData;
			DeleteRoomReference(*lData);
		}

		DeleteHashPos(&At);
	}
}

void UnlinkRSSAggregator(rss_aggregator *RSSAggr)
{
	HashPos *At;

	pthread_mutex_lock(&RSSQueueMutex);
	UnlinkRooms(RSSAggr);

	At = GetNewHashPos(RSSFetchUrls, 0);
	if (GetHashPosFromKey(RSSFetchUrls, SKEY(RSSAggr->Url), At))
	{
		DeleteEntryFromHash(RSSFetchUrls, At);
	}
	DeleteHashPos(&At);
	last_run = time(NULL);
	pthread_mutex_unlock(&RSSQueueMutex);
}

void DeleteRssCfg(void *vptr)
{
	rss_aggregator *RSSAggr = (rss_aggregator *)vptr;
	AsyncIO *IO = &RSSAggr->IO;

	if (IO->CitContext != NULL)
		EVRSSCM_syslog(LOG_DEBUG, "RSS: destroying\n");

	FreeStrBuf(&RSSAggr->Url);
	FreeStrBuf(&RSSAggr->rooms);
	FreeStrBuf(&RSSAggr->CData);
	FreeStrBuf(&RSSAggr->Key);
	DeleteHash(&RSSAggr->OtherQRnumbers);

	DeleteHashPos (&RSSAggr->Pos);
	DeleteHash (&RSSAggr->Messages);
	if (RSSAggr->recp.recp_room != NULL)
		free(RSSAggr->recp.recp_room);


	if (RSSAggr->Item != NULL)
	{
		flush_rss_item(RSSAggr->Item);

		free(RSSAggr->Item);
	}

	FreeAsyncIOContents(&RSSAggr->IO);
	memset(RSSAggr, 0, sizeof(rss_aggregator));
	free(RSSAggr);
}

eNextState RSSAggregator_Terminate(AsyncIO *IO)
{
	rss_aggregator *RSSAggr = (rss_aggregator *)IO->Data;

	EVRSSCM_syslog(LOG_DEBUG, "RSS: Terminating.\n");

	StopCurlWatchers(IO);
	UnlinkRSSAggregator(RSSAggr);
	return eAbort;
}

eNextState RSSAggregator_TerminateDB(AsyncIO *IO)
{
	rss_aggregator *RSSAggr = (rss_aggregator *)IO->Data;

	EVRSSCM_syslog(LOG_DEBUG, "RSS: Terminating.\n");


	StopDBWatchers(&RSSAggr->IO);
	UnlinkRSSAggregator(RSSAggr);
	return eAbort;
}

eNextState RSSAggregator_ShutdownAbort(AsyncIO *IO)
{
	const char *pUrl;
	rss_aggregator *RSSAggr = (rss_aggregator *)IO->Data;

	pUrl = IO->ConnectMe->PlainUrl;
	if (pUrl == NULL)
		pUrl = "";

	EVRSSC_syslog(LOG_DEBUG, "RSS: Aborting by shutdown: %s.\n", pUrl);

	StopCurlWatchers(IO);
	UnlinkRSSAggregator(RSSAggr);
	return eAbort;
}

void AppendLink(StrBuf *Message,
		StrBuf *link,
		StrBuf *LinkTitle,
		const char *Title)
{
	if (StrLength(link) > 0)
	{
		StrBufAppendBufPlain(Message, HKEY("<a href=\""), 0);
		StrBufAppendBuf(Message, link, 0);
		StrBufAppendBufPlain(Message, HKEY("\">"), 0);
		if (StrLength(LinkTitle) > 0)
			StrBufAppendBuf(Message, LinkTitle, 0);
		else if ((Title != NULL) && !IsEmptyStr(Title))
			StrBufAppendBufPlain(Message, Title, -1, 0);
		else
			StrBufAppendBuf(Message, link, 0);
		StrBufAppendBufPlain(Message, HKEY("</a><br>\n"), 0);
	}
}


void rss_format_item(networker_save_message *SaveMsg)
{
	StrBuf *Message;
	int msglen = 0;

	if (SaveMsg->author_or_creator != NULL) {

		char *From;
		StrBuf *Encoded = NULL;
		int FromAt;

		From = html_to_ascii(ChrPtr(SaveMsg->author_or_creator),
				     StrLength(SaveMsg->author_or_creator),
				     512, 0);
		StrBufPlain(SaveMsg->author_or_creator, From, -1);
		StrBufTrim(SaveMsg->author_or_creator);
		free(From);

		FromAt = strchr(ChrPtr(SaveMsg->author_or_creator), '@') != NULL;
		if (!FromAt && StrLength (SaveMsg->author_email) > 0)
		{
			StrBufRFC2047encode(&Encoded, SaveMsg->author_or_creator);
			SaveMsg->Msg.cm_fields['A'] = SmashStrBuf(&Encoded);
			SaveMsg->Msg.cm_fields['P'] =
				SmashStrBuf(&SaveMsg->author_email);
		}
		else
		{
			if (FromAt)
			{
				SaveMsg->Msg.cm_fields['A'] =
					SmashStrBuf(&SaveMsg->author_or_creator);
				SaveMsg->Msg.cm_fields['P'] =
					strdup(SaveMsg->Msg.cm_fields['A']);
			}
			else
			{
				StrBufRFC2047encode(&Encoded,
						    SaveMsg->author_or_creator);
				SaveMsg->Msg.cm_fields['A'] =
					SmashStrBuf(&Encoded);
				SaveMsg->Msg.cm_fields['P'] =
					strdup("rss@localhost");

			}
		}
	}
	else {
		SaveMsg->Msg.cm_fields['A'] = strdup("rss");
	}

	SaveMsg->Msg.cm_fields['N'] = strdup(NODENAME);
	if (SaveMsg->title != NULL) {
		long len;
		char *Sbj;
		StrBuf *Encoded, *QPEncoded;

		QPEncoded = NULL;
		StrBufSpaceToBlank(SaveMsg->title);
		len = StrLength(SaveMsg->title);
		Sbj = html_to_ascii(ChrPtr(SaveMsg->title), len, 512, 0);
		len = strlen(Sbj);
		if ((len > 0) && (Sbj[len - 1] == '\n'))
		{
			len --;
			Sbj[len] = '\0';
		}
		Encoded = NewStrBufPlain(Sbj, len);
		free(Sbj);

		StrBufTrim(Encoded);
		StrBufRFC2047encode(&QPEncoded, Encoded);

		SaveMsg->Msg.cm_fields['U'] = SmashStrBuf(&QPEncoded);
		FreeStrBuf(&Encoded);
	}
	if (SaveMsg->link == NULL)
		SaveMsg->link = NewStrBufPlain(HKEY(""));

#if 0 /* temporarily disable shorter urls. */
	SaveMsg->Msg.cm_fields[TMP_SHORTER_URLS] =
		GetShorterUrls(SaveMsg->description);
#endif

	msglen += 1024 + StrLength(SaveMsg->link) + StrLength(SaveMsg->description) ;

	Message = NewStrBufPlain(NULL, msglen);

	StrBufPlain(Message, HKEY(
			    "Content-type: text/html; charset=\"UTF-8\"\r\n\r\n"
			    "<html><body>\n"));
#if 0 /* disable shorter url for now. */
	SaveMsg->Msg.cm_fields[TMP_SHORTER_URL_OFFSET] = StrLength(Message);
#endif
	StrBufAppendBuf(Message, SaveMsg->description, 0);
	StrBufAppendBufPlain(Message, HKEY("<br><br>\n"), 0);

	AppendLink(Message, SaveMsg->link, SaveMsg->linkTitle, NULL);
	AppendLink(Message, SaveMsg->reLink, SaveMsg->reLinkTitle, "Reply to this");
	StrBufAppendBufPlain(Message, HKEY("</body></html>\n"), 0);


	SaveMsg->Message = Message;
}

eNextState RSSSaveMessage(AsyncIO *IO)
{
	long len;
	const char *Key;
	rss_aggregator *RSSAggr = (rss_aggregator *) IO->Data;

	rss_format_item(RSSAggr->ThisMsg);

	RSSAggr->ThisMsg->Msg.cm_fields['M'] =
		SmashStrBuf(&RSSAggr->ThisMsg->Message);

	CtdlSubmitMsg(&RSSAggr->ThisMsg->Msg, &RSSAggr->recp, NULL, 0);

	/* write the uidl to the use table so we don't store this item again */
	cdb_store(CDB_USETABLE,
		  SKEY(RSSAggr->ThisMsg->MsgGUID),
		  &RSSAggr->ThisMsg->ut,
		  sizeof(struct UseTable) );

	if (GetNextHashPos(RSSAggr->Messages,
			   RSSAggr->Pos,
			   &len, &Key,
			   (void**) &RSSAggr->ThisMsg))
		return NextDBOperation(IO, RSS_FetchNetworkUsetableEntry);
	else
		return eAbort;
}

eNextState RSS_FetchNetworkUsetableEntry(AsyncIO *IO)
{
	const char *Key;
	long len;
	struct cdbdata *cdbut;
	rss_aggregator *Ctx = (rss_aggregator *) IO->Data;

	/* Find out if we've already seen this item */
	strcpy(Ctx->ThisMsg->ut.ut_msgid,
	       ChrPtr(Ctx->ThisMsg->MsgGUID)); /// TODO
	Ctx->ThisMsg->ut.ut_timestamp = time(NULL);

	cdbut = cdb_fetch(CDB_USETABLE, SKEY(Ctx->ThisMsg->MsgGUID));
#ifndef DEBUG_RSS
	if (cdbut != NULL) {
		/* Item has already been seen */
		EVRSSC_syslog(LOG_DEBUG,
			  "%s has already been seen\n",
			  ChrPtr(Ctx->ThisMsg->MsgGUID));
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		cdb_store(CDB_USETABLE,
			  SKEY(Ctx->ThisMsg->MsgGUID),
			  &Ctx->ThisMsg->ut, sizeof(struct UseTable) );

		if (GetNextHashPos(Ctx->Messages,
				   Ctx->Pos,
				   &len, &Key,
				   (void**) &Ctx->ThisMsg))
			return NextDBOperation(
				IO,
				RSS_FetchNetworkUsetableEntry);
		else
			return eAbort;
	}
	else
#endif
	{
		NextDBOperation(IO, RSSSaveMessage);
		return eSendMore;
	}
}

eNextState RSSAggregator_AnalyseReply(AsyncIO *IO)
{
	struct UseTable ut;
	u_char rawdigest[MD5_DIGEST_LEN];
	struct MD5Context md5context;
	StrBuf *guid;
	struct cdbdata *cdbut;
	rss_aggregator *Ctx = (rss_aggregator *) IO->Data;

	if (IO->HttpReq.httpcode != 200)
	{
		StrBuf *ErrMsg;
		long lens[2];
		const char *strs[2];

		ErrMsg = NewStrBuf();
		EVRSSC_syslog(LOG_ALERT, "need a 200, got a %ld !\n",
			      IO->HttpReq.httpcode);
		
		strs[0] = ChrPtr(Ctx->Url);
		lens[0] = StrLength(Ctx->Url);

		strs[1] = ChrPtr(Ctx->rooms);
		lens[1] = StrLength(Ctx->rooms);
		StrBufPrintf(ErrMsg,
			     "Error while RSS-Aggregation Run of %s\n"
			     " need a 200, got a %ld !\n"
			     " Response text was: \n"
			     " \n %s\n",
			     ChrPtr(Ctx->Url),
			     IO->HttpReq.httpcode,
			     ChrPtr(IO->HttpReq.ReplyData));
		CtdlAideFPMessage(
			ChrPtr(ErrMsg),
			"RSS Aggregation run failure",
			2, strs, (long*) &lens);
		FreeStrBuf(&ErrMsg);
		return eAbort;
	}

	MD5Init(&md5context);

	MD5Update(&md5context,
		  (const unsigned char*)SKEY(IO->HttpReq.ReplyData));

	MD5Update(&md5context,
		  (const unsigned char*)SKEY(Ctx->Url));

	MD5Final(rawdigest, &md5context);
	guid = NewStrBufPlain(NULL,
			      MD5_DIGEST_LEN * 2 + 12 /* _rss2ctdl*/);
	StrBufHexEscAppend(guid, NULL, rawdigest, MD5_DIGEST_LEN);
	StrBufAppendBufPlain(guid, HKEY("_rssFM"), 0);
	if (StrLength(guid) > 40)
		StrBufCutAt(guid, 40, NULL);
	/* Find out if we've already seen this item */

#ifndef DEBUG_RSS
	cdbut = cdb_fetch(CDB_USETABLE, SKEY(guid));
	if (cdbut != NULL) {
                memcpy(&ut, cdbut->ptr,
                       ((cdbut->len > sizeof(struct UseTable)) ?
                        sizeof(struct UseTable) : cdbut->len));

		if (IO->Now - ut.ut_timestamp  > 
		    60 * 60 * 24 * 4)
		{
			/* Item has already been seen in the last 4 days */
			EVRSSC_syslog(LOG_DEBUG,
				      "%s has already been seen\n",
				      ChrPtr(Ctx->Url));
		}
		cdb_free(cdbut);
	}

	memcpy(ut.ut_msgid, SKEY(guid));
	ut.ut_timestamp = IO->Now;

	/* rewrite the record anyway, to update the timestamp */
	cdb_store(CDB_USETABLE,
		  SKEY(guid),
		  &ut, sizeof(struct UseTable) );
	FreeStrBuf(&guid);
	if (cdbut != NULL) return eAbort;
#endif
	return RSSAggregator_ParseReply(IO);
}

eNextState RSSAggregator_FinishHttp(AsyncIO *IO)
{
	return QueueDBOperation(IO, RSSAggregator_AnalyseReply);
}

/*
 * Begin a feed parse
 */
int rss_do_fetching(rss_aggregator *RSSAggr)
{
	AsyncIO		*IO = &RSSAggr->IO;
	rss_item *ri;
	time_t now;

	now = time(NULL);

	if ((RSSAggr->next_poll != 0) && (now < RSSAggr->next_poll))
		return 0;

	ri = (rss_item*) malloc(sizeof(rss_item));
	memset(ri, 0, sizeof(rss_item));
	RSSAggr->Item = ri;

	if (! InitcURLIOStruct(&RSSAggr->IO,
			       RSSAggr,
			       "Citadel RSS Client",
			       RSSAggregator_FinishHttp,
			       RSSAggregator_Terminate,
			       RSSAggregator_TerminateDB,
			       RSSAggregator_ShutdownAbort))
	{
		EVRSSCM_syslog(LOG_ALERT, "Unable to initialize libcurl.\n");
		return 0;
	}

	safestrncpy(((CitContext*)RSSAggr->IO.CitContext)->cs_host,
		    ChrPtr(RSSAggr->Url),
		    sizeof(((CitContext*)RSSAggr->IO.CitContext)->cs_host));

	EVRSSC_syslog(LOG_DEBUG, "Fetching RSS feed <%s>\n", ChrPtr(RSSAggr->Url));
	ParseURL(&RSSAggr->IO.ConnectMe, RSSAggr->Url, 80);
	CurlPrepareURL(RSSAggr->IO.ConnectMe);

	QueueCurlContext(&RSSAggr->IO);
	return 1;
}

/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCFG)
{
	const RoomNetCfgLine *pLine;
	rss_aggregator *RSSAggr = NULL;
	rss_aggregator *use_this_RSSAggr = NULL;
	void *vptr;

	pthread_mutex_lock(&RSSQueueMutex);
	if (GetHash(RSSQueueRooms, LKEY(qrbuf->QRnumber), &vptr))
	{
		EVRSSQ_syslog(LOG_DEBUG,
			      "rssclient: [%ld] %s already in progress.\n",
			      qrbuf->QRnumber,
			      qrbuf->QRname);
		pthread_mutex_unlock(&RSSQueueMutex);
		return;
	}
	pthread_mutex_unlock(&RSSQueueMutex);

	if (server_shutting_down) return;

	pLine = OneRNCFG->NetConfigs[rssclient];

	while (pLine != NULL)
	{
		const char *lPtr = NULL;

		RSSAggr = (rss_aggregator *) malloc(
			sizeof(rss_aggregator));

		memset (RSSAggr, 0, sizeof(rss_aggregator));
		RSSAggr->QRnumber = qrbuf->QRnumber;
		RSSAggr->roomlist_parts = 1;
		RSSAggr->Url = NewStrBufPlain(NULL, StrLength(pLine->Value[0]));
		StrBufExtract_NextToken(RSSAggr->Url,
					pLine->Value[0],
					&lPtr,
					'|');

		pthread_mutex_lock(&RSSQueueMutex);
		GetHash(RSSFetchUrls,
			SKEY(RSSAggr->Url),
			&vptr);

		use_this_RSSAggr = (rss_aggregator *)vptr;
		if (use_this_RSSAggr != NULL)
		{
			long *QRnumber;
			StrBufAppendBufPlain(
				use_this_RSSAggr->rooms,
				qrbuf->QRname,
				-1, 0);
			if (use_this_RSSAggr->roomlist_parts==1)
			{
				use_this_RSSAggr->OtherQRnumbers
					= NewHash(1, lFlathash);
			}
			QRnumber = (long*)malloc(sizeof(long));
			*QRnumber = qrbuf->QRnumber;
			Put(use_this_RSSAggr->OtherQRnumbers,
			    LKEY(qrbuf->QRnumber),
			    QRnumber,
			    NULL);
			use_this_RSSAggr->roomlist_parts++;

			pthread_mutex_unlock(&RSSQueueMutex);

			FreeStrBuf(&RSSAggr->Url);
			free(RSSAggr);
			RSSAggr = NULL;
			pLine = pLine->next;
			continue;
		}
		pthread_mutex_unlock(&RSSQueueMutex);

		RSSAggr->ItemType = RSS_UNSET;

		RSSAggr->rooms = NewStrBufPlain(
			qrbuf->QRname, -1);

		pthread_mutex_lock(&RSSQueueMutex);

		Put(RSSFetchUrls,
		    SKEY(RSSAggr->Url),
		    RSSAggr,
		    DeleteRssCfg);

		pthread_mutex_unlock(&RSSQueueMutex);
		pLine = pLine->next;
	}
}

/*
 * Scan for rooms that have RSS client requests configured
 */
void rssclient_scan(void) {
	int RSSRoomCount, RSSCount;
	rss_aggregator *rptr = NULL;
	void *vrptr = NULL;
	HashPos *it;
	long len;
	const char *Key;
	time_t now = time(NULL);

	/* Run no more than once every 15 minutes. */
	if ((now - last_run) < 900) {
		EVRSSQ_syslog(LOG_DEBUG,
			      "Client: polling interval not yet reached; last run was %ldm%lds ago",
			      ((now - last_run) / 60),
			      ((now - last_run) % 60)
		);
		return;
	}

	/*
	 * This is a simple concurrency check to make sure only one rssclient
	 * run is done at a time.
	 */
	pthread_mutex_lock(&RSSQueueMutex);
	RSSCount = GetCount(RSSFetchUrls);
	RSSRoomCount = GetCount(RSSQueueRooms);
	pthread_mutex_unlock(&RSSQueueMutex);

	if ((RSSRoomCount > 0) || (RSSCount > 0)) {
		EVRSSQ_syslog(LOG_DEBUG,
			      "rssclient: concurrency check failed; %d rooms and %d url's are queued",
			      RSSRoomCount, RSSCount
			);
		return;
	}

	become_session(&rss_CC);
	EVRSSQM_syslog(LOG_DEBUG, "rssclient started\n");
	CtdlForEachNetCfgRoom(rssclient_scan_room, NULL, rssclient);

	pthread_mutex_lock(&RSSQueueMutex);

	it = GetNewHashPos(RSSFetchUrls, 0);
	while (!server_shutting_down &&
	       GetNextHashPos(RSSFetchUrls, it, &len, &Key, &vrptr) &&
	       (vrptr != NULL)) {
		rptr = (rss_aggregator *)vrptr;
		if (!rss_do_fetching(rptr))
			UnlinkRSSAggregator(rptr);
	}
	DeleteHashPos(&it);
	pthread_mutex_unlock(&RSSQueueMutex);

	EVRSSQM_syslog(LOG_DEBUG, "rssclient ended\n");
	return;
}

void rss_cleanup(void)
{
	/* citthread_mutex_destroy(&RSSQueueMutex); TODO */
	DeleteHash(&RSSFetchUrls);
	DeleteHash(&RSSQueueRooms);
}

void LogDebugEnableRSSClient(const int n)
{
	RSSClientDebugEnabled = n;
}

CTDL_MODULE_INIT(rssclient)
{
	if (!threading)
	{
		CtdlREGISTERRoomCfgType(rssclient, ParseGeneric, 0, 1, SerializeGeneric, DeleteGenericCfgLine); /// todo: implement rss specific parser
		pthread_mutex_init(&RSSQueueMutex, NULL);
		RSSQueueRooms = NewHash(1, lFlathash);
		RSSFetchUrls = NewHash(1, NULL);
		syslog(LOG_INFO, "%s\n", curl_version());
		CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER, PRIO_AGGR + 300);
		CtdlRegisterEVCleanupHook(rss_cleanup);
		CtdlRegisterDebugFlagHook(HKEY("rssclient"), LogDebugEnableRSSClient, &RSSClientDebugEnabled);
	}
	else
	{
		CtdlFillSystemContext(&rss_CC, "rssclient");
	}
	return "rssclient";
}
