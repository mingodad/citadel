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
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
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

#ifdef DEBUG_MEMORY_LEAKS
struct igheap {
	struct igheap *next;
	char file[32];
	int line;
	void *block;
};

struct igheap *igheap = NULL;
#endif


pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
pthread_key_t MyConKey;				/* TSD key for MyContext() */

int verbosity = DEFAULT_VERBOSITY;		/* Logging level */

struct CitContext masterCC;
time_t last_purge = 0;				/* Last dead session purge */
static int num_threads = 0;			/* Current number of threads */
static int num_workers = 0;			/* Current number of worker threads */
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
		vsprintf(buf2, format, arg_ptr);   

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
	CtdlThreadStopAll();
	exit_signal = signum;
}




void InitialiseSemaphores(void)
{
	int i;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (i=0; i<MAX_SEMAPHORES; ++i) {
		pthread_mutex_init(&Critters[i], NULL);
	}
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
	if (pthread_key_create(&MyConKey, NULL) != 0) {
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
	//signal(SIGPIPE, SIG_IGN);
	signal(SIGPIPE, signal_cleanup);
}


/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	/* CtdlLogPrintf(CTDL_DEBUG, "begin_critical_section(%d)\n", which_one); */

	/* For all types of critical sections except those listed here,
	 * ensure nobody ever tries to do a critical section within a
	 * transaction; this could lead to deadlock.
	 */
	if (	(which_one != S_FLOORCACHE)
#ifdef DEBUG_MEMORY_LEAKS
		&& (which_one != S_DEBUGMEMLEAKS)
#endif
		&& (which_one != S_RPLIST)
	) {
		cdb_check_handles();
	}
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	pthread_mutex_unlock(&Critters[which_one]);
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

	return ((c = (struct CitContext *) pthread_getspecific(MyConKey),
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
	return(me);
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
	t_context *Ctx;

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

	while (bytes_written < nbytes) {
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


/*
 * The system-dependent part of master_cleanup() - close the master socket.
 */
void sysdep_master_cleanup(void) {
	struct ServiceFunctionHook *serviceptr;
	
	/*
	 * close all protocol master sockets
	 */
	for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    serviceptr = serviceptr->next ) {

		if (serviceptr->tcp_port > 0)
			CtdlLogPrintf(CTDL_INFO, "Closing listener on port %d\n",
				serviceptr->tcp_port);

		if (serviceptr->sockpath != NULL)
			CtdlLogPrintf(CTDL_INFO, "Closing listener on '%s'\n",
				serviceptr->sockpath);

		close(serviceptr->msock);

		/* If it's a Unix domain socket, remove the file. */
		if (serviceptr->sockpath != NULL) {
			unlink(serviceptr->sockpath);
		}
	}
	
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
		/*
		 * NB.. The pid file contains the pid of the actual server.
		 * This is not the pid of the watcher process
		 */
				fprintf(fp, ""F_PID_T"\n", current_child);
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
 * New thread interface.
 * To create a thread you must call one of the create thread functions.
 * You must pass it the address of (a pointer to a CtdlThreadNode initialised to NULL) like this
 * struct CtdlThreadNode *node = NULL;
 * pass in &node
 * If the thread is created *node will point to the thread control structure for the created thread.
 * If the thread creation fails *node remains NULL
 * Do not free the memory pointed to by *node, it doesn't belong to you.
 * If your thread function returns it will be started again without creating a new thread.
 * If your thread function wants to exit it should call CtdlThreadExit(ret_code);
 * This new interface duplicates much of the eCrash stuff. We should go for closer integration since that would
 * remove the need for the calls to eCrashRegisterThread and friends
 */


struct CtdlThreadNode *CtdlThreadList = NULL;

/*
 * Condition variable and Mutex for thread garbage collection
 */
/*static pthread_mutex_t thread_gc_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_gc_cond = PTHREAD_COND_INITIALIZER;
*/
static pthread_t GC_thread;
static char *CtdlThreadStates[CTDL_THREAD_LAST_STATE];
double CtdlThreadLoadAvg = 0;
double CtdlThreadWorkerAvg = 0;
/*
 * Pinched the following bits regarding signals from Kannel.org
 */
 
/*
 * Change this thread's signal mask to block user-visible signals
 * (HUP, TERM, QUIT, INT), and store the old signal mask in
 * *old_set_storage.
 * Return 0 for success, or -1 if an error occurred.
 */
 
 /* 
  * This does not work in Darwin alias MacOS X alias Mach kernel,
  * however. So we define a dummy function doing nothing.
  */
#if defined(DARWIN_OLD)
    static int pthread_sigmask();
#endif
  
static int ctdl_thread_internal_block_signals(sigset_t *old_set_storage)
{
    int ret;
    sigset_t block_signals;

    ret = sigemptyset(&block_signals);
    if (ret != 0) {
        CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Couldn't initialize signal set\n");
	    return -1;
    }
    ret = sigaddset(&block_signals, SIGHUP);
    ret |= sigaddset(&block_signals, SIGTERM);
    ret |= sigaddset(&block_signals, SIGQUIT);
    ret |= sigaddset(&block_signals, SIGINT);
    if (ret != 0) {
        CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Couldn't add signal to signal set.\n");
	    return -1;
    }
    ret = pthread_sigmask(SIG_BLOCK, &block_signals, old_set_storage);
    if (ret != 0) {
        CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Couldn't disable signals for thread creation\n");
        return -1;
    }
    return 0;
}

static void ctdl_thread_internal_restore_signals(sigset_t *old_set)
{
    int ret;

    ret = pthread_sigmask(SIG_SETMASK, old_set, NULL);
    if (ret != 0) {
        CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Couldn't restore signal set.\n");
    }
}


void ctdl_thread_internal_cleanup(void)
{
	int i;
	
	for (i=0; i<CTDL_THREAD_LAST_STATE; i++)
	{
		free (CtdlThreadStates[i]);
	}
}

void ctdl_thread_internal_init(void)
{
	struct CtdlThreadNode *this_thread;
	int ret = 0;
	
	GC_thread = pthread_self();
	CtdlThreadStates[CTDL_THREAD_INVALID] = strdup ("Invalid Thread");
	CtdlThreadStates[CTDL_THREAD_VALID] = strdup("Valid Thread");
	CtdlThreadStates[CTDL_THREAD_CREATE] = strdup("Thread being Created");
	CtdlThreadStates[CTDL_THREAD_CANCELLED] = strdup("Thread Cancelled");
	CtdlThreadStates[CTDL_THREAD_EXITED] = strdup("Thread Exited");
	CtdlThreadStates[CTDL_THREAD_STOPPING] = strdup("Thread Stopping");
	CtdlThreadStates[CTDL_THREAD_STOP_REQ] = strdup("Thread Stop Requested");
	CtdlThreadStates[CTDL_THREAD_SLEEPING] = strdup("Thread Sleeping");
	CtdlThreadStates[CTDL_THREAD_RUNNING] = strdup("Thread Running");
	CtdlThreadStates[CTDL_THREAD_BLOCKED] = strdup("Thread Blocked");
	
	/* Get ourself a thread entry */
	this_thread = malloc(sizeof(struct CtdlThreadNode));
	if (this_thread == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't allocate CtdlThreadNode, exiting\n");
		return;
	}
	// Ensuring this is zero'd means we make sure the thread doesn't start doing its thing until we are ready.
	memset (this_thread, 0, sizeof(struct CtdlThreadNode));
	
	/* We are garbage collector so create us as running */
	this_thread->state = CTDL_THREAD_RUNNING;
	
	if ((ret = pthread_attr_init(&this_thread->attr))) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, pthread_attr_init: %s\n", strerror(ret));
		free(this_thread);
		return;
	}

	this_thread->name = strdup("Garbage Collection Thread");
	
	pthread_mutex_init (&(this_thread->ThreadMutex), NULL);
	pthread_cond_init (&(this_thread->ThreadCond), NULL);
	
	this_thread->tid = GC_thread;
	
	num_threads++;	// Increase the count of threads in the system.

	this_thread->next = CtdlThreadList;
	CtdlThreadList = this_thread;
	if (this_thread->next)
		this_thread->next->prev = this_thread;
	/* Set up start times */
	gettimeofday(&this_thread->start_time, NULL);		/* Time this thread started */
	memcpy(&this_thread->last_state_change, &this_thread->start_time, sizeof (struct timeval));	/* Changed state so mark it. */
}


/*
 * A function to update a threads load averages
 */
 void ctdl_thread_internal_update_avgs(struct CtdlThreadNode *this_thread)
 {
	struct timeval now, result;
	double last_duration;

	pthread_mutex_lock(&this_thread->ThreadMutex); /* To prevent race condition of a sleeping thread */
	gettimeofday(&now, NULL);
	timersub(&now, &(this_thread->last_state_change), &result);
	// result now has a timeval for the time we spent in the last state since we last updated
	last_duration = (double)result.tv_sec + ((double)result.tv_usec / (double) 1000000);
	if (this_thread->state == CTDL_THREAD_SLEEPING)
		this_thread->avg_sleeping += last_duration;
	if (this_thread->state == CTDL_THREAD_RUNNING)
		this_thread->avg_running += last_duration;
	if (this_thread->state == CTDL_THREAD_BLOCKED)
		this_thread->avg_blocked += last_duration;
	memcpy (&this_thread->last_state_change, &now, sizeof (struct timeval));
	pthread_mutex_unlock(&this_thread->ThreadMutex);
}

/*
 * A function to chenge the state of a thread
 */
void ctdl_thread_internal_change_state (struct CtdlThreadNode *this_thread, enum CtdlThreadState new_state)
{
	/*
	 * Wether we change state or not we need update the load values
	 */
	ctdl_thread_internal_update_avgs(this_thread);
	pthread_mutex_lock(&this_thread->ThreadMutex); /* To prevent race condition of a sleeping thread */
	if ((new_state == CTDL_THREAD_STOP_REQ) && (this_thread->state > CTDL_THREAD_STOP_REQ))
		this_thread->state = new_state;
	if (((new_state == CTDL_THREAD_SLEEPING) || (new_state == CTDL_THREAD_BLOCKED)) && (this_thread->state == CTDL_THREAD_RUNNING))
		this_thread->state = new_state;
	if ((new_state == CTDL_THREAD_RUNNING) && ((this_thread->state == CTDL_THREAD_SLEEPING) || (this_thread->state == CTDL_THREAD_BLOCKED)))
		this_thread->state = new_state;
	pthread_mutex_unlock(&this_thread->ThreadMutex);
}


/*
 * A function to tell all threads to exit
 */
void CtdlThreadStopAll(void)
{
	struct CtdlThreadNode *this_thread;
	
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		if (this_thread->thread_func) // Don't tell garbage collector to stop
		{
			ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_STOP_REQ);
			pthread_cond_signal(&this_thread->ThreadCond);
			CtdlLogPrintf(CTDL_DEBUG, "Thread system stopping thread \"%s\" (%ld).\n", this_thread->name, this_thread->tid);
		}
		this_thread = this_thread->next;
	}
	end_critical_section(S_THREAD_LIST);
}


/*
 * A function to signal that we need to do garbage collection on the thread list
 */
void CtdlThreadGC(void)
{
	struct CtdlThreadNode *this_thread;
	
	CtdlLogPrintf(CTDL_DEBUG, "Thread system signalling garbage collection.\n");
	
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		if (!this_thread->thread_func)
			pthread_cond_signal(&this_thread->ThreadCond);
			
		this_thread = this_thread->next;
	}
	end_critical_section(S_THREAD_LIST);
}


