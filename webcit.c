/*
 * webcit.c
 *
 * This is the actual program called by the webserver.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "webcit.h"

int wc_session;
char wc_host[256];
char wc_port[256];
char wc_username[256];
char wc_password[256];
char wc_roomname[256];
int TransactionCount = 0;
int connected = 0;
int logged_in = 0;

struct webcontent *wlist = NULL;
struct webcontent *wlast = NULL;

struct urlcontent *urlstrings = NULL;


void unescape_input(buf)
char buf[]; {
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
		if (b<0) goto DONE;
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
				goto FOUNDIT;
				}
			++ptr;
			}
FOUNDIT:	ptr = up;
		for (a=0; a<b; ++a) ++ptr;
		strcpy(ptr,"");
		
		u->url_data = malloc(strlen(up));
		strcpy(u->url_data, up);
		unescape_input(u->url_data);

		up = ptr;
		++up;

		fprintf(stderr, "%s=%s\n", u->url_key, u->url_data);
		}
DONE:
	}

void free_urls() {
	struct urlcontent *u;

	while (urlstrings != NULL) {
		free(urlstrings->url_data);
		u = urlstrings->next;
		free(urlstrings);
		urlstrings = u;
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

int wContentLength() {
	struct webcontent *wptr;
	int len = 0;

	for (wptr = wlist; wptr != NULL; wptr = wptr->next) {
		len = len + strlen(wptr->w_data);
		}

	return(len);
	}

void wDumpContent() {
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
void output_headers() {
	printf("Server: %s\n", SERVER);
	printf("Connection: close\n");
	printf("Set-cookie: wc_session=%d\n", wc_session);
	if (strlen(wc_host)>0) printf("Set-cookie: wc_host=%s\n", wc_host);
	if (strlen(wc_port)>0) printf("Set-cookie: wc_port=%s\n", wc_port);
	if (strlen(wc_username)>0) printf("Set-cookie: wc_username=%s\n",
		wc_username);
	if (strlen(wc_password)>0) printf("Set-cookie: wc_password=%s\n",
		wc_password);
	if (strlen(wc_roomname)>0) printf("Set-cookie: wc_roomname=%s\n",
		wc_roomname);
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
		else
			printf("Content-type: application/octet-stream\n");

		fstat(fileno(fp), &statbuf);
		bytes = statbuf.st_size;
		printf("Content-length: %d\n", bytes);
		printf("\n");
		while (bytes--) {
			putc(getc(fp), stdout);
			}
		fclose(fp);
		}
	}


void session_loop() {
	char cmd[256];
	char buf[256];
	int a, b;
	int ContentLength = 0;
	char *content;

	/* We stuff these with the values coming from the client cookies,
	 * so we can use them to reconnect a timed out session if we have to.
	 */
	char c_host[256];
	char c_port[256];
	char c_username[256];
	char c_password[256];
	char c_roomname[256];

	strcpy(c_host, DEFAULT_HOST);
	strcpy(c_port, DEFAULT_PORT);
	strcpy(c_username, "");
	strcpy(c_password, "");
	strcpy(c_roomname, "");

	getz(cmd);
	fprintf(stderr, "\nCmd: %s\n", cmd);
	fflush(stderr);

	do {
		getz(buf);
		fprintf(stderr, "Buf: %s\n", buf);
		fflush(stderr);

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
		content[ContentLength] = 0;
		fprintf(stderr, "CONTENT:\n%s\n", content);
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
		serv_printf("IDEN %d|%d|%d|%s|%s",
			DEVELOPER_ID,
			CLIENT_ID,
			CLIENT_VERSION,
			SERVER,
			""
			);
		serv_gets(buf);
		/* FIX find out where the user is */
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
				logged_in = 1;
				strcpy(wc_username, c_username);
				strcpy(wc_password, c_password);
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
	for (a=0; a<strlen(cmd); ++a) if (cmd[a]=='?') {
		for (b=a; b<strlen(cmd); ++b) if (isspace(cmd[b])) cmd[b]=0;
		addurls(&cmd[a+1]);
		cmd[a] = 0;
		}

	if (!strncasecmp(cmd, "GET /static/", 12)) {
		strcpy(buf, &cmd[12]);
		for (a=0; a<strlen(buf); ++a) if (isspace(buf[a])) buf[a]=0;
		output_static(buf);
		}

	if (!strncasecmp(cmd, "GET /nothing", 12)) {
		printf("HTTP/1.0 200 OK\n");
		output_headers();
	
		wprintf("<HTML><HEAD><TITLE>WebCit</TITLE></HEAD><BODY>\n");
		wprintf("TransactionCount is %d<HR>\n", TransactionCount);
		wprintf("You're in session %d<BR>\n", wc_session);
		wprintf("</BODY></HTML>\n");
		wDumpContent();
		}

	else if ((!logged_in)&&(!strncasecmp(cmd, "POST /login", 11))) {
		do_login();
		}

	else if (!logged_in) {
		display_login_page();
		}

	/* Various commands... */

	/* When all else fails, display the login page. */
	else {
		display_login_page();
		}

	fflush(stdout);
	if (content != NULL) {
		free(content);
		content = NULL;
		}
	free_urls();
	}



int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("%s: usage: %s <session_id>\n", argv[0], argv[0]);
		exit(1);
		}

	wc_session = atoi(argv[1]);
	strcpy(wc_host, "");
	strcpy(wc_port, "");
	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");

	while (1) {
		session_loop();
		}

	exit(0);
	}
