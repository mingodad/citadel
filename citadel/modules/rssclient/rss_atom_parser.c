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
#include "clientsocket.h"
#include "msgbase.h"
#include "parsedate.h"
#include "database.h"
#include "citadel_dirs.h"
#include "md5.h"
#include "context.h"
#include "event_client.h"
#include "rss_atom_parser.h"

extern pthread_mutex_t RSSQueueMutex;

HashList *StartHandlers = NULL;
HashList *EndHandlers = NULL;
HashList *KnownNameSpaces = NULL;
void AddRSSStartHandler(rss_handler_func Handler, int Flags, const char *key, long len)
{
	rss_xml_handler *h;
	h = (rss_xml_handler*) malloc(sizeof (rss_xml_handler));
	h->Flags = Flags;
	h->Handler = Handler;
	Put(StartHandlers, key, len, h, NULL);
}
void AddRSSEndHandler(rss_handler_func Handler, int Flags, const char *key, long len)
{
	rss_xml_handler *h;
	h = (rss_xml_handler*) malloc(sizeof (rss_xml_handler));
	h->Flags = Flags;
	h->Handler = Handler;
	Put(EndHandlers, key, len, h, NULL);
}



/*
 * Convert an RDF/RSS datestamp into a time_t
 */
time_t rdf_parsedate(const char *p)
{
	struct tm tm;
	time_t t = 0;

	if (!p) return 0L;
	if (strlen(p) < 10) return 0L;

	memset(&tm, 0, sizeof tm);

	/*
	 * If the timestamp appears to be in W3C datetime format, try to
	 * parse it.  See also: http://www.w3.org/TR/NOTE-datetime
	 *
	 * This code, along with parsedate.c, is a potential candidate for
	 * moving into libcitadel.
	 */
	if ( (p[4] == '-') && (p[7] == '-') ) {
		tm.tm_year = atoi(&p[0]) - 1900;
		tm.tm_mon = atoi(&p[5]) - 1;
		tm.tm_mday = atoi(&p[8]);
		if ( (p[10] == 'T') && (p[13] == ':') ) {
			tm.tm_hour = atoi(&p[11]);
			tm.tm_min = atoi(&p[14]);
		}
		return mktime(&tm);
	}

	/* hmm... try RFC822 date stamp format */

	t = parsedate(p);
	if (t > 0) return(t);

	/* yeesh.  ok, just return the current date and time. */
	return(time(NULL));
}

void flush_rss_item(rss_item *ri)
{
	/* Initialize the feed item data structure */
	FreeStrBuf(&ri->guid);
	FreeStrBuf(&ri->title);
	FreeStrBuf(&ri->link);
	FreeStrBuf(&ri->author_or_creator);
	FreeStrBuf(&ri->author_email);
	FreeStrBuf(&ri->author_url);
	FreeStrBuf(&ri->description);
}

