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
int wc_port;
char wc_username[256];
char wc_password[256];
char wc_roomname[256];
int TransactionCount = 0;
int logged_in = 0;

struct webcontent *wlist = NULL;
struct webcontent *wlast = NULL;


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
	if (wc_port != 0) printf("Set-cookie: wc_port=%d\n", wc_port);
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
			printf("Content-type: junk/data\n");

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
	int a;

	getz(cmd);
	fprintf(stderr, "Cmd: %s\n", cmd);
	fflush(stderr);

	do {
		getz(buf);
		} while(strlen(buf)>0);

	++TransactionCount;

	if (!strncasecmp(cmd, "GET /static/", 12)) {
		strcpy(buf, &cmd[12]);
		for (a=0; a<strlen(buf); ++a) if (isspace(buf[a])) buf[a]=0;
		output_static(buf);
		}

	else if (!strncasecmp(cmd, "GET /demographics", 17)) {
		printf("HTTP/1.0 200 OK\n");
		output_headers();

		wprintf("<HTML><HEAD><TITLE>Stuff</TITLE></HEAD><BODY>\n");
		wprintf("It's time to include an image...\n");
		wprintf("<IMG SRC=\"/static/netscape.gif\">\n");
		wprintf("...in the page.</BODY></HTML>\n");
		wDumpContent();
		}

	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers();
	
		wprintf("<HTML><HEAD><TITLE>WebCit</TITLE></HEAD><BODY>\n");
		wprintf("TransactionCount is %d<HR>\n", TransactionCount);
		wprintf("You're in session %d<BR>\n", wc_session);
		wprintf("</BODY></HTML>\n");
		wDumpContent();
		}

	fflush(stdout);
	}



int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("%s: usage: %s <session_id>\n", argv[0], argv[0]);
		exit(1);
		}

	wc_session = atoi(argv[1]);
	strcpy(wc_host, "");
	wc_port = 0;
	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");

	while (1) {
		session_loop();
		}

	exit(0);
	}
