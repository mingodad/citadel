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

#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "webserver.h"

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t SessionListMutex;

struct wcsession *SessionList = NULL;

pthread_key_t MyConKey;                         /* TSD key for MySession() */

void do_housekeeping(void)
{
	struct wcsession *sptr, *ss, *session_to_kill;

	do {
		session_to_kill = NULL;
		pthread_mutex_lock(&SessionListMutex);
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {

			/* Kill idle sessions */
			if ((time(NULL) - (sptr->lastreq)) >
			   (time_t) WEBCIT_TIMEOUT) {
				sptr->killthis = 1;
			}

			/* Remove sessions flagged for kill */
			if (sptr->killthis) {

				/* remove session from linked list */
				if (sptr == SessionList) {
					SessionList = SessionList->next;
				}
				else for (ss=SessionList;ss!=NULL;ss=ss->next) {
					if (ss->next == sptr) {
						ss->next = ss->next->next;
					}
				}

				session_to_kill = sptr;
				goto BREAKOUT;
			}
		}
BREAKOUT:	pthread_mutex_unlock(&SessionListMutex);

		if (session_to_kill != NULL) {
			pthread_mutex_lock(&session_to_kill->SessionMutex);
			close(session_to_kill->serv_sock);
			pthread_mutex_unlock(&session_to_kill->SessionMutex);
			free(session_to_kill);
		}

	} while (session_to_kill != NULL);
}


/* 
 * Wake up occasionally and clean house
 */
void housekeeping_loop(void)
{
	while (1) {
		sleep(HOUSEKEEPING);
		do_housekeeping();
	}
}


/*
 * Generate a unique WebCit session ID (which is not the same thing as the
 * Citadel session ID).
 *
 * FIX ... ensure that session number is truly unique
 *
 */
int GenerateSessionID(void)
{
	static int seq = (-1);

	if (seq < 0) {
		seq = (int) time(NULL);
	}
		
	return ++seq;
}


/*
 * Collapse multiple cookies on one line
 */
int req_gets(int sock, char *buf, char *hold)
{
	int a;

	if (strlen(hold) == 0) {
		strcpy(buf, "");
		a = client_gets(sock, buf);
		if (a<1) return(-1);
	} else {
		strcpy(buf, hold);
	}
	strcpy(hold, "");

	if (!strncasecmp(buf, "Cookie: ", 8)) {
		for (a = 0; a < strlen(buf); ++a)
			if (buf[a] == ';') {
				sprintf(hold, "Cookie: %s", &buf[a + 1]);
				buf[a] = 0;
				while (isspace(hold[8]))
					strcpy(&hold[8], &hold[9]);
				return(0);
			}
	}
	return(0);
}

/*
 * lingering_close() a`la Apache. see
 * http://www.apache.org/docs/misc/fin_wait_2.html for rationale
 */

static int lingering_close(int fd)
{
	char buf[256];
	int i;
	fd_set set;
	struct timeval tv, start;

	gettimeofday(&start, NULL);
	shutdown(fd, 1);
	do {
		do {
			gettimeofday(&tv, NULL);
			tv.tv_sec = SLEEPING - (tv.tv_sec - start.tv_sec);
			tv.tv_usec = start.tv_usec - tv.tv_usec;
			if (tv.tv_usec < 0) {
				tv.tv_sec--;
				tv.tv_usec += 1000000;
			}
			FD_ZERO(&set);
			FD_SET(fd, &set);
			i = select(fd + 1, &set, NULL, NULL, &tv);
		} while (i == -1 && errno == EINTR);

		if (i <= 0)
			break;

		i = read(fd, buf, sizeof buf);
	} while (i != 0 && (i != -1 || errno == EINTR));

	return close(fd);
}




/*
 * This loop gets called once for every HTTP connection made to WebCit.  At
 * this entry point we have an HTTP socket with a browser allegedly on the
 * other end, but we have not yet bound to a WebCit session.
 *
 * The job of this function is to locate the correct session and bind to it,
 * or create a session if necessary and bind to it, then run the WebCit
 * transaction loop.  Afterwards, we unbind from the session.  When this
 * function returns, the worker thread is then free to handle another
 * transaction.
 */