void rss_xml_start(void *data, const char *supplied_el, const char **attr)
{
	rss_xml_handler *h;
	rss_aggregator  *rssc = (rss_aggregator*) data;
	rss_item        *ri = rssc->Item;
	void            *pv;
	const char      *pel;
	char            *sep = NULL;

	/* Axe the namespace, we don't care about it */
///	CtdlLogPrintf(0, "RSS: supplied el %d: %s...\n", rssc->Cfg->ItemType, supplied_el);
	pel = supplied_el;
	while (sep = strchr(pel, ':'), sep) {
		pel = sep + 1;
	}

	if (pel != supplied_el)
	{
		void *v;
		
		if (!GetHash(KnownNameSpaces, 
			     supplied_el, 
			     pel - supplied_el - 1,
			     &v))
		{
#ifdef DEBUG_RSS
			CtdlLogPrintf(0, "RSS: START ignoring because of wrong namespace [%s] = [%s]\n", 
				      supplied_el);
#endif
			return;
		}
	}

	StrBufPlain(rssc->Key, pel, -1);
	StrBufLowerCase(rssc->Key);
	if (GetHash(StartHandlers, SKEY(rssc->Key), &pv))
	{
		rssc->Current = h = (rss_xml_handler*) pv;

		if (((h->Flags & RSS_UNSET) != 0) && 
		    (rssc->ItemType == RSS_UNSET))
		{
			h->Handler(rssc->CData, ri, rssc, attr);
		}
		else if (((h->Flags & RSS_RSS) != 0) &&
		    (rssc->ItemType == RSS_RSS))
		{
			h->Handler(rssc->CData, ri, rssc, attr);
		}
		else if (((h->Flags & RSS_ATOM) != 0) &&
			 (rssc->ItemType == RSS_ATOM))
		{
			h->Handler(rssc->CData, ri, rssc, attr);			
		}
#ifdef DEBUG_RSS
		else 
			CtdlLogPrintf(0, "RSS: START unhandled: [%s] [%s]...\n", pel, supplied_el);
#endif
	}
#ifdef DEBUG_RSS
	else 
		CtdlLogPrintf(0, "RSS: START unhandled: [%s] [%s]...\n", pel,  supplied_el);
#endif
}

void rss_xml_end(void *data, const char *supplied_el)
{
	rss_xml_handler *h;
	rss_aggregator  *rssc = (rss_aggregator*) data;
	rss_item        *ri = rssc->Item;
	const char      *pel;
	char            *sep = NULL;
	void            *pv;

	/* Axe the namespace, we don't care about it */
	pel = supplied_el;
	while (sep = strchr(pel, ':'), sep) {
		pel = sep + 1;
	}
//	CtdlLogPrintf(0, "RSS: END %s...\n", el);
	if (pel != supplied_el)
	{
		void *v;
		
		if (!GetHash(KnownNameSpaces, 
			     supplied_el, 
			     pel - supplied_el - 1,
			     &v))
		{
#ifdef DEBUG_RSS
			CtdlLogPrintf(0, "RSS: END ignoring because of wrong namespace [%s] = [%s]\n", 
				      supplied_el, ChrPtr(rssc->CData));
#endif
			FlushStrBuf(rssc->CData);
			return;
		}
	}

	StrBufPlain(rssc->Key, pel, -1);
	StrBufLowerCase(rssc->Key);
	if (GetHash(EndHandlers, SKEY(rssc->Key), &pv))
	{
		h = (rss_xml_handler*) pv;

		if (((h->Flags & RSS_UNSET) != 0) && 
		    (rssc->ItemType == RSS_UNSET))
		{
			h->Handler(rssc->CData, ri, rssc, NULL);
		}
		else if (((h->Flags & RSS_RSS) != 0) &&
		    (rssc->ItemType == RSS_RSS))
		{
			h->Handler(rssc->CData, ri, rssc, NULL);
		}
		else if (((h->Flags & RSS_ATOM) != 0) &&
			 (rssc->ItemType == RSS_ATOM))
		{
			h->Handler(rssc->CData, ri, rssc, NULL);
		}
#ifdef DEBUG_RSS
		else 
			CtdlLogPrintf(0, "RSS: END   unhandled: [%s]  [%s] = [%s]...\n", pel, supplied_el, ChrPtr(rssc->CData));
#endif
	}
#ifdef DEBUG_RSS
	else 
		CtdlLogPrintf(0, "RSS: END   unhandled: [%s]  [%s] = [%s]...\n", pel, supplied_el, ChrPtr(rssc->CData));
#endif
	FlushStrBuf(rssc->CData);
	rssc->Current = NULL;
}





void RSS_item_rss_start (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	syslog(LOG_DEBUG, "RSS: This is an RSS feed.\n");
	Cfg->ItemType = RSS_RSS;
}

void RSS_item_rdf_start(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	syslog(LOG_DEBUG, "RSS: This is an RDF feed.\n");
	Cfg->ItemType = RSS_RSS;
}

