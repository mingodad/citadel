/*
 * Citadel/UX "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * $Id$
 *
 * Here's where we (hopefully) have all the parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * 
 * Eventually we'll try porting to a different platform and either have
 * multiple variants of this file or simply load it up with #ifdefs.
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <grp.h>
#ifdef __GNUC__
#include <malloc.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "dynloader.h"
#include "tools.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#ifdef DEBUG_MEMORY_LEAKS
struct TheHeap *heap = NULL;
#endif

pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
pthread_key_t MyConKey;				/* TSD key for MyContext() */

int verbosity = DEFAULT_VERBOSITY;		/* Logging level */

struct CitContext masterCC;
int rescan[2];					/* The Rescan Pipe */
time_t last_purge = 0;				/* Last dead session purge */
int num_threads = 0;				/* Current number of threads */
int num_sessions = 0;				/* Current number of sessions */

fd_set masterfds;				/* Master sockets etc. */
int masterhighest;

time_t last_timer = 0L;				/* Last timer hook processing */


/*
 * lprintf()  ...   Write logging information
 */
void lprintf(int loglevel, const char *format, ...) {   
        va_list arg_ptr;
	char buf[512];
  
        va_start(arg_ptr, format);   
        vsprintf(buf, format, arg_ptr);   
        va_end(arg_ptr);   

	if (loglevel <= verbosity) { 
		fprintf(stderr, "%s", buf);
		fflush(stderr);
		}

	PerformLogHooks(loglevel, buf);
	}   



#ifdef DEBUG_MEMORY_LEAKS
void *tracked_malloc(size_t tsize, char *tfile, int tline) {
	void *ptr;
	struct TheHeap *hptr;

	ptr = malloc(tsize);
	if (ptr == NULL) {
		lprintf(3, "DANGER!  mallok(%d) at %s:%d failed!\n",
			tsize, tfile, tline);
		return(NULL);
	}

	hptr = (struct TheHeap *) malloc(sizeof(struct TheHeap));
	strcpy(hptr->h_file, tfile);
	hptr->h_line = tline;
	hptr->next = heap;
	hptr->h_ptr = ptr;
	heap = hptr;
	return ptr;
	}

char *tracked_strdup(const char *orig, char *tfile, int tline) {
	char *s;

	s = tracked_malloc( (strlen(orig)+1), tfile, tline);
	if (s == NULL) return NULL;

	strcpy(s, orig);
	return s;
}

void tracked_free(void *ptr) {
	struct TheHeap *hptr, *freeme;

	if (heap->h_ptr == ptr) {
		hptr = heap->next;
		free(heap);
		heap = hptr;
		}
	else {
		for (hptr=heap; hptr->next!=NULL; hptr=hptr->next) {
			if (hptr->next->h_ptr == ptr) {
				freeme = hptr->next;
				hptr->next = hptr->next->next;
				free(freeme);
				}
			}
		}

	free(ptr);
	}

void *tracked_realloc(void *ptr, size_t size) {
	void *newptr;
	struct TheHeap *hptr;
	
	newptr = realloc(ptr, size);

	for (hptr=heap; hptr!=NULL; hptr=hptr->next) {
		if (hptr->h_ptr == ptr) hptr->h_ptr = newptr;
		}

	return newptr;
	}


void dump_tracked() {
	struct TheHeap *hptr;

	cprintf("%d Here's what's allocated...\n", LISTING_FOLLOWS);	
	for (hptr=heap; hptr!=NULL; hptr=hptr->next) {
		cprintf("%20s %5d\n",
			hptr->h_file, hptr->h_line);
		}
#ifdef __GNUC__
        malloc_stats();
#endif

	cprintf("000\n");
	}
#endif


/*
 * we used to use master_cleanup() as a signal handler to shut down the server.
 * however, master_cleanup() and the functions it calls do some things that
 * aren't such a good idea to do from a signal handler: acquiring mutexes,
 * playing with signal masks on BSDI systems, etc. so instead we install the
 * following signal handler to set a global variable to inform the main loop
 * that it's time to call master_cleanup() and exit.
 */

