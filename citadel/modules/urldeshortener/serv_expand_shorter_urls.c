
/*
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "ctdl_module.h"
#include "locate_host.h"
#include "citadel_dirs.h"

#include "event_client.h"

HashList *UrlShorteners = NULL;

size_t GetLocationString( void *ptr, size_t size, size_t nmemb, void *userdata)
{
#define LOCATION "location"
	if (strncasecmp((char*)ptr, LOCATION, sizeof(LOCATION) - 1) == 0)
	{
		StrBuf *pURL = (StrBuf*) userdata;
		char *pch = (char*) ptr;
		char *pche;
		
		pche = pch + (size * nmemb);
		pch += sizeof(LOCATION);
		
		while (isspace(*pch) || (*pch == ':'))
			pch ++;

		while (isspace(*pche) || (*pche == '\0'))
			pche--;
		
		FlushStrBuf(pURL);
		StrBufPlain(pURL, pch, pche - pch + 1);	
	}
	return size * nmemb;
}
eNextState ShutdownLookuUrl(AsyncIO *IO)
{
//TOOD
	return eAbort;
}
eNextState TerminateLookupUrl(AsyncIO *IO)
{
//TOOD
	return eAbort;
}
eNextState LookupUrlResult(AsyncIO *IO)
{
	return eTerminateConnection; /// /TODO
}

int LookupUrl(StrBuf *ShorterUrlStr)
{
	CURLcode sta;
	int rc = 0;
	CURL *chnd;
	AsyncIO *IO;


	IO = (AsyncIO*) malloc(sizeof(AsyncIO));
	memset(IO, 0, sizeof(AsyncIO));
	IO->CitContext = CloneContext(CC);

	ParseURL(&IO->ConnectMe, ShorterUrlStr, 80);
	CurlPrepareURL(IO->ConnectMe);
	if (! InitcURLIOStruct(IO, 
//			  Ctx, 
			  NULL,
			  "Citadel RSS ShorterURL Expander",
			  LookupUrlResult, 
			  TerminateLookupUrl, 
			  ShutdownLookuUrl))
	{
		syslog(LOG_ALERT, "Unable to initialize libcurl.\n");
		goto shutdown;
	}
	chnd = IO->HttpReq.chnd;

	OPT(SSL_VERIFYPEER, 0);
	OPT(SSL_VERIFYHOST, 0);
	OPT(FOLLOWLOCATION, 10);
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	OPT(HTTP_CONTENT_DECODING, 1);
	OPT(ENCODING, "");
#endif 
	OPT(HEADERFUNCTION , GetLocationString);
	OPT(WRITEHEADER, ShorterUrlStr);


	if (server_shutting_down)
		goto shutdown ;

	QueueCurlContext(IO);

shutdown:

       	return rc;

}



void CrawlMessageForShorterUrls(HashList *pUrls, StrBuf *Message)
{
	int nHits = 0;
	void *pv;
	int nShorter = 0;
	const char *pch;
	const char *pUrl;
	ConstStr *pCUrl;

	while (GetHash(UrlShorteners, IKEY(nShorter), &pv))
	{
		nShorter++;
		pch = ChrPtr(Message);
		pUrl = strstr(pch, ChrPtr((StrBuf*)pv));
		while ((pUrl != NULL) && (nHits < 99))
		{
			pCUrl = malloc(sizeof(ConstStr));

			pCUrl->Key = pUrl;
			pch = pUrl + StrLength((StrBuf*)pv);
			while (isalnum(*pch)||(*pch == '-')||(*pch == '/'))
				pch++;
			pCUrl->len = pch - pCUrl->Key;

			Put(pUrls, IKEY(nHits), pCUrl, NULL);
			nHits ++;
			pUrl = strstr(pch, ChrPtr((StrBuf*)pv));
		}
	}
}

int SortConstStrByPosition(const void *Item1, const void *Item2)
{
	const ConstStr *p1, *p2;
	p1 = (const ConstStr*) Item1;
	p2 = (const ConstStr*) Item2;
	if (p1->Key == p2->Key)
		return 0;
	if (p1->Key > p2->Key)
		return 1;
	return -1;
}

HashList *GetShorterUrls(StrBuf *Message)
{
	HashList *pUrls;
	/* we just suspect URL shorteners to be inside of feeds from twitter
	 * or other short content messages, so don't crawl through real blogs.
	 */
	if (StrLength(Message) > 500)
		return NULL;

	pUrls = NewHash(1, Flathash);
	CrawlMessageForShorterUrls(pUrls, Message);

	if (GetCount(pUrls) > 0)
		return pUrls;
	else 
		return NULL;

}

