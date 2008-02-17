/*
 * $Id$
 *
 * Citadel "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * Here's where we (hopefully) have most parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * 
 * If we ever port to a different platform and either have multiple
 * variants of this file or simply load it up with #ifdefs.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/syslog.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <grp.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "modules/crypto/serv_crypto.h"	/* Needed for init_ssl, client_write_ssl, client_read_ssl, destruct_ssl */
#include "ecrash.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "ctdl_module.h"
#include "threads.h"

#ifdef DEBUG_MEMORY_LEAKS
struct igheap {
	struct igheap *next;
	char file[32];
	int line;
	void *block;
};

struct igheap *igheap = NULL;
#endif


citthread_key_t MyConKey;				/* TSD key for MyContext() */

int verbosity = DEFAULT_VERBOSITY;		/* Logging level */

struct CitContext masterCC;
time_t last_purge = 0;				/* Last dead session purge */
int num_sessions = 0;				/* Current number of sessions */

int syslog_facility = LOG_DAEMON;
int enable_syslog = 0;


/*
 * Create an interface to lprintf that follows the coding convention.
 * This is here until such time as we have replaced all calls to lprintf with CtdlLogPrintf
 */
 
void CtdlLogPrintf(enum LogLevel loglevel, const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	vlprintf(loglevel, format, arg_ptr);
	va_end(arg_ptr);
}


/*
 * lprintf()  ...   Write logging information
 */
void lprintf(enum LogLevel loglevel, const char *format, ...) {   
	va_list arg_ptr;
	va_start(arg_ptr, format);
	vlprintf(loglevel, format, arg_ptr);
	va_end(arg_ptr);
}

void vlprintf(enum LogLevel loglevel, const char *format, va_list arg_ptr)
{
	char buf[SIZ], buf2[SIZ];

	if (enable_syslog) {
		vsyslog((syslog_facility | loglevel), format, arg_ptr);
	}

	/* stderr output code */
	if (enable_syslog || running_as_daemon) return;

	/* if we run in forground and syslog is disabled, log to terminal */
	if (loglevel <= verbosity) { 
		struct timeval tv;
		struct tm tim;
		time_t unixtime;

		gettimeofday(&tv, NULL);
		/* Promote to time_t; types differ on some OSes (like darwin) */
		unixtime = tv.tv_sec;
		localtime_r(&unixtime, &tim);
/*
		if (CC->cs_pid != 0) {
			sprintf(buf,
				"%04d/%02d/%02d %2d:%02d:%02d.%06ld [%3d] ",
				tim.tm_year + 1900, tim.tm_mon + 1,
				tim.tm_mday, tim.tm_hour, tim.tm_min,
				tim.tm_sec, (long)tv.tv_usec,
				CC->cs_pid);
		} else {
			sprintf(buf,
				"%04d/%02d/%02d %2d:%02d:%02d.%06ld ",
				tim.tm_year + 1900, tim.tm_mon + 1,
				tim.tm_mday, tim.tm_hour, tim.tm_min,
				tim.tm_sec, (long)tv.tv_usec);
		}
FIXME temp i want to see CC */
			sprintf(buf,
				"%2d:%02d:%02d.%06ld 0x%08lx ",
				tim.tm_hour, tim.tm_min,
				tim.tm_sec, (long)tv.tv_usec, CC);
		vsnprintf(buf2, SIZ, format, arg_ptr);   

		fprintf(stderr, "%s%s", buf, buf2);
		fflush(stderr);
	}
}   



/*
 * Signal handler to shut down the server.
 */

volatile int exit_signal = 0;
volatile int shutdown_and_halt = 0;
volatile int restart_server = 0;
volatile int running_as_daemon = 0;

static RETSIGTYPE signal_cleanup(int signum) {
	CtdlLogPrintf(CTDL_DEBUG, "Caught signal %d; shutting down.\n", signum);
#ifdef THREADS_USESIGNALS
	if (CT)
	{
		CtdlLogPrintf(CTDL_DEBUG, "Thread \"%s\" caught signal %d.\n", CT->name, signum);
		CT->signal = signum;
	}
	else
#endif
		exit_signal = signum;
}



/*
 * Some initialization stuff...
 */
