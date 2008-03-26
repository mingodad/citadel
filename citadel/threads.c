/*
 * $Id: sysdep.c 5882 2007-12-13 19:46:05Z davew $
 *
 * Citadel "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * Here's where we have the Citadel thread implimentation
 *
 */

#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>

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

#include "threads.h"
#include "ctdl_module.h"
#include "modules_init.h"
#include "housekeeping.h"
#include "config.h"
#include "citserver.h"
#include "sysdep_decls.h"

/*
 * define this to use the new worker_thread method of handling connections
 */
//#define NEW_WORKER

/*
 * New thread interface.
 * To create a thread you must call one of the create thread functions.
 * You must pass it the address of (a pointer to a CtdlThreadNode initialised to NULL) like this
 * struct CtdlThreadNode *node = NULL;
 * pass in &node
 * If the thread is created *node will point to the thread control structure for the created thread.
 * If the thread creation fails *node remains NULL
 * Do not free the memory pointed to by *node, it doesn't belong to you.
 * This new interface duplicates much of the eCrash stuff. We should go for closer integration since that would
 * remove the need for the calls to eCrashRegisterThread and friends
 */

static int num_threads = 0;			/* Current number of threads */
static int num_workers = 0;			/* Current number of worker threads */

CtdlThreadNode *CtdlThreadList = NULL;
CtdlThreadNode *CtdlThreadSchedList = NULL;

static CtdlThreadNode *GC_thread = NULL;
static char *CtdlThreadStates[CTDL_THREAD_LAST_STATE];
double CtdlThreadLoadAvg = 0;
double CtdlThreadWorkerAvg = 0;
citthread_key_t ThreadKey;

citthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */



void InitialiseSemaphores(void)
{
	int i;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (i=0; i<MAX_SEMAPHORES; ++i) {
		citthread_mutex_init(&Critters[i], NULL);
	}
}




/*
 * Obtain a semaphore lock to begin a critical section.
 * but only if no one else has one
 */
int try_critical_section(int which_one)
{
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
	return (citthread_mutex_trylock(&Critters[which_one]));
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
	citthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	citthread_mutex_unlock(&Critters[which_one]);
}


/*
 * A function to destroy the TSD
 */
static void ctdl_thread_internal_dest_tsd(void *arg)
{
	if (arg != NULL) {
		check_handles(arg);
		free(arg);
	}
}


/*
 * A function to initialise the thread TSD
 */
void ctdl_thread_internal_init_tsd(void)
{
	int ret;
	
	if ((ret = citthread_key_create(&ThreadKey, ctdl_thread_internal_dest_tsd))) {
		CtdlLogPrintf(CTDL_EMERG, "citthread_key_create: %s\n", strerror(ret));
		exit(CTDLEXIT_DB);
	}
}

/*
 * Ensure that we have a key for thread-specific data. 
 *
 * This should be called immediately after startup by any thread 
 * 
 */
void CtdlThreadAllocTSD(void)
{
	ThreadTSD *tsd;

	if (citthread_getspecific(ThreadKey) != NULL)
		return;

	tsd = malloc(sizeof(ThreadTSD));

	tsd->tid = NULL;

	memset(tsd->cursors, 0, sizeof tsd->cursors);
	tsd->self = NULL;
	
	citthread_setspecific(ThreadKey, tsd);
}


void ctdl_thread_internal_free_tsd(void)
{
	ctdl_thread_internal_dest_tsd(citthread_getspecific(ThreadKey));
	citthread_setspecific(ThreadKey, NULL);
}


void ctdl_thread_internal_cleanup(void)
{
	int i;
	CtdlThreadNode *this_thread, *that_thread;
	
	for (i=0; i<CTDL_THREAD_LAST_STATE; i++)
	{
		free (CtdlThreadStates[i]);
	}
	
	/* Clean up the scheduled thread list */
	this_thread = CtdlThreadSchedList;
	while (this_thread)
	{
		that_thread = this_thread;
		this_thread = this_thread->next;
		citthread_mutex_destroy(&that_thread->ThreadMutex);
		citthread_cond_destroy(&that_thread->ThreadCond);
		citthread_mutex_destroy(&that_thread->SleepMutex);
		citthread_cond_destroy(&that_thread->SleepCond);
		citthread_attr_destroy(&that_thread->attr);
		free(that_thread);
	}
	ctdl_thread_internal_free_tsd();
}

void ctdl_thread_internal_init(void)
{
	CtdlThreadNode *this_thread;
	int ret = 0;
	
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
	this_thread = malloc(sizeof(CtdlThreadNode));
	if (this_thread == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't allocate CtdlThreadNode, exiting\n");
		return;
	}
	// Ensuring this is zero'd means we make sure the thread doesn't start doing its thing until we are ready.
	memset (this_thread, 0, sizeof(CtdlThreadNode));
	
	citthread_mutex_init (&(this_thread->ThreadMutex), NULL);
	citthread_cond_init (&(this_thread->ThreadCond), NULL);
	citthread_mutex_init (&(this_thread->SleepMutex), NULL);
	citthread_cond_init (&(this_thread->SleepCond), NULL);
	
	/* We are garbage collector so create us as running */
	this_thread->state = CTDL_THREAD_RUNNING;
	
	if ((ret = citthread_attr_init(&this_thread->attr))) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, citthread_attr_init: %s\n", strerror(ret));
		free(this_thread);
		return;
	}

	this_thread->name = "Garbage Collection Thread";
	
	this_thread->tid = citthread_self();
	GC_thread = this_thread;
	CT = this_thread;
	
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
 void ctdl_thread_internal_update_avgs(CtdlThreadNode *this_thread)
 {
	struct timeval now, result;
	double last_duration;

	gettimeofday(&now, NULL);
	timersub(&now, &(this_thread->last_state_change), &result);
	/* I don't think these mutex's are needed here */
	citthread_mutex_lock(&this_thread->ThreadMutex);
	// result now has a timeval for the time we spent in the last state since we last updated
	last_duration = (double)result.tv_sec + ((double)result.tv_usec / (double) 1000000);
	if (this_thread->state == CTDL_THREAD_SLEEPING)
		this_thread->avg_sleeping += last_duration;
	if (this_thread->state == CTDL_THREAD_RUNNING)
		this_thread->avg_running += last_duration;
	if (this_thread->state == CTDL_THREAD_BLOCKED)
		this_thread->avg_blocked += last_duration;
	memcpy (&this_thread->last_state_change, &now, sizeof (struct timeval));
	citthread_mutex_unlock(&this_thread->ThreadMutex);
}

