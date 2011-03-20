/*
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

pthread_mutex_t RSSQueueMutex; /* locks the access to the following vars: */
HashList *RSSQueueRooms = NULL; /* rss_room_counter */
HashList *RSSFetchUrls = NULL; /* -> rss_aggregator; ->RefCount access to be locked too. */

eNextState RSSAggregatorTerminate(AsyncIO *IO);


struct rssnetcfg *rnclist = NULL;
void AppendLink(StrBuf *Message, StrBuf *link, StrBuf *LinkTitle, const char *Title)
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
typedef struct __networker_save_message {
	AsyncIO IO;
	struct CtdlMessage *Msg;
	struct recptypes *recp;
	rss_aggregator *Cfg;
	StrBuf *MsgGUID;
	StrBuf *Message;
	struct UseTable ut;
} networker_save_message;


void DeleteRoomReference(long QRnumber)
{
	HashPos *At;
	long HKLen;
	const char *HK;
	void *vData = NULL;
	rss_room_counter *pRoomC;

	At = GetNewHashPos(RSSQueueRooms, 0);

	GetHashPosFromKey(RSSQueueRooms, LKEY(QRnumber), At);
	GetHashPos(RSSQueueRooms, At, &HKLen, &HK, &vData);
	if (vData != NULL)
	{
		pRoomC = (rss_room_counter *) vData;
		pRoomC->count --;
		if (pRoomC->count == 0)
			DeleteEntryFromHash(RSSQueueRooms, At);
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
		while (GetNextHashPos(Cfg->OtherQRnumbers, At, &HKLen, &HK, &vData) && 
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
	if (GetHashPosFromKey(RSSFetchUrls, SKEY(Cfg->Url), At) == 0)
	{
		DeleteEntryFromHash(RSSFetchUrls, At);
	}
	DeleteHashPos(&At);
}

eNextState FreeNetworkSaveMessage (AsyncIO *IO)
{
	networker_save_message *Ctx = (networker_save_message *) IO->Data;

	pthread_mutex_lock(&RSSQueueMutex);
	Ctx->Cfg->RefCount --;

	if (Ctx->Cfg->RefCount == 0)
	{
		UnlinkRSSAggregator(Ctx->Cfg);

	}
	pthread_mutex_unlock(&RSSQueueMutex);

	CtdlFreeMessage(Ctx->Msg);
	free_recipients(Ctx->recp);
	FreeStrBuf(&Ctx->Message);
	FreeStrBuf(&Ctx->MsgGUID);
	free(Ctx);
	return eAbort;
}

eNextState AbortNetworkSaveMessage (AsyncIO *IO)
{
	return eAbort; ///TODO
}

eNextState RSSSaveMessage(AsyncIO *IO)
{
	networker_save_message *Ctx = (networker_save_message *) IO->Data;

	Ctx->Msg->cm_fields['M'] = SmashStrBuf(&Ctx->Message);

	CtdlSubmitMsg(Ctx->Msg, Ctx->recp, NULL, 0);

	/* write the uidl to the use table so we don't store this item again */
	cdb_store(CDB_USETABLE, SKEY(Ctx->MsgGUID), &Ctx->ut, sizeof(struct UseTable) );

	return eTerminateConnection;
}

eNextState RSS_FetchNetworkUsetableEntry(AsyncIO *IO)
{
	struct cdbdata *cdbut;
	networker_save_message *Ctx = (networker_save_message *) IO->Data;

	/* Find out if we've already seen this item */
	strcpy(Ctx->ut.ut_msgid, ChrPtr(Ctx->MsgGUID)); /// TODO
	Ctx->ut.ut_timestamp = time(NULL);

	cdbut = cdb_fetch(CDB_USETABLE, SKEY(Ctx->MsgGUID));
#ifndef DEBUG_RSS
	if (cdbut != NULL) {
		/* Item has already been seen */
		syslog(LOG_DEBUG, "%s has already been seen\n", ChrPtr(Ctx->MsgGUID));
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		cdb_store(CDB_USETABLE, 
			  SKEY(Ctx->MsgGUID), 
			  &Ctx->ut, sizeof(struct UseTable) );
		return eAbort;
	}
	else
#endif
	{
		NextDBOperation(IO, RSSSaveMessage);
		return eSendMore;
	}
}
void RSSQueueSaveMessage(struct CtdlMessage *Msg, struct recptypes *recp, StrBuf *MsgGUID, StrBuf *MessageBody, rss_aggregator *Cfg)
{
	networker_save_message *Ctx;

	Ctx = (networker_save_message *) malloc(sizeof(networker_save_message));
	memset(Ctx, 0, sizeof(networker_save_message));
	
	Ctx->MsgGUID = MsgGUID;
	Ctx->Message = MessageBody;
	Ctx->Msg = Msg;
	Ctx->Cfg = Cfg;
	Ctx->recp = recp;
	Ctx->IO.Data = Ctx;
	Ctx->IO.CitContext = CloneContext(CC);
	Ctx->IO.Terminate = FreeNetworkSaveMessage;
	Ctx->IO.ShutdownAbort = AbortNetworkSaveMessage;
	QueueDBOperation(&Ctx->IO, RSS_FetchNetworkUsetableEntry);
}


/*
 * Commit a fetched and parsed RSS item to disk
 */
void rss_save_item(rss_item *ri, rss_aggregator *Cfg)
{

	struct MD5Context md5context;
	u_char rawdigest[MD5_DIGEST_LEN];
	struct CtdlMessage *msg;
	struct recptypes *recp = NULL;
	int msglen = 0;
	StrBuf *Message;
	StrBuf *guid;
	StrBuf *Buf;

	recp = (struct recptypes *) malloc(sizeof(struct recptypes));
	if (recp == NULL) return;
	memset(recp, 0, sizeof(struct recptypes));
	Buf = NewStrBufDup(Cfg->rooms);
	recp->recp_room = SmashStrBuf(&Buf);
	recp->num_room = Cfg->roomlist_parts;
	recp->recptypes_magic = RECPTYPES_MAGIC;
   
	Cfg->RefCount ++;
	/* Construct a GUID to use in the S_USETABLE table.
	 * If one is not present in the item itself, make one up.
	 */
	if (ri->guid != NULL) {
		StrBufSpaceToBlank(ri->guid);
		StrBufTrim(ri->guid);
		guid = NewStrBufPlain(HKEY("rss/"));
		StrBufAppendBuf(guid, ri->guid, 0);
	}
	else {
		MD5Init(&md5context);
		if (ri->title != NULL) {
			MD5Update(&md5context, (const unsigned char*)ChrPtr(ri->title), StrLength(ri->title));
		}
		if (ri->link != NULL) {
			MD5Update(&md5context, (const unsigned char*)ChrPtr(ri->link), StrLength(ri->link));
		}
		MD5Final(rawdigest, &md5context);
		guid = NewStrBufPlain(NULL, MD5_DIGEST_LEN * 2 + 12 /* _rss2ctdl*/);
		StrBufHexEscAppend(guid, NULL, rawdigest, MD5_DIGEST_LEN);
		StrBufAppendBufPlain(guid, HKEY("_rss2ctdl"), 0);
	}

	/* translate Item into message. */
	syslog(LOG_DEBUG, "RSS: translating item...\n");
	if (ri->description == NULL) ri->description = NewStrBufPlain(HKEY(""));
	StrBufSpaceToBlank(ri->description);
	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = FMT_RFC822;

	if (ri->guid != NULL) {
		msg->cm_fields['E'] = strdup(ChrPtr(ri->guid));
	}

	if (ri->author_or_creator != NULL) {
		char *From;
		StrBuf *Encoded = NULL;
		int FromAt;
			
		From = html_to_ascii(ChrPtr(ri->author_or_creator),
				     StrLength(ri->author_or_creator), 
				     512, 0);
		StrBufPlain(ri->author_or_creator, From, -1);
		StrBufTrim(ri->author_or_creator);
		free(From);

		FromAt = strchr(ChrPtr(ri->author_or_creator), '@') != NULL;
		if (!FromAt && StrLength (ri->author_email) > 0)
		{
			StrBufRFC2047encode(&Encoded, ri->author_or_creator);
			msg->cm_fields['A'] = SmashStrBuf(&Encoded);
			msg->cm_fields['P'] = SmashStrBuf(&ri->author_email);
		}
		else
		{
			if (FromAt)
			{
				msg->cm_fields['A'] = SmashStrBuf(&ri->author_or_creator);
				msg->cm_fields['P'] = strdup(msg->cm_fields['A']);
			}
			else 
			{
				StrBufRFC2047encode(&Encoded, ri->author_or_creator);
				msg->cm_fields['A'] = SmashStrBuf(&Encoded);
				msg->cm_fields['P'] = strdup("rss@localhost");
			}
		}
	}
	else {
		msg->cm_fields['A'] = strdup("rss");
	}

	msg->cm_fields['N'] = strdup(NODENAME);
	if (ri->title != NULL) {
		long len;
		char *Sbj;
		StrBuf *Encoded, *QPEncoded;

		QPEncoded = NULL;
		StrBufSpaceToBlank(ri->title);
		len = StrLength(ri->title);
		Sbj = html_to_ascii(ChrPtr(ri->title), len, 512, 0);
		len = strlen(Sbj);
		if (Sbj[len - 1] == '\n')
		{
			len --;
			Sbj[len] = '\0';
		}
		Encoded = NewStrBufPlain(Sbj, len);
		free(Sbj);

		StrBufTrim(Encoded);
		StrBufRFC2047encode(&QPEncoded, Encoded);

		msg->cm_fields['U'] = SmashStrBuf(&QPEncoded);
		FreeStrBuf(&Encoded);
	}
	msg->cm_fields['T'] = malloc(64);
	snprintf(msg->cm_fields['T'], 64, "%ld", ri->pubdate);
	if (ri->channel_title != NULL) {
		if (StrLength(ri->channel_title) > 0) {
			msg->cm_fields['O'] = strdup(ChrPtr(ri->channel_title));
		}
	}
	if (ri->link == NULL) 
		ri->link = NewStrBufPlain(HKEY(""));

#if 0 /* temporarily disable shorter urls. */
	msg->cm_fields[TMP_SHORTER_URLS] = GetShorterUrls(ri->description);
#endif

	msglen += 1024 + StrLength(ri->link) + StrLength(ri->description) ;

	Message = NewStrBufPlain(NULL, StrLength(ri->description));

	StrBufPlain(Message, HKEY(
			    "Content-type: text/html; charset=\"UTF-8\"\r\n\r\n"
			    "<html><body>\n"));
#if 0 /* disable shorter url for now. */
	msg->cm_fields[TMP_SHORTER_URL_OFFSET] = StrLength(Message);
#endif
	StrBufAppendBuf(Message, ri->description, 0);
	StrBufAppendBufPlain(Message, HKEY("<br><br>\n"), 0);

	AppendLink(Message, ri->link, ri->linkTitle, NULL);
	AppendLink(Message, ri->reLink, ri->reLinkTitle, "Reply to this");
	StrBufAppendBufPlain(Message, HKEY("</body></html>\n"), 0);

	RSSQueueSaveMessage(msg, recp, guid, Message, Cfg);
}



/*
 * Begin a feed parse
 */
int rss_do_fetching(rss_aggregator *Cfg)
{
	rss_item *ri;
		
	time_t now;
	AsyncIO *IO;

        now = time(NULL);

	if ((Cfg->next_poll != 0) && (now < Cfg->next_poll))
		return 0;
	Cfg->RefCount = 1;

	ri = (rss_item*) malloc(sizeof(rss_item));
	memset(ri, 0, sizeof(rss_item));
	Cfg->Item = ri;
	IO = &Cfg->IO;
	IO->CitContext = CloneContext(CC);
	IO->Data = Cfg;


	syslog(LOG_DEBUG, "Fetching RSS feed <%s>\n", ChrPtr(Cfg->Url));
	ParseURL(&IO->ConnectMe, Cfg->Url, 80);
	CurlPrepareURL(IO->ConnectMe);

	if (! evcurl_init(IO, 
//			  Ctx, 
			  NULL,
			  "Citadel RSS Client",
			  ParseRSSReply, 
			  RSSAggregatorTerminate))
	{
		syslog(LOG_DEBUG, "Unable to initialize libcurl.\n");
		return 0;
	}

	evcurl_handle_start(IO);
	return 1;
}



void DeleteRssCfg(void *vptr)
{
	rss_aggregator *rncptr = (rss_aggregator *)vptr;

	FreeStrBuf(&rncptr->Url);
	FreeStrBuf(&rncptr->rooms);
	FreeStrBuf(&rncptr->CData);
	FreeStrBuf(&rncptr->Key);

	DeleteHash(&rncptr->OtherQRnumbers);

	if (rncptr->Item != NULL)
	{
		FreeStrBuf(&rncptr->Item->guid);
		FreeStrBuf(&rncptr->Item->title);
		FreeStrBuf(&rncptr->Item->link);
		FreeStrBuf(&rncptr->Item->linkTitle);
		FreeStrBuf(&rncptr->Item->reLink);
		FreeStrBuf(&rncptr->Item->reLinkTitle);
		FreeStrBuf(&rncptr->Item->description);
		FreeStrBuf(&rncptr->Item->channel_title);
		FreeStrBuf(&rncptr->Item->author_or_creator);
		FreeStrBuf(&rncptr->Item->author_url);
		FreeStrBuf(&rncptr->Item->author_email);

		free(rncptr->Item);
	}
	free(rncptr);
}

eNextState RSSAggregatorTerminate(AsyncIO *IO)
{
	rss_aggregator *rncptr = (rss_aggregator *)IO->Data;
	HashPos *At;
	long HKLen;
	const char *HK;
	void *vData;

	pthread_mutex_lock(&RSSQueueMutex);
	rncptr->RefCount --;
	if (rncptr->RefCount == 0)
	{
		UnlinkRSSAggregator(rncptr);

	}
	pthread_mutex_unlock(&RSSQueueMutex);
/*
	At = GetNewHashPos(RSSFetchUrls, 0);

	pthread_mutex_lock(&RSSQueueMutex);
	GetHashPosFromKey(RSSFetchUrls, SKEY(rncptr->Url), At);
	GetHashPos(RSSFetchUrls, At, &HKLen, &HK, &vData);
	DeleteEntryFromHash(RSSFetchUrls, At);
	pthread_mutex_unlock(&RSSQueueMutex);

	DeleteHashPos(&At);
*/
	return eAbort;
}

/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data)
{
	StrBuf *CfgData;
	StrBuf *CfgType;
	StrBuf *Line;
	rss_room_counter *Count = NULL;
	struct stat statbuf;
	char filename[PATH_MAX];
	int  fd;
	int Done;
	rss_aggregator *rncptr = NULL;
	rss_aggregator *use_this_rncptr = NULL;
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

	if (CtdlThreadCheckStop())
		return;
		
	/* Only do net processing for rooms that have netconfigs */
	fd = open(filename, 0);
	if (fd <= 0) {
		//syslog(LOG_DEBUG, "rssclient: %s no config.\n", qrbuf->QRname);
		return;
	}
	if (CtdlThreadCheckStop())
		return;
	if (fstat(fd, &statbuf) == -1) {
		syslog(LOG_DEBUG, "ERROR: could not stat configfile '%s' - %s\n",
			filename, strerror(errno));
		return;
	}
	if (CtdlThreadCheckStop())
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
	if (CtdlThreadCheckStop())
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
			Count = malloc(sizeof(rss_room_counter));
			Count->count = 0;
		    }
		    Count->count ++;
		    rncptr = (rss_aggregator *) malloc(sizeof(rss_aggregator));
		    memset (rncptr, 0, sizeof(rss_aggregator));
		    rncptr->roomlist_parts = 1;
		    rncptr->Url = NewStrBuf();
		    StrBufExtract_NextToken(rncptr->Url, Line, &lPtr, '|');

		    pthread_mutex_lock(&RSSQueueMutex);
		    GetHash(RSSFetchUrls, SKEY(rncptr->Url), &vptr);
		    use_this_rncptr = (rss_aggregator *)vptr;
		    if (use_this_rncptr != NULL)
		    {
			    /* mustn't attach to an active session */
			    if (use_this_rncptr->RefCount > 0)
			    {
				    DeleteRssCfg(rncptr);
				    Count->count--;
			    }
			    else 
			    {
				    long *QRnumber;
				    StrBufAppendBufPlain(use_this_rncptr->rooms, 
							 qrbuf->QRname, 
							 -1, 0);
				    if (use_this_rncptr->roomlist_parts == 1)
				    {
					    use_this_rncptr->OtherQRnumbers = NewHash(1, lFlathash);
				    }
				    QRnumber = (long*)malloc(sizeof(long));
				    *QRnumber = qrbuf->QRnumber;
				    Put(use_this_rncptr->OtherQRnumbers, LKEY(qrbuf->QRnumber), QRnumber, NULL);
				    use_this_rncptr->roomlist_parts++;
			    }
			    pthread_mutex_unlock(&RSSQueueMutex);
			    continue;
		    }
		    pthread_mutex_unlock(&RSSQueueMutex);

		    rncptr->ItemType = RSS_UNSET;
				
		    rncptr->rooms = NewStrBufPlain(qrbuf->QRname, -1);

		    pthread_mutex_lock(&RSSQueueMutex);
		    Put(RSSFetchUrls, SKEY(rncptr->Url), rncptr, DeleteRssCfg);
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
	static int doing_rssclient = 0;
	rss_aggregator *rptr = NULL;
	void *vrptr = NULL;
	HashPos  *it;
	long len;
	const char *Key;

	/* Run no more than once every 15 minutes. * /
	if ((time(NULL) - last_run) < 900) {
		return;
	}
*/
	/*
	 * This is a simple concurrency check to make sure only one rssclient run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_rssclient) return;
	doing_rssclient = 1;

	syslog(LOG_DEBUG, "rssclient started\n");
	CtdlForEachRoom(rssclient_scan_room, NULL);

	pthread_mutex_lock(&RSSQueueMutex);

	it = GetNewHashPos(RSSFetchUrls, 0);
	while (GetNextHashPos(RSSFetchUrls, it, &len, &Key, &vrptr) && 
	       (vrptr != NULL)) {
		rptr = (rss_aggregator *)vrptr;
		if (rptr->RefCount == 0) 
			if (!rss_do_fetching(rptr))
				UnlinkRSSAggregator(rptr);
	}
	DeleteHashPos(&it);
	pthread_mutex_unlock(&RSSQueueMutex);

	syslog(LOG_DEBUG, "rssclient ended\n");
	doing_rssclient = 0;
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
		pthread_mutex_init(&RSSQueueMutex, NULL);
		RSSQueueRooms = NewHash(1, lFlathash);
		RSSFetchUrls = NewHash(1, NULL);
		syslog(LOG_INFO, "%s\n", curl_version());
		CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER);
                CtdlRegisterCleanupHook(rss_cleanup);
	}
	return "rssclient";
}