void init_sysdep(void) {
	sigset_t set;

	/* Avoid vulnerabilities related to FD_SETSIZE if we can. */
#ifdef FD_SETSIZE
#ifdef RLIMIT_NOFILE
	struct rlimit rl;
	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = FD_SETSIZE;
	rl.rlim_max = FD_SETSIZE;
	setrlimit(RLIMIT_NOFILE, &rl);
#endif
#endif

	/* If we've got OpenSSL, we're going to use it. */
#ifdef HAVE_OPENSSL
	init_ssl();
#endif

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * CitContext structure (in the ContextList linked list) of the
	 * session to which the calling thread is currently bound.
	 */
	if (citthread_key_create(&MyConKey, NULL) != 0) {
		CtdlLogPrintf(CTDL_CRIT, "Can't create TSD key: %s\n",
			strerror(errno));
	}

	/*
	 * The action for unexpected signals and exceptions should be to
	 * call signal_cleanup() to gracefully shut down the server.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGTERM);
	// sigaddset(&set, SIGSEGV);	commented out because
	// sigaddset(&set, SIGILL);	we want core dumps
	// sigaddset(&set, SIGBUS);
	sigprocmask(SIG_UNBLOCK, &set, NULL);

	signal(SIGINT, signal_cleanup);
	signal(SIGQUIT, signal_cleanup);
	signal(SIGHUP, signal_cleanup);
	signal(SIGTERM, signal_cleanup);
	// signal(SIGSEGV, signal_cleanup);	commented out because
	// signal(SIGILL, signal_cleanup);	we want core dumps
	// signal(SIGBUS, signal_cleanup);

	/*
	 * Do not shut down the server on broken pipe signals, otherwise the
	 * whole Citadel service would come down whenever a single client
	 * socket breaks.
	 */
	signal(SIGPIPE, SIG_IGN);
}




/*
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 *
 */
int ig_tcp_server(char *ip_addr, int port_number, int queue_len, char **errormessage)
{
	struct sockaddr_in sin;
	int s, i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((u_short)port_number);
	if (ip_addr == NULL) {
		sin.sin_addr.s_addr = INADDR_ANY;
	}
	else {
		sin.sin_addr.s_addr = inet_addr(ip_addr);
	}
										
	if (sin.sin_addr.s_addr == !INADDR_ANY) {
		sin.sin_addr.s_addr = INADDR_ANY;
	}

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
				 "citserver: Can't create a socket: %s",
				 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		return(-1);
	}

	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
				 "citserver: Can't bind: %s",
				 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		close(s);
		return(-1);
	}

	/* set to nonblock - we need this for some obscure situations */
	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
				 "citserver: Can't set socket to non-blocking: %s",
				 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		close(s);
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
				 "citserver: Can't listen: %s",
				 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		close(s);
		return(-1);
	}

	return(s);
}



/*
 * Create a Unix domain socket and listen on it
 */
int ig_uds_server(char *sockpath, int queue_len, char **errormessage)
{
	struct sockaddr_un addr;
	int s;
	int i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	i = unlink(sockpath);
	if (i != 0) if (errno != ENOENT) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, "citserver: can't unlink %s: %s",
			sockpath, strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		return(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
			 "citserver: Can't create a socket: %s",
			 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		return(-1);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
			 "citserver: Can't bind: %s",
			 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		return(-1);
	}

	/* set to nonblock - we need this for some obscure situations */
	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
			 "citserver: Can't set socket to non-blocking: %s",
			 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		close(s);
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		*errormessage = (char*) malloc(SIZ + 1);
		snprintf(*errormessage, SIZ, 
			 "citserver: Can't listen: %s",
			 strerror(errno));
		CtdlLogPrintf(CTDL_EMERG, "%s\n", *errormessage);
		return(-1);
	}

	chmod(sockpath, S_ISGID|S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH);
	return(s);
}



/*
 * Return a pointer to the CitContext structure bound to the thread which
 * called this function.  If there's no such binding (for example, if it's
 * called by the housekeeper thread) then a generic 'master' CC is returned.
 *
 * This function is used *VERY* frequently and must be kept small.
 */
struct CitContext *MyContext(void) {

	register struct CitContext *c;

	return ((c = (struct CitContext *) citthread_getspecific(MyConKey),
		c == NULL) ? &masterCC : c
	);
}


/*
 * Initialize a new context and place it in the list.  The session number
 * used to be the PID (which is why it's called cs_pid), but that was when we
 * had one process per session.  Now we just assign them sequentially, starting
 * at 1 (don't change it to 0 because masterCC uses 0).
 */
struct CitContext *CreateNewContext(void) {
	struct CitContext *me;
	static int next_pid = 0;

