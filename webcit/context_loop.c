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

/*
 * We keep one of these around for each active session
 */
struct wc_session {
	struct wc_session *next;	/* Next session in list */
	int session_id;		/* Session ID */
	pid_t webcit_pid;	/* PID of the webcit process */
	int inpipe[2];		/* Data from webserver to session */
	int outpipe[2];		/* Data from session to webserver */
	pthread_mutex_t critter;	/* Critical section uses pipes */
	time_t lastreq;		/* Timestamp of most recent http */
};

struct wc_session *SessionList = NULL;
extern const char *defaulthost;
extern const char *defaultport;

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t MasterCritter;


/*
 * Grab a lock on the session, so other threads don't try to access
 * the pipes at the same time.
 */
static void lock_session(struct wc_session *session)
{
	printf("Locking session %d...\n", session->session_id);
	pthread_mutex_lock(&session->critter);
	printf("   ...got lock\n");
}

/*
 * Let go of the lock.
 */
static void unlock_session(struct wc_session *session)
{
	printf("Unlocking.\n");
	pthread_mutex_unlock(&session->critter);
}

/*
 * Remove a session context from the list
 */
void remove_session(struct wc_session *TheSession, int do_lock)
{
	struct wc_session *sptr;

	printf("Removing session.\n");
	if (do_lock)
		pthread_mutex_lock(&MasterCritter);

	if (SessionList == TheSession) {
		SessionList = SessionList->next;
	} else {
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
			if (sptr->next == TheSession) {
				sptr->next = TheSession->next;
			}
		}
	}

	close(TheSession->inpipe[1]);
	close(TheSession->outpipe[0]);
	if (do_lock)
		unlock_session(TheSession);
	free(TheSession);

	pthread_mutex_unlock(&MasterCritter);
}




void do_housekeeping(void)
{
	struct wc_session *sptr;

	pthread_mutex_lock(&MasterCritter);

	/* Kill idle sessions */
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
		if ((time(NULL) - (sptr->lastreq)) > (time_t) WEBCIT_TIMEOUT) {
			kill(sptr->webcit_pid, 15);
		}
	}

	/* Remove dead sessions */
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
		if (kill(sptr->webcit_pid, 0)) {
			remove_session(sptr, 0);
		}
	}

	pthread_mutex_unlock(&MasterCritter);
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





int GenerateSessionID(void)
{
	return getpid();
}


void gets0(int fd, char buf[])
{

	buf[0] = 0;
	do {
		buf[strlen(buf) + 1] = 0;
		read(fd, &buf[strlen(buf)], 1);
	} while (buf[strlen(buf) - 1] >= 32);
	buf[strlen(buf) - 1] = 0;
}

/*
 * Collapse multiple cookies on one line
 */
void req_gets(int sock, char *buf, char *hold)
{
	int a;

	if (strlen(hold) == 0) {
		client_gets(sock, buf);
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
				return;
			}
	}
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
 * This loop gets called once for every HTTP connection made to WebCit.
 */
