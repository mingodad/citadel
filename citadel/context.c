/*
 * $Id: sysdep.c 7989 2009-10-31 15:29:37Z davew $
 *
 * Citadel context management stuff.
 * See COPYING for copyright information.
 *
 * Here's where we (hopefully) have all the code that manipulates contexts.
 *
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "modules/crypto/serv_crypto.h"	/* Needed for init_ssl, client_write_ssl, client_read_ssl, destruct_ssl */
#include "ecrash.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "ctdl_module.h"
#include "threads.h"
#include "user_ops.h"
#include "control.h"



citthread_key_t MyConKey;				/* TSD key for MyContext() */


CitContext masterCC;
CitContext *ContextList = NULL;

time_t last_purge = 0;				/* Last dead session purge */
int num_sessions = 0;				/* Current number of sessions */

/* Flag for single user mode */
static int want_single_user = 0;

/* Try to go single user */

int CtdlTrySingleUser(void)
{
	int can_do = 0;
	
	begin_critical_section(S_SINGLE_USER);
	if (want_single_user)
		can_do = 0;
	else
	{
		can_do = 1;
		want_single_user = 1;
	}
	end_critical_section(S_SINGLE_USER);
	return can_do;
}

void CtdlEndSingleUser(void)
{
	begin_critical_section(S_SINGLE_USER);
	want_single_user = 0;
	end_critical_section(S_SINGLE_USER);
}


int CtdlWantSingleUser(void)
{
	return want_single_user;
}

int CtdlIsSingleUser(void)
{
	if (want_single_user)
	{
		/* check for only one context here */
		if (num_sessions == 1)
			return TRUE;
	}
	return FALSE;
}




/*
 * Check to see if the user who we just sent mail to is logged in.  If yes,
 * bump the 'new mail' counter for their session.  That enables them to
 * receive a new mail notification without having to hit the database.
 */
void BumpNewMailCounter(long which_user) 
{
	CtdlBumpNewMailCounter(which_user);
}

void CtdlBumpNewMailCounter(long which_user)
{
	CitContext *ptr;

	begin_critical_section(S_SESSION_TABLE);

	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if (ptr->user.usernum == which_user) {
			ptr->newmail += 1;
		}
	}

	end_critical_section(S_SESSION_TABLE);
}


/*
 * Check to see if a user is currently logged in
 * Take care with what you do as a result of this test.
 * The user may not have been logged in when this function was called BUT
 * because of threading the user might be logged in before you test the result.
 */
