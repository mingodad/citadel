/*
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
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
eNextState RSSAggregator_ShutdownAbort(AsyncIO *IO);
struct CitContext rss_CC;

struct rssnetcfg *rnclist = NULL;


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

void UnlinkRooms(rss_aggregator *Cfg)
{
	DeleteRoomReference(Cfg->QRnumber);
	if (Cfg->OtherQRnumbers != NULL)
	{
		long HKLen;
		const char *HK;
		HashPos *At;
		void *vData;

		At = GetNewHashPos(Cfg->OtherQRnumbers, 0);
		while (! server_shutting_down &&
		       GetNextHashPos(Cfg->OtherQRnumbers,
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

void UnlinkRSSAggregator(rss_aggregator *Cfg)
{
	HashPos *At;

	UnlinkRooms(Cfg);

	At = GetNewHashPos(RSSFetchUrls, 0);
	if (GetHashPosFromKey(RSSFetchUrls, SKEY(Cfg->Url), At))
	{
		DeleteEntryFromHash(RSSFetchUrls, At);
	}
	DeleteHashPos(&At);
	last_run = time(NULL);
}

void DeleteRssCfg(void *vptr)
{
	rss_aggregator *RSSAggr = (rss_aggregator *)vptr;
	AsyncIO *IO = &RSSAggr->IO;
	EVM_syslog(LOG_DEBUG, "RSS: destroying\n");

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
	free(RSSAggr);
}

eNextState RSSAggregator_Terminate(AsyncIO *IO)
{
	rss_aggregator *RSSAggr = (rss_aggregator *)IO->Data;

	EVM_syslog(LOG_DEBUG, "RSS: Terminating.\n");


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

	EV_syslog(LOG_DEBUG, "RSS: Aborting by shutdown: %s.\n", pUrl);


	UnlinkRSSAggregator(RSSAggr);
	return eAbort;
}


eNextState AbortNetworkSaveMessage (AsyncIO *IO)
{
	return eAbort; ///TODO
}

eNextState RSSSaveMessage(AsyncIO *IO)
{
	long len;
	const char *Key;
	rss_aggregator *RSSAggr = (rss_aggregator *) IO->Data;

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
		EV_syslog(LOG_DEBUG,
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

/*
 * Begin a feed parse
 */
int rss_do_fetching(rss_aggregator *Cfg)
{
	rss_item *ri;
	time_t now;

	now = time(NULL);

	if ((Cfg->next_poll != 0) && (now < Cfg->next_poll))
		return 0;

	ri = (rss_item*) malloc(sizeof(rss_item));
	memset(ri, 0, sizeof(rss_item));
	Cfg->Item = ri;

	if (! InitcURLIOStruct(&Cfg->IO,
			       Cfg,
			       "Citadel RSS Client",
			       RSSAggregator_ParseReply,
			       RSSAggregator_Terminate,
			       RSSAggregator_ShutdownAbort))
	{
		syslog(LOG_ALERT, "Unable to initialize libcurl.\n");
		return 0;
	}

	safestrncpy(((CitContext*)Cfg->IO.CitContext)->cs_host,
		    ChrPtr(Cfg->Url),
		    sizeof(((CitContext*)Cfg->IO.CitContext)->cs_host));

	syslog(LOG_DEBUG, "Fetching RSS feed <%s>\n", ChrPtr(Cfg->Url));
	ParseURL(&Cfg->IO.ConnectMe, Cfg->Url, 80);
	CurlPrepareURL(Cfg->IO.ConnectMe);

	QueueCurlContext(&Cfg->IO);
	return 1;
}

