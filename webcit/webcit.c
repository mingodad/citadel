/*
 * $Id$
 *
 * This is the main transaction loop of the web service.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 */

#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"

#include <stdio.h>
#include <stdarg.h>

/*
 * String to unset the cookie.
 * Any date "in the past" will work, so I chose my birthday, right down to
 * the exact minute.  :)
 */
static char *unset = "; expires=28-May-1971 18:10:00 GMT";

HashList *HandlerHash = NULL;


void WebcitAddUrlHandler(const char * UrlString, long UrlSLen, WebcitHandlerFunc F, int IsAjax)
{
	WebcitHandler *NewHandler;

	if (HandlerHash == NULL)
		HandlerHash = NewHash(1, NULL);
	
	NewHandler = (WebcitHandler*) malloc(sizeof(WebcitHandler));
	NewHandler->F = F;
	NewHandler->IsAjax = IsAjax;

	Put(HandlerHash, UrlString, UrlSLen, NewHandler, NULL);
}

/*   
 * remove escaped strings from i.e. the url string (like %20 for blanks)
 */
long unescape_input(char *buf)
{
	int a, b;
	char hex[3];
	long buflen;
	long len;

	buflen = strlen(buf);

	while ((buflen > 0) && (isspace(buf[buflen - 1]))){
		buf[buflen - 1] = 0;
		buflen --;
	}

	a = 0; 
	while (a < buflen) {
		if (buf[a] == '+')
			buf[a] = ' ';
		if (buf[a] == '%') {
			/* don't let % chars through, rather truncate the input. */
			if (a + 2 > buflen) {
				buf[a] = '\0';
				buflen = a;
			}
			else {			
				hex[0] = buf[a + 1];
				hex[1] = buf[a + 2];
				hex[2] = 0;
				b = 0;
				sscanf(hex, "%02x", &b);
				buf[a] = (char) b;
				len = buflen - a - 2;
				if (len > 0)
					memmove(&buf[a + 1], &buf[a + 3], len);
			
				buflen -=2;
			}
		}
		a++;
	}
	return a;
}

void free_url(void *U)
{
	urlcontent *u = (urlcontent*) U;
	free(u->url_data);
	free(u);
}

/*
 * Extract variables from the URL.
 */
void addurls(char *url, long ulen)
{
	char *aptr, *bptr, *eptr;
	char *up;
	char *buf;
	int len, keylen;
	urlcontent *u;
	struct wcsession *WCC = WC;

	if (WCC->urlstrings == NULL)
		WCC->urlstrings = NewHash(1, NULL);
	buf = (char*) malloc (ulen + 1);
	memcpy(buf, url, ulen);
	buf[ulen] = '\0';
	eptr = buf + ulen;
	up = buf;
	while ((up < eptr) && (!IsEmptyStr(up))) {
		aptr = up;
		while ((aptr < eptr) && (*aptr != '\0') && (*aptr != '='))
			aptr++;
		if (*aptr != '=')
			return;
		*aptr = '\0';
		aptr++;
		bptr = aptr;
		while ((bptr < eptr) && (*bptr != '\0')
		      && (*bptr != '&') && (*bptr != '?') && (*bptr != ' ')) {
			bptr++;
		}
		*bptr = '\0';
		u = (urlcontent *) malloc(sizeof(urlcontent));

		keylen = safestrncpy(u->url_key, up, sizeof u->url_key);
		if (keylen < 0){
			lprintf(1, "URLkey to long! [%s]", up);
			continue;
		}

		Put(WCC->urlstrings, u->url_key, keylen, u, free_url);
		len = bptr - aptr;
		u->url_data = malloc(len + 2);
		safestrncpy(u->url_data, aptr, len + 2);
		u->url_data_size = unescape_input(u->url_data);
		u->url_data[u->url_data_size] = '\0';
		up = bptr;
		++up;
#ifdef DEBUG_URLSTRINGS
		lprintf(9, "%s = [%ld]  %s\n", u->url_key, u->url_data_size, u->url_data); 
#endif
	}
	free(buf);
}

/*
 * free urlstring memory
 */
void free_urls(void)
{
	DeleteHash(&WC->urlstrings);
}

/*
 * Diagnostic function to display the contents of all variables
 */

void dump_vars(void)
{
	struct wcsession *WCC = WC;
	urlcontent *u;
	void *U;
	long HKLen;
	char *HKey;
	HashPos *Cursor;
	
	Cursor = GetNewHashPos ();
	while (GetNextHashPos(WCC->urlstrings, Cursor, &HKLen, &HKey, &U)) {
		u = (urlcontent*) U;
		wprintf("%38s = %s\n", u->url_key, u->url_data);
	}
}

/*
 * Return the value of a variable supplied to the current web page (from the url or a form)
 */

const char *XBstr(char *key, size_t keylen, size_t *len)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U)) {
		*len = ((urlcontent *)U)->url_data_size;
		return ((urlcontent *)U)->url_data;
	}
	else {
		*len = 0;
		return ("");
	}
}

const char *XBSTR(char *key, size_t *len)
{
	void *U;

	if ((WC->urlstrings != NULL) &&
	    GetHash(WC->urlstrings, key, strlen (key), &U)){
		*len = ((urlcontent *)U)->url_data_size;
		return ((urlcontent *)U)->url_data;
	}
	else {
		*len = 0;
		return ("");
	}
}


const char *BSTR(char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) &&
	    GetHash(WC->urlstrings, key, strlen (key), &U))
		return ((urlcontent *)U)->url_data;
	else	
		return ("");
}

const char *Bstr(char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return ((urlcontent *)U)->url_data;
	else	
		return ("");
}

