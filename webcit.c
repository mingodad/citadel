/*
 * webcit.c
 *
 * This is the actual program called by the webserver.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 *
 * $Id$
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "webcit.h"
#include "child.h"

int wc_session;
char wc_host[256];
char wc_port[256];
char wc_username[256];
char wc_password[256];
char wc_roomname[256];
int TransactionCount = 0;
int connected = 0;
int logged_in = 0;
int axlevel;

struct webcontent *wlist = NULL;
struct webcontent *wlast = NULL;

struct urlcontent *urlstrings = NULL;

static const char *defaulthost = DEFAULT_HOST;
static const char *defaultport = DEFAULT_PORT;


void unescape_input(char *buf)
{
	int a,b;
	char hex[3];

	while ((isspace(buf[strlen(buf)-1]))&&(strlen(buf)>0))
		buf[strlen(buf)-1] = 0;

	for (a=0; a<strlen(buf); ++a) {
		if (buf[a]=='+') buf[a]=' ';	
		if (buf[a]=='%') {
			hex[0]=buf[a+1];
			hex[1]=buf[a+2];
			hex[2]=0;
			sscanf(hex,"%02x",&b);
			buf[a] = (char) b;
			strcpy(&buf[a+1],&buf[a+3]);
			}
		}

	}


void addurls(char *url) {
	char *up, *ptr;
	char buf[256];
	int a,b;
	struct urlcontent *u;

	up = url;
	while (strlen(up)>0) {
		
		/* locate the = sign */
		strncpy(buf,up,255);
		b = (-1);
		for (a=255; a>=0; --a) if (buf[a]=='=') b=a;
		if (b<0) return;
		buf[b]=0;
	
		u = (struct urlcontent *)malloc(sizeof(struct urlcontent));
		u->next = urlstrings;
		urlstrings = u;
		strcpy(u->url_key, buf);
	
		/* now chop that part off */
		for (a=0; a<=b; ++a) ++up;
	
		/* locate the & sign */
		ptr = up;
		b = strlen(up);
		for (a=0; a<strlen(up); ++a) {
			if (!strncmp(ptr,"&",1)) {
				b=a;
				break;
				}
			++ptr;
			}
		ptr = up;
		for (a=0; a<b; ++a) ++ptr;
		strcpy(ptr,"");
		
		u->url_data = malloc(strlen(up)+1);
		strcpy(u->url_data, up);
		u->url_data[b] = 0;
		unescape_input(u->url_data);
		up = ptr;
		++up;
		}
	}

void free_urls(void) {
	struct urlcontent *u;

	while (urlstrings != NULL) {
		free(urlstrings->url_data);
		u = urlstrings->next;
		free(urlstrings);
		urlstrings = u;
		}
	}

/*
 * Diagnostic function to display the contents of all variables
 */
void dump_vars(void) {
	struct urlcontent *u;

	for (u = urlstrings; u != NULL; u = u->next) {
		wprintf("%38s = %s\n", u->url_key, u->url_data);
		}
	}

char *bstr(char *key) {
	struct urlcontent *u;

	for (u = urlstrings; u != NULL; u = u->next) {
		if (!strcasecmp(u->url_key, key)) return(u->url_data);
		}
	return("");
	}


void wprintf(const char *format, ...) {   
        va_list arg_ptr;   
	struct webcontent *wptr;

	wptr = (struct webcontent *)malloc(sizeof(struct webcontent));
	wptr->next = NULL;
	if (wlist == NULL) {
		wlist = wptr;
		wlast = wptr;
		}
	else {
		wlast->next = wptr;
		wlast = wptr;
		}
  
       	va_start(arg_ptr, format);   
       	vsprintf(wptr->w_data, format, arg_ptr);   
       	va_end(arg_ptr);   
	}

int wContentLength(void) {
	struct webcontent *wptr;
	int len = 0;

	for (wptr = wlist; wptr != NULL; wptr = wptr->next) {
		len = len + strlen(wptr->w_data);
		}

	return(len);
	}

void wDumpContent(void) {
	struct webcontent *wptr;

	printf("Content-type: text/html\n");
	printf("Content-length: %d\n", wContentLength());
	printf("\n");

	while (wlist != NULL) {
		fwrite(wlist->w_data, strlen(wlist->w_data), 1, stdout);
		wptr = wlist->next;
		free(wlist);
		wlist = wptr;
		}
	wlast = NULL;
	}


