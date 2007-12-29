/* $Id:$ */

#ifndef THREADS_H
#define THREADS_H

#include "sysdep.h"

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_DB_H
#include <db.h>
#elif defined(HAVE_DB4_DB_H)
#include <db4/db.h>
#else
#error Neither <db.h> nor <db4/db.h> was found by configure. Install db4-devel.
#endif

#include "server.h"
#include "sysdep_decls.h"

/*
 * Thread stuff
 */

enum CtdlThreadState {
	CTDL_THREAD_INVALID,
	CTDL_THREAD_VALID,
	CTDL_THREAD_CREATE,
	CTDL_THREAD_CANCELLED,
	CTDL_THREAD_EXITED,
	CTDL_THREAD_STOPPING,
	CTDL_THREAD_STOP_REQ,	/* Do NOT put any running states before this state */
	CTDL_THREAD_SLEEPING,
	CTDL_THREAD_BLOCKED,
	CTDL_THREAD_RUNNING,
	CTDL_THREAD_LAST_STATE
};
typedef struct CtdlThreadNode CtdlThreadNode;

struct CtdlThreadNode{
	citthread_t tid;				/* id as returned by citthread_create() */
	pid_t pid;				/* pid, as best the OS will let us determine */
	time_t when;				/* When to start a scheduled thread */
	struct CitContext *Context;		/* The session context that this thread mught be working on or NULL if none */
	long number;				/* A unigue number for this thread (not implimented yet) */
	int wakefd_recv;			/* An fd that this thread can sleep on (not implimented yet) */
	int wakefd_send;			/* An fd that this thread can send out on (Not implimented yet) */
	int signal;				/* A field to store a signal we caught. */
	const char *name;			/* A name for this thread */
	void *(*thread_func) (void *arg);	/* The actual function that does this threads work */
	void *user_args;			/* Arguments passed to this threads work function */
	long flags;				/* Flags that describe this thread */
	enum CtdlThreadState state;		/* Flag to show state of this thread */
	citthread_mutex_t ThreadMutex;		/* A mutex to sync this thread to others if this thread allows (also used for sleeping) */
	citthread_cond_t ThreadCond;		/* A condition variable to sync this thread with others */
	citthread_mutex_t SleepMutex;		/* A mutex for sleeping */
	citthread_cond_t SleepCond;		/* A condition variable for sleeping */
	citthread_attr_t attr;			/* Attributes of this thread */
	struct timeval start_time;		/* Time this thread was started */
	struct timeval last_state_change;	/* Time when this thread last changed state */
	double avg_sleeping;			/* Average sleeping time */
	double avg_running;			/* Average running time */
	double avg_blocked;			/* Average blocked time */
	double load_avg;			/* Load average for this thread */
	CtdlThreadNode *prev;		/* Previous thread in the thread table */
	CtdlThreadNode *next;		/* Next thread in the thread table */
} ;
 
extern CtdlThreadNode *CtdlThreadList;

typedef struct ThreadTSD ThreadTSD;

struct ThreadTSD {
	DB_TXN *tid;		/* Transaction handle */
	DBC *cursors[MAXCDB];	/* Cursors, for traversals... */
	CtdlThreadNode *self;	/* Pointer to this threads control structure */
} ;

extern double CtdlThreadLoadAvg;
extern double CtdlThreadWorkerAvg;
extern citthread_key_t ThreadKey;

void ctdl_thread_internal_init_tsd(void);
void ctdl_internal_thread_gc (void);
void ctdl_thread_internal_init(void);
void ctdl_thread_internal_cleanup(void);
void ctdl_thread_internal_calc_loadavg(void);
void ctdl_thread_internal_free_tsd(void);
CtdlThreadNode *ctdl_internal_create_thread(char *name, long flags, void *(*thread_func) (void *arg), void *args);
void ctdl_thread_internal_check_scheduled(void);

void InitialiseSemaphores(void);
int try_critical_section (int which_one);
void begin_critical_section (int which_one);
void end_critical_section (int which_one);
void go_threading(void);

#endif // THREADS_H