/*
 * A function to return the number of threads running in the system
 */
int CtdlThreadGetCount(void)
{
	return num_threads;
}

/*
 * A function to find the thread structure for this thread
 */
struct CtdlThreadNode *CtdlThreadSelf(void)
{
	pthread_t self_tid;
	struct CtdlThreadNode *this_thread;
	
	self_tid = pthread_self();
	
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		if (pthread_equal(self_tid, this_thread->tid))
		{
			end_critical_section(S_THREAD_LIST);
			return this_thread;
		}
		this_thread = this_thread->next;
	}
	end_critical_section(S_THREAD_LIST);
	return NULL;
}




/*
 * A function to rename a thread
 * Returns a char * and the caller owns the memory and should free it
 */
char *CtdlThreadName(struct CtdlThreadNode *thread, char *name)
{
	struct CtdlThreadNode *this_thread;
	char *old_name;
	
	if (!thread)
		this_thread = CtdlThreadSelf();
	else
		this_thread = thread;
	if (!this_thread)
	{
		CtdlLogPrintf(CTDL_WARNING, "Thread system WARNING. Attempt to CtdlThreadRename() a non thread.\n");
		return NULL;
	}
	begin_critical_section(S_THREAD_LIST);
	if (name)
	{
		old_name = this_thread->name;
		this_thread->name = strdup (name);
		free(old_name);
	}
	old_name = strdup(this_thread->name);
	end_critical_section (S_THREAD_LIST);
	return (old_name);
}	