/*
 * A function to chenge the state of a thread
 */
void ctdl_thread_internal_change_state (CtdlThreadNode *this_thread, enum CtdlThreadState new_state)
{
	/*
	 * Wether we change state or not we need update the load values
	 */
	ctdl_thread_internal_update_avgs(this_thread);
	/* This mutex not needed here? */
	citthread_mutex_lock(&this_thread->ThreadMutex); /* To prevent race condition of a sleeping thread */
	if ((new_state == CTDL_THREAD_STOP_REQ) && (this_thread->state > CTDL_THREAD_STOP_REQ))
		this_thread->state = new_state;
	if (((new_state == CTDL_THREAD_SLEEPING) || (new_state == CTDL_THREAD_BLOCKED)) && (this_thread->state == CTDL_THREAD_RUNNING))
		this_thread->state = new_state;
	if ((new_state == CTDL_THREAD_RUNNING) && ((this_thread->state == CTDL_THREAD_SLEEPING) || (this_thread->state == CTDL_THREAD_BLOCKED)))
		this_thread->state = new_state;
	citthread_mutex_unlock(&this_thread->ThreadMutex);
}


/*
 * A function to tell all threads to exit
 */
void CtdlThreadStopAll(void)
{
	//FIXME: The signalling of the condition should not be in the critical_section
	// We need to build a list of threads we are going to signal and then signal them afterwards
	
	CtdlThreadNode *this_thread;
	
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
#ifdef THREADS_USESIGNALS
		citthread_killl(this_thread->tid, SIGHUP);
#endif
		ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_STOP_REQ);
		citthread_cond_signal(&this_thread->ThreadCond);
		citthread_cond_signal(&this_thread->SleepCond);
		CtdlLogPrintf(CTDL_DEBUG, "Thread system stopping thread \"%s\" (0x%08lx).\n",
			this_thread->name, this_thread->tid);
		this_thread = this_thread->next;
	}
	end_critical_section(S_THREAD_LIST);
}


/*
 * A function to wake up all sleeping threads
 */
void CtdlThreadWakeAll(void)
{
	CtdlThreadNode *this_thread;
	
	CtdlLogPrintf(CTDL_DEBUG, "Thread system waking all threads.\n");
	
	begin_critical_section(S_THREAD_LIST);
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		if (!this_thread->thread_func)
		{
			citthread_cond_signal(&this_thread->ThreadCond);
			citthread_cond_signal(&this_thread->SleepCond);
		}
		this_thread = this_thread->next;
	}
	end_critical_section(S_THREAD_LIST);
}


/*
 * A function to return the number of threads running in the system
 */
int CtdlThreadGetCount(void)
{
	return  num_threads;
}

int CtdlThreadGetWorkers(void)
{
	return  num_workers;
}

double CtdlThreadGetWorkerAvg(void)
{
	double ret;
	
	begin_critical_section(S_THREAD_LIST);
	ret =  CtdlThreadWorkerAvg;
	end_critical_section(S_THREAD_LIST);
	return ret;
}

double CtdlThreadGetLoadAvg(void)
{
	double ret;
	
	begin_critical_section(S_THREAD_LIST);
	ret =  CtdlThreadLoadAvg;
	end_critical_section(S_THREAD_LIST);
	return ret;
}




/*
 * A function to rename a thread
 * Returns a const char *
 */
const char *CtdlThreadName(const char *name)
{
	const char *old_name;
	
	if (!CT)
	{
		CtdlLogPrintf(CTDL_WARNING, "Thread system WARNING. Attempt to CtdlThreadRename() a non thread. %s\n", name);
		return NULL;
	}
	old_name = CT->name;
	if (name)
		CT->name = name;
	return (old_name);
}	


/*
 * A function to force a thread to exit
 */
void CtdlThreadCancel(CtdlThreadNode *thread)
{
	CtdlThreadNode *this_thread;
	
	if (!thread)
		this_thread = CT;
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
	
	ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_CANCELLED);
	citthread_cancel(this_thread->tid);
}


/*
 * A function for a thread to check if it has been asked to stop
 */
int CtdlThreadCheckStop(void)
{
	int state;
	
	if (!CT)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, CtdlThreadCheckStop() called by a non thread.\n");
		CtdlThreadStopAll();
		return -1;
	}
	
	state = CT->state;

#ifdef THREADS_USESIGNALS
	if (CT->signal)
		CtdlLogPrintf(CTDL_DEBUG, "Thread \"%s\" caught signal %d.\n", CT->name, CT->signal);
#endif
	if(state == CTDL_THREAD_STOP_REQ)
	{
		CT->state = CTDL_THREAD_STOPPING;
		return -1;
	}
	else if((state < CTDL_THREAD_STOP_REQ) && (state > CTDL_THREAD_CREATE))
	{
		return -1;
	}
	return 0;
}


/*
 * A function to ask a thread to exit
 * The thread must call CtdlThreadCheckStop() periodically to determine if it should exit
 */
