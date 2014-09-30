/*
 * WebCit "system dependent" code.
 *
 * Copyright (c) 1996-2014 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <time.h>
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
#include "webcit.h"
#include "sysdep.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "webserver.h"
#include "modules_init.h"
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
pthread_key_t MyConKey;				/* TSD key for MyContext() */
pthread_key_t MyReq;				/* TSD key for MyReq() */
int msock;			/* master listening socket */
int time_to_die = 0;            /* Nonzero if server is shutting down */

extern void *context_loop(ParsedHttpHdrs *Hdr);
extern void *housekeeping_loop(void);
extern void do_housekeeping(void);

char ctdl_key_dir[PATH_MAX]=SSL_DIR;
char file_crpt_file_key[PATH_MAX]="";
char file_crpt_file_csr[PATH_MAX]="";
char file_crpt_file_cer[PATH_MAX]="";
char file_etc_mimelist[PATH_MAX]="";

const char editor_absolut_dir[PATH_MAX]=EDITORDIR;	/* nailed to what configure gives us. */
const char markdown_editor_absolutedir[]=MARKDOWNEDITORDIR;

char etc_dir[PATH_MAX];
char static_dir[PATH_MAX];		/* calculated on startup */
char static_local_dir[PATH_MAX];		/* calculated on startup */
char static_icon_dir[PATH_MAX];          /* where should we find our mime icons? */
char  *static_dirs[]={				/* needs same sort order as the web mapping */
	(char*)static_dir,			/* our templates on disk */
	(char*)static_local_dir,		/* user provided templates disk */
	(char*)editor_absolut_dir,		/* the editor on disk */
	(char*)static_icon_dir,                  /* our icons... */
	(char*)markdown_editor_absolutedir
};

int ExitPipe[2];
HashList *GZMimeBlackList = NULL; /* mimetypes which shouldn't be gzip compressed */

void LoadMimeBlacklist(void)
{
	StrBuf *MimeBlackLine;
	IOBuffer IOB;
	eReadState state;
	
	memset(&IOB, 0, sizeof(IOBuffer));
	IOB.fd = open(file_etc_mimelist, O_RDONLY);

	IOB.Buf = NewStrBuf();
	MimeBlackLine = NewStrBuf();
	GZMimeBlackList = NewHash(1, NULL);

	do
	{
		state = StrBufChunkSipLine(MimeBlackLine, &IOB);

		switch (state)
		{
		case eMustReadMore:
			if (StrBuf_read_one_chunk_callback(IOB.fd, 0, &IOB) <= 0)
				state = eReadFail;
			break;
		case eReadSuccess:
			if ((StrLength(MimeBlackLine) > 1) && 
			    (*ChrPtr(MimeBlackLine) != '#'))
			{
				Put(GZMimeBlackList, SKEY(MimeBlackLine),
				    (void*) 1, reference_free_handler);
			}
			FlushStrBuf(MimeBlackLine);
			break;
		case eReadFail:
			break;
		case eBufferNotEmpty:
			break;
		}
	}
	while (state != eReadFail);

	close(IOB.fd);
	
	FreeStrBuf(&IOB.Buf);
	FreeStrBuf(&MimeBlackLine);
}

void CheckGZipCompressionAllowed(const char *MimeType, long MLen)
{
	void *v;
	wcsession *WCC = WC;

	if (WCC->Hdr->HR.gzip_ok)
	    WCC->Hdr->HR.gzip_ok = GetHash(GZMimeBlackList, MimeType, MLen, &v) == 0;
}

void InitialiseSemaphores(void)
{
	int i;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (i=0; i<MAX_SEMAPHORES; ++i) {
		pthread_mutex_init(&Critters[i], NULL);
	}

	if (pipe(ExitPipe))
	{
		syslog(LOG_WARNING, "Failed to open exit pipe: %d [%s]\n", 
		       errno, 
		       strerror(errno));
		
		exit(-1);
	}
}

