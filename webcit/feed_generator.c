/*
 * RSS feed generator (could be adapted in the future to feed both RSS and Atom)
 *
 * Copyright (c) 2010-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "webcit.h"
#include "webserver.h"


/*
 * RSS feed generator -- do one message
 */
void feed_rss_one_message(long msgnum) {
	int in_body = 0;
	int in_messagetext = 0;
	int found_title = 0;
	int found_guid = 0;
	char pubdate[128];
	StrBuf *messagetext = NULL;
	int is_top_level_post = 1;
	const char *BufPtr = NULL;
	StrBuf *Line = NewStrBufPlain(NULL, 1024);
	char buf[1024];
	int permalink_hash = 0;

	/* Phase 1: read the message into memory */
	serv_printf("MSG4 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;
	StrBuf *ServerResponse = NewStrBuf();
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		StrBufAppendPrintf(ServerResponse, "%s\n", buf);
	}

	/* Phase 2: help SkyNet become self-aware */
	BufPtr = NULL;
	while (StrBufSipLine(Line, ServerResponse, &BufPtr), ((BufPtr!=StrBufNOTNULL)&&(BufPtr!=NULL)) ) {
		if (in_body) {
			/* do nothing */
		}
		else if (StrLength(Line) == 0) {
			++in_body;
		}
		else if ((StrLength(Line) > 5) && (!strncasecmp(ChrPtr(Line), "wefw=", 5))) {
			is_top_level_post = 0;	/* presence of references means it's a reply/comment */
		}
		else if ((StrLength(Line) > 5) && (!strncasecmp(ChrPtr(Line), "msgn=", 5))) {
			StrBufCutLeft(Line, 5);
			permalink_hash = ThreadIdHash(Line);
		}
	}

	/*
	 * Phase 3: output the message in RSS <item> form
	 * (suppress replies [comments] if this is a blog room)
	 */
	if ( (WC->CurRoom.view != VIEW_BLOG) || (is_top_level_post == 1) ) {
		wc_printf("<item>");
		wc_printf("<link>%s/readfwd?go=", ChrPtr(site_prefix));
		urlescputs(ChrPtr(WC->CurRoom.name));
		if ((WC->CurRoom.view == VIEW_BLOG) && (permalink_hash != 0)) {
			wc_printf("?p=%d", permalink_hash);
		}
		else {
			wc_printf("?start_reading_at=%ld", msgnum);
		}
		wc_printf("</link>");
	
		BufPtr = NULL;
		in_body = 0;
		in_messagetext = 0;
		while (StrBufSipLine(Line, ServerResponse, &BufPtr), ((BufPtr!=StrBufNOTNULL)&&(BufPtr!=NULL)) ) {
			safestrncpy(buf, ChrPtr(Line), sizeof buf);
			if (in_body) {
				if (in_messagetext) {
					StrBufAppendBufPlain(messagetext, buf, -1, 0);
					StrBufAppendBufPlain(messagetext, HKEY("\r\n"), 0);
				}
				else if (IsEmptyStr(buf)) {
					in_messagetext = 1;
				}
			}
			else if (!strncasecmp(buf, "subj=", 5)) {
				wc_printf("<title>");
				escputs(&buf[5]);
				wc_printf("</title>");
				++found_title;
			}
			else if (!strncasecmp(buf, "exti=", 5)) {
				wc_printf("<guid isPermaLink=\"false\">");
				escputs(&buf[5]);
				wc_printf("</guid>");
				++found_guid;
			}
			else if (!strncasecmp(buf, "time=", 5)) {
				http_datestring(pubdate, sizeof pubdate, atol(&buf[5]));
				wc_printf("<pubDate>%s</pubDate>", pubdate);
			}
			else if (!strncasecmp(buf, "text", 4)) {
				if (!found_title) {
					wc_printf("<title>Message #%ld</title>", msgnum);
				}
				if (!found_guid) {
					wc_printf("<guid isPermaLink=\"false\">%ld@%s</guid>",
						msgnum,
						ChrPtr(WC->serv_info->serv_humannode)
					);
				}
				wc_printf("<description>");
				in_body = 1;
				messagetext = NewStrBuf();
			}
		}
	
		if (in_body) {
			cdataout((char*)ChrPtr(messagetext));
			FreeStrBuf(&messagetext);
			wc_printf("</description>");
		}

		wc_printf("</item>");
	}

	FreeStrBuf(&Line);
	FreeStrBuf(&ServerResponse);
	return;
}