void ATOM_item_feed_start(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	syslog(LOG_DEBUG, "RSS: This is an ATOM feed.\n");
	Cfg->ItemType = RSS_ATOM;
}


void RSS_item_item_start(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	ri->item_tag_nesting ++;
	flush_rss_item(ri);
}

void ATOM_item_entry_start(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
/* Atom feed... */
	ri->item_tag_nesting ++;
	flush_rss_item(ri);
}

void ATOM_item_link_start (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	int i;
	const char *pHref = NULL;
	const char *pType = NULL;
	const char *pRel = NULL;
	const char *pTitle = NULL;

	for (i = 0; Attr[i] != NULL; i+=2)
	{
		if (!strcmp(Attr[i], "href"))
		{
			pHref = Attr[i+1];
		}
		else if (!strcmp(Attr[i], "rel"))
		{
			pRel = Attr[i+1];
		}
		else if (!strcmp(Attr[i], "type"))
		{
			pType = Attr[i+1];
		}
		else if (!strcmp(Attr[i], "title"))
		{
			pTitle = Attr[i+1];
		}
	}
	if (pHref == NULL)
		return; /* WHUT? Pointing... where? */
	if ((pType != NULL) && !strcasecmp(pType, "application/atom+xml"))
		return; /* these just point to other rss resources, we're not interested in them. */
	if (pRel != NULL)
	{
		if (!strcasecmp (pRel, "replies"))
		{
			NewStrBufDupAppendFlush(&ri->reLink, NULL, pHref, -1);
			StrBufTrim(ri->link);
			NewStrBufDupAppendFlush(&ri->reLinkTitle, NULL, pTitle, -1);
		}
		else if (!strcasecmp(pRel, "alternate")) /* Alternative representation of this Item... */
		{
			NewStrBufDupAppendFlush(&ri->link, NULL, pHref, -1);
			StrBufTrim(ri->link);
			NewStrBufDupAppendFlush(&ri->linkTitle, NULL, pTitle, -1);

		}
#if 0 /* these are also defined, but dunno what to do with them.. */
		else if (!strcasecmp(pRel, "related"))
		{
		}
		else if (!strcasecmp(pRel, "self"))
		{
		}
		else if (!strcasecmp(pRel, "enclosure"))
		{/* this reference can get big, and is probably the full article... */
		}
		else if (!strcasecmp(pRel, "via"))
		{/* this article was provided via... */
		}
#endif
	}
	else if (StrLength(ri->link) == 0)
	{
		NewStrBufDupAppendFlush(&ri->link, NULL, pHref, -1);
		StrBufTrim(ri->link);
		NewStrBufDupAppendFlush(&ri->linkTitle, NULL, pTitle, -1);
	}
}




void ATOMRSS_item_title_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if ((ri->item_tag_nesting == 0) && (StrLength(CData) > 0)) {
		NewStrBufDupAppendFlush(&ri->channel_title, CData, NULL, 0);
		StrBufTrim(ri->channel_title);
	}
}

void RSS_item_guid_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->guid, CData, NULL, 0);
	}
}

void ATOM_item_id_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->guid, CData, NULL, 0);
	}
}


void RSS_item_link_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->link, CData, NULL, 0);
		StrBufTrim(ri->link);
	}
}
void RSS_item_relink_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->reLink, CData, NULL, 0);
		StrBufTrim(ri->reLink);
	}
}

void RSSATOM_item_title_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->title, CData, NULL, 0);
		StrBufTrim(ri->title);
	}
}

void ATOM_item_content_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	long olen = StrLength (ri->description);
	long clen = StrLength (CData);
	if (clen > 0) 
	{
		if (olen == 0) {
			NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
			StrBufTrim(ri->description);
		}
		else if (olen < clen) {
			FlushStrBuf(ri->description);
			NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
			StrBufTrim(ri->description);
		}
	}
}
void ATOM_item_summary_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	/* this can contain an abstract of the article. but we don't want to verwrite a full document if we already have it. */
	if ((StrLength(CData) > 0) && (StrLength(ri->description) == 0))
	{
		NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
		StrBufTrim(ri->description);
	}
}

