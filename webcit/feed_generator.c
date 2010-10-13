/*
 * RSS feed generator (could be adapted in the future to feed both RSS and Atom)
 *
 * Copyright (c) 2010 by the citadel.org team
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

#include "webcit.h"
#include "webserver.h"


/*
 * RSS feed generator -- do one message
 */
void feed_rss_one_message(long msgnum) {
	char buf[1024];
	int in_body = 0;
	int found_title = 0;
	char pubdate[128];

	/* FIXME if this is a blog room we only want to include top-level messages */

	serv_printf("MSG0 %ld", msgnum);		/* FIXME we want msg4 eventually */
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	wc_printf("<item>");
	wc_printf("<link>%s/readfwd?gotofirst=", ChrPtr(site_prefix));
	urlescputs(ChrPtr(WC->CurRoom.name));
	wc_printf("?start_reading_at=%ld</link>", msgnum);

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (in_body) {
			escputs(buf);
			wc_printf("\r\n");
		}
		else if (!strncasecmp(buf, "subj=", 5)) {
			wc_printf("<title>");
			escputs(&buf[5]);
			wc_printf("</title>");
			++found_title;
		}
		else if (!strncasecmp(buf, "exti=", 5)) {
			wc_printf("<guid>");
			escputs(&buf[5]);
			wc_printf("</guid>");
		}
		else if (!strncasecmp(buf, "time=", 5)) {
			http_datestring(pubdate, sizeof pubdate, atol(&buf[5]));
			wc_printf("<pubDate>%s</pubDate>", pubdate);
		}
		else if (!strncasecmp(buf, "text", 4)) {
			if (!found_title) {
				wc_printf("<title>Message #%ld</title>", msgnum);
			}
			wc_printf("<description>");
			in_body = 1;
		}
	}

	if (in_body) {
		wc_printf("</description>");
	}

	wc_printf("</item>");
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
	num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
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
 * Entry point for RSS feed generator
 */
void feed_rss(void) {
	char buf[1024];

	output_headers(0, 0, 0, 0, 1, 0);
	hprintf("Content-type: text/xml\r\n");
	hprintf(
		"Server: %s / %s\r\n"
		"Connection: close\r\n"
	,
		PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software)
	);
	begin_burst();

	wc_printf("<?xml version=\"1.0\"?>"
		"<rss version=\"2.0\" xmlns:atom=\"http://www.w3.org/2005/Atom\">"
		"<channel>"
	);

	wc_printf("<title>");
	escputs(ChrPtr(WC->CurRoom.name));
	wc_printf("</title>");

	wc_printf("<link>");
	urlescputs(ChrPtr(site_prefix));
	wc_printf("</link>");

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
	urlescputs(ChrPtr(site_prefix));
	wc_printf("/image?name=_roompic_?gotofirst=");
	urlescputs(ChrPtr(WC->CurRoom.name));
	wc_printf("</url><link>");
	urlescputs(ChrPtr(site_prefix));
	wc_printf("</link></image>\r\n");

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
	char encoded_link[1024];

	strcpy(feed_link, "/feed_rss?gotofirst=");
	urlesc(&feed_link[20], sizeof(feed_link) - 20, (char *)ChrPtr(WCC->CurRoom.name) );
	CtdlEncodeBase64(encoded_link, feed_link, strlen(feed_link), 0);

	StrBufAppendPrintf(Target,
		"<link rel=\"alternate\" title=\"RSS\" href=\"/B64%s\" type=\"application/rss+xml\">",
		encoded_link
	);
}


/*
 * Offer the RSS feed button for this room
 */
void tmplput_rssbutton(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	char feed_link[1024];
	char encoded_link[1024];

	strcpy(feed_link, "/feed_rss?gotofirst=");
	urlesc(&feed_link[20], sizeof(feed_link) - 20, (char *)ChrPtr(WCC->CurRoom.name) );
	CtdlEncodeBase64(encoded_link, feed_link, strlen(feed_link), 0);

	StrBufAppendPrintf(Target, "<a type-\"application/rss+xml\" href=\"/B64%s\">", encoded_link);
	StrBufAppendPrintf(Target, "<img border=\"0\" src=\"static/rss_16x.png\">");
	StrBufAppendPrintf(Target, "</a>");
}


void 
InitModule_RSS
(void)
{
	WebcitAddUrlHandler(HKEY("feed_rss"), "", 0, feed_rss, ANONYMOUS|COOKIEUNNEEDED);
	RegisterNamespace("THISROOM:FEED:RSS", 0, 0, tmplput_rssbutton, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:FEED:RSSMETA", 0, 0, tmplput_rssmeta, NULL, CTX_NONE);
}