void escputs1(char *strbuf, int nbsp)
{
	int a;

	for (a=0; a<strlen(strbuf); ++a) {
		if (strbuf[a]=='<') wprintf("&lt;");
		else if (strbuf[a]=='>') wprintf("&gt;");
		else if (strbuf[a]=='&') wprintf("&amp;");
		else if (strbuf[a]==34) wprintf("&quot;");
		else if (strbuf[a]==LB) wprintf("<");
		else if (strbuf[a]==RB) wprintf(">");
		else if (strbuf[a]==QU) wprintf("\"");
		else if ((strbuf[a]==32)&&(nbsp==1)) {
			wprintf("&nbsp;");
			}
		else {
			wprintf("%c", strbuf[a]);
			}
		}
	}

void escputs(char *strbuf)
{
	escputs1(strbuf,0);
	}



char *urlesc(char *strbuf)
{
	int a,b,c;
        char *ec = " #&;`'|*?-~<>^()[]{}$\\";
	static char outbuf[512];
	
	strcpy(outbuf,"");

	for (a=0; a<strlen(strbuf); ++a) {
		c = 0;
		for (b=0; b<strlen(ec); ++b) {
			if (strbuf[a]==ec[b]) c=1;
			}
		b = strlen(outbuf);
		if (c==1) sprintf(&outbuf[b],"%%%02x",strbuf[a]);
		else sprintf(&outbuf[b],"%c",strbuf[a]);
		}
	return(outbuf);
	}

void urlescputs(char *strbuf)
{
	wprintf("%s",urlesc(strbuf));
	}


void getz(char *buf) {
	if (fgets(buf, 256, stdin) == NULL) strcpy(buf, "");
	else {
		while ((strlen(buf)>0)&&(!isprint(buf[strlen(buf)-1])))
			buf[strlen(buf)-1] = 0;
		}
	}

/*
 * Output all that important stuff that the browser will want to see
 */
void output_headers(void) {

	static char *unset = "; expires=28-May-1971 18:10:00 GMT";

	printf("Server: %s\n", SERVER);
	printf("Connection: close\n");
	printf("Set-cookie: wc_session=%d\n", wc_session);

	if (strlen(wc_host)>0) printf("Set-cookie: wc_host=%s\n", wc_host);
	else printf("Set-cookie: wc_host=%s\n", unset);

	if (strlen(wc_port)>0) printf("Set-cookie: wc_port=%s\n", wc_port);
	else printf("Set-cookie: wc_port=%s\n", unset);

	if (strlen(wc_username)>0) printf("Set-cookie: wc_username=%s\n",
		wc_username);
	else printf("Set-cookie: wc_username=%s\n", unset);

	if (strlen(wc_password)>0) printf("Set-cookie: wc_password=%s\n",
		wc_password);
	else printf("Set-cookie: wc_password=%s\n", unset);

	if (strlen(wc_roomname)>0) printf("Set-cookie: wc_roomname=%s\n",
		wc_roomname);
	else printf("Set-cookie: wc_roomname=%s\n", unset);
	}

void output_static(char *what) {
	char buf[256];
	FILE *fp;
	struct stat statbuf;
	off_t bytes;

	sprintf(buf, "static/%s", what);
	fp = fopen(buf, "rb");
	if (fp == NULL) {
		printf("HTTP/1.0 404 %s\n", strerror(errno));
		output_headers();
		printf("Content-Type: text/plain\n");
		sprintf(buf, "%s: %s\n", what, strerror(errno));
		printf("Content-length: %d\n", strlen(buf));
		printf("\n");
		fwrite(buf, strlen(buf), 1, stdout);
		}
	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers();

		if (!strncasecmp(&what[strlen(what)-4], ".gif", 4))
			printf("Content-type: image/gif\n");
		else if (!strncasecmp(&what[strlen(what)-5], ".html", 5))
			printf("Content-type: text/html\n");
		else
			printf("Content-type: application/octet-stream\n");

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		printf("Content-length: %ld\n", (long)bytes);
		printf("\n");
		while (bytes--) {
			putc(getc(fp), stdout);
			}
		fflush(stdout);
		fclose(fp);
		}
	}