void CtdlThreadStop(CtdlThreadNode *thread)
{
	CtdlThreadNode *this_thread;
	
	if (!thread)
		this_thread = CT;
	else
		this_thread = thread;
	if (!this_thread)
		return;
	if (!(this_thread->thread_func))
		return; 	// Don't stop garbage collector
#ifdef THREADS_USESIGNALS
	citthread_kill(this_thread->tid, SIGHUP);	
#endif
	ctdl_thread_internal_change_state (this_thread, CTDL_THREAD_STOP_REQ);
	citthread_cond_signal(&this_thread->ThreadCond);
	citthread_cond_signal(&this_thread->SleepCond);
}

/*
 * So we now have a sleep command that works with threads but it is in seconds
 */
void CtdlThreadSleep(int secs)
{
	struct timespec wake_time;
	struct timeval time_now;
	
	
	if (!CT)
	{
		CtdlLogPrintf(CTDL_WARNING, "CtdlThreadSleep() called by something that is not a thread. Should we die?\n");
		return;
	}
	
	memset (&wake_time, 0, sizeof(struct timespec));
	gettimeofday(&time_now, NULL);
	wake_time.tv_sec = time_now.tv_sec + secs;
	wake_time.tv_nsec = time_now.tv_usec * 10;

	ctdl_thread_internal_change_state (CT, CTDL_THREAD_SLEEPING);
	
	citthread_mutex_lock(&CT->ThreadMutex); /* Prevent something asking us to awaken before we've gone to sleep */
	citthread_cond_timedwait(&CT->SleepCond, &CT->ThreadMutex, &wake_time);
	citthread_mutex_unlock(&CT->ThreadMutex);
	
	ctdl_thread_internal_change_state (CT, CTDL_THREAD_RUNNING);
}


/*
 * Routine to clean up our thread function on exit
 */
static void ctdl_internal_thread_cleanup(void *arg)
{
	/*
	 * In here we were called by the current thread because it is exiting
	 * NB. WE ARE THE CURRENT THREAD
	 */
	CtdlLogPrintf(CTDL_NOTICE, "Thread \"%s\" (0x%08lx) exited.\n", CT->name, CT->tid);
	
	#ifdef HAVE_BACKTRACE
	eCrash_UnregisterThread();
	#endif
	
	citthread_mutex_lock(&CT->ThreadMutex);
	CT->state = CTDL_THREAD_EXITED;	// needs to be last thing else house keeping will unlink us too early
	citthread_mutex_unlock(&CT->ThreadMutex);
}

/*
 * A quick function to show the load averages
 */
void ctdl_thread_internal_calc_loadavg(void)
{
	CtdlThreadNode *that_thread;
	double load_avg, worker_avg;
	int workers = 0;

	that_thread = CtdlThreadList;
	load_avg = 0;
	worker_avg = 0;
	while(that_thread)
	{
		/* Update load averages */
		ctdl_thread_internal_update_avgs(that_thread);
		citthread_mutex_lock(&that_thread->ThreadMutex);
		that_thread->load_avg = (that_thread->avg_sleeping + that_thread->avg_running) / (that_thread->avg_sleeping + that_thread->avg_running + that_thread->avg_blocked) * 100;
		that_thread->avg_sleeping /= 2;
		that_thread->avg_running /= 2;
		that_thread->avg_blocked /= 2;
		load_avg += that_thread->load_avg;
		if (that_thread->flags & CTDLTHREAD_WORKER)
		{
			worker_avg += that_thread->load_avg;
			workers++;
		}
#ifdef WITH_THREADLOG
		CtdlLogPrintf(CTDL_DEBUG, "CtdlThread, \"%s\" (%lu) \"%s\" %.2f %.2f %.2f %.2f\n",
			that_thread->name,
			that_thread->tid,
			CtdlThreadStates[that_thread->state],
			that_thread->avg_sleeping,
			that_thread->avg_running,
			that_thread->avg_blocked,
			that_thread->load_avg);
#endif
		citthread_mutex_unlock(&that_thread->ThreadMutex);
		that_thread = that_thread->next;
	}
	CtdlThreadLoadAvg = load_avg/num_threads;
	CtdlThreadWorkerAvg = worker_avg/workers;
#ifdef WITH_THREADLOG
	CtdlLogPrintf(CTDL_INFO, "System load average %.2f, workers averag %.2f, threads %d, workers %d, sessions %d\n", CtdlThreadLoadAvg, CtdlThreadWorkerAvg, num_threads, num_workers, num_sessions);
#endif
}


/*
 * Garbage collection routine.
 * Gets called by main() in a loop to clean up the thread list periodically.
 */
