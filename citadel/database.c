/*
 * $Id$
 *
 * This file contains a set of abstractions that allow Citadel to plug into any
 * record manager or database system for its data store.
 *
 */

/*
 * Note that each call to a GDBM function is wrapped in an S_DATABASE critical
 * section.  This is done because GDBM is not threadsafe.  This is the ONLY
 * place in the entire Citadel server where any code enters two different
 * classes of critical sections at the same time; this is why the GDBM calls
 * are *tightly* wrapped in S_DATABASE.  Opening multiple concurrent critical
 * sections elsewhere in the code can, and probably will, cause deadlock
 * conditions to occur.  (Deadlock is bad.  Eliminate.)
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_GDBM_H
#include <gdbm.h>
#endif
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "sysdep_decls.h"


/*
 * This array holds one gdbm handle for each Citadel database.
 */
GDBM_FILE gdbms[MAXCDB];

/*
 * We also keep these around, for sequential searches (one per session slot)
 */
int max_keys = 0;
datum *dtkey;


/*
 * Reclaim unused space in the databases.  We need to do each one of
 * these discretely, rather than in a loop.
 */
void defrag_databases(void)
{

	/* defrag the message base */
	lprintf(7, "Defragmenting message base\n");
	begin_critical_section(S_MSGMAIN);
	begin_critical_section(S_DATABASE);
	gdbm_reorganize(gdbms[CDB_MSGMAIN]);
	end_critical_section(S_DATABASE);
	end_critical_section(S_MSGMAIN);

	/* defrag the user file, mailboxes, and user/room relationships */
	lprintf(7, "Defragmenting user file\n");
	begin_critical_section(S_USERSUPP);
	begin_critical_section(S_DATABASE);
	gdbm_reorganize(gdbms[CDB_USERSUPP]);
	gdbm_reorganize(gdbms[CDB_VISIT]);
	end_critical_section(S_DATABASE);
	end_critical_section(S_USERSUPP);

	/* defrag the room files and message lists */
	lprintf(7, "Defragmenting room files and message lists\n");
	begin_critical_section(S_QUICKROOM);
	begin_critical_section(S_DATABASE);
	gdbm_reorganize(gdbms[CDB_QUICKROOM]);
	gdbm_reorganize(gdbms[CDB_MSGLISTS]);
	end_critical_section(S_DATABASE);
	end_critical_section(S_QUICKROOM);

	/* defrag the floor table */
	lprintf(7, "Defragmenting floor table\n");
	begin_critical_section(S_FLOORTAB);
	begin_critical_section(S_DATABASE);
	gdbm_reorganize(gdbms[CDB_FLOORTAB]);
	end_critical_section(S_DATABASE);
	end_critical_section(S_FLOORTAB);
}


/*
 * Open the various gdbm databases we'll be using.  Any database which
 * does not exist should be created.
 */
void open_databases(void)
{
	lprintf(7, "%s\n", gdbm_version);

	/*
	 * Silently try to create the database subdirectory.  If it's
	 * already there, no problem.
	 */
	system("exec mkdir data 2>/dev/null");

	/* a critical section is unnecessary, as this function is called before
	   any other threads are created. and it causes problems on BSDI.

	   begin_critical_section(S_DATABASE);

	 */

	gdbms[CDB_MSGMAIN] = gdbm_open("data/msgmain.gdbm", 8192,
				       GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_MSGMAIN] == NULL) {
		lprintf(2, "Cannot open msgmain: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	gdbms[CDB_USERSUPP] = gdbm_open("data/usersupp.gdbm", 0,
					GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_USERSUPP] == NULL) {
		lprintf(2, "Cannot open usersupp: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	gdbms[CDB_VISIT] = gdbm_open("data/visit.gdbm", 0,
				     GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_VISIT] == NULL) {
		lprintf(2, "Cannot open visit file: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	gdbms[CDB_QUICKROOM] = gdbm_open("data/quickroom.gdbm", 0,
					 GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_QUICKROOM] == NULL) {
		lprintf(2, "Cannot open quickroom: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	gdbms[CDB_FLOORTAB] = gdbm_open("data/floortab.gdbm", 0,
					GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_FLOORTAB] == NULL) {
		lprintf(2, "Cannot open floortab: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	gdbms[CDB_MSGLISTS] = gdbm_open("data/msglists.gdbm", 0,
					GDBM_WRCREAT, 0600, NULL);
	if (gdbms[CDB_MSGLISTS] == NULL) {
		lprintf(2, "Cannot open msglists: %s\n",
			gdbm_strerror(gdbm_errno));
		exit(1);
	}
	/*
	   end_critical_section(S_DATABASE);
	 */

}


/*
 * Close all of the gdbm database files we've opened.  This can be done
 * in a loop, since it's just a bunch of closes.
 */