	me = (struct CitContext *) malloc(sizeof(struct CitContext));
	if (me == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "citserver: can't allocate memory!!\n");
		return NULL;
	}
	memset(me, 0, sizeof(struct CitContext));

	/* The new context will be created already in the CON_EXECUTING state
	 * in order to prevent another thread from grabbing it while it's
	 * being set up.
	 */
	me->state = CON_EXECUTING;
	/*
	 * Generate a unique session number and insert this context into
	 * the list.
	 */
	begin_critical_section(S_SESSION_TABLE);
	me->cs_pid = ++next_pid;
	me->prev = NULL;
	me->next = ContextList;
	ContextList = me;
	if (me->next != NULL) {
		me->next->prev = me;
	}
	++num_sessions;
	end_critical_section(S_SESSION_TABLE);
	return (me);
}


/*
 * The following functions implement output buffering. If the kernel supplies
 * native TCP buffering (Linux & *BSD), use that; otherwise, emulate it with
 * user-space buffering.
 */
#ifndef HAVE_DARWIN
#ifdef TCP_CORK
#	define HAVE_TCP_BUFFERING
#else
#	ifdef TCP_NOPUSH
#		define HAVE_TCP_BUFFERING
#		define TCP_CORK TCP_NOPUSH
#	endif
#endif /* TCP_CORK */
#endif /* HAVE_DARWIN */

#ifdef HAVE_TCP_BUFFERING
static unsigned on = 1, off = 0;
void buffer_output(void) {
	struct CitContext *ctx = MyContext();
	setsockopt(ctx->client_socket, IPPROTO_TCP, TCP_CORK, &on, 4);
	ctx->buffering = 1;
}

void unbuffer_output(void) {
	struct CitContext *ctx = MyContext();
	setsockopt(ctx->client_socket, IPPROTO_TCP, TCP_CORK, &off, 4);
	ctx->buffering = 0;
}

void flush_output(void) {
	struct CitContext *ctx = MyContext();
	setsockopt(ctx->client_socket, IPPROTO_TCP, TCP_CORK, &off, 4);
	setsockopt(ctx->client_socket, IPPROTO_TCP, TCP_CORK, &on, 4);
}
#else 
#ifdef HAVE_DARWIN
/* Stub functions for Darwin/OS X where TCP buffering isn't liked at all */
void buffer_output(void) {
	CC->buffering = 0;
}
void unbuffer_output(void) {
	CC->buffering = 0;
}
void flush_output(void) {
}
#else
void buffer_output(void) {
	if (CC->buffering == 0) {
		CC->buffering = 1;
		CC->buffer_len = 0;
		CC->output_buffer = malloc(SIZ);
	}
}

void flush_output(void) {
	if (CC->buffering == 1) {
		client_write(CC->output_buffer, CC->buffer_len);
		CC->buffer_len = 0;
	}
}

void unbuffer_output(void) {
	if (CC->buffering == 1) {
		CC->buffering = 0;
		/* We don't call flush_output because we can't. */
		client_write(CC->output_buffer, CC->buffer_len);
		CC->buffer_len = 0;
		free(CC->output_buffer);
		CC->output_buffer = NULL;
	}
}
#endif /* HAVE_DARWIN */
#endif /* HAVE_TCP_BUFFERING */



/*
 * client_write()   ...    Send binary data to the client.
 */
void client_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
#ifndef HAVE_TCP_BUFFERING
	int old_buffer_len = 0;
#endif
	fd_set wset;
	t_context *Ctx;
	int fdflags;

	Ctx = CC;
	if (Ctx->redirect_buffer != NULL) {
		if ((Ctx->redirect_len + nbytes + 2) >= Ctx->redirect_alloc) {
			Ctx->redirect_alloc = (Ctx->redirect_alloc * 2) + nbytes;
			Ctx->redirect_buffer = realloc(Ctx->redirect_buffer,
						Ctx->redirect_alloc);
		}
		memcpy(&Ctx->redirect_buffer[Ctx->redirect_len], buf, nbytes);
		Ctx->redirect_len += nbytes;
		Ctx->redirect_buffer[Ctx->redirect_len] = 0;
		return;
	}

#ifndef HAVE_TCP_BUFFERING
	/* If we're buffering for later, do that now. */
	if (Ctx->buffering) {
		old_buffer_len = Ctx->buffer_len;
		Ctx->buffer_len += nbytes;
		Ctx->output_buffer = realloc(Ctx->output_buffer, Ctx->buffer_len);
		memcpy(&Ctx->output_buffer[old_buffer_len], buf, nbytes);
		return;
	}
#endif

	/* Ok, at this point we're not buffering.  Go ahead and write. */

#ifdef HAVE_OPENSSL
	if (Ctx->redirect_ssl) {
		client_write_ssl(buf, nbytes);
		return;
	}
