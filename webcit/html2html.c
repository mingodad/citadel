/*
 * Output an HTML message, modifying it slightly to make sure it plays nice
 * with the rest of our web framework.
 *
 * Copyright (c) 2005-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"


/*
 * Strip surrounding single or double quotes from a string.
 */
void stripquotes(char *s)
{
	int len;

	if (!s) return;

	len = strlen(s);
	if (len < 2) return;

	if ( ( (s[0] == '\"') && (s[len-1] == '\"') ) || ( (s[0] == '\'') && (s[len-1] == '\'') ) ) {
		s[len-1] = 0;
		strcpy(s, &s[1]);
	}
}


/*
 * Check to see if a META tag has overridden the declared MIME character set.
 *
 * charset		Character set name (left unchanged if we don't do anything)
 * meta_http_equiv	Content of the "http-equiv" portion of the META tag
 * meta_content		Content of the "content" portion of the META tag
 */
void extract_charset_from_meta(char *charset, char *meta_http_equiv, char *meta_content)
{
	char *ptr;
	char buf[64];

	if (!charset) return;
	if (!meta_http_equiv) return;
	if (!meta_content) return;


	if (strcasecmp(meta_http_equiv, "Content-type")) return;

	ptr = strchr(meta_content, ';');
	if (!ptr) return;

	safestrncpy(buf, ++ptr, sizeof buf);
	striplt(buf);
	if (!strncasecmp(buf, "charset=", 8)) {
		strcpy(charset, &buf[8]);

		/*
		 * The brain-damaged webmail program in Microsoft Exchange declares
		 * a charset of "unicode" when they really mean "UTF-8".  GNU iconv
		 * treats "unicode" as an alias for "UTF-16" so we have to manually
		 * fix this here, otherwise messages generated in Exchange webmail
		 * show up as a big pile of weird characters.
		 */
		if (!strcasecmp(charset, "unicode")) {
			strcpy(charset, "UTF-8");
		}

		/* Remove wandering punctuation */
		if ((ptr=strchr(charset, '\"'))) *ptr = 0;
		striplt(charset);
	}
}



/*
 * Sanitize and enhance an HTML message for display.
 * Also convert weird character sets to UTF-8 if necessary.
 * Also fixup img src="cid:..." type inline images to fetch the image
 *
 */
