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
static int	_CxCallback(int cmd, void *data);
static void	timeout() {}

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
 **/
int		CxClConnect(const char *host) {
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

	DPF((DFA,"(Library was built with UNIX_SOCKET support)"));
	DPF((DFA,"Establishing connection to host \"%s\"",host));

        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;

        pse = getservbyname(service, protocol);
        if(pse) {
                sin.sin_port = pse->s_port;
        } else if((sin.sin_port = htons((u_short) atoi(service))) != 0) {
        } else {
		sin.sin_port = htons((u_short)504);
	}
        phe = gethostbyname(host);
        if (phe) {
                memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);

        } else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		printf("\n* * *  Fatal Error  * * *\n");
		printf("System Error: Can't get host entry for '%s'\n", host);
		printf("Error Details: %s\n\n", strerror(errno));
                return(-1);
        }
        if ((ppe = getprotobyname(protocol)) == 0) {
                fprintf(stderr, "Can't get %s protocol entry: %s\n",
                        protocol, strerror(errno));
                return(-1);
        }
        if (!strcmp(protocol, "udp")) {
                type = SOCK_DGRAM;
        } else {
                type = SOCK_STREAM;
        }

        s = socket(PF_INET, type, ppe->p_proto);
        if (s < 0) {
		printf("\n* * *  Fatal Error  * * *\n");
		printf("System Error: Can't create socket\n");
		printf("Error Details: %s\n\n", strerror(errno));
                return(-1);
        }
        signal(SIGALRM, timeout);
        alarm(30);
         if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		printf("\n* * *  Fatal Error  * * *\n");
		printf("System Error: Can't connect to '%s' [%s]\n",host, service);
		printf("Error Details: %s\n\n", strerror(errno));
                return(-1);
        }
        alarm(0);
        signal(SIGALRM, SIG_IGN);

	g_CxSocket = s;

	if(s) {
		CxClRecv(buf);
		if(g_CxClientName[0]) {
			sprintf(buf,"IDEN 1|1|100|libCxClient/%s (%s)|",VERSION, g_CxClientName);
		} else {
			sprintf(buf,"IDEN 1|1|100|libCxClient/%s (unknown)|",VERSION);
		}
		CxClSend(buf);
		CxClRecv(buf);
		CxClSend("ASYN 1");
		rc = CxClRecv(buf);

		/**
		 ** If the server doesn't support Asnychronous mode, then
		 ** we shouldn't try to be asynchronous...
		 **/
		if(CHECKRC(rc, RC_OK)) {
			g_CxAsynMode = 1;
		} else {
			g_CxAsynMode = 0;
		}

		/**
		 ** We don't return our socket anymore.
		 **/
        	return(0);
	}
	return(0);
}

/**
 ** CxClDisconnect(): Disconnect the socket.
 **/
void		CxClDisconnect() {

	DPF((DFA,"Caught orders to close socket."));

	shutdown(g_CxSocket, 0);
}

/**
 ** CxClStat(): Send a string to the server.
 **/
int		CxClStat() {

	if(g_CxSocket) {
		return(1);
	} else {
		return(0);
	}
}

/**
 ** CxClSend(): Send a string to the server.
 **/
void		CxClSend(const char *s) {
int 		bytes_written = 0;
int 		retval,nbytes;
char		*ss;

	DPF((DFA,"SEND: \"%s\"", s));

	ss = (char *)CxMalloc(strlen(s)+2);
	sprintf(ss,"%s\n",s);

	nbytes = strlen(ss);
	if(!nbytes) {
		CxFree(ss);
		return;
	}

        while (bytes_written < nbytes) {
                retval = write(g_CxSocket, &ss[bytes_written],
                               nbytes - bytes_written);
                if (retval < 1) {
			write (g_CxSocket, "\n", strlen("\n"));
                        return;
                }
                bytes_written = bytes_written + retval;
        }
	CxFree(ss);
}

/**
 ** ClRecvChr(): Retrieve the next message from the server.
 ** *********** SOURCE: citadel-source/citmail.c:serv_read()
 **/
static
void		ClRecvChar(char *buf, int bytes) {
int 		len, rlen;

        len = 0;
        while (len < bytes) {
                rlen = read(g_CxSocket, &buf[len], bytes - len);
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
void		_CxClWait() {

	DPF((DFA,"Waiting on Semaphore..."));
	while(g_CxSemaphore) ;

	DPF((DFA,"*** LOCKING SESSION ***"));
	g_CxSemaphore = 1;
}

/**
 ** _CxClClear(): Clear the semaphore.
 **/
static
void		_CxClClear() {

	DPF((DFA,"*** CLEARING SESSION ***"));
	g_CxSemaphore = 0;
}

/**
 ** CxClRecv(): Receive a string from the server.
 **/
int		CxClRecv(char *s) {
char		substr[4];
int		i, tmp;

	if(!CxClStat()) {
		return(NULL);
	}

	/**
	 ** At this point, we should wait for the semaphore to be cleared.
	 ** This will prevent multi-threaded clients from pissing all over
	 ** themselves when 2 threads attempt to read at the same time...
	 **/
	_CxClWait();

	/**
	 ** RETRY_RECV when we have a callback and need to re-synch the protocol.
	 **/
RETRY_RECV:

	DPF((DFA,"for(;message <= bottle;) ;"));
 
	/**
	 ** Read one character at a time.
         **/
	for(i = 0; ; i++) {
		ClRecvChar(&s[i], 1);
		if (s[i] == '\n' || i == 255)
			break;
	}
 
	/**
	 ** If we got a long line, discard characters until the newline.
	 **/
	if (i == 255)
		while (s[i] != '\n')
			ClRecvChar(&s[i], 1);
 

	/**
	 ** Strip all trailing nonprintables (crlf)
	 **/
	s[i] = 0;

	DPF((DFA,"I got \"%s\"",s));

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
		 **/
		DPF((DFA," s: \"%s\"", s));

		/**
		 ** Shift the entire string left 4 places.
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
	 ** We wish to clear the semaphore BEFORE executing any callbacks.
	 ** This will help to prevent nasty race conditions.  >:)
	 **/
	_CxClClear();

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
			DPF((DFA,"Preparing to process async message"));

			/**
			 ** Do we have ANY callbacks defined?
			 **/
			if(_CbHandles) {

				/**
				 ** Pass data to callback function, if appropriate.
				 **/
				if(_CxCallback(i, s)) {

					/**
					 ** ... Callback has failed.  We need to
					 ** proactively ignore this message now.
					 **/

					/** INCOMPLETE **/

				}

				goto RETRY_RECV;

			} else {

					/** INCOMPLETE **/

			}
		}
	}
	DPF((DFA,"Preparing to return rc: %d", i));

	return(i);
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

	return(0);
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

	return(0);

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

	return(0);
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
int		_CxCallback(int cmd, void *data) {
CXCBHNDL	cb;

	return(0);

	DPF((DFA, "Executing callback %d", cmd));
	cb = CxClCbExists(cmd);

	if(cb) cb->Function(data);
	else return(1);

	return(0);
}
