/*
 * author: David Frascone
 * 
 * eCrash Implementation
 *
 * eCrash will allow you to capture stack traces in the
 * event of a crash, and write those traces to disk, stdout,
 * or any other file handle.
 *
 * modified to integrate closer into citadel by Wilfried Goesgens
 *
 * vim: ts=4
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdep.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <libcitadel.h>
#include "ecrash.h"

#define NIY()	printf("function not implemented yet!\n");
#ifdef HAVE_BACKTRACE
#include <execinfo.h>
static eCrashParameters gbl_params;

static int    gbl_backtraceEntries;
static void **gbl_backtraceBuffer;
static char **gbl_backtraceSymbols;
static int    gbl_backtraceDoneFlag = 0;

static void *stack_frames[50];
static size_t size, NThread;
static char **strings;

/* 
 * Private structures for our thread list
 */
typedef struct thread_list_node{
	char *threadName;
	pthread_t thread;
	int backtraceSignal;
	sighandler_t oldHandler;
	struct thread_list_node *Next;
} ThreadListNode;

static pthread_mutex_t ThreadListMutex = PTHREAD_MUTEX_INITIALIZER;
static ThreadListNode *ThreadList = NULL;

/*********************************************************************
 *********************************************************************
 **     P  R  I  V  A  T  E      F  U  N  C  T  I  O  N  S
 *********************************************************************
 ********************************************************************/


/*!
 * Insert a node into our threadList
 *
 * @param name   Text string indicating our thread
 * @param thread Our Thread Id
 * @param signo  Signal to create backtrace with
 * @param old_handler Our old handler for signo
 *
 * @returns zero on success
 */
static int addThreadToList(char *name, pthread_t thread,int signo,
				           sighandler_t old_handler)
{
	ThreadListNode *node;

	node = malloc(sizeof(ThreadListNode));
	if (!node) return -1;

	DPRINTF(ECRASH_DEBUG_VERBOSE,
					"Adding thread 0x%08x (%s)\n", (unsigned int)thread, name);
	node->threadName = strdup(name);
	node->thread = thread;
	node->backtraceSignal = signo;
	node->oldHandler = old_handler;

	/* And, add it to the list */
	pthread_mutex_lock(&ThreadListMutex);
	node->Next = ThreadList;
	ThreadList = node;
	pthread_mutex_unlock(&ThreadListMutex);
	
	return 0;

} // addThreadToList

/*!
 * Remove a node from our threadList
 *
 * @param thread Our Thread Id
 *
 * @returns zero on success
 */
static int removeThreadFromList(pthread_t thread)
{
	ThreadListNode *Probe, *Prev=NULL;
	ThreadListNode *Removed = NULL;

	DPRINTF(ECRASH_DEBUG_VERBOSE,
					"Removing thread 0x%08x from list . . .\n", (unsigned int)thread);
	pthread_mutex_lock(&ThreadListMutex);
	for (Probe=ThreadList;Probe != NULL; Probe = Probe->Next) {
		if (Probe->thread == thread) {
			// We found it!  Unlink it and move on!
			Removed = Probe;
			if (Prev == NULL) { // head of list
				ThreadList = Probe->Next;
			} else {
				// Prev != null, so we need to link around ourselves.
				Prev->Next = Probe->Next;
			}
			Removed->Next = NULL;
			break;
		}

		Prev = Probe;
	}
	pthread_mutex_unlock(&ThreadListMutex);

	// Now, if something is in Removed, free it, and return success
	if (Removed) {
	    DPRINTF(ECRASH_DEBUG_VERBOSE,
						"   Found %s -- removing\n", Removed->threadName);
		// Reset the signal handler
		signal(Removed->backtraceSignal, Removed->oldHandler);

		// And free the allocated memory
		free (Removed->threadName);
		free (Removed);

		return 0;
	} else {
	    DPRINTF(ECRASH_DEBUG_VERBOSE,
						"   Not Found\n");
		return -1; // Not Found
	}
} // removeThreadFromList

/*!
 * Print out a line of output to all our destinations
 *
 * One by one, output a line of text to all of our output destinations.
 *
 * Return failure if we fail to output to any of them.
 *
 * @param format   Normal printf style vararg format
 *
 * @returns nothing// bytes written, or error on failure.
 */
static void outputPrintf(char *format, ...)
{
	va_list ap;

	va_start(ap, format);

	vsyslog(LOG_CRIT|LOG_NDELAY|LOG_MAIL, format, ap);
} // outputPrintf



/*!
 * Dump our backtrace into a global location
 *
 * This function will dump out our backtrace into our
 * global holding area.
 *
 */
static void createGlobalBacktrace( void )
{

	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	for (NThread = 0; NThread < size; NThread++) 
	{
		syslog(LOG_CRIT|LOG_NDELAY|LOG_MAIL, "RAW: %p  ", stack_frames[NThread]);
	}
	strings = backtrace_symbols(stack_frames, size);
	for (NThread = 0; NThread < size; NThread++) {
		if (strings != NULL) {
			syslog(LOG_CRIT|LOG_NDELAY|LOG_MAIL, "RAW: %p  ", strings[NThread]);
		}
	}
} /* createGlobalBacktrace */
static void outputRawtrace( void )
{

	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	for (NThread = 0; NThread < size; NThread++) 
	{
		syslog(LOG_CRIT|LOG_NDELAY|LOG_MAIL, "RAW: %p  ", stack_frames[NThread]);
	}
} /* createGlobalBacktrace */