#endif

	fdflags = fcntl(Ctx->client_socket, F_GETFL);

	while (bytes_written < nbytes) {
		if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
			FD_ZERO(&wset);
			FD_SET(Ctx->client_socket, &wset);
			if (select(1, NULL, &wset, NULL, NULL) == -1) {
				CtdlLogPrintf(CTDL_ERR,
					"client_write(%d bytes) select failed: %s (%d)\n",
					nbytes - bytes_written,
					strerror(errno), errno);
				cit_backtrace();
				Ctx->kill_me = 1;
				return;
			}
		}

		retval = write(Ctx->client_socket, &buf[bytes_written],
			nbytes - bytes_written);
		if (retval < 1) {
			CtdlLogPrintf(CTDL_ERR,
				"client_write(%d bytes) failed: %s (%d)\n",
				nbytes - bytes_written,
				strerror(errno), errno);
			cit_backtrace();
			// CtdlLogPrintf(CTDL_DEBUG, "Tried to send: %s",  &buf[bytes_written]);
			Ctx->kill_me = 1;
			return;
		}
		bytes_written = bytes_written + retval;
	}
}


/*
 * cprintf()  ...   Send formatted printable data to the client.   It is
 *		  implemented in terms of client_write() but remains in
 *		  sysdep.c in case we port to somewhere without va_args...
 */
void cprintf(const char *format, ...) {   
	va_list arg_ptr;   
	char buf[1024];   
   
	va_start(arg_ptr, format);   
	if (vsnprintf(buf, sizeof buf, format, arg_ptr) == -1)
		buf[sizeof buf - 2] = '\n';
	client_write(buf, strlen(buf)); 
	va_end(arg_ptr);
}   


/*
 * Read data from the client socket.
 * Return values are:
 *	1	Requested number of bytes has been read.
 *	0	Request timed out.
 *	-1	The socket is broken.
 * If the socket breaks, the session will be terminated.
 */
int client_read_to(char *buf, int bytes, int timeout)
{
	int len,rlen;
	fd_set rfds;
	int fd;
	struct timeval tv;
	int retval;

#ifdef HAVE_OPENSSL
	if (CC->redirect_ssl) {
		return (client_read_ssl(buf, bytes, timeout));
	}
#endif
	len = 0;
	fd = CC->client_socket;
	while(len<bytes) {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select( (fd)+1, 
				 &rfds, NULL, NULL, &tv);

		if (FD_ISSET(fd, &rfds) == 0) {
			return(0);
		}

		rlen = read(fd, &buf[len], bytes-len);
		if (rlen<1) {
			/* The socket has been disconnected! */
			CC->kill_me = 1;
			return(-1);
		}
		len = len + rlen;
	}
	return(1);
}

/*
 * Read data from the client socket with default timeout.
 * (This is implemented in terms of client_read_to() and could be
 * justifiably moved out of sysdep.c)
 */
INLINE int client_read(char *buf, int bytes)
{
	return(client_read_to(buf, bytes, config.c_sleeping));
}


/*
 * client_getln()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int client_getln(char *buf, int bufsize)
{
	int i, retval;

	/* Read one character at a time.
	 */
	for (i = 0;;i++) {
		retval = client_read(&buf[i], 1);
		if (retval != 1 || buf[i] == '\n' || i == (bufsize-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (bufsize-1))
		while (buf[i] != '\n' && retval == 1)
			retval = client_read(&buf[i], 1);

	/* Strip the trailing LF, and the trailing CR if present.
	 */
	buf[i] = 0;
	while ( (i > 0)
		&& ( (buf[i - 1]==13)
		     || ( buf[i - 1]==10)) ) {
		i--;
		buf[i] = 0;
	}
	if (retval < 0) safestrncpy(&buf[i], "000", bufsize - i);
	return(retval);
}


/*
 * Cleanup any contexts that are left lying around
 */
void context_cleanup(void)
{
	struct CitContext *ptr = NULL;
	struct CitContext *rem = NULL;

	/*
	 * Clean up the contexts.
	 * There are no threads so no critical_section stuff is needed.
	 */
	ptr = ContextList;
	
	/* We need to update the ContextList because some modules may want to itterate it
	 * Question is should we NULL it before iterating here or should we just keep updating it
	 * as we remove items?
	 *
	 * Answer is to NULL it first to prevent modules from doing any actions on the list at all
	 */
	ContextList=NULL;
	while (ptr != NULL){
		/* Remove the session from the active list */
		rem = ptr->next;
		--num_sessions;
		
		lprintf(CTDL_DEBUG, "Purging session %d\n", ptr->cs_pid);
		RemoveContext(ptr);
		free (ptr);
		ptr = rem;
	}
}



void close_masters (void)
{
	struct ServiceFunctionHook *serviceptr;
	
	/*
	 * close all protocol master sockets
	 */
	for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    serviceptr = serviceptr->next ) {

		if (serviceptr->tcp_port > 0)
		{
			CtdlLogPrintf(CTDL_INFO, "Closing listener on port %d\n",
				serviceptr->tcp_port);
			serviceptr->tcp_port = 0;
		}
		
		if (serviceptr->sockpath != NULL)
			CtdlLogPrintf(CTDL_INFO, "Closing listener on '%s'\n",
				serviceptr->sockpath);

		close(serviceptr->msock);
		/* If it's a Unix domain socket, remove the file. */
		if (serviceptr->sockpath != NULL) {
			unlink(serviceptr->sockpath);
			serviceptr->sockpath = NULL;
		}
	}
}


