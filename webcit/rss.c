/*
 * $Id$
 *
 * Generate some RSS for our rooms.
 */

#include "webcit.h"
#include "webserver.h"


time_t if_modified_since;


void display_rss_control(char *reply_to, char *subject)
{
	wprintf("<div style=\"align: right;\"><p>\n");
	wprintf("<a href=\"display_enter?recp=");
	urlescputs(reply_to);
	wprintf("&subject=");
	if (strncasecmp(subject, "Re: ", 3)) wprintf("Re:%20");
	urlescputs(subject);
	wprintf("\">[%s]</a> \n", _("Reply"));
	wprintf("<a href=\"display_enter?recp=");
	urlescputs(reply_to);
	wprintf("&force_room=_MAIL_&subject=");
	if (strncasecmp(subject, "Re: ", 3)) wprintf("Re:%20");
	urlescputs(subject);
	wprintf("\">[%s]</a>\n", _("Email"));
	wprintf("</p></div>\n");
}


void display_rss(char *roomname, char *request_method)
{
	int nummsgs;
	int a, b;
	int bq = 0;
	time_t now = 0L;
	struct tm now_tm;
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;                   /* Buffer of characters to be converted */
	char *obuf;                   /* Buffer for converted characters      */
	size_t ibuflen;               /* Length of input buffer               */
	size_t obuflen;               /* Length of output buffer              */
	char *osav;                   /* Saved pointer to output buffer       */
	char buf[SIZ];
	char date[30];
	char from[256];
	char subj[256];
	char node[256];
	char hnod[256];
	char room[256];
	char rfca[256];
	char rcpt[256];
	char msgn[256];
	char content_type[256];
	char charset[256];

	if (!WC->logged_in) {
		authorization_required(_("Not logged in"));
		return;
	}

	if (gotoroom((char *)roomname)) {
		lprintf(3, "RSS: Can't goto requested room\n");
		wprintf("HTTP/1.1 404 Not Found\r\n");
		wprintf("Content-Type: text/html\r\n");
		wprintf("\r\n");
		wprintf("Error retrieving RSS feed: couldn't find room\n");
		return;
	}

	nummsgs = load_msg_ptrs("MSGS LAST|15", 0);
	if (nummsgs == 0) {
		lprintf(3, "RSS: No messages found\n");
		wprintf("HTTP/1.1 404 Not Found\r\n");
		wprintf("Content-Type: text/html\r\n");
		wprintf("\r\n");
		wprintf(_("Error retrieving RSS feed: couldn't find messages\n"));
		return;
	}

	/* Read time of last message immediately */
	serv_printf("MSG4 %ld", WC->msgarr[nummsgs - 1]);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcasecmp(buf, "000")) {
			if (!strncasecmp(buf, "msgn=", 5)) {
				strcpy(msgn, &buf[5]);
			}
			if (!strncasecmp(buf, "time=", 5)) {
				now = atol(&buf[5]);
				gmtime_r(&now, &now_tm);
				strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &now_tm);
			}
		}
	}

	if (if_modified_since > 0 && if_modified_since > now) {
		lprintf(3, "RSS: Feed not updated since the last time you looked\n");
		wprintf("HTTP/1.1 304 Not Modified\r\n");
		wprintf("Last-Modified: %s\r\n", date);
		now = time(NULL);
		gmtime_r(&now, &now_tm);
		strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &now_tm);
		wprintf("Date: %s\r\n", date);
/*		if (*msgn) wprintf("ETag: %s\r\n\r\n", msgn); */
		wDumpContent(0);
		return;
	}

	/* Do RSS header */
	lprintf(3, "RSS: Yum yum! This feed is tasty!\n");
	wprintf("HTTP/1.1 200 OK\r\n");
	wprintf("Last-Modified: %s\r\n", date);
