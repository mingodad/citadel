/* $Id$ */

/* This message is exactly 1024 bytes */
const char* const message =
"The point of this little file is to stress test a Citadel server.\n"
"It spawns n threads, where n is a command line parameter, each of\n"
"which writes 1000 messages total to the server.\n"
"\n"
"-n is a command line parameter indicating how many users to simulate\n"
"(default 2000).  WARNING: Your system must be capable of creating this\n"
"many threads!\n"
"\n"
"-w is a command line parameter indicating how long to wait in seconds\n"
"between posting each message (default 10).  The actual interval\n"
"will be randomized between w / 3 and w * 3.\n"
"\n"
"A run is expected to take approximately three hours, given default\n"
"values, and assuming the server can keep up.  If the run takes much\n"
"longer than this, there may be a performance problem with the server.\n"
"For best results, the test should be run from a different machine than\n"
"the server, but connected via a fast network link (e.g. 100Base-T).\n"
"\n"
"To get baseline results, run the test with -n 1 (simulating 1 user)\n"
"on a machine with no other users logged in.\n"
"\n"
"Example:\n"
"stress -n 500 -w 25 myserver > stress.csv\n";

/* The program tries to be as small and as fast as possible.  Wherever
 * possible, we avoid allocating memory on the heap.  We do not pass data
 * between threads.  We do only a minimal amount of calculation.  In
 * particular, we only output raw timing data for the run; we do not
 * collate it, average it, or do anything else with it.  See below.
 * The program does, however, use the same CtdlIPC functions as the
 * standard Citadel text client, and incurs the same overhead as that
 * program, using those functions.
 *
 * The program first creates a new user with a randomized username which
 * begins with "testuser".  It then creates 100 rooms named test0 through
 * test99.  If they already exist, this condition is ignored.
 *
 * The program then creates n threads, all of which wait on a conditional
 * before they do anything.  Once all of the threads have been created,
 * they are signaled, and begin execution.  Each thread logs in to the
 * Citadel server separately, simulating a user login, then takes a
 * timestamp from the operating system.
 *
 * Each thread selects a room from 0-99 randomly, then writes a small
 * (1KB) test message to that room.  1K was chosen because it seems to
 * represent an average message size for messages we expect to see.
 * After writing the message, the thread sleeps for w seconds (sleep(w);)
 * and repeats the process, until it has written 1,000 messages.  The
 * program provides a status display to standard error, unless w <= 2, in
 * which case status display is disabled.
 *
 * After posting all messages, each thread takes a second timestamp, and
 * subtracts the first timestamp.  The resulting value (in seconds) is
 * sent to standard output.  The thread then exits.
 *
 * Once all threads have exited, the program exits.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "sysdep.h"
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
#include "citadel_ipc.h"

#ifndef HAVE_PTHREAD_H
#error This program requires threads
#endif

static int w = 10;		/* see above */
static int n = 2000;		/* see above */
static int m = 1000;		/* Number of messages to send; see above */

static char* username = NULL;
static char* password = NULL;

static char* hostname = NULL;
static char* portname = NULL;

/*
 * Mutex for the random number generator
 * We don't assume that rand_r() is present, so we have to
 * provide our own locking for rand()
 */
static pthread_mutex_t rand_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Conditional.  All the threads wait for this signal to actually
 * start bombarding the server.
 */
static pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t start_cond = PTHREAD_COND_INITIALIZER;

/*
 * connection died; hang it up
 */
#if 0
void connection_died(CtdlIPC* ipc, int using_ssl)
{
	CtdlIPC_delete(ipc);
	pthread_exit(NULL);
}
#endif


/*
 * This is the worker thread.  It logs in and creates the 1,000 messages
 * as described above.
 */