/*
 * The system-dependent part of master_cleanup() - close the master socket.
 */
void sysdep_master_cleanup(void) {
	
	close_masters();
	
	context_cleanup();
	
#ifdef HAVE_OPENSSL
	destruct_ssl();
#endif
	CtdlDestroyProtoHooks();
	CtdlDestroyDeleteHooks();
	CtdlDestroyXmsgHooks();
	CtdlDestroyNetprocHooks();
	CtdlDestroyUserHooks();
	CtdlDestroyMessageHook();
	CtdlDestroyCleanupHooks();
	CtdlDestroyFixedOutputHooks();	
	CtdlDestroySessionHooks();
	CtdlDestroyServiceHook();
	CtdlDestroyRoomHooks();
	CtdlDestroyDirectoryServiceFuncs();
	#ifdef HAVE_BACKTRACE
	eCrash_Uninit();
	#endif
}



/*
 * Terminate another session.
 * (This could justifiably be moved out of sysdep.c because it
 * no longer does anything that is system-dependent.)
 */
void kill_session(int session_to_kill) {
	struct CitContext *ptr;

	begin_critical_section(S_SESSION_TABLE);
	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if (ptr->cs_pid == session_to_kill) {
			ptr->kill_me = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
}

pid_t current_child;
void graceful_shutdown(int signum) {
	kill(current_child, signum);
	unlink(file_pid_file);
	exit(0);
}


/*
 * Start running as a daemon.
 */
void start_daemon(int unused) {
	int status = 0;
	pid_t child = 0;
	FILE *fp;
	int do_restart = 0;

	current_child = 0;

	/* Close stdin/stdout/stderr and replace them with /dev/null.
	 * We don't just call close() because we don't want these fd's
	 * to be reused for other files.
	 */
	chdir(ctdl_run_dir);

	child = fork();
	if (child != 0) {
		exit(0);
	}
	
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	setsid();
	umask(0);
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);

	do {
		current_child = fork();

		signal(SIGTERM, graceful_shutdown);
	
		if (current_child < 0) {
			perror("fork");
			exit(errno);
		}
	
		else if (current_child == 0) {
			return; /* continue starting citadel. */
		}
	
		else {
			fp = fopen(file_pid_file, "w");
			if (fp != NULL) {
				fprintf(fp, ""F_PID_T"\n", getpid());
				fclose(fp);
			}
			waitpid(current_child, &status, 0);
		}

		do_restart = 0;

		/* Did the main process exit with an actual exit code? */
		if (WIFEXITED(status)) {

			/* Exit code 0 means the watcher should exit */
			if (WEXITSTATUS(status) == 0) {
				do_restart = 0;
			}

			/* Exit code 101-109 means the watcher should exit */
			else if ( (WEXITSTATUS(status) >= 101) && (WEXITSTATUS(status) <= 109) ) {
				do_restart = 0;
			}

			/* Any other exit code means we should restart. */
			else {
				do_restart = 1;
			}
		}

		/* Any other type of termination (signals, etc.) should also restart. */
		else {
			do_restart = 1;
		}

	} while (do_restart);

	unlink(file_pid_file);
	exit(WEXITSTATUS(status));
}



/*
 * Generic routine to convert a login name to a full name (gecos)
 * Returns nonzero if a conversion took place
 */
int convert_login(char NameToConvert[]) {
	struct passwd *pw;
	int a;

	pw = getpwnam(NameToConvert);
	if (pw == NULL) {
		return(0);
	}
	else {
		strcpy(NameToConvert, pw->pw_gecos);
		for (a=0; a<strlen(NameToConvert); ++a) {
			if (NameToConvert[a] == ',') NameToConvert[a] = 0;
		}
		return(1);
	}
}

