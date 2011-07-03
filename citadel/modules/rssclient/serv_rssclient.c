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
	StrBuf *MsgGUID;
	StrBuf *Message;
	struct UseTable ut;
} networker_save_message;

eNextState FreeNetworkSaveMessage (AsyncIO *IO)
{
	networker_save_message *Ctx = (networker_save_message *) IO->Data;

	CtdlFreeMessage(Ctx->Msg);
	free_recipients(Ctx->recp);
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

// TODO: relink me:	ExpandShortUrls(ri->description);

eNextState FetchNetworkUsetableEntry(AsyncIO *IO)
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
		CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", ChrPtr(Ctx->MsgGUID));
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		cdb_store(CDB_USETABLE, 
			  SKEY(Ctx->MsgGUID), 
			  &Ctx->ut, sizeof(struct UseTable) );
		return eTerminateConnection;
	}
	else
#endif
	{
		NextDBOperation(IO, RSSSaveMessage);
		return eSendMore;
	}
}
void RSSQueueSaveMessage(struct CtdlMessage *Msg, struct recptypes *recp, StrBuf *MsgGUID, StrBuf *MessageBody)
{
	networker_save_message *Ctx;

	Ctx = (networker_save_message *) malloc(sizeof(networker_save_message));
	memset(Ctx, 0, sizeof(networker_save_message));
	
	Ctx->MsgGUID = MsgGUID;
	Ctx->Message = MessageBody;
	Ctx->Msg = Msg;
	Ctx->recp = recp;
	Ctx->IO.Data = Ctx;
	Ctx->IO.CitContext = CloneContext(CC);
	Ctx->IO.Terminate = FreeNetworkSaveMessage;
	Ctx->IO.ShutdownAbort = AbortNetworkSaveMessage;
	QueueDBOperation(&Ctx->IO, FetchNetworkUsetableEntry);
}


/*
 * Commit a fetched and parsed RSS item to disk
 */
void rss_save_item(rss_item *ri)
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
	Buf = NewStrBufDup(ri->roomlist);
	recp->recp_room = SmashStrBuf(&Buf);
	recp->num_room = ri->roomlist_parts;
	recp->recptypes_magic = RECPTYPES_MAGIC;
   
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
	CtdlLogPrintf(CTDL_DEBUG, "RSS: translating item...\n");
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
				msg->cm_fields['P'] = SmashStrBuf(&ri->author_or_creator);
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

	RSSQueueSaveMessage(msg, recp, guid, Message);
}



/*
 * Begin a feed parse
 */
void rss_do_fetching(rssnetcfg *Cfg) {
	rsscollection *rssc;
	rss_item *ri;
		
	time_t now;
	AsyncIO *IO;

        now = time(NULL);

	if ((Cfg->next_poll != 0) && (now < Cfg->next_poll))
		return;
	Cfg->Attached = 1;

	ri = (rss_item*) malloc(sizeof(rss_item));
	rssc = (rsscollection*) malloc(sizeof(rsscollection));
	memset(ri, 0, sizeof(rss_item));
	memset(rssc, 0, sizeof(rsscollection));
	rssc->Item = ri;
	rssc->Cfg = Cfg;
	IO = &rssc->IO;
	IO->CitContext = CloneContext(CC);
	IO->Data = rssc;
	ri->roomlist = Cfg->rooms;


	CtdlLogPrintf(CTDL_DEBUG, "Fetching RSS feed <%s>\n", ChrPtr(Cfg->Url));
	ParseURL(&IO->ConnectMe, Cfg->Url, 80);
	CurlPrepareURL(IO->ConnectMe);

	if (! evcurl_init(IO, 
//			  Ctx, 
			  NULL,
			  "Citadel RSS Client",
			  ParseRSSReply))
	{
		CtdlLogPrintf(CTDL_ALERT, "Unable to initialize libcurl.\n");
//		goto abort;
	}

	evcurl_handle_start(IO);
}