void RSS_item_description_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	long olen = StrLength (ri->description);
	long clen = StrLength (CData);
	if (clen > 0) 
	{
		if (olen == 0) {
			NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
			StrBufTrim(ri->description);
		}
		else if (olen < clen) {
			FlushStrBuf(ri->description);
			NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
			StrBufTrim(ri->description);
		}
	}
}

void ATOM_item_published_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{		  
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}

void ATOM_item_updated_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}

void RSS_item_pubdate_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}


void RSS_item_date_end (StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}



void RSS_item_author_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}


void ATOM_item_name_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}

void ATOM_item_email_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_email, CData, NULL, 0);
		StrBufTrim(ri->author_email);
	}
}

void RSS_item_creator_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if ((StrLength(CData) > 0) && 
	    (StrLength(ri->author_or_creator) == 0))
	{
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}


void ATOM_item_uri_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_url, CData, NULL, 0);
		StrBufTrim(ri->author_url);
	}
}

void RSS_item_item_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	--ri->item_tag_nesting;
	rss_save_item(ri, Cfg);
}


void ATOM_item_entry_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
	--ri->item_tag_nesting;
	rss_save_item(ri, Cfg);
}

void RSS_item_rss_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
//		syslog(LOG_DEBUG, "End of feed detected.  Closing parser.\n");
	ri->done_parsing = 1;
	
}
void RSS_item_rdf_end(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
//		syslog(LOG_DEBUG, "End of feed detected.  Closing parser.\n");
	ri->done_parsing = 1;
}


void RSSATOM_item_ignore(StrBuf *CData, rss_item *ri, rss_aggregator *Cfg, const char** Attr)
{
}



/*
 * This callback stores up the data which appears in between tags.
 */
void rss_xml_cdata_start(void *data) 
{
	rss_aggregator *rssc = (rss_aggregator*) data;

	FlushStrBuf(rssc->CData);
}

void rss_xml_cdata_end(void *data) 
{
}
void rss_xml_chardata(void *data, const XML_Char *s, int len) 
{
	rss_aggregator *rssc = (rss_aggregator*) data;

	StrBufAppendBufPlain (rssc->CData, s, len, 0);
}

/*
 * Callback function for passing libcurl's output to expat for parsing
 */
size_t rss_libcurl_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	XML_Parse((XML_Parser)stream, ptr, (size * nmemb), 0);
	return (size*nmemb);
}



