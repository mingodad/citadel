/*
 * $Id$
 *
 * Sleepycat (Berkeley) DB driver for Citadel/UX
 *
 */

/*****************************************************************************
       Tunable configuration parameters for the Sleepycat DB back end
 *****************************************************************************/

/* Citadel will checkpoint the db at the end of every session, but only if
 * the specified number of kilobytes has been written, or if the specified
 * number of minutes has passed, since the last checkpoint.
 */
#define MAX_CHECKPOINT_KBYTES	0
#define MAX_CHECKPOINT_MINUTES	15

/*****************************************************************************/

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <db.h>
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "database.h"
#include "sysdep_decls.h"
#include "dynloader.h"

DB *dbp[MAXCDB];		/* One DB handle for each Citadel database */
DB_ENV *dbenv;			/* The DB environment (global) */

struct cdbtsd {			/* Thread-specific DB stuff */
	DB_TXN *tid;		/* Transaction handle */
	DBC *cursor;		/* Cursor, for traversals... */
};

int num_ssd = 0;

static pthread_key_t tsdkey;

#define MYCURSOR	(((struct cdbtsd*)pthread_getspecific(tsdkey))->cursor)
#define MYTID		(((struct cdbtsd*)pthread_getspecific(tsdkey))->tid)

/* just a little helper function */
static int txabort(DB_TXN *tid) {
        int ret = txn_abort(tid);

        if (ret) {
                lprintf(1, "cdb_*: txn_abort: %s\n", db_strerror(ret));
		abort();
	}

        return ret;
}

/* this one is even more helpful than the last. */
static int txcommit(DB_TXN *tid) {
        int ret = txn_commit(tid, 0);

        if (ret) {
                lprintf(1, "cdb_*: txn_commit: %s\n", db_strerror(ret));
		abort();
	}

        return ret;
}

/* are you sensing a pattern yet? */
static int txbegin(DB_TXN **tid) {
        int ret = txn_begin(dbenv, NULL, tid, 0);

        if (ret) {
                lprintf(1, "cdb_*: txn_begin: %s\n", db_strerror(ret));
		abort();
	}

        return ret;
}

static void release_handles(void *arg) {
	if (arg != NULL) {
		struct cdbtsd *tsd = (struct cdbtsd *)arg;

		if (tsd->cursor != NULL) {
			lprintf(1, "cdb_*: WARNING: cursor still in progress; "
				"closing!\n");
			tsd->cursor->c_close(tsd->cursor);
		}

		if (tsd->tid != NULL) {
			lprintf(1, "cdb_*: ERROR: transaction still in progress; "
				"aborting!\n");
			txabort(tsd->tid);
		}
	}
}

static void dest_tsd(void *arg) {
	if (arg != NULL) {
		release_handles(arg);
		phree(arg);
	}
}

/*
 * Ensure that we have a key for thread-specific data.  We don't
 * put anything in here that Citadel cares about; this is just database
 * related stuff like cursors and transactions.
 *
 * This should be called immediately after startup by any thread which wants
 * to use database calls, except for whatever thread calls open_databases.
 */
void cdb_allocate_tsd(void) {
	struct cdbtsd *tsd = mallok(sizeof *tsd);

	tsd->tid = NULL;
	tsd->cursor = NULL;
	pthread_setspecific(tsdkey, tsd);
}

void cdb_free_tsd(void) {
	dest_tsd(pthread_getspecific(tsdkey));
	pthread_setspecific(tsdkey, NULL);
}

void cdb_release_handles(void) {
	release_handles(pthread_getspecific(tsdkey));
}


/*
 * Reclaim unused space in the databases.  We need to do each one of
 * these discretely, rather than in a loop.
 *
 * This is a stub function in the Sleepycat DB backend, because there is no
 * such API call available.
 */
void defrag_databases(void)
{
	/* do nothing */
}



/*
 * Request a checkpoint of the database.
 */
static void cdb_checkpoint(void) {
	int ret;

	ret = txn_checkpoint(dbenv,
				MAX_CHECKPOINT_KBYTES,
				MAX_CHECKPOINT_MINUTES,
				0);
	if (ret) {
		lprintf(1, "cdb_checkpoint: txn_checkpoint: %s\n", db_strerror(ret));
		abort();
	}
}

/*
 * Open the various databases we'll be using.  Any database which
 * does not exist should be created.  Note that we don't need an S_DATABASE
 * critical section here, because there aren't any active threads manipulating
 * the database yet -- and besides, it causes problems on BSDI.
 */
