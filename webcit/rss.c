/*
 * $Id$
 */
/**
 * \defgroup RssRooms Generate some RSS for our rooms.
 * \ingroup WebcitHttpServerRSS
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"

/* Since we don't have anonymous Webcit access yet, you can 
 * allow the feed to be produced by a special user created just for 
 * this purpose. The Citadel Developers do not take any responsibility
 * for the security of your system when you use this 'feature' */
#define ALLOW_ANON_RSS 0
#define ANON_RSS_USER ""
#define ANON_RSS_PASS ""
time_t if_modified_since;    /**< the last modified stamp */

/**
 * \brief view rss Config menu
 * \param reply_to the original author
 * \param subject the subject of the feed
 */
void display_rss_control(char *reply_to, char *subject)
{
	wprintf("<div style=\"align: right;\"><p>\n");
	wprintf("<a href=\"display_enter?recp=");
	urlescputs(reply_to);
	wprintf("&subject=");
	if (strncasecmp(subject, "Re: ", 3)) wprintf("Re:%%20");
	urlescputs(subject);
	wprintf("\">[%s]</a> \n", _("Reply"));
	wprintf("<a href=\"display_enter?recp=");
	urlescputs(reply_to);
	wprintf("&force_room=_MAIL_&subject=");
	if (strncasecmp(subject, "Re: ", 3)) wprintf("Re:%%20");
	urlescputs(subject);
	wprintf("\">[%s]</a>\n", _("Email"));
	wprintf("</p></div>\n");
}


/**
 * \brief print the feed to the subscriber
 * \param roomname the room we sould print out as rss 
 * \param request_method the way the rss is requested????
 */
