/*
 * webcit.c
 *
 * This is the actual program called by the webserver.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

/*
 * String to unset the cookie.
 * Any date "in the past" will work, so I chose my birthday, right down to
 * the exact minute.  :)
 */
static char *unset = "; expires=28-May-1971 18:10:00 GMT";

void unescape_input(char *buf)
{
	int a, b;
	char hex[3];

	while ((isspace(buf[strlen(buf) - 1])) && (strlen(buf) > 0))
		buf[strlen(buf) - 1] = 0;

	for (a = 0; a < strlen(buf); ++a) {
		if (buf[a] == '+')
			buf[a] = ' ';
		if (buf[a] == '%') {
			hex[0] = buf[a + 1];
			hex[1] = buf[a + 2];
			hex[2] = 0;
			sscanf(hex, "%02x", &b);
			buf[a] = (char) b;
			strcpy(&buf[a + 1], &buf[a + 3]);
		}
	}

}


void addurls(char *url)
{
	char *up, *ptr;
	char buf[256];
	int a, b;
	struct urlcontent *u;

	up = url;
	while (strlen(up) > 0) {

		/* locate the = sign */
		strncpy(buf, up, 255);
		b = (-1);
		for (a = 255; a >= 0; --a)
			if (buf[a] == '=')
				b = a;
		if (b < 0)
			return;
		buf[b] = 0;

		u = (struct urlcontent *) malloc(sizeof(struct urlcontent));
		u->next = WC->urlstrings;
		WC->urlstrings = u;
		strcpy(u->url_key, buf);

		/* now chop that part off */
		for (a = 0; a <= b; ++a)
			++up;

		/* locate the & sign */
		ptr = up;
		b = strlen(up);
		for (a = 0; a < strlen(up); ++a) {
			if (!strncmp(ptr, "&", 1)) {
				b = a;
				break;
			}
			++ptr;
		}
		ptr = up;
		for (a = 0; a < b; ++a)
			++ptr;
		strcpy(ptr, "");

		u->url_data = malloc(strlen(up) + 1);
		strcpy(u->url_data, up);
		u->url_data[b] = 0;
		unescape_input(u->url_data);
		up = ptr;
		++up;
	}
}

void free_urls(void)
{
	struct urlcontent *u;

	while (WC->urlstrings != NULL) {
		free(WC->urlstrings->url_data);
		u = WC->urlstrings->next;
		free(WC->urlstrings);
		WC->urlstrings = u;
	}
}

/*
 * Diagnostic function to display the contents of all variables
 */
void dump_vars(void)
{
	struct urlcontent *u;

	for (u = WC->urlstrings; u != NULL; u = u->next) {
		wprintf("%38s = %s\n", u->url_key, u->url_data);
	}
}

char *bstr(char *key)
{
	struct urlcontent *u;

	for (u = WC->urlstrings; u != NULL; u = u->next) {
		if (!strcasecmp(u->url_key, key))
			return (u->url_data);
	}
	return ("");
}


void wprintf(const char *format,...)
{
	va_list arg_ptr;
	char wbuf[1024];

	va_start(arg_ptr, format);
	vsprintf(wbuf, format, arg_ptr);
	va_end(arg_ptr);

	write(WC->http_sock, wbuf, strlen(wbuf));
}


/*
 * wDumpContent() wraps up an HTTP session, closes tags, etc.
 *
 * print_standard_html_footer should be set to 0 to transmit only, 1 to
 * append the main menu and closing tags, or 2 to
 * append the closing tags only.
 */
void wDumpContent(int print_standard_html_footer)
{
	if (WC->fake_frames) {
		wprintf("<CENTER><FONT SIZE=-1>"
			"<TABLE border=0 width=100%><TR>"
			"<TD><A HREF=\"/ungoto\">"
			"<IMG SRC=\"/static/back.gif\" BORDER=0>"
			"Ungoto</A></TD>");
		wprintf("<TD><A HREF=\"#TheTop\">"
			"<IMG SRC=\"/static/up.gif\" BORDER=0>"
			"Top of page</A></TD>");
		wprintf("<TD><A HREF=\"/display_enter\">"
			"<IMG SRC=\"/static/enter.gif\" BORDER=0>"
			"Enter a message</A></TD>");
		wprintf("<TD><A HREF=\"/gotonext\">"
			"Goto next room"
			"<IMG SRC=\"/static/forward.gif\" BORDER=0></A></TD>"
			"</TR></TABLE>"
			"</FONT>\n"
			"</TD></TR></TABLE></TABLE>\n");
		WC->fake_frames = 0;
	}

	if (print_standard_html_footer) {
		if (print_standard_html_footer != 2) {
			wprintf("<BR>");
		}
		wprintf("</BODY></HTML>\n");
	}


}