static volatile int time_to_die = 0;

static RETSIGTYPE signal_cleanup(int signum) {
	time_to_die = 1;
}


/*
 * Some initialization stuff...
 */
void init_sysdep(void) {
	int a;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (a=0; a<MAX_SEMAPHORES; ++a) {
		pthread_mutex_init(&Critters[a], NULL);
	}

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * thread's CitContext structure in the ContextList linked list.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		lprintf(1, "Can't create TSD key!!  %s\n", strerror(errno));
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
	/* lprintf(9, "begin_critical_section(%d)\n", which_one); */
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	/* lprintf(9, "end_critical_section(%d)\n", which_one); */
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

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons((u_short)port_number);

	s = socket(PF_INET, SOCK_STREAM,
		(getprotobyname("tcp")->p_proto));

	if (s < 0) {
		lprintf(1, "citserver: Can't create a socket: %s\n",
			strerror(errno));
		return(-1);
	}

	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		lprintf(1, "citserver: Can't bind: %s\n",
			strerror(errno));
		return(-1);
	}

	if (listen(s, queue_len) < 0) {
		lprintf(1, "citserver: Can't listen: %s\n", strerror(errno));
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

	unlink(sockpath);

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		lprintf(1, "citserver: Can't create a socket: %s\n",
			strerror(errno));
		return(-1);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		lprintf(1, "citserver: Can't bind: %s\n",
			strerror(errno));
		return(-1);
	}

	if (listen(s, queue_len) < 0) {
		lprintf(1, "citserver: Can't listen: %s\n", strerror(errno));
		return(-1);
	}

	return(s);
}



/*
 * Return a pointer to the CitContext structure bound to the thread which
 * called this function.  If there's no such binding (for example, if it's
 * called by the housekeeper thread) then a generic 'master' CC is returned.
 */
struct CitContext *MyContext(void) {
	struct CitContext *retCC;
	retCC = (struct CitContext *) pthread_getspecific(MyConKey);
	if (retCC == NULL) retCC = &masterCC;
	return(retCC);
}


/*
 * Initialize a new context and place it in the list.
 */
struct CitContext *CreateNewContext(void) {
	struct CitContext *me, *ptr;
	int num = 1;
	int startover = 0;

	me = (struct CitContext *) mallok(sizeof(struct CitContext));
	if (me == NULL) {
		lprintf(1, "citserver: can't allocate memory!!\n");
		return NULL;
	}
	memset(me, 0, sizeof(struct CitContext));

	/* The new context will be created already in the CON_EXECUTING state
	 * in order to prevent another thread from grabbing it while it's
	 * being set up.
	 */
	me->state = CON_EXECUTING;

	begin_critical_section(S_SESSION_TABLE);

	/* obtain a unique session number */
	do {
		startover = 0;
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->cs_pid == num) {
				++num;
				startover = 1;
			}
		}
	} while (startover == 1);

	me->cs_pid = num;
	me->next = ContextList;
	ContextList = me;
	++num_sessions;

	end_critical_section(S_SESSION_TABLE);
	return(me);
}



/*
 * client_write()   ...    Send binary data to the client.
 */