void *context_loop(int sock)
{
	char (*req)[256];
	char buf[256], hold[256];
	char browser_host[256];
	char browser[256];
	int num_lines = 0;
	int a;
	int f;
	int desired_session = 0;
	int got_cookie = 0;
	char str_session[256];
	struct wc_session *sptr;
	struct wc_session *TheSession;
	int ContentLength;
	int CloseSession = 0;

	if ((req = malloc((long) sizeof(char[256][256]))) == NULL) {
		sprintf(buf, "Can't malloc buffers; dropping connection.\n");
		fprintf(stderr, "%s", buf);
		write(sock, buf, strlen(buf));
		close(sock);
		pthread_exit(NULL);
	}
	bzero(req, sizeof(char[256][256]));	/* clear it out */
	strcpy(browser, "unknown");

	printf("Reading request from socket %d\n", sock);

	/*
	 * Find out what it is that the web browser is asking for
	 */
	ContentLength = 0;
	do {
		req_gets(sock, buf, hold);
		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			cookie_to_stuff(&buf[15], &desired_session,
				NULL, NULL, NULL, NULL);
			got_cookie = 1;
		}
		else if (!strncasecmp(buf, "Content-length: ", 16)) {
			ContentLength = atoi(&buf[16]);
		}
		else if (!strncasecmp(buf, "User-agent: ", 12)) {
			strcpy(browser, &buf[12]);
		}
		strcpy(&req[num_lines++][0], buf);
	} while (strlen(buf) > 0);


	/*
	 * If requesting a non-root page, there should already be a cookie
	 * set.  If there isn't, the client browser has cookies turned off
	 * (or doesn't support them) and we have to barf & bail.
	 */
	strcpy(buf, &req[0][0]);
	if (!strncasecmp(buf, "GET ", 4)) strcpy(buf, &buf[4]);
	else if (!strncasecmp(buf, "HEAD ", 5)) strcpy(buf, &buf[5]);
	if (buf[1]==' ') buf[1]=0;
	if ( (strcmp(buf, "/")) && (got_cookie == 0)) {
		strcpy(&req[0][0], "GET /static/nocookies.html HTTP/1.0");
	}


	/*
	 * See if there's an existing session open with the desired ID
	 */
	TheSession = NULL;
	if (desired_session != 0) {
		pthread_mutex_lock(&MasterCritter);
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
			if (sptr->session_id == desired_session) {
				TheSession = sptr;
				lock_session(TheSession);
			}
		}
		pthread_mutex_unlock(&MasterCritter);
	}
	/*
	 * Before we trumpet to the universe that the session we're looking
	 * for actually exists, check first to make sure it's still there.
	 */
	if (TheSession != NULL) {
		if (kill(TheSession->webcit_pid, 0)) {
			printf("   Session is *DEAD* !!\n");
			remove_session(TheSession, 1);
			TheSession = NULL;
		}
	}
	/*
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		printf("Creating a new session\n");
		locate_host(browser_host, sock);
		pthread_mutex_lock(&MasterCritter);
		TheSession = (struct wc_session *)
		    malloc(sizeof(struct wc_session));
		TheSession->session_id = GenerateSessionID();
		pipe(TheSession->inpipe);
		pipe(TheSession->outpipe);
		pthread_mutex_init(&TheSession->critter, NULL);
		lock_session(TheSession);
		TheSession->next = SessionList;
		SessionList = TheSession;
		pthread_mutex_unlock(&MasterCritter);
		sprintf(str_session, "%d", TheSession->session_id);
		f = fork();
		if (f > 0)
			TheSession->webcit_pid = f;

		fflush(stdout);
		fflush(stdin);
		if (f == 0) {

			/* Hook stdio to the ends of the pipe we're using */
			dup2(TheSession->inpipe[0], 0);
			dup2(TheSession->outpipe[1], 1);

			/* Close the ends of the pipes that we're not using */
			close(TheSession->inpipe[1]);
			close(TheSession->outpipe[0]);

			/* Close the HTTP socket in this pid; don't need it */
			close(sock);

			/* Run the actual WebCit session */
			execlp("./webcit", "webcit", str_session, defaulthost,
			       defaultport, browser_host, browser, NULL);

			/* Simple page to display if exec fails */
			printf("HTTP/1.0 404 WebCit Failure\n\n");
			printf("Server: %s\n", SERVER);
			printf("X-WebCit-Session: close\n");
			printf("Content-type: text/html\n");
			printf("Content-length: 76\n");
			printf("\n");
			printf("<HTML><HEAD><TITLE>Error</TITLE></HEAD><BODY>\n");
			printf("execlp() failed: %s</BODY></HTML>\n", strerror(errno));
			exit(0);
		} else {
			/* Close the ends of the pipes that we're not using */
			close(TheSession->inpipe[0]);
			close(TheSession->outpipe[1]);
		}
	}
	/* 
	 * Send the request to the appropriate session...
	 */
	TheSession->lastreq = time(NULL);
	printf("   Writing %d lines of command\n", num_lines);
	printf("%s\n", &req[0][0]);
	for (a = 0; a < num_lines; ++a) {
		write(TheSession->inpipe[1], &req[a][0], strlen(&req[a][0]));
		write(TheSession->inpipe[1], "\n", 1);
	}
	printf("   Writing %d bytes of content\n", ContentLength);
	while (ContentLength > 0) {
		a = ContentLength;
		if (a > sizeof buf)
			a = sizeof buf;
		if (!client_read(sock, buf, a))
			goto end;
		if (write(TheSession->inpipe[1], buf, a) != a)
			goto end;
		ContentLength -= a;
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

	printf("   Reading %d bytes of content\n", ContentLength);

	while (ContentLength--) {
		read(TheSession->outpipe[0], buf, 1);
		write(sock, buf, 1);
	}

	/*
	 * If the last response included a "close session" directive,
	 * remove the context now.
	 */
	if (CloseSession) {
		remove_session(TheSession, 1);
	} else {
end:		unlock_session(TheSession);
	}
	free(req);


	/*
	 * Now our HTTP connection is done.  It would be relatively easy
	 * to support HTTP/1.1 "persistent" connections by looping back to
	 * the top of this function.  For now, we'll just close.
	 */
	printf("   Closing socket %d ... ret=%d\n", sock,
	       lingering_close(sock));

	/*
	 * The thread handling this HTTP connection is now finished.
	 * Instead of calling pthread_exit(), just return. It does the same
	 * thing, and supresses a compiler warning.
	 */
	return NULL;
}