void escputs1(char *strbuf, int nbsp)
{
	int a;

	for (a = 0; a < strlen(strbuf); ++a) {
		if (strbuf[a] == '<')
			wprintf("&lt;");
		else if (strbuf[a] == '>')
			wprintf("&gt;");
		else if (strbuf[a] == '&')
			wprintf("&amp;");
		else if (strbuf[a] == '\"')
			wprintf("&quot;");
		else if (strbuf[a] == '\'') 
			wprintf("&#39;");
		else if (strbuf[a] == LB)
			wprintf("<");
		else if (strbuf[a] == RB)
			wprintf(">");
		else if (strbuf[a] == QU)
			wprintf("\"");
		else if ((strbuf[a] == 32) && (nbsp == 1)) {
			wprintf("&nbsp;");
		} else {
			wprintf("%c", strbuf[a]);
		}
	}
}

void escputs(char *strbuf)
{
	escputs1(strbuf, 0);
}



char *urlesc(char *strbuf)
{
	int a, b, c;
	char *ec = " #&;`'|*?-~<>^()[]{}$\\";
	static char outbuf[512];

	strcpy(outbuf, "");

	for (a = 0; a < strlen(strbuf); ++a) {
		c = 0;
		for (b = 0; b < strlen(ec); ++b) {
			if (strbuf[a] == ec[b])
				c = 1;
		}
		b = strlen(outbuf);
		if (c == 1)
			sprintf(&outbuf[b], "%%%02x", strbuf[a]);
		else
			sprintf(&outbuf[b], "%c", strbuf[a]);
	}
	return (outbuf);
}

void urlescputs(char *strbuf)
{
	wprintf("%s", urlesc(strbuf));
}




/*
 * Output all that important stuff that the browser will want to see
 *
 * control codes:
 * 
 * Bits 0 and 1:
 * 0 = Nothing.  Do not display any leading HTTP or HTML.
 * 1 = HTTP headers plus the "fake frames" found in most windows.
 * 2 = HTTP headers required to terminate the session (unset cookies)
 * 3 = HTTP and HTML headers, but no 'fake frames'
 *
 * Bit 2: Set to 1 to auto-refresh page every 30 seconds
 */