/*!
 * Print out (to all the fds, etc), or global backtrace
 */
static void outputGlobalBacktrace ( void )
{
	int i;

	for (i=0; i < gbl_backtraceEntries; i++) {
		if (gbl_backtraceSymbols != FALSE) {
	        	outputPrintf("*      Frame %02x: %s\n",
				     i, gbl_backtraceSymbols[i]);
		} else {
			outputPrintf("*      Frame %02x: %p\n", i,
				     gbl_backtraceBuffer[i]);
		}
	}
} // outputGlobalBacktrace

/*!
 * Output our current stack's backtrace
 */
static void outputBacktrace( void )
{
	createGlobalBacktrace();
	outputGlobalBacktrace();
} /* outputBacktrace */

static void outputBacktraceThreads( void )
{
	ThreadListNode *probe;
	int i;

	// When we're backtracing, don't worry about the mutex . . hopefully
	// we're in a safe place.

	for (probe=ThreadList; probe; probe=probe->Next) {
		gbl_backtraceDoneFlag = 0;
		pthread_kill(probe->thread, probe->backtraceSignal);
		for (i=0; i < gbl_params.threadWaitTime; i++) {
			if (gbl_backtraceDoneFlag)
				break;
			sleep(1);
		}
		if (gbl_backtraceDoneFlag) {
			outputPrintf("*  Backtrace of \"%s\" (0x%08x)\n", 
						 probe->threadName, (unsigned int)probe->thread);
			outputGlobalBacktrace();
		} else {
			outputPrintf("*  Error: unable to get backtrace of \"%s\" (0x%08x)\n", 
						 probe->threadName, (unsigned int)probe->thread);
		}
		outputPrintf("*\n");
	}
} // outputBacktraceThreads


/*!
 * Handle signals (crash signals)
 *
 * This function will catch all crash signals, and will output the
 * crash dump.  
 *
 * It will physically write (and sync) the current thread's information
 * before it attempts to send signals to other threads.
 * 
 * @param signum Signal received.
 */
static void crash_handler(int signo)
{
	outputRawtrace();
	outputPrintf("*********************************************************\n");
	outputPrintf("*               eCrash Crash Handler\n");
	outputPrintf("*********************************************************\n");
	outputPrintf("*\n");
	outputPrintf("*  Got a crash! signo=%d\n", signo);
	outputPrintf("*\n");
	outputPrintf("*  Offending Thread's Backtrace:\n");
	outputPrintf("*\n");
	outputBacktrace();
	outputPrintf("*\n");

	if (gbl_params.dumpAllThreads != FALSE) {
		outputBacktraceThreads();
	}

	outputPrintf("*\n");
	outputPrintf("*********************************************************\n");
	outputPrintf("*               eCrash Crash Handler\n");
	outputPrintf("*********************************************************\n");

	exit(signo);
} // crash_handler

/*!
 * Handle signals (bt signals)
 *
 * This function shoudl be called to generate a crashdump into our
 * global area.  Once the dump has been completed, this function will
 * return after tickling a global.  Since mutexes are not async
 * signal safe, the main thread, after signaling us to generate our
 * own backtrace, will sleep for a few seconds waiting for us to complete.
 *
 * @param signum Signal received.
 */
static void bt_handler(int signo)
{
	createGlobalBacktrace();
	gbl_backtraceDoneFlag=1;
} // bt_handler

/*!
 * Validate a passed-in symbol table
 *
 * For now, just print it out (if verbose), and make sure it's
 * sorted and none of the pointers are zero.
 */
static int ValidateSymbolTable( void )
{
	int i;
	int rc=0;
	unsigned long lastAddress =0;

	// Get out of here if the table is empty
	if (!gbl_params.symbolTable) return 0;

	// Dump it in verbose mode
	DPRINTF(ECRASH_DEBUG_VERBOSE,
					"Symbol Table Provided with %d symbols\n",
					gbl_params.symbolTable->numSymbols);
	for (i=0; i < gbl_params.symbolTable->numSymbols; i++){
		// Dump it in verbose mode
		DPRINTF(ECRASH_DEBUG_VERBOSE, 
				"%-30s %p\n",
				gbl_params.symbolTable->symbols[i].function,
				gbl_params.symbolTable->symbols[i].address);
		if (lastAddress >
		    (unsigned long)gbl_params.symbolTable->symbols[i].address) {
			DPRINTF(ECRASH_DEBUG_ERROR,
					"Error: symbol table is not sorted (last=%p, current=%p)\n",
					(void *)lastAddress,
					gbl_params.symbolTable->symbols[i].address);
			rc = -1;
		}

	} // for

	return rc;
	
} // ValidateSymbolTable

