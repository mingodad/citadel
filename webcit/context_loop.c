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

struct wcsession *SessionList = NULL; /**< our sessions ????*/

pthread_key_t MyConKey;         /**< TSD key for MySession() */


/*
 * free the memory used for viewing atachments
 */
void free_attachment(void *vattach) {
	wc_attachment *att = (wc_attachment*) vattach;
	FreeStrBuf(&att->content_type);
	FreeStrBuf(&att->filename);
	free(att->data);
	free(att);
}


void DestroySession(struct wcsession **sessions_to_kill)
{
	close((*sessions_to_kill)->serv_sock);
	close((*sessions_to_kill)->chat_sock);
//		if ((*sessions_to_kill)->preferences != NULL) {
//			free((*sessions_to_kill)->preferences);
//		}
	if ((*sessions_to_kill)->cache_fold != NULL) {
		free((*sessions_to_kill)->cache_fold);
	}
	DeleteHash(&((*sessions_to_kill)->attachments));
	free_march_list((*sessions_to_kill));
	DeleteHash(&((*sessions_to_kill)->hash_prefs));
	DeleteHash(&((*sessions_to_kill)->IconBarSetttings));
	DeleteHash(&((*sessions_to_kill)->ServCfg));
	FreeStrBuf(&((*sessions_to_kill)->UrlFragment1));
	FreeStrBuf(&((*sessions_to_kill)->UrlFragment2));
	FreeStrBuf(&((*sessions_to_kill)->WBuf));
	FreeStrBuf(&((*sessions_to_kill)->HBuf));
	FreeStrBuf(&((*sessions_to_kill)->CLineBuf));
	free((*sessions_to_kill));
	(*sessions_to_kill) = NULL;
}

void shutdown_sessions(void)
{
	struct wcsession *sptr;
	
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
			sptr->killthis = 1;
	}
}