void CtdlThreadGC (void)
{
	CtdlThreadNode *this_thread, *that_thread;
	int workers = 0, sys_workers;
	int ret=0;
	
	begin_critical_section(S_THREAD_LIST);
	
	/* Handle exiting of garbage collector thread */
	if(num_threads == 1)
		CtdlThreadList->state = CTDL_THREAD_EXITED;
	
#ifdef WITH_THREADLOG
	CtdlLogPrintf(CTDL_DEBUG, "Thread system running garbage collection.\n");
#endif
	/*
	 * Woke up to do garbage collection
	 */
	this_thread = CtdlThreadList;
	while(this_thread)
	{
		that_thread = this_thread;
		this_thread = this_thread->next;
		
		if ((that_thread->state == CTDL_THREAD_STOP_REQ || that_thread->state == CTDL_THREAD_STOPPING)
			&& (!citthread_equal(that_thread->tid, citthread_self())))
				that_thread->stop_ticker++;
		else
		{
			/**
			 * Catch the situation where a worker was asked to stop but couldn't and we are not
			 * shutting down.
			 */
			that_thread->stop_ticker = 0;
		}
		
		if (that_thread->stop_ticker == 5)
		{
			CtdlLogPrintf(CTDL_DEBUG, "Thread System: The thread \"%s\" (0x%08lx) failed to self terminate within 5 ticks. It would be cancelled now.\n", that_thread->name, that_thread->tid);
			if ((that_thread->flags & CTDLTHREAD_WORKER) == 0)
				CtdlLogPrintf(CTDL_INFO, "Thread System: A non worker thread would have been canceled this may cause message loss.\n");
//			that_thread->state = CTDL_THREAD_CANCELLED;
			that_thread->stop_ticker++;
//			citthread_cancel(that_thread->tid);
//			continue;
		}
		
		/* Do we need to clean up this thread? */
		if ((that_thread->state != CTDL_THREAD_EXITED) && (that_thread->state != CTDL_THREAD_CANCELLED))
		{
			if(that_thread->flags & CTDLTHREAD_WORKER)
				workers++;	/* Sanity check on number of worker threads */
			continue;
		}
		
		if (citthread_equal(that_thread->tid, citthread_self()) && that_thread->thread_func)
		{	/* Sanity check */
			end_critical_section(S_THREAD_LIST);
			CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, a thread is trying to clean up after itself.\n");
			abort();
			return;
		}
		
		if (num_threads <= 0)
		{	/* Sanity check */
			end_critical_section(S_THREAD_LIST);
			CtdlLogPrintf(CTDL_EMERG, "Thread system PANIC, num_threads <= 0 and trying to do Garbage Collection.\n");
			abort();
			return;
		}

		if(that_thread->flags & CTDLTHREAD_WORKER)
			num_workers--;	/* This is a wroker thread so reduce the count. */
		num_threads--;
		/* If we are unlinking the list head then the next becomes the list head */
		if(that_thread->prev)
			that_thread->prev->next = that_thread->next;
		else
			CtdlThreadList = that_thread->next;
		if(that_thread->next)
			that_thread->next->prev = that_thread->prev;
		
		citthread_cond_signal(&that_thread->ThreadCond);
		citthread_cond_signal(&that_thread->SleepCond);	// Make sure this thread is awake
		citthread_mutex_lock(&that_thread->ThreadMutex);	// Make sure it has done what its doing
		citthread_mutex_unlock(&that_thread->ThreadMutex);
		/*
		 * Join on the thread to do clean up and prevent memory leaks
		 * Also makes sure the thread has cleaned up after itself before we remove it from the list
		 * We can join on the garbage collector thread the join should just return EDEADLCK
		 */
		ret = citthread_join (that_thread->tid, NULL);
		if (ret == EDEADLK)
			CtdlLogPrintf(CTDL_DEBUG, "Garbage collection on own thread.\n");
		else if (ret == EINVAL)
			CtdlLogPrintf(CTDL_DEBUG, "Garbage collection, that thread already joined on.\n");
		else if (ret == ESRCH)
			CtdlLogPrintf(CTDL_DEBUG, "Garbage collection, no thread to join on.\n");
		else if (ret != 0)
			CtdlLogPrintf(CTDL_DEBUG, "Garbage collection, citthread_join returned an unknown error(%d).\n", ret);
		/*
		 * Now we own that thread entry
		 */
		CtdlLogPrintf(CTDL_INFO, "Garbage Collection for thread \"%s\" (0x%08lx).\n",
			that_thread->name, that_thread->tid);
		citthread_mutex_destroy(&that_thread->ThreadMutex);
		citthread_cond_destroy(&that_thread->ThreadCond);
		citthread_mutex_destroy(&that_thread->SleepMutex);
		citthread_cond_destroy(&that_thread->SleepCond);
		citthread_attr_destroy(&that_thread->attr);
		free(that_thread);
	}
	sys_workers = num_workers;
	end_critical_section(S_THREAD_LIST);
	
	/* Sanity check number of worker threads */
	if (workers != sys_workers)
	{
		CtdlLogPrintf(CTDL_EMERG,
			"Thread system PANIC, discrepancy in number of worker threads. Counted %d, should be %d.\n",
			workers, sys_workers
			);
		abort();
	}
}



 
/*
 * Runtime function for a Citadel Thread.
 * This initialises the threads environment and then calls the user supplied thread function
 * Note that this is the REAL thread function and wraps the users thread function.
 */ 
static void *ctdl_internal_thread_func (void *arg)
{
	CtdlThreadNode *this_thread;
	void *ret = NULL;

	/* lock and unlock the thread list.
	 * This causes this thread to wait until all its creation stuff has finished before it
	 * can continue its execution.
	 */
	begin_critical_section(S_THREAD_LIST);
	this_thread = (CtdlThreadNode *) arg;
	gettimeofday(&this_thread->start_time, NULL);		/* Time this thread started */
//	citthread_mutex_lock(&this_thread->ThreadMutex);
	
	// Register the cleanup function to take care of when we exit.
	citthread_cleanup_push(ctdl_internal_thread_cleanup, NULL);
	// Get our thread data structure
	CtdlThreadAllocTSD();
	CT = this_thread;
	this_thread->pid = getpid();
	memcpy(&this_thread->last_state_change, &this_thread->start_time, sizeof (struct timeval));	/* Changed state so mark it. */
	/* Only change to running state if we weren't asked to stop during the create cycle
	 * Other wise there is a window to allow this threads creation to continue to full grown and
	 * therby prevent a shutdown of the server.
	 */
//	citthread_mutex_unlock(&this_thread->ThreadMutex);
		
	if (!CtdlThreadCheckStop())
	{
		citthread_mutex_lock(&this_thread->ThreadMutex);
		this_thread->state = CTDL_THREAD_RUNNING;
		citthread_mutex_unlock(&this_thread->ThreadMutex);
	}
	end_critical_section(S_THREAD_LIST);
	
	// Register for tracing
	#ifdef HAVE_BACKTRACE
	eCrash_RegisterThread(this_thread->name, 0);
	#endif
	
	// Tell the world we are here
	CtdlLogPrintf(CTDL_NOTICE, "Created a new thread \"%s\" (0x%08lx).\n",
		this_thread->name, this_thread->tid);
	
	/*
	 * run the thread to do the work but only if we haven't been asked to stop
	 */
	if (!CtdlThreadCheckStop())
		ret = (this_thread->thread_func)(this_thread->user_args);
	
	/*
	 * Our thread is exiting either because it wanted to end or because the server is stopping
	 * We need to clean up
	 */
	citthread_cleanup_pop(1);	// Execute our cleanup routine and remove it
	
	return(ret);
}