void output_image() {
	char buf[256];
	char xferbuf[4096];
	off_t bytes;
	off_t thisblock;
	off_t accomplished = 0L;


	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_gets(buf);
	if (buf[0]=='2') {
		bytes = extract_long(&buf[4], 0);
		printf("HTTP/1.0 200 OK\n");
		output_headers();
		printf("Content-type: image/gif\n");
		printf("Content-length: %ld\n", bytes);
		printf("\n");

		while (bytes > (off_t)0) {
			thisblock = (off_t)sizeof(xferbuf);
			if (thisblock > bytes) thisblock = bytes;
			serv_printf("READ %ld|%ld", accomplished, thisblock);
			serv_gets(buf);
			if (buf[0]=='6') thisblock = extract_long(&buf[4],0);
			serv_read(xferbuf, (int)thisblock);
			fwrite(xferbuf, thisblock, 1, stdout);
			bytes = bytes - thisblock;
			accomplished = accomplished + thisblock;
			}
		fflush(stdout);
		serv_puts("CLOS");
		serv_gets(buf);
		}
	else {
		printf("HTTP/1.0 404 %s\n", strerror(errno));
		output_headers();
		printf("Content-Type: text/plain\n");
		sprintf(buf, "Error retrieving image\n");
		printf("Content-length: %d\n", strlen(buf));
		printf("\n");
		fwrite(buf, strlen(buf), 1, stdout);
		}

	}

void extract_action(char *actbuf, char *cmdbuf) {
	int i;

	strcpy(actbuf, cmdbuf);
	if (!strncasecmp(actbuf, "GET /", 5)) strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "PUT /", 5)) strcpy(actbuf, &actbuf[5]);
	if (!strncasecmp(actbuf, "POST /", 6)) strcpy(actbuf, &actbuf[6]);

	for (i=0; i<strlen(actbuf); ++i) {
		if (actbuf[i]==' ') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='/') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='?') { actbuf[i]=0; i=0; }
		if (actbuf[i]=='&') { actbuf[i]=0; i=0; }
		}
	}