long LBstr(char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return atol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

long LBSTR(char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return atol(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBstr(char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return atoi(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int IBSTR(char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return atoi(((urlcontent *)U)->url_data);
	else	
		return (0);
}

int HaveBstr(char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return ((urlcontent *)U)->url_data_size != 0;
	else	
		return (0);
}

int HAVEBSTR(char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return ((urlcontent *)U)->url_data_size != 0;
	else	
		return (0);
}


int YesBstr(char *key, size_t keylen)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, keylen, &U))
		return strcmp( ((urlcontent *)U)->url_data, "yes") == 0;
	else	
		return (0);
}

int YESBSTR(char *key)
{
	void *U;

	if ((WC->urlstrings != NULL) && 
	    GetHash(WC->urlstrings, key, strlen(key), &U))
		return strcmp( ((urlcontent *)U)->url_data, "yes") == 0;
	else	
		return (0);
}

/*
 * web-printing funcion. uses our vsnprintf wrapper
 */
void wprintf(const char *format,...)
{
	va_list arg_ptr;
	char wbuf[4096];

	va_start(arg_ptr, format);
	vsnprintf(wbuf, sizeof wbuf, format, arg_ptr);
	va_end(arg_ptr);

	client_write(wbuf, strlen(wbuf));
}


/*
 * wrap up an HTTP session, closes tags, etc.
 *
 * print_standard_html_footer should be set to:
 * 0 to transmit only,
 * 1 to append the main menu and closing tags,
 * 2 to append the closing tags only.
 */
void wDumpContent(int print_standard_html_footer)
{
	if (print_standard_html_footer) {
		wprintf("</div>\n");	/* end of "text" div */
		do_template("trailing");
	}

	/* If we've been saving it all up for one big output burst,
	 * go ahead and do that now.
	 */
	end_burst();
}


/*
 * Copy a string, escaping characters which have meaning in HTML.  
 *
 * target		target buffer
 * strbuf		source buffer
 * nbsp			If nonzero, spaces are converted to non-breaking spaces.
 * nolinebreaks		if set, linebreaks are removed from the string.
 */
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks)
{
	char *aptr, *bptr, *eptr;

	*target = '\0';
	aptr = strbuf;
	bptr = target;
	eptr = target + tSize - 6; // our biggest unit to put in... 

	while ((bptr < eptr) && !IsEmptyStr(aptr) ){
		if (*aptr == '<') {
			memcpy(bptr, "&lt;", 4);
			bptr += 4;
		}
		else if (*aptr == '>') {
			memcpy(bptr, "&gt;", 4);
			bptr += 4;
		}
		else if (*aptr == '&') {
			memcpy(bptr, "&amp;", 5);
			bptr += 5;
		}
		else if (*aptr == '\"') {
			memcpy(bptr, "&quot;", 6);
			bptr += 6;
		}
		else if (*aptr == '\'') {
			memcpy(bptr, "&#39;", 5);
			bptr += 5;
		}
		else if (*aptr == LB) {
			*bptr = '<';
			bptr ++;
		}
		else if (*aptr == RB) {
			*bptr = '>';
			bptr ++;
		}
		else if (*aptr == QU) {
			*bptr ='"';
			bptr ++;
		}
		else if ((*aptr == 32) && (nbsp == 1)) {
			memcpy(bptr, "&nbsp;", 6);
			bptr += 6;
		}
		else if ((*aptr == '\n') && (nolinebreaks)) {
			*bptr='\0';	/* nothing */
		}
		else if ((*aptr == '\r') && (nolinebreaks)) {
			*bptr='\0';	/* nothing */
		}
		else{
			*bptr = *aptr;
			bptr++;
		}
		aptr ++;
	}
	*bptr = '\0';
	if ((bptr = eptr - 1 ) && !IsEmptyStr(aptr) )
		return -1;
	return (bptr - target);
}

void escputs1(char *strbuf, int nbsp, int nolinebreaks)
{
	char *buf;
	long Siz;

	if (strbuf == NULL) return;
	Siz = (3 * strlen(strbuf)) + SIZ ;
	buf = malloc(Siz);
	stresc(buf, Siz, strbuf, nbsp, nolinebreaks);
	wprintf("%s", buf);
	free(buf);
}

/* 
 * static wrapper for ecsputs1
 */
void escputs(char *strbuf)
{
	escputs1(strbuf, 0, 0);
}


/*
 * urlescape buffer and print it to the client
 */
void urlescputs(char *strbuf)
{
	char outbuf[SIZ];
	
	urlesc(outbuf, SIZ, strbuf);
	wprintf("%s", outbuf);
}


/*
 * Copy a string, escaping characters for JavaScript strings.
 */
void jsesc(char *target, size_t tlen, char *strbuf)
{
	int len;
	char *tend;
	char *send;
	char *tptr;
	char *sptr;

	target[0]='\0';
	len = strlen (strbuf);
	send = strbuf + len;
	tend = target + tlen;
	sptr = strbuf;
	tptr = target;
	
	while (!IsEmptyStr(sptr) && 
	       (sptr < send) &&
	       (tptr < tend)) {
	       
		if (*sptr == '<')
			*tptr = '[';
		else if (*sptr == '>')
			*tptr = ']';
		else if (*sptr == '\'') {
			if (tend - tptr < 3)
				return;
			*(tptr++) = '\\';
			*tptr = '\'';
		}
		else if (*sptr == '"') {
			if (tend - tptr < 8)
				return;
			*(tptr++) = '&';
			*(tptr++) = 'q';
			*(tptr++) = 'u';
			*(tptr++) = 'o';
			*(tptr++) = 't';
			*tptr = ';';
		}
		else if (*sptr == '&') {
			if (tend - tptr < 7)
				return;
			*(tptr++) = '&';
			*(tptr++) = 'a';
			*(tptr++) = 'm';
			*(tptr++) = 'p';
			*tptr = ';';
		} else {
			*tptr = *sptr;
		}
		tptr++; sptr++;
	}
	*tptr = '\0';
}

/*
 * escape and print javascript
 */
void jsescputs(char *strbuf)
{
	char outbuf[SIZ];
	
	jsesc(outbuf, SIZ, strbuf);
	wprintf("%s", outbuf);
}

/*
 * Copy a string, escaping characters for message text hold
 */
void msgesc(char *target, size_t tlen, char *strbuf)
{
	int len;
	char *tend;
	char *send;
	char *tptr;
	char *sptr;

	target[0]='\0';
	len = strlen (strbuf);
	send = strbuf + len;
	tend = target + tlen;
	sptr = strbuf;
	tptr = target;

	while (!IsEmptyStr(sptr) && 
	       (sptr < send) &&
	       (tptr < tend)) {
	       
		if (*sptr == '\n')
			*tptr = ' ';
		else if (*sptr == '\r')
			*tptr = ' ';
		else if (*sptr == '\'') {
			if (tend - tptr < 8)
				return;
			*(tptr++) = '&';
			*(tptr++) = '#';
			*(tptr++) = '3';
			*(tptr++) = '9';
			*tptr = ';';
		} else {
			*tptr = *sptr;
		}
		tptr++; sptr++;
	}
	*tptr = '\0';
}

/*
 * print a string to the client after cleaning it with msgesc() and stresc()
 */
void msgescputs1( char *strbuf)
{
	char *outbuf;
	char *outbuf2;
	int buflen;

	if (strbuf == NULL) return;
	buflen = 3 * strlen(strbuf) + SIZ;
	outbuf = malloc( buflen);
	outbuf2 = malloc( buflen);
	msgesc(outbuf, buflen, strbuf);
	stresc(outbuf2, buflen, outbuf, 0, 0);
	wprintf("%s", outbuf2);
	free(outbuf);
	free(outbuf2);
}

/*
 * print a string to the client after cleaning it with msgesc()
 */
void msgescputs(char *strbuf) {
	char *outbuf;
	size_t len;

	if (strbuf == NULL) return;
	len =  (3 * strlen(strbuf)) + SIZ;
	outbuf = malloc(len);
	msgesc(outbuf, len, strbuf);
	wprintf("%s", outbuf);
	free(outbuf);
}




/*
 * Output HTTP headers and leading HTML for a page
 */
void output_headers(	int do_httpheaders,	/* 1 = output HTTP headers                          */
			int do_htmlhead,	/* 1 = output HTML <head> section and <body> opener */

			int do_room_banner,	/* 0=no, 1=yes,                                     
						 * 2 = I'm going to embed my own, so don't open the 
						 *     <div id="content"> either.                   
						 */

			int unset_cookies,	/* 1 = session is terminating, so unset the cookies */
			int suppress_check,	/* 1 = suppress check for instant messages          */
			int cache		/* 1 = allow browser to cache this page             */
) {
	char cookie[1024];
	char httpnow[128];

	wprintf("HTTP/1.1 200 OK\n");
	http_datestring(httpnow, sizeof httpnow, time(NULL));

	if (do_httpheaders) {
		wprintf("Content-type: text/html; charset=utf-8\r\n"
			"Server: %s / %s\n"
			"Connection: close\r\n",
			PACKAGE_STRING, serv_info.serv_software
		);
	}

	if (cache) {
		char httpTomorow[128];

		http_datestring(httpTomorow, sizeof httpTomorow, 
				time(NULL) + 60 * 60 * 24 * 2);

		wprintf("Pragma: public\r\n"
			"Cache-Control: max-age=3600, must-revalidate\r\n"
			"Last-modified: %s\r\n"
			"Expires: %s\r\n",
			httpnow,
			httpTomorow
		);
	}
	else {
		wprintf("Pragma: no-cache\r\n"
			"Cache-Control: no-store\r\n"
			"Expires: -1\r\n"
		);
	}

	stuff_to_cookie(cookie, 1024, WC->wc_session, WC->wc_username,
			WC->wc_password, WC->wc_roomname);

	if (unset_cookies) {
		wprintf("Set-cookie: webcit=%s; path=/\r\n", unset);
	} else {
		wprintf("Set-cookie: webcit=%s; path=/\r\n", cookie);
		if (server_cookie != NULL) {
			wprintf("%s\n", server_cookie);
		}
	}

	if (do_htmlhead) {
		begin_burst();
		if (!access("static.local/webcit.css", R_OK)) {
			svprintf(HKEY("CSSLOCAL"), WCS_STRING,
			   "<link href=\"static.local/webcit.css\" rel=\"stylesheet\" type=\"text/css\">"
			);
		}
		do_template("head");
	}

	/* ICONBAR */
	if (do_htmlhead) {


		/* check for ImportantMessages (these display in a div overlaying the main screen) */
		if (!IsEmptyStr(WC->ImportantMessage)) {
			wprintf("<div id=\"important_message\">\n"
				"<span class=\"imsg\">");
			escputs(WC->ImportantMessage);
			wprintf("</span><br />\n"
				"</div>\n"
				"<script type=\"text/javascript\">\n"
				"	 setTimeout('hide_imsg_popup()', 5000);	\n"
				"</script>\n");
			WC->ImportantMessage[0] = 0;
		}

		if ( (WC->logged_in) && (!unset_cookies) ) {
			wprintf("<div id=\"iconbar\">");
			do_selected_iconbar();
			/** check for instant messages (these display in a new window) */
			page_popup();
			wprintf("</div>");
		}

		if (do_room_banner == 1) {
			wprintf("<div id=\"banner\">\n");
			embed_room_banner(NULL, navbar_default);
			wprintf("</div>\n");
		}
	}

	if (do_room_banner == 1) {
		wprintf("<div id=\"content\">\n");
	}
}


/*
 * Generic function to do an HTTP redirect.  Easy and fun.
 */
void http_redirect(const char *whichpage) {
	wprintf("HTTP/1.1 302 Moved Temporarily\n");
	wprintf("Location: %s\r\n", whichpage);
	wprintf("URI: %s\r\n", whichpage);
	wprintf("Content-type: text/html; charset=utf-8\r\n\r\n");
	wprintf("<html><body>");
	wprintf("Go <a href=\"%s\">here</A>.", whichpage);
	wprintf("</body></html>\n");
}



/*
 * Output a piece of content to the web browser using conformant HTTP and MIME semantics
 */
void http_transmit_thing(char *thing, size_t length, const char *content_type,
			 int is_static) {

	output_headers(0, 0, 0, 0, 0, is_static);

	wprintf("Content-type: %s\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		content_type,
		PACKAGE_STRING);

#ifdef HAVE_ZLIB
	/* If we can send the data out compressed, please do so. */
	if (WC->gzip_ok) {
		char *compressed_data = NULL;
		size_t compressed_len;

		compressed_len =  ((length * 101) / 100) + 100;
		compressed_data = malloc(compressed_len);

		if (compress_gzip((Bytef *) compressed_data,
				  &compressed_len,
				  (Bytef *) thing,
				  (uLongf) length, Z_BEST_SPEED) == Z_OK) {
			wprintf("Content-encoding: gzip\r\n"
				"Content-length: %ld\r\n"
				"\r\n",
				(long) compressed_len
			);
			client_write(compressed_data, (size_t)compressed_len);
			free(compressed_data);
			return;
		}
	}
#endif

	/* No compression ... just send it out as-is */
	wprintf("Content-length: %ld\r\n"
		"\r\n",
		(long) length
	);
	client_write(thing, (size_t)length);
}

/*
 * print menu box like used in the floor view or admin interface.
 * This function takes pair of strings as va_args, 
 * Title	Title string of the box
 * Class	CSS Class for the box
 * nLines	How many string pairs should we print? (URL, UrlText)
 * ...		Pairs of URL Strings and their Names
 */
void print_menu_box(char* Title, char *Class, int nLines, ...)
{
	va_list arg_list;
	long i;
	
	svput("BOXTITLE", WCS_STRING, Title);
	do_template("beginbox");
	
	wprintf("<ul class=\"%s\">", Class);
	
	va_start(arg_list, nLines);
	for (i = 0; i < nLines; ++i)
	{ 
		wprintf("<li><a href=\"%s\">", va_arg(arg_list, char *));
		wprintf((char *) va_arg(arg_list, char *));
		wprintf("</a></li>\n");
	}
	va_end (arg_list);
	
	wprintf("</a></li>\n");
	
	wprintf("</ul>");
	
	do_template("endbox");
}


/*
 * dump out static pages from disk
 */
void output_static(char *what)
{
	FILE *fp;
	struct stat statbuf;
	off_t bytes;
	off_t count = 0;
	size_t res;
	char *bigbuffer;
	const char *content_type;
	int len;

	fp = fopen(what, "rb");
	if (fp == NULL) {
		lprintf(9, "output_static('%s')  -- NOT FOUND --\n", what);
		wprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf("Cannot open %s: %s\r\n", what, strerror(errno));
	} else {
		len = strlen (what);
		content_type = GuessMimeByFilename(what, len);

		if (fstat(fileno(fp), &statbuf) == -1) {
			lprintf(9, "output_static('%s')  -- FSTAT FAILED --\n", what);
			wprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
			wprintf("Content-Type: text/plain\r\n");
			wprintf("\r\n");
			wprintf("Cannot fstat %s: %s\n", what, strerror(errno));
			return;
		}

		count = 0;
		bytes = statbuf.st_size;
		if ((bigbuffer = malloc(bytes + 2)) == NULL) {
			lprintf(9, "output_static('%s')  -- MALLOC FAILED (%s) --\n", what, strerror(errno));
			wprintf("HTTP/1.1 500 internal server error\r\n");
			wprintf("Content-Type: text/plain\r\n");
			wprintf("\r\n");
			return;
		}
		while (count < bytes) {
			if ((res = fread(bigbuffer + count, 1, bytes - count, fp)) == 0) {
				lprintf(9, "output_static('%s')  -- FREAD FAILED (%s) %zu bytes of %zu --\n", what, strerror(errno), bytes - count, bytes);
				wprintf("HTTP/1.1 500 internal server error \r\n");
				wprintf("Content-Type: text/plain\r\n");
				wprintf("\r\n");
				return;
			}
			count += res;
		}

		fclose(fp);

		lprintf(9, "output_static('%s')  %s\n", what, content_type);
		http_transmit_thing(bigbuffer, (size_t)bytes, content_type, 1);
		free(bigbuffer);
	}
	if (yesbstr("force_close_session")) {
		end_webcit_session();
	}
}

/*
 * When the browser requests an image file from the Citadel server,
 * this function is called to transmit it.
 */
void output_image()
{
	char buf[SIZ];
	char *xferbuf = NULL;
	off_t bytes;
	const char *MimeType;

	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		xferbuf = malloc(bytes + 2);

		/** Read it from the server */
		read_server_binary(xferbuf, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);

		MimeType = GuessMimeType (xferbuf, bytes);
		/** Write it to the browser */
		if (!IsEmptyStr(MimeType))
		{
			http_transmit_thing(xferbuf, 
					    (size_t)bytes, 
					    MimeType, 
					    0);
			free(xferbuf);
			return;
		}
		/* hm... unknown mimetype? fallback to blank gif */
		free(xferbuf);
	} 

	
	/*
	 * Instead of an ugly 404, send a 1x1 transparent GIF
	 * when there's no such image on the server.
	 */
	char blank_gif[SIZ];
	snprintf (blank_gif, SIZ, "%s%s", static_dirs[0], "/blank.gif");
	output_static(blank_gif);
}