/*
 * A function to force a thread to exit
 */
void CtdlThreadCancel(struct CtdlThreadNode *thread)
{
	struct CtdlThreadNode *this_thread;
	
	if (!thread)
		this_thread = CtdlThreadSelf();
	else
		this_thread = thread;
	if (!this_thread)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Attempt to CtdlThreadCancel() a non thread.\n");
		CtdlThreadStopAll();
		return;
	}
	
	if (!this_thread->thread_func)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC. Attempt to CtdlThreadCancel() the garbage collector.\n");
		CtdlThreadStopAll();
		return;
	}
	
	begin_critical_section(S_THREAD_LIST);
	ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_CANCELLED);
	pthread_cancel(this_thread->tid);
	end_critical_section (S_THREAD_LIST);
}



/*
 * A function for a thread to check if it has been asked to stop
 */
int CtdlThreadCheckStop(void)
{
	struct CtdlThreadNode *this_thread;
	
	this_thread = CtdlThreadSelf();
	if (!this_thread)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, CtdlThreadCheckStop() called by a non thread.\n");
		CtdlThreadStopAll();
		return -1;
	}
	if(this_thread->state == CTDL_THREAD_STOP_REQ)
	{
		this_thread->state = CTDL_THREAD_STOPPING;
		return -1;
	}
	else if(this_thread->state < CTDL_THREAD_STOP_REQ)
		return -1;
		
	return 0;
}


