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

typedef struct rssnetcfg rssnetcfg;
struct rssnetcfg {
	rssnetcfg *next;
	char url[256];
	char *rooms;
	time_t last_error_when;
	int ItemType;
};

#define RSS_UNSET       (1<<0)
#define RSS_RSS         (1<<1)
#define RSS_ATOM        (1<<2)
#define RSS_REQUIRE_BUF (1<<3)

typedef struct _rss_item {
	char *roomlist;
	int done_parsing;
	StrBuf *guid;
	StrBuf *title;
	StrBuf *link;
	StrBuf *linkTitle;
	StrBuf *reLink;
	StrBuf *reLinkTitle;
	StrBuf *description;
	time_t pubdate;
	StrBuf *channel_title;
	int item_tag_nesting;
	StrBuf *author_or_creator;
	StrBuf *author_url;
}rss_item;


typedef void (*rss_handler_func)(StrBuf *CData, 
				 rss_item *ri, 
				 rssnetcfg *Cfg, 
				 const char** Attr);

typedef struct __rss_xml_handler {
	int Flags;
	rss_handler_func Handler;
}rss_xml_handler;


typedef struct _rsscollection {
	StrBuf *CData;
	StrBuf *Key;

	rss_item *Item;
	rssnetcfg *Cfg;
	
	rss_xml_handler *Current;
} rsscollection;

struct rssnetcfg *rnclist = NULL;
HashList *StartHandlers;
HashList *EndHandlers;
HashList *KnownNameSpaces;
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

///#if 0
//#ifdef HAVE_ICONV
#include <iconv.h>


/* 
 * dug this out of the trashcan of the midgard project, lets see if it works for us.
 * original code by Alexander Bokovoy <bokovoy@avilink.ne> distributed under GPL V2 or later
 */

/* Returns: 
 >= 0 - successfull, 0 means conversion doesn't use multibyte sequences 
   -1 - error during iconv_open call 
   -2 - error during iconv_close call 
   ---------------------------------- 
   This function expects that multibyte encoding in 'charset' wouldn't have 
   characters with more than 3 bytes. It is not intended to convert UTF-8 because 
   we'll never receive UTF-8 in our handler (it is handled by Exat itself). 
*/ 
static int 
fill_encoding_info (const char *charset, XML_Encoding * info) 
{ 
  iconv_t cd = (iconv_t)(-1); 
  int flag; 
	CtdlLogPrintf(0, "RSS: fill encoding info ...\n");
 
#if G_BYTE_ORDER == G_LITTLE_ENDIAN 
  cd = iconv_open ("UCS-2LE", charset); 
#else 
  cd = iconv_open ("UCS-2BE", charset); 
#endif 
 
  if (cd == (iconv_t) (-1)) 
    { 
      return -1; 
    } 
 
  { 
    unsigned short out; 
    unsigned char buf[4]; 
    unsigned int i0, i1, i2; 
    int result; 
    flag = 0; 
    for (i0 = 0; i0 < 0x100; i0++) 
      { 
        buf[0] = i0; 
        info->map[i0] = 0; 
        //result = try (cd, buf, 1, &out); 
        if (result < 0) 
          { 
          } 
        else if (result > 0) 
          { 
            info->map[i0] = out; 
          } 
        else 
          { 
            for (i1 = 0; i1 < 0x100; i1++) 
              { 
                buf[1] = i1; 
                ///result = try (cd, buf, 2, &out); 
                if (result < 0) 
                  { 
                  } 
                else if (result > 0) 
                  { 
                    flag++; 
                    info->map[i0] = -2; 
                  } 
                else 
                  { 
                    for (i2 = 0; i2 < 0x100; i2++) 
                      { 
                        buf[2] = i2; 
                        ////result = try (cd, buf, 3, &out); 
                        if (result < 0) 
                          { 
                          } 
                        else if (result > 0) 
                          { 
                            flag++; 
                            info->map[i0] = -3; 
                          } 
                      } 
                  } 
              } 
          } 
      } 
  } 
 
  if (iconv_close (cd) < 0) 
    { 
      return -2; 
    } 
  return flag; 
} 

