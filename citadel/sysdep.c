/*
 * Citadel/UX "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * Here's where we (hopefully) have all the parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * Wherever possible, we use function wrappers and type definitions to create
 * abstractions that are platform-independent from the calling side.
 * 
 * Eventually we'll try porting to a different platform and either have
 * multiple variants of this file or simply load it up with #ifdefs.
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "hooks.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "dynloader.h"

#ifdef NEED_SELECT_H
#include <sys/select.h>
#endif

pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
pthread_key_t MyConKey;				/* TSD key for MyContext() */
symtab *my_symtab;				/* Dynamic modules symbol table */

int msock;					/* master listening socket */
int verbosity = 3;				/* Logging level */


/*
 * lprintf()  ...   Write logging information
 */
void lprintf(int loglevel, const char *format, ...) {   
        va_list arg_ptr;   
        char buf[256];   
        int rc;   
  
	if (loglevel <= verbosity) { 
        	va_start(arg_ptr, format);   
        	rc = vsprintf(buf, format, arg_ptr);   
        	va_end(arg_ptr);   
		
		fprintf(stderr, "%s", buf);
		fflush(stderr);
		}
  
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
	 * Set up a place to put thred-specific data.
	 * We only need a single pointer per thread - it points to the
	 * thread's CitContext structure in the ContextList linked list.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		lprintf(1, "Can't create TSD key!!  %s\n", strerror(errno));
		}

	/*
	 * The action for unexpected signals and exceptions should be to
	 * call master_cleanup() to gracefully shut down the server.
	 */
	signal(SIGINT, (void(*)(int))master_cleanup);
	signal(SIGQUIT, (void(*)(int))master_cleanup);
	signal(SIGHUP, (void(*)(int))master_cleanup);
	signal(SIGTERM, (void(*)(int))master_cleanup);
	}


/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	int oldval;

	lprintf(8, "begin_critical_section(%d)\n", which_one);

	/* Don't get interrupted during the critical section */
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldval);

	/* Obtain a semaphore */
	pthread_mutex_lock(&Critters[which_one]);

	}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	int oldval;

	lprintf(8, "  end_critical_section(%d)\n", which_one);

	/* Let go of the semaphore */
	pthread_mutex_unlock(&Critters[which_one]);

	/* If a cancel was sent during the critical section, do it now.
	 * Then re-enable thread cancellation.
	 */
	pthread_testcancel();
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldval);
	pthread_testcancel();

	}



/*
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 */
int ig_tcp_server(int port_number, int queue_len)
{
	struct sockaddr_in sin;
	int s, i;

	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	if (port_number == 0) {
		lprintf(1, "citserver: No port number specified.  Run setup again.\n");
		exit(1);
		}
	
	sin.sin_port = htons((u_short)port_number);

	s = socket(PF_INET, SOCK_STREAM, (getprotobyname("tcp")->p_proto));
	if (s < 0) {
		lprintf(1, "citserver: Can't create a socket: %s\n",
			strerror(errno));
		exit(errno);
		}

	/* Set the SO_REUSEADDR socket option, because it makes sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		lprintf(1, "citserver: Can't bind: %s\n", strerror(errno));
		exit(errno);
		}

	if (listen(s, queue_len) < 0) {
		lprintf(1, "citserver: Can't listen: %s\n", strerror(errno));
		exit(errno);
		}

	return(s);
	}


/*
 * Return a pointer to a thread's own CitContext structure (old)
 * NOTE: this version of MyContext() is commented out because it is no longer
 * in use.  It was written before I discovered TSD keys.  This
 * version pounds through the context list until it finds the one matching
 * the currently running thread.  It remains here, commented out, in case it
 * is needed for future ports to threading libraries which have the equivalent
 * of pthread_self() but not pthread_key_create() and its ilk.
 *
 * struct CitContext *MyContext() {
 *	struct CitContext *ptr;
 *	THREAD me;
 *
 *	me = pthread_self();
 *	for (ptr=ContextList; ptr!=NULL; ptr=ptr->next) {
 *		if (ptr->mythread == me) return(ptr);
 *		}
 *	return(NULL);
 *	}
 */

/*
 * Return a pointer to a thread's own CitContext structure (new)
 */
struct CitContext *MyContext(void) {
	return (struct CitContext *) pthread_getspecific(MyConKey);
	}


/*
 * Wedge our way into the context list.
 */
struct CitContext *CreateNewContext(void) {
	struct CitContext *me;

	lprintf(9, "CreateNewContext: calling malloc()\n");
	me = (struct CitContext *) malloc(sizeof(struct CitContext));
	if (me == NULL) {
		lprintf(1, "citserver: can't allocate memory!!\n");
		pthread_exit(NULL);
		}
	bzero(me, sizeof(struct CitContext));

	begin_critical_section(S_SESSION_TABLE);
	me->next = ContextList;
	ContextList = me;
	end_critical_section(S_SESSION_TABLE);
	return(me);
	}

/*
 * Add a thread's thread ID to the context
 */
void InitMyContext(struct CitContext *con)
{
	int oldval;

	con->mythread = pthread_self();
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldval);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldval);
	if (pthread_setspecific(MyConKey, (void *)con) != 0) {
		lprintf(1, "ERROR!  pthread_setspecific() failed: %s\n",
			strerror(errno));
		}
	}

