/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: libtransport.o
 ** Date: 2000-11-30
 ** Last Revision: 2000-11-30
 ** Description: Interface to Transport module.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include 	<unistd.h>
#include	<string.h>
#include	<termcap.h>
#include	<dlfcn.h> 
#include 	<signal.h>
#include 	<string.h>
#include 	<pwd.h>
#include 	<errno.h>
#include 	<sys/types.h>
#include 	<sys/socket.h>
#include 	<netinet/in.h>
#include 	<arpa/inet.h>
#include 	<sys/un.h>
#include 	<netdb.h>
#include 	<dlfcn.h>
#include	<CxClient.h>
#include	"uname.h"
#include	"autoconf.h"

static int	g_CxChatMode = 0,
		g_CxSocket = 0,
		g_CxAsynMode = 0,
		g_CxSemaphore = 0;
static CXCBHNDL	_CbHandles = 0;
static char	g_CxClientName[32] = "";
static int	_CxCallback(int, int, void *);
static void	timeout() {}
static void	_CxClSend( int, const char * );
static int	_CxClRecv( int, int*, char *, int );

/**
 ** CXTBL: Connection handle table.  Use this to make libCxClient thread-safe, and allow
 ** us to maintain multiple concurrent connections.
 **/
typedef struct	_cx_tbl_entry {

	int	cxId;		/* cxId: Connection ID */

	char	host[255],	/* host: Citadel/UX hostname */
		user[64],	/* user: Citadel/UX username */
		pass[64];	/* pass: Citadel/UX password */
	int	port,		/* port: Port number to connect to */
		connected,	/* connected: (bool) Are we connected to our Citadel/UX host? */
		asynMode,	/* asynMode: (bool) Are we actively in ASYN mode? */
		semaphore;	/* semaphore: (bool) Prevent access to _sock at this time? */

	/**
	 ** Internal
	 **/
	int	_sock;		/* _sock: TCP/IP connection socket */

	struct _cx_tbl_entry
		*_next,		/* _next: Next CXTBL entry. */
		*_prev;		/* _prev: Previous CXTBL entry. */

}		CXTBLENT;
typedef CXTBLENT* CXHNDL;

/**
 ** [GLOBAL CXTABLE] There should only exist one of these in memory at any
 ** point in time, to ensure threadsafeness.
 **/
static CXHNDL		g_CxTbl = 0L;

/**
 ** _CxTbNewID(): Get the next cxId for the specified connection table.
 **/
static
int		_CxTbNewID( CXHNDL tbl ) {
CXHNDL		p;
int		ret;

	p = tbl;
	ret = 1;
	while( p ) {
		if(p->cxId == ret) ret = (p->cxId)+1;
		p = p->_next;
	}

	DPF((DFA, "Next cxId: %d", ret));
	return(ret);
}

/**
 ** _CxTbNew(): New CXTBL entry.
 **/
static
CXHNDL		_CxTbNew( CXHNDL tbl ) {
CXHNDL		ret = 0;

	DPF((DFA, "Creating new CXTBL handle."));

	ret = (CXHNDL) CxMalloc( sizeof(CXTBLENT) );
	if(ret<=0) return(NULL);

	/**
	 ** Initialize these pointers to prevent confusion.
	 **/
	ret->_next = NULL;
	ret->_prev = NULL;

	/**
	 ** Establish Default values
	 **/
	ret->port = 504;
	ret->connected = 0;
	ret->asynMode = 0;
	ret->semaphore = 0;
	ret->_sock = 0;
	ret->host[0] = 0;
	ret->user[0] = 0;
	ret->pass[0] = 0;

	/**
	 ** Obtain the next cxId for this particular table.
	 **/
	ret->cxId = _CxTbNewID( tbl );

	DPF((DFA, "Returning hndl @0x%08x", ret ));
	return(ret);
}

/**
 ** _CxTbEntry(): Return a handle to a particular table entry.
 **/