void output_html(const char *supplied_charset, int treat_as_wiki, int msgnum, StrBuf *Source, StrBuf *Target) {
	char buf[SIZ];
	char *msg;
	char *ptr;
	char *msgstart;
	char *msgend;
	StrBuf *converted_msg;
	int buffer_length = 1;
	int line_length = 0;
	int content_length = 0;
	char new_window[SIZ];
	int brak = 0;
	int alevel = 0;
	int scriptlevel = 0;
	int script_start_pos = (-1);
	int i;
	int linklen;
	char charset[128];
	StrBuf *BodyArea = NULL;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;                   /* Buffer of characters to be converted */
	char *obuf;                   /* Buffer for converted characters      */
	size_t ibuflen;               /* Length of input buffer               */
	size_t obuflen;               /* Length of output buffer              */
	char *osav;                   /* Saved pointer to output buffer       */
#endif
	if (Target == NULL)
		Target = WC->WBuf;

	safestrncpy(charset, supplied_charset, sizeof charset);
	msg = strdup("");
	sprintf(new_window, "<a target=\"%s\" href=", TARGET);

	if (Source == NULL) while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		line_length = strlen(buf);
		buffer_length = content_length + line_length + 2;
		ptr = realloc(msg, buffer_length);
		if (ptr == NULL) {
			StrBufAppendPrintf(Target, "<b>");
			StrBufAppendPrintf(Target, _("realloc() error! couldn't get %d bytes: %s"),
					buffer_length + 1,
					strerror(errno));
			StrBufAppendPrintf(Target, "</b><br><br>\n");
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				/** flush */
			}
			free(msg);
			return;
		}
		msg = ptr;
		strcpy(&msg[content_length], buf);
		content_length += line_length;
		strcpy(&msg[content_length], "\n");
		content_length += 1;
	}
	else {
		content_length = StrLength(Source);
		free(msg);
		msg = (char*) ChrPtr(Source);/* TODO: remove cast */
		buffer_length = content_length;
	}

	/** Do a first pass to isolate the message body */
	ptr = msg + 1;
	msgstart = msg;
	msgend = &msg[content_length];

	while (ptr < msgend) {

		/** Advance to next tag */
		ptr = strchr(ptr, '<');
		if ((ptr == NULL) || (ptr >= msgend)) break;
		++ptr;
		if ((ptr == NULL) || (ptr >= msgend)) break;

		/*
		 *  Look for META tags.  Some messages (particularly in
		 *  Asian locales) illegally declare a message's character
		 *  set in the HTML instead of in the MIME headers.  This
		 *  is wrong but we have to work around it anyway.
		 */
		if (!strncasecmp(ptr, "META", 4)) {

			char *meta_start;
			char *meta_end;
			int meta_length;
			char *meta;
			char *meta_http_equiv;
			char *meta_content;
			char *spaceptr;

			meta_start = &ptr[4];
			meta_end = strchr(ptr, '>');
			if ((meta_end != NULL) && (meta_end <= msgend)) {
				meta_length = meta_end - meta_start + 1;
				meta = malloc(meta_length + 1);
				safestrncpy(meta, meta_start, meta_length);
				meta[meta_length] = 0;
				striplt(meta);
				if (!strncasecmp(meta, "HTTP-EQUIV=", 11)) {
					meta_http_equiv = strdup(&meta[11]);
					spaceptr = strchr(meta_http_equiv, ' ');
					if (spaceptr != NULL) {
						*spaceptr = 0;
						meta_content = strdup(++spaceptr);
						if (!strncasecmp(meta_content, "content=", 8)) {
							strcpy(meta_content, &meta_content[8]);
							stripquotes(meta_http_equiv);
							stripquotes(meta_content);
							extract_charset_from_meta(charset,
									meta_http_equiv, meta_content);
						}
						free(meta_content);
					}
					free(meta_http_equiv);
				}
				free(meta);
			}
		}

		/*
		 * Any of these tags cause everything up to and including
		 * the tag to be removed.
		 */	
		if ( (!strncasecmp(ptr, "HTML", 4))
				||(!strncasecmp(ptr, "HEAD", 4))
				||(!strncasecmp(ptr, "/HEAD", 5))
				||(!strncasecmp(ptr, "BODY", 4)) ) {
			char *pBody = NULL;

			if (!strncasecmp(ptr, "BODY", 4)) {
				pBody = ptr;
			}
			ptr = strchr(ptr, '>');
			if ((ptr == NULL) || (ptr >= msgend)) break;
			if ((pBody != NULL) && (ptr - pBody > 4)) {
				char* src;
				char *cid_start, *cid_end;

				*ptr = '\0';
				pBody += 4; 
				while ((isspace(*pBody)) && (pBody < ptr))
					pBody ++;
				BodyArea = NewStrBufPlain(NULL,  ptr - pBody);

				if (pBody < ptr) {
					src = strstr(pBody, "cid:");
					if (src) {
						cid_start = src + 4;
						cid_end = cid_start;
						while ((*cid_end != '"') && 
								!isspace(*cid_end) &&
								(cid_end < ptr))
							cid_end ++;

						/* copy tag and attributes up to src="cid: */
						StrBufAppendBufPlain(BodyArea, pBody, src - pBody, 0);

						/* add in /webcit/mimepart/<msgno>/CID/ 
						   trailing / stops dumb URL filters getting excited */
						StrBufAppendPrintf(BodyArea,
								"/webcit/mimepart/%d/",msgnum);
						StrBufAppendBufPlain(BodyArea, cid_start, cid_end - cid_start, 0);

						if (ptr - cid_end > 0)
							StrBufAppendBufPlain(BodyArea, 
									cid_end + 1, 
									ptr - cid_end, 0);
					}
					else 
						StrBufAppendBufPlain(BodyArea, pBody, ptr - pBody, 0);
				}
				*ptr = '>';
			}
			++ptr;
			if ((ptr == NULL) || (ptr >= msgend)) break;
			msgstart = ptr;
		}

		/*
		 * Any of these tags cause everything including and following
		 * the tag to be removed.
		 */
		if ( (!strncasecmp(ptr, "/HTML", 5))
				||(!strncasecmp(ptr, "/BODY", 5)) ) {
			--ptr;
			msgend = ptr;
			strcpy(ptr, "");

		}

		++ptr;
	}
	if (msgstart > msg) {
		strcpy(msg, msgstart);
	}

	/* Now go through the message, parsing tags as necessary. */
	converted_msg = NewStrBufPlain(NULL, content_length + 8192);


	/** Convert foreign character sets to UTF-8 if necessary. */
