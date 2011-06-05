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
/*
 * Commit a fetched and parsed RSS item to disk
 */
void rss_save_item(rss_item *ri)
{

	struct MD5Context md5context;
	u_char rawdigest[MD5_DIGEST_LEN];
	int i;
	char utmsgid[SIZ];
	struct cdbdata *cdbut;
	struct UseTable ut;
	struct CtdlMessage *msg;
	struct recptypes *recp = NULL;
	int msglen = 0;
	StrBuf *Message;

	recp = (struct recptypes *) malloc(sizeof(struct recptypes));
	if (recp == NULL) return;
	memset(recp, 0, sizeof(struct recptypes));
	memset(&ut, 0, sizeof(struct UseTable));
	recp->recp_room = strdup(ri->roomlist);
	recp->num_room = num_tokens(ri->roomlist, '|');
	recp->recptypes_magic = RECPTYPES_MAGIC;
   
	/* Construct a GUID to use in the S_USETABLE table.
	 * If one is not present in the item itself, make one up.
	 */
	if (ri->guid != NULL) {
		StrBufSpaceToBlank(ri->guid);
		StrBufTrim(ri->guid);
		snprintf(utmsgid, sizeof utmsgid, "rss/%s", ChrPtr(ri->guid));
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
		for (i=0; i<MD5_DIGEST_LEN; i++) {
			sprintf(&utmsgid[i*2], "%02X", (unsigned char) (rawdigest[i] & 0xff));
			utmsgid[i*2] = tolower(utmsgid[i*2]);
			utmsgid[(i*2)+1] = tolower(utmsgid[(i*2)+1]);
		}
		strcat(utmsgid, "_rss2ctdl");
	}

	/* Find out if we've already seen this item */

	cdbut = cdb_fetch(CDB_USETABLE, utmsgid, strlen(utmsgid));
#ifndef DEBUG_RSS
	if (cdbut != NULL) {
		/* Item has already been seen */
		CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", utmsgid);
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		strcpy(ut.ut_msgid, utmsgid);
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
	}
	else
#endif
{
		/* Item has not been seen, so save it. */
		CtdlLogPrintf(CTDL_DEBUG, "RSS: saving item...\n");
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
		// TODO: reenable me	ExpandShortUrls(ri->description);
		msglen += 1024 + StrLength(ri->link) + StrLength(ri->description) ;

		Message = NewStrBufPlain(NULL, StrLength(ri->description));

		StrBufPlain(Message, HKEY(
			 "Content-type: text/html; charset=\"UTF-8\"\r\n\r\n"
			 "<html><body>\n"));

		StrBufAppendBuf(Message, ri->description, 0);
		StrBufAppendBufPlain(Message, HKEY("<br><br>\n"), 0);

		AppendLink(Message, ri->link, ri->linkTitle, NULL);
		AppendLink(Message, ri->reLink, ri->reLinkTitle, "Reply to this");
		StrBufAppendBufPlain(Message, HKEY("</body></html>\n"), 0);

		msg->cm_fields['M'] = SmashStrBuf(&Message);

		CtdlSubmitMsg(msg, recp, NULL, 0);
		CtdlFreeMessage(msg);

		/* write the uidl to the use table so we don't store this item again */
		strcpy(ut.ut_msgid, utmsgid);
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
	}
	free_recipients(recp);
}





/*
 * Begin a feed parse
 */
void rss_do_fetching(rssnetcfg *Cfg) {
	rsscollection *rssc;
	rss_item *ri;
		
	time_t now;

	CURL *chnd;
	AsyncIO *IO;

        now = time(NULL);

	if ((Cfg->next_poll != 0) && (now < Cfg->next_poll))
		return;


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
	chnd = IO->HttpReq.chnd;

	evcurl_handle_start(IO);
}




/*
 * Scan a room's netconfig to determine whether it is requesting any RSS feeds
 */
void rssclient_scan_room(struct ctdlroom *qrbuf, void *data)
{
	char filename[PATH_MAX];
	char buf[1024];
	char instr[32];
	FILE *fp;
	char feedurl[256];
	rssnetcfg *rncptr = NULL;
	rssnetcfg *use_this_rncptr = NULL;
	int len = 0;
	char *ptr = NULL;

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	if (CtdlThreadCheckStop())
		return;
		
	/* Only do net processing for rooms that have netconfigs */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL && !CtdlThreadCheckStop()) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, "rssclient")) {

			use_this_rncptr = NULL;

			extract_token(feedurl, buf, 1, '|', sizeof feedurl);

			/* If any other rooms have requested the same feed, then we will just add this
			 * room to the target list for that client request.
			 */
			for (rncptr=rnclist; rncptr!=NULL; rncptr=rncptr->next) {
				if (!strcmp(ChrPtr(rncptr->Url), feedurl)) {
					use_this_rncptr = rncptr;
				}
			}

			/* Otherwise create a new client request */
			if (use_this_rncptr == NULL) {
				rncptr = (rssnetcfg *) malloc(sizeof(rssnetcfg));
				memset(rncptr, 0, sizeof(rssnetcfg));
				rncptr->ItemType = RSS_UNSET;
				if (rncptr != NULL) {
					rncptr->next = rnclist;
					rncptr->Url = NewStrBufPlain(feedurl, -1);
					rncptr->rooms = NULL;
					rnclist = rncptr;
					use_this_rncptr = rncptr;
				}
			}

			/* Add the room name to the request */
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

	fclose(fp);

}

/*
 * Scan for rooms that have RSS client requests configured
 */
void rssclient_scan(void) {
	static time_t last_run = 0L;
	static int doing_rssclient = 0;
	rssnetcfg *rptr = NULL;

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

	while (rnclist != NULL && !CtdlThreadCheckStop()) {
		rss_do_fetching(rnclist);
		rptr = rnclist;
		rnclist = rnclist->next;
		if (rptr->rooms != NULL) free(rptr->rooms);
		free(rptr);
	}

	CtdlLogPrintf(CTDL_DEBUG, "rssclient ended\n");
	last_run = time(NULL);
	doing_rssclient = 0;
	return;
}


CTDL_MODULE_INIT(rssclient)
{
	if (threading)
	{
		CtdlLogPrintf(CTDL_INFO, "%s\n", curl_version());
		CtdlRegisterSessionHook(rssclient_scan, EVT_TIMER);
	}
	return "rssclient";
}