void* worker(void* data)
{
	CtdlIPC* ipc;	/* My connection to the server */
	int r;		/* IPC return code */
	char aaa[SIZ];	/* Generic buffer */
	int c;		/* Message count */
	time_t start, end;	/* Timestamps */
	struct ctdlipcmessage msg;	/* The message we will post */
	int* argc_;
	char*** argv_;

	argc_ = (int*)data;
	argv_ = (char***)(data + 1);

	/* Setup the message we will be posting */
	msg.text = (char*)message;
	msg.anonymous = 0;
	msg.type = 1;
	strcpy(msg.recipient, "");
	strcpy(msg.subject, "Test message; ignore");
	strcpy(msg.author, username);

	ipc = CtdlIPC_new(*argc_, *argv_, hostname, portname);
	if (!ipc)
		return NULL;	/* oops, something happened... */

	CtdlIPC_chat_recv(ipc, aaa);
	if (aaa[0] != '2') {
		fprintf(stderr, "Citadel refused me: %s\n", &aaa[4]);
		return NULL;	/* server ran out of connections maybe? */
	}

	CtdlIPCIdentifySoftware(ipc, 8, 8, REV_LEVEL, "Citadel stress tester",
		"localhost", aaa);	/* we're lying, the server knows */
	
	r = CtdlIPCTryLogin(ipc, username, aaa);
	if (r / 100 != 3) {
		fprintf(stderr, "Citadel refused username: %s\n", aaa);
		CtdlIPC_delete_ptr(&ipc);
		return NULL;	/* Gawd only knows what went wrong */
	}

	r = CtdlIPCTryPassword(ipc, password, aaa);
	if (r / 100 != 2) {
		fprintf(stderr, "Citadel refused password: %s\n", aaa);
		CtdlIPC_delete_ptr(&ipc);
		return NULL;	/* Gawd only knows what went wrong */
	}

	/* Wait for the rest of the threads */
	pthread_cond_wait(&start_cond, &start_mutex);

	/* And now the fun begins!  Send out a whole shitload of messages */
	start = time(NULL);
	for (c = 0; c < m; c++) {
		int rm;
		char room[7];
		struct ctdlipcroom *rret;

		/* Select the room to goto */
		pthread_mutex_lock(&rand_mutex);
		/* See Numerical Recipes in C or Knuth vol. 2 ch. 3 */
		rm = (int)(99.0*rand()/(RAND_MAX+1.0));
		pthread_mutex_unlock(&rand_mutex);

		/* Goto the selected room */
		sprintf(room, "test%d", rm);
		r = CtdlIPCGotoRoom(ipc, room, "", &rret, aaa);
		if (r / 100 != 2) {
			fprintf(stderr, "Citadel refused room change: %s\n", aaa);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;
		}

		/* Post the message */
		r = CtdlIPCPostMessage(ipc, 1, &msg, aaa);
		if (r / 100 != 4) {
			fprintf(stderr, "Citadel refused message entry: %s\n", aaa);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;
		}

		/* Wait for a while */
		sleep(w);
	}
	end = time(NULL);
	printf("%ld\n", end - start);
	return (void*)(end - start);
}


/*
 * Main loop.  Start a shitload of threads, all of which will attempt to
 * kick a Citadel server square in the nuts.
 */
int main(int argc, char** argv)
{
	void* data[2];		/* pass args to worker thread */
	pthread_t** threads;	/* A shitload of threads */
	pthread_attr_t attr;	/* Thread attributes (we use defaults) */
	int i;			/* Counters */
	int t = 0;
	long runtime;		/* Run time for each thread */

	data[0] = (void*)argc;	/* pass args to worker thread */
	data[1] = (void*)argv;	/* pass args to worker thread */

	/* First, memory for our shitload of threads */
	threads = calloc(n, sizeof(pthread_t*));
	if (!threads) {
		perror("Not enough memory");
		return 1;
	}

	/* Then thread attributes (all defaults for now */
	pthread_attr_init(&attr);

	/* Then, create some threads */
	for (i = 0; i < n; ++i) {
		pthread_create(threads[i], &attr, worker, data);
	}

	fprintf(stderr, "Starting in 10 seconds\r");
	sleep(10);

	/* Then, signal the conditional they all are waiting on */
	pthread_mutex_lock(&start_mutex);
	pthread_cond_broadcast(&start_cond);
	pthread_mutex_unlock(&start_mutex);

	/* Then wait for them to exit */
	for (i = 0; i < n; i++) {
		pthread_join(*(threads[i]), (void*)&runtime);
		/* We're ignoring this value for now... TODO */
	}
	fprintf(stderr, "\r                                                                               \r");
	return 0;
}