#ifdef HAVE_ICONV
	if ( (strcasecmp(charset, "us-ascii"))
			&& (strcasecmp(charset, "UTF-8"))
			&& (strcasecmp(charset, ""))
	   ) {
		syslog(9, "Converting %s to UTF-8\n", charset);
		ctdl_iconv_open("UTF-8", charset, &ic);
		if (ic == (iconv_t)(-1) ) {
			syslog(5, "%s:%d iconv_open() failed: %s\n",
					__FILE__, __LINE__, strerror(errno));
		}
	}
	if  (Source == NULL) {
		if (ic != (iconv_t)(-1) ) {
			ibuf = msg;
			ibuflen = content_length;
			obuflen = content_length + (content_length / 2) ;
			obuf = (char *) malloc(obuflen);
			osav = obuf;
			iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
			content_length = content_length + (content_length / 2) - obuflen;
			osav[content_length] = 0;
			free(msg);
			msg = osav;
			iconv_close(ic);
		}
	}
	else {
		if (ic != (iconv_t)(-1) ) {
			StrBuf *Buf = NewStrBufPlain(NULL, StrLength(Source) + 8096);;
			StrBufConvert(Source, Buf, &ic);
			FreeStrBuf(&Buf);
			iconv_close(ic);
			msg = (char*)ChrPtr(Source); /* TODO: get rid of this. */
		}
	}