static
CXHNDL		_CxTbEntry( CXHNDL tbl, int id ) {
CXHNDL		p;

	DPF((DFA,"Resolve [tbl@0x%08x] id %d", tbl, id ));
	p = tbl;
	while( p ) {
		DPF((DFA,"p->cxId: %d", p->cxId));
		if( id == p->cxId ) {
			DPF((DFA," ->host: %s:%d", p->host, p->port));
			DPF((DFA," ->user: %s", p->user));
			DPF((DFA," ->pass: %s", p->pass));
			DPF((DFA," ->_sock: %d", p->_sock));
			return(p);
		}
		p = p->_next;
	}
	return((CXHNDL)NULL);
}

/**
 ** _CxTbInsert(): Insert a new CxTbl entry into the table.  Return a handle
 ** id for the new entry.  (Parameters here can be set at a later time.)
 **/
static
int		_CxTbInsert( const char *host, int port, const char *user, const char *pass ) {
CXHNDL		p,n;
char		*tmp;

	DPF((DFA,"Insert new table entry."));

	DPF((DFA,"Allocating new CXTBL block."));
	n = _CxTbNew( g_CxTbl );

	DPF((DFA,"Copying host"));
	if(host && *host) {
		if(strlen(host) >= 254) {
			tmp = (char *)CxMalloc( 255 );
			strcpy( tmp, host );
			tmp[254] = 0;
			strcpy(n->host, tmp);
			CxFree(tmp);

		} else {
			strcpy(n->host, host);
		}
	}

	DPF((DFA,"Copying user"));
	if(user && *user) {
		if(strlen(user) >= 64) {
			tmp = (char *)CxMalloc( 65 );
			strcpy( tmp, user );
			tmp[64] = 0;
			strcpy(n->user, tmp);
			CxFree(tmp);
		} else {
			strcpy(n->user, user);
		}
	}

	DPF((DFA,"Copying pass"));
	if(pass && *pass) {
		if(strlen(pass) >= 64) {
			tmp = (char *)CxMalloc( 65 );
			strcpy( tmp, pass );
			tmp[64] = 0;
			strcpy(n->pass, tmp);
			CxFree(tmp);
		} else {
			strcpy(n->pass, pass);
		}
	}

	DPF((DFA,"Copying port"));
	if(port) n->port = port;

	DPF((DFA,"Binding to g_CxTbl"));
	if(!g_CxTbl) {
		DPF((DFA,"new g_CxTbl"));
		g_CxTbl = n;
		DPF((DFA,"New table @0x%08x", g_CxTbl ));
		return(n->cxId);

	} else {
		DPF((DFA,"existing g_CxTbl"));
		p = g_CxTbl;
		while( p && p->_next ) {
			p = p->_next;
		}
		if( p ) {
			p->_next = n;
			n->_prev = p;
		}
	}

	return(n->cxId);
}

/**
 ** _CxTbDelete(): Delete the specified id.
 **/
static
void		_CxTbDelete( int id ) {
CXHNDL		p;

	if(!g_CxTbl || !id ) return;

	DPF((DFA,"Delete id %d", id));
	p = g_CxTbl;
	while( p ) {
		if( p->cxId == id ) break;
		p = p->_next;
	}

	DPF((DFA,"p @0x%08x", p));

	if( p ) {

		DPF((DFA,"p->_next @0x%08x", p->_next));
		DPF((DFA,"p->_prev @0x%08x", p->_prev));

		/**
		 ** This was the only entry in the CxTbl.
		 **/
		if( !p->_next && !p->_prev ) {
			CxFree(p);
			g_CxTbl = NULL;

		/**
		 ** Gymnastics time...
		 **/
		} else {
			if( p->_next ) p->_next->_prev = p->_prev;
			if( p->_prev ) p->_prev->_next = p->_next;

			if( g_CxTbl == p ) g_CxTbl = p->_next;

			CxFree(p);
		}
	}
	DPF((DFA,"g_CxTbl @0x%08x", g_CxTbl));
}

/**
 ** CxClConnection(): Obtain a Connection handle for a new host/username/password.  This _must_ be
 ** performed before any other CxCl functions can be called.
 **/
int		CxClConnection( const char *host, int port, const char *user, const char *pass ) {

	DPF((DFA,"New connection hndl %s:%s@%s:%d", user, "**", host, port));
	return(_CxTbInsert( host, port, user, pass ) );
}

/**
 ** CxClDelete(): Delete the specified connection handle.
 **/
