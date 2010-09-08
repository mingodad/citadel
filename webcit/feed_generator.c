/*
 * RSS/Atom feed generator
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
	wc_printf("<item>");
	wc_printf("<title>title %ld title</title>", msgnum);
	wc_printf("<pubDate>Wed, 08 Sep 2010 20:03:21 GMT</pubDate>");
	wc_printf("<link>http://xxxxx.xxxx.xxxxxx.xxxx.xxx</link>");
	wc_printf("<description>&#60;b&#62;foo bar baz:&#60;/b&#62; message %ld</description>", msgnum);
	wc_printf("<guid>xxxx-xxxx-xxxx-xxxx-xxxx-xxxx</guid>");
	wc_printf("</item>");
}

/*
 * RSS feed generator -- go through the message list
 */
void feed_rss_do_messages(void) {
	char buf[1024];
	long *msgs = NULL;
	int num_msgs = 0;
	int num_msgs_alloc = 0;
	int i;

	serv_puts("MSGS ALL");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000"))
	{
		if (num_msgs >= num_msgs_alloc) {
			num_msgs_alloc += 1024;
			msgs = realloc(msgs, num_msgs_alloc*sizeof(long) );
		}
		msgs[num_msgs++] = atol(buf);
	}

	i = num_msgs;
	while (i > 0) {
		feed_rss_one_message(msgs[i-1]);
		--i;
	}

	free(msgs);

}

/*
 * Entry point for RSS feed generator
 */
void feed_rss(void) {

	output_headers(0, 0, 0, 1, 1, 0);
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

	//	<language>en-us</language>
	//	<description>Linux Today News Service</description>
	//	<atom:link href="http://linuxtoday.com/biglt.rss" rel="self" type="application/rss+xml" />

	//    <image>
	//      <title>Linux Today</title>
	//      <url>http://linuxtoday.com/pics/ltnet.png</url>
	//      <link>http://linuxtoday.com</link>
	//    </image>

	feed_rss_do_messages();

	wc_printf("</channel>"
		"</rss>"
		"\r\n\r\n"
	);

	wDumpContent(0);
	end_webcit_session();
}


void 
InitModule_RSS
(void)
{
	WebcitAddUrlHandler(HKEY("feed_rss"), "", 0, feed_rss, ANONYMOUS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);
}
