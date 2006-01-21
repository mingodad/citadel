/*
 * $Id$
 */
/**
 * \defgroup HTML2HTML Output an HTML message, modifying it slightly to make sure it plays nice
 * with the rest of our web framework.
 *
 */
/*@{*/
#include "webcit.h"
#include "vcard.h"
#include "webserver.h"


/**
 * \brief Sanitize and enhance an HTML message for display.
 * Also convert weird character sets to UTF-8 if necessary.
 * \param charset the input charset
 */
void output_html(char *charset) {
	char buf[SIZ];
	char *msg;
	char *ptr;
	char *msgstart;
	char *msgend;
	char *converted_msg;
	int buffer_length = 1;
	int line_length = 0;
	int content_length = 0;
	int output_length = 0;
	char new_window[SIZ];
	int brak = 0;
	int alevel = 0;
	int i;
	int linklen;
#ifdef HAVE_ICONV
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;                   /**< Buffer of characters to be converted */
	char *obuf;                   /**< Buffer for converted characters      */
	size_t ibuflen;               /**< Length of input buffer               */
	size_t obuflen;               /**< Length of output buffer              */
	char *osav;                   /**< Saved pointer to output buffer       */
#endif

	msg = strdup("");
	sprintf(new_window, "<a target=\"%s\" href=", TARGET);

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		line_length = strlen(buf);
		buffer_length = content_length + line_length + 2;
		msg = realloc(msg, buffer_length);
		if (msg == NULL) {
			wprintf("<b>");
			wprintf(_("realloc() error! couldn't get %d bytes: %s"),
				buffer_length + 1,
				strerror(errno));
			wprintf("</b><br /><br />\n");
			return;
		}
		strcpy(&msg[content_length], buf);
		content_length += line_length;
		strcpy(&msg[content_length], "\n");
		content_length += 1;
	}

#ifdef HAVE_ICONV
	if ( (strcasecmp(charset, "us-ascii"))
	   && (strcasecmp(charset, "UTF-8"))
	   && (strcasecmp(charset, ""))
	) {
		ic = iconv_open("UTF-8", charset);
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

	ptr = msg;
	msgstart = msg;
	msgend = &msg[content_length];

	while (ptr < msgend) {

		/** Advance to next tag */
		ptr = strchr(ptr, '<');
		if ((ptr == NULL) || (ptr >= msgend)) break;
		++ptr;
		if ((ptr == NULL) || (ptr >= msgend)) break;

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

	converted_msg = malloc(content_length);
	strcpy(converted_msg, "");
	ptr = msgstart;
	while (ptr < msgend) {
		/**
		 * Change mailto: links to WebCit mail, by replacing the
		 * link with one that points back to our mail room.  Due to
		 * the way we parse URL's, it'll even handle mailto: links
		 * that have "?subject=" in them.
		 */
		if (!strncasecmp(ptr, "<a href=\"mailto:", 16)) {
			content_length += 64;
			converted_msg = realloc(converted_msg, content_length);
			sprintf(&converted_msg[output_length],
				"<a href=\"display_enter"
				"?force_room=_MAIL_&recp=");
			output_length += 47;
			ptr = &ptr[16];
			++alevel;
		}
		/** Make links open in a separate window */
		else if (!strncasecmp(ptr, "<a href=", 8)) {
			content_length += 64;
			converted_msg = realloc(converted_msg, content_length);
			sprintf(&converted_msg[output_length], new_window);
			output_length += strlen(new_window);
			ptr = &ptr[8];
			++alevel;
		}
		/**
		 * Turn anything that looks like a URL into a real link, as long
		 * as it's not inside a tag already
		 */
		else if ( (brak == 0) && (alevel == 0)
		     && (!strncasecmp(ptr, "http://", 7))) {
				linklen = 0;
				/** Find the end of the link */
				for (i=0; i<=strlen(ptr); ++i) {
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
					) linklen = i;
					if (linklen > 0) break;
				}
				if (linklen > 0) {
					content_length += (32 + linklen);
					converted_msg = realloc(converted_msg, content_length);
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
			/**
			 * We need to know when we're inside a tag,
			 * so we don't turn things that look like URL's into
			 * links, when they're already links - or image sources.
			 */
			if (*ptr == '<') ++brak;
			if (*ptr == '>') --brak;
			if (!strncasecmp(ptr, "</A>", 3)) --alevel;
			converted_msg[output_length] = *ptr++;
			converted_msg[++output_length] = 0;
		}
	}

	/** Output our big pile of markup */
	client_write(converted_msg, output_length);

	/** A little trailing vertical whitespace... */
	wprintf("<br /><br />\n");

	/** Now give back the memory */
	free(converted_msg);
	free(msg);
}

/*@}*/