void		CxClDelete( int id ) {

	DPF((DFA,"Delete hndl %d", id ));
	_CxTbDelete( id );
}


/**
 ** CxClSetHost(): Set the username for a specific connection handle.
 **/
void		CxClSetHost( int id, const char *host ) {
CXHNDL		e;

	if(!host || !*host) return;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return;

	DPF((DFA,"Set tbl[%d].host = '%s'", id, host ));
	memset( &(e->host), 0, 253 );
	strcpy( e->host, host );
}

/**
 ** CxClSetUser(): Set the username for a specific connection handle.
 **/
void		CxClSetUser( int id, const char *user ) {
CXHNDL		e;

	if(!user || !*user) return;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return;

	DPF((DFA,"Set tbl[%d].user = '%s'", id, user ));
	strcpy( e->user, user );
}

/**
 ** CxClGetUser(): Set the username for a specific connection handle.
 ** [*] FREE the results of this operation!!
 **/
char		*CxClGetUser( int id ) {
CXHNDL		e;
char		*ret;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return(NULL);

	if(e->user[0]) {
		ret = (char *)CxMalloc( strlen( e->user ) + 1 );
		strcpy( ret, e->user );
		return( ret );

	} else {
		return(NULL);
	}
}

/**
 ** CxClSetPass(): Set the username for a specific connection handle.
 **/
void		CxClSetPass( int id, const char *pass ) {
CXHNDL		e;

	if(!pass || !*pass) return;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return;

	DPF((DFA,"Set tbl[%d].pass = '%s'", id, pass ));
	strcpy( e->pass, pass );
}

/**
 ** CxClGetPass(): Set the username for a specific connection handle.
 **/
char		*CxClGetPass( int id ) {
CXHNDL		e;
char		*ret;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return(NULL);

	if(e->pass) {
		ret = (char *)CxMalloc( strlen(e->pass) +1 );
		strcpy(ret, e->pass);
		return(ret);

	} else {
		return(NULL);
	}
}

/**
 ** CxClSetPass(): Set the username for a specific connection handle.
/**
 ** CxClRegClient(): (For Developers) Register your client name with
 ** libCxClient.  This gets reported along with the IDEN information passed
 ** to the server.  It should be called before CxClConnect().
 **/
void		CxClRegClient(const char *cl_name) {

	DPF((DFA,"Developer registered this as \"%s\"", cl_name));

	/**
	 ** If this will cause libCxClient to crash, then just die.
	 **/
	if(strlen(cl_name)>31) {
		printf("* * *  Fatal Error  * * *\n");
		printf("Invalid use of CxClRegClient().  I expect cl_name to be less than 31 characters in length.\n");
		printf("cl_name = '%s'\n", cl_name);
		printf("\nI can't continue.  Please re-build your client.\n");
		exit(999);
	}

	strcpy(g_CxClientName, cl_name);
}

/**
 ** CxClConnect(): Establish a connection to the server via the Transport layer.
 ** [Much of this code was gleaned from the "citadel" client]
 **
 ** [Returns]
 **  On Success: 0
 **  On Failure: -1: Mis-configuration
 **              -[errno]: use abs(errno) to retrieve error message.
 **/
