/*
 * context_loop.c
 *
 * This is the other half of the webserver.  It handles the task of hooking
 * up HTTP requests with the session they belong to, using HTTP cookies to
 * keep track of things.  If the HTTP request doesn't belong to any currently
 * active session, a new session is spawned.
 *
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include "webcit.h"

/*
 * We keep one of these around for each active session
 */
struct wc_session {
	struct wc_session *next;	/* Next session in list */
	int session_id;			/* Session ID */
	int inpipe[2];			/* Data from webserver to session */
	int outpipe[2];			/* Data from session to webserver */
	pthread_mutex_t critter;	/* Critical section uses pipes */
	};

struct wc_session *SessionList = NULL;

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t MasterCritter;

int GenerateSessionID() {
	return getpid();
	}


void gets0(int fd, char buf[]) {

	buf[0] = 0;
	do {
		buf[strlen(buf)+1] = 0;
		read(fd, &buf[strlen(buf)], 1);
		} while (buf[strlen(buf)-1] >= 32);
	buf[strlen(buf)-1] = 0;
	}

/*
 * Collapse multiple cookies on one line
 */
void req_gets(int sock, char *buf, char *hold) {
	int a;

	if (strlen(hold)==0) {
		client_gets(sock, buf);
		}
	else {
		strcpy(buf, hold);
		}
	strcpy(hold, "");

	if (!strncasecmp(buf, "Cookie: ", 8)) {
		for (a=0; a<strlen(buf); ++a) if (buf[a]==';') {
			sprintf(hold, "Cookie: %s", &buf[a+1]);
			buf[a]=0;
			while (isspace(hold[8])) strcpy(&hold[8], &hold[9]);
			return;
			}
		}
	}

extern const char *defaulthost;
extern const char *defaultport;

/*
 * This loop gets called once for every HTTP connection made to WebCit.
 */
void *context_loop(int sock) {
	char req[256][256];
	char buf[256], hold[256];
	int num_lines = 0;
	int a;
	int f;
	int desired_session = 0;
	char str_session[256];
	struct wc_session *sptr;
	struct wc_session *TheSession;
	int ContentLength;
	int CloseSession = 0;

	printf("Reading request from socket %d\n", sock);

	/*
	 * Find out what it is that the web browser is asking for
	 */
	ContentLength = 0;
	do {
		req_gets(sock, buf, hold);
		if (!strncasecmp(buf, "Cookie: wc_session=", 19)) {
			desired_session = atoi(&buf[19]);
			}
		if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
			}
		strcpy(&req[num_lines++][0], buf);
		} while(strlen(buf)>0);

	/*
	 * See if there's an existing session open with the desired ID
	 */
	TheSession = NULL;
	if (desired_session != 0) {
		pthread_mutex_lock(&MasterCritter);
		for (sptr=SessionList; sptr!=NULL; sptr=sptr->next) {
			if (sptr->session_id == desired_session) {
				TheSession = sptr;
				}
			}
		pthread_mutex_unlock(&MasterCritter);
		}

	/*
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		printf("Creating a new session\n");
		pthread_mutex_lock(&MasterCritter);
		TheSession = (struct wc_session *)
			malloc(sizeof(struct wc_session));
		TheSession->session_id = GenerateSessionID();
		pipe(TheSession->inpipe);
		pipe(TheSession->outpipe);
		pthread_mutex_init(&TheSession->critter, NULL);
		TheSession->next = SessionList;
		SessionList = TheSession;
		sprintf(str_session, "%d", TheSession->session_id);
		f = fork();
		fflush(stdout); fflush(stdin);
		if (f==0) {
			dup2(TheSession->inpipe[0], 0);
			dup2(TheSession->outpipe[1], 1);
			execlp("./webcit", "webcit", str_session, defaulthost,
			       defaultport, NULL);
			printf("HTTP/1.0 404 WebCit Failure\n\n");
			printf("Server: %s\n", SERVER);
			printf("Content-type: text/html\n");
			printf("Content-length: 76\n");
			printf("\n");
			printf("<HTML><HEAD><TITLE>Error</TITLE></HEAD>\n");
			printf("<BODY>execlp() failed</BODY></HTML>\n");
			exit(0);
			}
		pthread_mutex_unlock(&MasterCritter);
		}

	/*
	 * Grab a lock on the session, so other threads don't try to access
	 * the pipes at the same time.
	 */
	printf("Locking session %d...\n", TheSession->session_id);
	pthread_mutex_lock(&TheSession->critter);
	printf("   ...got lock\n");

	/* 
	 * Send the request to the appropriate session...
	 */
	printf("   Writing %d lines of command\n", num_lines);
	printf("%s\n", &req[0][0]);
	for (a=0; a<num_lines; ++a) {
		write(TheSession->inpipe[1], &req[a][0], strlen(&req[a][0]));
		write(TheSession->inpipe[1], "\n", 1);
		}
	printf("   Writing %d bytes of content\n", ContentLength);
	while (ContentLength--) {
		read(sock, buf, 1);
		write(TheSession->inpipe[1], buf, 1);
		}

	/*
	 * ...and get the response.
	 */
	printf("   Reading response\n");
	ContentLength = 0;
	do {
		gets0(TheSession->outpipe[0], buf);
		write(sock, buf, strlen(buf));
		write(sock, "\n", 1);
		if (!strncasecmp(buf, "Content-length: ", 16))
			ContentLength = atoi(&buf[16]);
		if (!strcasecmp(buf, "X-WebCit-Session: close")) {
			CloseSession = 1;
			}
		} while (strlen(buf) > 0);

	printf("   Reading %d bytes of content\n");
	while(ContentLength--) {
		read(TheSession->outpipe[0], buf, 1);
		write(sock, buf, 1);
		}

	/*
	 * Now our HTTP connection is done.  It would be relatively easy
	 * to support HTTP/1.1 "persistent" connections by looping back to
	 * the top of this function.  For now, we'll just close.
	 */
	printf("   Closing socket\n");
	close(sock);

	/*
	 * Let go of the lock
	 */
	printf("Unlocking.\n");
	pthread_mutex_unlock(&TheSession->critter);



	/*
	 * If the last response included a "close session" directive,
	 * remove the context now.
	 */
	if (CloseSession) {
		printf("Removing session.\n");
		pthread_mutex_lock(&MasterCritter);

		if (SessionList==TheSession) {
			SessionList = SessionList->next;
			}
		else {
			for (sptr=SessionList; sptr!=NULL; sptr=sptr->next) {
				if (sptr->next == TheSession) {
					sptr->next = TheSession->next;
					}
				}
			}
	
		free(TheSession);
	
		pthread_mutex_unlock(&MasterCritter);
		}



	/*
	 * The thread handling this HTTP connection is now finished.
	 */
	pthread_exit(NULL);
	}