/*
 * Function to initialise an empty thread structure
 */
CtdlThreadNode *ctdl_internal_init_thread_struct(CtdlThreadNode *this_thread, long flags)
{
	int ret = 0;
	
	// Ensuring this is zero'd means we make sure the thread doesn't start doing its thing until we are ready.
	memset (this_thread, 0, sizeof(CtdlThreadNode));
	
	/* Create the mutex's early so we can use them */
	citthread_mutex_init (&(this_thread->ThreadMutex), NULL);
	citthread_cond_init (&(this_thread->ThreadCond), NULL);
	citthread_mutex_init (&(this_thread->SleepMutex), NULL);
	citthread_cond_init (&(this_thread->SleepCond), NULL);
	
	this_thread->state = CTDL_THREAD_CREATE;
	
	if ((ret = citthread_attr_init(&this_thread->attr))) {
		citthread_mutex_unlock(&this_thread->ThreadMutex);
		citthread_mutex_destroy(&(this_thread->ThreadMutex));
		citthread_cond_destroy(&(this_thread->ThreadCond));
		citthread_mutex_destroy(&(this_thread->SleepMutex));
		citthread_cond_destroy(&(this_thread->SleepCond));
		CtdlLogPrintf(CTDL_EMERG, "Thread system, citthread_attr_init: %s\n", strerror(ret));
		free(this_thread);
		return NULL;
	}

	/* Our per-thread stacks need to be bigger than the default size,
	 * otherwise the MIME parser crashes on FreeBSD, and the IMAP service
	 * crashes on 64-bit Linux.
	 */
	if (flags & CTDLTHREAD_BIGSTACK)
	{
#ifdef WITH_THREADLOG
		CtdlLogPrintf(CTDL_INFO, "Thread system. Creating BIG STACK thread.\n");
#endif
		if ((ret = citthread_attr_setstacksize(&this_thread->attr, THREADSTACKSIZE))) {
			citthread_mutex_unlock(&this_thread->ThreadMutex);
			citthread_mutex_destroy(&(this_thread->ThreadMutex));
			citthread_cond_destroy(&(this_thread->ThreadCond));
			citthread_mutex_destroy(&(this_thread->SleepMutex));
			citthread_cond_destroy(&(this_thread->SleepCond));
			citthread_attr_destroy(&this_thread->attr);
			CtdlLogPrintf(CTDL_EMERG, "Thread system, citthread_attr_setstacksize: %s\n",
				strerror(ret));
			free(this_thread);
			return NULL;
		}
	}

	/* Set this new thread with an avg_blocked of 2. We do this so that its creation affects the
	 * load average for the system. If we don't do this then we create a mass of threads at the same time 
	 * because the creation didn't affect the load average.
	 */
	this_thread->avg_blocked = 2;
	
	return (this_thread);
}



 
/*
 * Internal function to create a thread.
 */ 
CtdlThreadNode *ctdl_internal_create_thread(char *name, long flags, void *(*thread_func) (void *arg), void *args)
{
	int ret = 0;
	CtdlThreadNode *this_thread;

	if (num_threads >= 32767)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system. Thread list full.\n");
		return NULL;
	}
		
	this_thread = malloc(sizeof(CtdlThreadNode));
	if (this_thread == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't allocate CtdlThreadNode, exiting\n");
		return NULL;
	}
	
	/* Initialise the thread structure */
	if (ctdl_internal_init_thread_struct(this_thread, flags) == NULL)
	{
		free(this_thread);
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't initialise CtdlThreadNode, exiting\n");
		return NULL;
	}
	/*
	 * If we got here we are going to create the thread so we must initilise the structure
	 * first because most implimentations of threading can't create it in a stopped state
	 * and it might want to do things with its structure that aren't initialised otherwise.
	 */
	if(name)
	{
		this_thread->name = name;
	}
	else
	{
		this_thread->name = "Un-named Thread";
	}
	
	this_thread->flags = flags;
	this_thread->thread_func = thread_func;
	this_thread->user_args = args;
	
//	citthread_mutex_lock(&this_thread->ThreadMutex);
	
	begin_critical_section(S_THREAD_LIST);
	/*
	 * We pass this_thread into the thread as its args so that it can find out information
	 * about itself and it has a bit of storage space for itself, not to mention that the REAL
	 * thread function needs to finish off the setup of the structure
	 */
	if ((ret = citthread_create(&this_thread->tid, &this_thread->attr, ctdl_internal_thread_func, this_thread) != 0))
	{
		end_critical_section(S_THREAD_LIST);
		CtdlLogPrintf(CTDL_ALERT, "Thread system, Can't create thread: %s\n",
			strerror(ret));
		citthread_mutex_unlock(&this_thread->ThreadMutex);
		citthread_mutex_destroy(&(this_thread->ThreadMutex));
		citthread_cond_destroy(&(this_thread->ThreadCond));
		citthread_mutex_destroy(&(this_thread->SleepMutex));
		citthread_cond_destroy(&(this_thread->SleepCond));
		citthread_attr_destroy(&this_thread->attr);
		free(this_thread);
		return NULL;
	}
	
	num_threads++;	// Increase the count of threads in the system.
	if(this_thread->flags & CTDLTHREAD_WORKER)
		num_workers++;

	this_thread->next = CtdlThreadList;
	CtdlThreadList = this_thread;
	if (this_thread->next)
		this_thread->next->prev = this_thread;
	ctdl_thread_internal_calc_loadavg();
	
//	citthread_mutex_unlock(&this_thread->ThreadMutex);
	end_critical_section(S_THREAD_LIST);
	
	return this_thread;
}