static int 
iconv_convertor (void *data, const char *s) 
{ 
  XML_Encoding *info = data; 
  int res; 
	CtdlLogPrintf(0, "RSS: Converting ...\n");

  if (s == NULL) 
    return -1; 
/*
  GByteArray *result; 
  result = g_byte_array_new (); 
  if (process_block (info->data, (char *) s, strlen (s), result) == 0) 
    { 
      res = *(result->data); 
      g_byte_array_free (result, TRUE); 
      return res; 
    } 
  g_byte_array_free (result, TRUE); 
*/
  return -1; 
} 

static void 
my_release (void *data) 
{ 
  iconv_t cd = (iconv_t) data; 
  if (iconv_close (cd) != 0) 
    { 
/// TODO: uh no.      exit (1); 
    } 
} 
int 
handle_unknown_xml_encoding (void *encodingHandleData, 
			     const XML_Char * name, 
			     XML_Encoding * info) 
{ 
  int result; 
  CtdlLogPrintf(0, "RSS: unknown encoding ...\n");
  result = fill_encoding_info (name, info); 
  if (result >= 0) 
    { 
      /*  
        Special case: client asked for reverse conversion, we'll provide him with 
        iconv descriptor which handles it. Client should release it by himself. 
      */ 
      if(encodingHandleData != NULL) 
            *((iconv_t *)encodingHandleData) = iconv_open(name, "UTF-8"); 
      /*  
         Optimization: we do not need conversion function if encoding is one-to-one,  
         info->map table will be enough  
       */ 
      if (result == 0) 
        { 
          info->data = NULL; 
          info->convert = NULL; 
          info->release = NULL; 
          return 1; 
        } 
      /*  
         We do need conversion function because this encoding uses multibyte sequences 
       */ 
      info->data = (void *) iconv_open ("UTF-8", name); 
      if ((int)info->data == -1) 
        return -1; 
      info->convert = iconv_convertor; 
      info->release = my_release; 
      return 1; 
    } 
  if(encodingHandleData != NULL)  
    *(iconv_t *)encodingHandleData = NULL; 
  return 0; 
} 

///#endif
//#endif

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
			StrBuf *Encoded, *QPEncoded;
			StrBuf *UserName;
			StrBuf *EmailAddress;
			StrBuf *EncBuf;
			
			UserName = NewStrBuf();
			EmailAddress = NewStrBuf();
			EncBuf = NewStrBuf();
////TODO!
			From = html_to_ascii(ChrPtr(ri->author_or_creator),
					     StrLength(ri->author_or_creator), 
					     512, 0);

			Encoded = NewStrBufPlain(From, -1);
			free(From);
			StrBufTrim(Encoded);
			QPEncoded = StrBufSanitizeEmailRecipientVector(Encoded, UserName, EmailAddress, EncBuf);
			msg->cm_fields['A'] = SmashStrBuf(&QPEncoded);

			FreeStrBuf(&Encoded);
			FreeStrBuf(&UserName);
			FreeStrBuf(&EmailAddress);
			FreeStrBuf(&EncBuf);

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
			Encoded = NewStrBufPlain(Sbj, -1);
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
		msglen += 1024 + StrLength(ri->link) + StrLength(ri->description) ;
		msg->cm_fields['M'] = malloc(msglen);
		snprintf(msg->cm_fields['M'], msglen,
			 "Content-type: text/html; charset=\"UTF-8\"\r\n\r\n"
			 "<html><body>\n"
			 "%s<br><br>\n"
			 "<a href=\"%s\">%s</a><br>\n"
			 "<a href=\"%s\">%s</a>\n"
			 "</body></html>\n"
			 ,
			 ChrPtr(ri->description),
			 ChrPtr(ri->link),
			 (StrLength(ri->linkTitle)>0)?ChrPtr(ri->linkTitle):ChrPtr(ri->link),
			 ChrPtr(ri->reLink),
			 (StrLength(ri->reLinkTitle)>0)?ChrPtr(ri->reLinkTitle):"Reply to this"
		);

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
	FreeStrBuf(&ri->author_url);
	FreeStrBuf(&ri->description);
}

