/*
 * $Id$
 *
 * Citadel/UX "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * Here's where we (hopefully) have most parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * 
 * If we ever port to a different platform and either have multiple
 * variants of this file or simply load it up with #ifdefs.
 *
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

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
#include <netinet/in.h>
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
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "tools.h"
#include "serv_crypto.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


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
int rescan[2];					/* The Rescan Pipe */
time_t last_purge = 0;				/* Last dead session purge */
static int num_threads = 0;			/* Current number of threads */
int num_sessions = 0;				/* Current number of sessions */

fd_set masterfds;				/* Master sockets etc. */
int masterhighest;

pthread_t initial_thread;		/* tid for main() thread */

int syslog_facility = (-1);


/*
 * lprintf()  ...   Write logging information
 * 
 * Note: the variable "buf" below needs to be large enough to handle any
 * log data sent through this function.  BE CAREFUL!
 */
void lprintf(enum LogLevel loglevel, const char *format, ...) {   
        va_list arg_ptr;
	char buf[SIZ];
 
        va_start(arg_ptr, format);   
        vsnprintf(buf, sizeof(buf), format, arg_ptr);   
        va_end(arg_ptr);   

	if (syslog_facility >= 0) {
		if (loglevel <= verbosity) {
			/* Hackery -IO */
			if (CC && CC->cs_pid) {
				memmove(buf + 6, buf, sizeof(buf) - 6);
				snprintf(buf, 6, "[%3d]", CC->cs_pid);
				buf[5] = ' ';
			}
			syslog(loglevel, buf);
		}
	}
	else if (loglevel <= verbosity) { 
		struct timeval tv;
		struct tm *tim;
		time_t unixtime;

		gettimeofday(&tv, NULL);
		/* Promote to time_t; types differ on some OSes (like darwin) */
		unixtime = tv.tv_sec;
		tim = localtime(&unixtime);
		/*
		 * Log provides millisecond accuracy.  If you need
		 * microsecond accuracy and your OS supports it, change
		 * %03ld to %06ld and remove " / 1000" after tv.tv_usec.
		 */
		if (CC && CC->cs_pid) {
#if 0
			/* Millisecond display */
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%03ld [%3d] %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec / 1000,
				CC->cs_pid, buf);
#endif
			/* Microsecond display */
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%06ld [%3d] %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec,
				CC->cs_pid, buf);
		} else {
#if 0
			/* Millisecond display */
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%03ld %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec / 1000, buf);
#endif
			/* Microsecond display */
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%06ld %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec, buf);
		}
		fflush(stderr);
	}

	PerformLogHooks(loglevel, buf);
}   



/*
 * We used to use master_cleanup() as a signal handler to shut down the server.
 * however, master_cleanup() and the functions it calls do some things that
 * aren't such a good idea to do from a signal handler: acquiring mutexes,
 * playing with signal masks on BSDI systems, etc. so instead we install the
 * following signal handler to set a global variable to inform the main loop
 * that it's time to call master_cleanup() and exit.
 */

volatile int time_to_die = 0;

static RETSIGTYPE signal_cleanup(int signum) {
	time_to_die = 1;
}


/*
 * Some initialization stuff...
 */
void init_sysdep(void) {
	int a;

#ifdef HAVE_OPENSSL
	init_ssl();
#endif

	/* Set up a bunch of semaphores to be used for critical sections */
	for (a=0; a<MAX_SEMAPHORES; ++a) {
		pthread_mutex_init(&Critters[a], NULL);
	}

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * CitContext structure (in the ContextList linked list) of the
	 * session to which the calling thread is currently bound.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		lprintf(CTDL_CRIT, "Can't create TSD key!!  %s\n", strerror(errno));
	}

	/*
	 * The action for unexpected signals and exceptions should be to
	 * call signal_cleanup() to gracefully shut down the server.
	 */
	signal(SIGINT, signal_cleanup);
	signal(SIGQUIT, signal_cleanup);
	signal(SIGHUP, signal_cleanup);
	signal(SIGTERM, signal_cleanup);

	/*
	 * Do not shut down the server on broken pipe signals, otherwise the
	 * whole Citadel service would come down whenever a single client
	 * socket breaks.
	 */
	signal(SIGPIPE, SIG_IGN);
}


/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	/* lprintf(CTDL_DEBUG, "begin_critical_section(%d)\n", which_one); */


	/* ensure nobody ever tries to do a critical section within a
	  	transaction; this could lead to deadlock. */
#ifdef DEBUG_MEMORY_LEAKS
	if (which_one != S_DEBUGMEMLEAKS) {
#endif
		cdb_check_handles();
#ifdef DEBUG_MEMORY_LEAKS
	}
