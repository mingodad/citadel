/*
 * $Id$
 *
 * Sleepycat (Berkeley) DB driver for Citadel/UX
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <db.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "database.h"
#include "sysdep_decls.h"


/*
 * This array holds one DB handle for each Citadel database.
 */
DB *dbp[MAXCDB];

DB_ENV *dbenv;

DBC **cursorz = NULL;
int num_cursorz = 0;
#define MYCURSOR cursorz[CC->cs_pid]

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
 * Open the various databases we'll be using.  Any database which
 * does not exist should be created.  Note that we don't need an S_DATABASE
 * critical section here, because there aren't any active threads manipulating
 * the database yet -- and besides, it causes problems on BSDI.
 */
void open_databases(void)
{
	int ret;
	int i;
	char dbfilename[256];

        /*
         * Silently try to create the database subdirectory.  If it's
         * already there, no problem.
         */
        system("exec mkdir data 2>/dev/null");

	lprintf(9, "Setting up DB environment\n");
	ret = db_env_create(&dbenv, 0);
	if (ret) {
		lprintf(1, "db_env_create: %s\n", db_strerror(ret));
		exit(ret);
	}
	dbenv->set_errpfx(dbenv, "citserver");

        /*
         * We want to specify the shared memory buffer pool cachesize,
         * but everything else is the default.
         */
        ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0);
	if (ret) {
		lprintf(1, "set_cachesize: %s\n", db_strerror(ret));
                dbenv->close(dbenv, 0);
                exit(ret);
        }

        /*
         * We have multiple processes reading/writing these files, so
         * we need concurrency control and a shared buffer pool, but
         * not logging or transactions.
         */
        /* (void)dbenv->set_data_dir(dbenv, "/database/files"); */
        ret = dbenv->open(dbenv, "./data",
        	( DB_CREATE | DB_INIT_MPOOL ),
		0);
	if (ret) {
		lprintf(1, "dbenv->open: %s\n", db_strerror(ret));
                dbenv->close(dbenv, 0);
                exit(ret);
        }

	lprintf(7, "Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], dbenv, 0);
		if (ret) {
			lprintf(1, "db_create: %s\n", db_strerror(ret));
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
				DB_CREATE,
				0600);
		if (ret) {
			lprintf(1, "db_open[%d]: %s\n", i, db_strerror(ret));
			exit(ret);
		}

	}

}


/*
 * Close all of the gdbm database files we've opened.  This can be done
 * in a loop, since it's just a bunch of closes.
 */
void close_databases(void)
{
	int a;
	int ret;

	begin_critical_section(S_DATABASE);
	for (a = 0; a < MAXCDB; ++a) {
		lprintf(7, "Closing database %d\n", a);
		ret = dbp[a]->close(dbp[a], 0);
		if (ret) {
			lprintf(1, "db_close: %s\n", db_strerror(ret));
		}
		
	}



        /* Close the handle. */
        ret = dbenv->close(dbenv, 0);
	if (ret) {
                lprintf(1, "DBENV->close: %s\n", db_strerror(ret));
        }


	end_critical_section(S_DATABASE);

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
	int ret;

	memset(&dkey, 0, sizeof(DBT));
	memset(&ddata, 0, sizeof(DBT));
	dkey.size = ckeylen;
	dkey.data = ckey;
	ddata.size = cdatalen;
	ddata.data = cdata;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->put(dbp[cdb],		/* db */
				NULL,		/* transaction ID (hmm...) */
				&dkey,		/* key */
				&ddata,		/* data */
				0);		/* flags */
	end_critical_section(S_DATABASE);
	if (ret) {
		lprintf(1, "cdb_store: %s\n", db_strerror(ret));
		return (-1);
	}
	return (0);
}


/*
 * Delete a piece of data.  Returns 0 if the operation was successful.
 */
int cdb_delete(int cdb, void *key, int keylen)
{

	DBT dkey;
	int ret;

	dkey.size = keylen;
	dkey.data = key;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->del(dbp[cdb], NULL, &dkey, 0);
	end_critical_section(S_DATABASE);
	return (ret);

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
	int ret;

	memset(&dkey, 0, sizeof(DBT));
	memset(&dret, 0, sizeof(DBT));
	dkey.size = keylen;
	dkey.data = key;
	dret.flags = DB_DBT_MALLOC;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->get(dbp[cdb], NULL, &dkey, &dret, 0);
	end_critical_section(S_DATABASE);
	if ((ret != 0) && (ret != DB_NOTFOUND)) {
		lprintf(1, "cdb_fetch: %s\n", db_strerror(ret));
	}
	if (ret != 0) return NULL;
	tempcdb = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	if (tempcdb == NULL) {
		lprintf(2, "Cannot allocate memory!\n");
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
 * progress per session at any given time.)
 */
void cdb_rewind(int cdb)
{
	int ret = 0;

	/*
	 * Make sure we have a cursor allocated for this session
	 */

	if (num_cursorz <= CC->cs_pid) {
		num_cursorz = CC->cs_pid + 1;
		if (cursorz == NULL) {
			cursorz = (DBC **)
			    mallok((sizeof(DBC *) * num_cursorz));
		} else {
			cursorz = (DBC **)
			    reallok(cursorz, (sizeof(DBC *) * num_cursorz));
		}
	}


	/*
	 * Now initialize the cursor
	 */
	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->cursor(dbp[cdb], NULL, &MYCURSOR, 0);
	if (ret) {
		lprintf(1, "db_cursor: %s\n", db_strerror(ret));
	}
	end_critical_section(S_DATABASE);
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

	begin_critical_section(S_DATABASE);
	ret = MYCURSOR->c_get(MYCURSOR,
		&key, &data, DB_NEXT);
	end_critical_section(S_DATABASE);
	
	if (ret) return NULL;		/* presumably, end of file */

	cdbret = (struct cdbdata *) mallok(sizeof(struct cdbdata));
	cdbret->len = data.size;
	cdbret->ptr = data.data;

	return (cdbret);
}