void open_databases(void)
{
	int ret;
	int i;
	char dbfilename[SIZ];
	u_int32_t flags = 0;

	lprintf(9, "cdb_*: open_databases() starting\n");
        /*
         * Silently try to create the database subdirectory.  If it's
         * already there, no problem.
         */
        system("exec mkdir data 2>/dev/null");

	lprintf(9, "cdb_*: Setting up DB environment\n");
	db_env_set_func_yield(sched_yield);
	ret = db_env_create(&dbenv, 0);
	if (ret) {
		lprintf(1, "cdb_*: db_env_create: %s\n", db_strerror(ret));
		exit(ret);
	}
	dbenv->set_errpfx(dbenv, "citserver");

        /*
         * We want to specify the shared memory buffer pool cachesize,
         * but everything else is the default.
         */
        ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0);
	if (ret) {
		lprintf(1, "cdb_*: set_cachesize: %s\n", db_strerror(ret));
                dbenv->close(dbenv, 0);
                exit(ret);
        }

	if ((ret = dbenv->set_lk_detect(dbenv, DB_LOCK_DEFAULT))) {
		lprintf(1, "cdb_*: set_lk_detect: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		exit(ret);
	}

        flags = DB_CREATE|DB_RECOVER|DB_INIT_MPOOL|DB_PRIVATE|DB_INIT_TXN|
		DB_INIT_LOCK|DB_THREAD;
        ret = dbenv->open(dbenv, "./data", flags, 0);
	if (ret) {
		lprintf(1, "cdb_*: dbenv->open: %s\n", db_strerror(ret));
                dbenv->close(dbenv, 0);
                exit(ret);
        }

	lprintf(7, "cdb_*: Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], dbenv, 0);
		if (ret) {
			lprintf(1, "cdb_*: db_create: %s\n", db_strerror(ret));
			exit(ret);
		}


		/* Arbitrary names for our tables -- we reference them by
		 * number, so we don't have string names for them.
		 */
		sprintf(dbfilename, "cdb.%02x", i);

		ret = dbp[i]->open(dbp[i],
				dbfilename,
				NULL,
				DB_BTREE,
				DB_CREATE|DB_THREAD,
				0600);
		if (ret) {
			lprintf(1, "cdb_*: db_open[%d]: %s\n", i, db_strerror(ret));
			exit(ret);
		}
	}

	if ((ret = pthread_key_create(&tsdkey, dest_tsd))) {
		lprintf(1, "cdb_*: pthread_key_create: %s\n", strerror(ret));
		exit(1);
	}

	cdb_allocate_tsd();
	CtdlRegisterSessionHook(cdb_checkpoint, EVT_TIMER);
	lprintf(9, "cdb_*: open_databases() finished\n");
}


/*
 * Close all of the db database files we've opened.  This can be done
 * in a loop, since it's just a bunch of closes.
 */
void close_databases(void)
{
	int a;
	int ret;

	cdb_free_tsd();

	if ((ret = txn_checkpoint(dbenv, 0, 0, 0))) {
		lprintf(1, "cdb_*: txn_checkpoint: %s\n", db_strerror(ret));
		abort();
	}

	for (a = 0; a < MAXCDB; ++a) {
		lprintf(7, "cdb_*: Closing database %d\n", a);
		ret = dbp[a]->close(dbp[a], 0);
		if (ret) {
			lprintf(1, "cdb_*: db_close: %s\n", db_strerror(ret));
			abort();
		}
		
	}

        /* Close the handle. */
        ret = dbenv->close(dbenv, 0);
	if (ret) {
                lprintf(1, "cdb_*: DBENV->close: %s\n", db_strerror(ret));
		abort();
        }
}

/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * key already exists it should be overwritten.
 */
int cdb_store(int cdb,
	      void *ckey, int ckeylen,
	      void *cdata, int cdatalen)
{

	DBT dkey, ddata;
	DB_TXN *tid;
	int ret;

	memset(&dkey, 0, sizeof(DBT));
	memset(&ddata, 0, sizeof(DBT));
	dkey.size = ckeylen;
	dkey.data = ckey;
	ddata.size = cdatalen;
	ddata.data = cdata;

	if (MYTID != NULL) {
		ret = dbp[cdb]->put(dbp[cdb],		/* db */
					MYTID,		/* transaction ID */
					&dkey,		/* key */
					&ddata,		/* data */
					0);		/* flags */
		if (ret) {
			lprintf(1, "cdb_store(%d): %s\n", cdb,
				db_strerror(ret));
			abort();
		}
		return ret;
	} else {
	    retry:
		if (txbegin(&tid))
			return -1;

		if ((ret = dbp[cdb]->put(dbp[cdb],	/* db */
					 tid,		/* transaction ID */
					 &dkey,		/* key */
					 &ddata,	/* data */
					 0))) {		/* flags */
			if (ret == DB_LOCK_DEADLOCK) {
				if (txabort(tid))
					return ret;
				else
					goto retry;
			} else {
				lprintf(1, "cdb_store(%d): %s\n", cdb,
					db_strerror(ret));
				abort();
			}
		} else {
			return txcommit(tid);
		}
	}
}


/*
 * Delete a piece of data.  Returns 0 if the operation was successful.
 */