#endif

	/*
	 *	At this point, the message has been stripped down to
	 *	only the content inside the <BODY></BODY> tags, and has
	 *	been converted to UTF-8 if it was originally in a foreign
	 *	character set.  The text is also guaranteed to be null
	 *	terminated now.
	 */

	if (converted_msg == NULL) {
		StrBufAppendPrintf(Target, "Error %d: %s<br>%s:%d", errno, strerror(errno), __FILE__, __LINE__);
		goto BAIL;
	}

	if (BodyArea != NULL) {
		StrBufAppendBufPlain(converted_msg, HKEY("<table "), 0);  
		StrBufAppendBuf(converted_msg, BodyArea, 0);
		StrBufAppendBufPlain(converted_msg, HKEY(" width=\"100%\"><tr><td>"), 0);
	}
	ptr = msg;
	msgend = strchr(msg, 0);
	while (ptr < msgend) {

		/** Try to sanitize the html of any rogue scripts */
		if (!strncasecmp(ptr, "<script", 7)) {
			if (scriptlevel == 0) {
				script_start_pos = StrLength(converted_msg);
			}
			++scriptlevel;
		}
		if (!strncasecmp(ptr, "</script", 8)) {
			--scriptlevel;
		}

		/**
		 * Change mailto: links to WebCit mail, by replacing the
		 * link with one that points back to our mail room.  Due to
		 * the way we parse URL's, it'll even handle mailto: links
		 * that have "?subject=" in them.
		 */
		if (!strncasecmp(ptr, "<a href=\"mailto:", 16)) {
			content_length += 64;
			StrBufAppendPrintf(converted_msg,
					"<a href=\"display_enter?force_room=_MAIL_?recp=");
			ptr = &ptr[16];
			++alevel;
			++brak;
		}
		/** Make external links open in a separate window */
		else if (!strncasecmp(ptr, "<a href=\"", 9)) {
			++alevel;
			++brak;
			if ( ((strchr(ptr, ':') < strchr(ptr, '/')))
					&&  ((strchr(ptr, '/') < strchr(ptr, '>'))) 
			   ) {
				/* open external links to new window */
				StrBufAppendPrintf(converted_msg, new_window);
				ptr = &ptr[8];
			}
			else if (
				(treat_as_wiki)
				&& (strncasecmp(ptr, "<a href=\"wiki?", 14))
				&& (strncasecmp(ptr, "<a href=\"dotgoto?", 17))
				&& (strncasecmp(ptr, "<a href=\"knrooms?", 17))
			) {
				content_length += 64;
				StrBufAppendPrintf(converted_msg, "<a href=\"wiki?go=");
				StrBufUrlescAppend(converted_msg, WC->CurRoom.name, NULL);
				StrBufAppendPrintf(converted_msg, "?page=");
				ptr = &ptr[9];
			}
			else {
				StrBufAppendPrintf(converted_msg, "<a href=\"");
				ptr = &ptr[9];
			}
		}
		/** Fixup <img src="cid:... ...> to fetch the mime part */
		else if (!strncasecmp(ptr, "<img ", 5)) {
			char *cid_start, *cid_end;
			char* tag_end=strchr(ptr,'>');
			char* src;
			/* FIXME - handle this situation (maybe someone opened an <img cid... 
			 * and then ended the message)
			 */
			if (!tag_end) {
				syslog(9, "tag_end is null and ptr is:\n");
				syslog(9, "%s\n", ptr);
				syslog(9, "Theoretical bytes remaining: %d\n", (int)(msgend - ptr));
			}

			src=strstr(ptr, "src=\"cid:");
			++brak;

			if (src
			    && isspace(*(src-1))
				&& tag_end
				&& (cid_start=strchr(src,':'))
				&& (cid_end=strchr(cid_start,'"'))
				&& (cid_end < tag_end)
			) {
				/* copy tag and attributes up to src="cid: */
				StrBufAppendBufPlain(converted_msg, ptr, src - ptr, 0);
				cid_start++;

				/* add in /webcit/mimepart/<msgno>/CID/ 
				   trailing / stops dumb URL filters getting excited */
				StrBufAppendPrintf(converted_msg,
						" src=\"/webcit/mimepart/%d/",msgnum);
				StrBufAppendBufPlain(converted_msg, cid_start, cid_end - cid_start, 0);
				StrBufAppendBufPlain(converted_msg, "/\"", -1, 0);

				ptr = cid_end+1;
			}
			StrBufAppendBufPlain(converted_msg, ptr, tag_end - ptr, 0);
			ptr = tag_end;
		}

		/**
		 * Turn anything that looks like a URL into a real link, as long
		 * as it's not inside a tag already
		 */
		else if ( (brak == 0) && (alevel == 0)
		     && (!strncasecmp(ptr, "http://", 7))) {
				/** Find the end of the link */
				int strlenptr;
				linklen = 0;
				
				strlenptr = strlen(ptr);
				for (i=0; i<=strlenptr; ++i) {
					if ((ptr[i]==0)
					   ||(isspace(ptr[i]))
					   ||(ptr[i]==10)
					   ||(ptr[i]==13)
					   ||(ptr[i]=='(')
					   ||(ptr[i]==')')
					   ||(ptr[i]=='<')
					   ||(ptr[i]=='>')
					   ||(ptr[i]=='[')
					   ||(ptr[i]==']')
					   ||(ptr[i]=='"')
					   ||(ptr[i]=='\'')
					) linklen = i;
					/* did s.b. send us an entity? */
					if (ptr[i] == '&') {
						if ((ptr[i+2] ==';') ||
						    (ptr[i+3] ==';') ||
						    (ptr[i+5] ==';') ||
						    (ptr[i+6] ==';') ||
						    (ptr[i+7] ==';'))
							linklen = i;
					}
					if (linklen > 0) break;
				}
				if (linklen > 0) {
					char *ltreviewptr;
					char *nbspreviewptr;
					char linkedchar;
					int len;
					
					len = linklen;
					linkedchar = ptr[len];
					ptr[len] = '\0';
					/* spot for some subject strings tinymce tends to give us. */
					ltreviewptr = strchr(ptr, '<');
					if (ltreviewptr != NULL) {
						*ltreviewptr = '\0';
						linklen = ltreviewptr - ptr;
					}

					nbspreviewptr = strstr(ptr, "&nbsp;");
					if (nbspreviewptr != NULL) {
						/* nbspreviewptr = '\0'; */
						linklen = nbspreviewptr - ptr;
					}
					if (ltreviewptr != 0)
						*ltreviewptr = '<';

					ptr[len] = linkedchar;

					content_length += (32 + linklen);
                                        StrBufAppendPrintf(converted_msg, "%s\"", new_window);
					StrBufAppendBufPlain(converted_msg, ptr, linklen, 0);
					StrBufAppendPrintf(converted_msg, "\">");
					StrBufAppendBufPlain(converted_msg, ptr, linklen, 0);
					ptr += linklen;
					StrBufAppendPrintf(converted_msg, "</A>");
				}
		}
		else {
			StrBufAppendBufPlain(converted_msg, ptr, 1, 0);
			ptr++;
		}


		if ((ptr >= msg) && (ptr <= msgend)) {
			/*
			 * We need to know when we're inside a tag,
			 * so we don't turn things that look like URL's into
			 * links, when they're already links - or image sources.
			 */
			if ((ptr > msg) && (*(ptr-1) == '<')) {
				++brak;
			}
			if ((ptr > msg) && (*(ptr-1) == '>')) {
				--brak;
				if ((scriptlevel == 0) && (script_start_pos >= 0)) {
					StrBufCutRight(converted_msg, StrLength(converted_msg) - script_start_pos);
					script_start_pos = (-1);
				}
			}
			if (!strncasecmp(ptr, "</A>", 3)) --alevel;
		}
	}

	if (BodyArea != NULL) {
		StrBufAppendBufPlain(converted_msg, HKEY("</td></tr></table>"), 0);  
		FreeStrBuf(&BodyArea);
	}

	/**	uncomment these two lines to override conversion	*/
	/**	memcpy(converted_msg, msg, content_length);		*/
	/**	output_length = content_length;				*/

	/** Output our big pile of markup */
	StrBufAppendBuf(Target, converted_msg, 0);