void session_loop(void) {
	char cmd[256];
	char action[256];
	char buf[256];
	int a, b;
	int ContentLength = 0;
	char *content;
FILE *fp;

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_host[256];
	char c_port[256];
	char c_username[256];
	char c_password[256];
	char c_roomname[256];

	strcpy(c_host, defaulthost);
	strcpy(c_port, defaultport);
	strcpy(c_username, "");
	strcpy(c_password, "");
	strcpy(c_roomname, "");

	getz(cmd);
	extract_action(action, cmd);

	do {
		getz(buf);

		if (!strncasecmp(buf, "Cookie: wc_host=", 16))
			strcpy(c_host, &buf[16]);
		if (!strncasecmp(buf, "Cookie: wc_port=", 16))
			strcpy(c_port, &buf[16]);
		if (!strncasecmp(buf, "Cookie: wc_username=", 20))
			strcpy(c_username, &buf[20]);
		if (!strncasecmp(buf, "Cookie: wc_password=", 20))
			strcpy(c_password, &buf[20]);
		if (!strncasecmp(buf, "Cookie: wc_roomname=", 20))
			strcpy(c_roomname, &buf[20]);

		if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
			}

		} while(strlen(buf)>0);

	++TransactionCount;

	if (ContentLength > 0) {
		content = malloc(ContentLength+1);
		fread(content, ContentLength, 1, stdin);
fp = fopen("content", "wb");
fwrite(content, ContentLength, 1, fp);
fclose(fp);
		content[ContentLength] = 0;
		addurls(content);
		}
	else {
		content = NULL;
		}

	/*
	 * If we're not connected to a Citadel server, try to hook up the
	 * connection now.  Preference is given to the host and port specified
	 * by browser cookies, if cookies have been supplied.
	 */
	if (!connected) {
		serv_sock = connectsock(c_host, c_port, "tcp");
		connected = 1;
		serv_gets(buf);	/* get the server welcome message */
		strcpy(wc_host, c_host);
		strcpy(wc_port, c_port);
		get_serv_info();
		}


	/*
	 * If we're not logged in, but we have username and password cookies
	 * supplied by the browser, try using them to log in.
	 */
	if ((!logged_in)&&(strlen(c_username)>0)&&(strlen(c_password)>0)) {
		serv_printf("USER %s", c_username);
		serv_gets(buf);
		if (buf[0]=='3') {
			serv_printf("PASS %s", c_password);
			serv_gets(buf);
			if (buf[0]=='2') {
				become_logged_in(c_username, c_password, buf);
				}
			}
		}

	/*
	 * If we don't have a current room, but a cookie specifying the
	 * current room is supplied, make an effort to go there.
	 */
	if ((strlen(wc_roomname)==0) && (strlen(c_roomname)>0) ) {
		serv_printf("GOTO %s", c_roomname);
		serv_gets(buf);
		if (buf[0]=='2') {
			strcpy(wc_roomname, c_roomname);
			}
		}

	/* If there are variables in the URL, we must grab them now */	
	for (a=0; a<strlen(cmd); ++a) if ((cmd[a]=='?')||(cmd[a]=='&')) {
		for (b=a; b<strlen(cmd); ++b) if (isspace(cmd[b])) cmd[b]=0;
		addurls(&cmd[a+1]);
		cmd[a] = 0;
		}

	if (!strcasecmp(action, "static")) {
		strcpy(buf, &cmd[12]);
		for (a=0; a<strlen(buf); ++a) if (isspace(buf[a])) buf[a]=0;
		output_static(buf);
		}

	else if (!strcasecmp(action, "image")) {
		output_image();
		}

	else if ((!logged_in)&&(!strcasecmp(action, "login"))) {
		do_login();
		}

	else if (!logged_in) {
		display_login(NULL);
		}

	/* Various commands... */
	
	else if (!strcasecmp(action, "do_welcome")) {
		do_welcome();
		}

	else if (!strcasecmp(action, "display_main_menu")) {
		display_main_menu();
		}

	else if (!strcasecmp(action, "advanced")) {
		display_advanced_menu();
		}

	else if (!strcasecmp(action, "whobbs")) {
		whobbs();
		}

	else if (!strcasecmp(action, "knrooms")) {
		list_all_rooms_by_floor();
		}

	else if (!strcasecmp(action, "gotonext")) {
		slrp_highest();
		gotonext();
		}

	else if (!strcasecmp(action, "skip")) {
		gotonext();
		}

	else if (!strcasecmp(action, "ungoto")) {
		ungoto();
		}

	else if (!strcasecmp(action, "dotgoto")) {
		slrp_highest();
		dotgoto();
		}

	else if (!strcasecmp(action, "termquit")) {
		do_logout();
		}

	else if (!strcasecmp(action, "readnew")) {
		readloop("readnew");
		}

	else if (!strcasecmp(action, "readold")) {
		readloop("readold");
		}

	else if (!strcasecmp(action, "readfwd")) {
		readloop("readfwd");
		}

	else if (!strcasecmp(action, "display_enter")) {
		display_enter();
		}

	else if (!strcasecmp(action, "post")) {
		post_message();
		}

	else if (!strcasecmp(action, "confirm_delete_msg")) {
		confirm_delete_msg();
		}

	else if (!strcasecmp(action, "delete_msg")) {
		delete_msg();
		}

	else if (!strcasecmp(action, "confirm_move_msg")) {
		confirm_move_msg();
		}

	else if (!strcasecmp(action, "move_msg")) {
		move_msg();
		}

	else if (!strcasecmp(action, "userlist")) {
		userlist();
		}

	else if (!strcasecmp(action, "showuser")) {
		showuser();
		}

	/* When all else fails... */
	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers();
	
		wprintf("<HTML><HEAD><TITLE>WebCit</TITLE></HEAD><BODY BACKGROUND=\"/image&name=background\" TEXT=\"#000000\" LINK=\"#004400\">\n");
		wprintf("TransactionCount is %d<BR>\n", TransactionCount);
		wprintf("You're in session %d<HR>\n", wc_session);
		wprintf("Command: <BR><PRE>\n");
		escputs(cmd);
		wprintf("</PRE><HR>\n");
		wprintf("Variables: <BR><PRE>\n");
		dump_vars();
		wprintf("</PRE><HR>\n");
		wprintf("</BODY></HTML>\n");
		wDumpContent();
		}

	fflush(stdout);
	if (content != NULL) {
		free(content);
		content = NULL;
		}
	free_urls();
	}

int main(int argc, char *argv[]) {

	if (argc < 2 || argc > 4) {
		fprintf(stderr,
			"webcit: usage: webcit <session_id> [host [port]]\n");
		return 1;
		}

	wc_session = atoi(argv[1]);

	if (argc > 2) {
		defaulthost = argv[2];
		if (argc > 3)
			defaultport = argv[3];
		}

	strcpy(wc_host, "");
	strcpy(wc_port, "");
	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");

	while (1) {
		session_loop();
		}
	}