int		CxClConnect( int id ) {
char		buf[512];
struct 
hostent 	*phe;
struct 
servent 	*pse;
struct 
protoent 	*ppe;
struct 
sockaddr_in 	sin;
int 		s, type, rc;
char		*service = "citadel";
char		*protocol = "tcp";
CXHNDL		e;

	DPF((DFA,"(Library was built with UNIX_SOCKET support)"));

	e = _CxTbEntry( g_CxTbl, id );

	if(!e) {
		DPF((DFA,"Did not call CxConnection(), huh?"));
		return(-1);
	}

	if(!e->host[0]) {
		DPF((DFA,"No hostname provided.  Use CxClSetHost() first!!"));
		return(-1);
	}
	DPF((DFA,"Establishing connection to host \"%s\"",e->host));

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;

	DPF((DFA,"-> getservbyname()"));
        pse = getservbyname(service, protocol);
        if(pse) {
                sin.sin_port = pse->s_port;
        } else if((sin.sin_port = htons((u_short) atoi(service))) != 0) {
        } else {
		sin.sin_port = htons((u_short)504);
	}

	DPF((DFA,"-> gethostbyname(): \"%s\"", e->host));
        phe = gethostbyname(e->host);
	DPF((DFA,"phe@0x%08x", phe));
        if (phe) {
                memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);

        } else if ((sin.sin_addr.s_addr = inet_addr(e->host)) == INADDR_NONE) {
		DPF((DFA,"Unable to get host entry.  %s", strerror(errno)));
                return(-1*(errno));
        }

	DPF((DFA,"-> getprotobyname()"));
        if ((ppe = getprotobyname(protocol)) == 0) {
		DPF((DFA,"Unable to get protocol entry.  %s", strerror(errno)));
                return(-1);
        }
        if (!strcmp(protocol, "udp")) {
                type = SOCK_DGRAM;
        } else {
                type = SOCK_STREAM;
        }

        s = socket(PF_INET, type, ppe->p_proto);
        if (s < 0) {
		DPF((DFA,"Unable to create socket.  %s", strerror(errno)));
                return(-1);
        }
        signal(SIGALRM, timeout);
        alarm(30);

	DPF((DFA,"-> connect()"));
        if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
/**		printf("\n* * *  Fatal Error  * * *\n");
		printf("System Error: Can't connect to '%s' [%s]\n",e->host, service);
		printf("Error Details: %s\n\n", strerror(errno)); 
 **/
                return(errno*(-1));
        }
        alarm(0);
        signal(SIGALRM, SIG_IGN);

	DPF((DFA,"Socket %d", s));
	e->_sock = s;
	e->connected = 1;

	if( s ) {

		DPF((DFA,"-> recv"));
		_CxClRecv( e->_sock, &(e->semaphore), buf, id );
		if(g_CxClientName[0]) {
			sprintf(buf,"IDEN 1|1|100|CX/%s (%s)|",VERSION, g_CxClientName);
		} else {
			sprintf(buf,"IDEN 1|1|100|CX/%s|",VERSION);
		}
		_CxClSend(s, buf);
		_CxClRecv(e->_sock, &(e->semaphore), buf, id);

		_CxClSend(s, "ASYN 1");
		rc = _CxClRecv(e->_sock, &(e->semaphore), buf, id);

		/**
		 ** If the server doesn't support Asnychronous mode, then
		 ** we shouldn't try to be asynchronous...
		 **/
		if(CHECKRC(rc, RC_OK)) {
			DPF((DFA,":: Server in ASYNCHRONOUS mode."));
			e->asynMode = 1;
		} else {
			DPF((DFA,":: ASYNCHRONOUS mode not supported."));
			e->asynMode = 0;
		}

		/**
		 ** We don't return our socket anymore.
		 **/
        	return(0);
	}
	return(0);
}

/**
 ** CxClDisconnect(): Disconnect the specified socket.
 **/
void		CxClDisconnect( int id ) {
CXHNDL		e;

	DPF((DFA,"Caught orders to close socket %d.", id));

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return;

	/**
	 ** Sleep until the semaphore is cleared.
	 **/
	while(e->semaphore) ;

	shutdown(e->_sock, 0);
}

/**
 ** CxClStat(): Return connection status.
 **/
int		CxClStat( int id ) {
CXHNDL		e;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return(0);

	if( e->connected ) {
		return(1);
	} else {
		return(0);
	}
}

/**
 ** _CxClSend(): REAL send.  Uses a socket instead of the ID.
 **/
static
void		_CxClSend( int sock, const char *s ) {
int 		bytes_written = 0;
int 		retval,nbytes;
char		*ss;

	/**
	 ** If the socket is not open, there's no point in going here.
	 **/
	if(!sock) return;

	DPF((DFA,"PROT --> \"%s\"", s));

	ss = (char *)CxMalloc(strlen(s)+2);
	sprintf(ss,"%s\n",s);

	nbytes = strlen(ss);
	if(!nbytes) {
		CxFree(ss);
		return;
	}

	while (bytes_written < nbytes) {
		retval = write(sock, &ss[bytes_written],
				nbytes - bytes_written);
		if (retval < 1) {
			write (sock, "\n", strlen("\n"));
			return;
		}
		bytes_written = bytes_written + retval;
	}
	CxFree(ss);
}