void display_rss(char *roomname, char *request_method)
{
	int nummsgs;
	int a, b;
	int bq = 0;
	time_t now = 0L;
	struct tm now_tm;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;                   /**< Buffer of characters to be converted */
	char *obuf;                   /**< Buffer for converted characters      */
	size_t ibuflen;               /**< Length of input buffer               */
	size_t obuflen;               /**< Length of output buffer              */
	char *osav;                   /**< Saved pointer to output buffer       */
#endif
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
		#ifdef ALLOW_ANON_RSS
		serv_printf("USER %s", ANON_RSS_USER);
		serv_getln(buf, sizeof buf);
		serv_printf("PASS %s", ANON_RSS_PASS);
		serv_getln(buf, sizeof buf);
		become_logged_in(ANON_RSS_USER, ANON_RSS_PASS, buf);
		WC->killthis = 1;
		#else
		authorization_required(_("Not logged in"));
		return;
		#endif
	}

	if (gotoroom((char *)roomname)) {
		lprintf(3, "RSS: Can't goto requested room\n");
		hprintf("HTTP/1.1 404 Not Found\r\n");
		hprintf("Content-Type: text/html\r\n");
		wprintf("Error retrieving RSS feed: couldn't find room\n");
		end_burst();
		return;
	}

	 nummsgs = load_msg_ptrs("MSGS LAST|15", 0);
	if (nummsgs == 0) {
		lprintf(3, "RSS: No messages found\n");
		hprintf("HTTP/1.1 404 Not Found\r\n");
		hprintf("Content-Type: text/html\r\n");
		wprintf(_("Error retrieving RSS feed: couldn't find messages\n"));
		end_burst();
		return;
	} 
	

	/** Read time of last message immediately */
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
	// Commented out. Play dumb for now, also doesn't work with anonrss hack
	/* if (if_modified_since > 0 && if_modified_since > now) {
		lprintf(3, "RSS: Feed not updated since the last time you looked\n");
		hprintf("HTTP/1.1 304 Not Modified\r\n");
		hprintf("Last-Modified: %s\r\n", date);
		now = time(NULL);
		gmtime_r(&now, &now_tm);
		strftime(date, sizeof date, "%a, %d %b %Y %H:%M:%S GMT", &now_tm);
		hprintf("Date: %s\r\n", date);
		if (*msgn) hprintf("ETag: %s\r\n", msgn); */
		// wDumpContent(0);
		// return;
	//} 

	/* Do RSS header */
	lprintf(3, "RSS: Yum yum! This feed is tasty!\n");
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Last-Modified: %s\r\n", date);
/*	if (*msgn) wprintf("ETag: %s\r\n\r\n", msgn); */
	hprintf("Content-Type: application/rss+xml\r\n");
	hprintf("Server: %s\r\n", PACKAGE_STRING);
	hprintf("Connection: close\r\n");
	if (!strcasecmp(request_method, "HEAD"))
		return;

	/* <?xml.. etc confuses our subst parser, so do it here */
	svput("XML_HEAD", WCS_STRING, "<?xml version=\"1.0\" ?>");
	svput("XML_STYLE", WCS_STRING, "<?xml-stylesheet type=\"text/css\" href=\"/static/rss_browser.css\" ?>");
	svput("ROOM", WCS_STRING, WC->wc_roomname);
	svput("NODE", WCS_STRING, serv_info.serv_humannode);
	// Fix me
	svprintf(HKEY("ROOM_LINK"), WCS_STRING, "%s://%s/", (is_https ? "https" : "http"), WC->http_host);
	
	/** Get room info for description */
	serv_puts("RINF");
	serv_getln(buf, sizeof buf);
	char description[SIZ] = "";
	if (buf[0] == '1') {
		while (1) {
			serv_getln(buf, sizeof buf);
			if (!strcmp(buf, "000"))
				break;
			strncat(description, buf, strlen(buf));
		}
	}
	svput("ROOM_DESCRIPTION", WCS_STRING, description);
	if (now) {
		svput("822_PUB_DATE", WCS_STRING, date);
	}
	svput("GENERATOR", WCS_STRING, PACKAGE_STRING);
	do_template("rss_head");

	/** Read all messages and output as RSS items */
	for (a = 0; a < nummsgs; ++a) {
		/** Read message and output each as RSS item */
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
				goto ENDITEM;	/** screw it */
			} else if (!strncasecmp(buf, "from=", 5)) {
				strcpy(from, &buf[5]);
				utf8ify_rfc822_string(from);
			} else if (!strncasecmp(buf, "subj=", 5)) {
				strcpy(subj, &buf[5]);
				utf8ify_rfc822_string(subj);
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
		if (subj[0]) {
			svprintf(HKEY("SUBJ"), WCS_STRING, _("%s from"), subj);
		} else {
			svput("SUBJ", WCS_STRING, _("From"));
		}
		svprintf(HKEY("IN_ROOM"), WCS_STRING, _("%s in %s"), from, room);
		if (strcmp(hnod, serv_info.serv_humannode) && !IsEmptyStr(hnod)) {
			svprintf(HKEY("NODE"), WCS_STRING, _(" on %s"), hnod);
		}
		if (now) {
			svprintf(HKEY("822_PUB_DATE"),WCS_STRING, _("%s"), date);
		}
		svprintf(HKEY("GUID"), WCS_STRING,"%s", msgn);
		do_template("rss_item");
		/** Now the hard part, the message itself */
		strcpy(content_type, "text/plain");
		while (serv_getln(buf, sizeof buf), !IsEmptyStr(buf)) {
			if (!strcmp(buf, "000")) {
				goto ENDBODY;
			}
			if (!strncasecmp(buf, "Content-type: ", 14)) {
				int len;
				safestrncpy(content_type, &buf[14], sizeof content_type);
				len = strlen (content_type);
				for (b = 0; b < len; ++b) {
					if (!strncasecmp(&content_type[b], "charset=", 8)) {
						safestrncpy(charset, &content_type[b + 8], sizeof charset);
					}
				}
				for (b = 0; b < len; ++b) {
					if (content_type[b] == ';') {
						content_type[b] = 0;
						len = b - 1;
					}
				}
			}
		}

		/** Set up a character set conversion if we need to */
 #ifdef HAVE_ICONV
		if (strcasecmp(charset, "us-ascii") && strcasecmp(charset, "utf-8") && strcasecmp(charset, "") ) {
			ic = ctdl_iconv_open("UTF-8", charset);
			if (ic == (iconv_t)(-1)) {
				lprintf(5, "%s:%d iconv_open() failed: %s\n",
					__FILE__, __LINE__, strerror(errno));
				goto ENDBODY;
			}
		}
#endif 

		/** Messages in legacy Citadel variformat get handled thusly... */
		if (!strcasecmp(content_type, "text/x-citadel-variformat")) {
			int intext = 0;

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
				url(buf, sizeof(buf));
				escputs(buf);
				wprintf("\n");
			}
			display_rss_control(from, subj);
		} 
		/** Boring old 80-column fixed format text gets handled this way... */
		else if (!strcasecmp(content_type, "text/plain")) {
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				int len;
				len = strlen(buf);
				if (buf[len-1] == '\n') buf[--len] = 0;
				if (buf[len-1] == '\r') buf[--len] = 0;
	
#ifdef HAVE_ICONV
				if (ic != (iconv_t)(-1) ) {
					ibuf = buf;
					ibuflen = len;
					obuflen = SIZ;
					obuf = (char *) malloc(obuflen);
					osav = obuf;
					iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
					osav[SIZ-obuflen] = 0;
					safestrncpy(buf, osav, sizeof buf);
					free(osav);
				}
#endif
				len = strlen (buf);
				while ((!IsEmptyStr(buf)) && (isspace(buf[len - 1])))
					buf[--len] = 0;
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
				url(buf, sizeof(buf));
				escputs(buf);
				wprintf("</tt><br />\n");
			}
			display_rss_control(from, subj);
		} 
		/** HTML is fun, but we've got to strip it first */
		else if (!strcasecmp(content_type, "text/html")) {
			output_html(charset, 0); 
		} 

ENDBODY:
		//wprintf("   </item>\n");
		do_template("rss_item_end");
ENDITEM:
		now = 0L;
	}

	/** Do RSS footer */
	wprintf("   </channel>\n");
	wprintf("</rss>\n");
	wDumpContent(0);
	#ifdef ALLOW_ANON_RSS
	end_webcit_session();
	#endif
}


/*@}*/