/*
 * Remove a context from the context list.
 */
void RemoveContext(struct CitContext *con)
{
	struct CitContext *ptr;

	lprintf(7, "Starting RemoveContext()\n");
	lprintf(9, "session count before RemoveContext is %d\n", session_count());
	if (con==NULL) {
		lprintf(7, "WARNING: RemoveContext() called with null!\n");
		return;
		}

	begin_critical_section(S_SESSION_TABLE);
	lprintf(7, "Closing socket %d\n", con->client_socket);
	close(con->client_socket);

	if (ContextList==con) {
		ContextList = ContextList->next;
		}
	else {
		for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
			if (ptr->next == con) {
				ptr->next = ptr->next->next;
				}
			}
		}
	
	free(con);

	lprintf(9, "session count after RemoveContext is %d\n", session_count());

	lprintf(7, "Done with RemoveContext\n");
	end_critical_section(S_SESSION_TABLE);
	}


/*
 * Return the number of sessions currently running.
 * (This should probably be moved out of sysdep.c)
 */
int session_count(void) {
	struct CitContext *ptr;
	int TheCount = 0;

	lprintf(9, "session_count() starting\n");
	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		++TheCount;
		lprintf(9, "Counted session %3d (%d)\n", ptr->cs_pid, TheCount);
		}

	lprintf(9, "session_count() finishing\n");
	return(TheCount);
	}


/*
 * client_write()   ...    Send binary data to the client.
 */
void client_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(CC->client_socket, &buf[bytes_written],
			nbytes - bytes_written);
		if (retval < 1) {
			lprintf(2, "client_write() failed: %s\n",
				strerror(errno));
			cleanup(errno);
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
        int rc;   
   
        va_start(arg_ptr, format);   
        rc = vsprintf(buf, format, arg_ptr);   
        va_end(arg_ptr);   
  
	client_write(buf, strlen(buf)); 
	}   


/*
 * Read data from the client socket.
 * Return values are:
 *	1	Requested number of bytes has been read.
 *	0	Request timed out.
 * If the socket breaks, the session is immediately terminated.
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
			cleanup(errno);
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
	int retval = 0;

	/* Clear the buffer, and read one character at a time.
	 */
	buf[0] = 0;
	do {
		if (strlen(buf)<255) {
			buf[strlen(buf) + 1] = 0;
			retval = client_read(&buf[strlen(buf)], 1);
			}
		} while ( (buf[strlen(buf)-1] != 10) && (retval==1) );

	/* Strip the trailing newline.
	 */
	if (strlen(buf) > 0) buf[strlen(buf)-1] = 0;
	return(retval);
	}



/*
 * The system-dependent part of master_cleanup() - close the master socket.
 */
void sysdep_master_cleanup(void) {
	lprintf(7, "Closing master socket %d\n", msock);
	close(msock);
	}

/*
 * Cleanup routine to be called when one thread is shutting down.
 */
void cleanup(int exit_code)
{
	/* Terminate the thread.
	 * Its cleanup handler will call cleanup_stuff()
	 */
	lprintf(7, "Calling pthread_exit()\n");
	pthread_exit(NULL);
	}

/*
 * Terminate another session.
 */
void kill_session(int session_to_kill) {
	struct CitContext *ptr;

	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if (ptr->cs_pid == session_to_kill) {
			pthread_cancel(ptr->mythread);
			}
		}
	}


/*
 * The system-dependent wrapper around the main context loop.
 */