/*********************************************************************
 *********************************************************************
 **      P  U  B  L  I  C      F  U  N  C  T  I  O  N  S
 *********************************************************************
 ********************************************************************/

/*!
 * Initialize eCrash.
 * 
 * This function must be called before calling any other eCrash
 * functions.  It sets up the global behavior of the system, and
 * registers the calling thread for crash dumps.
 *
 * @param params Our input parameters.  The passed in structure will be copied.
 *
 * @return Zero on success.
 */
int eCrash_Init(eCrashParameters *params)
{
	int sigIndex;
	int ret = 0;
#ifdef DO_SIGNALS_RIGHT
	sigset_t blocked;
	struct sigaction act;
#endif

	DPRINTF(ECRASH_DEBUG_VERY_VERBOSE,"Init Starting params = %p\n", params);

	// Allocate our backtrace area
	gbl_backtraceBuffer = malloc(sizeof(void *) * (params->maxStackDepth+5));

#ifdef DO_SIGNALS_RIGHT
	sigemptyset(&blocked);
	act.sa_sigaction = crash_handler;
	act.sa_mask = blocked;
	act.sa_flags = SA_SIGINFO;
#endif

	if (params != NULL) {
		// Make ourselves a global copy of params.
		gbl_params = *params;
		gbl_params.filename = strdup(params->filename);

		// Set our defaults, if they weren't specified
		if (gbl_params.maxStackDepth == 0 )
			gbl_params.maxStackDepth = ECRASH_DEFAULT_STACK_DEPTH;

		if (gbl_params.defaultBacktraceSignal == 0 )
			gbl_params.defaultBacktraceSignal = ECRASH_DEFAULT_BACKTRACE_SIGNAL;

		if (gbl_params.threadWaitTime == 0 )
			gbl_params.threadWaitTime = ECRASH_DEFAULT_THREAD_WAIT_TIME;

		if (gbl_params.debugLevel == 0 )
			gbl_params.debugLevel = ECRASH_DEBUG_DEFAULT;

		// Copy our symbol table
		if (gbl_params.symbolTable) {
		    DPRINTF(ECRASH_DEBUG_VERBOSE,
							"symbolTable @ %p -- %d symbols\n", gbl_params.symbolTable,
						gbl_params.symbolTable->numSymbols);
			// Make a copy of our symbol table
			gbl_params.symbolTable = malloc(sizeof(eCrashSymbolTable));
			memcpy(gbl_params.symbolTable, params->symbolTable,
				   sizeof(eCrashSymbolTable));

			// Now allocate / copy the actual table.
			gbl_params.symbolTable->symbols = malloc(sizeof(eCrashSymbol) *
							             gbl_params.symbolTable->numSymbols);
			memcpy(gbl_params.symbolTable->symbols,
				   params->symbolTable->symbols,
				   sizeof(eCrashSymbol) * gbl_params.symbolTable->numSymbols);

			ValidateSymbolTable();
		}
	
		// And, finally, register for our signals
		for (sigIndex=0; gbl_params.signals[sigIndex] != 0; sigIndex++) {
			DPRINTF(ECRASH_DEBUG_VERY_VERBOSE,
							"   Catching signal[%d] %d\n", sigIndex,
					gbl_params.signals[sigIndex]);

			// I know there's a better way to catch signals with pthreads.
			// I'll do it later TODO
			signal(gbl_params.signals[sigIndex], crash_handler);
		}
	} else {
		DPRINTF(ECRASH_DEBUG_ERROR, "   Error:  Null Params!\n");
		ret = -1;
	}
	DPRINTF(ECRASH_DEBUG_VERY_VERBOSE, "Init Complete ret=%d\n", ret);
	return ret;
} /* eCrash_Init */

/*!
 * UnInitialize eCrash.
 * 
 * This function may be called to de-activate eCrash, release the
 * signal handlers, and free any memory allocated by eCrash.
 *
 * @return Zero on success.
 */
int eCrash_Uninit( void )
{
	NIY();

	return 0;
} /* eCrash_Uninit */

/*!
 * Register a thread for backtracing on crash.
 * 
 * This function must be called by any thread wanting it's stack
 * dumped in the event of a crash.  The thread my specify what 
 * signal should be used, or the default, SIGUSR1 will be used.
 *
 * @param signo Signal to use to generate dump (default: SIGUSR1)
 *
 * @return Zero on success.
 */
int eCrash_RegisterThread(char *name, int signo)
{
	sighandler_t old_handler;

	// Register for our signal
	if (signo == 0) {
		signo = gbl_params.defaultBacktraceSignal;
	}

	old_handler = signal(signo, bt_handler);
	return addThreadToList(name, pthread_self(), signo, old_handler);

} /* eCrash_RegisterThread */

/*!
 * Un-register a thread for stack dumps.
 * 
 * This function may be called to un-register any previously 
 * registered thread.
 *
 * @return Zero on success.
 */
int eCrash_UnregisterThread( void )
{
	return removeThreadFromList(pthread_self());
} /* eCrash_UnregisterThread */

#endif
