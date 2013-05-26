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
#define N ((rss_aggregator*)IO->Data)->Cfg.QRnumber

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

typedef enum _RSSState {
	eRSSCreated,
	eRSSFetching,
	eRSSFailure,
	eRSSParsing,
	eRSSUT
} RSSState;
ConstStr RSSStates[] = {
	{HKEY("Aggregator created")},
	{HKEY("Fetching content")},
	{HKEY("Failed")},
	{HKEY("parsing content")},
	{HKEY("checking usetable")}
};

static void SetRSSState(AsyncIO *IO, RSSState State)
{
	CitContext* CCC = IO->CitContext;
	if (CCC != NULL)
		memcpy(CCC->cs_clientname, RSSStates[State].Key, RSSStates[State].len + 1);
}

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
	DeleteRoomReference(RSSAggr->Cfg.QRnumber);
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
			pRSSConfig *Data = (pRSSConfig*) vData;
			DeleteRoomReference(Data->QRnumber);
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

	CheckIfAlreadySeen("RSS Item Insert", RSSAggr->ThisMsg->MsgGUID, IO->Now, 0, eWrite, IO->ID, CCID);

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
	rss_aggregator *Ctx = (rss_aggregator *) IO->Data;

	/* Find out if we've already seen this item */
// todo: expiry?
#ifndef DEBUG_RSS
	SetRSSState(IO, eRSSUT);
	if (CheckIfAlreadySeen("RSS Item Seen",
			       Ctx->ThisMsg->MsgGUID,
			       IO->Now,
			       IO->Now - USETABLE_ANTIEXPIRE,
			       eCheckUpdate,
			       IO->ID, CCID)
	    != 0)
	{
		/* Item has already been seen */
		EVRSSC_syslog(LOG_DEBUG,
			  "%s has already been seen\n",
			  ChrPtr(Ctx->ThisMsg->MsgGUID));
		SetRSSState(IO, eRSSParsing);

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
		SetRSSState(IO, eRSSParsing);

		NextDBOperation(IO, RSSSaveMessage);
		return eSendMore;
	}
	return eSendMore;
}

void UpdateLastKnownGood(pRSSConfig *pCfg, time_t now)
{
	OneRoomNetCfg* pRNCfg;
	begin_critical_section(S_NETCONFIGS);
	pRNCfg = CtdlGetNetCfgForRoom (pCfg->QRnumber);
	if (pRNCfg != NULL)
	{
		RSSCfgLine *RSSCfg = (RSSCfgLine *)pRNCfg->NetConfigs[rssclient];

		while (RSSCfg != NULL)
		{
			if (RSSCfg == pCfg->pCfg)
				break;

			RSSCfg = RSSCfg->next;
		}
		if (RSSCfg != NULL)
		{
			pRNCfg->changed = 1;
			RSSCfg->last_known_good = now;
		}
	}

	end_critical_section(S_NETCONFIGS);
}

eNextState RSSAggregator_AnalyseReply(AsyncIO *IO)
{
	HashPos *it = NULL;
	long len;
	const char *Key;
	pRSSConfig *pCfg;
	u_char rawdigest[MD5_DIGEST_LEN];
	struct MD5Context md5context;
	StrBuf *guid;
	rss_aggregator *Ctx = (rss_aggregator *) IO->Data;

	if (IO->HttpReq.httpcode != 200)
	{
		StrBuf *ErrMsg;
		long lens[2];
		const char *strs[2];

		SetRSSState(IO, eRSSFailure);
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
			2, strs, (long*) &lens,
			IO->Now,
			IO->ID, CCID);
		
		FreeStrBuf(&ErrMsg);
		EVRSSC_syslog(LOG_DEBUG,
			      "RSS feed returned an invalid http status code. <%s><HTTP %ld>\n",
			      ChrPtr(Ctx->Url),
			      IO->HttpReq.httpcode);
		return eAbort;
	}

	pCfg = &Ctx->Cfg;

	while (pCfg != NULL)
	{
		UpdateLastKnownGood (pCfg, IO->Now);
		if ((Ctx->roomlist_parts > 1) && 
		    (it == NULL))
		{
			it = GetNewHashPos(RSSFetchUrls, 0);
		}
		if (it != NULL)
		{
			void *vptr;
			if (GetNextHashPos(Ctx->OtherQRnumbers, it, &len, &Key, &vptr))
				pCfg = vptr;
			else
				pCfg = NULL;
		}
		else 
			pCfg = NULL;
	}
	DeleteHashPos (&it);

	SetRSSState(IO, eRSSUT);

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

	if (CheckIfAlreadySeen("RSS Whole",
			       guid,
			       IO->Now,
			       IO->Now - USETABLE_ANTIEXPIRE,
			       eCheckUpdate,
			       IO->ID, CCID)
	    != 0)
	{
		FreeStrBuf(&guid);

		EVRSSC_syslog(LOG_DEBUG, "RSS feed already seen. <%s>\n", ChrPtr(Ctx->Url));
		return eAbort;
	}
	FreeStrBuf(&guid);
