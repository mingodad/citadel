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

#define DATABASE_NAME	"citadel.db"

/*
 * This array holds one DB handle for each Citadel database.
 */
DB *dbp[MAXCDB];

DBC *MYCURSOR;	/* FIXME !! */

/*
 * Reclaim unused space in the databases.  We need to do each one of
 * these discretely, rather than in a loop.
 */
void defrag_databases(void)
{
	/* FIXME ... do we even need this?  If not, we'll just keep it as
	 * a stub function to keep the API consistent.
	 */
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

	lprintf(7, "Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], NULL, 0);
		if (ret != 0) {
			lprintf(1, "db_create: %s\n", db_strerror(ret));
			exit(ret);
		}

		/* FIXME these are tunable, so tune them */
		dbp[i]->set_pagesize(dbp[i], 1024);
		dbp[i]->set_cachesize(dbp[i], 0, 32768, 0);

		/* Arbitrary names for our tables -- we reference them by
		 * number, so we don't have string names for them.
		 */
		sprintf(dbfilename, "cdb.%02x", i);

		ret = dbp[i]->open(dbp[i],
				DATABASE_NAME,
				dbfilename,
				DB_BTREE,
				(DB_CREATE|DB_NOMMAP|DB_THREAD|DB_UPGRADE),
				0600);

		if (ret != 0) {
			lprintf(1, "db_open: %s\n", db_strerror(ret));
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
		if (ret != 0) {
			lprintf(1, "db_close: %s\n", db_strerror(ret));
		}
		
	}
	end_critical_section(S_DATABASE);

}


/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * key already exists it should be overwritten.
 */
int cdb_store(int cdb,
	      void *key, int keylen,
	      void *data, int datalen)
{

	DBT dkey, ddata;
	int ret;

	dkey.size = keylen;
	dkey.data = key;
	ddata.size = datalen;
	ddata.data = data;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->put(dbp[cdb],		/* db */
				NULL,		/* transaction ID (hmm...) */
				&dkey,		/* key */
				&ddata,		/* data */
				0);		/* flags */
	end_critical_section(S_DATABASE);
	if (ret < 0) {
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

	dkey.size = keylen;
	dkey.data = key;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->get(dbp[cdb], NULL, &dkey, &dret, 0);
	end_critical_section(S_DATABASE);
	if (ret) {
		return NULL;
	}
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
 * Prepare for a sequential search of an entire database.  (In the DB model,
 * use per-session key. There is guaranteed to be no more than one traversal in
 * progress per session at any given time.)
 */
void cdb_rewind(int cdb)
{
	int ret = 0;

	begin_critical_section(S_DATABASE);
	ret = dbp[cdb]->cursor(dbp[cdb], NULL, &MYCURSOR, 0);
	if (ret != 0) {
		lprintf(1, "db_create: %s\n", db_strerror(ret));
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