/**
 ** CxClSend(): Send a string to the server.
 **/
void		CxClSend(int id, const char *s) {
CXHNDL		e;

	e = _CxTbEntry( g_CxTbl, id );
	if(!e) return;
	if(!e->connected) return;

	DPF((DFA,"SEND: \"%s\"", s));
	_CxClSend( e->_sock, s );
}

/**
 ** ClRecvChr(): Retrieve the next message from the server.
 ** *********** SOURCE: citadel-source/citmail.c:serv_read()
 **/
static
void		ClRecvChar(int socket, char *buf, int bytes) {
int 		len, rlen;

        len = 0;
        while (len < bytes) {
                rlen = read(socket, &buf[len], bytes - len);
                if (rlen < 1) {
                        return;
                }
                len = len + rlen;
        }
}

/**
 ** _CxClWait(): Wait on the semaphore.
 **/
static
void		_CxClWait( int *e ) {

	DPF((DFA,"Waiting on Semaphore..."));
	while(*e) ;

	DPF((DFA,"*** LOCKING SESSION ***"));
	(*e) = 1;
}

/**
 ** _CxClClear(): Clear the semaphore.
 **/
static
void		_CxClClear( int *e ) {

	DPF((DFA,"*** CLEARING SESSION ***"));
	(*e) = 0;
}

/**
 ** _CxClRecv(): REAL receive.
 **/
