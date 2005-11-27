/*
 * $Id$
 *
 * This is the other half of the webserver.  It handles the task of hooking
 * up HTTP requests with the sessions they belong to, using HTTP cookies to
 * keep track of things.  If the HTTP request doesn't belong to any currently
 * active session, a new session is started.
 *
 */

#include "webcit.h"
#include "webserver.h"

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t SessionListMutex;

struct wcsession *SessionList = NULL;

pthread_key_t MyConKey;                         /* TSD key for MySession() */


void free_attachments(struct wcsession *sess) {
	struct wc_attachment *att;

	while (sess->first_attachment != NULL) {
		att = sess->first_attachment;
		sess->first_attachment = sess->first_attachment->next;
		free(att->data);
		free(att);
	}
}


void do_housekeeping(void)
{
	struct wcsession *sptr, *ss;
	struct wcsession *sessions_to_kill = NULL;
	int num_sessions = 0;
	static int num_threads = MIN_WORKER_THREADS;

	/*
	 * Lock the session list, moving any candidates for euthanasia into
	 * a separate list.
	 */
	pthread_mutex_lock(&SessionListMutex);
	num_sessions = 0;
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
		++num_sessions;

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

			sptr->next = sessions_to_kill;
			sessions_to_kill = sptr;
		}
	}
	pthread_mutex_unlock(&SessionListMutex);

	/*
	 * Now free up and destroy the culled sessions.
	 */
	while (sessions_to_kill != NULL) {
		lprintf(3, "Destroying session %d\n", sessions_to_kill->wc_session);
		pthread_mutex_lock(&sessions_to_kill->SessionMutex);
		close(sessions_to_kill->serv_sock);
		close(sessions_to_kill->chat_sock);
		if (sessions_to_kill->preferences != NULL) {
			free(sessions_to_kill->preferences);
		}
		free_attachments(sessions_to_kill);
		pthread_mutex_unlock(&sessions_to_kill->SessionMutex);
		sptr = sessions_to_kill->next;
		free(sessions_to_kill);
		sessions_to_kill = sptr;
		--num_sessions;
	}

	/*
	 * If there are more sessions than threads, then we should spawn
	 * more threads ... up to a predefined maximum.
	 */
	while ( (num_sessions > num_threads)
	      && (num_threads <= MAX_WORKER_THREADS) ) {
		spawn_another_worker_thread();
		++num_threads;
		lprintf(3, "There are %d sessions and %d threads active.\n",
			num_sessions, num_threads);
	}
}


/* 
 * Wake up occasionally and clean house
 */
void housekeeping_loop(void)
{
	while (1) {
		sleeeeeeeeeep(HOUSEKEEPING);
		do_housekeeping();
	}
}


/*
 * Generate a unique WebCit session ID (which is not the same thing as the
 * Citadel session ID).
 *
 * FIXME ... ensure that session number is truly unique
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
		a = client_getln(sock, buf, SIZ);
		if (a<1) return(-1);
	} else {
		safestrncpy(buf, hold, SIZ);
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

int lingering_close(int fd)
{
	char buf[SIZ];
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
 * Check for bogus requests coming from (for example) brain-dead
 * Windoze boxes that are infected with the latest worm-of-the-week.
 * If we detect one of these, bail out without bothering our Citadel
 * server.
 */