eNextState ParseRSSReply(AsyncIO *IO)
{
	rss_aggregator *rssc;
	rss_item *ri;
	const char *at;
	char *ptr;
	long len;

	rssc = IO->Data;
	pthread_mutex_lock(&RSSQueueMutex);
	rssc->RefCount ++;
	pthread_mutex_unlock(&RSSQueueMutex);
	ri = rssc->Item;
	rssc->CData = NewStrBufPlain(NULL, SIZ);
	rssc->Key = NewStrBuf();
	at = NULL;
	StrBufSipLine(rssc->Key, IO->HttpReq.ReplyData, &at);
	ptr = NULL;

#define encoding "encoding=\""
	ptr = strstr(ChrPtr(rssc->Key), encoding);
	if (ptr != NULL)
	{
		char *pche;

		ptr += sizeof (encoding) - 1;
		pche = strchr(ptr, '"');
		if (pche != NULL)
			StrBufCutAt(rssc->Key, -1, pche);
		else 
			ptr = "UTF-8";
	}
	else
		ptr = "UTF-8";

	syslog(LOG_DEBUG, "RSS: Now parsing [%s] \n", ChrPtr(rssc->Url));

	rssc->xp = XML_ParserCreateNS(ptr, ':');
	if (!rssc->xp) {
		syslog(LOG_DEBUG, "Cannot create XML parser!\n");
		goto shutdown;
	}
	FlushStrBuf(rssc->Key);

	XML_SetElementHandler(rssc->xp, rss_xml_start, rss_xml_end);
	XML_SetCharacterDataHandler(rssc->xp, rss_xml_chardata);
	XML_SetUserData(rssc->xp, rssc);
	XML_SetCdataSectionHandler(rssc->xp,
				   rss_xml_cdata_start,
				   rss_xml_cdata_end);


	len = StrLength(IO->HttpReq.ReplyData);
	ptr = SmashStrBuf(&IO->HttpReq.ReplyData);
	XML_Parse(rssc->xp, ptr, len, 0);
	free (ptr);
	if (ri->done_parsing == 0)
		XML_Parse(rssc->xp, "", 0, 1);


	syslog(LOG_DEBUG, "RSS: XML Status [%s] \n", 
		      XML_ErrorString(
			      XML_GetErrorCode(rssc->xp)));

shutdown:
	XML_ParserFree(rssc->xp);

	flush_rss_item(ri);
	FreeStrBuf(&rssc->CData);
	FreeStrBuf(&rssc->Key);

        ///Cfg->next_poll = time(NULL) + config.c_net_freq; 

	pthread_mutex_lock(&RSSQueueMutex);
	rssc->RefCount --;
	pthread_mutex_unlock(&RSSQueueMutex);
	return eTerminateConnection;
}


void rss_parser_cleanup(void)
{
        DeleteHash(&StartHandlers);
        DeleteHash(&EndHandlers);
	DeleteHash(&KnownNameSpaces);
}