/*
 * Wrapper function to create a thread
 * ensures the critical section and other protections are in place.
 * char *name = name to give to thread, if NULL, use generic name
 * int flags = flags to determine type of thread and standard facilities
 */
CtdlThreadNode *CtdlThreadCreate(char *name, long flags, void *(*thread_func) (void *arg), void *args)
{
	CtdlThreadNode *ret = NULL;
	
	ret = ctdl_internal_create_thread(name, flags, thread_func, args);
	return ret;
}



/*
 * Internal function to schedule a thread.
 * Must be called from within a S_THREAD_LIST critical section
 */ 
CtdlThreadNode *CtdlThreadSchedule(char *name, long flags, void *(*thread_func) (void *arg), void *args, time_t when)
{
	CtdlThreadNode *this_thread;

	if (num_threads >= 32767)
	{
		CtdlLogPrintf(CTDL_EMERG, "Thread system. Thread list full.\n");
		return NULL;
	}
		
	this_thread = malloc(sizeof(CtdlThreadNode));
	if (this_thread == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't allocate CtdlThreadNode, exiting\n");
		return NULL;
	}
	/* Initialise the thread structure */
	if (ctdl_internal_init_thread_struct(this_thread, flags) == NULL)
	{
		free(this_thread);
		CtdlLogPrintf(CTDL_EMERG, "Thread system, can't initialise CtdlThreadNode, exiting\n");
		return NULL;
	}

	/*
	 * If we got here we are going to create the thread so we must initilise the structure
	 * first because most implimentations of threading can't create it in a stopped state
	 * and it might want to do things with its structure that aren't initialised otherwise.
	 */
	if(name)
	{
		this_thread->name = name;
	}
	else
	{
		this_thread->name = "Un-named Thread";
	}
	
	this_thread->flags = flags;
	this_thread->thread_func = thread_func;
	this_thread->user_args = args;
	
	/*
	 * When to start this thread
	 */
	this_thread->when = when;

	begin_critical_section(S_SCHEDULE_LIST);
	this_thread->next = CtdlThreadSchedList;
	CtdlThreadSchedList = this_thread;
	if (this_thread->next)
		this_thread->next->prev = this_thread;
	end_critical_section(S_SCHEDULE_LIST);
	
	return this_thread;
}



CtdlThreadNode *ctdl_thread_internal_start_scheduled (CtdlThreadNode *this_thread)
{
	int ret = 0;
	
//	citthread_mutex_lock(&that_thread->ThreadMutex);
	begin_critical_section(S_THREAD_LIST);
	/*
	 * We pass this_thread into the thread as its args so that it can find out information
	 * about itself and it has a bit of storage space for itself, not to mention that the REAL
	 * thread function needs to finish off the setup of the structure
	 */
	if ((ret = citthread_create(&this_thread->tid, &this_thread->attr, ctdl_internal_thread_func, this_thread) != 0))
	{
		end_critical_section(S_THREAD_LIST);
		CtdlLogPrintf(CTDL_DEBUG, "Failed to start scheduled thread \"%s\": %s\n", this_thread->name, strerror(ret));
//		citthread_mutex_unlock(&this_thread->ThreadMutex);
		citthread_mutex_destroy(&(this_thread->ThreadMutex));
		citthread_cond_destroy(&(this_thread->ThreadCond));
		citthread_mutex_destroy(&(this_thread->SleepMutex));
		citthread_cond_destroy(&(this_thread->SleepCond));
		citthread_attr_destroy(&this_thread->attr);
		free(this_thread);
		return NULL;
	}
	
	
	num_threads++;	// Increase the count of threads in the system.
	if(this_thread->flags & CTDLTHREAD_WORKER)
		num_workers++;

	this_thread->next = CtdlThreadList;
	CtdlThreadList = this_thread;
	if (this_thread->next)
		this_thread->next->prev = this_thread;
//	citthread_mutex_unlock(&that_thread->ThreadMutex);
	
	ctdl_thread_internal_calc_loadavg();
	end_critical_section(S_THREAD_LIST);
	
	
	return this_thread;
}



void ctdl_thread_internal_check_scheduled(void)
{
	CtdlThreadNode *this_thread, *that_thread;
	time_t now;
	
	if (try_critical_section(S_SCHEDULE_LIST))
		return;	/* If this list is locked we wait till the next chance */
	
	now = time(NULL);
	
#ifdef WITH_THREADLOG
	CtdlLogPrintf(CTDL_DEBUG, "Checking for scheduled threads to start.\n");
#endif

	this_thread = CtdlThreadSchedList;
	while(this_thread)
	{
		that_thread = this_thread;
		this_thread = this_thread->next;
		
		if (now > that_thread->when)
		{
			/* Unlink from schedule list */
			if (that_thread->prev)
				that_thread->prev->next = that_thread->next;
			else
				CtdlThreadSchedList = that_thread->next;
			if (that_thread->next)
				that_thread->next->prev = that_thread->prev;
				
			that_thread->next = that_thread->prev = NULL;
#ifdef WITH_THREADLOG
			CtdlLogPrintf(CTDL_DEBUG, "About to start scheduled thread \"%s\".\n", that_thread->name);
#endif
			if (CT->state > CTDL_THREAD_STOP_REQ)
			{	/* Only start it if the system is not stopping */
				if (ctdl_thread_internal_start_scheduled (that_thread))
				{
#ifdef WITH_THREADLOG
					CtdlLogPrintf(CTDL_INFO, "Thread system, Started a scheduled thread \"%s\" (%ud).\n",
						that_thread->name, that_thread->tid);
#endif
				}
			}
		}
#ifdef WITH_THREADLOG
		else
		{
			CtdlLogPrintf(CTDL_DEBUG, "Thread \"%s\" will start in %ld seconds.\n",
				that_thread->name, that_thread->when - time(NULL));
		}
#endif
	}
	end_critical_section(S_SCHEDULE_LIST);
}


/*
 * A warapper function for select so we can show a thread as blocked
 */