/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data)
{
	StrBuf *CfgData=NULL;
	StrBuf *CfgType;
	StrBuf *Line;
	rss_room_counter *Count = NULL;
	struct stat statbuf;
	char filename[PATH_MAX];
	int fd;
	int Done;
	rss_aggregator *RSSAggr = NULL;
	rss_aggregator *use_this_RSSAggr = NULL;
	void *vptr;
	const char *CfgPtr, *lPtr;
	const char *Err;

	pthread_mutex_lock(&RSSQueueMutex);
	if (GetHash(RSSQueueRooms, LKEY(qrbuf->QRnumber), &vptr))
	{
		syslog(LOG_DEBUG,
		       "rssclient: [%ld] %s already in progress.\n",
		       qrbuf->QRnumber,
		       qrbuf->QRname);
		pthread_mutex_unlock(&RSSQueueMutex);
		return;
	}
	pthread_mutex_unlock(&RSSQueueMutex);

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	if (server_shutting_down)
		return;

	/* Only do net processing for rooms that have netconfigs */
	fd = open(filename, 0);
	if (fd <= 0) {
		/* syslog(LOG_DEBUG,
		   "rssclient: %s no config.\n",
		   qrbuf->QRname); */
		return;
	}

	if (server_shutting_down)
		return;

	if (fstat(fd, &statbuf) == -1) {
		syslog(LOG_DEBUG,
		       "ERROR: could not stat configfile '%s' - %s\n",
		       filename,
		       strerror(errno));
		return;
	}

	if (server_shutting_down)
		return;

	CfgData = NewStrBufPlain(NULL, statbuf.st_size + 1);

	if (StrBufReadBLOB(CfgData, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		FreeStrBuf(&CfgData);
		syslog(LOG_DEBUG, "ERROR: reading config '%s' - %s<br>\n",
		       filename, strerror(errno));
		return;
	}
	close(fd);
	if (server_shutting_down)
		return;

	CfgPtr = NULL;
	CfgType = NewStrBuf();
	Line = NewStrBufPlain(NULL, StrLength(CfgData));
	Done = 0;
	while (!Done)
	{
		Done = StrBufSipLine(Line, CfgData, &CfgPtr) == 0;
		if (StrLength(Line) > 0)
		{
			lPtr = NULL;
			StrBufExtract_NextToken(CfgType, Line, &lPtr, '|');
			if (!strcasecmp("rssclient", ChrPtr(CfgType)))
			{
				if (Count == NULL)
				{
					Count = malloc(
						sizeof(rss_room_counter));
					Count->count = 0;
				}
				Count->count ++;
				RSSAggr = (rss_aggregator *) malloc(
					sizeof(rss_aggregator));

				memset (RSSAggr, 0, sizeof(rss_aggregator));
				RSSAggr->QRnumber = qrbuf->QRnumber;
				RSSAggr->roomlist_parts = 1;
				RSSAggr->Url = NewStrBuf();

				StrBufExtract_NextToken(RSSAggr->Url,
							Line,
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
			}
		}
	}
	if (Count != NULL)
	{
		Count->QRnumber = qrbuf->QRnumber;
		pthread_mutex_lock(&RSSQueueMutex);
		syslog(LOG_DEBUG, "rssclient: [%ld] %s now starting.\n",
		       qrbuf->QRnumber, qrbuf->QRname);
		Put(RSSQueueRooms, LKEY(qrbuf->QRnumber), Count, NULL);
		pthread_mutex_unlock(&RSSQueueMutex);
	}
	FreeStrBuf(&CfgData);
	FreeStrBuf(&CfgType);
	FreeStrBuf(&Line);
}

/*
 * Scan for rooms that have RSS client requests configured
 */
void rssclient_scan(void) {
	rss_aggregator *rptr = NULL;
	void *vrptr = NULL;
	HashPos *it;
	long len;
	const char *Key;
	time_t now = time(NULL);

	/* Run no more than once every 15 minutes. */
	if ((now - last_run) < 900) {
		return;
	}

	/*
	 * This is a simple concurrency check to make sure only one rssclient
	 * run is done at a time.We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */

	if ((GetCount(RSSQueueRooms) > 0) || (GetCount(RSSFetchUrls) > 0))
		return;

	become_session(&rss_CC);
	syslog(LOG_DEBUG, "rssclient started\n");
	CtdlForEachRoom(rssclient_scan_room, NULL);

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

	syslog(LOG_DEBUG, "rssclient ended\n");
	return;
}

void rss_cleanup(void)
{
	/* citthread_mutex_destroy(&RSSQueueMutex); TODO */
	DeleteHash(&RSSFetchUrls);
	DeleteHash(&RSSQueueRooms);
}


CTDL_MODULE_INIT(rssclient)
{
	if (threading)
	{
		CtdlFillSystemContext(&rss_CC, "rssclient");
		pthread_mutex_init(&RSSQueueMutex, NULL);
		RSSQueueRooms = NewHash(1, lFlathash);
		RSSFetchUrls = NewHash(1, NULL);
		syslog(LOG_INFO, "%s\n", curl_version());
		CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER);
		CtdlRegisterEVCleanupHook(rss_cleanup);
	}
	return "rssclient";
}