void client_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	int sock;

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

	while (bytes_written < nbytes) {
		retval = write(sock, &buf[bytes_written],
			nbytes - bytes_written);
		if (retval < 1) {
			lprintf(2, "client_write() failed: %s\n",
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
        char buf[256];   
   
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
			lprintf(2, "client_read() failed: %s\n",
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
int client_read(char *buf, int bytes)
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
		if (retval != 1 || buf[i] == '\n' || i == 255)
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == 255)
		while (buf[i] != '\n' && retval == 1)
			retval = client_read(&buf[i], 1);

	/* Strip the trailing newline and any trailing nonprintables (cr's)
	 */
	buf[i] = 0;
	while ((strlen(buf)>0)&&(!isprint(buf[strlen(buf)-1])))
		buf[strlen(buf)-1] = 0;
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
		lprintf(3, "Closing listener on port %d\n",
			serviceptr->tcp_port);
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
 * Tie in to the 'netsetup' program.
 *
 * (We're going to hope that netsetup never feeds more than 4096 bytes back.)
 */
void cmd_nset(char *cmdbuf)
{
	int retcode;
	char fbuf[4096];
	FILE *netsetup;
	int ch;
	int a, b;
	char netsetup_args[3][256];

	if (CC->usersupp.axlevel < 6) {
		cprintf("%d Higher access required.\n", 
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	for (a=1; a<=3; ++a) {
		if (num_parms(cmdbuf) >= a) {
			extract(netsetup_args[a-1], cmdbuf, a-1);
			for (b=0; b<strlen(netsetup_args[a-1]); ++b) {
				if (netsetup_args[a-1][b] == 34) {
					netsetup_args[a-1][b] = '_';
				}
			}
		}
		else {
			netsetup_args[a-1][0] = 0;
		}
	}

	sprintf(fbuf, "./netsetup \"%s\" \"%s\" \"%s\" </dev/null 2>&1",
		netsetup_args[0], netsetup_args[1], netsetup_args[2]);
	netsetup = popen(fbuf, "r");
	if (netsetup == NULL) {
		cprintf("%d %s\n", ERROR, strerror(errno));
		return;
	}

	fbuf[0] = 0;
	while (ch = getc(netsetup), (ch > 0)) {
		fbuf[strlen(fbuf)+1] = 0;
		fbuf[strlen(fbuf)] = ch;
	}

	retcode = pclose(netsetup);

	if (retcode != 0) {
		for (a=0; a<strlen(fbuf); ++a) {
			if (fbuf[a] < 32) fbuf[a] = 32;
		}
		fbuf[245] = 0;
		cprintf("%d %s\n", ERROR, fbuf);
		return;
	}

	cprintf("%d Command succeeded.  Output follows:\n", LISTING_FOLLOWS);
	cprintf("%s", fbuf);
	if (fbuf[strlen(fbuf)-1] != 10) cprintf("\n");
	cprintf("000\n");
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
 * of CPU time and keep the context list locked too much.
 *
 * After that's done, we raise or lower the size of the worker thread pool
 * if such an action is appropriate.
 */
void dead_session_purge(void) {
	struct CitContext *ptr, *rem;
        pthread_attr_t attr;
	pthread_t newthread;

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
			lprintf(9, "Purging session %d\n", rem->cs_pid);
			RemoveContext(rem);
		}

	} while (rem != NULL);


	/* Raise or lower the size of the worker thread pool if such
	 * an action is appropriate.
	 */

	if ( (num_sessions > num_threads)
	   && (num_threads < config.c_max_workers) ) {

		pthread_attr_init(&attr);
       		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&newthread, &attr,
	   	   (void* (*)(void*)) worker_thread, NULL) != 0) {
			lprintf(1, "Can't create worker thead: %s\n",
			strerror(errno));
		}

	}
	
	else if ( (num_sessions < num_threads)
	   && (num_threads > config.c_min_workers) ) {
		--num_threads;
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
}



/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	pthread_t HousekeepingThread;	/* Thread descriptor */
        pthread_attr_t attr;		/* Thread attributes */
	char tracefile[128];		/* Name of file to log traces to */
	int a, i;			/* General-purpose variables */
	struct passwd *pw;
	int drop_root_perms = 1;
	char *moddir;
	struct ServiceFunctionHook *serviceptr;
        
	/* specify default port name and trace file */
	strcpy(tracefile, "");

	/* initialize the master context */
	InitializeMasterCC();

	/* parse command-line arguments */
	for (a=1; a<argc; ++a) {

		/* -t specifies where to log trace messages to */
		if (!strncmp(argv[a], "-t", 2)) {
			strcpy(tracefile, argv[a]);
			strcpy(tracefile, &tracefile[2]);
			freopen(tracefile, "r", stdin);
			freopen(tracefile, "w", stdout);
			freopen(tracefile, "w", stderr);
		}

		/* run in the background if -d was specified */
		else if (!strcmp(argv[a], "-d")) {
			start_daemon( (strlen(tracefile) > 0) ? 0 : 1 ) ;
		}

		/* -x specifies the desired logging level */
		else if (!strncmp(argv[a], "-x", 2)) {
			verbosity = atoi(&argv[a][2]);
		}

		else if (!strncmp(argv[a], "-h", 2)) {
			safestrncpy(bbs_home_directory, &argv[a][2],
				    sizeof bbs_home_directory);
			home_specified = 1;
		}

		else if (!strncmp(argv[a], "-f", 2)) {
			do_defrag = 1;
		}

		/* -r tells the server not to drop root permissions. don't use
		 * this unless you know what you're doing. this should be
		 * removed in the next release if it proves unnecessary. */
		else if (!strcmp(argv[a], "-r"))
			drop_root_perms = 0;

		/* any other parameter makes it crash and burn */
		else {
			lprintf(1,	"citserver: usage: "
					"citserver [-tTraceFile] [-d] [-f]"
					" [-xLogLevel] [-hHomeDir]\n");
			exit(1);
		}

	}

	/* Tell 'em who's in da house */
	lprintf(1,
"\nMultithreaded message server for Citadel/UX\n"
"Copyright (C) 1987-1999 by the Citadel/UX development team.\n"
"Citadel/UX is free software, covered by the GNU General Public License, and\n"
"you are welcome to change it and/or distribute copies of it under certain\n"
"conditions.  There is absolutely no warranty for this software.  Please\n"
"read the 'COPYING.txt' file for details.\n\n");

	/* Initialize... */
	init_sysdep();
	openlog("citserver",LOG_PID,LOG_USER);

	/* Load site-specific parameters */
	lprintf(7, "Loading citadel.config\n");
	get_config();

	/*
	 * Do non system dependent startup functions.
	 */
	master_startup();

	/*
	 * Bind the server to our favorite ports.
	 */
	CtdlRegisterServiceHook(config.c_port_number,		/* TCP */
				NULL,
				citproto_begin_session,
				do_command_loop);
	CtdlRegisterServiceHook(0,				/* TCP */
				"citadel.socket",
				citproto_begin_session,
				do_command_loop);

	/*
	 * Load any server-side modules (plugins) available here.
	 */
	lprintf(7, "Initializing loadable modules\n");
	if ((moddir = malloc(strlen(bbs_home_directory) + 9)) != NULL) {
		sprintf(moddir, "%s/modules", bbs_home_directory);
		DLoader_Init(moddir);
		free(moddir);
	}

	/*
	 * The rescan pipe exists so that worker threads can be woken up and
	 * told to re-scan the context list for fd's to listen on.  This is
	 * necessary, for example, when a context is about to go idle and needs
	 * to get back on that list.
	 */
	if (pipe(rescan)) {
		lprintf(1, "Can't create rescan pipe!\n");
		exit(errno);
	}

	/*
	 * Set up a fd_set containing all the master sockets to which we
	 * always listen.  It's computationally less expensive to just copy
	 * this to a local fd_set when starting a new select() and then add
	 * the client sockets than it is to initialize a new one and then
	 * figure out what to put there.
	 */
	FD_ZERO(&masterfds);
	masterhighest = 0;
	FD_SET(rescan[0], &masterfds);
	if (rescan[0] > masterhighest) masterhighest = rescan[0];

	for (serviceptr = ServiceHookTable; serviceptr != NULL;
	    serviceptr = serviceptr->next ) {
		FD_SET(serviceptr->msock, &masterfds);
		if (serviceptr->msock > masterhighest) {
			masterhighest = serviceptr->msock;
		}
	}


	/*
	 * Now that we've bound the sockets, change to the BBS user id and its
	 * corresponding group ids
	 */
	if (drop_root_perms) {
		if ((pw = getpwuid(BBSUID)) == NULL)
			lprintf(1, "WARNING: getpwuid(%d): %s\n"
				   "Group IDs will be incorrect.\n", BBSUID,
				strerror(errno));
		else {
			initgroups(pw->pw_name, pw->pw_gid);
			if (setgid(pw->pw_gid))
				lprintf(3, "setgid(%d): %s\n", pw->pw_gid,
					strerror(errno));
		}
		lprintf(7, "Changing uid to %d\n", BBSUID);
		if (setuid(BBSUID) != 0) {
			lprintf(3, "setuid() failed: %s\n", strerror(errno));
		}
	}

	/*
	 * Create the housekeeper thread
	 */
	lprintf(7, "Starting housekeeper thread\n");
	pthread_attr_init(&attr);
       	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (pthread_create(&HousekeepingThread, &attr,
	   (void* (*)(void*)) housekeeping_loop, NULL) != 0) {
		lprintf(1, "Can't create housekeeping thead: %s\n",
			strerror(errno));
	}


	/*
	 * Now create a bunch of worker threads.
	 */
	for (i=0; i<(config.c_min_workers-1); ++i) {
		pthread_attr_init(&attr);
       		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&HousekeepingThread, &attr,
	   	   (void* (*)(void*)) worker_thread, NULL) != 0) {
			lprintf(1, "Can't create worker thead: %s\n",
			strerror(errno));
		}
	}

	/* Now this thread can become a worker as well. */
	worker_thread();

	return(0);
}


/*
 * Bind a thread to a context.
 */
inline void become_session(struct CitContext *which_con) {
	pthread_setspecific(MyConKey, (void *)which_con );
}



/* 
 * This loop just keeps going and going and going...
 */	
void worker_thread(void) {
	int i;
	char junk;
	int highest;
	struct CitContext *ptr;
	struct CitContext *bind_me = NULL;
	fd_set readfds;
	int retval;
	struct CitContext *con= NULL;	/* Temporary context pointer */
	struct ServiceFunctionHook *serviceptr;
	struct sockaddr_in fsin;	/* Data for master socket */
	int alen;			/* Data for master socket */
	int ssock;			/* Descriptor for client socket */
	struct timeval tv;

	++num_threads;

	while (!time_to_die) {

		/* 
		 * A naive implementation would have all idle threads
		 * calling select() and then they'd all wake up at once.  We
		 * solve this problem by putting the select() in a critical
		 * section, so only one thread has the opportunity to wake
		 * up.  If we wake up on the master socket, create a new
		 * session context; otherwise, just bind the thread to the
		 * context we want and go on our merry way.
		 */

		begin_critical_section(S_I_WANNA_SELECT);
SETUP_FD:	memcpy(&readfds, &masterfds, sizeof(fd_set) );
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

		tv.tv_sec = 60;		/* wake up every minute if no input */
		tv.tv_usec = 0;
		retval = select(highest + 1, &readfds, NULL, NULL, &tv);

		/* Now figure out who made this select() unblock.
		 * First, check for an error or exit condition.
		 */
		if (retval < 0) {
			end_critical_section(S_I_WANNA_SELECT);
			lprintf(9, "Exiting (%s)\n", strerror(errno));
			time_to_die = 1;
		}

		/* Next, check to see if it's a new client connecting
		 * on the master socket.
		 */
		else for (serviceptr = ServiceHookTable; serviceptr != NULL;
		     serviceptr = serviceptr->next ) {

			if (FD_ISSET(serviceptr->msock, &readfds)) {
				alen = sizeof fsin;
				ssock = accept(serviceptr->msock,
					(struct sockaddr *)&fsin, &alen);
				if (ssock < 0) {
					lprintf(2, "citserver: accept(): %s\n",
						strerror(errno));
				}
				else {
					lprintf(7, "citserver: "
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
		if (!time_to_die) if (FD_ISSET(rescan[0], &readfds)) {
			read(rescan[0], &junk, 1);
			goto SETUP_FD;
		}

		/* It must be a client socket.  Find a context that has data
		 * waiting on its socket *and* is in the CON_IDLE state.
		 */
		else {
			bind_me = NULL;
			begin_critical_section(S_SESSION_TABLE);
			for (ptr = ContextList;
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
		if ((time(NULL) - last_timer) > 60L) {
			last_timer = time(NULL);
			PerformSessionHooks(EVT_TIMER);
		}
	}

	/* If control reaches this point, the server is shutting down */	
	master_cleanup();
	--num_threads;
	pthread_exit(NULL);
}