/*
 * A function to ask a thread to exit
 * The thread must call CtdlThreadCheckStop() periodically to determine if it should exit
 */
void CtdlThreadStop(struct CtdlThreadNode *thread)
{
	struct CtdlThreadNode *this_thread;
	
	if (!thread)
		this_thread = CtdlThreadSelf();
	else
		this_thread = thread;
	if (!this_thread)
		return;
	if (!(this_thread->thread_func))
		return; 	// Don't stop garbage collector
		
	begin_critical_section (S_THREAD_LIST);
	ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_STOP_REQ);
	pthread_cond_signal(&this_thread->ThreadCond);
	end_critical_section(S_THREAD_LIST);
}

/*
 * So we now have a sleep command that works with threads but it is in seconds
 */
void CtdlThreadSleep(int secs)
{
	struct timespec wake_time;
	struct timeval time_now;
	struct CtdlThreadNode *self;
	
	
	self = CtdlThreadSelf();
	if (!self)
	{
		CtdlLogPrintf(CTDL_WARNING, "CtdlThreadSleep() called by something that is not a thread. Should we die?\n");
		return;
	}
	
	begin_critical_section(S_THREAD_LIST);
	ctdl_thread_internal_change_state (self, CTDL_THREAD_SLEEPING);
	pthread_mutex_lock(&self->ThreadMutex); /* Prevent something asking us to awaken before we've gone to sleep */
	end_critical_section(S_THREAD_LIST);
	
	memset (&wake_time, 0, sizeof(struct timespec));
	gettimeofday(&time_now, NULL);
	wake_time.tv_sec = time_now.tv_sec + secs;
	wake_time.tv_nsec = time_now.tv_usec * 10;
	pthread_cond_timedwait(&self->ThreadCond, &self->ThreadMutex, &wake_time);
	begin_critical_section(S_THREAD_LIST);
	pthread_mutex_unlock(&self->ThreadMutex);
	ctdl_thread_internal_change_state (self, CTDL_THREAD_RUNNING);
	end_critical_section(S_THREAD_LIST);
}