/*	if (*msgn) wprintf("ETag: %s\r\n\r\n", msgn); */
	wprintf("Content-Type: application/rss+xml\r\n");
	wprintf("$erver: %s\r\n", SERVER);
	wprintf("Connection: close\r\n");
	wprintf("\r\n");
	if (!strcasecmp(request_method, "HEAD"))
		return;

	wprintf("<?xml version=\"1.0\"?>\n");
	wprintf("<rss version=\"2.0\">\n");
	wprintf("   <channel>\n");
	wprintf("   <title>%s - %s</title>\n", WC->wc_roomname, serv_info.serv_humannode);
	wprintf("   <link>%s://%s:%d/dotgoto?room=", (is_https ? "https" : "http"), WC->http_host, PORT_NUM);
	escputs(roomname);
	wprintf("</link>\n");
	wprintf("   <description>");
	/* Get room info for description */
	serv_puts("RINF");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (1) {
			serv_getln(buf, sizeof buf);
			if (!strcmp(buf, "000"))
				break;
			wprintf("%s\n", buf);
		}
	}
	wprintf("</description>\n");
	if (now) {
		wprintf("   <pubDate>%s</pubDate>\n", date);
	}
	wprintf("   <generator>%s</generator>\n", SERVER);
	wprintf("   <docs>http://blogs.law.harvard.edu/tech/rss</docs>\n");
	wprintf("   <ttl>30</ttl>\n");

	/* Read all messages and output as RSS items */
	for (a = 0; a < nummsgs; ++a) {
		/* Read message and output each as RSS item */
		serv_printf("MSG4 %ld", WC->msgarr[a]);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '1') continue;

		now = 0L;
		strcpy(subj, "");
		strcpy(hnod, "");
		strcpy(node, "");
		strcpy(room, "");
		strcpy(rfca, "");
		strcpy(rcpt, "");
		strcpy(msgn, "");

		while (serv_getln(buf, sizeof buf), strcasecmp(buf, "text")) {
			if (!strcmp(buf, "000")) {
				goto ENDITEM;	/* screw it */
			} else if (!strncasecmp(buf, "from=", 5)) {
				strcpy(from, &buf[5]);
#ifdef HAVE_ICONV
				utf8ify_rfc822_string(from);
#endif
			} else if (!strncasecmp(buf, "subj=", 5)) {
				strcpy(subj, &buf[5]);
#ifdef HAVE_ICONV
				utf8ify_rfc822_string(subj);
#endif
			} else if (!strncasecmp(buf, "hnod=", 5)) {
				strcpy(node, &buf[5]);
			} else if (!strncasecmp(buf, "room=", 5)) {
				strcpy(room, &buf[5]);
			} else if (!strncasecmp(buf, "rfca=", 5)) {
				strcpy(rfca, &buf[5]);
			} else if (!strncasecmp(buf, "rcpt=", 5)) {
				strcpy(rcpt, &buf[5]);
			} else if (!strncasecmp(buf, "msgn=", 5)) {
				strcpy(msgn, &buf[5]);
			} else if (!strncasecmp(buf, "time=", 5)) {
				now = atol(&buf[5]);
				gmtime_r(&now, &now_tm);
				strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &now_tm);
			}
		}
		wprintf("   <item>\n");
		if (subj[0]) {
			wprintf("      <title>%s from", subj);
		} else {
			wprintf("      <title>From");
		}
		wprintf(" %s", from);
		wprintf(" in %s", room);
		if (strcmp(hnod, serv_info.serv_humannode) && strlen(hnod) > 0) {
			wprintf(" on %s", hnod);
		}
		wprintf("</title>\n");
		if (now) {
			wprintf("      <pubDate>%s</pubDate>\n", date);
		}
		wprintf("      <guid isPermaLink=\"false\">%s</guid>\n", msgn);
		/* Now the hard part, the message itself */
		strcpy(content_type, "text/plain");
		while (serv_getln(buf, sizeof buf), strlen(buf) > 0) {
			if (!strcmp(buf, "000")) {
				goto ENDBODY;
			}
			if (!strncasecmp(buf, "Content-type: ", 14)) {
				safestrncpy(content_type, &buf[14], sizeof content_type);
				for (b = 0; b < strlen(content_type); ++b) {
					if (!strncasecmp(&content_type[b], "charset=", 8)) {
						safestrncpy(charset, &content_type[b + 8], sizeof charset);
					}
				}
				for (b = 0; b < strlen(content_type); ++b) {
					if (content_type[b] == ';') {
						content_type[b] = 0;
					}
				}
			}
		}

		/* Set up a character set conversion if we need to */