int CtdlIsUserLoggedIn (char *user_name)
{
	CitContext *cptr;
	int ret = 0;

	begin_critical_section (S_SESSION_TABLE);
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if (!strcasecmp(cptr->user.fullname, user_name)) {
			ret = 1;
			break;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	return ret;
}



/*
 * Check to see if a user is currently logged in.
 * Basically same as CtdlIsUserLoggedIn() but uses the user number instead.
 * Take care with what you do as a result of this test.
 * The user may not have been logged in when this function was called BUT
 * because of threading the user might be logged in before you test the result.
 */
int CtdlIsUserLoggedInByNum (long usernum)
{
	CitContext *cptr;
	int ret = 0;

	begin_critical_section(S_SESSION_TABLE);
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if (cptr->user.usernum == usernum) {
			ret = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	return ret;
}



/*
 * Return a pointer to the CitContext structure bound to the thread which
 * called this function.  If there's no such binding (for example, if it's
 * called by the housekeeper thread) then a generic 'master' CC is returned.
 *
 * This function is used *VERY* frequently and must be kept small.
 */
CitContext *MyContext(void) {

	register CitContext *c;

	return ((c = (CitContext *) citthread_getspecific(MyConKey),
		c == NULL) ? &masterCC : c
	);
}




/*
 * Terminate a session.
 */
void RemoveContext (CitContext *con)
{
	if (con==NULL) {
		CtdlLogPrintf(CTDL_ERR,
			"WARNING: RemoveContext() called with NULL!\n");
		return;
	}
	CtdlLogPrintf(CTDL_DEBUG, "RemoveContext() session %d\n", con->cs_pid);

	/* Run any cleanup routines registered by loadable modules.
	 * Note: We have to "become_session()" because the cleanup functions
	 *       might make references to "CC" assuming it's the right one.
	 */
	become_session(con);
	logout();
	PerformSessionHooks(EVT_STOP);
	become_session(NULL);

	CtdlLogPrintf(CTDL_NOTICE, "[%3d] Session ended.\n", con->cs_pid);

	/* If the client is still connected, blow 'em away. */
	CtdlLogPrintf(CTDL_DEBUG, "Closing socket %d\n", con->client_socket);
	close(con->client_socket);

	/* If using AUTHMODE_LDAP, free the DN */
	if (con->ldap_dn) {
		free(con->ldap_dn);
		con->ldap_dn = NULL;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Done with RemoveContext()\n");
}




/*
 * Initialize a new context and place it in the list.  The session number
 * used to be the PID (which is why it's called cs_pid), but that was when we
 * had one process per session.  Now we just assign them sequentially, starting
 * at 1 (don't change it to 0 because masterCC uses 0).
 */
CitContext *CreateNewContext(void) {
	CitContext *me;
	static int next_pid = 0;

	me = (CitContext *) malloc(sizeof(CitContext));
	if (me == NULL) {
		CtdlLogPrintf(CTDL_ALERT, "citserver: can't allocate memory!!\n");
		return NULL;
	}
	memset(me, 0, sizeof(CitContext));
	
	/* Give the contaxt a name. Hopefully makes it easier to track */
	strcpy (me->user.fullname, "SYS_notauth");
	
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
	me->cs_pid = ++next_pid;
	me->prev = NULL;
	me->next = ContextList;
	ContextList = me;
	if (me->next != NULL) {
		me->next->prev = me;
	}
	++num_sessions;
	end_critical_section(S_SESSION_TABLE);
	return (me);
}


CitContext *CtdlGetContextArray(int *count)
{
	int nContexts, i;
	CitContext *nptr, *cptr;
	
	nContexts = num_sessions;
	nptr = malloc(sizeof(CitContext) * nContexts);
	if (!nptr)
		return NULL;
	begin_critical_section(S_SESSION_TABLE);
	for (cptr = ContextList, i=0; cptr != NULL && i < nContexts; cptr = cptr->next, i++)
		memcpy(&nptr[i], cptr, sizeof (CitContext));
	end_critical_section (S_SESSION_TABLE);
	
	*count = i;
	return nptr;
}



/**
 * This function fills in a context and its user field correctly
 * Then creates/loads that user
 */
void CtdlFillSystemContext(CitContext *context, char *name)
{
	char sysname[USERNAME_SIZE];

	memset(context, 0, sizeof(CitContext));
	context->internal_pgm = 1;
	context->cs_pid = 0;
	strcpy (sysname, "SYS_");
	strcat (sysname, name);
	/* internal_create_user has the side effect of loading the user regardless of wether they
	 * already existed or needed to be created
	 */
	internal_create_user (sysname, &(context->user), -1) ;
	
	/* Check to see if the system user needs upgrading */
	if (context->user.usernum == 0)
	{	/* old system user with number 0, upgrade it */
		context->user.usernum = get_new_user_number();
		CtdlLogPrintf(CTDL_DEBUG, "Upgrading system user \"%s\" from user number 0 to user number %d\n", context->user.fullname, context->user.usernum);
		/* add user to the database */
		CtdlPutUser(&(context->user));
		cdb_store(CDB_USERSBYNUMBER, &(context->user.usernum), sizeof(long), context->user.fullname, strlen(context->user.fullname)+1);
	}
}

/*
 * Cleanup any contexts that are left lying around
 */
void context_cleanup(void)
{
	CitContext *ptr = NULL;
	CitContext *rem = NULL;

	/*
	 * Clean up the contexts.
	 * There are no threads so no critical_section stuff is needed.
	 */
	ptr = ContextList;
	
	/* We need to update the ContextList because some modules may want to itterate it
	 * Question is should we NULL it before iterating here or should we just keep updating it
	 * as we remove items?
	 *
	 * Answer is to NULL it first to prevent modules from doing any actions on the list at all
	 */
	ContextList=NULL;
	while (ptr != NULL){
		/* Remove the session from the active list */
		rem = ptr->next;
		--num_sessions;
		
		CtdlLogPrintf(CTDL_DEBUG, "Purging session %d\n", ptr->cs_pid);
		RemoveContext(ptr);
		free (ptr);
		ptr = rem;
	}
}



/*
 * Terminate another session.
 * (This could justifiably be moved out of sysdep.c because it
 * no longer does anything that is system-dependent.)
 */
void kill_session(int session_to_kill) {
	CitContext *ptr;

	begin_critical_section(S_SESSION_TABLE);
	for (ptr = ContextList; ptr != NULL; ptr = ptr->next) {
		if (ptr->cs_pid == session_to_kill) {
			ptr->kill_me = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
}

/*
 * Purge all sessions which have the 'kill_me' flag set.
 * This function has code to prevent it from running more than once every
 * few seconds, because running it after every single unbind would waste a lot
 * of CPU time and keep the context list locked too much.  To force it to run
 * anyway, set "force" to nonzero.
 */
void dead_session_purge(int force) {
	CitContext *ptr, *ptr2;		/* general-purpose utility pointer */
	CitContext *rem = NULL;	/* list of sessions to be destroyed */
	
	if (force == 0) {
		if ( (time(NULL) - last_purge) < 5 ) {
			return;	/* Too soon, go away */
		}
	}
	time(&last_purge);

	if (try_critical_section(S_SESSION_TABLE))
		return;
		
	ptr = ContextList;
	while (ptr) {
		ptr2 = ptr;
		ptr = ptr->next;
		
		if ( (ptr2->state == CON_IDLE) && (ptr2->kill_me) ) {
			/* Remove the session from the active list */
			if (ptr2->prev) {
				ptr2->prev->next = ptr2->next;
			}
			else {
				ContextList = ptr2->next;
			}
			if (ptr2->next) {
				ptr2->next->prev = ptr2->prev;
			}

			--num_sessions;
			/* And put it on our to-be-destroyed list */
			ptr2->next = rem;
			rem = ptr2;
		}
	}
	end_critical_section(S_SESSION_TABLE);

	/* Now that we no longer have the session list locked, we can take
	 * our time and destroy any sessions on the to-be-killed list, which
	 * is allocated privately on this thread's stack.
	 */
	while (rem != NULL) {
		CtdlLogPrintf(CTDL_DEBUG, "Purging session %d\n", rem->cs_pid);
		RemoveContext(rem);
		ptr = rem;
		rem = rem->next;
		free(ptr);
	}
}





/*
 * masterCC is the context we use when not attached to a session.  This
 * function initializes it.
 */
void InitializeMasterCC(void) {
	memset(&masterCC, 0, sizeof( CitContext));
	masterCC.internal_pgm = 1;
	masterCC.cs_pid = 0;
}




/*
 * Bind a thread to a context.  (It's inline merely to speed things up.)
 */
INLINE void become_session(CitContext *which_con) {
	citthread_setspecific(MyConKey, (void *)which_con );
}



