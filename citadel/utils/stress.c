
/* This message is exactly 1024 bytes */
char* const message =
"The point of this little file is to stress test a Citadel server.\n"
"It spawns n threads, where n is a command line parameter, each of\n"
"which writes 1000 messages total to the server.\n"
"\n"
"-n is a command line parameter indicating how many users to simulate\n"
"(default 100).  WARNING: Your system must be capable of creating this\n"
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
 * sent to standard output, followed by the minimum, average, and maximum
 * amounts of time (in milliseconds) it took to post a message.  The
 * thread then exits.
 *
 * Once all threads have exited, the program exits.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <libcitadel.h>
#include "sysdep.h"
#include <time.h>
#include "citadel_ipc.h"

#ifndef HAVE_PTHREAD_H
#error This program requires threads
#endif

static int w = 10;		/* see above */
static int n = 100;		/* see above */
static int m = 1000;		/* Number of messages to send; see above */
static volatile int count = 0;	/* Total count of messages posted */
static volatile int total = 0;	/* Total messages to be posted */
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t arg_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

static char username[12];
static char password[12];

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
 * This is the worker thread.  It logs in and creates the 1,000 messages
 * as described above.
 */
void* worker(void* data)
{
	CtdlIPC* ipc;	/* My connection to the server */
	void** args;	/* Args sent in */
	int r;		/* IPC return code */
	char aaa[SIZ];	/* Generic buffer */
	int c;		/* Message count */
	time_t start, end;	/* Timestamps */
	struct ctdlipcmessage msg;	/* The message we will post */
	int argc_;
	char** argv_;
	long tmin = LONG_MAX, trun = 0, tmax = LONG_MIN;

	args = (void*)data;
	argc_ = (int)args[0];
	argv_ = (char**)args[1];

	/* Setup the message we will be posting */
	msg.text = message;
	msg.anonymous = 0;
	msg.type = 1;
	strcpy(msg.recipient, "");
	strcpy(msg.subject, "Test message; ignore");
	strcpy(msg.author, username);

	pthread_mutex_lock(&arg_mutex);
	ipc = CtdlIPC_new(argc_, argv_, NULL, NULL);
	pthread_mutex_unlock(&arg_mutex);
	if (!ipc)
		return NULL;	/* oops, something happened... */

	CtdlIPC_chat_recv(ipc, aaa);
	if (aaa[0] != '2') {
		fprintf(stderr, "Citadel refused me: %s\n", &aaa[4]);
		return NULL;	/* server ran out of connections maybe? */
	}

	CtdlIPCIdentifySoftware(ipc, 8, 8, REV_LEVEL, "Citadel stress tester",
		"localhost", aaa);	/* we're lying, the server knows */
	
	r = CtdlIPCQueryUsername(ipc, username, aaa);
	if (r / 100 == 2) {
		/* testuser already exists (from previous run?) */
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
	} else {
		/* testuser doesn't yet exist */
		r = CtdlIPCCreateUser(ipc, username, 1, aaa);
		if (r / 100 != 2) {
			fprintf(stderr, "Citadel refused create user: %s\n", aaa);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;	/* Gawd only knows what went wrong */
		}
		r = CtdlIPCChangePassword(ipc, password, aaa);
		if (r / 100 != 2) {
			fprintf(stderr, "Citadel refused change password: %s\n", aaa);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;	/* Gawd only knows what went wrong */
		}
	}

	/* Wait for the rest of the threads */
	pthread_mutex_lock(&start_mutex);
	pthread_cond_wait(&start_cond, &start_mutex);
	pthread_mutex_unlock(&start_mutex);

	/* And now the fun begins!  Send out a whole shitload of messages */
	start = time(NULL);
	for (c = 0; c < m; c++) {
		int rm;
		char room[7];
		struct ctdlipcroom *rret;
		struct timeval tv;
		long tstart, tend;
		int wait;

		/* Wait for a while */
		pthread_mutex_lock(&rand_mutex);
		/* See Numerical Recipes in C or Knuth vol. 2 ch. 3 */
		/* Randomize between w/3 to w*3 (yes, it's complicated) */
		wait = (int)((1.0+2.7*(float)w)*rand()/(RAND_MAX+(float)w/3.0)); /* range 0-99 */
		pthread_mutex_unlock(&rand_mutex);
		sleep(wait);

		/* Select the room to goto */
		pthread_mutex_lock(&rand_mutex);
		/* See Numerical Recipes in C or Knuth vol. 2 ch. 3 */
		rm = (int)(100.0*rand()/(RAND_MAX+1.0)); /* range 0-99 */
		pthread_mutex_unlock(&rand_mutex);

		/* Goto the selected room */
		sprintf(room, "test%d", rm);
		/* Create the room if not existing. Ignore the return */
		r = CtdlIPCCreateRoom(ipc, 1, room, 0, NULL, 0, aaa);
		if (r / 100 != 2 && r != 574) {	/* Already exists */
			fprintf(stderr, "Citadel refused room create: %s\n", aaa);
			pthread_mutex_lock(&count_mutex);
			total -= m - c;
			pthread_mutex_unlock(&count_mutex);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;
		}
		gettimeofday(&tv, NULL);
		tstart = tv.tv_sec * 1000 + tv.tv_usec / 1000; /* cvt to msec */
		r = CtdlIPCGotoRoom(ipc, room, "", &rret, aaa);
		if (r / 100 != 2) {
			fprintf(stderr, "Citadel refused room change: %s\n", aaa);
			pthread_mutex_lock(&count_mutex);
			total -= m - c;
			pthread_mutex_unlock(&count_mutex);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;
		}

		/* Post the message */
		r = CtdlIPCPostMessage(ipc, 1, NULL, &msg, aaa);
		if (r / 100 != 4) {
			fprintf(stderr, "Citadel refused message entry: %s\n", aaa);
			pthread_mutex_lock(&count_mutex);
			total -= m - c;
			pthread_mutex_unlock(&count_mutex);
			CtdlIPC_delete_ptr(&ipc);
			return NULL;
		}

		/* Do a status update */
		pthread_mutex_lock(&count_mutex);
		count++;
		pthread_mutex_unlock(&count_mutex);
		fprintf(stderr, " %d/%d=%d%%             \r",
			count, total,
			(int)(100 * count / total));
		gettimeofday(&tv, NULL);
		tend = tv.tv_sec * 1000 + tv.tv_usec / 1000; /* cvt to msec */
		tend -= tstart;
		if (tend < tmin) tmin = tend;
		if (tend > tmax) tmax = tend;
		trun += tend;
	}
	end = time(NULL);
	pthread_mutex_lock(&output_mutex);
	fprintf(stderr, "               \r");
	printf("%ld %ld %ld %ld\n", end - start, tmin, trun / c, tmax);
	pthread_mutex_unlock(&output_mutex);
	return (void*)(end - start);
}