/*
 * Purge all sessions which have the 'kill_me' flag set.
 * This function has code to prevent it from running more than once every
 * few seconds, because running it after every single unbind would waste a lot
 * of CPU time and keep the context list locked too much.  To force it to run
 * anyway, set "force" to nonzero.
 */
void dead_session_purge(int force) {
	struct CitContext *ptr, *ptr2;		/* general-purpose utility pointer */
	struct CitContext *rem = NULL;	/* list of sessions to be destroyed */
	
	if (force == 0) {
		if ( (time(NULL) - last_purge) < 5 ) {
			return;	/* Too soon, go away */
		}
	}
	time(&last_purge);

	if (try_critical_section(S_SESSION_TABLE))
		return;
		
	ptr = ContextList;
	while (ptr) {
		ptr2 = ptr;
		ptr = ptr->next;
		
		if ( (ptr2->state == CON_IDLE) && (ptr2->kill_me) ) {
			/* Remove the session from the active list */
			if (ptr2->prev) {
				ptr2->prev->next = ptr2->next;
			}
			else {
				ContextList = ptr2->next;
			}
			if (ptr2->next) {
				ptr2->next->prev = ptr2->prev;
			}

			--num_sessions;
			/* And put it on our to-be-destroyed list */
			ptr2->next = rem;
			rem = ptr2;
		}
	}
	end_critical_section(S_SESSION_TABLE);

	/* Now that we no longer have the session list locked, we can take
	 * our time and destroy any sessions on the to-be-killed list, which
	 * is allocated privately on this thread's stack.
	 */
	while (rem != NULL) {
		CtdlLogPrintf(CTDL_DEBUG, "Purging session %d\n", rem->cs_pid);
		RemoveContext(rem);
		ptr = rem;
		rem = rem->next;
		free(ptr);
	}
}





/*
 * masterCC is the context we use when not attached to a session.  This
 * function initializes it.
 */
void InitializeMasterCC(void) {
	memset(&masterCC, 0, sizeof(struct CitContext));
	masterCC.internal_pgm = 1;
	masterCC.cs_pid = 0;
}






/*
 * Bind a thread to a context.  (It's inline merely to speed things up.)
 */
INLINE void become_session(struct CitContext *which_con) {
	citthread_setspecific(MyConKey, (void *)which_con );
}



/* 
 * This loop just keeps going and going and going...
 */
/*
 * FIXME:
 * This current implimentation of worker_thread creates a bottle neck in several situations
 * The first thing to remember is that a single thread can handle more than one connection at a time.
 * More threads mean less memory for the system to run in.
 * So for efficiency we want every thread to be doing something useful or waiting in the main loop for
 * something to happen anywhere.
 * This current implimentation requires worker threads to wait in other locations, after it has
 * been committed to a single connection which is very wasteful.
 * As an extreme case consider this:
 * A slow client connects and this slow client sends only one character each second.
 * With this current implimentation a single worker thread is dispatched to handle that connection
 * until such times as the client timeout expires, an error occurs on the socket or the client
 * completes its transmission.
 * THIS IS VERY BAD since that thread could have handled a read from many more clients in each one
 * second interval between chars.
 *
 * It is my intention to re-write this code and the associated client_getln, client_read functions
 * to allow any thread to read data on behalf of any connection (context).
 * To do this I intend to have this main loop read chars into a buffer stored in the context.
 * Once the correct criteria for a full buffer is met then we will dispatch a thread to 
 * process it.
 * This worker thread loop also needs to be able to handle binary data.
 */
 
