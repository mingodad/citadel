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

int wc_session;
char wc_host[256];
int wc_port;
char wc_username[256];
char wc_password[256];
char wc_roomname[256];

void getz(char *buf) {
	if (fgets(buf, 256, stdin) == NULL) strcpy(buf, "");
	else {
		while ((strlen(buf)>0)&&(!isprint(buf[strlen(buf)-1])))
			buf[strlen(buf)-1] = 0;
		}
	}

void output_reconnect_cookies() {
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

void session_loop() {
	char buf[256];
	char content[4096];
	static int TransactionCount = 0;

	do {
		getz(buf);
		} while(strlen(buf)>0);

	printf("HTTP/1.0 200 OK\n");
	printf("Server: WebCit v2 (Velma)\n");
	printf("Connection: close\n");
	output_reconnect_cookies();
	printf("Content-Type: text/html\n");

	strcpy(content, "");

	sprintf(&content[strlen(content)],
		"<HTML><HEAD><TITLE>WebCit</TITLE></HEAD><BODY>\n");
	sprintf(&content[strlen(content)],
		"TransactionCount is %d<HR>\n", ++TransactionCount);
	sprintf(&content[strlen(content)],
		"You're in session %d<BR>\n", wc_session);
	sprintf(&content[strlen(content)],
		"</BODY></HTML>\n");

	printf("Content-length: %d\n", strlen(content));
	printf("\n");
	fwrite(content, strlen(content), 1, stdout);
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