static
int		_CxClRecv( int sock, int *semaphore, char *s, int cxid ) {
char		substr[4];
int		i, tmp;
char		*tmpstr;

	/**
	 ** If the socket is not open, there's no point in going here.
	 **/
	DPF((DFA,"RECV on %d", sock));
	if(!sock) {
		DPF((DFA,"No socket."));
		return( 0 );
	}

	/**
	 ** RETRY_RECV when we have a callback and need to re-synch the protocol.
	 **/
RETRY_RECV:

	_CxClWait( semaphore );
	DPF((DFA,"for(;message <= bottle;) ;"));
 
	/**
	 ** Read one character at a time.
         **/
	for(i = 0; ; i++) {
		ClRecvChar(sock, &s[i], 1);
		if (s[i] == '\n' || i == 255)
			break;
	}
 
	/**
	 ** If we got a long line, discard characters until the newline.
	 **/
	if (i == 255)
		while (s[i] != '\n')
			ClRecvChar(sock, &s[i], 1);

	_CxClClear( semaphore );

	/**
	 ** Strip all trailing nonprintables (crlf)
	 **/
	s[i] = 0;

	DPF((DFA,"PROT <-- \"%s\"", s));

	strncpy(substr,s,4);

	/**
	 ** Check to see if the message is prepended with a server result code.
	 **/
	if(
		(!strcmp(substr,"000")) ||
		((substr[0]>='0' && substr[0]<='9') &&
		 (substr[1]>='0' && substr[1]<='9') &&
		 (substr[2]>='0' && substr[2]<='9') &&
		 ((substr[3]==' ') || (substr[3]=='*')))
	  ) {
		i = (int)strtol(s, (char **)NULL, 10);
		if(substr[3]=='*') i+=RC_MESGWAIT;

		/**
		 ** This removes the result code & other data from the
		 ** returned string.  This is _really_ going to mess with
		 ** lots of code.  Ugh.
		 **
		 ** (Shift the entire string left 4 places.)
		 **/
		for(tmp = 0; tmp < strlen(s); tmp++) {
			if(tmp+4 < strlen(s)) s[tmp] = s[tmp+4];
			else s[tmp] = 0;
		}
		s[tmp] = 0;

		DPF((DFA," s: \"%s\"", s));

	/**
	 ** Otherwise, we can assume that this is an RC_LISTING entry.
	 **/
	} else {
		i = -1;
	}

	/**
	 ** This is the only instance of Goto you'll find in
	 ** libCxClient.  The point is: Once we're done handling
	 ** an asynchronous message, we need to go back & handle
	 ** other messages as normal.
	 **/
	DPF((DFA,"rc = %d (%d)", i, CHECKRC(i, RC_ASYNCMSG)));
	if(i>0) {

		/**
		 ** If the server has told us a secret...
		 **/
		if(CHECKRC(i, RC_ASYNCMSG)) {
			DPF((DFA,"Preparing to process async message on CXID"));

			/**
			 ** Do we have ANY callbacks defined?
			 **/
			if(_CbHandles) {

				/**
				 ** Pass data to callback function, if appropriate.
				 **/
				if(_CxCallback(i, cxid, s)) {

					/**
					 ** ... Callback has failed.  We need to
					 ** proactively ignore this message now.
					 ** NOTE: WE MAY NEED TO ROLL THE SOCKET
					 ** FORWARD TO SKIP ALL OUT-OF-BAND
					 ** MESSAGES!
					 **/

					DPF((DFA,"PROT: ROLL: Rolling socket forward (CALLBACK FAILURE)"));
					tmpstr = (char *)CxMalloc( 255 );
					bzero( tmpstr, 254 );
					i = _CxClRecv( sock, semaphore, tmpstr, cxid );
					do {

						i = _CxClRecv( sock, semaphore, tmpstr, cxid );
						DPF(( DFA,"PROT: ROLL: i: %d", i ));

					} while( i<0 );
					free( tmpstr );
					DPF((DFA,"PROT: ROLL: Cleared OOB data."));

					goto RETRY_RECV;

				/**
				 ** Previously, I returned 000 upon receiving an
				 ** ASYN message.  This was the incorrect behaviour,
				 ** as the expected RECV operation has _not_ been
				 ** completed!  At this point, our Callback should've
				 ** executed appropriately, and we can resume reading
				 ** from the Socket as previously planned.
				 **/
				} else {

				
					goto RETRY_RECV;
				}

			/**
			 ** If there are no callback handles, we need to ignore
			 ** what we just saw.  NOTE: WE MAY NEED TO ROLL THE
			 ** SOCKET FORWARD TO SKIP ALL OUT-OF-BAND MESSAGES!
			 **/
			} else {

					DPF((DFA,"PROT: ROLL: Rolling socket forward (NO CALLBACK)"));
					tmpstr = (char *)CxMalloc( 255 );
					bzero( tmpstr, 254 );
					i = _CxClRecv( sock, semaphore, tmpstr, cxid );
					do {

						i = _CxClRecv( sock, semaphore, tmpstr, cxid );
						DPF(( DFA,"PROT: ROLL: i: %d", i ));

					} while( i<0 );
					free( tmpstr );
					DPF((DFA,"PROT: ROLL: Cleared OOB data."));
					goto RETRY_RECV;

			}
		}
	}
	DPF((DFA,"Preparing to return rc: %d", i));

	return(i);
}

/**
 ** CxClRecv(): Receive a string from the server.
 **/
int		CxClRecv(int id, char *s) {
char		substr[4];
int		i, tmp;
CXHNDL		e;

	DPF((DFA,"Receive on handle %d", id));
	e = _CxTbEntry( g_CxTbl, id );
	if(!e) {
		DPF((DFA,"Handle %d unresolvable", id));
		return(0);
	}
	if(!e->connected) {
		DPF((DFA,"Handle %d not connected", id));
		return(0);
	}

	DPF((DFA,"Preparing to receive on %d", e->_sock));
	return(_CxClRecv( e->_sock, &(e->semaphore), s, id ));
}

/**
 ** CxClChatInit(): Initialize Chat mode
 **/
int		CxClChatInit() {

	if(g_CxChatMode) {
		return(1);
	}

	if(g_CxChatMode) return(1);

	DPF((DFA,"Entering CHAT mode.  Please prepare client for chat-mode\n"));
	DPF((DFA,"communications..."));

	g_CxChatMode = 1;

	return(0);
}

/**
 ** CxClChatShutdown(): Shut down CHAT mode.
 **/
void		CxClChatShutdown() {

	if(!g_CxChatMode) {
		return;
	}
	if(!g_CxChatMode) 
		return;
	DPF((DFA,"Exiting CHAT mode."));

	g_CxChatMode = 0;
}