void ExpandShortUrls(StrBuf *Message, HashList *pUrls, int Callback)
{
	StrBuf *Shadow;
	ConstStr *pCUrl;
	const char *pch;
	const char *pche;

	StrBuf *ShorterUrlStr;
	HashPos *Pos;
	const char *Key;
	void *pv;
	long len;
	
	Shadow = NewStrBufPlain(NULL, StrLength(Message));
	SortByPayload (pUrls, SortConstStrByPosition);
		
	ShorterUrlStr = NewStrBufPlain(NULL, StrLength(Message));
		
	pch = ChrPtr(Message);
	pche = pch + StrLength(Message);
	Pos = GetNewHashPos(pUrls, 1);
	while (GetNextHashPos(pUrls, Pos, &len, &Key, &pv))
	{
		pCUrl = (ConstStr*) pv;

		if (pch != pCUrl->Key)
			StrBufAppendBufPlain(Shadow, pch, pCUrl->Key - pch, 0);
			
		StrBufPlain(ShorterUrlStr, CKEY(*pCUrl));
		if (LookupUrl(ShorterUrlStr))
		{
			StrBufAppendBufPlain(Shadow, HKEY("<a href=\""), 0);
			StrBufAppendBuf(Shadow, ShorterUrlStr, 0);
			StrBufAppendBufPlain(Shadow, HKEY("\">"), 0);
			StrBufAppendBuf(Shadow, ShorterUrlStr, 0);
			StrBufAppendBufPlain(Shadow, HKEY("["), 0);
			StrBufAppendBufPlain(Shadow, pCUrl->Key, pCUrl->len, 0);
			StrBufAppendBufPlain(Shadow, HKEY("]</a>"), 0);
		}
		else
		{
			StrBufAppendBufPlain(Shadow, HKEY("<a href=\""), 0);
			StrBufAppendBufPlain(Shadow, pCUrl->Key, pCUrl->len, 0);
			StrBufAppendBufPlain(Shadow, HKEY("\">"), 0);
			StrBufAppendBufPlain(Shadow, pCUrl->Key, pCUrl->len, 0);
			StrBufAppendBufPlain(Shadow, HKEY("</a>"), 0);
		}
		pch = pCUrl->Key + pCUrl->len + 1;

	}
	if (pch < pche)
		StrBufAppendBufPlain(Shadow, pch, pche - pch, 0);
	FlushStrBuf(Message);
	StrBufAppendBuf(Message, Shadow, 0);

	FreeStrBuf(&ShorterUrlStr);
	FreeStrBuf(&Shadow);
	DeleteHashPos(&Pos);
	

	DeleteHash(&pUrls);
}

void LoadUrlShorteners(void)
{
	int i = 0;
	int fd;
	const char *POS = NULL;
	const char *Err = NULL;
	StrBuf *Content, *Line;


	UrlShorteners = NewHash(0, Flathash);

	fd = open(file_citadel_urlshorteners, 0);

	if (fd != 0)
	{
		Content = NewStrBufPlain(NULL, SIZ);
		Line = NewStrBuf();
		while (POS != StrBufNOTNULL)
		{
			StrBufTCP_read_buffered_line_fast (Line, Content, &POS, &fd, 1, 1, &Err);
			StrBufTrim(Line);
			if ((*ChrPtr(Line) != '#') && (StrLength(Line) > 0))
			{
				Put(UrlShorteners, IKEY(i), Line, HFreeStrBuf);
				i++;
				Line = NewStrBuf();
			}
			else
				FlushStrBuf(Line);
			if (POS == NULL)
				POS = StrBufNOTNULL;
		}
		FreeStrBuf(&Line);
		FreeStrBuf(&Content);
	}
	close(fd);
}

void shorter_url_cleanup(void)
{
	DeleteHash(&UrlShorteners);
}


CTDL_MODULE_INIT(urldeshortener)
{
	if (threading)
	{
		syslog(LOG_INFO, "%s\n", curl_version());
	}
	else 
	{
		LoadUrlShorteners ();
                CtdlRegisterCleanupHook(shorter_url_cleanup);
	}
	return "UrlShortener";
}