/*
 * Shift argument list
 */
int shift(int argc, char **argv, int start, int count)
{
	int i;

	for (i = start; i < argc - count; ++i)
		argv[i] = argv[i + count];
	return argc - count;
}


/*
 * Main loop.  Start a shitload of threads, all of which will attempt to
 * kick a Citadel server square in the nuts.
 */
int main(int argc, char** argv)
{
	void* data[2];		/* pass args to worker thread */
	pthread_t* threads;	/* A shitload of threads */
	pthread_attr_t attr;	/* Thread attributes (we use defaults) */
	int i;			/* Counters */
	long runtime;		/* Run time for each thread */

	/* Read argument list */
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "-n")) {
			n = atoi(argv[i + 1]);
			argc = shift(argc, argv, i, 2);
		}
		if (!strcmp(argv[i], "-w")) {
			w = atoi(argv[i + 1]);
			argc = shift(argc, argv, i, 2);
		}
		if (!strcmp(argv[i], "-m")) {
			m = atoi(argv[i + 1]);
			argc = shift(argc, argv, i, 2);
		}
		if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			fprintf(stderr, "Read stress.c for usage info\n");
			return 1;
		}
	}

	data[0] = (void*)argc;	/* pass args to worker thread */
	data[1] = (void*)argv;	/* pass args to worker thread */

	/* This is how many total messages will be posted */
	total = n * m;

	/* Pick a randomized username */
	pthread_mutex_lock(&rand_mutex);
	/* See Numerical Recipes in C or Knuth vol. 2 ch. 3 */
	i = (int)(100.0*rand()/(RAND_MAX+1.0));	/* range 0-99 */
	pthread_mutex_unlock(&rand_mutex);
	sprintf(username, "testuser%d", i);
	strcpy(password, username);

	/* First, memory for our shitload of threads */
	threads = calloc(n, sizeof(pthread_t));
	if (!threads) {
		perror("Not enough memory");
		return 1;
	}

	/* Then thread attributes (all defaults for now) */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* Then, create some threads */
	fprintf(stderr, "Creating threads      \r");
	for (i = 0; i < n; ++i) {
		pthread_create(&threads[i], &attr, worker, (void*)data);
		
		/* Give thread #0 time to create the user account */
		if (i == 0) sleep(3);
	}

	//fprintf(stderr, "Starting in %d seconds\r", n);
	//sleep(n);
	fprintf(stderr, "                      \r");

	/* Then, signal the conditional they all are waiting on */
	pthread_mutex_lock(&start_mutex);
	pthread_cond_broadcast(&start_cond);
	pthread_mutex_unlock(&start_mutex);

	/* Then wait for them to exit */
	for (i = 0; i < n; i++) {
		pthread_join(threads[i], (void*)&runtime);
		/* We're ignoring this value for now... TODO */
	}
	fprintf(stderr, "\r                                                                               \r");
	return 0;
}