#ifdef HAVE_ICONV
		if (strcasecmp(charset, "us-ascii") && strcasecmp(charset, "utf-8") && strcasecmp(charset, "") ) {
			ic = iconv_open("UTF-8", charset);
			if (ic == (iconv_t)(-1)) {
				lprintf(5, "%s:%d iconv_open() failed: %s\n",
					__FILE__, __LINE__, strerror(errno));
				goto ENDBODY;
			}
		}
#endif

		/* Messages in legacy Citadel variformat get handled thusly... */
		if (!strcasecmp(content_type, "text/x-citadel-variformat")) {
			int intext = 0;

			wprintf("      <description><![CDATA[");
			while (1) {
				serv_getln(buf, sizeof buf);
				if (!strcmp(buf, "000")) {
					if (bq == 1)
						wprintf("</blockquote>");
					wprintf("\n");
					break;
				}
				if (intext == 1 && isspace(buf[0])) {
					wprintf("<br/>");
				}
				intext = 1;
				if (bq == 0 && !strncmp(buf, " >", 2)) {
					wprintf("<blockquote>");
					bq = 1;
				} else if (bq == 1 && strncmp(buf, " >", 2)) {
					wprintf("</blockquote>");
					bq = 0;
				}
				url(buf);
				escputs(buf);
				wprintf("\n");
			}
			display_rss_control(from, subj);
			wprintf("]]></description>\n");
		}
		/* Boring old 80-column fixed format text gets handled this way... */
		else if (!strcasecmp(content_type, "text/plain")) {
			wprintf("      <description><![CDATA[");
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = 0;
				if (buf[strlen(buf)-1] == '\r') buf[strlen(buf)-1] = 0;
	
#ifdef HAVE_ICONV
				if (ic != (iconv_t)(-1) ) {
					ibuf = buf;
					ibuflen = strlen(ibuf);
					obuflen = SIZ;
					obuf = (char *) malloc(obuflen);
					osav = obuf;
					iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
					osav[SIZ-obuflen] = 0;
					safestrncpy(buf, osav, sizeof buf);
					free(osav);
				}
#endif

				while ((strlen(buf) > 0) && (isspace(buf[strlen(buf) - 1])))
					buf[strlen(buf) - 1] = 0;
				if ((bq == 0) &&
			    	((!strncmp(buf, ">", 1)) || (!strncmp(buf, " >", 2)) || (!strncmp(buf, " :-)", 4)))) {
					wprintf("<blockquote>");
					bq = 1;
				} else if ((bq == 1) &&
				   	(strncmp(buf, ">", 1)) && (strncmp(buf, " >", 2)) && (strncmp(buf, " :-)", 4))) {
					wprintf("</blockquote>");
					bq = 0;
				}
				wprintf("<tt>");
				url(buf);
				escputs(buf);
				wprintf("</tt><br />\n");
			}
			display_rss_control(from, subj);
			wprintf("]]></description>\n");
		}
		/* HTML is fun, but we've got to strip it first */
		else if (!strcasecmp(content_type, "text/html")) {
			wprintf("      <description><![CDATA[");
			output_html(charset);
			wprintf("]]></description>\n");
		}

ENDBODY:
		wprintf("   </item>\n");
ENDITEM:
		now = 0L;
	}

	/* Do RSS footer */
	wprintf("   </channel>\n");
	wprintf("</rss>\n");
	wDumpContent(0);
}