void output_headers(int controlcode)
{
	char cookie[256];
	int print_standard_html_head = 0;
	int refresh30 = 0;

	print_standard_html_head	=	controlcode & 0x03;
	refresh30			=	((controlcode & 0x04) >> 2);

	wprintf("HTTP/1.0 200 OK\n");

	if (print_standard_html_head > 0) {
		wprintf("Content-type: text/html\n");
		wprintf("Server: %s\n", SERVER);
		wprintf("Connection: close\n");
		wprintf("Pragma: no-cache\n");
		wprintf("Cache-Control: no-store\n");
	}
	stuff_to_cookie(cookie, WC->wc_session, WC->wc_username,
			WC->wc_password, WC->wc_roomname);
	if (print_standard_html_head == 2) {
		wprintf("Set-cookie: webcit=%s\n", unset);
	} else {
		wprintf("Set-cookie: webcit=%s\n", cookie);
	}

	if (print_standard_html_head > 0) {
		wprintf("\n");
		wprintf("<HTML><HEAD><TITLE>");
		escputs(serv_info.serv_humannode);
		wprintf("</TITLE>\n"
			"<META HTTP-EQUIV=\"Expires\" CONTENT=\"0\">\n"
			"<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">\n");
		if (refresh30) wprintf(
			"<META HTTP-EQUIV=\"refresh\" CONTENT=\"30\">\n");
		wprintf("</HEAD>\n");
		if (WC->ExpressMessages != NULL) {
			wprintf("<SCRIPT language=\"javascript\">\n");
			wprintf("function ExpressMessage() {\n");
			wprintf(" alert(\"");
			escputs(WC->ExpressMessages);
			wprintf("\")\n");
			wprintf(" }\n </SCRIPT>\n");
		}



		/* JavaScript key-based navigation would go here if it
		 * were finished
		 */

		wprintf("<BODY ");
		if (WC->ExpressMessages != NULL) {
			wprintf("onload=\"ExpressMessage()\" ");
			free(WC->ExpressMessages);
			WC->ExpressMessages = NULL;
		}
		wprintf("BACKGROUND=\"/image&name=background\" TEXT=\"#000000\" LINK=\"#004400\">\n");
	
	
	if (print_standard_html_head == 1) {
		wprintf("<A NAME=\"TheTop\"></A>"
			"<TABLE border=0 width=100%>"
			"<TR VALIGN=TOP><TD VALIGN=LEFT CELLPADDING=0>");

		display_menubar(0);

		wprintf("</TD><TD VALIGN=TOP>"
			"<TABLE border=0 width=100%><TR VALIGN=TOP>"
			"<TD>\n");

		embed_room_banner(NULL);

		wprintf("</TD></TR><TR VALIGN=TOP><TD>\n");
		
		WC->fake_frames = 1;
		}
	}
}



void ExpressMessageCat(char *buf) {
	if (WC->ExpressMessages == NULL) {
		WC->ExpressMessages = malloc(strlen(buf) + 4);
		strcpy(WC->ExpressMessages, "");
	} else {
		WC->ExpressMessages = realloc(WC->ExpressMessages,
			(strlen(WC->ExpressMessages) + strlen(buf) + 4));
	}
	strcat(WC->ExpressMessages, buf);
	strcat(WC->ExpressMessages, "\\n");
}


void check_for_express_messages()
{
	char buf[256];

	serv_puts("PEXP");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			ExpressMessageCat(buf);
		}
	}
}




void output_static(char *what)
{
	char buf[4096];
	long thisblock;
	FILE *fp;
	struct stat statbuf;
	off_t bytes;

	sprintf(buf, "static/%s", what);
	fp = fopen(buf, "rb");
	if (fp == NULL) {
		wprintf("HTTP/1.0 404 %s\n", strerror(errno));
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("Cannot open %s: %s\n", what, strerror(errno));
	} else {
		output_headers(0);

		if (!strncasecmp(&what[strlen(what) - 4], ".gif", 4))
			wprintf("Content-type: image/gif\n");
		else if (!strncasecmp(&what[strlen(what) - 4], ".txt", 4))
			wprintf("Content-type: text/plain\n");
		else if (!strncasecmp(&what[strlen(what) - 4], ".jpg", 4))
			wprintf("Content-type: image/jpeg\n");
		else if (!strncasecmp(&what[strlen(what) - 5], ".html", 5))
			wprintf("Content-type: text/html\n");
		else
			wprintf("Content-type: application/octet-stream\n");

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		fprintf(stderr, "Static: %s, %ld bytes\n", what, bytes);
		wprintf("Content-length: %ld\n", (long) bytes);
		wprintf("\n");
		while (bytes > 0) {
			thisblock = sizeof(buf);
			if (thisblock > bytes) thisblock = bytes;
			fread(buf, thisblock, 1, fp);
			write(WC->http_sock, buf, thisblock);
			bytes = bytes - thisblock;
		}
		fclose(fp);
	}
	if (!strcasecmp(bstr("force_close_session"), "yes")) {
		end_webcit_session();
	}
}

/*
 * When the browser requests an image file from the Citadel server,
 * this function is called to transmit it.
 */
void output_image()
{
	char buf[256];
	char xferbuf[4096];
	off_t bytes;
	off_t thisblock;
	off_t accomplished = 0L;


	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_gets(buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		output_headers(0);
		wprintf("Content-type: image/gif\n");
		wprintf("Content-length: %ld\n", (long) bytes);
		wprintf("\n");

		while (bytes > (off_t) 0) {
			thisblock = (off_t) sizeof(xferbuf);
			if (thisblock > bytes)
				thisblock = bytes;
			serv_printf("READ %ld|%ld", accomplished, thisblock);
			serv_gets(buf);
			if (buf[0] == '6')
				thisblock = extract_long(&buf[4], 0);
			serv_read(xferbuf, (int) thisblock);
			write(WC->http_sock, xferbuf, thisblock);
			bytes = bytes - thisblock;
			accomplished = accomplished + thisblock;
		}
		serv_puts("CLOS");
		serv_gets(buf);
	} else {
		wprintf("HTTP/1.0 404 %s\n", strerror(errno));
		output_headers(0);
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("Error retrieving image\n");
	}

}