BAIL:	/** A little trailing vertical whitespace... */
	StrBufAppendPrintf(Target, "<br><br>\n");

	/** Now give back the memory */
	FreeStrBuf(&converted_msg);
	if ((msg != NULL) && (Source == NULL)) free(msg);
}






/*
 * Look for URL's embedded in a buffer and make them linkable.  We use a
 * target window in order to keep the Citadel session in its own window.
 */
void UrlizeText(StrBuf* Target, StrBuf *Source, StrBuf *WrkBuf)
{
	int len, UrlLen, Offset, TrailerLen;
	const char *start, *end, *pos;
	
	FlushStrBuf(Target);

	start = NULL;
	len = StrLength(Source);
	end = ChrPtr(Source) + len;
	for (pos = ChrPtr(Source); (pos < end) && (start == NULL); ++pos) {
		if (!strncasecmp(pos, "http://", 7))
			start = pos;
		else if (!strncasecmp(pos, "ftp://", 6))
			start = pos;
	}

	if (start == NULL) {
		StrBufAppendBuf(Target, Source, 0);
		return;
	}
	FlushStrBuf(WrkBuf);

	for (pos = ChrPtr(Source) + len; pos > start; --pos) {
		if (  (!isprint(*pos))
		   || (isspace(*pos))
		   || (*pos == '{')
		   || (*pos == '}')
		   || (*pos == '|')
		   || (*pos == '\\')
		   || (*pos == '^')
		   || (*pos == '[')
		   || (*pos == ']')
		   || (*pos == '`')
		   || (*pos == '<')
		   || (*pos == '>')
		   || (*pos == '(')
		   || (*pos == ')')
		) {
			end = pos;
		}
	}
	
	UrlLen = end - start;
	StrBufAppendBufPlain(WrkBuf, start, UrlLen, 0);

	Offset = start - ChrPtr(Source);
	if (Offset != 0)
		StrBufAppendBufPlain(Target, ChrPtr(Source), Offset, 0);
	StrBufAppendPrintf(Target, "%ca href=%c%s%c TARGET=%c%s%c%c%s%c/A%c",
			   LB, QU, ChrPtr(WrkBuf), QU, QU, TARGET, 
			   QU, RB, ChrPtr(WrkBuf), LB, RB);

	TrailerLen = StrLength(Source) - (end - ChrPtr(Source));
	if (TrailerLen > 0)
		StrBufAppendBufPlain(Target, end, TrailerLen, 0);
}