void do_housekeeping(void)
{
	struct wcsession *sptr, *ss;
	struct wcsession *sessions_to_kill = NULL;
	int num_sessions = 0;
	static int num_threads = MIN_WORKER_THREADS;

	/**
	 * Lock the session list, moving any candidates for euthanasia into
	 * a separate list.
	 */
	pthread_mutex_lock(&SessionListMutex);
	num_sessions = 0;
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
		++num_sessions;

		/** Kill idle sessions */
		if ((time(NULL) - (sptr->lastreq)) >
		   (time_t) WEBCIT_TIMEOUT) {
			sptr->killthis = 1;
		}

		/** Remove sessions flagged for kill */
		if (sptr->killthis) {

			/** remove session from linked list */
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

	/**
	 * Now free up and destroy the culled sessions.
	 */
	while (sessions_to_kill != NULL) {
		lprintf(3, "Destroying session %d\n", sessions_to_kill->wc_session);
		pthread_mutex_lock(&sessions_to_kill->SessionMutex);
		pthread_mutex_unlock(&sessions_to_kill->SessionMutex);
		sptr = sessions_to_kill->next;

		DestroySession(&sessions_to_kill);
		sessions_to_kill = sptr;
		--num_sessions;
	}

	/**
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


/**
 * \brief Wake up occasionally and clean house
 */
void housekeeping_loop(void)
{
	while (1) {
		sleeeeeeeeeep(HOUSEKEEPING);
		do_housekeeping();
	}
}


/**
 * \brief Create a Session id
 * Generate a unique WebCit session ID (which is not the same thing as the
 * Citadel session ID).
 *
 * \todo FIXME ... ensure that session number is truly unique
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
////int req_gets(int *sock, char *buf, char *hold, size_t hlen)
////{
////	int a, b;
////
////	if (IsEmptyStr(hold)) {
////		strcpy(buf, "");
////		a = client_getln(sock, buf, SIZ);
////		if (a<1) return(-1);
////	} else {
////		safestrncpy(buf, hold, SIZ);
////	}
////	strcpy(hold, "");
////
////	if (!strncasecmp(buf, "Cookie: ", 8)) {
////		int len;
////		len = strlen(buf);
////		for (a = 0; a < len; ++a)
////			if (buf[a] == ';') {
////				// we don't refresh len, because of we 
////				// only exit from here.
////				snprintf(hold, hlen, "Cookie: %s", &buf[a + 1]);
////				buf[a] = 0;
////				b = 8;
////				while (isspace(hold[b]))
////					b++;
////				
////				memmove(&hold[8], &hold[b], len - b + 1);
////				return(0);
////			}
////	}
////
////	return(0);
////}


/*
 * Collapse multiple cookies on one line
 */
int ReqGetStrBuf(int *sock, StrBuf *Target, StrBuf *buf)
{
	
	return ClientGetLine(sock, Target, buf);
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



/**
 * \brief	sanity requests
 *		Check for bogus requests coming from brain-dead Windows boxes.
 *
 * \param	http_cmd	The HTTP request to check
 */
int is_bogus(StrBuf *http_cmd) {
	const char *url;
	int i, max;

	url = ChrPtr(http_cmd);
	if (IsEmptyStr(url)) return(1);
	++url;

	char *bogus_prefixes[] = {
		"/scripts/root.exe",	/**< Worms and trojans and viruses, oh my! */
		"/c/winnt",
		"/MSADC/",
		"/_vti",		/**< Broken Microsoft DAV implementation */
		"/MSOffice"		/**< Stoopid MSOffice thinks everyone is IIS */
	};

	max = sizeof(bogus_prefixes) / sizeof(char *);

	for (i=0; i<max; ++i) {
		if (!strncasecmp(url, bogus_prefixes[i], strlen(bogus_prefixes[i]))) {
			return(2);
		}
	}

	return(0);	/* probably ok */
}


const char *nix(void *vptr) {return ChrPtr( (StrBuf*)vptr);}

/**
 * \brief handle one request
 * This loop gets called once for every HTTP connection made to WebCit.  At
 * this entry point we have an HTTP socket with a browser allegedly on the
 * other end, but we have not yet bound to a WebCit session.
 *
 * The job of this function is to locate the correct session and bind to it,
 * or create a session if necessary and bind to it, then run the WebCit
 * transaction loop.  Afterwards, we unbind from the session.  When this
 * function returns, the worker thread is then free to handle another
 * transaction.
 * \param sock the socket we will put our answer to
 */
void context_loop(int *sock)
{
	const char *buf;
	int desired_session = 0;
	int got_cookie = 0;
	int gzip_ok = 0;
	struct wcsession *TheSession, *sptr;
	char httpauth_string[1024];
	char httpauth_user[1024];
	char httpauth_pass[1024];
	char *ptr = NULL;
	int session_is_new = 0;
	int nLine = 0;
	int LineLen;
	void *vLine;
	StrBuf *Buf, *Line, *LastLine, *HeaderName, *ReqLine, *ReqType, *HTTPVersion;
	StrBuf *accept_language = NULL;
	const char *pch, *pchs, *pche;
	HashList *HTTPHeaders;

	strcpy(httpauth_string, "");
	strcpy(httpauth_user, DEFAULT_HTTPAUTH_USER);
	strcpy(httpauth_pass, DEFAULT_HTTPAUTH_PASS);

	/**
	 * Find out what it is that the web browser is asking for
	 */
	HeaderName = NewStrBuf();
	Buf = NewStrBuf();
	LastLine = NULL;
	HTTPHeaders = NewHash(1, NULL);
	/**
	 * Read in the request
	 */
	do {
		nLine ++;
		Line = NewStrBuf();
		if (ReqGetStrBuf(sock, Line, Buf) < 0) return;

		LineLen = StrLength(Line);

		if (nLine == 1) {
			ReqLine = Line;
			continue;
		}
		if (LineLen == 0) {
			FreeStrBuf(&Line);
			continue;
		}

		/** Do we need to Unfold? */
		if ((LastLine != NULL) && 
		    (isspace(*ChrPtr(Line)))) {
			pch = pchs = ChrPtr(Line);
			pche = pchs + StrLength(Line);
			while (isspace(*pch) && (pch < pche))
				pch ++;
			StrBufCutLeft(Line, pch - pchs);
			StrBufAppendBuf(LastLine, Line, 0);
			FreeStrBuf(&Line);
			continue;
		}

		StrBufExtract_token(HeaderName, Line, 0, ':');
	//// TODO: filter bad chars!

		pchs = ChrPtr(Line);
		pch = pchs + StrLength(HeaderName) + 1;
		pche = pchs + StrLength(Line);
		while (isspace(*pch) && (pch < pche))
			pch ++;
		StrBufCutLeft(Line, pch - pchs);

		StrBufUpCase(HeaderName);
		Put(HTTPHeaders, SKEY(HeaderName), Line, HFreeStrBuf);
		LastLine = Line;
	} while (LineLen > 0);
	FreeStrBuf(&HeaderName);

////	dbg_PrintHash(HTTPHeaders, nix, NULL);


	/**
	 * Can we compress?
	 */
	if (GetHash(HTTPHeaders, HKEY("ACCEPT-ENCODING"), &vLine) && 
	    (vLine != NULL)) {
		buf = ChrPtr((StrBuf*)vLine);
		if (strstr(&buf[16], "gzip")) {
			gzip_ok = 1;
		}
	}

	/**
	 * Browser-based sessions use cookies for session 

authentication
	 */
	if (GetHash(HTTPHeaders, HKEY("COOKIE"), &vLine) && 
	    (vLine != NULL)) {
		cookie_to_stuff(vLine, &desired_session,
				NULL, 0, NULL, 0, NULL, 0);
		got_cookie = 1;
	}

	/**
	 * GroupDAV-based sessions use HTTP authentication
	 */
	if (GetHash(HTTPHeaders, HKEY("AUTHORIZATION"), &vLine) && 
	    (vLine != NULL)) {
		Line = (StrBuf*)vLine;
		if (strncasecmp(ChrPtr(Line), "Basic", 5) == 0) {
			StrBufCutLeft(Line, 6);
			CtdlDecodeBase64(httpauth_string, ChrPtr(Line), StrLength(Line));
			extract_token(httpauth_user, httpauth_string, 0, ':', sizeof httpauth_user);
			extract_token(httpauth_pass, httpauth_string, 1, ':', sizeof httpauth_pass);
		}
		else 
			lprintf(1, "Authentication scheme not supported! [%s]\n", ChrPtr(Line));
	}

	if (GetHash(HTTPHeaders, HKEY("IF-MODIFIED-SINCE"), &vLine) && 
	    (vLine != NULL)) {
		if_modified_since = httpdate_to_timestamp((StrBuf*)vLine);
	}

	if (GetHash(HTTPHeaders, HKEY("ACCEPT-LANGUAGE"), &vLine) && 
	    (vLine != NULL)) {
		accept_language = (StrBuf*) vLine;
	}


	/**
	 * If the request is prefixed by "/webcit" then chop that off.  This
	 * allows a front end web server to forward all /webcit requests to us
	 * while still using the same web server port for other things.
	 */

	ReqType = NewStrBuf();
	HTTPVersion = NewStrBuf();
	StrBufExtract_token(HTTPVersion, ReqLine, 2, ' ');
	StrBufExtract_token(ReqType, ReqLine, 0, ' ');
	StrBufCutLeft(ReqLine, StrLength(ReqType) + 1);
	StrBufCutRight(ReqLine, StrLength(HTTPVersion) + 1);

	if ((follow_xff == 1) && (StrLength(ReqLine) >= 8) &&
	    (ptr = strstr(ChrPtr(ReqLine), "/webcit/"),	/*< Handle "/webcit/" */
	     (ptr != NULL))) {
		StrBufCutLeft(ReqLine, 7);
	}

	/** Begin parsing the request. */
#ifdef TECH_PREVIEW
	if ((strncmp(ChrPtr(ReqLine), "/sslg", 5) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/static/", 8) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/tiny_mce/", 10) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/wholist_section", 16) != 0) &&
	    (strstr(ChrPtr(ReqLine), "wholist_section") == NULL)) {
#endif
		lprintf(5, "HTTP: %s %s %s\n", ChrPtr(ReqType), ChrPtr(ReqLine), ChrPtr(HTTPVersion));
#ifdef TECH_PREVIEW
	}
#endif

	/** Check for bogus requests */
	if ((StrLength(HTTPVersion) == 0) ||
	    (StrLength(ReqType) == 0) || 
	    is_bogus(ReqLine)) {
		StrBufPlain(ReqLine, HKEY("/404 HTTP/1.1"));
		StrBufPlain(ReqType, HKEY("GET"));
	}
	FreeStrBuf(&HTTPVersion);

	/**
	 * While we're at it, gracefully handle requests for the
	 * robots.txt and favicon.ico files.
	 */
	if (!strncasecmp(ChrPtr(ReqLine), "/robots.txt", 11)) {
		StrBufPlain(ReqLine, 
			    HKEY("/static/robots.txt"
				 "?force_close_session=yes HTTP/1.1"));
		StrBufPlain(ReqType, HKEY("GET"));
	}
	else if (!strncasecmp(ChrPtr(ReqLine), "/favicon.ico", 12)) {
		StrBufPlain(ReqLine, HKEY("/static/favicon.ico"));
		StrBufPlain(ReqType, HKEY("GET"));
	}

	/**
	 * These are the URL's which may be executed without a
	 * session cookie already set.  If it's not one of these,
	 * force the session to close because cookies are
	 * probably disabled on the client browser.
	 */
	else if ( (StrLength(ReqLine) > 1 )
		&& (strncasecmp(ChrPtr(ReqLine), "/listsub", 8))
		&& (strncasecmp(ChrPtr(ReqLine), "/freebusy", 9))
		&& (strncasecmp(ChrPtr(ReqLine), "/do_logout", 10))
		&& (strncasecmp(ChrPtr(ReqLine), "/groupdav", 9))
		&& (strncasecmp(ChrPtr(ReqLine), "/static", 7))
		&& (strncasecmp(ChrPtr(ReqLine), "/rss", 4))
		&& (strncasecmp(ChrPtr(ReqLine), "/404", 4))
	        && (got_cookie == 0)) {
		StrBufPlain(ReqLine, 
			    HKEY("/static/nocookies.html"
				 "?force_close_session=yes"));
	}

	/**
	 * See if there's an existing session open with the desired ID or user/pass
	 */
	TheSession = NULL;

	if (TheSession == NULL) {
		pthread_mutex_lock(&SessionListMutex);
		for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {

			/** If HTTP-AUTH, look for a session with matching credentials */
			if ( (!IsEmptyStr(httpauth_user))
			   &&(!strcasecmp(sptr->httpauth_user, httpauth_user))
			   &&(!strcasecmp(sptr->httpauth_pass, httpauth_pass)) ) {
				TheSession = sptr;
			}

			/** If cookie-session, look for a session with matching session ID */
			if ( (desired_session != 0) && (sptr->wc_session == desired_session)) {
				TheSession = sptr;
			}

		}
		pthread_mutex_unlock(&SessionListMutex);
	}

	/**
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		lprintf(3, "Creating a new session\n");
		TheSession = (struct wcsession *)
			malloc(sizeof(struct wcsession));
		memset(TheSession, 0, sizeof(struct wcsession));
		TheSession->serv_sock = (-1);
		TheSession->chat_sock = (-1);
	
		/* If we're recreating a session that expired, it's best to give it the same
		 * session number that it had before.  The client browser ought to pick up
		 * the new session number and start using it, but in some rare situations it
		 * doesn't, and that's a Bad Thing because it causes lots of spurious sessions
		 * to get created.
		 */	
		if (desired_session == 0) {
			TheSession->wc_session = GenerateSessionID();
		}
		else {
			TheSession->wc_session = desired_session;
		}

		strcpy(TheSession->httpauth_user, httpauth_user);
		strcpy(TheSession->httpauth_pass, httpauth_pass);
		TheSession->hash_prefs = NewHash(1,NULL);	/* Get a hash table for the user preferences */
		pthread_mutex_init(&TheSession->SessionMutex, NULL);
		pthread_mutex_lock(&SessionListMutex);
		TheSession->urlstrings = NULL;
		TheSession->vars = NULL;
		TheSession->nonce = rand();
		TheSession->WBuf = NULL;
		TheSession->CLineBuf = NewStrBuf();
		TheSession->next = SessionList;
		TheSession->is_mobile = -1;
		SessionList = TheSession;
		pthread_mutex_unlock(&SessionListMutex);
		session_is_new = 1;
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
	
	TheSession->urlstrings = NewHash(1,NULL);
	TheSession->vars = NewHash(1,NULL);
	TheSession->http_sock = *sock;
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->gzip_ok = gzip_ok;
#ifdef ENABLE_NLS
	if (session_is_new) {
		httplang_to_locale(accept_language);
	}
	go_selected_language();					/* set locale */
#endif
	session_loop(HTTPHeaders, ReqLine, ReqType, Buf);				/* do transaction */
#ifdef ENABLE_NLS
	stop_selected_language();				/* unset locale */
#endif
	DeleteHash(&TheSession->summ);
	DeleteHash(&TheSession->urlstrings);
	DeleteHash(&TheSession->vars);
	FreeStrBuf(&TheSession->WBuf);
	FreeStrBuf(&TheSession->HBuf);
	
	
	pthread_mutex_unlock(&TheSession->SessionMutex);	/* unbind */

	/* Free the request buffer */
	DeleteHash(&HTTPHeaders);
	FreeStrBuf(&ReqLine);
	FreeStrBuf(&ReqType);
	FreeStrBuf(&Buf);
	/*
	 * Free up any session-local substitution variables which
	 * were set during this transaction
	 */
	
	
}

void tmpl_nonce(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	struct wcsession *WCC = WC;
	StrBufAppendPrintf(Target, "%ld",
			   (WCC != NULL)? WCC->nonce:0);		   
}

void 
InitModule_CONTEXT
(void)
{
	RegisterNamespace("NONCE", 0, 0, tmpl_nonce, 0);
}
