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
#include "modules_init.h"

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t SessionListMutex;

wcsession *SessionList = NULL; /**< our sessions ????*/

pthread_key_t MyConKey;         /**< TSD key for MySession() */
HashList *HttpReqTypes = NULL;
HashList *HttpHeaderHandler = NULL;
extern HashList *HandlerHash;

void DestroyHttpHeaderHandler(void *V)
{
	OneHttpHeader *pHdr;
	pHdr = (OneHttpHeader*) V;
	FreeStrBuf(&pHdr->Val);
	free(pHdr);
}

void shutdown_sessions(void)
{
	wcsession *sptr;
	
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
			sptr->killthis = 1;
	}
}

void do_housekeeping(void)
{
	wcsession *sptr, *ss;
	wcsession *sessions_to_kill = NULL;
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

		session_destroy_modules(&sessions_to_kill);
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
 * Create a Session id
 * Generate a unique WebCit session ID (which is not the same thing as the
 * Citadel session ID).
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
 * \brief Detects a 'mobile' user agent 
 */
int is_mobile_ua(char *user_agent) {
      if (strstr(user_agent,"iPhone OS") != NULL) {
	return 1;
      } else if (strstr(user_agent,"Windows CE") != NULL) {
	return 1;
      } else if (strstr(user_agent,"SymbianOS") != NULL) {
	return 1;
      } else if (strstr(user_agent, "Opera Mobi") != NULL) {
	return 1;
      } else if (strstr(user_agent, "Firefox/2.0.0 Opera 9.51 Beta") != NULL) {
	      /*  For some reason a new install of Opera 9.51beta decided to spoof. */
	  return 1;
	  }
      return 0;
}



/*
 * Look for commonly-found probes of malware such as worms, viruses, trojans, and Microsoft Office.
 * Short-circuit these requests so we don't have to send them through the full processing loop.
 */
int is_bogus(StrBuf *http_cmd) {////TODO!
	const char *url;
	int i, max;
	const char *bogus_prefixes[] = {
		"/scripts/root.exe",	/* Worms and trojans and viruses, oh my! */
		"/c/winnt",
		"/MSADC/",
		"/_vti",		/* Broken Microsoft DAV implementation */
		"/MSOffice",		/* Stoopid MSOffice thinks everyone is IIS */
		"/nonexistenshit"	/* Exploit found in the wild January 2009 */
	};

	url = ChrPtr(http_cmd);
	if (IsEmptyStr(url)) return(1);
	++url;

	max = sizeof(bogus_prefixes) / sizeof(char *);

	for (i=0; i<max; ++i) {
		if (!strncasecmp(url, bogus_prefixes[i], strlen(bogus_prefixes[i]))) {
			return(2);
		}
	}

	return(0);	/* probably ok */
}


int ReadHttpSubject(ParsedHttpHdrs *Hdr, StrBuf *Line, StrBuf *Buf)
{
	const char *Args;
	void *vLine, *vHandler;
	const char *Pos = NULL;


	Hdr->ReqLine = Line;
	/* The requesttype... GET, POST... */
	StrBufExtract_token(Buf, Hdr->ReqLine, 0, ' ');
	if (GetHash(HttpReqTypes, SKEY(Buf), &vLine) &&
	    (vLine != NULL))
	{
		Hdr->eReqType = *(long*)vLine;
	}
	else {
		Hdr->eReqType = eGET;
		return 1;
	}
	StrBufCutLeft(Hdr->ReqLine, StrLength(Buf) + 1);

	/* the HTTP Version... */
	StrBufExtract_token(Buf, Hdr->ReqLine, 1, ' ');
	StrBufCutRight(Hdr->ReqLine, StrLength(Buf) + 1);
	if ((StrLength(Buf) == 0) ||
	    is_bogus(Hdr->ReqLine)) {
		Hdr->eReqType = eGET;
		return 1;
	}

	Hdr->this_page = NewStrBufDup(Hdr->ReqLine);
	/* chop Filename / query arguments */
	Args = strchr(ChrPtr(Hdr->ReqLine), '?');
	if (Args == NULL) /* whe're not that picky about params... TODO: this will spoil '&' in filenames.*/
		Args = strchr(ChrPtr(Hdr->ReqLine), '&');
	if (Args != NULL) {
		Args ++; /* skip the ? */
		Hdr->PlainArgs = NewStrBufPlain(
			Args, 
			StrLength(Hdr->ReqLine) -
			(Args - ChrPtr(Hdr->ReqLine)));
		StrBufCutAt(Hdr->ReqLine, 0, Args - 1);
	} /* don't parse them yet, maybe we don't even care... */
	
	/* now lookup what we are going to do with this... */
	/* skip first slash */
	StrBufExtract_NextToken(Buf, Hdr->ReqLine, &Pos, '/');
	do {
		StrBufExtract_NextToken(Buf, Hdr->ReqLine, &Pos, '/');

		GetHash(HandlerHash, SKEY(Buf), &vHandler),
		Hdr->Handler = (WebcitHandler*) vHandler;
		if (Hdr->Handler == NULL)
			break;
		/* are we about to ignore some prefix like webcit/ ? */
		if ((Hdr->Handler->Flags & URLNAMESPACE) == 0)
			break;
	} while (1);
	/* remove the handlername from the URL */
	if (Pos != NULL) {
		StrBufCutLeft(Hdr->ReqLine, 
			      Pos - ChrPtr(Hdr->ReqLine));
	}
/*
	if (Hdr->Handler == NULL)
		return 1;
*/
	Hdr->HTTPHeaders = NewHash(1, NULL);

	return 0;
}

int AnalyseHeaders(ParsedHttpHdrs *Hdr)
{
	OneHttpHeader *pHdr;
	void *vHdr;
	long HKLen;
	const char *HashKey;
	HashPos *at = GetNewHashPos(Hdr->HTTPHeaders, 0);
	
	while (GetNextHashPos(Hdr->HTTPHeaders, at, &HKLen, &HashKey, &vHdr) && 
	       (vHdr != NULL)) {
		pHdr = (OneHttpHeader *)vHdr;
		if (pHdr->HaveEvaluator)
			pHdr->H(pHdr->Val, Hdr);

	}
	DeleteHashPos(&at);
	return 0;
}

/*const char *nix(void *vptr) {return ChrPtr( (StrBuf*)vptr);}*/

/*
 * Read in the request
 */
int ReadHTTPRequset (ParsedHttpHdrs *Hdr)
{
	const char *pch, *pchs, *pche;
	OneHttpHeader *pHdr;
	StrBuf *Line, *LastLine, *HeaderName;
	int nLine = 0;
	void *vF;
	int isbogus = 0;

	HeaderName = NewStrBuf();
	Hdr->ReadBuf = NewStrBuf();
	LastLine = NULL;
	do {
		nLine ++;
		Line = NewStrBuf();

		if (ClientGetLine(&Hdr->http_sock, Line, Hdr->ReadBuf, &Hdr->Pos) < 0) return 1;

		if (StrLength(Line) == 0) {
			FreeStrBuf(&Line);
			continue;
		}
		if (nLine == 1) {
			isbogus = ReadHttpSubject(Hdr, Line, HeaderName);
			if (isbogus) break;
			continue;
		}

		/* Do we need to Unfold? */
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

		StrBufSanitizeAscii(Line, '§');
		StrBufExtract_token(HeaderName, Line, 0, ':');

		pchs = ChrPtr(Line);
		pch = pchs + StrLength(HeaderName) + 1;
		pche = pchs + StrLength(Line);
		while (isspace(*pch) && (pch < pche))
			pch ++;
		StrBufCutLeft(Line, pch - pchs);

		StrBufUpCase(HeaderName);

		pHdr = (OneHttpHeader*) malloc(sizeof(OneHttpHeader));
		memset(pHdr, 0, sizeof(OneHttpHeader));
		pHdr->Val = Line;

		if (GetHash(HttpHeaderHandler, SKEY(HeaderName), &vF) &&
		    (vF != NULL))
		{
			OneHttpHeader *FHdr = (OneHttpHeader*) vF;
			pHdr->H = FHdr->H;
			pHdr->HaveEvaluator = 1;
		}
		Put(Hdr->HTTPHeaders, SKEY(HeaderName), pHdr, DestroyHttpHeaderHandler);
		LastLine = Line;
	} while (Line != NULL);

	FreeStrBuf(&HeaderName);

	return isbogus;
}



/*
 * handle one request
 *
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
void context_loop(int *sock)
{
	ParsedHttpHdrs Hdr;
	int isbogus = 0;
	wcsession *TheSession, *sptr;
	struct timeval tx_start;
	struct timeval tx_finish;
	
	gettimeofday(&tx_start, NULL);		/* start a stopwatch for performance timing */

	memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
	Hdr.eReqType = eGET;
	Hdr.http_sock = *sock;
	/*
	 * Find out what it is that the web browser is asking for
	 */
	isbogus = ReadHTTPRequset(&Hdr);

	if (!isbogus)
		isbogus = AnalyseHeaders(&Hdr);
/*
	if (isbogus)
		StrBufPlain(ReqLine, HKEY("/404"));
*/

/*	dbg_PrintHash(HTTPHeaders, nix, NULL);  */

	/*
	 * If the request is prefixed by "/webcit" then chop that off.  This
	 * allows a front end web server to forward all /webcit requests to us
	 * while still using the same web server port for other things.
	 * /
	if (!isbogus &&
	    (StrLength(ReqLine) >= 8) && 
	    (strstr(ChrPtr(ReqLine), "/webcit/")) ) {
		StrBufCutLeft(ReqLine, 7);
	}

	/* Begin parsing the request. * /
#ifdef TECH_PREVIEW
	if ((strncmp(ChrPtr(ReqLine), "/sslg", 5) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/static/", 8) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/tiny_mce/", 10) != 0) &&
	    (strncmp(ChrPtr(ReqLine), "/wholist_section", 16) != 0) &&
	    (strstr(ChrPtr(ReqLine), "wholist_section") == NULL)) {
#endif
		lprintf(5, "HTTP: %s %s\n", ReqStrs[Hdr.eReqType], ChrPtr(ReqLine));
#ifdef TECH_PREVIEW
	}
#endif

*/

	/**
	 * See if there's an existing session open with the desired ID or user/pass
	 */
	TheSession = NULL;

	if (TheSession == NULL) {
		pthread_mutex_lock(&SessionListMutex);
		for (sptr = SessionList; 
		     ((sptr != NULL) && (TheSession == NULL)); 
		      sptr = sptr->next) {

			/** If HTTP-AUTH, look for a session with matching credentials * /
			if ( (////TODO check auth type here...
			     &&(!strcasecmp(ChrPtr(sptr->httpauth_user), httpauth_user))
			     &&(!strcasecmp(ChrPtr(sptr->httpauth_pass), httpauth_pass)) ) {
				TheSession = sptr;
			}

			/** If cookie-session, look for a session with matching session ID */
			if ( (Hdr.desired_session != 0) && (sptr->wc_session == Hdr.desired_session)) {
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
		TheSession = (wcsession *)
			malloc(sizeof(wcsession));
		memset(TheSession, 0, sizeof(wcsession));
		TheSession->Hdr = &Hdr;
		TheSession->serv_sock = (-1);
		TheSession->chat_sock = (-1);
	
		/* If we're recreating a session that expired, it's best to give it the same
		 * session number that it had before.  The client browser ought to pick up
		 * the new session number and start using it, but in some rare situations it
		 * doesn't, and that's a Bad Thing because it causes lots of spurious sessions
		 * to get created.
		 */	
		if (Hdr.desired_session == 0) {
			TheSession->wc_session = GenerateSessionID();
		}
		else {
			TheSession->wc_session = Hdr.desired_session;
		}
/*
		TheSession->httpauth_user = NewStrBufPlain(httpauth_user, -1);
			TheSession->httpauth_pass = NewStrBufPlain(httpauth_user, -1);
*/
		pthread_setspecific(MyConKey, (void *)TheSession);
		session_new_modules(TheSession);

		pthread_mutex_init(&TheSession->SessionMutex, NULL);
		pthread_mutex_lock(&SessionListMutex);
		TheSession->nonce = rand();
		TheSession->next = SessionList;
		TheSession->is_mobile = -1;
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
	
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->Hdr = &Hdr;

	session_attach_modules(TheSession);
	session_loop();				/* do transaction */


	/* How long did this transaction take? */
	gettimeofday(&tx_finish, NULL);
	
	lprintf(9, "Transaction [%s] completed in %ld.%06ld seconds.\n",
		ChrPtr(Hdr.this_page),
		((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
		((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000
	);

	session_detach_modules(TheSession);

	TheSession->Hdr = NULL;
	pthread_mutex_unlock(&TheSession->SessionMutex);	/* unbind */


	http_destroy_modules(&Hdr);
/* TODO

	FreeStrBuf(&c_username);
	FreeStrBuf(&c_password);
	FreeStrBuf(&c_roomname);
	FreeStrBuf(&c_httpauth_user);
	FreeStrBuf(&c_httpauth_pass);
*/
	/* Free the request buffer */
	///FreeStrBuf(&ReqLine);
	
}

void tmplput_nonce(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	StrBufAppendPrintf(Target, "%ld",
			   (WCC != NULL)? WCC->nonce:0);		   
}

void tmplput_current_user(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, WC->wc_fullname, 0);
}

void tmplput_current_room(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, WC->wc_roomname, 0); 
}


void Header_HandleCookie(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->RawCookie = Line;
	if (hdr->DontNeedAuth)
		return;
/*
	c_username = NewStrBuf();
	c_password = NewStrBuf();
	c_roomname = NewStrBuf();
	safestrncpy(c_httpauth_string, "", sizeof c_httpauth_string);
	c_httpauth_user = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_USER));
	c_httpauth_pass = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_PASS));
*/
	cookie_to_stuff(Line, &hdr->desired_session,
			hdr->c_username,
			hdr->c_password,
			hdr->c_roomname);
	hdr->got_cookie = 1;
}


	/*
	 * Browser-based sessions use cookies for session authentication
	 * /
	if (!isbogus &&
	    GetHash(HTTPHeaders, HKEY("COOKIE"), &vLine) && 
	    (vLine != NULL)) {
		cookie_to_stuff(vLine, &desired_session,
				NULL, NULL, NULL);
		got_cookie = 1;
	}
	*/
	/*
	 * GroupDAV-based sessions use HTTP authentication
	 */
/*
	if (!isbogus &&
	    GetHash(HTTPHeaders, HKEY("AUTHORIZATION"), &vLine) && 
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

*/
void Header_HandleAuth(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	const char *Pos = NULL;
	StrBufDecodeBase64(Line);
	StrBufExtract_NextToken(hdr->c_username, Line, &Pos, ':');
	StrBufExtract_NextToken(hdr->c_password, Line, &Pos, ':');
}

void Header_HandleContentLength(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->ContentLength = StrToi(Line);
}

void Header_HandleContentType(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->ContentType = Line;
}

void Header_HandleUserAgent(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->user_agent = Line;
#ifdef TECH_PREVIEW
/* TODO: do this later on session creating
	if ((WCC->is_mobile < 0) && is_mobile_ua(&buf[12])) {			
		WCC->is_mobile = 1;
	}
	else {
		WCC->is_mobile = 0;
	}
*/
#endif
}


void Header_HandleHost(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if ((follow_xff) && (hdr->http_host != NULL))
		return;
	else
		hdr->http_host = Line;
}

void Header_HandleXFFHost(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (follow_xff)
		hdr->http_host = Line;
}


void Header_HandleXFF(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->browser_host = Line;

	while (StrBufNum_tokens(hdr->browser_host, ',') > 1) {
		StrBufRemove_token(hdr->browser_host, 0, ',');
	}
	StrBufTrim(hdr->browser_host);
}

void Header_HandleIfModSince(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->if_modified_since = httpdate_to_timestamp(Line);
}

void Header_HandleAcceptEncoding(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	/*
	 * Can we compress?
	 */
	if (strstr(&ChrPtr(Line)[16], "gzip")) {
		hdr->gzip_ok = 1;
	}
}

/*
{
	c_username = NewStrBuf();
	c_password = NewStrBuf();
	c_roomname = NewStrBuf();
	safestrncpy(c_httpauth_string, "", sizeof c_httpauth_string);
	c_httpauth_user = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_USER));
	c_httpauth_pass = NewStrBufPlain(HKEY(DEFAULT_HTTPAUTH_PASS));
}
*/
	/* *
	 * These are the URL's which may be executed without a
	 * session cookie already set.  If it's not one of these,
	 * force the session to close because cookies are
	 * probably disabled on the client browser.
	 * /
	else if ( (StrLength(ReqLine) > 1 )
		&& (strncasecmp(ChrPtr(ReqLine), "/404", 4))
	        && (Hdr.got_cookie == 0)) {
		StrBufPlain(ReqLine, 
			    HKEY("/static/nocookies.html"
				 "?force_close_session=yes"));
	}
*/
const char *ReqStrs[eNONE] = {
	"GET",
	"POST",
	"OPTIONS",
	"PROPFIND",
	"PUT",
	"DELETE",
	"HEAD",
	"MOVE",
	"COPY"
};

void
ServerStartModule_CONTEXT
(void)
{
	long *v;
	HttpReqTypes = NewHash(1, NULL);
	HttpHeaderHandler = NewHash(1, NULL);

	v = malloc(sizeof(long));
	*v = eGET;
	Put(HttpReqTypes, HKEY("GET"), v, NULL);

	v = malloc(sizeof(long));
	*v = ePOST;
	Put(HttpReqTypes, HKEY("POST"), v, NULL);

	v = malloc(sizeof(long));
	*v = eOPTIONS;
	Put(HttpReqTypes, HKEY("OPTIONS"), v, NULL);

	v = malloc(sizeof(long));
	*v = ePROPFIND;
	Put(HttpReqTypes, HKEY("PROPFIND"), v, NULL);

	v = malloc(sizeof(long));
	*v = ePUT;
	Put(HttpReqTypes, HKEY("PUT"), v, NULL);

	v = malloc(sizeof(long));
	*v = eDELETE;
	Put(HttpReqTypes, HKEY("DELETE"), v, NULL);

	v = malloc(sizeof(long));
	*v = eHEAD;
	Put(HttpReqTypes, HKEY("HEAD"), v, NULL);

	v = malloc(sizeof(long));
	*v = eMOVE;
	Put(HttpReqTypes, HKEY("MOVE"), v, NULL);

	v = malloc(sizeof(long));
	*v = eCOPY;
	Put(HttpReqTypes, HKEY("COPY"), v, NULL);
}

void 
ServerShutdownModule_CONTEXT
(void)
{
	DeleteHash(&HttpReqTypes);
	DeleteHash(&HttpHeaderHandler);
}

void RegisterHeaderHandler(const char *Name, long Len, Header_Evaluator F)
{
	OneHttpHeader *pHdr;
	pHdr = (OneHttpHeader*) malloc(sizeof(OneHttpHeader));
	memset(pHdr, 0, sizeof(OneHttpHeader));
	pHdr->H = F;
	Put(HttpHeaderHandler, Name, Len, pHdr, DestroyHttpHeaderHandler);
}
extern void blank_page(void); ///TODO: remove me
void 
InitModule_CONTEXT
(void)
{
	RegisterHeaderHandler(HKEY("COOKIE"), Header_HandleCookie);
	RegisterHeaderHandler(HKEY("AUTHORIZATION"), Header_HandleAuth);
	RegisterHeaderHandler(HKEY("CONTENT-LENGTH"), Header_HandleContentLength);
	RegisterHeaderHandler(HKEY("CONTENT-TYPE"), Header_HandleContentType);
	RegisterHeaderHandler(HKEY("USER-AGENT"), Header_HandleUserAgent);
	RegisterHeaderHandler(HKEY("X-FORWARDED-HOST"), Header_HandleXFFHost);
	RegisterHeaderHandler(HKEY("HOST"), Header_HandleHost);
	RegisterHeaderHandler(HKEY("X-FORWARDED-FOR"), Header_HandleXFF);
	RegisterHeaderHandler(HKEY("ACCEPT-ENCODING"), Header_HandleAcceptEncoding);
	RegisterHeaderHandler(HKEY("IF-MODIFIED-SINCE"), Header_HandleIfModSince);

	RegisterNamespace("CURRENT_USER", 0, 1, tmplput_current_user, CTX_NONE);
	RegisterNamespace("CURRENT_ROOM", 0, 1, tmplput_current_room, CTX_NONE);
	RegisterNamespace("NONCE", 0, 0, tmplput_nonce, 0);



	WebcitAddUrlHandler(HKEY("blank"), blank_page, ANONYMOUS|BOGUS);

	WebcitAddUrlHandler(HKEY("webcit"), blank_page, URLNAMESPACE);
}
	


void 
HttpDestroyModule_CONTEXT
(ParsedHttpHdrs *httpreq)
{
	FreeStrBuf(&httpreq->ReqLine);
	FreeStrBuf(&httpreq->ReadBuf);
	FreeStrBuf(&httpreq->PlainArgs);
	FreeStrBuf(&httpreq->this_page);
	DeleteHash(&httpreq->HTTPHeaders);

}