void *worker_thread(void *arg) {
	int i;
	int highest;
	struct CitContext *ptr;
	struct CitContext *bind_me = NULL;
	fd_set readfds;
	int retval = 0;
	struct CitContext *con= NULL;	/* Temporary context pointer */
	struct ServiceFunctionHook *serviceptr;
	int ssock;			/* Descriptor for client socket */
	struct timeval tv;
	int force_purge = 0;
	int m;
	

	while (!CtdlThreadCheckStop()) {

		/* make doubly sure we're not holding any stale db handles
		 * which might cause a deadlock.
		 */
		cdb_check_handles();
do_select:	force_purge = 0;
		bind_me = NULL;		/* Which session shall we handle? */

		/* Initialize the fdset. */
		FD_ZERO(&readfds);
		highest = 0;

		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->state == CON_IDLE) {
				FD_SET(ptr->client_socket, &readfds);
				if (ptr->client_socket > highest)
					highest = ptr->client_socket;
			}
			if ((bind_me == NULL) && (ptr->state == CON_READY)) {
				bind_me = ptr;
				ptr->state = CON_EXECUTING;
			}
		}
		end_critical_section(S_SESSION_TABLE);

		if (bind_me) {
			goto SKIP_SELECT;
		}

		/* If we got this far, it means that there are no sessions
		 * which a previous thread marked for attention, so we go
		 * ahead and get ready to select().
		 */

		/* First, add the various master sockets to the fdset. */
		for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    	serviceptr = serviceptr->next ) {
			m = serviceptr->msock;
			FD_SET(m, &readfds);
			if (m > highest) {
				highest = m;
			}
		}

		if (!CtdlThreadCheckStop()) {
			tv.tv_sec = 1;		/* wake up every second if no input */
			tv.tv_usec = 0;
			retval = CtdlThreadSelect(highest + 1, &readfds, NULL, NULL, &tv);
		}

		if (CtdlThreadCheckStop()) return(NULL);

		/* Now figure out who made this select() unblock.
		 * First, check for an error or exit condition.
		 */
		if (retval < 0) {
			if (errno == EBADF) {
				CtdlLogPrintf(CTDL_NOTICE, "select() failed: (%s)\n",
					strerror(errno));
				goto do_select;
			}
			if (errno != EINTR) {
				CtdlLogPrintf(CTDL_EMERG, "Exiting (%s)\n", strerror(errno));
				CtdlThreadStopAll();
			} else if (!CtdlThreadCheckStop()) {
				CtdlLogPrintf(CTDL_DEBUG, "Interrupted select.\n");
				goto do_select;
			}
		}
		else if(retval == 0) {
			goto SKIP_SELECT;
		}
		/* Next, check to see if it's a new client connecting
		 * on a master socket.
		 */
		else for (serviceptr = ServiceHookTable; serviceptr != NULL;
		     serviceptr = serviceptr->next ) {

			if (FD_ISSET(serviceptr->msock, &readfds)) {
				ssock = accept(serviceptr->msock, NULL, 0);
				if (ssock >= 0) {
					CtdlLogPrintf(CTDL_DEBUG,
						"New client socket %d\n",
						ssock);

					/* The master socket is non-blocking but the client
					 * sockets need to be blocking, otherwise certain
					 * operations barf on FreeBSD.  Not a fatal error.
					 */
					if (fcntl(ssock, F_SETFL, 0) < 0) {
						CtdlLogPrintf(CTDL_EMERG,
							"citserver: Can't set socket to blocking: %s\n",
							strerror(errno));
					}

					/* New context will be created already
					 * set up in the CON_EXECUTING state.
					 */
					con = CreateNewContext();

					/* Assign our new socket number to it. */
					con->client_socket = ssock;
					con->h_command_function =
						serviceptr->h_command_function;
					con->h_async_function =
						serviceptr->h_async_function;
					con->ServiceName =
						serviceptr->ServiceName;
					
					/* Determine whether it's a local socket */
					if (serviceptr->sockpath != NULL)
						con->is_local_socket = 1;
	
					/* Set the SO_REUSEADDR socket option */
					i = 1;
					setsockopt(ssock, SOL_SOCKET,
						SO_REUSEADDR,
						&i, sizeof(i));

					become_session(con);
					begin_session(con);
					serviceptr->h_greeting_function();
					become_session(NULL);
					con->state = CON_IDLE;
					goto do_select;
				}
			}
		}

		/* It must be a client socket.  Find a context that has data
		 * waiting on its socket *and* is in the CON_IDLE state.  Any
		 * active sockets other than our chosen one are marked as
		 * CON_READY so the next thread that comes around can just bind
		 * to one without having to select() again.
		 */
		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if ( (FD_ISSET(ptr->client_socket, &readfds))
			   && (ptr->state != CON_EXECUTING) ) {
				ptr->input_waiting = 1;
				if (!bind_me) {
					bind_me = ptr;	/* I choose you! */
					bind_me->state = CON_EXECUTING;
				}
				else {
					ptr->state = CON_READY;
				}
			}
		}
		end_critical_section(S_SESSION_TABLE);