void url(char *buf, size_t bufsize)
{
	int len, UrlLen, Offset, TrailerLen, outpos;
	char *start, *end, *pos;
	char urlbuf[SIZ];
	char outbuf[SIZ];

	start = NULL;
	len = strlen(buf);
	if (len > bufsize) {
		syslog(1, "URL: content longer than buffer!");
		return;
	}
	end = buf + len;
	for (pos = buf; (pos < end) && (start == NULL); ++pos) {
		if (!strncasecmp(pos, "http://", 7))
			start = pos;
		if (!strncasecmp(pos, "ftp://", 6))
			start = pos;
	}

	if (start == NULL)
		return;

	for (pos = buf+len; pos > start; --pos) {
		if (  (!isprint(*pos))
		   || (isspace(*pos))
		   || (*pos == '{')
		   || (*pos == '}')
		   || (*pos == '|')
		   || (*pos == '\\')
		   || (*pos == '^')
		   || (*pos == '[')
		   || (*pos == ']')
		   || (*pos == '`')
		   || (*pos == '<')
		   || (*pos == '>')
		   || (*pos == '(')
		   || (*pos == ')')
		) {
			end = pos;
		}
	}
	
	UrlLen = end - start;
	if (UrlLen > sizeof(urlbuf)){
		syslog(1, "URL: content longer than buffer!");
		return;
	}
	memcpy(urlbuf, start, UrlLen);
	urlbuf[UrlLen] = '\0';

	Offset = start - buf;
	if ((Offset != 0) && (Offset < sizeof(outbuf)))
		memcpy(outbuf, buf, Offset);
	outpos = snprintf(&outbuf[Offset], sizeof(outbuf) - Offset,  
			  "%ca href=%c%s%c TARGET=%c%s%c%c%s%c/A%c",
			  LB, QU, urlbuf, QU, QU, TARGET, QU, RB, urlbuf, LB, RB);
	if (outpos >= sizeof(outbuf) - Offset) {
		syslog(1, "URL: content longer than buffer!");
		return;
	}

	TrailerLen = len - (end - start);
	if (TrailerLen > 0)
		memcpy(outbuf + Offset + outpos, end, TrailerLen);
	if (Offset + outpos + TrailerLen > bufsize) {
		syslog(1, "URL: content longer than buffer!");
		return;
	}
	memcpy (buf, outbuf, Offset + outpos + TrailerLen);
	*(buf + Offset + outpos + TrailerLen) = '\0';
}