void sd_context_loop(struct CitContext *con) {
	pthread_cleanup_push(*cleanup_stuff, NULL);
	context_loop(con);
	pthread_cleanup_pop(0);
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
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	struct sockaddr_in fsin;	/* Data for master socket */
	int alen;			/* Data for master socket */
	int ssock;			/* Descriptor for master socket */
	THREAD SessThread;		/* Thread descriptor */
        pthread_attr_t attr;		/* Thread attributes */
	struct CitContext *con;		/* Temporary context pointer */
	char tracefile[128];		/* Name of file to log traces to */
	int a, i;			/* General-purpose variables */
	char convbuf[128];
	char modpath[128];
        
	/* specify default port name and trace file */
	strcpy(tracefile, "");

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
			strcpy(convbuf, argv[a]);
			verbosity = atoi(&convbuf[2]);
			}

		else if (!strncmp(argv[a], "-h", 2)) {
			strcpy(convbuf, argv[a]);
			strcpy(bbs_home_directory, &convbuf[2]);
			home_specified = 1;
			}

		/* any other parameter makes it crash and burn */
		else {
			lprintf(1, "citserver: usage: ");
			lprintf(1, "citserver [-tTraceFile]");
			lprintf(1, " [-d] [-xLogLevel] [-hHomeDir]\n");
			exit(1);
			}

		}

	/* Tell 'em who's in da house */
	lprintf(1, "Multithreaded message server for %s\n", CITADEL);
	lprintf(1, "Copyright (C) 1987-1998 by Art Cancro.  ");
	lprintf(1, "All rights reserved.\n\n");

	/* Initialize... */
	init_sysdep();
	openlog("citserver",LOG_PID,LOG_USER);
        lprintf(1, "Initting modules...\n");
        snprintf(modpath, 128, "%s/modules", BBSDIR);
        DLoader_Init(modpath, &my_symtab);
        lprintf(1, "Modules done initializing...\n");
/*
        lprintf(1, "First symtab item:");
        lprintf(1, my_symtab->fcn_name);
        lprintf(1, "\n");
*/                                                 
	/* Load site-specific parameters */
	lprintf(7, "Loading citadel.config\n");
	get_config();

	/* Databases must be opened *after* config is loaded, otherwise we might
	 * end up working in the wrong directory.
	 */
	lprintf(7, "Opening databases\n");
	open_databases();

	lprintf(7, "Checking floor reference counts\n");
	check_ref_counts();

	/*
	 * Bind the server to our favourite port.
	 * There is no need to check for errors, because ig_tcp_server()
	 * exits if it doesn't succeed.
	 */
	lprintf(7, "Attempting to bind to port %d...\n", config.c_port_number);
	msock = ig_tcp_server(config.c_port_number, 5);
	lprintf(7, "Listening on socket %d\n", msock);

	/*
	 * Now that we've bound the socket, change to the BBS user id
	lprintf(7, "Changing uid to %d\n", BBSUID);
	if (setuid(BBSUID) != 0) {
		lprintf(3, "setuid() failed: %s", strerror(errno));
		}
	 */

	/* 
	 * Endless loop.  Listen on the master socket.  When a connection
	 * comes in, create a socket, a context, and a thread.
	 */	
	while (1) {
		ssock = accept(msock, (struct sockaddr *)&fsin, &alen);
		if (ssock < 0) {
			lprintf(2, "citserver: accept() failed: %s\n",
				strerror(errno));
			}
		else {
			lprintf(7, "citserver: Client socket %d\n", ssock);
			lprintf(9, "creating context\n");
			con = CreateNewContext();
			con->client_socket = ssock;

			/* Set the SO_REUSEADDR socket option */
			lprintf(9, "setting socket options\n");
			i = 1;
			setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
				&i, sizeof(i));

			/* set attributes for the new thread */
			lprintf(9, "setting thread attributes\n");
		        pthread_attr_init(&attr);
        		pthread_attr_setdetachstate(&attr,
				PTHREAD_CREATE_DETACHED);

			/* now create the thread */
			lprintf(9, "creating thread\n");
			if (pthread_create(&SessThread, &attr, (void *)sd_context_loop,
			   con) != 0) {
				lprintf(1,
					"citserver: can't create thread: %s\n",
					strerror(errno));
				}

			/* detach the thread 
   			 * (defunct -- now done at thread creation time)
			 * if (pthread_detach(&SessThread) != 0) {
			 *	lprintf(1,
			 *		"citserver: can't detach thread: %s\n",
			 *		strerror(errno));
			 *	}
			 */
			lprintf(9, "done!\n");
			}
		}
	}

