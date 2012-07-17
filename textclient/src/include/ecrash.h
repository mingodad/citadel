/*
 * eCrash.h
 * David Frascone
 * 
 * eCrash types and prototypes.
 *
 * vim: ts=4
 */

#ifndef _ECRASH_H_
#define _ECRASH_H_

#include <stdio.h>
#include <signal.h>

typedef void (*sighandler_t)(int);

typedef long BOOL;

#define MAX_LINE_LEN 256

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#define BOOL int
#endif

#define ECRASH_DEFAULT_STACK_DEPTH 10
#define ECRASH_DEFAULT_BACKTRACE_SIGNAL SIGUSR2
#define ECRASH_DEFAULT_THREAD_WAIT_TIME 10
#define ECRASH_MAX_NUM_SIGNALS 30

/** \struct eCrashSymbol
 *  \brief Function Name / Address pair
 *
 *  This is used in conjunction with eCrashSymbolTable to
 *  provide an alternative to backtrace_symbols.
 */
typedef struct {
	char          *function;
	void          *address;
} eCrashSymbol;
/** \struct eCrashSymbolTable
 *  \brief Symbol table used to avoid backtrace_symbols()
 *
 *  This structure is used to provide a alternative to 
 *  backtrace_symbols which is not async signal safe.
 *
 *  The symbol table should be sorted by address (it will 
 *  be either binary or linearly searched)
 */
typedef struct {
	int           numSymbols;
	eCrashSymbol *symbols;
} eCrashSymbolTable;



#define ECRASH_DEBUG_ENABLE  /* undef to turn off debug */

#ifdef ECRASH_DEBUG_ENABLE
# define ECRASH_DEBUG_VERY_VERBOSE 1
# define ECRASH_DEBUG_VERBOSE      2
# define ECRASH_DEBUG_INFO         3
# define ECRASH_DEBUG_WARN         4
# define ECRASH_DEBUG_ERROR        5
# define ECRASH_DEBUG_OFF          6
# define ECRASH_DEBUG_DEFAULT	  (ECRASH_DEBUG_ERROR)
# define DPRINTF(level, fmt...) \
		if (level >= gbl_params.debugLevel) { printf(fmt); fflush(stdout); }
#else /* ECRASH_DEBUG_ENABLE */
# define DPRINTF(level, fmt...) 
#endif /* ECRASH_DEBUG_ENABLE */


/** \struct eCrashParameters
 *  \brief eCrash Initialization Parameters
 *  
 *  This structure contains all the global initialization functions
 *  for eCrash.
 * 
 *  @see eCrash_Init
 */
typedef struct {


	/*  OUTPUT OPTIONS */
		/** Filename to output to, or NULL */
	char *filename;			
		/** FILE * to output to or NULL */
	FILE *filep;
		/** fd to output to or -1 */
	int	fd;

	int debugLevel;

		/** If true, all registered threads will
		 *   be dumped
         */
	BOOL dumpAllThreads;

		/** How far to backtrace each stack */
	unsigned int maxStackDepth;

		/** Default signal to use to tell a thread to drop it's
		 *  stack.
		 */
	int defaultBacktraceSignal;

		/** How long to wait for a threads
		 *  dump
		 */	
	unsigned int threadWaitTime;

		/** If this is non-zero, the dangerous function, backtrace_symbols
		  * will be used.  That function does a malloc(), so is not async
		  * signal safe, and could cause deadlocks.
		  */
	BOOL useBacktraceSymbols; 

		/** To avoid the issues with backtrace_symbols (see comments above)
		  * the caller can supply it's own symbol table, containing function
		  * names and start addresses.  This table can be created using 
		  * a script, or a static table.
		  *
		  * If this variable is not null, it will be used, instead of 
		  * backtrace_symbols, reguardless of the setting
		  * of useBacktraceSymbols.
		  */
	eCrashSymbolTable *symbolTable;

		/** Array of crash signals to catch,
		 *  ending in 0  I would have left it a [] array, but I
		 *  do a static copy, so I needed a set size.
         */
	int signals[ECRASH_MAX_NUM_SIGNALS];

} eCrashParameters;

/*!
 * Initialize eCrash.
 * 
 * This function must be called before calling any other eCrash
 * functions.  It sets up the global behavior of the system.
 *
 * @param params Our input parameters.  The passed in structure will be copied.
 *
 * @return Zero on success.
 */
int eCrash_Init(eCrashParameters *params);
/*!
 * UnInitialize eCrash.
 * 
 * This function may be called to de-activate eCrash, release the
 * signal handlers, and free any memory allocated by eCrash.
 *
 * @return Zero on success.
 */
int eCrash_Uninit( void );

/*!
 * Register a thread for backtracing on crash.
 * 
 * This function must be called by any thread wanting it's stack
 * dumped in the event of a crash.  The thread my specify what 
 * signal should be used, or the default, SIGUSR1 will be used.
 *
 * @param name String used to refer to us in crash dumps
 * @param signo Signal to use to generate dump (default: SIGUSR1)
 *
 * @return Zero on success.
 */
int eCrash_RegisterThread(char *name, int signo);

/*!
 * Un-register a thread for stack dumps.
 * 
 * This function may be called to un-register any previously 
 * registered thread.
 *
 * @return Zero on success.
 */
int eCrash_UnregisterThread( void );

#endif /* _E_CRASH_H_ */