#endif
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	/* lprintf(CTDL_DEBUG, "end_critical_section(%d)\n", which_one); */
	pthread_mutex_unlock(&Critters[which_one]);
}



/*
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 *
 */
int ig_tcp_server(int port_number, int queue_len)
{
	struct sockaddr_in sin;
	int s, i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((u_short)port_number);

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't create a socket: %s\n",
			strerror(errno));
		return(-1);
	}

	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't bind: %s\n",
			strerror(errno));
		close(s);
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't listen: %s\n", strerror(errno));
		close(s);
		return(-1);
	}

	return(s);
}



/*
 * Create a Unix domain socket and listen on it
 */
int ig_uds_server(char *sockpath, int queue_len)
{
	struct sockaddr_un addr;
	int s;
	int i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	i = unlink(sockpath);
	if (i != 0) if (errno != ENOENT) {
		lprintf(CTDL_EMERG, "citserver: can't unlink %s: %s\n",
			sockpath, strerror(errno));
		return(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't create a socket: %s\n",
			strerror(errno));
		return(-1);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't bind: %s\n",
			strerror(errno));
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		lprintf(CTDL_EMERG, "citserver: Can't listen: %s\n", strerror(errno));
		return(-1);
	}

	chmod(sockpath, 0777);
	return(s);
}



/*
 * Return a pointer to the CitContext structure bound to the thread which
 * called this function.  If there's no such binding (for example, if it's
 * called by the housekeeper thread) then a generic 'master' CC is returned.
 *
 * It's inlined because it's used *VERY* frequently.
 */
INLINE struct CitContext *MyContext(void) {
	return ((pthread_getspecific(MyConKey) == NULL)
		? &masterCC
		: (struct CitContext *) pthread_getspecific(MyConKey)
	);
}


/*
 * Initialize a new context and place it in the list.  The session number
 * used to be the PID (which is why it's called cs_pid), but that was when we
 * had one process per session.  Now we just assign them sequentially, starting
 * at 1 (don't change it to 0 because masterCC uses 0) and re-using them when
 * sessions terminate.
 */
struct CitContext *CreateNewContext(void) {
	struct CitContext *me, *ptr;

	me = (struct CitContext *) malloc(sizeof(struct CitContext));
	if (me == NULL) {
		lprintf(CTDL_ALERT, "citserver: can't allocate memory!!\n");
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

	if (ContextList == NULL) {
		ContextList = me;
		me->cs_pid = 1;
		me->next = NULL;
	}

	else if (ContextList->cs_pid > 1) {
		me->next = ContextList;
		ContextList = me;
		me->cs_pid = 1;
	}

	else {
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->next == NULL) {
				ptr->next = me;
				me->cs_pid = ptr->cs_pid + 1;
				me->next = NULL;
				goto DONE;
			}
			else if (ptr->next->cs_pid > (ptr->cs_pid+1)) {
				me->next = ptr->next;
				ptr->next = me;
				me->cs_pid = ptr->cs_pid + 1;
				goto DONE;
			}
		}
	}

DONE:	++num_sessions;
	end_critical_section(S_SESSION_TABLE);
	return(me);
}


/*
 * buffer_output() ... tell client_write to buffer all output until
 *                     instructed to dump it all out later
 */
void buffer_output(void) {
	if (CC->buffering == 0) {
		CC->buffering = 1;
		CC->buffer_len = 0;
		CC->output_buffer = malloc(SIZ);
	}
}

/*
 * unbuffer_output()  ...  dump out all that output we've been buffering.
 */
void unbuffer_output(void) {
	if (CC->buffering == 1) {
		CC->buffering = 0;
		client_write(CC->output_buffer, CC->buffer_len);
		free(CC->output_buffer);
		CC->output_buffer = NULL;
		CC->buffer_len = 0;
	}
}



/*
 * client_write()   ...    Send binary data to the client.
 */
void client_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	int sock;
	int old_buffer_len = 0;

	if (CC->redirect_fp != NULL) {
		fwrite(buf, nbytes, 1, CC->redirect_fp);
		return;
	}

	if (CC->redirect_sock > 0) {
		sock = CC->redirect_sock;	/* and continue below... */
	}
	else {
		sock = CC->client_socket;
	}

	/* If we're buffering for later, do that now. */
	if (CC->buffering) {
		old_buffer_len = CC->buffer_len;
		CC->buffer_len += nbytes;
		CC->output_buffer = realloc(CC->output_buffer, CC->buffer_len);
		memcpy(&CC->output_buffer[old_buffer_len], buf, nbytes);
		return;
	}

	/* Ok, at this point we're not buffering.  Go ahead and write. */