CTDL_MODULE_INIT(rssparser)
{
	if (!threading)
	{
		StartHandlers = NewHash(1, NULL);
		EndHandlers = NewHash(1, NULL);

		AddRSSStartHandler(RSS_item_rss_start,     RSS_UNSET, HKEY("rss"));
		AddRSSStartHandler(RSS_item_rdf_start,     RSS_UNSET, HKEY("rdf"));
		AddRSSStartHandler(ATOM_item_feed_start,    RSS_UNSET, HKEY("feed"));
		AddRSSStartHandler(RSS_item_item_start,    RSS_RSS, HKEY("item"));
		AddRSSStartHandler(ATOM_item_entry_start,  RSS_ATOM, HKEY("entry"));
		AddRSSStartHandler(ATOM_item_link_start,   RSS_ATOM, HKEY("link"));

		AddRSSEndHandler(ATOMRSS_item_title_end,   RSS_ATOM|RSS_RSS|RSS_REQUIRE_BUF, HKEY("title"));
		AddRSSEndHandler(RSS_item_guid_end,        RSS_RSS|RSS_REQUIRE_BUF, HKEY("guid"));
		AddRSSEndHandler(ATOM_item_id_end,         RSS_ATOM|RSS_REQUIRE_BUF, HKEY("id"));
		AddRSSEndHandler(RSS_item_link_end,        RSS_RSS|RSS_REQUIRE_BUF, HKEY("link"));
#if 0 
// hm, rss to the comments of that blog, might be interesting in future, but... 
		AddRSSEndHandler(RSS_item_relink_end,      RSS_RSS|RSS_REQUIRE_BUF, HKEY("commentrss"));
// comment count...
		AddRSSEndHandler(RSS_item_relink_end,      RSS_RSS|RSS_REQUIRE_BUF, HKEY("comments"));
#endif
		AddRSSEndHandler(RSSATOM_item_title_end,   RSS_ATOM|RSS_RSS|RSS_REQUIRE_BUF, HKEY("title"));
		AddRSSEndHandler(ATOM_item_content_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("content"));
		AddRSSEndHandler(RSS_item_description_end, RSS_RSS|RSS_ATOM|RSS_REQUIRE_BUF, HKEY("encoded"));
		AddRSSEndHandler(ATOM_item_summary_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("summary"));
		AddRSSEndHandler(RSS_item_description_end, RSS_RSS|RSS_REQUIRE_BUF, HKEY("description"));
		AddRSSEndHandler(ATOM_item_published_end,  RSS_ATOM|RSS_REQUIRE_BUF, HKEY("published"));
		AddRSSEndHandler(ATOM_item_updated_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("updated"));
		AddRSSEndHandler(RSS_item_pubdate_end,     RSS_RSS|RSS_REQUIRE_BUF, HKEY("pubdate"));
		AddRSSEndHandler(RSS_item_date_end,        RSS_RSS|RSS_REQUIRE_BUF, HKEY("date"));
		AddRSSEndHandler(RSS_item_author_end,      RSS_RSS|RSS_REQUIRE_BUF, HKEY("author"));
		AddRSSEndHandler(RSS_item_creator_end,     RSS_RSS|RSS_REQUIRE_BUF, HKEY("creator"));
/* <author> */
		AddRSSEndHandler(ATOM_item_email_end,      RSS_ATOM|RSS_REQUIRE_BUF, HKEY("email"));
		AddRSSEndHandler(ATOM_item_name_end,       RSS_ATOM|RSS_REQUIRE_BUF, HKEY("name"));
		AddRSSEndHandler(ATOM_item_uri_end,        RSS_ATOM|RSS_REQUIRE_BUF, HKEY("uri"));
/* </author> */
		AddRSSEndHandler(RSS_item_item_end,        RSS_RSS, HKEY("item"));
		AddRSSEndHandler(RSS_item_rss_end,         RSS_RSS, HKEY("rss"));
		AddRSSEndHandler(RSS_item_rdf_end,         RSS_RSS, HKEY("rdf"));
		AddRSSEndHandler(ATOM_item_entry_end,      RSS_ATOM, HKEY("entry"));


/* at the start of atoms: <seq> <li>link to resource</li></seq> ignore them. */
		AddRSSStartHandler(RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("seq"));
		AddRSSEndHandler  (RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("seq"));
		AddRSSStartHandler(RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("li"));
		AddRSSEndHandler  (RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("li"));

/* links to other feed generators... */
		AddRSSStartHandler(RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("feedflare"));
		AddRSSEndHandler  (RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("feedflare"));
		AddRSSStartHandler(RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("browserfriendly"));
		AddRSSEndHandler  (RSSATOM_item_ignore,      RSS_RSS|RSS_ATOM, HKEY("browserfriendly"));

		KnownNameSpaces = NewHash(1, NULL);
		Put(KnownNameSpaces, HKEY("http://a9.com/-/spec/opensearch/1.1/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://a9.com/-/spec/opensearchrss/1.0/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://backend.userland.com/creativeCommonsRssModule"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/atom/ns#"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/dc/elements/1.1/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/rss/1.0/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/rss/1.0/modules/content/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/rss/1.0/modules/slash/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/rss/1.0/modules/syndication/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/rss/1.0/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://purl.org/syndication/thread/1.0"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://rssnamespace.org/feedburner/ext/1.0"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://schemas.google.com/g/2005"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://webns.net/mvcb/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://web.resource.org/cc/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://wellformedweb.org/CommentAPI/"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.georss.org/georss"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.w3.org/1999/xhtml"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.w3.org/1999/02/22-rdf-syntax-ns#"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.w3.org/1999/02/22-rdf-syntax-ns#"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.w3.org/2003/01/geo/wgs84_pos#"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("http://www.w3.org/2005/Atom"), NULL, reference_free_handler);
		Put(KnownNameSpaces, HKEY("urn:flickr:"), NULL, reference_free_handler);
#if 0
		/* we don't like these namespaces because of they shadow our usefull parameters. */
		Put(KnownNameSpaces, HKEY("http://search.yahoo.com/mrss/"), NULL, reference_free_handler);
#endif
                CtdlRegisterCleanupHook(rss_parser_cleanup);
	}
	return "rssparser";
}