/*
 * RSS feed generator -- go through the message list
 */
void feed_rss_do_messages(void) {
	wcsession *WCC = WC;
	int num_msgs = 0;
	int i;
	SharedMessageStatus Stat;
	message_summary *Msg = NULL;

	memset(&Stat, 0, sizeof Stat);
	Stat.maxload = INT_MAX;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	num_msgs = load_msg_ptrs("MSGS ALL", NULL, &Stat, NULL);
	if (num_msgs < 1) return;

	i = num_msgs;					/* convention is to feed newest-to-oldest */
	while (i > 0) {
		Msg = GetMessagePtrAt(i-1, WCC->summ);
		if (Msg != NULL) {
			feed_rss_one_message(Msg->msgnum);
		}
		--i;
	}
}


/*
 * Output the room info file of the current room as a <description> for the channel
 */
void feed_rss_do_room_info_as_description(void)
{
	wc_printf("<description>");
	escputs(ChrPtr(WC->CurRoom.name));	/* FIXME use the output of RINF instead */
	wc_printf("</description>\r\n");
}


/*
 * Entry point for RSS feed generator
 */
void feed_rss(void) {
	char buf[1024];

	output_headers(0, 0, 0, 0, 1, 0);
	hprintf("Content-type: text/xml; charset=utf-8\r\n");
	hprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n"
	,
		PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software)
	);
	begin_burst();

	wc_printf("<?xml version=\"1.0\"?>"
		"<rss version=\"2.0\">"
		"<channel>"
	);

	wc_printf("<title>");
	escputs(ChrPtr(WC->CurRoom.name));
	wc_printf("</title>");

	wc_printf("<link>");
	escputs(ChrPtr(site_prefix));
	wc_printf("/</link>");

	serv_puts("RINF");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		wc_printf("<description>\r\n");
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			escputs(buf);
			wc_printf("\r\n");
		}
		wc_printf("</description>");
	}

	wc_printf("<image><title>");
	escputs(ChrPtr(WC->CurRoom.name));
	wc_printf("</title><url>");
	escputs(ChrPtr(site_prefix));
	wc_printf("/image?name=_roompic_?go=");
	urlescputs(ChrPtr(WC->CurRoom.name));
	wc_printf("</url><link>");
	escputs(ChrPtr(site_prefix));
	wc_printf("/</link></image>\r\n");

	feed_rss_do_room_info_as_description();
	feed_rss_do_messages();

	wc_printf("</channel>"
		"</rss>"
		"\r\n\r\n"
	);

	wDumpContent(0);
}


/*
 * Offer the RSS feed meta tag for this room
 */
void tmplput_rssmeta(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	char feed_link[1024];

	strcpy(feed_link, "/feed_rss?go=");
	urlesc(&feed_link[20], sizeof(feed_link) - 20, (char *)ChrPtr(WCC->CurRoom.name) );
	StrBufAppendPrintf(Target,
		"<link rel=\"alternate\" title=\"RSS\" href=\"%s\" type=\"application/rss+xml\">",
		feed_link
	);
}


/*
 * Offer the RSS feed button for this room
 */
void tmplput_rssbutton(StrBuf *Target, WCTemplputParams *TP) 
{
	StrBuf *FeedLink = NULL;

	FeedLink = NewStrBufPlain(HKEY("/feed_rss?go="));
	StrBufUrlescAppend(FeedLink, WC->CurRoom.name, NULL);

	StrBufAppendPrintf(Target, "<a type=\"application/rss+xml\" href=\"");
	StrBufAppendBuf(Target, FeedLink, 0);
	StrBufAppendPrintf(Target, "\"><img src=\"static/webcit_icons/essen/16x16/rss.png\" alt=\"RSS\">");
	StrBufAppendPrintf(Target, "</a>");
	FreeStrBuf(&FeedLink);
}


void 
InitModule_RSS
(void)
{
	WebcitAddUrlHandler(HKEY("feed_rss"), "", 0, feed_rss, ANONYMOUS|COOKIEUNNEEDED);
	RegisterNamespace("THISROOM:FEED:RSS", 0, 0, tmplput_rssbutton, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:FEED:RSSMETA", 0, 0, tmplput_rssmeta, NULL, CTX_NONE);
}