#ifdef HAVE_OPENSSL
	if (CC->redirect_ssl) {
		client_write_ssl(buf, nbytes);
		return;
	}
#endif

	while (bytes_written < nbytes) {
		retval = write(sock, &buf[bytes_written],
			nbytes - bytes_written);
		if (retval < 1) {
			lprintf(CTDL_ERR, "client_write() failed: %s\n",
				strerror(errno));
			if (sock == CC->client_socket) CC->kill_me = 1;
			return;
		}
		bytes_written = bytes_written + retval;
	}
}


/*
 * cprintf()  ...   Send formatted printable data to the client.   It is
 *                  implemented in terms of client_write() but remains in
 *                  sysdep.c in case we port to somewhere without va_args...
 */
void cprintf(const char *format, ...) {   
        va_list arg_ptr;   
        char buf[SIZ];   
   
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
	struct timeval tv;
	int retval;

#ifdef HAVE_OPENSSL
	if (CC->redirect_ssl) {
		return (client_read_ssl(buf, bytes, timeout));
	}
#endif
	len = 0;
	while(len<bytes) {
		FD_ZERO(&rfds);
		FD_SET(CC->client_socket, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select( (CC->client_socket)+1, 
					&rfds, NULL, NULL, &tv);

		if (FD_ISSET(CC->client_socket, &rfds) == 0) {
			return(0);
		}

		rlen = read(CC->client_socket, &buf[len], bytes-len);
		if (rlen<1) {
			lprintf(CTDL_ERR, "client_read() failed: %s\n",
				strerror(errno));
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
 * client_gets()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int client_gets(char *buf)
{
	int i, retval;

	/* Read one character at a time.
	 */
	for (i = 0;;i++) {
		retval = client_read(&buf[i], 1);
		if (retval != 1 || buf[i] == '\n' || i == (SIZ-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (SIZ-1))
		while (buf[i] != '\n' && retval == 1)
			retval = client_read(&buf[i], 1);

	/* Strip the trailing newline and any trailing nonprintables (cr's)
	 */
	buf[i] = 0;
	while ((strlen(buf)>0)&&(!isprint(buf[strlen(buf)-1])))
		buf[strlen(buf)-1] = 0;
	if (retval < 0) strcpy(buf, "000");
	return(retval);
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
			lprintf(CTDL_INFO, "Closing listener on port %d\n",
				serviceptr->tcp_port);

		if (serviceptr->sockpath != NULL)
			lprintf(CTDL_INFO, "Closing listener on '%s'\n",
				serviceptr->sockpath);

		close(serviceptr->msock);

		/* If it's a Unix domain socket, remove the file. */
		if (serviceptr->sockpath != NULL) {
			unlink(serviceptr->sockpath);
		}
	}
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




/*
 * Start running as a daemon.  Only close stdio if do_close_stdio is set.
 */
void start_daemon(int do_close_stdio) {
	if (do_close_stdio) {
		/* close(0); */
		close(1);
		close(2);
	}
	signal(SIGHUP,SIG_IGN);
	signal(SIGINT,SIG_IGN);
	signal(SIGQUIT,SIG_IGN);
	if (fork()!=0) exit(0);
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

struct worker_node *worker_list = NULL;


/*
 * create a worker thread. this function must always be called from within
 * an S_WORKER_LIST critical section!
 */
void create_worker(void) {
	int ret;
	struct worker_node *n;
	pthread_attr_t attr;

	n = malloc(sizeof(struct worker_node));
	if (n == NULL) {
		lprintf(CTDL_EMERG, "can't allocate worker_node, exiting\n");
		time_to_die = -1;
		return;
	}

	if ((ret = pthread_attr_init(&attr))) {
		lprintf(CTDL_EMERG, "pthread_attr_init: %s\n", strerror(ret));
		time_to_die = -1;
		return;
	}

	/* we seem to need something bigger than FreeBSD's default 64k stack */

	if ((ret = pthread_attr_setstacksize(&attr, 128 * 1024))) {
		lprintf(CTDL_EMERG, "pthread_attr_setstacksize: %s\n", strerror(ret));
		time_to_die = -1;
		return;
	}

	if ((ret = pthread_create(&n->tid, &attr, worker_thread, NULL) != 0))
	{

		lprintf(CTDL_ALERT, "Can't create worker thread: %s\n",
			strerror(ret));
	}

	n->next = worker_list;
	worker_list = n;
}



/*
 * Purge all sessions which have the 'kill_me' flag set.
 * This function has code to prevent it from running more than once every
 * few seconds, because running it after every single unbind would waste a lot
 * of CPU time and keep the context list locked too much.
 *
 * After that's done, we raise or lower the size of the worker thread pool
 * if such an action is appropriate.
 */
void dead_session_purge(void) {
	struct CitContext *ptr, *rem;
	struct worker_node **node, *tmp;
	pthread_t self;

	if ( (time(NULL) - last_purge) < 5 ) return;	/* Too soon, go away */
	time(&last_purge);

	do {
		rem = NULL;
		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if ( (ptr->state == CON_IDLE) && (ptr->kill_me) ) {
				rem = ptr;
			}
		}
		end_critical_section(S_SESSION_TABLE);

		/* RemoveContext() enters its own S_SESSION_TABLE critical
		 * section, so we have to do it like this.
		 */	
		if (rem != NULL) {
			lprintf(CTDL_DEBUG, "Purging session %d\n", rem->cs_pid);
			RemoveContext(rem);
		}

	} while (rem != NULL);


	/* Raise or lower the size of the worker thread pool if such
	 * an action is appropriate.
	 */

	self = pthread_self();

	if ( (num_sessions > num_threads)
	   && (num_threads < config.c_max_workers) ) {
		begin_critical_section(S_WORKER_LIST);
		create_worker();
		end_critical_section(S_WORKER_LIST);
	}
	
	/* don't let the initial thread die since it's responsible for
	   waiting for all the other threads to terminate. */
	else if ( (num_sessions < num_threads)
	   && (num_threads > config.c_min_workers)
	   && (self != initial_thread) ) {
		cdb_free_tsd();
		begin_critical_section(S_WORKER_LIST);
		--num_threads;

		/* we're exiting before server shutdown... unlink ourself from
		   the worker list and detach our thread to avoid memory leaks
		 */

		for (node = &worker_list; *node != NULL; node = &(*node)->next)
			if ((*node)->tid == self) {
				tmp = *node;
				*node = (*node)->next;
				free(tmp);
				break;
			}

		pthread_detach(self);
		end_critical_section(S_WORKER_LIST);
		pthread_exit(NULL);
	}

}





/*
 * Redirect a session's output to a file or socket.
 * This function may be called with a file handle *or* a socket (but not
 * both).  Call with neither to return output to its normal client socket.
 */
void CtdlRedirectOutput(FILE *fp, int sock) {

	if (fp != NULL) CC->redirect_fp = fp;
	else CC->redirect_fp = NULL;

	if (sock > 0) CC->redirect_sock = sock;
	else CC->redirect_sock = (-1);

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
 * Set up a fd_set containing all the master sockets to which we
 * always listen.  It's computationally less expensive to just copy
 * this to a local fd_set when starting a new select() and then add
 * the client sockets than it is to initialize a new one and then
 * figure out what to put there.
 */
void init_master_fdset(void) {
	struct ServiceFunctionHook *serviceptr;
	int m;

	lprintf(CTDL_DEBUG, "Initializing master fdset\n");

	FD_ZERO(&masterfds);
	masterhighest = 0;

	lprintf(CTDL_DEBUG, "Will listen on rescan pipe %d\n", rescan[0]);
	FD_SET(rescan[0], &masterfds);
	if (rescan[0] > masterhighest) masterhighest = rescan[0];

	for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    serviceptr = serviceptr->next ) {
		m = serviceptr->msock;
		lprintf(CTDL_DEBUG, "Will listen on master socket %d\n", m);
		FD_SET(m, &masterfds);
		if (m > masterhighest) {
			masterhighest = m;
		}
	}
	lprintf(CTDL_DEBUG, "masterhighest = %d\n", masterhighest);
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
	char junk;
	int highest;
	/* This is synchronized below; it helps implement round robin mode */
	static struct CitContext* next_session = NULL;
	struct CitContext *ptr;
	struct CitContext *bind_me = NULL;
	fd_set readfds;
	int retval;
	struct CitContext *con= NULL;	/* Temporary context pointer */
	struct ServiceFunctionHook *serviceptr;
	int ssock;			/* Descriptor for client socket */
	struct timeval tv;

	num_threads++;

	cdb_allocate_tsd();

	while (!time_to_die) {

		/* 
		 * A naive implementation would have all idle threads
		 * calling select() and then they'd all wake up at once
		 * (known in computer science as the "thundering herd"
		 * problem).  We solve this problem by putting the select()
		 * in a critical section, so only one thread has the
		 * opportunity to wake up.  If we wake up on a master
		 * socket, create a new session context; otherwise, just
		 * bind the thread to the context we want and go on our
		 * merry way.
		 */

		/* make doubly sure we're not holding any stale db handles
		 * which might cause a deadlock.
		 */
		cdb_check_handles();

		begin_critical_section(S_I_WANNA_SELECT);
SETUP_FD:	memcpy(&readfds, &masterfds, sizeof masterfds);
		highest = masterhighest;
		begin_critical_section(S_SESSION_TABLE);
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->state == CON_IDLE) {
				FD_SET(ptr->client_socket, &readfds);
				if (ptr->client_socket > highest)
					highest = ptr->client_socket;
			}
		}
		end_critical_section(S_SESSION_TABLE);

		tv.tv_sec = 1;		/* wake up every second if no input */
		tv.tv_usec = 0;

		do_select:
		if (!time_to_die)
			retval = select(highest + 1, &readfds, NULL, NULL, &tv);
		else {
			end_critical_section(S_I_WANNA_SELECT);
			break;
		}

		/* Now figure out who made this select() unblock.
		 * First, check for an error or exit condition.
		 */
		if (retval < 0) {
			if (errno != EINTR) {
				lprintf(CTDL_EMERG, "Exiting (%s)\n", strerror(errno));
				time_to_die = 1;
			} else if (!time_to_die)
				goto do_select;
		}

		/* Next, check to see if it's a new client connecting
		 * on a master socket.
		 */
		else for (serviceptr = ServiceHookTable; serviceptr != NULL;
		     serviceptr = serviceptr->next ) {

			if (FD_ISSET(serviceptr->msock, &readfds)) {
				ssock = accept(serviceptr->msock, NULL, 0);
				if (ssock < 0) {
					lprintf(CTDL_CRIT,
						"citserver: accept(): %s\n",
						strerror(errno));
				}
				else {
					lprintf(CTDL_NOTICE,
						"New client socket %d\n",
						ssock);

					/* New context will be created already
				 	* set up in the CON_EXECUTING state.
				 	*/
					con = CreateNewContext();

					/* Assign new socket number to it. */
					con->client_socket = ssock;
					con->h_command_function =
						serviceptr->h_command_function;

					/* Determine whether local socket */
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
					goto SETUP_FD;
				}
			}
		}

		/* If the rescan pipe went active, someone is telling this
		 * thread that the &readfds needs to be refreshed with more
		 * current data.
		 */
		if (time_to_die) {
			end_critical_section(S_I_WANNA_SELECT);
			break;
		}

		if (FD_ISSET(rescan[0], &readfds)) {
			read(rescan[0], &junk, 1);
			goto SETUP_FD;
		}

		/* It must be a client socket.  Find a context that has data
		 * waiting on its socket *and* is in the CON_IDLE state.
		 */
		else {
			bind_me = NULL;
			begin_critical_section(S_SESSION_TABLE);
			/*
			 * We start where we left off.  If we get to the end
			 * we'll start from the beginning again, then give up
			 * if we still don't find anything.  This ensures
			 * that all contexts get a more-or-less equal chance
			 * to run. And yes, I did add a goto to the code. -IO
			 */
find_session:		if (next_session == NULL)
				next_session = ContextList;
			for (ptr = next_session;
			    ( (ptr != NULL) && (bind_me == NULL) );
			    ptr = ptr->next) {
				if ( (FD_ISSET(ptr->client_socket, &readfds))
				   && (ptr->state == CON_IDLE) ) {
					bind_me = ptr;
				}
			}
			if (bind_me != NULL) {
				/* Found one.  Stake a claim to it before
				 * letting anyone else touch the context list.
				 */
				bind_me->state = CON_EXECUTING;
				next_session = bind_me->next;
			} else if (next_session == ContextList) {
				next_session = NULL;
			}
			if (bind_me == NULL && next_session != NULL) {
				next_session = NULL;
				goto find_session;
			}

			end_critical_section(S_SESSION_TABLE);
			end_critical_section(S_I_WANNA_SELECT);

			/* We're bound to a session, now do *one* command */
			if (bind_me != NULL) {
				become_session(bind_me);
				CC->h_command_function();
				become_session(NULL);
				bind_me->state = CON_IDLE;
				if (bind_me->kill_me == 1) {
					RemoveContext(bind_me);
				} 
				write(rescan[1], &junk, 1);
			}

		}
		dead_session_purge();
		do_housekeeping();
		check_sched_shutdown();
	}

	/* If control reaches this point, the server is shutting down */	
	--num_threads;
	return NULL;
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
	return -1;
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
		lprintf(CTDL_DEBUG, "%30s : %d\n", thisheap->file, thisheap->line);
	}
}

#endif /*  DEBUG_MEMORY_LEAKS */