/*******************************************************************************
 ** Communications Layer Abstractions:  These functions are to be used by the
 ** higher level functions to handle communications with the servers.  The goal
 ** is to abstract client<->server communication as much as possible, in such a
 ** way that changes to the underlying transports don't affect CxClient code.
 **/

/**
 ** struct _Cmd_Stack: These are the commands we are sending to the server.  It
 ** is governed by the MAX_SERV_CMD size limit (which, with the current protocol,
 ** will be "1").
 **
 ** This is an internal structure which is not useful anywhere else in this 
 ** software.
 **/
typedef
struct		_Cmd_Stack {
   char		cmd[4];		/** Command. **/
   CXLIST	arg_list;	/** Arguments. **/
} _CXCSOBJ;

/**
 ** CxClReq(): Send a request message to the server.  On success, returns a numeric
 ** handle to the request id (which, for now, is the command's offset in the 
 ** CMDSTACK.
 **/
int		CxClSrvReq() {
}

/**
 ** CxClReqCancel(): Cancel a pending request.  If the request does not exist in
 ** the CMDSTACK, don't do anything.
 ** // Unimplemented in Citadel/UX 5.74
 **/
int		CxClSrvCnclReq() {
}

/**
 ** CxClCbExists(): Return whether or not a callback exists.  If callback
 ** exists, a pointer is returned to the Callback's handle.  Otherwise,
 ** NULL is returned.
 **/
CXCBHNDL	CxClCbExists(int cmd) {
CXCBHNDL	x;

	DPF((DFA,"[_CbHandles] @0x%08x", _CbHandles));
	x = _CbHandles;
	while( x ) {
		DPF((DFA,"[x] @0x%08x %i", x, cmd));
		if(x->cmd == cmd) {
			DPF((DFA,"[*] Found"));
			return(x);
		}
		x = x->next;
	}
	DPF((DFA,"[X] Not Found"));
	return(NULL);
}

/**
 ** CxClCbRegister(): Register a Transport Protocol callback.
 **/
int		CxClCbRegister(int cmd, void *func) {
CXCBHNDL	new;

	DPF((DFA, "Registering callback for '%d' (@0x%08x)", cmd, func));
	new = 0;

	/**
	 ** Check to see if callback is already in table.  If it is, we'll
	 ** assume that the user intended to REPLACE the existing pointer.
	 **/
	new = CxClCbExists(cmd);

	/**
	 ** If we already exist, we can substitute the existing callback pointer
	 ** with another one and return.  No additional effort is required.
	 **/
	if(new) {
		DPF((DFA, "Replacing existing callback"));
		new->Function = func;
		return(0);

	/**
	 ** Since it doesn't exist in the stack already, we need ta add it
	 ** into the stack.
	 **/
	} else {
		DPF((DFA, "Registering new callback"));
		new = (CXCBHNDL)CxMalloc(sizeof(CXCSCALLBACK));

		new->cmd = cmd;
		new->Function = func;
	
		/**
		 ** If we haven't defined any callbacks yet, we need to define
		 ** this as the 'head' of the Stack.
		 **/
		if( ! _CbHandles ) {
			_CbHandles = new;
			new->next = NULL;

		/**
		 ** ... Otherwise, we need to add the newest callback to the
		 ** head of the stack.
		 **/
		} else {
			new->next = _CbHandles;
			_CbHandles = new;
		}
		DPF((DFA,"[new] @0x%08x",new));
		DPF((DFA,"->next: @0x%08x",new->next));
		DPF((DFA,"[_CbHandles] @0x%08x",_CbHandles));
	}
}

/**
 ** CxClCbShutdown(): Shutdown the callback subsystem.
 **/
void		CxClCbShutdown() {
CXCBHNDL	x, y;

	DPF((DFA,"Shutting down callback subsystem"));
	x = _CbHandles;
	while( x ) {
		y = x;
		x = x->next;
		CxFree(y);
	}
	_CbHandles = 0;
}

/**
 ** _CxCallback(): Execute a callback.
 **/
static
int		_CxCallback(int cmd, int cxid, void *data) {
CXCBHNDL	cb;

	DPF((DFA, "Executing callback %d", cmd));
	cb = CxClCbExists(cmd);

	if(cb) cb->Function(cxid, data);
	else return(1);

	return(0);
}