void context_loop(int sock)
{
	struct httprequest *req = NULL;
	struct httprequest *last = NULL;
	struct httprequest *hptr;
	char buf[256], hold[256];
	int desired_session = 0;
	int got_cookie = 0;
	struct wcsession *TheSession, *sptr;

	/*
	 * Find out what it is that the web browser is asking for
	 */
	memset(hold, 0, sizeof(hold));
	do {
		if (req_gets(sock, buf, hold) < 0) return;
		fprintf(stderr, "%sReq: %s%s\n",
			( (req==NULL) ? "\033[32m" : "" ) ,
			buf,
			( (req==NULL) ? "\033[0m" : "" )  );
		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			cookie_to_stuff(&buf[15], &desired_session,
				NULL, NULL, NULL);
			got_cookie = 1;
		}

		hptr = (struct httprequest *)
			malloc(sizeof(struct httprequest));
		if (req == NULL)
			req = hptr;
		else
			last->next = hptr;
		hptr->next = NULL;
		last = hptr;

		strcpy(hptr->line, buf);

	} while (strlen(buf) > 0);


	/*
	 * If requesting a non-root page, there should already be a cookie
	 * set.  If there isn't, the client browser has cookies turned off
	 * (or doesn't support them) and we have to barf & bail.
	 */
	strcpy(buf, req->line);
	if (!strncasecmp(buf, "GET ", 4)) strcpy(buf, &buf[4]);
	else if (!strncasecmp(buf, "HEAD ", 5)) strcpy(buf, &buf[5]);
	if (buf[1]==' ') buf[1]=0;

	/*
	 * While we're at it, gracefully handle requests for the
	 * robots.txt file...
	 */
	if (!strncasecmp(buf, "/robots.txt", 11)) {
		strcpy(req->line, "GET /static/robots.txt HTTP/1.0");
	}

	/* Do the non-root-cookie check now. */
	else if ( (strcmp(buf, "/")) && (got_cookie == 0)) {
		strcpy(req->line, "GET /static/nocookies.html HTTP/1.0");
	}



	/*
	 * See if there's an existing session open with the desired ID
	 */
	TheSession = NULL;
	if (desired_session != 0) {
		pthread_mutex_lock(&SessionListMutex);
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
			if (sptr->wc_session == desired_session) {
				TheSession = sptr;
			}
		}
		pthread_mutex_unlock(&SessionListMutex);
	}

	/*
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		fprintf(stderr, "Creating a new session\n");
		TheSession = (struct wcsession *)
			malloc(sizeof(struct wcsession));
		memset(TheSession, 0, sizeof(struct wcsession));
		TheSession->wc_session = GenerateSessionID();
		pthread_mutex_init(&TheSession->SessionMutex, NULL);

		pthread_mutex_lock(&SessionListMutex);
		TheSession->next = SessionList;
		SessionList = TheSession;
		pthread_mutex_unlock(&SessionListMutex);
	}


	/*
	 *
	 * FIX ... check session integrity here before continuing
	 *
	 */



	/*
	 * Bind to the session and perform the transaction
	 */
	pthread_mutex_lock(&TheSession->SessionMutex);		/* bind */
	pthread_setspecific(MyConKey, (void *)TheSession);
	TheSession->http_sock = sock;
	TheSession->lastreq = time(NULL);			/* log */
	session_loop(req);		/* perform the requested transaction */
	pthread_mutex_unlock(&TheSession->SessionMutex);	/* unbind */

	/* Free the request buffer */
	while (req != NULL) {
		hptr = req->next;
		free(req);
		req = hptr;
	}

	/*
	 * Now our HTTP connection is done.  Close the socket and exit this
	 * function, so the worker thread can handle a new HTTP connection.
	 */
	lingering_close(sock);
}