void rss_xml_start(void *data, const char *supplied_el, const char **attr)
{
	rss_xml_handler *h;
	rsscollection   *rssc = (rsscollection*) data;
	rssnetcfg       *Cfg = rssc->Cfg;
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
		    (Cfg->ItemType == RSS_UNSET))
		{
			h->Handler(rssc->CData, ri, Cfg, attr);
		}
		else if (((h->Flags & RSS_RSS) != 0) &&
		    (Cfg->ItemType == RSS_RSS))
		{
			h->Handler(rssc->CData, ri, Cfg, attr);
		}
		else if (((h->Flags & RSS_ATOM) != 0) &&
			 (Cfg->ItemType == RSS_ATOM))
		{
			h->Handler(rssc->CData, ri, Cfg, attr);			
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
	rsscollection   *rssc = (rsscollection*) data;
	rssnetcfg       *Cfg = rssc->Cfg;
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
		    (Cfg->ItemType == RSS_UNSET))
		{
			h->Handler(rssc->CData, ri, Cfg, NULL);
		}
		else if (((h->Flags & RSS_RSS) != 0) &&
		    (Cfg->ItemType == RSS_RSS))
		{
			h->Handler(rssc->CData, ri, Cfg, NULL);
		}
		else if (((h->Flags & RSS_ATOM) != 0) &&
			 (Cfg->ItemType == RSS_ATOM))
		{
			h->Handler(rssc->CData, ri, Cfg, NULL);
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





void RSS_item_rss_start (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	CtdlLogPrintf(CTDL_DEBUG, "RSS: This is an RSS feed.\n");
	Cfg->ItemType = RSS_RSS;
}

void RSS_item_rdf_start(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	CtdlLogPrintf(CTDL_DEBUG, "RSS: This is an RDF feed.\n");
	Cfg->ItemType = RSS_RSS;
}

void ATOM_item_feed_start(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	CtdlLogPrintf(CTDL_DEBUG, "RSS: This is an ATOM feed.\n");
	Cfg->ItemType = RSS_ATOM;
}


void RSS_item_item_start(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	ri->item_tag_nesting ++;
	flush_rss_item(ri);
}

void ATOM_item_entry_start(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
/* Atom feed... */
	ri->item_tag_nesting ++;
	flush_rss_item(ri);
}

void ATOM_item_link_start (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
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




void ATOMRSS_item_title_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if ((ri->item_tag_nesting == 0) && (StrLength(CData) > 0)) {
		NewStrBufDupAppendFlush(&ri->channel_title, CData, NULL, 0);
		StrBufTrim(ri->channel_title);
	}
}

void RSS_item_guid_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->guid, CData, NULL, 0);
	}
}

void ATOM_item_id_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->guid, CData, NULL, 0);
	}
}


void RSS_item_link_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->link, CData, NULL, 0);
		StrBufTrim(ri->link);
	}
}

void RSSATOM_item_title_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->title, CData, NULL, 0);
		StrBufTrim(ri->title);
	}
}

void ATOM_item_content_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
		StrBufTrim(ri->description);
	}
}
void ATOM_item_summary_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	/* this can contain an abstract of the article. but we don't want to verwrite a full document if we already have it. */
	if ((StrLength(CData) > 0) && (StrLength(ri->description) == 0))
	{
		NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
		StrBufTrim(ri->description);
	}
}

void RSS_item_description_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->description, CData, NULL, 0);
	}
}

void ATOM_item_published_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{		  
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}

void ATOM_item_updated_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}

void RSS_item_pubdate_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}


void RSS_item_date_end (StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		StrBufTrim(CData);
		ri->pubdate = rdf_parsedate(ChrPtr(CData));
	}
}



void RSS_item_author_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}


void ATOM_item_name_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}

void RSS_item_creator_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if ((StrLength(CData) > 0) && 
	    (StrLength(ri->author_or_creator) == 0))
	{
		NewStrBufDupAppendFlush(&ri->author_or_creator, CData, NULL, 0);
		StrBufTrim(ri->author_or_creator);
	}
}


void ATOM_item_uri_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	if (StrLength(CData) > 0) {
		NewStrBufDupAppendFlush(&ri->author_url, CData, NULL, 0);
		StrBufTrim(ri->author_url);
	}
}

void RSS_item_item_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	--ri->item_tag_nesting;
	rss_save_item(ri);
}