int CtdlThreadSelect(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	int ret;
	
	ctdl_thread_internal_change_state(CT, CTDL_THREAD_BLOCKED);
	ret = select(n, readfds, writefds, exceptfds, timeout);
	/**
	 * If the select returned <= 0 then it failed due to an error
	 * or timeout so this thread could stop if asked to do so.
	 * Anything else means it needs to continue unless the system is shutting down
	 */
	if (ret <= 0)
	{
		/**
		 * select says nothing to do so we can change to running if we haven't been asked to stop.
		 */
		ctdl_thread_internal_change_state(CT, CTDL_THREAD_RUNNING);
	}
	else
	{
		/**
		 * The select says this thread needs to do something useful.
		 * This thread was in an idle state so it may have been asked to stop
		 * but if the system isn't shutting down this thread is no longer
		 * idle and select has given it a task to do so it must not stop
		 * In this condition we need to force it into the running state.
		 * CtdlThreadGC will clear its ticker for us.
		 */
		if (GC_thread->state > CTDL_THREAD_STOP_REQ)
		{
			citthread_mutex_lock(&CT->ThreadMutex); /* To prevent race condition of a sleeping thread */
			CT->state = CTDL_THREAD_RUNNING;
			citthread_mutex_unlock(&CT->ThreadMutex);
		}
	}

	return ret;
}



void *new_worker_thread(void *arg);
extern void close_masters (void);



void go_threading(void)
{
	int i;
	CtdlThreadNode *last_worker;
	
	/*
	 * Initialise the thread system
	 */
	ctdl_thread_internal_init();

	/* Second call to module init functions now that threading is up */
	initialise_modules(1);

	/*
	 * This thread is now used for garbage collection of other threads in the thread list
	 */
	CtdlLogPrintf(CTDL_INFO, "Startup thread %d becoming garbage collector,\n", citthread_self());

	/*
	 * We do a lot of locking and unlocking of the thread list in here.
	 * We do this so that we can repeatedly release time for other threads
	 * that may be waiting on the thread list.
	 * We are a low priority thread so we can afford to do this
	 */
	
	while (CtdlThreadGetCount())
	{
		if (CT->signal)
			exit_signal = CT->signal;
		if (exit_signal)
		{
			CtdlThreadStopAll();
//			close_masters();
		}
		check_sched_shutdown();
		if (CT->state > CTDL_THREAD_STOP_REQ)
		{
			begin_critical_section(S_THREAD_LIST);
			ctdl_thread_internal_calc_loadavg();
			end_critical_section(S_THREAD_LIST);
			
			ctdl_thread_internal_check_scheduled(); /* start scheduled threads */
		}
		
		/* Reduce the size of the worker thread pool if necessary. */
		if ((CtdlThreadGetWorkers() > config.c_min_workers + 1) && (CtdlThreadWorkerAvg < 20) && (CT->state > CTDL_THREAD_STOP_REQ))
		{
			/* Ask a worker thread to stop as we no longer need it */
			begin_critical_section(S_THREAD_LIST);
			last_worker = CtdlThreadList;
			while (last_worker)
			{
				citthread_mutex_lock(&last_worker->ThreadMutex);
				if (last_worker->flags & CTDLTHREAD_WORKER && (last_worker->state > CTDL_THREAD_STOPPING) && (last_worker->Context == NULL))
				{
					citthread_mutex_unlock(&last_worker->ThreadMutex);
					break;
				}
				citthread_mutex_unlock(&last_worker->ThreadMutex);
				last_worker = last_worker->next;
			}
			end_critical_section(S_THREAD_LIST);
			if (last_worker)
			{
#ifdef WITH_THREADLOG
				CtdlLogPrintf(CTDL_DEBUG, "Thread system, stopping excess worker thread \"%s\" (0x%08lx).\n",
					last_worker->name,
					last_worker->tid
					);
#endif
				CtdlThreadStop(last_worker);
			}
		}
	
		/*
		 * If all our workers are working hard, start some more to help out
		 * with things
		 */
		/* FIXME: come up with a better way to dynamically alter the number of threads
		 * based on the system load
		 */
#ifdef NEW_WORKER
		if ((((CtdlThreadGetWorkers() < config.c_max_workers) && (CtdlThreadGetWorkers() <= num_sessions) ) || CtdlThreadGetWorkers() < config.c_min_workers) && (CT->state > CTDL_THREAD_STOP_REQ))
#else
		if ((((CtdlThreadGetWorkers() < config.c_max_workers) && (CtdlThreadGetWorkerAvg() > 60) && (CtdlThreadGetLoadAvg() < 90) ) || CtdlThreadGetWorkers() < config.c_min_workers) && (CT->state > CTDL_THREAD_STOP_REQ))
#endif /* NEW_WORKER */
		{
			for (i=0; i<5 ; i++)
			{
#ifdef NEW_WORKER
				CtdlThreadCreate("Worker Thread (new)",
					CTDLTHREAD_BIGSTACK + CTDLTHREAD_WORKER,
					new_worker_thread,
					NULL
					);
#else
				CtdlThreadCreate("Worker Thread",
					CTDLTHREAD_BIGSTACK + CTDLTHREAD_WORKER,
					worker_thread,
					NULL
					);
#endif /* NEW_WORKER */
			}
		}
		
		CtdlThreadGC();
		
		if (CtdlThreadGetCount() <= 1) // Shutting down clean up the garbage collector
		{
			CtdlThreadGC();
		}
		
		if (CtdlThreadGetCount())
			CtdlThreadSleep(1);
	}
	/*
	 * If the above loop exits we must be shutting down since we obviously have no threads
	 */
	ctdl_thread_internal_cleanup();
}




/*
 * Starting a new implimentation of a worker thread.
 * This new implimentation will be faster and do more work per thread.
 */
 