/*
 * Routine to clean up our thread function on exit
 */
static void ctdl_internal_thread_cleanup(void *arg)
{
	struct CtdlThreadNode *this_thread;
	this_thread = CtdlThreadSelf();
	/*
	 * In here we were called by the current thread because it is exiting
	 * NB. WE ARE THE CURRENT THREAD
	 */
	CtdlLogPrintf(CTDL_NOTICE, "Thread \"%s\" (%ld) exited.\n", this_thread->name, this_thread->tid);
	begin_critical_section(S_THREAD_LIST);
	#ifdef HAVE_BACKTRACE
	eCrash_UnregisterThread();
	#endif
	this_thread->state = CTDL_THREAD_EXITED;	// needs to be last thing else house keeping will unlink us too early
	end_critical_section(S_THREAD_LIST);
//	CtdlThreadGC();
}

/*
 * A quick function to show the load averages
 */
void ctdl_thread_internal_calc_loadavg(void)
{
	struct CtdlThreadNode *that_thread;
	double load_avg, worker_avg;
	int workers = 0;

	begin_critical_section(S_THREAD_LIST);
	that_thread = CtdlThreadList;
	load_avg = 0;
	worker_avg = 0;
	while(that_thread)
	{
		/* Update load averages */
		ctdl_thread_internal_update_avgs(that_thread);
		pthread_mutex_lock(&that_thread->ThreadMutex);
		that_thread->load_avg = that_thread->avg_sleeping + that_thread->avg_running + that_thread->avg_blocked;
		that_thread->load_avg = that_thread->avg_running / that_thread->load_avg * 100;
		that_thread->avg_sleeping /= 2;
		that_thread->avg_running /= 2;
		that_thread->avg_blocked /= 2;
		load_avg += that_thread->load_avg;
		if (that_thread->flags & CTDLTHREAD_WORKER)
		{
			worker_avg += that_thread->load_avg;
			workers++;
		}
		CtdlLogPrintf(CTDL_DEBUG, "CtdlThread, \"%s\" (%ld) \"%s\" %f %f %f %f.\n",
			that_thread->name,
			that_thread->tid,
			CtdlThreadStates[that_thread->state],
			that_thread->avg_sleeping,
			that_thread->avg_running,
			that_thread->avg_blocked,
			that_thread->load_avg);

		pthread_mutex_unlock(&that_thread->ThreadMutex);
		that_thread = that_thread->next;
	}
	CtdlThreadLoadAvg = load_avg/num_threads;
	CtdlThreadWorkerAvg = worker_avg/workers;
	CtdlLogPrintf(CTDL_INFO, "System load average %f, workers averag %f\n", CtdlThreadLoadAvg, CtdlThreadWorkerAvg);
	end_critical_section(S_THREAD_LIST);
}