void close_databases(void)
{
	int a;

	begin_critical_section(S_DATABASE);
	for (a = 0; a < MAXCDB; ++a) {
		lprintf(7, "Closing database %d\n", a);
		gdbm_close(gdbms[a]);
	}
	end_critical_section(S_DATABASE);

	for (a = 0; a < max_keys; ++a) {
		if (dtkey[a].dptr != NULL) {
			phree(dtkey[a].dptr);
		}
	}

}


/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * datum already exists it should be overwritten.
 */
int cdb_store(int cdb,
	      void *key, int keylen,
	      void *data, int datalen)
{

	datum dkey, ddata;
	int retval;

	dkey.dsize = keylen;
	dkey.dptr = key;
	ddata.dsize = datalen;
	ddata.dptr = data;

	begin_critical_section(S_DATABASE);
	retval = gdbm_store(gdbms[cdb], dkey, ddata, GDBM_REPLACE);
	end_critical_section(S_DATABASE);
	if (retval < 0) {
		lprintf(2, "gdbm error: %s\n", gdbm_strerror(gdbm_errno));
		return (-1);
	}
	return (0);
}


/*
 * Delete a piece of data.  Returns 0 if the operation was successful.
 */
int cdb_delete(int cdb, void *key, int keylen)
{

	datum dkey;
	int retval;

	dkey.dsize = keylen;
	dkey.dptr = key;

	begin_critical_section(S_DATABASE);
	retval = gdbm_delete(gdbms[cdb], dkey);
	end_critical_section(S_DATABASE);
	return (retval);

}




/*
 * Fetch a piece of data.  If not found, returns NULL.  Otherwise, it returns
 * a struct cdbdata which it is the caller's responsibility to free later on
 * using the cdb_free() routine.
 */
struct cdbdata *cdb_fetch(int cdb, void *key, int keylen)
{

	struct cdbdata *tempcdb;
	datum dkey, dret;

	dkey.dsize = keylen;
	dkey.dptr = key;

	begin_critical_section(S_DATABASE);
	dret = gdbm_fetch(gdbms[cdb], dkey);
	end_critical_section(S_DATABASE);
	if (dret.dptr == NULL) {
		return NULL;
	}
	tempcdb = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	if (tempcdb == NULL) {
		lprintf(2, "Cannot allocate memory!\n");
	}
	tempcdb->len = dret.dsize;
	tempcdb->ptr = dret.dptr;
	return (tempcdb);
}


/*
 * Free a cdbdata item (ok, this is really no big deal, but we might need to do
 * more complex stuff with other database managers in the future).
 */
void cdb_free(struct cdbdata *cdb)
{
	phree(cdb->ptr);
	phree(cdb);
}


/* 
 * Prepare for a sequential search of an entire database.  (In the gdbm model,
 * we do this by keeping an array dtkey[] of "the next" key for each session
 * that is open.  There is guaranteed to be no more than one traversal in
 * progress per session at any given time.)
 */
void cdb_rewind(int cdb)
{

	while (max_keys <= CC->cs_pid) {
		++max_keys;
		if (dtkey == NULL) {
			dtkey = (datum *)
			    mallok((sizeof(datum) * max_keys));
		} else {
			dtkey = (datum *)
			    reallok(dtkey, (sizeof(datum) * max_keys));
		}
		dtkey[max_keys - 1].dsize = 0;
		dtkey[max_keys - 1].dptr = NULL;
	}

	if (dtkey[CC->cs_pid].dptr != NULL) {
		phree(dtkey[CC->cs_pid].dptr);
	}
	begin_critical_section(S_DATABASE);
	dtkey[CC->cs_pid] = gdbm_firstkey(gdbms[cdb]);
	end_critical_section(S_DATABASE);
}


/*
 * Fetch the next item in a sequential search.  Returns a pointer to a 
 * cdbdata structure, or NULL if we've hit the end.
 */
struct cdbdata *cdb_next_item(int cdb)
{
	datum dret;
	struct cdbdata *cdbret;
	void *ptr = NULL;


	if (dtkey[CC->cs_pid].dptr == NULL) {	/* end of file */
		return NULL;
	}
	begin_critical_section(S_DATABASE);
	dret = gdbm_fetch(gdbms[cdb], dtkey[CC->cs_pid]);
	end_critical_section(S_DATABASE);
	if (dret.dptr == NULL) {	/* bad read */
		phree(dtkey[CC->cs_pid].dptr);
		return NULL;
	}
	cdbret = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	cdbret->len = dret.dsize;
	cdbret->ptr = dret.dptr;

	ptr = dtkey[CC->cs_pid].dptr;
	begin_critical_section(S_DATABASE);
	dtkey[CC->cs_pid] = gdbm_nextkey(gdbms[cdb], dtkey[CC->cs_pid]);
	end_critical_section(S_DATABASE);

	if (ptr != NULL) {	/* Free the previous key. */
		free(ptr);
	}

	return (cdbret);
}