#endif
	SetRSSState(IO, eRSSParsing);
	return RSSAggregator_ParseReply(IO);
}

eNextState RSSAggregator_FinishHttp(AsyncIO *IO)
{
	StopCurlWatchers(IO);
	return CurlQueueDBOperation(IO, RSSAggregator_AnalyseReply);
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
	SetRSSState(IO, eRSSCreated);

	safestrncpy(((CitContext*)RSSAggr->IO.CitContext)->cs_host,
		    ChrPtr(RSSAggr->Url),
		    sizeof(((CitContext*)RSSAggr->IO.CitContext)->cs_host));

	EVRSSC_syslog(LOG_DEBUG, "Fetching RSS feed <%s>\n", ChrPtr(RSSAggr->Url));
	ParseURL(&RSSAggr->IO.ConnectMe, RSSAggr->Url, 80);
	CurlPrepareURL(RSSAggr->IO.ConnectMe);

	SetRSSState(IO, eRSSFetching);
	QueueCurlContext(&RSSAggr->IO);
	return 1;
}

/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCFG)
{
	const RSSCfgLine *RSSCfg = (RSSCfgLine *)OneRNCFG->NetConfigs[rssclient];
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

	while (RSSCfg != NULL)
	{
		pthread_mutex_lock(&RSSQueueMutex);
		GetHash(RSSFetchUrls,
			SKEY(RSSCfg->Url),
			&vptr);

		use_this_RSSAggr = (rss_aggregator *)vptr;
		if (use_this_RSSAggr != NULL)
		{
			pRSSConfig *pRSSCfg;

			StrBufAppendBufPlain(
				use_this_RSSAggr->rooms,
				qrbuf->QRname,
				-1, 0);
			if (use_this_RSSAggr->roomlist_parts==1)
			{
				use_this_RSSAggr->OtherQRnumbers
					= NewHash(1, lFlathash);
			}

			pRSSCfg = (pRSSConfig *) malloc(sizeof(pRSSConfig));

			pRSSCfg->QRnumber = qrbuf->QRnumber;
			pRSSCfg->pCfg = RSSCfg;

			Put(use_this_RSSAggr->OtherQRnumbers,
			    LKEY(qrbuf->QRnumber),
			    pRSSCfg,
			    NULL);
			use_this_RSSAggr->roomlist_parts++;

			pthread_mutex_unlock(&RSSQueueMutex);

			RSSCfg = RSSCfg->next;
			continue;
		}
		pthread_mutex_unlock(&RSSQueueMutex);

		RSSAggr = (rss_aggregator *) malloc(
			sizeof(rss_aggregator));

		memset (RSSAggr, 0, sizeof(rss_aggregator));
		RSSAggr->Cfg.QRnumber = qrbuf->QRnumber;
		RSSAggr->Cfg.pCfg = RSSCfg;
		RSSAggr->roomlist_parts = 1;
		RSSAggr->Url = NewStrBufDup(RSSCfg->Url);

		RSSAggr->ItemType = RSS_UNSET;

		RSSAggr->rooms = NewStrBufPlain(
			qrbuf->QRname, -1);

		pthread_mutex_lock(&RSSQueueMutex);

		Put(RSSFetchUrls,
		    SKEY(RSSAggr->Url),
		    RSSAggr,
		    DeleteRssCfg);

		pthread_mutex_unlock(&RSSQueueMutex);
		RSSCfg = RSSCfg->next;
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
	EVRSSQM_syslog(LOG_DEBUG, "rssclient started");
	CtdlForEachNetCfgRoom(rssclient_scan_room, NULL, rssclient);

	if (GetCount(RSSFetchUrls) > 0)
	{
		pthread_mutex_lock(&RSSQueueMutex);
		EVRSSQ_syslog(LOG_DEBUG,
			       "rssclient starting %d Clients",
			       GetCount(RSSFetchUrls));
		
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
	}
	else
		EVRSSQM_syslog(LOG_DEBUG, "Nothing to do.");

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


typedef struct __RSSVetoInfo {
	StrBuf *ErrMsg;
	time_t Now;
	int Veto;
}RSSVetoInfo;

void rssclient_veto_scan_room(struct ctdlroom *qrbuf, void *data, OneRoomNetCfg *OneRNCFG)
{
	RSSVetoInfo *Info = (RSSVetoInfo *) data;
	const RSSCfgLine *RSSCfg = (RSSCfgLine *)OneRNCFG->NetConfigs[rssclient];

	while (RSSCfg != NULL)
	{
		if ((RSSCfg->last_known_good != 0) &&
		    (RSSCfg->last_known_good + USETABLE_ANTIEXPIRE < Info->Now))
		{
			StrBufAppendPrintf(Info->ErrMsg,
					   "RSS feed not seen for a %d days:: <",
					   (Info->Now - RSSCfg->last_known_good) / (24 * 60 * 60));

			StrBufAppendBuf(Info->ErrMsg, RSSCfg->Url, 0);
			StrBufAppendBufPlain(Info->ErrMsg, HKEY(">\n"), 0);
		}
		RSSCfg = RSSCfg->next;
	}
}

int RSSCheckUsetableVeto(StrBuf *ErrMsg)
{
	RSSVetoInfo Info;

	Info.ErrMsg = ErrMsg;
	Info.Now = time (NULL);
	Info.Veto = 0;

	CtdlForEachNetCfgRoom(rssclient_veto_scan_room, &Info, rssclient);

	return Info.Veto;;
}




void ParseRSSClientCfgLine(const CfgLineType *ThisOne, StrBuf *Line, const char *LinePos, OneRoomNetCfg *OneRNCFG)
{
	RSSCfgLine *RSSCfg;

	RSSCfg = (RSSCfgLine *) malloc (sizeof(RSSCfgLine));
	RSSCfg->Url = NewStrBufPlain (NULL, StrLength (Line));
	

	StrBufExtract_NextToken(RSSCfg->Url, Line, &LinePos, '|');
	RSSCfg->last_known_good = StrBufExtractNext_long(Line, &LinePos, '|');


	RSSCfg->next = (RSSCfgLine *)OneRNCFG->NetConfigs[ThisOne->C];
	OneRNCFG->NetConfigs[ThisOne->C] = (RoomNetCfgLine*) RSSCfg;
}

void SerializeRSSClientCfgLine(const CfgLineType *ThisOne, StrBuf *OutputBuffer, OneRoomNetCfg *RNCfg, RoomNetCfgLine *data)
{
	RSSCfgLine *RSSCfg = (RSSCfgLine*) data;

	StrBufAppendBufPlain(OutputBuffer, CKEY(ThisOne->Str), 0);
	StrBufAppendBufPlain(OutputBuffer, HKEY("|"), 0);
	StrBufAppendBufPlain(OutputBuffer, SKEY(RSSCfg->Url), 0);
	StrBufAppendPrintf(OutputBuffer, "|%ld\n", RSSCfg->last_known_good);
}

void DeleteRSSClientCfgLine(const CfgLineType *ThisOne, RoomNetCfgLine **data)
{
	RSSCfgLine *RSSCfg = (RSSCfgLine*) *data;

	FreeStrBuf(&RSSCfg->Url);
	free(*data);
	*data = NULL;
}


CTDL_MODULE_INIT(rssclient)
{
	if (!threading)
	{
		CtdlRegisterTDAPVetoHook (RSSCheckUsetableVeto, CDB_USETABLE, 0);

		CtdlREGISTERRoomCfgType(rssclient, ParseRSSClientCfgLine, 0, 1, SerializeRSSClientCfgLine, DeleteRSSClientCfgLine);
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