/*
 * Extract an embedded photo from a vCard for display on the client
 */
void display_vcard_photo_img(char *msgnum_as_string)
{
	long msgnum = 0L;
	char *vcard;
	struct vCard *v;
	char *xferbuf;
    char *photosrc;
	int decoded;
	const char *contentType;

	msgnum = atol(msgnum_as_string);
	
	vcard = load_mimepart(msgnum,"1");
	v = vcard_load(vcard);
	
	photosrc = vcard_get_prop(v, "PHOTO", 1,0,0);
	xferbuf = malloc(strlen(photosrc));
	if (xferbuf == NULL) {
		lprintf(5, "xferbuf malloc failed\n");
		return;
	}
	memset(xferbuf, 1, SIZ);
	decoded = CtdlDecodeBase64(
		xferbuf,
		photosrc,
		strlen(photosrc));
	contentType = GuessMimeType(xferbuf, decoded);
	http_transmit_thing(xferbuf, decoded, contentType, 0);
	free(v);
	free(photosrc);
	free(xferbuf);
}

/*
 * Generic function to output an arbitrary MIME part from an arbitrary
 * message number on the server.
 *
 * msgnum		Number of the item on the citadel server
 * partnum		The MIME part to be output
 * force_download	Nonzero to force set the Content-Type: header to "application/octet-stream"
 */