/*
 * Convenience functions to display a page containing only a string
 */
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext)
{
	wprintf("HTTP/1.0 200 OK\n");
	output_headers(1);
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=%s><TR><TD>", titlebarcolor);
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>%s</B>\n", titlebarmsg);
	wprintf("</FONT></TD></TR></TABLE><BR>\n");
	escputs(messagetext);

	wprintf("<HR>\n");
	embed_main_menu();
	wDumpContent(1);
}

void display_error(char *errormessage)
{
	convenience_page("770000", "Error", errormessage);
}

void display_success(char *successmessage)
{
	convenience_page("007700", "OK", successmessage);
}



void extract_action(char *actbuf, char *cmdbuf)
{
	int i;

	strcpy(actbuf, cmdbuf);
	if (!strncasecmp(actbuf, "GET /", 5))
		strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "PUT /", 5))
		strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "POST /", 6))
		strcpy(actbuf, &actbuf[6]);

	for (i = 0; i < strlen(actbuf); ++i) {
		if (actbuf[i] == ' ') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '/') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '?') {
			actbuf[i] = 0;
			i = 0;
		}
		if (actbuf[i] == '&') {
			actbuf[i] = 0;
			i = 0;
		}
	}
}


void upload_handler(char *name, char *filename, char *encoding,
		    void *content, char *cbtype, size_t length)
{

	fprintf(stderr, "UPLOAD HANDLER CALLED\n");
	fprintf(stderr, "    name = %s\n", name);
	fprintf(stderr, "filename = %s\n", filename);
	fprintf(stderr, "encoding = %s\n", encoding);
	fprintf(stderr, "    type = %s\n", cbtype);
	fprintf(stderr, "  length = %ld\n", (long)length);

	if (strlen(name) > 0) {
		WC->upload = malloc(length);
		if (WC->upload != NULL) {
			WC->upload_length = length;
			memcpy(WC->upload, content, length);
		}
	}
}


/*
 * Entry point for WebCit transaction
 */
