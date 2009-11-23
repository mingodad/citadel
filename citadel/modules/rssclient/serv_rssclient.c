/*
 * $Id$
 *
 * Bring external RSS feeds into rooms.
 *
 * Copyright (c) 2007-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#define RSS_UNSET 0
#define RSS_RSS 1
#define RSS_ATOM 2

typedef struct _rss_item {
	char *chardata;
	int chardata_len;
	char *roomlist;
	int done_parsing;
	char *guid;
	char *title;
	char *link;
	char *description;
	time_t pubdate;
	char channel_title[256];
	int item_tag_nesting;
	char *author_or_creator;
	char *author_url;
}rss_item;


typedef struct _rsscollection {
	rss_item *Item;
	rssnetcfg *Cfg;
	

} rsscollection;

struct rssnetcfg *rnclist = NULL;


#if 0
#ifdef HAVE_ICONV
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

#endif
#endif

/*
 * Commit a fetched and parsed RSS item to disk
 */
void rss_save_item(rsscollection *rssc) {

	struct MD5Context md5context;
	u_char rawdigest[MD5_DIGEST_LEN];
	int i;
	char utmsgid[SIZ];
	struct cdbdata *cdbut;
	struct UseTable ut;
	struct CtdlMessage *msg;
	struct recptypes *recp = NULL;
	int msglen = 0;
	rss_item *ri = rssc->Item;

	CtdlLogPrintf(0, "RSS: saving item...\n");
	recp = (struct recptypes *) malloc(sizeof(struct recptypes));
	if (recp == NULL) return;
	memset(recp, 0, sizeof(struct recptypes));
	recp->recp_room = strdup(ri->roomlist);
	recp->num_room = num_tokens(ri->roomlist, '|');
	recp->recptypes_magic = RECPTYPES_MAGIC;
   
	/* Construct a GUID to use in the S_USETABLE table.
	 * If one is not present in the item itself, make one up.
	 */
	if (ri->guid != NULL) {
		snprintf(utmsgid, sizeof utmsgid, "rss/%s", ri->guid);
	}
	else {
		MD5Init(&md5context);
		if (ri->title != NULL) {
			MD5Update(&md5context, (unsigned char*)ri->title, strlen(ri->title));
		}
		if (ri->link != NULL) {
			MD5Update(&md5context, (unsigned char*)ri->link, strlen(ri->link));
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
	if (cdbut != NULL) {
		/* Item has already been seen */
		CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", utmsgid);
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		strcpy(ut.ut_msgid, utmsgid);
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
	}
	else {
		/* Item has not been seen, so save it. */

		if (ri->description == NULL) ri->description = strdup("");
		for (i=strlen(ri->description); i>=0; --i) {
			if (isspace(ri->description[i])) {
				ri->description[i] = ' ';
			}
		}

		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;

		if (ri->author_or_creator != NULL) {
			msg->cm_fields['A'] = html_to_ascii(ri->author_or_creator,
				strlen(ri->author_or_creator), 512, 0);
			striplt(msg->cm_fields['A']);
		}
		else {
			msg->cm_fields['A'] = strdup("rss");
		}

		msg->cm_fields['N'] = strdup(NODENAME);
		if (ri->title != NULL) {
			msg->cm_fields['U'] = html_to_ascii(ri->title, strlen(ri->title), 512, 0);
			striplt(msg->cm_fields['U']);
		}
		msg->cm_fields['T'] = malloc(64);
		snprintf(msg->cm_fields['T'], 64, "%ld", ri->pubdate);
		if (ri->channel_title != NULL) {
			if (!IsEmptyStr(ri->channel_title)) {
				msg->cm_fields['O'] = strdup(ri->channel_title);
			}
		}
		if (ri->link == NULL) 
			ri->link = strdup("");
		msglen += 1024 + strlen(ri->link) + strlen(ri->description) ;
		msg->cm_fields['M'] = malloc(msglen);
		snprintf(msg->cm_fields['M'], msglen,
			"Content-type: text/html; charset=\"UTF-8\"\r\n\r\n"
			"<html><body>\n"
			"%s<br><br>\n"
			"<a href=\"%s\">%s</a>\n"
			"</body></html>\n"
			,
			ri->description,
			ri->link, ri->link
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
time_t rdf_parsedate(char *p)
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

#define RSS_UNSET 0
#define RSS_RSS 1
#define RSS_ATOM 2

void flush_rss_ite(rss_item *ri)
{
	/* Initialize the feed item data structure */
	if (ri->guid != NULL) free(ri->guid);
	ri->guid = NULL;
	if (ri->title != NULL) free(ri->title);
	ri->title = NULL;
	if (ri->link != NULL) free(ri->link);
	ri->link = NULL;
	if (ri->author_or_creator != NULL) free(ri->author_or_creator);
	ri->author_or_creator = NULL;
	if (ri->description != NULL) free(ri->description);
	ri->description = NULL;
	/* Throw away any existing character data */
	if (ri->chardata_len > 0) {
		free(ri->chardata);
		ri->chardata = 0;
		ri->chardata_len = 0;
	}
}

void rss_xml_start(void *data, const char *supplied_el, const char **attr) {
	rsscollection *rssc = (rsscollection*) data;
	rss_item *ri = rssc->Item;
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
///	CtdlLogPrintf(0, "RSS: supplied el %d: %s...\n", rssc->Cfg->ItemType, supplied_el);
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}

	if ((rssc->Cfg->ItemType == RSS_UNSET) &&
	    !strcasecmp(el, "rss")) 
	{
		rssc->Cfg->ItemType = RSS_RSS;
	}
	else if ((rssc->Cfg->ItemType == RSS_UNSET) &&
	    !strcasecmp(el, "feed")) 
	{
		rssc->Cfg->ItemType = RSS_ATOM;
	}
	else if ((rssc->Cfg->ItemType == RSS_RSS) &&
	    !strcasecmp(el, "item")) 
	{
		ri->item_tag_nesting ++ ;
		flush_rss_ite(ri);
	}
	else if ( (rssc->Cfg->ItemType == RSS_ATOM) &&
		 !strcasecmp(el, "entry")) 
	{ /* Atom feed... */
		CtdlLogPrintf(0, "RSS: found atom...\n");
		++ri->item_tag_nesting;
		flush_rss_ite(ri);
	}
	else if ((rssc->Cfg->ItemType == RSS_ATOM) &&
		 !strcasecmp(el, "link"))
	{
		int found ;
		int i;

		for (found = 0, i = 0;!found && attr[i] != NULL; i+=2)
		{
			if (!strcmp(attr[i], "href"))
			{
				found = 1;
				if (ri->link != NULL)
					free(ri->link);
				ri->link = strdup(attr[i+1]);
				striplt(ri->link);
			}
		}

	}

}

void rss_xml_end(void *data, const char *supplied_el) {
	rsscollection *rssc = (rsscollection*) data;
	rss_item *ri = rssc->Item;
	char el[256];
	char *sep = NULL;

	/* Axe the namespace, we don't care about it */
	safestrncpy(el, supplied_el, sizeof el);
	while (sep = strchr(el, ':'), sep) {
		strcpy(el, ++sep);
	}
//	CtdlLogPrintf(0, "RSS: END %s...\n", el);

	if ( (!strcasecmp(el, "title")) && (ri->item_tag_nesting == 0) && (ri->chardata != NULL) ) {
		safestrncpy(ri->channel_title, ri->chardata, sizeof ri->channel_title);
		striplt(ri->channel_title);
	}

	if ( (rssc->Cfg->ItemType == RSS_RSS) && 
	     (!strcasecmp(el, "guid")) && (ri->chardata != NULL) ) {
		if (ri->guid != NULL) free(ri->guid);
		striplt(ri->chardata);
		ri->guid = strdup(ri->chardata);
	}
	else if ( (rssc->Cfg->ItemType == RSS_ATOM) && 
		  (!strcasecmp(el, "id")) && (ri->chardata != NULL) ) {
		if (ri->guid != NULL) free(ri->guid);
		striplt(ri->chardata);
		ri->guid = strdup(ri->chardata);
	}

	else if ( (rssc->Cfg->ItemType == RSS_RSS) && (!strcasecmp(el, "link")) && (ri->chardata != NULL) ) {
		if (ri->link != NULL) free(ri->link);
		striplt(ri->chardata);
		ri->link = strdup(ri->chardata);
	}

	else if ( (!strcasecmp(el, "title")) && (ri->chardata != NULL) ) {
		if (ri->title != NULL) free(ri->title);
		striplt(ri->chardata);
		ri->title = strdup(ri->chardata);
	}

	else if ((rssc->Cfg->ItemType == RSS_ATOM) && 
		 (!strcasecmp(el, "content")) && 
		 (ri->chardata != NULL) ) {
		if (ri->description != NULL) free(ri->description);
		ri->description = strdup(ri->chardata);
	}
	else if ( (rssc->Cfg->ItemType == RSS_RSS) && 
		  (!strcasecmp(el, "description")) &&
		  (ri->chardata != NULL) ) {
		if (ri->description != NULL) free(ri->description);
		ri->description = strdup(ri->chardata);
	}
		  
	else if ((rssc->Cfg->ItemType == RSS_ATOM) && 
		 ((!strcasecmp(el, "published")) ||
		  (!strcasecmp(el, "updated"))) &&
		 (ri->chardata != NULL) ) {
		striplt(ri->chardata);
		ri->pubdate = rdf_parsedate(ri->chardata);
	}


	else if ((rssc->Cfg->ItemType == RSS_RSS) && 
		 ((!strcasecmp(el, "pubdate")) || 
		  (!strcasecmp(el, "date"))) && 
		 (ri->chardata != NULL) ) {
		striplt(ri->chardata);
		ri->pubdate = rdf_parsedate(ri->chardata);
	}

	else if ((rssc->Cfg->ItemType == RSS_RSS) && 
		 ((!strcasecmp(el, "author")) || 
		  (!strcasecmp(el, "creator"))) && 
		 (ri->chardata != NULL) ) {
		if (ri->author_or_creator != NULL) free(ri->author_or_creator);
		striplt(ri->chardata);
		ri->author_or_creator = strdup(ri->chardata);
	}

	else if ((rssc->Cfg->ItemType == RSS_ATOM) && 
		 (!strcasecmp(el, "name")) && 
		 (ri->chardata != NULL) ) {
		if (ri->author_or_creator != NULL) free(ri->author_or_creator);
		striplt(ri->chardata);
		ri->author_or_creator = strdup(ri->chardata);
	}
	else if ((rssc->Cfg->ItemType == RSS_ATOM) && 
		 (!strcasecmp(el, "uri")) && 
		 (ri->chardata != NULL) ) {
		if (ri->author_url != NULL) free(ri->author_url);
		striplt(ri->chardata);
		ri->author_url = strdup(ri->chardata);
	}

	else if ((rssc->Cfg->ItemType == RSS_RSS) && 
		 !strcasecmp(el, "item")) {
		--ri->item_tag_nesting;
		rss_save_item(rssc);
	}
	else if ((rssc->Cfg->ItemType == RSS_ATOM) && 
		 !strcasecmp(el, "entry")) {
		--ri->item_tag_nesting;
		rss_save_item(rssc);
	}

	else if ( (!strcasecmp(el, "rss")) || 
		  (!strcasecmp(el, "rdf")) ) {
//		CtdlLogPrintf(CTDL_DEBUG, "End of feed detected.  Closing parser.\n");
		ri->done_parsing = 1;
	}

	if (ri->chardata_len > 0) {
		free(ri->chardata);
		ri->chardata = 0;
		ri->chardata_len = 0;
	}

}


/*
 * This callback stores up the data which appears in between tags.
 */
void rss_xml_chardata(void *data, const XML_Char *s, int len) {
	rsscollection *rssc = (rsscollection*) data;
	rss_item *ri = rssc->Item;
	int old_len;
	int new_len;
	char *new_buffer;

	old_len = ri->chardata_len;
	new_len = old_len + len;
	new_buffer = realloc(ri->chardata, new_len + 1);
	if (new_buffer != NULL) {
		memcpy(&new_buffer[old_len], s, len);
		new_buffer[new_len] = 0;
		ri->chardata = new_buffer;
		ri->chardata_len = new_len;
	}
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

	CURL *curl;
	CURLcode res;
	char errmsg[1024] = "";

	rssc.Item = &ri;
	rssc.Cfg = Cfg;

	CtdlLogPrintf(CTDL_DEBUG, "Fetching RSS feed <%s>\n", Cfg->url);

	curl = curl_easy_init();
	if (!curl) {
		CtdlLogPrintf(CTDL_ALERT, "Unable to initialize libcurl.\n");
		return;
	}

	xp = XML_ParserCreateNS("UTF-8", ':');
	if (!xp) {
		CtdlLogPrintf(CTDL_ALERT, "Cannot create XML parser!\n");
		curl_easy_cleanup(curl);
		return;
	}

	curl_easy_setopt(curl, CURLOPT_URL, Cfg->url);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, xp);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rss_libcurl_callback);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
#endif
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */
	if (!IsEmptyStr(config.c_ip_addr)) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	memset(&ri, 0, sizeof(rss_item));
	ri.roomlist = Cfg->rooms;
#ifdef HAVE_ICONV
#if 0
	XML_SetUnknownEncodingHandler(xp,
				      handle_unknown_xml_encoding,
				      NULL);
#endif
#endif
	XML_SetElementHandler(xp, rss_xml_start, rss_xml_end);
	XML_SetCharacterDataHandler(xp, rss_xml_chardata);
	XML_SetUserData(xp, &rssc);
	if (CtdlThreadCheckStop())
	{
		XML_ParserFree(xp);
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

	if (ri.done_parsing == 0)
		XML_Parse(xp, "", 0, 1);


	CtdlLogPrintf(CTDL_ALERT, "RSS: XML Status [%s] \n", 
		      XML_ErrorString(
			      XML_GetErrorCode(xp)));

shutdown:
	curl_easy_cleanup(curl);
	XML_ParserFree(xp);

	flush_rss_ite(&ri);
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

	CtdlThreadAllocTSD();

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
	return NULL;
}


CTDL_MODULE_INIT(rssclient)
{
	if (threading)
	{
		CtdlLogPrintf(CTDL_INFO, "%s\n", curl_version());
		CtdlThreadSchedule ("RSS Client", CTDLTHREAD_BIGSTACK, rssclient_scan, NULL, 0);
	}
	/* return our Subversion id for the Log */
        return "$Id$";
}