void ATOM_item_entry_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
	--ri->item_tag_nesting;
	rss_save_item(ri);
}

void RSS_item_rss_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
//		CtdlLogPrintf(CTDL_DEBUG, "End of feed detected.  Closing parser.\n");
	ri->done_parsing = 1;
	
}
void RSS_item_rdf_end(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
//		CtdlLogPrintf(CTDL_DEBUG, "End of feed detected.  Closing parser.\n");
	ri->done_parsing = 1;
}


void RSSATOM_item_ignore(StrBuf *CData, rss_item *ri, rssnetcfg *Cfg, const char** Attr)
{
}



/*
 * This callback stores up the data which appears in between tags.
 */
void rss_xml_cdata_start(void *data) 
{
	rsscollection *rssc = (rsscollection*) data;

	FlushStrBuf(rssc->CData);
}

void rss_xml_cdata_end(void *data) 
{
}
void rss_xml_chardata(void *data, const XML_Char *s, int len) 
{
	rsscollection *rssc = (rsscollection*) data;

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



/*
 * Begin a feed parse
 */
void rss_do_fetching(rssnetcfg *Cfg) {
	rsscollection rssc;
	rss_item ri;
	XML_Parser xp;
	StrBuf *Answer;

	CURL *curl;
	CURLcode res;
	char errmsg[1024] = "";
	char *ptr;
	const char *at;
	long len;

	memset(&ri, 0, sizeof(rss_item));
	rssc.Item = &ri;
	rssc.Cfg = Cfg;

	CtdlLogPrintf(CTDL_DEBUG, "Fetching RSS feed <%s>\n", Cfg->url);

	curl = curl_easy_init();
	if (!curl) {
		CtdlLogPrintf(CTDL_ALERT, "Unable to initialize libcurl.\n");
		return;
	}
	Answer = NewStrBufPlain(NULL, SIZ);

	curl_easy_setopt(curl, CURLOPT_URL, Cfg->url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, Answer);
//	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rss_libcurl_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
#endif
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */
	if (
		(!IsEmptyStr(config.c_ip_addr))
		&& (strcmp(config.c_ip_addr, "*"))
		&& (strcmp(config.c_ip_addr, "::"))
		&& (strcmp(config.c_ip_addr, "0.0.0.0"))
	) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	if (CtdlThreadCheckStop())
	{
		curl_easy_cleanup(curl);
		return;
	}
	
	if (CtdlThreadCheckStop())
		goto shutdown ;

	res = curl_easy_perform(curl);
	if (res) {
		CtdlLogPrintf(CTDL_ALERT, "libcurl error %d: %s\n", res, errmsg);
	}

	if (CtdlThreadCheckStop())
		goto shutdown ;




	memset(&ri, 0, sizeof(rss_item));
	ri.roomlist = Cfg->rooms;
	rssc.CData = NewStrBufPlain(NULL, SIZ);
	rssc.Key = NewStrBuf();
	at = NULL;
	StrBufSipLine(rssc.Key, Answer, &at);
	ptr = NULL;

#define encoding "encoding=\""
	ptr = strstr(ChrPtr(rssc.Key), encoding);
	if (ptr != NULL)
	{
		char *pche;

		ptr += sizeof (encoding) - 1;
		pche = strchr(ptr, '"');
		if (pche != NULL)
			StrBufCutAt(rssc.Key, -1, pche);
		else 
			ptr = "UTF-8";
	}
	else
		ptr = "UTF-8";


	xp = XML_ParserCreateNS(ptr, ':');
	if (!xp) {
		CtdlLogPrintf(CTDL_ALERT, "Cannot create XML parser!\n");
		goto shutdown;
	}
	FlushStrBuf(rssc.Key);
//#ifdef HAVE_ICONV
#if 0
	XML_SetUnknownEncodingHandler(xp,
				      handle_unknown_xml_encoding,
				      &rssc);
#endif
//#endif
	XML_SetElementHandler(xp, rss_xml_start, rss_xml_end);
	XML_SetCharacterDataHandler(xp, rss_xml_chardata);
	XML_SetUserData(xp, &rssc);
	XML_SetCdataSectionHandler(xp,
				   rss_xml_cdata_start,
				   rss_xml_cdata_end);


	len = StrLength(Answer);
	ptr = SmashStrBuf(&Answer);
	XML_Parse(xp, ptr, len, 0);
	free (ptr);
	if (ri.done_parsing == 0)
		XML_Parse(xp, "", 0, 1);


	CtdlLogPrintf(CTDL_ALERT, "RSS: XML Status [%s] \n", 
		      XML_ErrorString(
			      XML_GetErrorCode(xp)));

shutdown:
	curl_easy_cleanup(curl);
	XML_ParserFree(xp);

	flush_rss_item(&ri);
	FreeStrBuf(&rssc.CData);
	FreeStrBuf(&rssc.Key);
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
				if (!strcmp(rncptr->url, feedurl)) {
					use_this_rncptr = rncptr;
				}
			}

			/* Otherwise create a new client request */
			if (use_this_rncptr == NULL) {
				rncptr = (rssnetcfg *) malloc(sizeof(rssnetcfg));
				rncptr->ItemType = RSS_UNSET;
				if (rncptr != NULL) {
					rncptr->next = rnclist;
					safestrncpy(rncptr->url, feedurl, sizeof rncptr->url);
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
void *rssclient_scan(void *args) {
	static time_t last_run = 0L;
	static int doing_rssclient = 0;
	rssnetcfg *rptr = NULL;
	CitContext rssclientCC;

	/* Give this thread its own private CitContext */
	CtdlFillSystemContext(&rssclientCC, "rssclient");
	citthread_setspecific(MyConKey, (void *)&rssclientCC );

	/*
	 * This is a simple concurrency check to make sure only one rssclient run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_rssclient) return NULL;
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
	if (!CtdlThreadCheckStop())
		CtdlThreadSchedule ("RSS Client", CTDLTHREAD_BIGSTACK, rssclient_scan, NULL, last_run + config.c_net_freq);
	else
		CtdlLogPrintf(CTDL_DEBUG, "rssclient: Task STOPPED.\n");
	CtdlClearSystemContext();
	return NULL;
}


CTDL_MODULE_INIT(rssclient)
{
	if (threading)
	{
		CtdlLogPrintf(CTDL_INFO, "%s\n", curl_version());
		CtdlThreadSchedule ("RSS Client", CTDLTHREAD_BIGSTACK, rssclient_scan, NULL, 0);
	}

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
	AddRSSEndHandler(RSSATOM_item_title_end,   RSS_ATOM|RSS_RSS|RSS_REQUIRE_BUF, HKEY("title"));
	AddRSSEndHandler(ATOM_item_content_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("content"));
	AddRSSEndHandler(RSS_item_description_end, RSS_RSS|RSS_REQUIRE_BUF, HKEY("encoded"));
	AddRSSEndHandler(ATOM_item_summary_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("summary"));
	AddRSSEndHandler(RSS_item_description_end, RSS_RSS|RSS_REQUIRE_BUF, HKEY("description"));
	AddRSSEndHandler(ATOM_item_published_end,  RSS_ATOM|RSS_REQUIRE_BUF, HKEY("published"));
	AddRSSEndHandler(ATOM_item_updated_end,    RSS_ATOM|RSS_REQUIRE_BUF, HKEY("updated"));
	AddRSSEndHandler(RSS_item_pubdate_end,     RSS_RSS|RSS_REQUIRE_BUF, HKEY("pubdate"));
	AddRSSEndHandler(RSS_item_date_end,        RSS_RSS|RSS_REQUIRE_BUF, HKEY("date"));
	AddRSSEndHandler(RSS_item_author_end,      RSS_RSS|RSS_REQUIRE_BUF, HKEY("author"));
	AddRSSEndHandler(RSS_item_creator_end,     RSS_RSS|RSS_REQUIRE_BUF, HKEY("creator"));
	AddRSSEndHandler(ATOM_item_name_end,       RSS_ATOM|RSS_REQUIRE_BUF, HKEY("name"));
	AddRSSEndHandler(ATOM_item_uri_end,        RSS_ATOM|RSS_REQUIRE_BUF, HKEY("uri"));

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
	return "rssclient";
}