citthread_mutex_t RSSQueueMutex; /* locks the access to the following vars: */
HashList *RSSQueueRooms = NULL;
HashList *RSSFetchUrls = NULL;


/*
	while (fgets(buf, sizeof buf, fp) != NULL && !CtdlThreadCheckStop()) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, "rssclient")) {

			use_this_rncptr = NULL;

			extract_token(feedurl, buf, 1, '|', sizeof feedurl);

			/* If any other rooms have requested the same feed, then we will just add this
			 * room to the target list for that client request.
			 * / TODO: how do we do this best?
			for (rncptr=rnclist; rncptr!=NULL; rncptr=rncptr->next) {
				if (!strcmp(ChrPtr(rncptr->Url), feedurl)) {
					use_this_rncptr = rncptr;
				}
			}
			* /
			/* Otherwise create a new client request * /
			if (use_this_rncptr == NULL) {
				rncptr = (rssnetcfg *) malloc(sizeof(rssnetcfg));
				memset(rncptr, 0, sizeof(rssnetcfg));
				rncptr->ItemType = RSS_UNSET;

				rncptr->Url = NewStrBufPlain(feedurl, -1);
				rncptr->rooms = NULL;
				rnclist = rncptr;
				use_this_rncptr = rncptr;

			}

			/* Add the room name to the request * /
			if (use_this_rncptr != NULL) {
				if (use_this_rncptr->rooms == NULL) {
					rncptr->rooms = strdup(qrbuf->QRname);
				}
				else {
					len = strlen(use_this_rncptr->rooms) + strlen(qrbuf->QRname) + 5;
					ptr = realloc(use_this_rncptr->rooms, len);
					if (ptr != NULL) {
						strcat(ptr, "|");
						strcat(ptr, qrbuf->QRname);
						use_this_rncptr->rooms = ptr;
					}
				}
			}
		}

	}
			*/
typedef struct __RoomCounter {
	int count;
	long QRnumber;
} RoomCounter;



void DeleteRssCfg(void *vptr)
{
	rssnetcfg *rncptr = (rssnetcfg *)vptr;

	FreeStrBuf(&rncptr->Url);
	FreeStrBuf(&rncptr->rooms);
	free(rncptr);
}