void mimepart(char *msgnum, char *partnum, int force_download)
{
	char buf[256];
	off_t bytes;
	char content_type[256];
	char *content = NULL;
	
	serv_printf("OPNA %s|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		content = malloc(bytes + 2);
		if (force_download) {
			strcpy(content_type, "application/octet-stream");
		}
		else {
			extract_token(content_type, &buf[4], 3, '|', sizeof content_type);
		}
		output_headers(0, 0, 0, 0, 0, 0);
		read_server_binary(content, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(content, bytes, content_type, 0);
		free(content);
	} else {
		wprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf(_("An error occurred while retrieving this part: %s\n"), &buf[4]);
	}

}


/*
 * Read any MIME part of a message, from the server, into memory.
 */
char *load_mimepart(long msgnum, char *partnum)
{
	char buf[SIZ];
	off_t bytes;
	char content_type[SIZ];
	char *content;
	
	serv_printf("DLAT %ld|%s", msgnum, partnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '6') {
		bytes = extract_long(&buf[4], 0);
		extract_token(content_type, &buf[4], 3, '|', sizeof content_type);

		content = malloc(bytes + 2);
		serv_read(content, bytes);

		content[bytes] = 0;	/* null terminate for good measure */
		return(content);
	}
	else {
		return(NULL);
	}

}


/*
 * Convenience functions to display a page containing only a string
 *
 * titlebarcolor	color of the titlebar of the frame
 * titlebarmsg		text to display in the title bar
 * messagetext		body of the box
 */
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext)
{
	wprintf("HTTP/1.1 200 OK\n");
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<table width=100%% border=0 bgcolor=\"#%s\"><tr><td>", titlebarcolor);
	wprintf("<span class=\"titlebar\">%s</span>\n", titlebarmsg);
	wprintf("</td></tr></table>\n");
	wprintf("</div>\n<div id=\"content\">\n");
	escputs(messagetext);

	wprintf("<hr />\n");
	wDumpContent(1);
}


/*
 * Display a blank page.
 */
void blank_page(void) {
	output_headers(1, 1, 0, 0, 0, 0);
	wDumpContent(2);
}


/*
 * A template has been requested
 */
void url_do_template(void) {
	do_template(bstr("template"));
}



/*
 * Offer to make any page the user's "start page."
 */
void offer_start_page(void) {
	wprintf("<a href=\"change_start_page?startpage=");
	urlescputs(WC->this_page);
	wprintf("\">");
	wprintf(_("Make this my start page"));
	wprintf("</a>");
#ifdef TECH_PREVIEW
	wprintf("<br/><a href=\"rss?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\" title=\"RSS 2.0 feed for ");
	escputs(WC->wc_roomname);
	wprintf("\"><img alt=\"RSS\" border=\"0\" src=\"static/xml_button.gif\"/></a>\n");
#endif
}


/*
 * Change the user's start page
 */