/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	pthread_mutex_unlock(&Critters[which_one]);
}


void ShutDownWebcit(void)
{
	free_zone_directory ();
	icaltimezone_release_zone_tab ();
	icalmemory_free_ring ();
	ShutDownLibCitadel ();
	shutdown_modules ();
#ifdef HAVE_OPENSSL
	if (is_https) {
		shutdown_ssl();
	}
#endif
}

/*
 * Entry point for worker threads
 */
void worker_entry(void)
{
	int ssock;
	int i = 0;
	int fail_this_transaction = 0;
	ParsedHttpHdrs Hdr;

	memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
	Hdr.HR.eReqType = eGET;
	http_new_modules(&Hdr);	

	do {
		/* Each worker thread blocks on accept() while waiting for something to do. */
		fail_this_transaction = 0;
		ssock = -1; 
		errno = EAGAIN;
		do {
			fd_set wset;
			--num_threads_executing;
                        FD_ZERO(&wset);
                        FD_SET(msock, &wset);
                        FD_SET(ExitPipe[1], &wset);

                        select(msock + 1, NULL, &wset, NULL, NULL);
			if (time_to_die)
				break;

			ssock = accept(msock, NULL, 0);
			++num_threads_executing;
			if (ssock < 0) fail_this_transaction = 1;
		} while ((msock > 0) && (ssock < 0)  && (time_to_die == 0));

		if ((msock == -1)||(time_to_die))
		{/* ok, we're going down. */
			int shutdown = 0;

			/* The first thread to get here will have to do the cleanup.
			 * Make sure it's really just one.
			 */
			begin_critical_section(S_SHUTDOWN);
			if (msock == -1)
			{
				msock = -2;
				shutdown = 1;
			}
			end_critical_section(S_SHUTDOWN);
			if (shutdown == 1)
			{/* we're the one to cleanup the mess. */
				http_destroy_modules(&Hdr);
				syslog(LOG_DEBUG, "I'm master shutdown: tagging sessions to be killed.\n");
				shutdown_sessions();
				syslog(LOG_DEBUG, "master shutdown: waiting for others\n");
				sleeeeeeeeeep(1); /* wait so some others might finish... */
				syslog(LOG_DEBUG, "master shutdown: cleaning up sessions\n");
				do_housekeeping();
				syslog(LOG_DEBUG, "master shutdown: cleaning up libical\n");

				ShutDownWebcit();

				syslog(LOG_DEBUG, "master shutdown exiting.\n");				
				exit(0);
			}
			break;
		}
		if (ssock < 0 ) continue;

		check_thread_pool_size();

		/* Now do something. */
		if (msock < 0) {
			if (ssock > 0) close (ssock);
			syslog(LOG_DEBUG, "in between.");
			pthread_exit(NULL);
		} else {
			/* Got it? do some real work! */
			/* Set the SO_REUSEADDR socket option */
			i = 1;
			setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

			/* If we are an HTTPS server, go crypto now. */
#ifdef HAVE_OPENSSL
			if (is_https) {
				if (starttls(ssock) != 0) {
					fail_this_transaction = 1;
					close(ssock);
				}
			}
			else 
#endif
			{
				int fdflags; 
				fdflags = fcntl(ssock, F_GETFL);
				if (fdflags < 0)
					syslog(LOG_WARNING, "unable to get server socket flags! %s \n",
						strerror(errno));
				fdflags = fdflags | O_NONBLOCK;
				if (fcntl(ssock, F_SETFL, fdflags) < 0)
					syslog(LOG_WARNING, "unable to set server socket nonblocking flags! %s \n",
						strerror(errno));
			}

			if (fail_this_transaction == 0) {
				Hdr.http_sock = ssock;

				/* Perform an HTTP transaction... */
				context_loop(&Hdr);

				/* Shut down SSL/TLS if required... */
#ifdef HAVE_OPENSSL
				if (is_https) {
					endtls();
				}
#endif

				/* ...and close the socket. */
				if (Hdr.http_sock > 0) {
					lingering_close(ssock);
				}
				http_detach_modules(&Hdr);

			}

		}

	} while (!time_to_die);

	http_destroy_modules(&Hdr);
	syslog(LOG_DEBUG, "Thread exiting.\n");
	pthread_exit(NULL);
}