/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data)
{
	StrBuf *CfgData;
	StrBuf *CfgType;
	StrBuf *Line;
	RoomCounter *Count = NULL;
	struct stat statbuf;
	char filename[PATH_MAX];
	//char buf[1024];
	//char instr[32];
	int  fd;
	int Done;
	//char feedurl[256];
	rssnetcfg *rncptr = NULL;
	rssnetcfg *use_this_rncptr = NULL;
	//int len = 0;
	//char *ptr = NULL;
	void *vptr;
	const char *CfgPtr, *lPtr;
	const char *Err;

	citthread_mutex_lock(&RSSQueueMutex);
	if (GetHash(RSSQueueRooms, LKEY(qrbuf->QRnumber), &vptr))
	{
		//CtdlLogPrintf(CTDL_DEBUG, "rssclient: %s already in progress.\n", qrbuf->QRname);
		citthread_mutex_unlock(&RSSQueueMutex);
		return;
	}
	citthread_mutex_unlock(&RSSQueueMutex);

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	if (CtdlThreadCheckStop())
		return;
		
	/* Only do net processing for rooms that have netconfigs */
	fd = open(filename, 0);
	if (fd <= 0) {
		//CtdlLogPrintf(CTDL_DEBUG, "rssclient: %s no config.\n", qrbuf->QRname);
		return;
	}
	if (CtdlThreadCheckStop())
		return;
	if (fstat(fd, &statbuf) == -1) {
		CtdlLogPrintf(CTDL_DEBUG,  "ERROR: could not stat configfile '%s' - %s\n",
			filename, strerror(errno));
		return;
	}
	if (CtdlThreadCheckStop())
		return;
	CfgData = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(CfgData, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		FreeStrBuf(&CfgData);
		CtdlLogPrintf(CTDL_DEBUG,  "ERROR: reading config '%s' - %s<br>\n",
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
		if (!strcmp("rssclient", ChrPtr(CfgType)))
		{
		    if (Count == NULL)
		    {
			Count = malloc(sizeof(RoomCounter));
			Count->count = 0;
		    }
		    Count->count ++;
		    rncptr = (rssnetcfg *) malloc(sizeof(rssnetcfg));
		    memset (rncptr, 0, sizeof(rssnetcfg));
		    rncptr->roomlist_parts = 1;
		    rncptr->Url = NewStrBuf();
		    StrBufExtract_NextToken(rncptr->Url, Line, &lPtr, '|');

		    citthread_mutex_lock(&RSSQueueMutex);
		    GetHash(RSSFetchUrls, SKEY(rncptr->Url), &vptr);
		    use_this_rncptr = (rssnetcfg *)vptr;
		    citthread_mutex_unlock(&RSSQueueMutex);

		    if (use_this_rncptr != NULL)
		    {
			/* mustn't attach to an active session */
			if (use_this_rncptr->Attached == 1)
			{
			    DeleteRssCfg(rncptr);
			}
			else 
			{
				StrBufAppendBufPlain(use_this_rncptr->rooms, 
						     qrbuf->QRname, 
						     -1, 0);
				use_this_rncptr->roomlist_parts++;
			}

			continue;
		    }

		    rncptr->ItemType = RSS_UNSET;
				
		    rncptr->rooms = NewStrBufPlain(qrbuf->QRname, -1);

		    citthread_mutex_lock(&RSSQueueMutex);
		    Put(RSSFetchUrls, SKEY(rncptr->Url), rncptr, DeleteRssCfg);
		    citthread_mutex_unlock(&RSSQueueMutex);
		}
	    }
	}
	if (Count != NULL)
	{
		Count->QRnumber = qrbuf->QRnumber;
		citthread_mutex_lock(&RSSQueueMutex);
		Put(RSSQueueRooms, LKEY(qrbuf->QRnumber), Count, NULL);
		citthread_mutex_unlock(&RSSQueueMutex);
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
	rssnetcfg *rptr = NULL;
	void *vrptr = NULL;
	HashPos  *it;
	long len;
	const char *Key;

	/*
	 * This is a simple concurrency check to make sure only one rssclient run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_rssclient) return;
	doing_rssclient = 1;

	CtdlLogPrintf(CTDL_DEBUG, "rssclient started\n");
	CtdlForEachRoom(rssclient_scan_room, NULL);

	citthread_mutex_lock(&RSSQueueMutex);

	it = GetNewHashPos(RSSQueueRooms, 0);
	while (GetNextHashPos(RSSFetchUrls, it, &len, &Key, &vrptr) && 
	       (vrptr != NULL)) {
		rptr = (rssnetcfg *)vrptr;
		if (!rptr->Attached) rss_do_fetching(rptr);
	}
	DeleteHashPos(&it);
	citthread_mutex_unlock(&RSSQueueMutex);

	CtdlLogPrintf(CTDL_DEBUG, "rssclientscheduler ended\n");
	doing_rssclient = 0;
	return;
}

void RSSCleanup(void)
{
	citthread_mutex_destroy(&RSSQueueMutex);
	DeleteHash(&RSSFetchUrls);
	DeleteHash(&RSSQueueRooms);
}


CTDL_MODULE_INIT(rssclient)
{
	if (threading)
	{
		citthread_mutex_init(&RSSQueueMutex, NULL);
		RSSQueueRooms = NewHash(1, Flathash);
		RSSFetchUrls = NewHash(1, NULL);
		CtdlLogPrintf(CTDL_INFO, "%s\n", curl_version());
		CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER);
	}
	return "rssclient";
}
