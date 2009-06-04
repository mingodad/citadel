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

wcsession *FindSession(wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex)
{
	wcsession  *sptr, *TheSession = NULL;	
	
	pthread_mutex_lock(ListMutex);
	for (sptr = *wclist; 
	     ((sptr != NULL) && (TheSession == NULL)); 
	     sptr = sptr->next) {
		
		/** If HTTP-AUTH, look for a session with matching credentials */
		switch (Hdr->HR.got_auth)
		{
		case AUTH_BASIC:
			if ( (Hdr->HR.SessionKey != sptr->SessionKey))
				continue;
			GetAuthBasic(Hdr);
			if ((!strcasecmp(ChrPtr(Hdr->c_username), ChrPtr(sptr->wc_username))) &&
			    (!strcasecmp(ChrPtr(Hdr->c_password), ChrPtr(sptr->wc_password))) ) 
				TheSession = sptr;
			break;
		case AUTH_COOKIE:
			/** If cookie-session, look for a session with matching session ID */
			if ( (Hdr->HR.desired_session != 0) && 
			     (sptr->wc_session == Hdr->HR.desired_session)) 
				TheSession = sptr;
			break;			     
		case NO_AUTH:
			break;
		}
	}
	pthread_mutex_unlock(ListMutex);
	return TheSession;
}