/*
 * Garbage collection routine.
 * Gets called by main() in a loop to clean up the thread list periodically.
 */
void ctdl_internal_thread_gc (void)
{
	struct CtdlThreadNode *this_thread, *that_thread;
	int workers = 0;
	
	/* Handle exiting of garbage collector thread */
	if(num_threads == 1)
		CtdlThreadList->state = CTDL_THREAD_EXITED;
	
	CtdlLogPrintf(CTDL_DEBUG, "Thread system running garbage collection.\n");
	/*
	 * Woke up to do garbage collection
	 */
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		that_thread = this_thread;
		this_thread = this_thread->next;
		
		/* Do we need to clean up this thread? */
		if (that_thread->state != CTDL_THREAD_EXITED)
		{
			if(that_thread->flags & CTDLTHREAD_WORKER)
				workers++;	/* Sanity check on number of worker threads */
			continue;
		}
		
		if (pthread_equal(that_thread->tid, pthread_self()) && that_thread->thread_func)
		{	/* Sanity check */
			end_critical_section(S_THREAD_LIST);
			CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, a thread is trying to clean up after itself.\n");
			CtdlThreadStopAll();
			return;
		}
		
		if (num_threads <= 0)
		{	/* Sanity check */
			end_critical_section (S_THREAD_LIST);
			CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, num_threads <= 0 and trying to do Garbage Collection.\n");
			CtdlThreadStopAll();
			return;
		}

		/* If we are unlinking the list head then the next becomes the list head */
		if (that_thread == CtdlThreadList)
			CtdlThreadList = that_thread->next;
		if(that_thread->prev)
			that_thread->prev->next = that_thread->next;
		if(that_thread->next)
			that_thread->next->prev = that_thread->next;
		num_threads--;
		if(that_thread->flags & CTDLTHREAD_WORKER)
			num_workers--;	/* This is a wroker thread so reduce the count. */
		
		/*
		 * Join on the thread to do clean up and prevent memory leaks
		 * Also makes sure the thread has cleaned up after itself before we remove it from the list
		 * If that thread has no function it must be the garbage collector
		 */
		if (that_thread->thread_func)
			pthread_join (that_thread->tid, NULL);
		
		/*
		 * Now we own that thread entry
		 */
		CtdlLogPrintf(CTDL_INFO, "Garbage Collection for thread \"%s\" (%ld).\n", that_thread->name, that_thread->tid);
		if(that_thread->name)
			free(that_thread->name);
		pthread_mutex_destroy(&that_thread->ThreadMutex);
		pthread_cond_destroy(&that_thread->ThreadCond);
		pthread_attr_destroy(&that_thread->attr);
		free(that_thread);
	}
	
	/* Sanity check number of worker threads */
	if (workers != num_workers)
	{
		end_critical_section(S_THREAD_LIST);
		CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, discrepancy in number of worker threads. Counted %d, should be %d.\n", workers, num_workers);
		return;
	}
	end_critical_section(S_THREAD_LIST);
}



 
/*
 * Runtime function for a Citadel Thread.
 * This initialises the threads environment and then calls the user supplied thread function
 * Note that this is the REAL thread function and wraps the users thread function.
 */ 