int is_bogus(char *http_cmd) {

	if (!strncasecmp(http_cmd, "GET /scripts/root.exe", 21)) return(1);
	if (!strncasecmp(http_cmd, "GET /c/winnt", 12)) return(2);
	if (!strncasecmp(http_cmd, "GET /MSADC/", 11)) return(3);

	return(0);	/* probably ok */
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
	char buf[SIZ], hold[SIZ];
	int desired_session = 0;
	int got_cookie = 0;
	int gzip_ok = 0;
	struct wcsession *TheSession, *sptr;
	char httpauth_string[SIZ];
	char httpauth_user[SIZ];
	char httpauth_pass[SIZ];
	char *ptr = NULL;

	strcpy(httpauth_string, "");
	strcpy(httpauth_user, DEFAULT_HTTPAUTH_USER);
	strcpy(httpauth_pass, DEFAULT_HTTPAUTH_PASS);

	/*
	 * Find out what it is that the web browser is asking for
	 */
	memset(hold, 0, sizeof(hold));
	do {
		if (req_gets(sock, buf, hold) < 0) return;

		/*
		 * Can we compress?
		 */
		if (!strncasecmp(buf, "Accept-encoding:", 16)) {
			if (strstr(&buf[16], "gzip")) {
				gzip_ok = 1;
			}
		}

		/*
		 * Browser-based sessions use cookies for session authentication
		 */
		if (!strncasecmp(buf, "Cookie: webcit=", 15)) {
			cookie_to_stuff(&buf[15], &desired_session,
				NULL, 0, NULL, 0, NULL, 0);
			got_cookie = 1;
		}

		/*
		 * GroupDAV-based sessions use HTTP authentication
		 */
		if (!strncasecmp(buf, "Authorization: Basic ", 21)) {
			CtdlDecodeBase64(httpauth_string, &buf[21], strlen(&buf[21]));
			extract_token(httpauth_user, httpauth_string, 0, ':', sizeof httpauth_user);
			extract_token(httpauth_pass, httpauth_string, 1, ':', sizeof httpauth_pass);
		}

		if (!strncasecmp(buf, "If-Modified-Since: ", 19)) {
			if_modified_since = httpdate_to_timestamp(&buf[19]);
		}

		if (!strncasecmp(buf, "Accept-Language: ", 17)) {
			httplang_to_locale(&buf[17]);
		}

		/*
		 * Read in the request
		 */
		hptr = (struct httprequest *)
			malloc(sizeof(struct httprequest));
		if (req == NULL)
			req = hptr;
		else
			last->next = hptr;
		hptr->next = NULL;
		last = hptr;

		safestrncpy(hptr->line, buf, sizeof hptr->line);

	} while (strlen(buf) > 0);

	/*
	 * If the request is prefixed by "/webcit" then chop that off.  This
	 * allows a front end web server to forward all /webcit requests to us
	 * while still using the same web server port for other things.
	 */
	
	ptr = strstr(req->line, " /webcit ");	/* Handle "/webcit" */
	if (ptr != NULL) {
		strcpy(ptr+2, ptr+8);
	}

	ptr = strstr(req->line, " /webcit");	/* Handle "/webcit/" */
	if (ptr != NULL) {
		strcpy(ptr+1, ptr+8);
	}

	/* Begin parsing the request. */

	safestrncpy(buf, req->line, sizeof buf);
	lprintf(5, "HTTP: %s\n", buf);

	/* Check for bogus requests */
	if (is_bogus(buf)) goto bail;

	/*
	 * Strip out the method, leaving the URL up front...
	 */
	remove_token(buf, 0, ' ');
	if (buf[1]==' ') buf[1]=0;

	/*
	 * While we're at it, gracefully handle requests for the
	 * robots.txt and favicon.ico files.
	 */
	if (!strncasecmp(buf, "/robots.txt", 11)) {
		strcpy(req->line, "GET /static/robots.txt"
				"?force_close_session=yes HTTP/1.1");
	}
	else if (!strncasecmp(buf, "/favicon.ico", 12)) {
		strcpy(req->line, "GET /static/favicon.ico");
	}

	/* These are the URL's which may be executed without a
	 * session cookie already set.  If it's not one of these,
	 * force the session to close because cookies are
	 * probably disabled on the client browser.
	 */
	else if ( (strcmp(buf, "/"))
		&& (strncasecmp(buf, "/listsub", 8))
		&& (strncasecmp(buf, "/freebusy", 9))
		&& (strncasecmp(buf, "/do_logout", 10))
		&& (strncasecmp(buf, "/groupdav", 9))
		&& (strncasecmp(buf, "/static", 7))
		&& (strncasecmp(buf, "/rss", 4))
	        && (got_cookie == 0)) {
		strcpy(req->line, "GET /static/nocookies.html"
				"?force_close_session=yes HTTP/1.1");
	}

	/*
	 * See if there's an existing session open with the desired ID or user/pass
	 */
	TheSession = NULL;

	if (TheSession == NULL) {
		pthread_mutex_lock(&SessionListMutex);
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {

			/* If HTTP-AUTH, look for a session with matching credentials */
			if ( (strlen(httpauth_user) > 0)
			   &&(!strcasecmp(sptr->httpauth_user, httpauth_user))
			   &&(!strcasecmp(sptr->httpauth_pass, httpauth_pass)) ) {
				TheSession = sptr;
			}

			/* If cookie-session, look for a session with matching session ID */
			if ( (desired_session != 0) && (sptr->wc_session == desired_session)) {
				TheSession = sptr;
			}

		}
		pthread_mutex_unlock(&SessionListMutex);
	}

	/*
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		lprintf(3, "Creating a new session\n");
		TheSession = (struct wcsession *)
			malloc(sizeof(struct wcsession));
		memset(TheSession, 0, sizeof(struct wcsession));
		TheSession->serv_sock = (-1);
		TheSession->chat_sock = (-1);
		TheSession->wc_session = GenerateSessionID();
		strcpy(TheSession->httpauth_user, httpauth_user);
		strcpy(TheSession->httpauth_pass, httpauth_pass);
		pthread_mutex_init(&TheSession->SessionMutex, NULL);

		pthread_mutex_lock(&SessionListMutex);
		TheSession->next = SessionList;
		SessionList = TheSession;
		pthread_mutex_unlock(&SessionListMutex);
	}

	/*
	 * A future improvement might be to check the session integrity
	 * at this point before continuing.
	 */

	/*
	 * Bind to the session and perform the transaction
	 */
	pthread_mutex_lock(&TheSession->SessionMutex);		/* bind */
	pthread_setspecific(MyConKey, (void *)TheSession);
	TheSession->http_sock = sock;
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->gzip_ok = gzip_ok;
	session_loop(req);				/* do transaction */
	pthread_mutex_unlock(&TheSession->SessionMutex);	/* unbind */

	/* Free the request buffer */
bail:	while (req != NULL) {
		hptr = req->next;
		free(req);
		req = hptr;
	}

	/* Free up any session-local substitution variables which
	 * were set during this transaction
	 */
	clear_local_substs();
}