void session_loop(struct httprequest *req)
{
	char cmd[256];
	char action[256];
	char buf[256];
	int a, b;
	int ContentLength = 0;
	int BytesRead;
	char ContentType[512];
	char *content;
	struct httprequest *hptr;
	char browser_host[256];
	char user_agent[256];

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_host[256];
	char c_port[256];
	char c_username[256];
	char c_password[256];
	char c_roomname[256];
	char cookie[256];

	strcpy(c_host, defaulthost);
	strcpy(c_port, defaultport);
	strcpy(c_username, "");
	strcpy(c_password, "");
	strcpy(c_roomname, "");

	WC->upload_length = 0;
	WC->upload = NULL;

	hptr = req;
	if (hptr == NULL) return;

	strcpy(cmd, hptr->line);
	hptr = hptr->next;
	extract_action(action, cmd);

	while (hptr != NULL) {
		strcpy(buf, hptr->line);
		hptr = hptr->next;

		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			strcpy(cookie, &buf[15]);
			cookie_to_stuff(cookie, NULL,
				      c_username, c_password, c_roomname);
		}
		else if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
		}
		else if (!strncasecmp(buf, "Content-type: ", 14)) {
			strcpy(ContentType, &buf[14]);
		}
		else if (!strncasecmp(buf, "User-agent: ", 12)) {
			strcpy(user_agent, &buf[12]);
		}
	}

	if (ContentLength > 0) {
		fprintf(stderr, "Content length: %d\n", ContentLength);
		content = malloc(ContentLength + 1);
		memset(content, 0, ContentLength+1);
		BytesRead = 0;

		while (BytesRead < ContentLength) {
			a=read(WC->http_sock, &content[BytesRead],
				ContentLength - BytesRead);
			if (a <= 0) BytesRead = ContentLength;
			else BytesRead += a;
		}

		if (!strncasecmp(ContentType,
			      "application/x-www-form-urlencoded", 33)) {
			addurls(content);
		} else if (!strncasecmp(ContentType, "multipart", 9)) {
			mime_parser(content, ContentLength, ContentType,
				    *upload_handler);
		}
	} else {
		content = NULL;
	}

	/* If there are variables in the URL, we must grab them now */
	for (a = 0; a < strlen(cmd); ++a)
		if ((cmd[a] == '?') || (cmd[a] == '&')) {
			for (b = a; b < strlen(cmd); ++b)
				if (isspace(cmd[b]))
					cmd[b] = 0;
			addurls(&cmd[a + 1]);
			cmd[a] = 0;
		}
	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.  Preference is given to the host and port specified
	 * by browser cookies, if cookies have been supplied.
	 */
	if (!WC->connected) {
		if (strlen(bstr("host")) > 0)
			strcpy(c_host, bstr("host"));
		if (strlen(bstr("port")) > 0)
			strcpy(c_port, bstr("port"));

		if (!strcasecmp(c_host, "uds")) {
			/* unix domain socket */
			sprintf(buf, "%s/citadel.socket", c_port);
			WC->serv_sock = uds_connectsock(buf);
		}
		else {
			/* tcp socket */
			WC->serv_sock = tcp_connectsock(c_host, c_port);
		}

		if (WC->serv_sock < 0) {
			do_logout();
		}

		WC->connected = 1;
		serv_gets(buf);	/* get the server welcome message */
		locate_host(browser_host, WC->http_sock);
		get_serv_info(browser_host, user_agent);
	}
	check_for_express_messages();

	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!WC->logged_in) && (strlen(c_username) > 0) && (strlen(c_password) > 0)) {
		serv_printf("USER %s", c_username);
		serv_gets(buf);
		if (buf[0] == '3') {
			serv_printf("PASS %s", c_password);
			serv_gets(buf);
			if (buf[0] == '2') {
				become_logged_in(c_username, c_password, buf);
			}
		}
	}
	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((strlen(WC->wc_roomname) == 0) && (strlen(c_roomname) > 0)) {
		serv_printf("GOTO %s", c_roomname);
		serv_gets(buf);
		if (buf[0] == '2') {
			strcpy(WC->wc_roomname, c_roomname);
		}
	}
	if (!strcasecmp(action, "static")) {
		strcpy(buf, &cmd[12]);
		for (a = 0; a < strlen(buf); ++a)
			if (isspace(buf[a]))
				buf[a] = 0;
		output_static(buf);
	} else if (!strcasecmp(action, "image")) {
		output_image();
	} else if ((!WC->logged_in) && (!strcasecmp(action, "login"))) {
		do_login();
	} else if (!WC->logged_in) {
		display_login(NULL);
	}
	/* Various commands... */

	else if (!strcasecmp(action, "do_welcome")) {
		do_welcome();
	} else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
	} else if (!strcasecmp(action, "advanced")) {
		display_advanced_menu();
	} else if (!strcasecmp(action, "whobbs")) {
		whobbs();
	} else if (!strcasecmp(action, "knrooms")) {
		list_all_rooms_by_floor();
	} else if (!strcasecmp(action, "gotonext")) {
		slrp_highest();
		gotonext();
	} else if (!strcasecmp(action, "skip")) {
		gotonext();
	} else if (!strcasecmp(action, "ungoto")) {
		ungoto();
	} else if (!strcasecmp(action, "dotgoto")) {
		slrp_highest();
		smart_goto(bstr("room"));
	} else if (!strcasecmp(action, "termquit")) {
		do_logout();
	} else if (!strcasecmp(action, "readnew")) {
		readloop("readnew");
	} else if (!strcasecmp(action, "readold")) {
		readloop("readold");
	} else if (!strcasecmp(action, "readfwd")) {
		readloop("readfwd");
	} else if (!strcasecmp(action, "display_enter")) {
		display_enter();
	} else if (!strcasecmp(action, "post")) {
		post_message();
	} else if (!strcasecmp(action, "confirm_delete_msg")) {
		confirm_delete_msg();
	} else if (!strcasecmp(action, "delete_msg")) {
		delete_msg();
	} else if (!strcasecmp(action, "confirm_move_msg")) {
		confirm_move_msg();
	} else if (!strcasecmp(action, "move_msg")) {
		move_msg();
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
	} else if (!strcasecmp(action, "display_editroom")) {
		display_editroom();
	} else if (!strcasecmp(action, "editroom")) {
		editroom();
	} else if (!strcasecmp(action, "display_editinfo")) {
		display_edit("Room info", "EINF 0", "RINF", "/editinfo");
	} else if (!strcasecmp(action, "editinfo")) {
		save_edit("Room info", "EINF 1", 1);
	} else if (!strcasecmp(action, "display_editbio")) {
		sprintf(buf, "RBIO %s", WC->wc_username);
		display_edit("Your bio", "NOOP", buf, "editbio");
	} else if (!strcasecmp(action, "editbio")) {
		save_edit("Your bio", "EBIO", 0);
	} else if (!strcasecmp(action, "confirm_delete_room")) {
		confirm_delete_room();
	} else if (!strcasecmp(action, "delete_room")) {
		delete_room();
	} else if (!strcasecmp(action, "validate")) {
		validate();
	} else if (!strcasecmp(action, "display_editpic")) {
		display_graphics_upload("your photo",
					"UIMG 0|_userpic_",
					"/editpic");
	} else if (!strcasecmp(action, "editpic")) {
		do_graphics_upload("UIMG 1|_userpic_");
	} else if (!strcasecmp(action, "display_editroompic")) {
		display_graphics_upload("the graphic for this room",
					"UIMG 0|_roompic_",
					"/editroompic");
	} else if (!strcasecmp(action, "editroompic")) {
		do_graphics_upload("UIMG 1|_roompic_");
	} else if (!strcasecmp(action, "select_floor_to_edit_pic")) {
		select_floor_to_edit_pic();
	} else if (!strcasecmp(action, "display_editfloorpic")) {
		sprintf(buf, "UIMG 0|_floorpic_|%s",
			bstr("which_floor"));
		display_graphics_upload("the graphic for this floor",
					buf,
					"/editfloorpic");
	} else if (!strcasecmp(action, "editfloorpic")) {
		sprintf(buf, "UIMG 1|_floorpic_|%s",
			bstr("which_floor"));
		do_graphics_upload(buf);
	} else if (!strcasecmp(action, "display_reg")) {
		display_reg(0);
	} else if (!strcasecmp(action, "register")) {
		register_user();
	} else if (!strcasecmp(action, "display_changepw")) {
		display_changepw();
	} else if (!strcasecmp(action, "changepw")) {
		changepw();
	} else if (!strcasecmp(action, "display_edit_node")) {
		display_edit_node();
	} else if (!strcasecmp(action, "display_netconf")) {
		display_netconf();
	} else if (!strcasecmp(action, "display_confirm_unshare")) {
		display_confirm_unshare();
	} else if (!strcasecmp(action, "display_confirm_delete_node")) {
		display_confirm_delete_node();
	} else if (!strcasecmp(action, "delete_node")) {
		delete_node();
	} else if (!strcasecmp(action, "unshare")) {
		unshare();
	} else if (!strcasecmp(action, "display_add_node")) {
		display_add_node();
	} else if (!strcasecmp(action, "add_node")) {
		add_node();
	} else if (!strcasecmp(action, "display_share")) {
		display_share();
	} else if (!strcasecmp(action, "share")) {
		share();
	} else if (!strcasecmp(action, "terminate_session")) {
		slrp_highest();
		terminate_session();
	} else if (!strcasecmp(action, "edit_me")) {
		edit_me();
	} else if (!strcasecmp(action, "display_siteconfig")) {
		display_siteconfig();
	} else if (!strcasecmp(action, "siteconfig")) {
		siteconfig();
	} else if (!strcasecmp(action, "display_generic")) {
		display_generic();
	} else if (!strcasecmp(action, "do_generic")) {
		do_generic();
	} else if (!strcasecmp(action, "display_menubar")) {
		display_menubar(1);
	} else if (!strcasecmp(action, "diagnostics")) {
		output_headers(1);

		wprintf("You're in session %d<HR>\n", WC->wc_session);
		wprintf("Command: <BR><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><HR>\n");
		wprintf("Variables: <BR><PRE>\n");
		dump_vars();
		wprintf("</PRE><HR>\n");
		wDumpContent(1);
	}
	/* When all else fais, display the main menu. */
	else {
		display_main_menu();
	}

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