int cdb_delete(int cdb, void *key, int keylen)
{

	DBT dkey;
	DB_TXN *tid;
	int ret;

	memset(&dkey, 0, sizeof dkey);
	dkey.size = keylen;
	dkey.data = key;

	if (MYTID != NULL) {
		ret = dbp[cdb]->del(dbp[cdb], MYTID, &dkey, 0);
		return (ret);
	} else {
	    retry:
		if (txbegin(&tid))
			return -1;

		if ((ret = dbp[cdb]->del(dbp[cdb], tid, &dkey, 0))) {
			if (ret == DB_LOCK_DEADLOCK) {
					if (txabort(tid))
						return ret;
					else
						goto retry;
			} else {
				lprintf(1, "cdb_delete(%d): %s\n", cdb,
					db_strerror(ret));
				abort();
			}
		} else {
			return txcommit(tid);
		}
	}
}




/*
 * Fetch a piece of data.  If not found, returns NULL.  Otherwise, it returns
 * a struct cdbdata which it is the caller's responsibility to free later on
 * using the cdb_free() routine.
 */
struct cdbdata *cdb_fetch(int cdb, void *key, int keylen)
{

	struct cdbdata *tempcdb;
	DBT dkey, dret;
	DB_TXN *tid;
	int ret;

	memset(&dkey, 0, sizeof(DBT));
	memset(&dret, 0, sizeof(DBT));
	dkey.size = keylen;
	dkey.data = key;
	dret.flags = DB_DBT_MALLOC;

	if (MYTID != NULL) {
		ret = dbp[cdb]->get(dbp[cdb], MYTID, &dkey, &dret, 0);
	} else {
	    retry:
		if (txbegin(&tid))
			return NULL;

		ret = dbp[cdb]->get(dbp[cdb], tid, &dkey, &dret, 0);

		if (ret == DB_LOCK_DEADLOCK) {
			if (txabort(tid))
				return NULL;
			else
				goto retry;
		} else if (txcommit(tid))
			return NULL;
	}

	if ((ret != 0) && (ret != DB_NOTFOUND)) {
		lprintf(1, "cdb_fetch: %s\n", db_strerror(ret));
		abort();
	}
	if (ret != 0) return NULL;
	tempcdb = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	if (tempcdb == NULL) {
		lprintf(2, "cdb_fetch: Cannot allocate memory!\n");
		abort();
	}
	tempcdb->len = dret.size;
	tempcdb->ptr = dret.data;
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
 * Prepare for a sequential search of an entire database.
 * (There is guaranteed to be no more than one traversal in
 * progress per thread at any given time.)
 */
void cdb_rewind(int cdb)
{
	int ret = 0;

	if (MYCURSOR != NULL)
		MYCURSOR->c_close(MYCURSOR);

	if (MYTID == NULL) {
		lprintf(1, "cdb_rewind: ERROR: cursor use outside transaction\n");
		abort();
	}

	/*
	 * Now initialize the cursor
	 */
	ret = dbp[cdb]->cursor(dbp[cdb], MYTID, &MYCURSOR, 0);
	if (ret) {
		lprintf(1, "cdb_rewind: db_cursor: %s\n", db_strerror(ret));
		abort();
	}
}


/*
 * Fetch the next item in a sequential search.  Returns a pointer to a 
 * cdbdata structure, or NULL if we've hit the end.
 */
struct cdbdata *cdb_next_item(int cdb)
{
	DBT key, data;
	struct cdbdata *cdbret;
	int ret = 0;

        /* Initialize the key/data pair so the flags aren't set. */
        memset(&key, 0, sizeof(key));
        memset(&data, 0, sizeof(data));
	data.flags = DB_DBT_MALLOC;

	ret = MYCURSOR->c_get(MYCURSOR,
		&key, &data, DB_NEXT);
	
	if (ret) {
		MYCURSOR->c_close(MYCURSOR);
		MYCURSOR = NULL;
		return NULL;		/* presumably, end of file */
	}

	cdbret = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	cdbret->len = data.size;
	cdbret->ptr = data.data;

	return (cdbret);
}


/*
 * Transaction-based stuff.  I'm writing this as I bake cookies...
 */

void cdb_begin_transaction(void) {

	if (MYTID != NULL) {	/* FIXME this slows it down, take it out */
		lprintf(1, "cdb_begin_transaction: ERROR: opening a new transaction with one already open!\n");
		abort();
	}
	else {
		txbegin(&MYTID);
	}
}

void cdb_end_transaction(void) {
	if (MYCURSOR != NULL) {
		lprintf(1, "cdb_end_transaction: WARNING: cursor still open at transaction end\n");
		MYCURSOR->c_close(MYCURSOR);
		MYCURSOR = NULL;
	}
	if (MYTID == NULL) {
		lprintf(1, "cdb_end_transaction: ERROR: txcommit(NULL) !!\n");
		abort();
	} else
		txcommit(MYTID);

	MYTID = NULL;
}