static void *ctdl_internal_thread_func (void *arg)
{
	struct CtdlThreadNode *this_thread;
	void *ret = NULL;

	/* lock and unlock the thread list.
	 * This causes this thread to wait until all its creation stuff has finished before it
	 * can continue its execution.
	 */
	begin_critical_section(S_THREAD_LIST);
	// Get our thread data structure
	this_thread = (struct CtdlThreadNode *) arg;
	this_thread->state = CTDL_THREAD_RUNNING;
	this_thread->pid = getpid();
	gettimeofday(&this_thread->start_time, NULL);		/* Time this thread started */
	memcpy(&this_thread->last_state_change, &this_thread->start_time, sizeof (struct timeval));	/* Changed state so mark it. */
	end_critical_section(S_THREAD_LIST);
		
	// Tell the world we are here
	CtdlLogPrintf(CTDL_NOTICE, "Created a new thread \"%s\" (%ld). \n", this_thread->name, this_thread->tid);

	// Register the cleanup function to take care of when we exit.
	pthread_cleanup_push(ctdl_internal_thread_cleanup, NULL);
	
	
	/*
	 * run the thread to do the work
	 */
	ret = (this_thread->thread_func)(this_thread->user_args);
	
	/*
	 * Our thread is exiting either because it wanted to end or because the server is stopping
	 * We need to clean up
	 */
	pthread_cleanup_pop(1);	// Execute our cleanup routine and remove it
	
	return(ret);
}


 
/*
 * Internal function to create a thread.
 * Must be called from within a S_THREAD_LIST critical section
 */ 
struct CtdlThreadNode *ctdl_internal_create_thread(char *name, long flags, void *(*thread_func) (void *arg), void *args)
{
	int ret = 0;
	struct CtdlThreadNode *this_thread;
	int sigtrick = 0;
	sigset_t old_signal_set;

	if (num_threads >= 32767)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system. Thread list full.\n");
		return NULL;
	}
		
	this_thread = malloc(sizeof(struct CtdlThreadNode));
	if (this_thread == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't allocate CtdlThreadNode, exiting\n");
		return NULL;
	}
	// Ensuring this is zero'd means we make sure the thread doesn't start doing its thing until we are ready.
	memset (this_thread, 0, sizeof(struct CtdlThreadNode));
	
	this_thread->state = CTDL_THREAD_CREATE;
	
	if ((ret = pthread_attr_init(&this_thread->attr))) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, pthread_attr_init: %s\n", strerror(ret));
		free(this_thread);
		return NULL;
	}

	/* Our per-thread stacks need to be bigger than the default size,
	 * otherwise the MIME parser crashes on FreeBSD, and the IMAP service
	 * crashes on 64-bit Linux.
	 */
	if (flags & CTDLTHREAD_BIGSTACK)
	{
		CtdlLogPrintf(CTDL_INFO, "Thread system. Creating BIG STACK thread.\n");
		if ((ret = pthread_attr_setstacksize(&this_thread->attr, THREADSTACKSIZE))) {
			CtdlLogPrintf(CTDL_EMERG, "Thread system, pthread_attr_setstacksize: %s\n",
				strerror(ret));
			pthread_attr_destroy(&this_thread->attr);
			free(this_thread);
			return NULL;
		}
	}

	/*
	 * If we got here we are going to create the thread so we must initilise the structure
	 * first because most implimentations of threading can't create it in a stopped state
	 * and it might want to do things with its structure that aren't initialised otherwise.
	 */
	if(name)
	{
		this_thread->name = strdup(name);
	}
	else
	{
		this_thread->name = strdup("Un-named Thread");
	}
	
	this_thread->flags = flags;
	this_thread->thread_func = thread_func;
	this_thread->user_args = args;
	pthread_mutex_init (&(this_thread->ThreadMutex), NULL);
	pthread_cond_init (&(this_thread->ThreadCond), NULL);
	
	/*
	 * We want to make sure that only the main thread handles signals,
	 * so that each signal is handled exactly once.  To do this, we
	 * make sure that each new thread has all the signals that we
	 * handle blocked.  To avoid race conditions, we block them in 
	 * the spawning thread first, then create the new thread (which
	 * inherits the settings), and then restore the old settings in
	 * the spawning thread.  This means that there is a brief period
	 * when no signals will be processed, but during that time they
	 * should be queued by the operating system.
	 */
	if (pthread_equal(GC_thread, pthread_self())) 
	    sigtrick = ctdl_thread_internal_block_signals(&old_signal_set) == 0;

	/*
	 * We pass this_thread into the thread as its args so that it can find out information
	 * about itself and it has a bit of storage space for itself, not to mention that the REAL
	 * thread function needs to finish off the setup of the structure
	 */
	if ((ret = pthread_create(&this_thread->tid, &this_thread->attr, ctdl_internal_thread_func, this_thread) != 0))
	{

		CtdlLogPrintf(CTDL_ALERT, "Thread system, Can't create thread: %s\n",
			strerror(ret));
		if (this_thread->name)
			free (this_thread->name);
		pthread_mutex_destroy(&(this_thread->ThreadMutex));
		pthread_cond_destroy(&(this_thread->ThreadCond));
		pthread_attr_destroy(&this_thread->attr);
		free(this_thread);
		if (sigtrick)
			ctdl_thread_internal_restore_signals(&old_signal_set);
		return NULL;
	}
	
	if (sigtrick)
		ctdl_thread_internal_restore_signals(&old_signal_set);
	
	num_threads++;	// Increase the count of threads in the system.
	if(this_thread->flags & CTDLTHREAD_WORKER)
		num_workers++;

	this_thread->next = CtdlThreadList;
	CtdlThreadList = this_thread;
	if (this_thread->next)
		this_thread->next->prev = this_thread;
	// Register for tracing
	#ifdef HAVE_BACKTRACE
	eCrash_RegisterThread(this_thread->name, 0);
	#endif
	return this_thread;
}

