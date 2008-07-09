/*
 * $Id$
 */
/**
 * \defgroup HTML2HTML Output an HTML message, modifying it slightly to make sure it plays nice
 * with the rest of our web framework.
 * \ingroup WebcitHttpServer
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"


/**
 * \brief	Strip surrounding single or double quotes from a string.
 *
 * \param s	String to be stripped.
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


/**
 * \brief Check to see if a META tag has overridden the declared MIME character set.
 *
 * \param charset		Character set name (left unchanged if we don't do anything)
 * \param meta_http_equiv	Content of the "http-equiv" portion of the META tag
 * \param meta_content		Content of the "content" portion of the META tag
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

	}
}



/**
 * \brief Sanitize and enhance an HTML message for display.
 *        Also convert weird character sets to UTF-8 if necessary.
 *
 * \param supplied_charset the input charset as declared in the MIME headers
 */
void output_html(char *supplied_charset, int treat_as_wiki) {
	char buf[SIZ];
	char *msg;
	char *ptr;
	char *msgstart;
	char *msgend;
	char *converted_msg;
	size_t converted_alloc = 0;
	int buffer_length = 1;
	int line_length = 0;
	int content_length = 0;
	int output_length = 0;
	char new_window[SIZ];
	int brak = 0;
	int alevel = 0;
	int scriptlevel = 0;
	int script_start_pos = (-1);
	int i;
	int linklen;
	char charset[128];
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;                   /**< Buffer of characters to be converted */
	char *obuf;                   /**< Buffer for converted characters      */
	size_t ibuflen;               /**< Length of input buffer               */
	size_t obuflen;               /**< Length of output buffer              */
	char *osav;                   /**< Saved pointer to output buffer       */
#endif

	safestrncpy(charset, supplied_charset, sizeof charset);
	msg = strdup("");
	sprintf(new_window, "<a target=\"%s\" href=", TARGET);

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		line_length = strlen(buf);
		buffer_length = content_length + line_length + 2;
		ptr = realloc(msg, buffer_length);
		if (ptr == NULL) {
			wprintf("<b>");
			wprintf(_("realloc() error! couldn't get %d bytes: %s"),
				buffer_length + 1,
				strerror(errno));
			wprintf("</b><br /><br />\n");
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

		/**
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

		/**
		 * Any of these tags cause everything up to and including
		 * the tag to be removed.
		 */	
		if ( (!strncasecmp(ptr, "HTML", 4))
		   ||(!strncasecmp(ptr, "HEAD", 4))
		   ||(!strncasecmp(ptr, "/HEAD", 5))
		   ||(!strncasecmp(ptr, "BODY", 4)) ) {
			ptr = strchr(ptr, '>');
			if ((ptr == NULL) || (ptr >= msgend)) break;
			++ptr;
			if ((ptr == NULL) || (ptr >= msgend)) break;
			msgstart = ptr;
		}

		/**
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

	/** Convert foreign character sets to UTF-8 if necessary. */
#ifdef HAVE_ICONV
	if ( (strcasecmp(charset, "us-ascii"))
	   && (strcasecmp(charset, "UTF-8"))
	   && (strcasecmp(charset, ""))
	) {
		lprintf(9, "Converting %s to UTF-8\n", charset);
		ic = ctdl_iconv_open("UTF-8", charset);
		if (ic == (iconv_t)(-1) ) {
			lprintf(5, "%s:%d iconv_open() failed: %s\n",
				__FILE__, __LINE__, strerror(errno));
		}
	}
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
#endif

	/**
	 *	At this point, the message has been stripped down to
	 *	only the content inside the <BODY></BODY> tags, and has
	 *	been converted to UTF-8 if it was originally in a foreign
	 *	character set.  The text is also guaranteed to be null
	 *	terminated now.
	 */

	/** Now go through the message, parsing tags as necessary. */
	converted_alloc = content_length + 8192;
	converted_msg = malloc(converted_alloc);
	if (converted_msg == NULL) {
		wprintf("Error %d: %s<br />%s:%d", errno, strerror(errno), __FILE__, __LINE__);
		goto BAIL;
	}

	strcpy(converted_msg, "");
	ptr = msg;
	msgend = strchr(msg, 0);
	while (ptr < msgend) {

		/** Try to sanitize the html of any rogue scripts */
		if (!strncasecmp(ptr, "<script", 7)) {
			if (scriptlevel == 0) {
				script_start_pos = output_length;
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
			if (content_length >= converted_alloc) {
				converted_alloc += 8192;
				converted_msg = realloc(converted_msg, converted_alloc);
				if (converted_msg == NULL) {
					abort();
				}
			}
			sprintf(&converted_msg[output_length],
				"<a href=\"display_enter?force_room=_MAIL_&recp=");
			output_length += 46;
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
				content_length += 64;
				if (content_length >= converted_alloc) {
					converted_alloc += 8192;
					converted_msg = realloc(converted_msg, converted_alloc);
					if (converted_msg == NULL) {
						abort();
					}
				}
				sprintf(&converted_msg[output_length], new_window);
				output_length += strlen(new_window);
				ptr = &ptr[8];
			}
			else if ( (treat_as_wiki) && (strncasecmp(ptr, "<a href=\"wiki?", 14)) ) {
				content_length += 64;
				if (content_length >= converted_alloc) {
					converted_alloc += 8192;
					converted_msg = realloc(converted_msg, converted_alloc);
					if (converted_msg == NULL) {
						abort();
					}
				}
				sprintf(&converted_msg[output_length], "<a href=\"wiki?page=");
				output_length += 19;
				ptr = &ptr[9];
			}
			else {
				sprintf(&converted_msg[output_length], "<a href=\"");
				output_length += 9;
				ptr = &ptr[9];
			}
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
					int len = linklen;
					
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
						///*nbspreviewptr = '\0';
						linklen = nbspreviewptr - ptr;
					}
					if (ltreviewptr != 0)
						*ltreviewptr = '<';

					ptr[len] = linkedchar;

					content_length += (32 + linklen);
					if (content_length >= converted_alloc) {
						converted_alloc += 8192;
						converted_msg = realloc(converted_msg, converted_alloc);
						if (converted_msg == NULL) {
							abort();
						}
					}
					sprintf(&converted_msg[output_length], new_window);
					output_length += strlen(new_window);
					converted_msg[output_length] = '\"';
					converted_msg[++output_length] = 0;
					for (i=0; i<linklen; ++i) {
						converted_msg[output_length] = ptr[i];
						converted_msg[++output_length] = 0;
					}
					sprintf(&converted_msg[output_length], "\">");
					output_length += 2;
					for (i=0; i<linklen; ++i) {
						converted_msg[output_length] = *ptr++;
						converted_msg[++output_length] = 0;
					}
					sprintf(&converted_msg[output_length], "</A>");
					output_length += 4;
				}
		}
		else {
			converted_msg[output_length] = *ptr++;
			converted_msg[++output_length] = 0;
		}

		/**
		 * We need to know when we're inside a tag,
		 * so we don't turn things that look like URL's into
		 * links, when they're already links - or image sources.
		 */
		if (*(ptr-1) == '<') {
			++brak;
		}
		if (*(ptr-1) == '>') {
			--brak;
			if ((scriptlevel == 0) && (script_start_pos >= 0)) {
				output_length = script_start_pos;
				converted_msg[output_length] = 0;
				script_start_pos = (-1);
			}
		}
		if (!strncasecmp(ptr, "</A>", 3)) --alevel;
	}

	/**	uncomment these two lines to override conversion	*/
	/**	memcpy(converted_msg, msg, content_length);		*/
	/**	output_length = content_length;				*/

	/** Output our big pile of markup */
	client_write(converted_msg, output_length);

BAIL:	/** A little trailing vertical whitespace... */
	wprintf("<br /><br />\n");

	/** Now give back the memory */
	if (converted_msg != NULL) free(converted_msg);
	if (msg != NULL) free(msg);
}

/*@}*/
