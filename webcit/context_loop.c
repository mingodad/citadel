/*
 * context_loop.c
 *
 * This is the other half of the webserver.  It handles the task of hooking
 * up HTTP requests with the session they belong to, using HTTP cookies to
 * keep track of things.  If the HTTP request doesn't belong to any currently
 * active session, a new session is spawned.
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
 * This loop gets called once for every HTTP connection made to WebCit.
 */
void *context_loop(int *socknumptr) {
	char req[256][256];
	char buf[256];
	int num_lines = 0;
	int a;
	int f;
	int sock;
	int desired_session = 0;
	char str_session[256];
	struct wc_session *sptr;
	struct wc_session *TheSession;
	int ContentLength;

	sock = *socknumptr;

	/*
	 * Find out what it is that the web browser is asking for
	 */
	do {
		client_gets(sock, buf);
		if (!strncasecmp(buf, "Cookie: wc_session=", 19)) {
			desired_session = atoi(&buf[19]);
			}


		strcpy(&req[num_lines++][0], buf);
		} while(strlen(buf)>0);

	/*
	 * See if there's an existing session open with the desired ID
	 */
	TheSession = NULL;
	if (desired_session != 0) {
		for (sptr=SessionList; sptr!=NULL; sptr=sptr->next) {
			if (sptr->session_id == desired_session) {
				TheSession = sptr;
				}
			}
		}

	/*
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		printf("Creating a new session\n");
		TheSession = (struct wc_session *)
			malloc(sizeof(struct wc_session));
		TheSession->session_id = GenerateSessionID();
		pthread_mutex_init(&TheSession->critter, NULL);
		pipe(TheSession->inpipe);
		pipe(TheSession->outpipe);
		TheSession->next = SessionList;
		SessionList = TheSession;
		sprintf(str_session, "%d", TheSession->session_id);
		f = fork();
		fflush(stdout); fflush(stdin);
		if (f==0) {
			dup2(TheSession->inpipe[0], 0);
			dup2(TheSession->outpipe[1], 1);
			execlp("./webcit", "webcit", str_session, NULL);
			printf("HTTP/1.0 404 WebCit Failure\n\n");
			printf("Server: %s\n", SERVER);
			printf("Content-type: text/html\n");
			printf("Content-length: 76\n");
			printf("\n");
			printf("<HTML><HEAD><TITLE>Error</TITLE></HEAD>\n");
			printf("<BODY>execlp() failed</BODY></HTML>\n");
			exit(0);
			}
		}

	/* 
	 * Send the request to the appropriate session
	 */
	pthread_mutex_lock(&TheSession->critter);
	for (a=0; a<num_lines; ++a) {
		write(TheSession->inpipe[1], &req[a][0], strlen(&req[a][0]));
		write(TheSession->inpipe[1], "\n", 1);
		}
	/* write(TheSession->inpipe[1], "\n", 1); */

	/*
	 * ...and get the response (FIX for non-text)
	 */
	ContentLength = 0;
	do {
		gets0(TheSession->outpipe[0], buf);
		write(sock, buf, strlen(buf));
		write(sock, "\n", 1);
		if (!strncasecmp(buf, "Content-length: ", 16))
			ContentLength = atoi(&buf[16]);
		} while (strlen(buf) > 0);

	while(ContentLength--) {
		read(TheSession->outpipe[0], buf, 1);
		write(sock, buf, 1);
		}

	pthread_mutex_unlock(&TheSession->critter);

	/*
	 * Now our HTTP connection is done.  It would be relatively easy
	 * to support HTTP/1.1 "persistent" connections by looping back to
	 * the top of this function.  For now, we'll just exit.
	 */
	close(sock);
	pthread_exit(NULL);
	}