void change_start_page(void) {

	if (bstr("startpage") == NULL) {
		safestrncpy(WC->ImportantMessage,
			_("You no longer have a start page selected."),
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	set_preference("startpage", NewStrBufPlain(bstr("startpage"), -1), 1);

	output_headers(1, 1, 0, 0, 0, 0);
	do_template("newstartpage");
	wDumpContent(1);
}



/*
 * convenience function to indicate success
 */
void display_success(char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}


/*
 * Authorization required page 
 * This is probably temporary and should be revisited 
 */
void authorization_required(const char *message)
{
	wprintf("HTTP/1.1 401 Authorization Required\r\n");
	wprintf("WWW-Authenticate: Basic realm=\"%s\"\r\n", serv_info.serv_humannode);
	wprintf("Content-Type: text/html\r\n\r\n");
	wprintf("<h1>");
	wprintf(_("Authorization Required"));
	wprintf("</h1>\r\n");
	wprintf(_("The resource you requested requires a valid username and password. "
		"You could not be logged in: %s\n"), message);
	wDumpContent(0);
}

/*
 * This function is called by the MIME parser to handle data uploaded by
 * the browser.  Form data, uploaded files, and the data from HTTP PUT
 * operations (such as those found in GroupDAV) all arrive this way.
 *
 * name		Name of the item being uploaded
 * filename	Filename of the item being uploaded
 * partnum	MIME part identifier (not needed)
 * disp		MIME content disposition (not needed)
 * content	The actual data
 * cbtype	MIME content-type
 * cbcharset	Character set
 * length	Content length
 * encoding	MIME encoding type (not needed)
 * userdata	Not used here
 */
void upload_handler(char *name, char *filename, char *partnum, char *disp,
			void *content, char *cbtype, char *cbcharset,
			size_t length, char *encoding, void *userdata)
{
	urlcontent *u;
#ifdef DEBUG_URLSTRINGS
	lprintf(9, "upload_handler() name=%s, type=%s, len=%d\n", name, cbtype, length);
#endif
	if (WC->urlstrings == NULL)
		WC->urlstrings = NewHash(1, NULL);

	/* Form fields */
	if ( (length > 0) && (IsEmptyStr(cbtype)) ) {
		u = (urlcontent *) malloc(sizeof(urlcontent));
		
		safestrncpy(u->url_key, name, sizeof(u->url_key));
		u->url_data = malloc(length + 1);
		u->url_data_size = length;
		memcpy(u->url_data, content, length);
		u->url_data[length] = 0;
		Put(WC->urlstrings, u->url_key, strlen(u->url_key), u, free_url);
#ifdef DEBUG_URLSTRINGS
		lprintf(9, "Key: <%s> len: [%ld] Data: <%s>\n", u->url_key, u->url_data_size, u->url_data);
#endif
	}

	/** Uploaded files */
	if ( (length > 0) && (!IsEmptyStr(cbtype)) ) {
		WC->upload = malloc(length);
		if (WC->upload != NULL) {
			WC->upload_length = length;
			safestrncpy(WC->upload_filename, filename,
					sizeof(WC->upload_filename));
			safestrncpy(WC->upload_content_type, cbtype,
					sizeof(WC->upload_content_type));
			memcpy(WC->upload, content, length);
		}
		else {
			lprintf(3, "malloc() failed: %s\n", strerror(errno));
		}
	}

}

/*
 * Convenience functions to wrap around asynchronous ajax responses
 */
void begin_ajax_response(void) {
        output_headers(0, 0, 0, 0, 0, 0);

        wprintf("Content-type: text/html; charset=UTF-8\r\n"
                "Server: %s\r\n"
                "Connection: close\r\n"
                "Pragma: no-cache\r\n"
                "Cache-Control: no-cache\r\n"
		"Expires: -1\r\n"
		,
                PACKAGE_STRING);
        begin_burst();
}

/*
 * print ajax response footer 
 */
void end_ajax_response(void) {
        wprintf("\r\n");
        wDumpContent(0);
}

/*
 * Wraps a Citadel server command in an AJAX transaction.
 */
void ajax_servcmd(void)
{
	char buf[1024];
	char gcontent[1024];
	char *junk;
	size_t len;

	begin_ajax_response();

	serv_printf("%s", bstr("g_cmd"));
	serv_getln(buf, sizeof buf);
	wprintf("%s\n", buf);

	if (buf[0] == '8') {
		serv_printf("\n\n000");
	}
	if ((buf[0] == '1') || (buf[0] == '8')) {
		while (serv_getln(gcontent, sizeof gcontent), strcmp(gcontent, "000")) {
			wprintf("%s\n", gcontent);
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"));
		serv_puts("000");
	}
	if (buf[0] == '6') {
		len = atol(&buf[4]);
		junk = malloc(len);
		serv_read(junk, len);
		free(junk);
	}
	if (buf[0] == '7') {
		len = atol(&buf[4]);
		junk = malloc(len);
		memset(junk, 0, len);
		serv_write(junk, len);
		free(junk);
	}

	end_ajax_response();
	
	/*
	 * This is kind of an ugly hack, but this is the only place it can go.
	 * If the command was GEXP, then the instant messenger window must be
	 * running, so reset the "last_pager_check" watchdog timer so
	 * that page_popup() doesn't try to open it a second time.
	 */
	if (!strncasecmp(bstr("g_cmd"), "GEXP", 4)) {
		WC->last_pager_check = time(NULL);
	}
}


/*
 * Helper function for the asynchronous check to see if we need
 * to open the instant messenger window.
 */
void seconds_since_last_gexp(void)
{
	char buf[256];

	begin_ajax_response();
	if ( (time(NULL) - WC->last_pager_check) < 30) {
		wprintf("NO\n");
	}
	else {
		serv_puts("NOOP");
		serv_getln(buf, sizeof buf);
		if (buf[3] == '*') {
			wprintf("YES");
		}
		else {
			wprintf("NO");
		}
	}
	end_ajax_response();
}

/**
 * \brief Detects a 'mobile' user agent 
 */
int is_mobile_ua(char *user_agent) {
	if (strstr(user_agent,"iPhone OS") != NULL) {
		return 1;
	} else if (strstr(user_agent,"Windows CE") != NULL) {
		return 1;
	} else if (strstr(user_agent,"SymbianOS") != NULL) {
		return 1;
	}
	return 0;
}


/*
 * Entry point for WebCit transaction
 */
void session_loop(struct httprequest *req)
{
	char cmd[1024];
	char action[1024];
	char arg[8][128];
	size_t sizes[10];
	char *index[10];
	char buf[SIZ];
	char request_method[128];
	char pathname[1024];
	int a, b, nBackDots, nEmpty;
	int ContentLength = 0;
	char ContentType[512];
	char *content = NULL;
	char *content_end = NULL;
	struct httprequest *hptr;
	char browser_host[256];
	char user_agent[256];
	int body_start = 0;
	int is_static = 0;
	int n_static = 0;
	int len = 0;
	/*
	 * We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_username[SIZ];
	char c_password[SIZ];
	char c_roomname[SIZ];
	char c_httpauth_string[SIZ];
	char c_httpauth_user[SIZ];
	char c_httpauth_pass[SIZ];
	char cookie[SIZ];

	safestrncpy(c_username, "", sizeof c_username);
	safestrncpy(c_password, "", sizeof c_password);
	safestrncpy(c_roomname, "", sizeof c_roomname);
	safestrncpy(c_httpauth_string, "", sizeof c_httpauth_string);
	safestrncpy(c_httpauth_user, DEFAULT_HTTPAUTH_USER, sizeof c_httpauth_user);
	safestrncpy(c_httpauth_pass, DEFAULT_HTTPAUTH_PASS, sizeof c_httpauth_pass);
	strcpy(browser_host, "");

	WC->upload_length = 0;
	WC->upload = NULL;
	WC->is_mobile = 0;

	hptr = req;
	if (hptr == NULL) return;

	safestrncpy(cmd, hptr->line, sizeof cmd);
	hptr = hptr->next;
	extract_token(request_method, cmd, 0, ' ', sizeof request_method);
	extract_token(pathname, cmd, 1, ' ', sizeof pathname);

	/** Figure out the action */
	index[0] = action;
	sizes[0] = sizeof action;
	for (a=1; a<9; a++)
	{
		index[a] = arg[a-1];
		sizes[a] = sizeof arg[a-1];
	}
////	index[9] = &foo; todo
	nBackDots = 0;
	nEmpty = 0;
	for ( a = 0; a < 9; ++a)
	{
		extract_token(index[a], pathname, a + 1, '/', sizes[a]);
		if (strstr(index[a], "?")) *strstr(index[a], "?") = 0;
		if (strstr(index[a], "&")) *strstr(index[a], "&") = 0;
		if (strstr(index[a], " ")) *strstr(index[a], " ") = 0;
		if ((index[a][0] == '.') && (index[a][1] == '.'))
			nBackDots++;
		if (index[a][0] == '\0')
			nEmpty++;
	}

	while (hptr != NULL) {
		safestrncpy(buf, hptr->line, sizeof buf);
		/* lprintf(9, "HTTP HEADER: %s\n", buf); */
		hptr = hptr->next;

		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			safestrncpy(cookie, &buf[15], sizeof cookie);
			cookie_to_stuff(cookie, NULL,
					c_username, sizeof c_username,
					c_password, sizeof c_password,
					c_roomname, sizeof c_roomname);
		}
		else if (!strncasecmp(buf, "Authorization: Basic ", 21)) {
			CtdlDecodeBase64(c_httpauth_string, &buf[21], strlen(&buf[21]));
			extract_token(c_httpauth_user, c_httpauth_string, 0, ':', sizeof c_httpauth_user);
			extract_token(c_httpauth_pass, c_httpauth_string, 1, ':', sizeof c_httpauth_pass);
		}
		else if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
		}
		else if (!strncasecmp(buf, "Content-type: ", 14)) {
			safestrncpy(ContentType, &buf[14], sizeof ContentType);
		}
		else if (!strncasecmp(buf, "User-agent: ", 12)) {
			safestrncpy(user_agent, &buf[12], sizeof user_agent);
			if (is_mobile_ua(&buf[12])) {
				WC->is_mobile = 1;
			}
		}
		else if (!strncasecmp(buf, "X-Forwarded-Host: ", 18)) {
			if (follow_xff) {
				safestrncpy(WC->http_host, &buf[18], sizeof WC->http_host);
			}
		}
		else if (!strncasecmp(buf, "Host: ", 6)) {
			if (IsEmptyStr(WC->http_host)) {
				safestrncpy(WC->http_host, &buf[6], sizeof WC->http_host);
			}
		}
		else if (!strncasecmp(buf, "X-Forwarded-For: ", 17)) {
			safestrncpy(browser_host, &buf[17], sizeof browser_host);
			while (num_tokens(browser_host, ',') > 1) {
				remove_token(browser_host, 0, ',');
			}
			striplt(browser_host);
		}
	}

	if (ContentLength > 0) {
		int BuffSize;

		BuffSize = ContentLength + SIZ;
		content = malloc(BuffSize);
		memset(content, 0, BuffSize);
		snprintf(content,  BuffSize, "Content-type: %s\n"
				"Content-length: %d\n\n",
				ContentType, ContentLength);
		body_start = strlen(content);

		/** Read the entire input data at once. */
		client_read(WC->http_sock, &content[body_start], ContentLength);

		if (!strncasecmp(ContentType, "application/x-www-form-urlencoded", 33)) {
			addurls(&content[body_start], ContentLength);
		} else if (!strncasecmp(ContentType, "multipart", 9)) {
			content_end = content + ContentLength + body_start;
			mime_parser(content, content_end, *upload_handler, NULL, NULL, NULL, 0);
		}
	} else {
		content = NULL;
	}

	/* make a note of where we are in case the user wants to save it */
	safestrncpy(WC->this_page, cmd, sizeof(WC->this_page));
	remove_token(WC->this_page, 2, ' ');
	remove_token(WC->this_page, 0, ' ');

	/* If there are variables in the URL, we must grab them now */
	len = strlen(cmd);
	for (a = 0; a < len; ++a) {
		if ((cmd[a] == '?') || (cmd[a] == '&')) {
			for (b = a; b < len; ++b) {
				if (isspace(cmd[b])){
					cmd[b] = 0;
					len = b - 1;
				}
			}
			addurls(&cmd[a + 1], len - a);
			cmd[a] = 0;
			len = a - 1;
		}
	}

	/* If it's a "force 404" situation then display the error and bail. */
	if (!strcmp(action, "404")) {
		wprintf("HTTP/1.1 404 Not found\r\n");
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf("Not found\r\n");
		goto SKIP_ALL_THIS_CRAP;
	}

	/* Static content can be sent without connecting to Citadel. */
	is_static = 0;
	for (a=0; a<ndirs; ++a) {
		if (!strcasecmp(action, (char*)static_content_dirs[a])) { /* map web to disk location */
			is_static = 1;
			n_static = a;
		}
	}
	if (is_static) {
		if (nBackDots < 2)
		{
			snprintf(buf, sizeof buf, "%s/%s/%s/%s/%s/%s/%s/%s",
				 static_dirs[n_static], 
				 index[1], index[2], index[3], index[4], index[5], index[6], index[7]);
			for (a=0; a<8; ++a) {
				if (buf[strlen(buf)-1] == '/') {
					buf[strlen(buf)-1] = 0;
				}
			}
			for (a = 0; a < strlen(buf); ++a) {
				if (isspace(buf[a])) {
					buf[a] = 0;
				}
			}
			output_static(buf);
		}
		else 
		{
			lprintf(9, "Suspicious request. Ignoring.");
			wprintf("HTTP/1.1 404 Security check failed\r\n");
			wprintf("Content-Type: text/plain\r\n");
			wprintf("\r\n");
			wprintf("You have sent a malformed or invalid request.\r\n");
		}
		goto SKIP_ALL_THIS_CRAP;	/* Don't try to connect */
	}

	/* If the client sent a nonce that is incorrect, kill the request. */
	if (strlen(bstr("nonce")) > 0) {
		lprintf(9, "Comparing supplied nonce %s to session nonce %ld\n", 
			bstr("nonce"), WC->nonce);
		if (ibstr("nonce") != WC->nonce) {
			lprintf(9, "Ignoring request with mismatched nonce.\n");
			wprintf("HTTP/1.1 404 Security check failed\r\n");
			wprintf("Content-Type: text/plain\r\n");
			wprintf("\r\n");
			wprintf("Security check failed.\r\n");
			goto SKIP_ALL_THIS_CRAP;
		}
	}

	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.
	 */
	if (!WC->connected) {
		if (!strcasecmp(ctdlhost, "uds")) {
			/* unix domain socket */
			snprintf(buf, SIZ, "%s/citadel.socket", ctdlport);
			WC->serv_sock = uds_connectsock(buf);
		}
		else {
			/* tcp socket */
			WC->serv_sock = tcp_connectsock(ctdlhost, ctdlport);
		}

		if (WC->serv_sock < 0) {
			do_logout();
			goto SKIP_ALL_THIS_CRAP;
		}
		else {
			WC->connected = 1;
			serv_getln(buf, sizeof buf);	/** get the server welcome message */

			/**
			 * From what host is our user connecting?  Go with
			 * the host at the other end of the HTTP socket,
			 * unless we are following X-Forwarded-For: headers
			 * and such a header has already turned up something.
			 */
			if ( (!follow_xff) || (strlen(browser_host) == 0) ) {
				locate_host(browser_host, WC->http_sock);
			}

			get_serv_info(browser_host, user_agent);
			if (serv_info.serv_rev_level < MINIMUM_CIT_VERSION) {
				wprintf(_("You are connected to a Citadel "
					"server running Citadel %d.%02d. \n"
					"In order to run this version of WebCit "
					"you must also have Citadel %d.%02d or"
					" newer.\n\n\n"),
						serv_info.serv_rev_level / 100,
						serv_info.serv_rev_level % 100,
						MINIMUM_CIT_VERSION / 100,
						MINIMUM_CIT_VERSION % 100
					);
				end_webcit_session();
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}

	/*
	 * Functions which can be performed without logging in
	 */
	if (!strcasecmp(action, "listsub")) {
		do_listsub();
		goto SKIP_ALL_THIS_CRAP;
	}
	if (!strcasecmp(action, "freebusy")) {
		do_freebusy(cmd);
		goto SKIP_ALL_THIS_CRAP;
	}

	/*
	 * If we're not logged in, but we have HTTP Authentication data,
	 * try logging in to Citadel using that.
	 */
	if ((!WC->logged_in)
	   && (strlen(c_httpauth_user) > 0)
	   && (strlen(c_httpauth_pass) > 0)) {
		serv_printf("USER %s", c_httpauth_user);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_httpauth_pass);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				become_logged_in(c_httpauth_user,
						c_httpauth_pass, buf);
				safestrncpy(WC->httpauth_user, c_httpauth_user, sizeof WC->httpauth_user);
				safestrncpy(WC->httpauth_pass, c_httpauth_pass, sizeof WC->httpauth_pass);
			} else {
				/* Should only display when password is wrong */
				authorization_required(&buf[4]);
				goto SKIP_ALL_THIS_CRAP;
			}
		}
	}

	/* This needs to run early */
#ifdef TECH_PREVIEW
	if (!strcasecmp(action, "rss")) {
		display_rss(bstr("room"), request_method);
		goto SKIP_ALL_THIS_CRAP;
	}
#endif

	/* 
	 * The GroupDAV stuff relies on HTTP authentication instead of
	 * our session's authentication.
	 */
	if (!strncasecmp(action, "groupdav", 8)) {
		groupdav_main(req, ContentType, /* do GroupDAV methods */
			ContentLength, content+body_start);
		if (!WC->logged_in) {
			WC->killthis = 1;	/* If not logged in, don't */
		}				/* keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}


	/*
	 * Automatically send requests with any method other than GET or
	 * POST to the GroupDAV code as well.
	 */
	if ((strcasecmp(request_method, "GET")) && (strcasecmp(request_method, "POST"))) {
		groupdav_main(req, ContentType, /** do GroupDAV methods */
			ContentLength, content+body_start);
		if (!WC->logged_in) {
			WC->killthis = 1;	/** If not logged in, don't */
		}				/** keep the session active */
		goto SKIP_ALL_THIS_CRAP;
	}

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!WC->logged_in)
	   && (!IsEmptyStr(c_username))
	   && (!IsEmptyStr(c_password))) {
		serv_printf("USER %s", c_username);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_password);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				become_logged_in(c_username, c_password, buf);
			}
		}
	}
	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((IsEmptyStr(WC->wc_roomname)) && (!IsEmptyStr(c_roomname))) {
		serv_printf("GOTO %s", c_roomname);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			safestrncpy(WC->wc_roomname, c_roomname, sizeof WC->wc_roomname);
		}
	}

	if (!strcasecmp(action, "image")) {
		output_image();
	} else if (!strcasecmp(action, "display_mime_icon")) {
		display_mime_icon();

	/*
	 * All functions handled below this point ... make sure we log in
	 * before doing anything else!
	 */
	} else if ((!WC->logged_in) && (!strcasecmp(action, "login"))) {
		do_login();
	} else if ((!WC->logged_in) && (!strcasecmp(action, "display_openid_login"))) {
		display_openid_login(NULL);
	} else if ((!WC->logged_in) && (!strcasecmp(action, "openid_login"))) {
		do_openid_login();
	} else if (!strcasecmp(action, "finalize_openid_login")) {
		finalize_openid_login();
	} else if (!strcasecmp(action, "openid_manual_create")) {
		openid_manual_create();
	} else if (!WC->logged_in) {
		display_login(NULL);
	}

	/*
	 * Various commands...
	 */

	else {
		void *vHandler;
		WebcitHandler *Handler;

		GetHash(HandlerHash, action, strlen(action) /* TODO*/, &vHandler),
			Handler = (WebcitHandler*) vHandler;
		if (Handler != NULL) {
			if (Handler->IsAjax)
				begin_ajax_response();
			Handler->F();
			if (Handler->IsAjax)
				end_ajax_response();
		}
		

	else if (!strcasecmp(action, "do_welcome")) {
		do_welcome();
	} else if (!strcasecmp(action, "blank")) {
		blank_page();
	} else if (!strcasecmp(action, "do_template")) {
		url_do_template();
	} else if (!strcasecmp(action, "display_aide_menu")) {
		display_aide_menu();
	} else if (!strcasecmp(action, "server_shutdown")) {
		display_shutdown();
	} else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
	} else if (!strcasecmp(action, "who")) {
		who();
	} else if (!strcasecmp(action, "sslg")) {
		seconds_since_last_gexp();
	} else if (!strcasecmp(action, "who_inner_html")) {
		begin_ajax_response();
		who_inner_div();
		end_ajax_response();
	} else if (!strcasecmp(action, "wholist_section")) {
		begin_ajax_response();
		wholist_section();
		end_ajax_response();
	} else if (!strcasecmp(action, "new_messages_html")) {
		begin_ajax_response();
		new_messages_section();
		end_ajax_response();
	} else if (!strcasecmp(action, "tasks_inner_html")) {
		begin_ajax_response();
		tasks_section();
		end_ajax_response();
	} else if (!strcasecmp(action, "calendar_inner_html")) {
		begin_ajax_response();
		calendar_section();
		end_ajax_response();
	} else if (!strcasecmp(action, "mini_calendar")) {
		begin_ajax_response();
		ajax_mini_calendar();
		end_ajax_response();
	} else if (!strcasecmp(action, "iconbar_ajax_menu")) {
		begin_ajax_response();
		do_iconbar();
		end_ajax_response();
	} else if (!strcasecmp(action, "iconbar_ajax_rooms")) {
		begin_ajax_response();
		do_iconbar_roomlist();
		end_ajax_response();
	} else if (!strcasecmp(action, "knrooms")) {
		knrooms();
	} else if (!strcasecmp(action, "gotonext")) {
		slrp_highest();
		gotonext();
	} else if (!strcasecmp(action, "skip")) {
		gotonext();
	} else if (!strcasecmp(action, "ungoto")) {
		ungoto();
	} else if (!strcasecmp(action, "dotgoto")) {
		if (WC->wc_view != VIEW_MAILBOX) {	/* dotgoto acts like dotskip when we're in a mailbox view */
			slrp_highest();
		}
		smart_goto(bstr("room"));
	} else if (!strcasecmp(action, "dotskip")) {
		smart_goto(bstr("room"));
	} else if (!strcasecmp(action, "termquit")) {
		do_logout();
	} else if (!strcasecmp(action, "readnew")) {
		readloop("readnew");
	} else if (!strcasecmp(action, "readold")) {
		readloop("readold");
	} else if (!strcasecmp(action, "readfwd")) {
		readloop("readfwd");
	} else if (!strcasecmp(action, "headers")) {
		readloop("headers");
	} else if (!strcasecmp(action, "do_search")) {
		readloop("do_search");
	} else if (!strcasecmp(action, "msg")) {
		embed_message(index[1]);
	} else if (!strcasecmp(action, "printmsg")) {
		print_message(index[1]);
	} else if (!strcasecmp(action, "msgheaders")) {
		display_headers(index[1]);
	} else if (!strcasecmp(action, "vcardphoto")) {
		display_vcard_photo_img(index[1]);	
	} else if (!strcasecmp(action, "wiki")) {
		display_wiki_page();
	} else if (!strcasecmp(action, "display_enter")) {
		display_enter();
	} else if (!strcasecmp(action, "post")) {
		post_message();
	} else if (!strcasecmp(action, "move_msg")) {
		move_msg();
	} else if (!strcasecmp(action, "delete_msg")) {
		delete_msg();
	} else if (!strcasecmp(action, "userlist")) {
		userlist();
	} else if (!strcasecmp(action, "showuser")) {
		showuser();
	} else if (!strcasecmp(action, "display_page")) {
		display_page();
	} else if (!strcasecmp(action, "page_user")) {
		page_user();
	} else if (!strcasecmp(action, "chat")) {
		do_chat();
	} else if (!strcasecmp(action, "display_private")) {
		display_private("", 0);
	} else if (!strcasecmp(action, "goto_private")) {
		goto_private();
	} else if (!strcasecmp(action, "zapped_list")) {
		zapped_list();
	} else if (!strcasecmp(action, "display_zap")) {
		display_zap();
	} else if (!strcasecmp(action, "zap")) {
		zap();
	} else if (!strcasecmp(action, "display_entroom")) {
		display_entroom();
	} else if (!strcasecmp(action, "entroom")) {
		entroom();
	} else if (!strcasecmp(action, "display_whok")) {
		display_whok();
	} else if (!strcasecmp(action, "do_invt_kick")) {
		do_invt_kick();
	} else if (!strcasecmp(action, "display_editroom")) {
		display_editroom();
	} else if (!strcasecmp(action, "netedit")) {
		netedit();
	} else if (!strcasecmp(action, "editroom")) {
		editroom();
	} else if (!strcasecmp(action, "display_editinfo")) {
		display_edit(_("Room info"), "EINF 0", "RINF", "editinfo", 1);
	} else if (!strcasecmp(action, "editinfo")) {
		save_edit(_("Room info"), "EINF 1", 1);
	} else if (!strcasecmp(action, "display_editbio")) {
		snprintf(buf, SIZ, "RBIO %s", WC->wc_fullname);
		display_edit(_("Your bio"), "NOOP", buf, "editbio", 3);
	} else if (!strcasecmp(action, "editbio")) {
		save_edit(_("Your bio"), "EBIO", 0);
	} else if (!strcasecmp(action, "confirm_move_msg")) {
		confirm_move_msg();
	} else if (!strcasecmp(action, "delete_room")) {
		delete_room();
	} else if (!strcasecmp(action, "validate")) {
		validate();
		/* The users photo display / upload facility */
	} else if (!strcasecmp(action, "display_editpic")) {
		display_graphics_upload(_("your photo"),
					"_userpic_",
					"editpic");
	} else if (!strcasecmp(action, "editpic")) {
		do_graphics_upload("_userpic_");
                /* room picture dispay / upload facility */
	} else if (!strcasecmp(action, "display_editroompic")) {
		display_graphics_upload(_("the icon for this room"),
					"_roompic_",
					"editroompic");
	} else if (!strcasecmp(action, "editroompic")) {
		do_graphics_upload("_roompic_");
		/* the greetingpage hello pic */
	} else if (!strcasecmp(action, "display_edithello")) {
		display_graphics_upload(_("the Greetingpicture for the login prompt"),
					"hello",
					"edithellopic");
	} else if (!strcasecmp(action, "edithellopic")) {
		do_graphics_upload("hello");
		/* the logoff banner */
	} else if (!strcasecmp(action, "display_editgoodbyepic")) {
		display_graphics_upload(_("the Logoff banner picture"),
					"UIMG 0|%s|goodbuye",
					"editgoodbuyepic");
	} else if (!strcasecmp(action, "editgoodbuyepic")) {
		do_graphics_upload("UIMG 1|%s|goodbuye");

	} else if (!strcasecmp(action, "delete_floor")) {
		delete_floor();
	} else if (!strcasecmp(action, "rename_floor")) {
		rename_floor();
	} else if (!strcasecmp(action, "create_floor")) {
		create_floor();
	} else if (!strcasecmp(action, "display_editfloorpic")) {
		snprintf(buf, SIZ, "UIMG 0|_floorpic_|%s",
			bstr("which_floor"));
		display_graphics_upload(_("the icon for this floor"),
					buf,
					"editfloorpic");
	} else if (!strcasecmp(action, "editfloorpic")) {
		snprintf(buf, SIZ, "UIMG 1|_floorpic_|%s",
			bstr("which_floor"));
		do_graphics_upload(buf);
	} else if (!strcasecmp(action, "display_reg")) {
		display_reg(0);
	} else if (!strcasecmp(action, "display_changepw")) {
		display_changepw();
	} else if (!strcasecmp(action, "changepw")) {
		changepw();
	} else if (!strcasecmp(action, "display_edit_node")) {
		display_edit_node();
	} else if (!strcasecmp(action, "edit_node")) {
		edit_node();
	} else if (!strcasecmp(action, "display_netconf")) {
		display_netconf();
	} else if (!strcasecmp(action, "display_confirm_delete_node")) {
		display_confirm_delete_node();
	} else if (!strcasecmp(action, "delete_node")) {
		delete_node();
	} else if (!strcasecmp(action, "display_add_node")) {
		display_add_node();
	} else if (!strcasecmp(action, "terminate_session")) {
		slrp_highest();
		terminate_session();
	} else if (!strcasecmp(action, "edit_me")) {
		edit_me();
	} else if (!strcasecmp(action, "display_siteconfig")) {
		display_siteconfig();
	} else if (!strcasecmp(action, "chat_recv")) {
		chat_recv();
	} else if (!strcasecmp(action, "chat_send")) {
		chat_send();
	} else if (!strcasecmp(action, "siteconfig")) {
		siteconfig();
	} else if (!strcasecmp(action, "display_generic")) {
		display_generic();
	} else if (!strcasecmp(action, "do_generic")) {
		do_generic();
	} else if (!strcasecmp(action, "ajax_servcmd")) {
		ajax_servcmd();
	} else if (!strcasecmp(action, "display_menubar")) {
		display_menubar(1);
	} else if (!strcasecmp(action, "mimepart")) {
		mimepart(index[1], index[2], 0);
	} else if (!strcasecmp(action, "mimepart_download")) {
		mimepart(index[1], index[2], 1);
	} else if (!strcasecmp(action, "edit_vcard")) {
		edit_vcard();
	} else if (!strcasecmp(action, "submit_vcard")) {
		submit_vcard();
	} else if (!strcasecmp(action, "select_user_to_edit")) {
		select_user_to_edit(NULL, NULL);
	} else if (!strcasecmp(action, "display_edituser")) {
		display_edituser(NULL, 0);
	} else if (!strcasecmp(action, "edituser")) {
		edituser();
	} else if (!strcasecmp(action, "create_user")) {
		create_user();
	} else if (!strcasecmp(action, "changeview")) {
		change_view();
	} else if (!strcasecmp(action, "change_start_page")) {
		change_start_page();
	} else if (!strcasecmp(action, "display_floorconfig")) {
		display_floorconfig(NULL);
	} else if (!strcasecmp(action, "toggle_self_service")) {
		toggle_self_service();
	} else if (!strcasecmp(action, "display_edit_task")) {
		display_edit_task();
	} else if (!strcasecmp(action, "save_task")) {
		save_task();
	} else if (!strcasecmp(action, "display_edit_event")) {
		display_edit_event();
	} else if (!strcasecmp(action, "save_event")) {
		save_event();
	} else if (!strcasecmp(action, "respond_to_request")) {
		respond_to_request();
	} else if (!strcasecmp(action, "handle_rsvp")) {
		handle_rsvp();
	} else if (!strcasecmp(action, "summary")) {
		summary();
	} else if (!strcasecmp(action, "summary_inner_div")) {
		begin_ajax_response();
		summary_inner_div();
		end_ajax_response();
	} else if (!strcasecmp(action, "display_customize_iconbar")) {
		display_customize_iconbar();
	} else if (!strcasecmp(action, "commit_iconbar")) {
		commit_iconbar();
	} else if (!strcasecmp(action, "set_room_policy")) {
		set_room_policy();
	} else if (!strcasecmp(action, "display_inetconf")) {
		display_inetconf();
	} else if (!strcasecmp(action, "save_inetconf")) {
		save_inetconf();
	} else if (!strcasecmp(action, "display_smtpqueue")) {
		display_smtpqueue();
	} else if (!strcasecmp(action, "display_smtpqueue_inner_div")) {
		display_smtpqueue_inner_div();
	} else if (!strcasecmp(action, "display_sieve")) {
		display_sieve();
	} else if (!strcasecmp(action, "save_sieve")) {
		save_sieve();
	} else if (!strcasecmp(action, "display_pushemail")) {
		display_pushemail();
	} else if (!strcasecmp(action, "save_pushemail")) {
		save_pushemail();
	} else if (!strcasecmp(action, "display_add_remove_scripts")) {
		display_add_remove_scripts(NULL);
	} else if (!strcasecmp(action, "create_script")) {
		create_script();
	} else if (!strcasecmp(action, "delete_script")) {
		delete_script();
	} else if (!strcasecmp(action, "setup_wizard")) {
		do_setup_wizard();
	} else if (!strcasecmp(action, "display_preferences")) {
		display_preferences();
	} else if (!strcasecmp(action, "set_preferences")) {
		set_preferences();
	} else if (!strcasecmp(action, "recp_autocomplete")) {
		recp_autocomplete(bstr("recp"));
	} else if (!strcasecmp(action, "cc_autocomplete")) {
		recp_autocomplete(bstr("cc"));
	} else if (!strcasecmp(action, "bcc_autocomplete")) {
		recp_autocomplete(bstr("bcc"));
	} else if (!strcasecmp(action, "display_address_book_middle_div")) {
		display_address_book_middle_div();
	} else if (!strcasecmp(action, "display_address_book_inner_div")) {
		display_address_book_inner_div();
	} else if (!strcasecmp(action, "set_floordiv_expanded")) {
		set_floordiv_expanded(index[1]);
	} else if (!strcasecmp(action, "diagnostics")) {
		output_headers(1, 1, 1, 0, 0, 0);
		wprintf("Session: %d<hr />\n", WC->wc_session);
		wprintf("Command: <br /><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><hr />\n");
		wprintf("Variables: <br /><PRE>\n");
		dump_vars();
		wprintf("</PRE><hr />\n");
		wDumpContent(1);
	} else if (!strcasecmp(action, "add_new_note")) {
		add_new_note();
	} else if (!strcasecmp(action, "ajax_update_note")) {
		ajax_update_note();
	} else if (!strcasecmp(action, "display_room_directory")) {
		display_room_directory();
	} else if (!strcasecmp(action, "display_pictureview")) {
		display_pictureview();
	} else if (!strcasecmp(action, "download_file")) {
		download_file(index[1]);
	} else if (!strcasecmp(action, "upload_file")) {
		upload_file();
	} else if (!strcasecmp(action, "display_openids")) {
		display_openids();
	} else if (!strcasecmp(action, "openid_attach")) {
		openid_attach();
	} else if (!strcasecmp(action, "openid_detach")) {
		openid_detach();
	}

	/* When all else fais, display the main menu. */
	else {
		display_main_menu();
	}
}
SKIP_ALL_THIS_CRAP:
	fflush(stdout);
	if (content != NULL) {
		free(content);
		content = NULL;
	}
	free_urls();
	if (WC->upload_length > 0) {
		free(WC->upload);
		WC->upload_length = 0;
	}
}


/*
 * Replacement for sleep() that uses select() in order to avoid SIGALRM
 */
void sleeeeeeeeeep(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}


