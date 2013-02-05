/*
 * This is the other half of the webserver.  It handles the task of hooking
 * up HTTP requests with the sessions they belong to, using HTTP cookies to
 * keep track of things.  If the HTTP request doesn't belong to any currently
 * active session, a new session is started.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include "modules_init.h"

/* Only one thread may manipulate SessionList at a time... */
pthread_mutex_t SessionListMutex;

wcsession *SessionList = NULL;	/* Linked list of all webcit sessions */

pthread_key_t MyConKey;         /* TSD key for MySession() */
HashList *HttpReqTypes = NULL;
HashList *HttpHeaderHandler = NULL;
extern HashList *HandlerHash;

/* the following two values start at 1 because the initial parent thread counts as one. */
int num_threads_existing = 1;		/* Number of worker threads which exist. */
int num_threads_executing = 1;		/* Number of worker threads currently executing. */

extern void session_loop(void);
void spawn_another_worker_thread(void);


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
	time_t the_time;

	/*
	 * Lock the session list, moving any candidates for euthanasia into
	 * a separate list.
	 */
	the_time = 0;
	CtdlLogResult(pthread_mutex_lock(&SessionListMutex));
	for (sptr = SessionList; sptr != NULL; sptr = sptr->next) {
		if (the_time == 0)
			the_time = time(NULL);
		/* Kill idle sessions */
		if ((sptr->inuse == 0) && 
		    ((the_time - (sptr->lastreq)) > (time_t) WEBCIT_TIMEOUT))
		{
			syslog(LOG_DEBUG, "Timeout session %d", sptr->wc_session);
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
	CtdlLogResult(pthread_mutex_unlock(&SessionListMutex));

	/*
	 * Now free up and destroy the culled sessions.
	 */
	while (sessions_to_kill != NULL) {
		syslog(LOG_DEBUG, "Destroying session %d", sessions_to_kill->wc_session);
		sptr = sessions_to_kill->next;
		session_destroy_modules(&sessions_to_kill);
		sessions_to_kill = sptr;
	}
}

/*
 * Check the size of our thread pool.  If all threads are executing, spawn another.
 */
void check_thread_pool_size(void)
{
	if (time_to_die) return;		/* don't expand the thread pool during shutdown */

	begin_critical_section(S_SPAWNER);	/* only one of these should run at a time */
	if (
		(num_threads_executing >= num_threads_existing)
		&& (num_threads_existing < MAX_WORKER_THREADS)
	) {
		syslog(LOG_DEBUG, "%d of %d threads are executing.  Adding another worker thread.",
			num_threads_executing,
			num_threads_existing
		);
		spawn_another_worker_thread();
	}
	end_critical_section(S_SPAWNER);
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

wcsession *FindSession(wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex)
{
	wcsession *sptr = NULL;
	wcsession *TheSession = NULL;	
	
	if (Hdr->HR.got_auth == AUTH_BASIC) {
		GetAuthBasic(Hdr);
	}

	CtdlLogResult(pthread_mutex_lock(ListMutex));
	for (sptr = *wclist; ((sptr != NULL) && (TheSession == NULL)); sptr = sptr->next) {
		
		/* If HTTP-AUTH, look for a session with matching credentials */
		switch (Hdr->HR.got_auth)
		{
		case AUTH_BASIC:
			if (	(!strcasecmp(ChrPtr(Hdr->c_username), ChrPtr(sptr->wc_username)))
				&& (!strcasecmp(ChrPtr(Hdr->c_password), ChrPtr(sptr->wc_password)))
				&& (sptr->killthis == 0)
			) {
				syslog(LOG_DEBUG, "Matched a session with the same http-auth");
				TheSession = sptr;
			}
			break;
		case AUTH_COOKIE:
			/* If cookie-session, look for a session with matching session ID */
			if (	(Hdr->HR.desired_session != 0)
				&& (sptr->wc_session == Hdr->HR.desired_session)
			) {
				syslog(LOG_DEBUG, "Matched a session with the same cookie");
				TheSession = sptr;
			}
			break;			     
		case NO_AUTH:
			/* Any unbound session is a candidate */
			if ( (sptr->wc_session == 0) && (sptr->inuse == 0) ) {
				syslog(LOG_DEBUG, "Reusing an unbound session");
				TheSession = sptr;
			}
			break;
		}
	}
	CtdlLogResult(pthread_mutex_unlock(ListMutex));
	if (TheSession == NULL) {
		syslog(LOG_DEBUG, "No existing session was matched");
	}
	return TheSession;
}

wcsession *CreateSession(int Lockable, int Static, wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex)
{
	wcsession *TheSession;
	TheSession = (wcsession *) malloc(sizeof(wcsession));
	memset(TheSession, 0, sizeof(wcsession));
	TheSession->Hdr = Hdr;
	TheSession->serv_sock = (-1);
	TheSession->lastreq = time(NULL);;

	pthread_setspecific(MyConKey, (void *)TheSession);
	
	/* If we're recreating a session that expired, it's best to give it the same
	 * session number that it had before.  The client browser ought to pick up
	 * the new session number and start using it, but in some rare situations it
	 * doesn't, and that's a Bad Thing because it causes lots of spurious sessions
	 * to get created.
	 */	
	if (Hdr->HR.desired_session == 0) {
		TheSession->wc_session = GenerateSessionID();
		syslog(LOG_DEBUG, "Created new session %d", TheSession->wc_session);
	}
	else {
		TheSession->wc_session = Hdr->HR.desired_session;
		syslog(LOG_DEBUG, "Re-created session %d", TheSession->wc_session);
	}
	Hdr->HR.Static = Static;
	session_new_modules(TheSession);

	if (Lockable) {
		pthread_mutex_init(&TheSession->SessionMutex, NULL);

		if (ListMutex != NULL)
			CtdlLogResult(pthread_mutex_lock(ListMutex));

		if (wclist != NULL) {
			TheSession->nonce = rand();
			TheSession->next = *wclist;
			*wclist = TheSession;
		}
		if (ListMutex != NULL)
			CtdlLogResult(pthread_mutex_unlock(ListMutex));
	}
	return TheSession;
}


/* If it's a "force 404" situation then display the error and bail. */
void do_404(void)
{
	hprintf("HTTP/1.1 404 Not found\r\n");
	hprintf("Content-Type: text/plain\r\n");
	wc_printf("Not found\r\n");
	end_burst();
}

int ReadHttpSubject(ParsedHttpHdrs *Hdr, StrBuf *Line, StrBuf *Buf)
{
	const char *Args;
	void *vLine, *vHandler;
	const char *Pos = NULL;

	Hdr->HR.ReqLine = Line;
	/* The requesttype... GET, POST... */
	StrBufExtract_token(Buf, Hdr->HR.ReqLine, 0, ' ');
	if (GetHash(HttpReqTypes, SKEY(Buf), &vLine) &&
	    (vLine != NULL))
	{
		Hdr->HR.eReqType = *(long*)vLine;
	}
	else {
		Hdr->HR.eReqType = eGET;
		return 1;
	}
	StrBufCutLeft(Hdr->HR.ReqLine, StrLength(Buf) + 1);

	/* the HTTP Version... */
	StrBufExtract_token(Buf, Hdr->HR.ReqLine, 1, ' ');
	StrBufCutRight(Hdr->HR.ReqLine, StrLength(Buf) + 1);
	
	if (StrLength(Buf) == 0) {
		Hdr->HR.eReqType = eGET;
		return 1;
	}

	StrBufAppendBuf(Hdr->this_page, Hdr->HR.ReqLine, 0);

	/* chop Filename / query arguments */
	Args = strchr(ChrPtr(Hdr->HR.ReqLine), '?');
	if (Args == NULL) /* whe're not that picky about params... TODO: this will spoil '&' in filenames.*/
		Args = strchr(ChrPtr(Hdr->HR.ReqLine), '&');
	if (Args != NULL) {
		Args ++; /* skip the ? */
		StrBufPlain(Hdr->PlainArgs, 
			    Args, 
			    StrLength(Hdr->HR.ReqLine) -
			    (Args - ChrPtr(Hdr->HR.ReqLine)));
		StrBufCutAt(Hdr->HR.ReqLine, 0, Args - 1);
	} /* don't parse them yet, maybe we don't even care... */
	
	/* now lookup what we are going to do with this... */
	/* skip first slash */
	StrBufExtract_NextToken(Buf, Hdr->HR.ReqLine, &Pos, '/');
	do {
		StrBufExtract_NextToken(Buf, Hdr->HR.ReqLine, &Pos, '/');

		GetHash(HandlerHash, SKEY(Buf), &vHandler),
		Hdr->HR.Handler = (WebcitHandler*) vHandler;
		if (Hdr->HR.Handler == NULL)
			break;
		/*
		 * If the request is prefixed by "/webcit" then chop that off.  This
		 * allows a front end web server to forward all /webcit requests to us
		 * while still using the same web server port for other things.
		 */
		if ((Hdr->HR.Handler->Flags & URLNAMESPACE) != 0)
			continue;
		break;
	} while (1);
	/* remove the handlername from the URL */
	if ((Pos != NULL) && (Pos != StrBufNOTNULL)){
		StrBufCutLeft(Hdr->HR.ReqLine, 
			      Pos - ChrPtr(Hdr->HR.ReqLine));
	}

	if (Hdr->HR.Handler != NULL) {
		if ((Hdr->HR.Handler->Flags & BOGUS) != 0) {
			return 1;
		}
		Hdr->HR.DontNeedAuth = (
			((Hdr->HR.Handler->Flags & ISSTATIC) != 0) ||
			((Hdr->HR.Handler->Flags & ANONYMOUS) != 0)
		);
	}
	else {
		/* If this is a "flat" request for the root, display the configured landing page. */
		int return_value;
		StrBuf *NewLine = NewStrBuf();
		Hdr->HR.DontNeedAuth = 1;
		StrBufAppendPrintf(NewLine, "GET /landing?go=%s HTTP/1.0", ChrPtr(Buf));
		syslog(LOG_DEBUG, "Replacing with: %s", ChrPtr(NewLine));
		return_value = ReadHttpSubject(Hdr, NewLine, Buf);
		FreeStrBuf(&NewLine);
		return return_value;
	}

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
int ReadHTTPRequest (ParsedHttpHdrs *Hdr)
{
	const char *pch, *pchs, *pche;
	OneHttpHeader *pHdr;
	StrBuf *Line, *LastLine, *HeaderName;
	int nLine = 0;
	void *vF;
	int isbogus = 0;

	HeaderName = NewStrBuf();
	LastLine = NULL;
	do {
		nLine ++;
		Line = NewStrBufPlain(NULL, SIZ / 4);

		if (ClientGetLine(Hdr, Line) < 0) return 1;

		if (StrLength(Line) == 0) {
			FreeStrBuf(&Line);
			continue;
		}
		if (nLine == 1) {
			Hdr->HTTPHeaders = NewHash(1, NULL);
			pHdr = (OneHttpHeader*) malloc(sizeof(OneHttpHeader));
			memset(pHdr, 0, sizeof(OneHttpHeader));
			pHdr->Val = Line;
			Put(Hdr->HTTPHeaders, HKEY("GET /"), pHdr, DestroyHttpHeaderHandler);
			syslog(LOG_DEBUG, "%s", ChrPtr(Line));
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
		pche = pchs + StrLength(Line);
		pch = pchs + StrLength(HeaderName) + 1;
		pche = pchs + StrLength(Line);
		while ((pch < pche) && isspace(*pch))
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

void OverrideRequest(ParsedHttpHdrs *Hdr, const char *Line, long len)
{
	StrBuf *Buf = NewStrBuf();

	if (Hdr->HR.ReqLine != NULL) {
		FlushStrBuf(Hdr->HR.ReqLine);
		StrBufPlain(Hdr->HR.ReqLine, Line, len);
	}
	else {
		Hdr->HR.ReqLine = NewStrBufPlain(Line, len);
	}
	ReadHttpSubject(Hdr, Hdr->HR.ReqLine, Buf);

	FreeStrBuf(&Buf);
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
void context_loop(ParsedHttpHdrs *Hdr)
{
	int isbogus = 0;
	wcsession *TheSession;
	struct timeval tx_start;
	struct timeval tx_finish;
	int session_may_be_reused = 1;
	time_t now;
	
	gettimeofday(&tx_start, NULL);		/* start a stopwatch for performance timing */

	/*
	 * Find out what it is that the web browser is asking for
	 */
	isbogus = ReadHTTPRequest(Hdr);

	Hdr->HR.dav_depth = 32767; /* TODO: find a general way to have non-0 defaults */

	if (!isbogus) {
		isbogus = AnalyseHeaders(Hdr);
	}

	if (	(isbogus)
		|| ((Hdr->HR.Handler != NULL)
		&& ((Hdr->HR.Handler->Flags & BOGUS) != 0))
	) {
		wcsession *Bogus;
		Bogus = CreateSession(0, 1, NULL, Hdr, NULL);
		do_404();
		syslog(LOG_WARNING, "HTTP: 404 [%ld.%06ld] %s %s",
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
			ReqStrs[Hdr->HR.eReqType],
			ChrPtr(Hdr->this_page)
			);
		session_detach_modules(Bogus);
		session_destroy_modules(&Bogus);
		return;
	}

	if ((Hdr->HR.Handler != NULL) && ((Hdr->HR.Handler->Flags & ISSTATIC) != 0))
	{
		wcsession *Static;
		Static = CreateSession(0, 1, NULL, Hdr, NULL);
		
		Hdr->HR.Handler->F();

		/* How long did this transaction take? */
		gettimeofday(&tx_finish, NULL);
		
		syslog(LOG_DEBUG, "HTTP: 200 [%ld.%06ld] %s %s",
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
			ReqStrs[Hdr->HR.eReqType],
			ChrPtr(Hdr->this_page)
		);
		session_detach_modules(Static);
		session_destroy_modules(&Static);
		return;
	}

	if (Hdr->HR.got_auth == AUTH_BASIC) {
		CheckAuthBasic(Hdr);
	}

	if (Hdr->HR.got_auth) {
		session_may_be_reused = 0;
	}

	/*
	 * See if there's an existing session open with any of:
	 * - The desired Session ID
	 * - A matching http-auth username and password
	 * - An unbound session flagged as reusable
	 */
	TheSession = FindSession(&SessionList, Hdr, &SessionListMutex);

	/*
	 * If there were no qualifying sessions, then create a new one.
	 */
	if ((TheSession == NULL) || (TheSession->killthis != 0)) {
		TheSession = CreateSession(1, 0, &SessionList, Hdr, &SessionListMutex);
	}

	/*
	 * Reject transactions which require http-auth, if http-auth was not provided
	 */
	if (	(StrLength(Hdr->c_username) == 0)
		&& (!Hdr->HR.DontNeedAuth)
		&& (Hdr->HR.Handler != NULL)
		&& ((XHTTP_COMMANDS & Hdr->HR.Handler->Flags) == XHTTP_COMMANDS)
	) {
		syslog(LOG_DEBUG, "http-auth required but not provided");
		OverrideRequest(Hdr, HKEY("GET /401 HTTP/1.0"));
		Hdr->HR.prohibit_caching = 1;				
	}

	/*
	 * A future improvement might be to check the session integrity
	 * at this point before continuing.
	 */

	/*
	 * Bind to the session and perform the transaction
	 */
	now = time(NULL);;
	CtdlLogResult(pthread_mutex_lock(&TheSession->SessionMutex));
	pthread_setspecific(MyConKey, (void *)TheSession);
	
	TheSession->inuse = 1;				/* mark the session as bound */
	TheSession->lastreq = now;			/* log */
	TheSession->Hdr = Hdr;

	/*
	 * If a language was requested via a cookie, select that language now.
	 */
	if (StrLength(Hdr->c_language) > 0) {
		syslog(LOG_DEBUG, "Session cookie requests language '%s'", ChrPtr(Hdr->c_language));
		set_selected_language(ChrPtr(Hdr->c_language));
		go_selected_language();
	}

	/*
	 * do the transaction
	 */
	session_attach_modules(TheSession);
	session_loop();

	/* How long did this transaction take? */
	gettimeofday(&tx_finish, NULL);

	syslog(LOG_INFO, "HTTP: 200 [%ld.%06ld] %s %s",
		((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
		((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
		ReqStrs[Hdr->HR.eReqType],
		ChrPtr(Hdr->this_page)
	);

	session_detach_modules(TheSession);

	/* If *this* very transaction did not explicitly specify a session cookie,
	 * and it did not log in, we want to flag the session as a candidate for
	 * re-use by the next unbound client that comes along.  This keeps our session
	 * table from getting bombarded with new sessions when, for example, a web
	 * spider crawls the site without using cookies.
	 */
	if ((session_may_be_reused) && (!WC->logged_in)) {
		WC->wc_session = 0;			/* flag as available for re-use */
		TheSession->selected_language = 0;	/* clear any non-default language setting */
	}

	TheSession->Hdr = NULL;
	TheSession->inuse = 0;					/* mark the session as unbound */
	CtdlLogResult(pthread_mutex_unlock(&TheSession->SessionMutex));
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

void Header_HandleContentLength(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.ContentLength = StrToi(Line);
}

void Header_HandleContentType(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.ContentType = Line;
}


void Header_HandleHost(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (hdr->HostHeader != NULL) {
		FreeStrBuf(&hdr->HostHeader);
	}
	hdr->HostHeader = NewStrBuf();
	StrBufAppendPrintf(hdr->HostHeader, "%s://", (is_https ? "https" : "http") );
	StrBufAppendBuf(hdr->HostHeader, Line, 0);
}

void Header_HandleXFFHost(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (!follow_xff) return;

	if (hdr->HostHeader != NULL) {
		FreeStrBuf(&hdr->HostHeader);
	}

	hdr->HostHeader = NewStrBuf();
	StrBufAppendPrintf(hdr->HostHeader, "http://");	/* this is naive; do something about it */
	StrBufAppendBuf(hdr->HostHeader, Line, 0);
}


void Header_HandleXFF(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.browser_host = Line;

	while (StrBufNum_tokens(hdr->HR.browser_host, ',') > 1) {
		StrBufRemove_token(hdr->HR.browser_host, 0, ',');
	}
	StrBufTrim(hdr->HR.browser_host);
}

void Header_HandleIfModSince(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.if_modified_since = httpdate_to_timestamp(Line);
}

void Header_HandleAcceptEncoding(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	/*
	 * Can we compress?
	 */
	if (strstr(&ChrPtr(Line)[16], "gzip")) {
		hdr->HR.gzip_ok = 1;
	}
}
const char *ReqStrs[eNONE] = {
	"GET",
	"POST",
	"OPTIONS",
	"PROPFIND",
	"PUT",
	"DELETE",
	"HEAD",
	"MOVE",
	"COPY",
	"REPORT"
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

	v = malloc(sizeof(long));
	*v = eREPORT;
	Put(HttpReqTypes, HKEY("REPORT"), v, NULL);
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


void 
InitModule_CONTEXT
(void)
{
	RegisterHeaderHandler(HKEY("CONTENT-LENGTH"), Header_HandleContentLength);
	RegisterHeaderHandler(HKEY("CONTENT-TYPE"), Header_HandleContentType);
	RegisterHeaderHandler(HKEY("X-FORWARDED-HOST"), Header_HandleXFFHost); /* Apache way... */
	RegisterHeaderHandler(HKEY("X-REAL-IP"), Header_HandleXFFHost);        /* NGinX way... */
	RegisterHeaderHandler(HKEY("HOST"), Header_HandleHost);
	RegisterHeaderHandler(HKEY("X-FORWARDED-FOR"), Header_HandleXFF);
	RegisterHeaderHandler(HKEY("ACCEPT-ENCODING"), Header_HandleAcceptEncoding);
	RegisterHeaderHandler(HKEY("IF-MODIFIED-SINCE"), Header_HandleIfModSince);

	RegisterNamespace("CURRENT_USER", 0, 1, tmplput_current_user, NULL, CTX_NONE);
	RegisterNamespace("NONCE", 0, 0, tmplput_nonce, NULL, 0);

	WebcitAddUrlHandler(HKEY("404"), "", 0, do_404, ANONYMOUS|COOKIEUNNEEDED);
/*
 * Look for commonly-found probes of malware such as worms, viruses, trojans, and Microsoft Office.
 * Short-circuit these requests so we don't have to send them through the full processing loop.
 */
	WebcitAddUrlHandler(HKEY("scripts"), "", 0, do_404, ANONYMOUS|BOGUS);		/* /root.exe - Worms and trojans and viruses, oh my! */
	WebcitAddUrlHandler(HKEY("c"), "", 0, do_404, ANONYMOUS|BOGUS);		/* /winnt */
	WebcitAddUrlHandler(HKEY("MSADC"), "", 0, do_404, ANONYMOUS|BOGUS);
	WebcitAddUrlHandler(HKEY("_vti"), "", 0, do_404, ANONYMOUS|BOGUS);		/* Broken Microsoft DAV implementation */
	WebcitAddUrlHandler(HKEY("MSOffice"), "", 0, do_404, ANONYMOUS|BOGUS);		/* Stoopid MSOffice thinks everyone is IIS */
	WebcitAddUrlHandler(HKEY("nonexistenshit"), "", 0, do_404, ANONYMOUS|BOGUS);	/* Exploit found in the wild January 2009 */
}
	

void 
HttpNewModule_CONTEXT
(ParsedHttpHdrs *httpreq)
{
	httpreq->PlainArgs = NewStrBufPlain(NULL, SIZ);
	httpreq->this_page = NewStrBufPlain(NULL, SIZ);
}

void 
HttpDetachModule_CONTEXT
(ParsedHttpHdrs *httpreq)
{
	FlushStrBuf(httpreq->PlainArgs);
	FlushStrBuf(httpreq->HostHeader);
	FlushStrBuf(httpreq->this_page);
	FlushStrBuf(httpreq->PlainArgs);
	DeleteHash(&httpreq->HTTPHeaders);
	memset(&httpreq->HR, 0, sizeof(HdrRefs));
}

void 
HttpDestroyModule_CONTEXT
(ParsedHttpHdrs *httpreq)
{
	FreeStrBuf(&httpreq->this_page);
	FreeStrBuf(&httpreq->PlainArgs);
	FreeStrBuf(&httpreq->this_page);
	FreeStrBuf(&httpreq->PlainArgs);
	FreeStrBuf(&httpreq->HostHeader);
	DeleteHash(&httpreq->HTTPHeaders);

}