/*
 * Wrapper function to create a thread
 * ensures the critical section and other protections are in place.
 * char *name = name to give to thread, if NULL, use generic name
 * int flags = flags to determine type of thread and standard facilities
 */
struct CtdlThreadNode *CtdlThreadCreate(char *name, long flags, void *(*thread_func) (void *arg), void *args)
{
	struct CtdlThreadNode *ret = NULL;
	
	begin_critical_section(S_THREAD_LIST);
	ret = ctdl_internal_create_thread(name, flags, thread_func, args);
	end_critical_section(S_THREAD_LIST);
	return ret;
}



/*
 * A warapper function for select so we can show a thread as blocked
 */
int CtdlThreadSelect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
	struct CtdlThreadNode *self;
	int ret;
	
	self = CtdlThreadSelf();
	ctdl_thread_internal_change_state(self, CTDL_THREAD_BLOCKED);
	ret = select(n, readfds, writefds, exceptfds, timeout);
	ctdl_thread_internal_change_state(self, CTDL_THREAD_RUNNING);
	return ret;
}

/*
 * Purge all sessions which have the 'kill_me' flag set.
 * This function has code to prevent it from running more than once every
 * few seconds, because running it after every single unbind would waste a lot
 * of CPU time and keep the context list locked too much.  To force it to run
 * anyway, set "force" to nonzero.
 *
 *
 * After that's done, we raise the size of the worker thread pool
 * if such an action is appropriate.
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

	begin_critical_section(S_SESSION_TABLE);
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

	/* Raise the size of the worker thread pool if necessary. */
	begin_critical_section(S_THREAD_LIST);
	if ( (num_sessions > num_workers)
	   && (num_workers < config.c_max_workers) ) {
		ctdl_internal_create_thread("Worker Thread", CTDLTHREAD_BIGSTACK + CTDLTHREAD_WORKER, worker_thread, NULL);
	}
	end_critical_section(S_THREAD_LIST);
	// FIXME: reduce the number of worker threads too
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
	pthread_setspecific(MyConKey, (void *)which_con );
}



/* 
 * This loop just keeps going and going and going...
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

	cdb_allocate_tsd();

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
//			retval = select(highest + 1, &readfds, NULL, NULL, &tv);
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
				CtdlLogPrintf(CTDL_DEBUG, "Un handled select failure.\n");
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
		check_sched_shutdown();
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