/*
 * Shut us down the regular way.
 * signum is the signal we want to forward
 */
pid_t current_child;
void graceful_shutdown_watcher(int signum) {
	syslog(LOG_INFO, "Watcher thread exiting.\n");
	write(ExitPipe[0], HKEY("                              "));
	kill(current_child, signum);
	if (signum != SIGHUP)
		exit(0);
}


/*
 * Shut us down the regular way.
 * signum is the signal we want to forward
 */
pid_t current_child;
void graceful_shutdown(int signum) {
	FILE *FD;
	int fd;

	syslog(LOG_INFO, "WebCit is being shut down on signal %d.\n", signum);
	fd = msock;
	msock = -1;
	time_to_die = 1;
	FD=fdopen(fd, "a+");
	fflush (FD);
	fclose (FD);
	close(fd);
	write(ExitPipe[0], HKEY("                              "));
}


/*
 * Start running as a daemon.
 */
void start_daemon(char *pid_file) 
{
	int status = 0;
	pid_t child = 0;
	FILE *fp;
	int do_restart = 0;

	current_child = 0;

	/* Close stdin/stdout/stderr and replace them with /dev/null.
	 * We don't just call close() because we don't want these fd's
	 * to be reused for other files.
	 */
	chdir("/");

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	child = fork();
	if (child != 0) {
		exit(0);
	}

	setsid();
	umask(0);
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
	signal(SIGTERM, graceful_shutdown_watcher);
	signal(SIGHUP, graceful_shutdown_watcher);

	do {
		current_child = fork();

	
		if (current_child < 0) {
			perror("fork");
			ShutDownLibCitadel ();
			exit(errno);
		}
	
		else if (current_child == 0) {	/* child process */
			signal(SIGHUP, graceful_shutdown);

			return; /* continue starting webcit. */
		}
		else { /* watcher process */
			if (pid_file) {
				fp = fopen(pid_file, "w");
				if (fp != NULL) {
					fprintf(fp, "%d\n", getpid());
					fclose(fp);
				}
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

	if (pid_file) {
		unlink(pid_file);
	}
	ShutDownLibCitadel ();
	exit(WEXITSTATUS(status));
}


/*
 * Spawn an additional worker thread into the pool.
 */
void spawn_another_worker_thread()
{
	pthread_t SessThread;	/* Thread descriptor */
	pthread_attr_t attr;	/* Thread attributes */
	int ret;

	++num_threads_existing;
	++num_threads_executing;

	/* set attributes for the new thread */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/*
	 * Our per-thread stacks need to be bigger than the default size,
	 * otherwise the MIME parser crashes on FreeBSD.
	 */
	if ((ret = pthread_attr_setstacksize(&attr, 1024 * 1024))) {
		syslog(LOG_WARNING, "pthread_attr_setstacksize: %s\n", strerror(ret));
		pthread_attr_destroy(&attr);
	}

	/* now create the thread */
	if (pthread_create(&SessThread, &attr, (void *(*)(void *)) worker_entry, NULL) != 0) {
		syslog(LOG_WARNING, "Can't create thread: %s\n", strerror(errno));
	}

	/* free up the attributes */
	pthread_attr_destroy(&attr);
}


void
webcit_calc_dirs_n_files(int relh, const char *basedir, int home, char *webcitdir, char *relhome)
{
	char dirbuffer[PATH_MAX]="";
	/* calculate all our path on a central place */
    /* where to keep our config */
	
#define COMPUTE_DIRECTORY(SUBDIR) memcpy(dirbuffer,SUBDIR, sizeof dirbuffer);\
	snprintf(SUBDIR,sizeof SUBDIR,  "%s%s%s%s%s%s%s", \
			 (home&!relh)?webcitdir:basedir, \
             ((basedir!=webcitdir)&(home&!relh))?basedir:"/", \
             ((basedir!=webcitdir)&(home&!relh))?"/":"", \
			 relhome, \
             (relhome[0]!='\0')?"/":"",\
			 dirbuffer,\
			 (dirbuffer[0]!='\0')?"/":"");
	basedir=RUNDIR;
	COMPUTE_DIRECTORY(socket_dir);
	basedir=WWWDIR "/static";
	COMPUTE_DIRECTORY(static_dir);
	basedir=WWWDIR "/static/icons";
	COMPUTE_DIRECTORY(static_icon_dir);
	basedir=WWWDIR "/static.local";
	COMPUTE_DIRECTORY(static_local_dir);
	StripSlashes(static_dir, 1);
	StripSlashes(static_icon_dir, 1);
	StripSlashes(static_local_dir, 1);

	snprintf(file_crpt_file_key,
		 sizeof file_crpt_file_key, 
		 "%s/citadel.key",
		 ctdl_key_dir);
	snprintf(file_crpt_file_csr,
		 sizeof file_crpt_file_csr, 
		 "%s/citadel.csr",
		 ctdl_key_dir);
	snprintf(file_crpt_file_cer,
		 sizeof file_crpt_file_cer, 
		 "%s/citadel.cer",
		 ctdl_key_dir);


	basedir=ETCDIR;
	COMPUTE_DIRECTORY(etc_dir);
	StripSlashes(etc_dir, 1);
	snprintf(file_etc_mimelist,
		 sizeof file_etc_mimelist, 
		 "%s/nogz-mimetypes.txt",
		 etc_dir);

	/* we should go somewhere we can leave our coredump, if enabled... */
	syslog(LOG_INFO, "Changing directory to %s\n", socket_dir);
	if (chdir(webcitdir) != 0) {
		perror("chdir");
	}
}

void drop_root(uid_t UID)
{
	struct passwd pw, *pwp = NULL;

	/*
	 * Now that we've bound the sockets, change to the Citadel user id and its
	 * corresponding group ids
	 */
	if (UID != -1) {
		
#ifdef HAVE_GETPWUID_R
#ifdef SOLARIS_GETPWUID
		pwp = getpwuid_r(UID, &pw, pwbuf, sizeof(pwbuf));
#else /* SOLARIS_GETPWUID */
		getpwuid_r(UID, &pw, pwbuf, sizeof(pwbuf), &pwp);
#endif /* SOLARIS_GETPWUID */
#else /* HAVE_GETPWUID_R */
		pwp = NULL;
#endif /* HAVE_GETPWUID_R */

		if (pwp == NULL)
			syslog(LOG_CRIT, "WARNING: getpwuid(%d): %s\n"
				"Group IDs will be incorrect.\n", UID,
				strerror(errno));
		else {
			initgroups(pw.pw_name, pw.pw_gid);
			if (setgid(pw.pw_gid))
				syslog(LOG_CRIT, "setgid(%ld): %s\n", (long)pw.pw_gid,
					strerror(errno));
		}
		syslog(LOG_INFO, "Changing uid to %ld\n", (long)UID);
		if (setuid(UID) != 0) {
			syslog(LOG_CRIT, "setuid() failed: %s\n", strerror(errno));
		}
#if defined (HAVE_SYS_PRCTL_H) && defined (PR_SET_DUMPABLE)
		prctl(PR_SET_DUMPABLE, 1);
#endif
	}
}


/*
 * print the actual stack frame.
 */
void wc_backtrace(long LogLevel)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;


	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			syslog(LogLevel, "%s\n", strings[i]);
		else
			syslog(LogLevel, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
}