/*
 * Select on master socket.
 * First worker thread in here acquires the lock and builds an FDSET of master sockets.
 * then it goes into a loop selecting on the master sockets timing out every few milliseconds.
 * If it times out it rebiulds its list and loops.
 * If the select succeeds it creates a new context and returns.
 * During this time the other workers are selecting on existing contexts or sleeping.
 */
void select_on_master(void)
{
        fd_set readfds;
        struct ServiceFunctionHook *serviceptr;
        int ssock;			/* Descriptor for client socket */
        int highest;
        int m, i;
        int retval = 0;
        struct timeval tv;
        struct CitContext *con;
        const char *old_name;



        old_name = CtdlThreadName("select_on_master");

        /* Initialize the fdset. */
        FD_ZERO(&readfds);
        highest = 0;

        /* First, add the various master sockets to the fdset. */
        for (serviceptr = ServiceHookTable; serviceptr != NULL; serviceptr = serviceptr->next ) {
                m = serviceptr->msock;
                FD_SET(m, &readfds);
                if (m > highest) {
                        highest = m;
                }
        }

        tv.tv_sec = 1;		/* wake up every 1 sec if no input */
        tv.tv_usec = 0;
        retval = CtdlThreadSelect(highest + 1, &readfds, NULL, NULL, &tv);

        /* Select got an error or we are shutting down so get out */
        if (retval == 0 || CtdlThreadCheckStop()) {
                CtdlThreadName(old_name);
                return;
        }

        /* Select says something happened on one of our master sockets so now we handle it */
        for (serviceptr = ServiceHookTable; serviceptr != NULL; serviceptr = serviceptr->next ) {
                if (FD_ISSET(serviceptr->msock, &readfds)) {
                        ssock = accept(serviceptr->msock, NULL, 0);
                        if (ssock >= 0) {
                                CtdlLogPrintf(CTDL_DEBUG, "New client socket %d\n", ssock);
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
                                CT->Context = con;

                                /* Assign our new socket number to it. */
                                con->client_socket = ssock;
                                con->h_command_function = serviceptr->h_command_function;
                                con->h_async_function = serviceptr->h_async_function;
                                con->ServiceName = serviceptr->ServiceName;
                                /* Determine whether it's a local socket */
                                if (serviceptr->sockpath != NULL)
                                        con->is_local_socket = 1;

                                /* Set the SO_REUSEADDR socket option */
                                i = 1;
                                setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

                                become_session(con);
                                begin_session(con);
                                serviceptr->h_greeting_function();
                                become_session(NULL);
                                con->state = CON_IDLE;
                                break;
                        }
                }
        }

        CtdlThreadName(old_name);
}

/*
 * Select on client socket.
 * First worker thread in here acquires the lock and builds an FDSET of client sockets.
 * then it selects on the client sockets timing out after 1 second.
 * If it times out the thread goes off to check on housekeeping etc.
 * If the select succeeds the thread goes off to handle the client request.
 * If the list of client connections is empty the threads all sleep for one second
 */
struct CitContext *select_on_client(void)
{
	fd_set readfds;
	struct timeval tv;
	int retval = 0;
	int highest=0;
	const char *old_name;
	
	
	old_name = CtdlThreadName("select_on_client");
	
	/* Initialise the fdset */
	FD_ZERO(&readfds);
	FD_SET(CT->Context->client_socket, &readfds);
	highest = CT->Context->client_socket;	
	/* Now we can select on any connections that are waiting */
	
	if (!CtdlThreadCheckStop())
	{
		tv.tv_sec = config.c_sleeping;		/* wake up every second if no input */
		tv.tv_usec = 0;
		retval = select(highest + 1, &readfds, NULL, NULL, &tv);
	}
	else	/* Shutting down? */
	{
		CtdlThreadName(old_name);
		return(NULL);
	}
		

	/* Now figure out who made this select() unblock.
	 * First, check for an error or exit condition.
	 */
	if (retval < 0) {
		if (errno == EBADF) {
			CtdlLogPrintf(CTDL_NOTICE, "select() failed: (%s)\n",
				strerror(errno));
		}
		if (errno != EINTR) {
			CtdlLogPrintf(CTDL_EMERG, "Exiting (%s)\n", strerror(errno));
			CtdlThreadStopAll();
		} else if (!CtdlThreadCheckStop()) {
			CtdlLogPrintf(CTDL_DEBUG, "Un handled select failure.\n");
		}
		CtdlThreadName(old_name);
		return NULL;
	}
	else if(retval == 0)
	{
		CtdlThreadName(old_name);
		CT->Context->kill_me = 1;
		CT->Context = NULL;
		return CT->Context;
	}
	
	CT->Context->state = CON_EXECUTING;
	CT->Context->input_waiting = 1;
	
	CtdlThreadName(old_name);
	return (CT->Context);
}



/*
 * Do the worker threads work when needed
 */
int execute_session(struct CitContext *bind_me)
{
	int force_purge;
	
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
	if (force_purge)
		CT->Context = NULL;
	become_session(NULL);
	bind_me->state = CON_IDLE;
	return force_purge;
}



extern void dead_session_purge(int force);

/*
 * A new worker_thread loop.
 */
 
void *new_worker_thread(void *arg)
{
	struct CitContext *bind_me;
	int force_purge;
	
	while (!CtdlThreadCheckStop()) {

		/* make doubly sure we're not holding any stale db handles
		 * which might cause a deadlock.
		 */
		cdb_check_handles();
		force_purge = 0;
		bind_me = NULL;		/* Which session shall we handle? */
			
		if (CT->Context == NULL)
			select_on_master();
		if (CtdlThreadCheckStop())
			break;
			
		if (CT->Context)
			bind_me = select_on_client();
		if (CtdlThreadCheckStop())
			break;
			
		if (bind_me)
			force_purge = execute_session(bind_me);
			
		dead_session_purge(force_purge);
		if (CtdlThreadCheckStop())
			break;
			
		do_housekeeping();
	}
	return NULL;
}