SKIP_SELECT:
		/* We're bound to a session */
		if (bind_me != NULL) {
			become_session(bind_me);

			/* If the client has sent a command, execute it. */
			if (CC->input_waiting) {
				CC->h_command_function();
				CC->input_waiting = 0;
			}

			/* If there are asynchronous messages waiting and the
			 * client supports it, do those now */
			if ((CC->is_async) && (CC->async_waiting)
			   && (CC->h_async_function != NULL)) {
				CC->h_async_function();
				CC->async_waiting = 0;
			}
			
			force_purge = CC->kill_me;
			become_session(NULL);
			bind_me->state = CON_IDLE;
		}

		dead_session_purge(force_purge);
		do_housekeeping();
	}
	/* If control reaches this point, the server is shutting down */	
	return(NULL);
}




/*
 * SyslogFacility()
 * Translate text facility name to syslog.h defined value.
 */
int SyslogFacility(char *name)
{
	int i;
	struct
	{
		int facility;
		char *name;
	}   facTbl[] =
	{
		{   LOG_KERN,   "kern"		},
		{   LOG_USER,   "user"		},
		{   LOG_MAIL,   "mail"		},
		{   LOG_DAEMON, "daemon"	},
		{   LOG_AUTH,   "auth"		},
		{   LOG_SYSLOG, "syslog"	},
		{   LOG_LPR,	"lpr"		},
		{   LOG_NEWS,   "news"		},
		{   LOG_UUCP,   "uucp"		},
		{   LOG_LOCAL0, "local0"	},
		{   LOG_LOCAL1, "local1"	},
		{   LOG_LOCAL2, "local2"	},
		{   LOG_LOCAL3, "local3"	},
		{   LOG_LOCAL4, "local4"	},
		{   LOG_LOCAL5, "local5"	},
		{   LOG_LOCAL6, "local6"	},
		{   LOG_LOCAL7, "local7"	},
		{   0,		  NULL		}
	};
	for(i = 0; facTbl[i].name != NULL; i++) {
		if(!strcasecmp(name, facTbl[i].name))
			return facTbl[i].facility;
	}
	enable_syslog = 0;
	return LOG_DAEMON;
}


/********** MEM CHEQQER ***********/

#ifdef DEBUG_MEMORY_LEAKS

#undef malloc
#undef realloc
#undef strdup
#undef free

void *tracked_malloc(size_t size, char *file, int line) {
	struct igheap *thisheap;
	void *block;

	block = malloc(size);
	if (block == NULL) return(block);

	thisheap = malloc(sizeof(struct igheap));
	if (thisheap == NULL) {
		free(block);
		return(NULL);
	}

	thisheap->block = block;
	strcpy(thisheap->file, file);
	thisheap->line = line;
	
	begin_critical_section(S_DEBUGMEMLEAKS);
	thisheap->next = igheap;
	igheap = thisheap;
	end_critical_section(S_DEBUGMEMLEAKS);

	return(block);
}


void *tracked_realloc(void *ptr, size_t size, char *file, int line) {
	struct igheap *thisheap;
	void *block;

	block = realloc(ptr, size);
	if (block == NULL) return(block);

	thisheap = malloc(sizeof(struct igheap));
	if (thisheap == NULL) {
		free(block);
		return(NULL);
	}

	thisheap->block = block;
	strcpy(thisheap->file, file);
	thisheap->line = line;
	
	begin_critical_section(S_DEBUGMEMLEAKS);
	thisheap->next = igheap;
	igheap = thisheap;
	end_critical_section(S_DEBUGMEMLEAKS);

	return(block);
}



void tracked_free(void *ptr) {
	struct igheap *thisheap;
	struct igheap *trash;

	free(ptr);

	if (igheap == NULL) return;
	begin_critical_section(S_DEBUGMEMLEAKS);
	for (thisheap = igheap; thisheap != NULL; thisheap = thisheap->next) {
		if (thisheap->next != NULL) {
			if (thisheap->next->block == ptr) {
				trash = thisheap->next;
				thisheap->next = thisheap->next->next;
				free(trash);
			}
		}
	}
	if (igheap->block == ptr) {
		trash = igheap;
		igheap = igheap->next;
		free(trash);
	}
	end_critical_section(S_DEBUGMEMLEAKS);
}

char *tracked_strdup(const char *s, char *file, int line) {
	char *ptr;

	if (s == NULL) return(NULL);
	ptr = tracked_malloc(strlen(s) + 1, file, line);
	if (ptr == NULL) return(NULL);
	strncpy(ptr, s, strlen(s));
	return(ptr);
}

void dump_heap(void) {
	struct igheap *thisheap;

	for (thisheap = igheap; thisheap != NULL; thisheap = thisheap->next) {
		CtdlLogPrintf(CTDL_CRIT, "UNFREED: %30s : %d\n",
			thisheap->file, thisheap->line);
	}
}

#endif /*  DEBUG_MEMORY_LEAKS */