wcsession *CreateSession(int Lockable, wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex)
{
	wcsession *TheSession;
	lprintf(3, "Creating a new session\n");
	TheSession = (wcsession *)
		malloc(sizeof(wcsession));
	memset(TheSession, 0, sizeof(wcsession));
	TheSession->Hdr = Hdr;
	TheSession->SessionKey = Hdr->HR.SessionKey;
	TheSession->serv_sock = (-1);
	TheSession->chat_sock = (-1);
	TheSession->is_mobile = -1;

	pthread_setspecific(MyConKey, (void *)TheSession);
	
	/* If we're recreating a session that expired, it's best to give it the same
	 * session number that it had before.  The client browser ought to pick up
	 * the new session number and start using it, but in some rare situations it
	 * doesn't, and that's a Bad Thing because it causes lots of spurious sessions
	 * to get created.
	 */	
	if (Hdr->HR.desired_session == 0) {
		TheSession->wc_session = GenerateSessionID();
	}
	else {
		TheSession->wc_session = Hdr->HR.desired_session;
	}

	session_new_modules(TheSession);

	if (Lockable) {
		pthread_mutex_init(&TheSession->SessionMutex, NULL);

		if (ListMutex != NULL)
			pthread_mutex_lock(ListMutex);

		if (wclist != NULL) {
			TheSession->nonce = rand();
			TheSession->next = *wclist;
			*wclist = TheSession;
		}
		if (ListMutex != NULL)
			pthread_mutex_unlock(ListMutex);
	}
	return TheSession;
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

/* If it's a "force 404" situation then display the error and bail. */
void do_404(void)
{
	hprintf("HTTP/1.1 404 Not found\r\n");
	hprintf("Content-Type: text/plain\r\n");
	wprintf("Not found\r\n");
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
	if (Pos != NULL) {
		StrBufCutLeft(Hdr->HR.ReqLine, 
			      Pos - ChrPtr(Hdr->HR.ReqLine));
	}

	if (Hdr->HR.Handler != NULL) {
		if ((Hdr->HR.Handler->Flags & BOGUS) != 0)
			return 1;
		Hdr->HR.DontNeedAuth = (
			((Hdr->HR.Handler->Flags & ISSTATIC) != 0) ||
			((Hdr->HR.Handler->Flags & ANONYMOUS) != 0)
			);
	}
	else {
		Hdr->HR.DontNeedAuth = 1; /* Flat request? show him the login screen... */
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
			lprintf(9, "%s\n", ChrPtr(Line));
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

void OverrideRequest(ParsedHttpHdrs *Hdr, const char *Line, long len)
{
	StrBuf *Buf = NewStrBuf();

	FlushStrBuf(Hdr->HR.ReqLine);
	StrBufPlain(Hdr->HR.ReqLine, Line, len);
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
	
	gettimeofday(&tx_start, NULL);		/* start a stopwatch for performance timing */

	/*
	 * Find out what it is that the web browser is asking for
	 */
	isbogus = ReadHTTPRequest(Hdr);

	if (!isbogus)
		isbogus = AnalyseHeaders(Hdr);

	if ((isbogus) ||
	    ((Hdr->HR.Handler != NULL) &&
	     ((Hdr->HR.Handler->Flags & BOGUS) != 0)))
	{
		wcsession *Bogus;

		Bogus = CreateSession(0, NULL, Hdr, NULL);

		do_404();

		lprintf(9, "HTTP: 404 [%ld.%06ld] %s %s \n",
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
			ReqStrs[Hdr->HR.eReqType],
			ChrPtr(Hdr->this_page)
			);
		session_detach_modules(Bogus);
		session_destroy_modules(&Bogus);
		return;
	}

	if ((Hdr->HR.Handler != NULL) && 
	    ((Hdr->HR.Handler->Flags & ISSTATIC) != 0))
	{
		wcsession *Static;
		Static = CreateSession(0, NULL, Hdr, NULL);
		
		Hdr->HR.Handler->F();

		/* How long did this transaction take? */
		gettimeofday(&tx_finish, NULL);
		
#ifdef TECH_PREVIEW
		if ((Hdr->HR.Handler != NULL) ||
		    ((Hdr->HR.Handler->Flags & LOGCHATTY) == 0))
#endif
			lprintf(9, "HTTP: 200 [%ld.%06ld] %s %s \n",
				((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
				((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
				ReqStrs[Hdr->HR.eReqType],
				ChrPtr(Hdr->this_page)
				);
		session_detach_modules(Static);
		session_destroy_modules(&Static);
		return;
	}

	if (Hdr->HR.got_auth == AUTH_BASIC) 
		CheckAuthBasic(Hdr);


/*	dbg_PrintHash(HTTPHeaders, nix, NULL);  */

	/**
	 * See if there's an existing session open with the desired ID or user/pass
	 */
	TheSession = NULL;

	if (TheSession == NULL) {
		TheSession = FindSession(&SessionList, Hdr, &SessionListMutex);
	}

	/**
	 * Create a new session if we have to
	 */
	if (TheSession == NULL) {
		TheSession = CreateSession(1, &SessionList, Hdr, &SessionListMutex);

		if ((StrLength(Hdr->c_username) == 0) &&
		    (!Hdr->HR.DontNeedAuth)) {
			OverrideRequest(Hdr, HKEY("GET /static/nocookies.html?force_close_session=yes HTTP/1.0"));
			Hdr->HR.prohibit_caching = 1;
		}
		
		if (StrLength(Hdr->c_language) > 0) {
			lprintf(9, "Session cookie requests language '%s'\n", ChrPtr(Hdr->c_language));
			set_selected_language(ChrPtr(Hdr->c_language));
			go_selected_language();
		}
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
	TheSession->Hdr = Hdr;

	session_attach_modules(TheSession);
	session_loop();				/* do transaction */


	/* How long did this transaction take? */
	gettimeofday(&tx_finish, NULL);
	

#ifdef TECH_PREVIEW
	if ((Hdr->HR.Handler != NULL) &&
	    ((Hdr->HR.Handler->Flags & LOGCHATTY) == 0))
#endif
		lprintf(9, "HTTP: 200 [%ld.%06ld] %s %s \n",
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) / 1000000,
			((tx_finish.tv_sec*1000000 + tx_finish.tv_usec) - (tx_start.tv_sec*1000000 + tx_start.tv_usec)) % 1000000,
			ReqStrs[Hdr->HR.eReqType],
			ChrPtr(Hdr->this_page)
			);

	session_detach_modules(TheSession);

	TheSession->Hdr = NULL;
	pthread_mutex_unlock(&TheSession->SessionMutex);	/* unbind */
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

void Header_HandleContentLength(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.ContentLength = StrToi(Line);
}

void Header_HandleContentType(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.ContentType = Line;
}

void Header_HandleUserAgent(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	hdr->HR.user_agent = Line;
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
	if ((follow_xff) && (hdr->HR.http_host != NULL))
		return;
	else
		hdr->HR.http_host = Line;
}

void Header_HandleXFFHost(StrBuf *Line, ParsedHttpHdrs *hdr)
{
	if (follow_xff)
		hdr->HR.http_host = Line;
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


void 
InitModule_CONTEXT
(void)
{
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

	WebcitAddUrlHandler(HKEY("404"), do_404, ANONYMOUS|COOKIEUNNEEDED);
/*
 * Look for commonly-found probes of malware such as worms, viruses, trojans, and Microsoft Office.
 * Short-circuit these requests so we don't have to send them through the full processing loop.
 */
	WebcitAddUrlHandler(HKEY("scripts"), do_404, ANONYMOUS|BOGUS); /* /root.exe	/* Worms and trojans and viruses, oh my! */
	WebcitAddUrlHandler(HKEY("c"), do_404, ANONYMOUS|BOGUS);        /* /winnt */
	WebcitAddUrlHandler(HKEY("MSADC"), do_404, ANONYMOUS|BOGUS);
	WebcitAddUrlHandler(HKEY("_vti"), do_404, ANONYMOUS|BOGUS);		/* Broken Microsoft DAV implementation */
	WebcitAddUrlHandler(HKEY("MSOffice"), do_404, ANONYMOUS|BOGUS);		/* Stoopid MSOffice thinks everyone is IIS */
	WebcitAddUrlHandler(HKEY("nonexistenshit"), do_404, ANONYMOUS|BOGUS);	/* Exploit found in the wild January 2009 */
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
	DeleteHash(&httpreq->HTTPHeaders);

}
