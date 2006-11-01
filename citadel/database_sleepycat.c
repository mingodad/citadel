/*
 * $Id$
 *
 * Sleepycat (Berkeley) DB driver for Citadel
 *
 */

/*****************************************************************************
       Tunable configuration parameters for the Sleepycat DB back end
 *****************************************************************************/

/* Citadel will checkpoint the db at the end of every session, but only if
 * the specified number of kilobytes has been written, or if the specified
 * number of minutes has passed, since the last checkpoint.
 */
#define MAX_CHECKPOINT_KBYTES	256
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
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_DB_H
#include <db.h>
#elif defined(HAVE_DB4_DB_H)
#include <db4/db.h>
#else
#error Neither <db.h> nor <db4/db.h> was found by configure. Install db4-devel.
#endif


#if DB_VERSION_MAJOR < 4 || DB_VERSION_MINOR < 1
#error Citadel requires Berkeley DB v4.1 or newer.  Please upgrade.
#endif


#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "citserver.h"
#include "database.h"
#include "msgbase.h"
#include "sysdep_decls.h"
#include "config.h"

static DB *dbp[MAXCDB];		/* One DB handle for each Citadel database */
static DB_ENV *dbenv;		/* The DB environment (global) */

struct cdbtsd {			/* Thread-specific DB stuff */
	DB_TXN *tid;		/* Transaction handle */
	DBC *cursors[MAXCDB];	/* Cursors, for traversals... */
};

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

static pthread_key_t tsdkey;

#define MYCURSORS	(((struct cdbtsd*)pthread_getspecific(tsdkey))->cursors)
#define MYTID		(((struct cdbtsd*)pthread_getspecific(tsdkey))->tid)

/* just a little helper function */
static void txabort(DB_TXN * tid)
{
	int ret;

	ret = tid->abort(tid);

	if (ret) {
		lprintf(CTDL_EMERG, "cdb_*: txn_abort: %s\n",
			db_strerror(ret));
		abort();
	}
}

/* this one is even more helpful than the last. */
static void txcommit(DB_TXN * tid)
{
	int ret;

	ret = tid->commit(tid, 0);

	if (ret) {
		lprintf(CTDL_EMERG, "cdb_*: txn_commit: %s\n",
			db_strerror(ret));
		abort();
	}
}

/* are you sensing a pattern yet? */
static void txbegin(DB_TXN ** tid)
{
	int ret;

	ret = dbenv->txn_begin(dbenv, NULL, tid, 0);

	if (ret) {
		lprintf(CTDL_EMERG, "cdb_*: txn_begin: %s\n",
			db_strerror(ret));
		abort();
	}
}

static void dbpanic(DB_ENV * env, int errval)
{
	lprintf(CTDL_EMERG, "cdb_*: Berkeley DB panic: %d\n", errval);
}

static void cclose(DBC * cursor)
{
	int ret;

	if ((ret = cursor->c_close(cursor))) {
		lprintf(CTDL_EMERG, "cdb_*: c_close: %s\n",
			db_strerror(ret));
		abort();
	}
}

static void bailIfCursor(DBC ** cursors, const char *msg)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (cursors[i] != NULL) {
			lprintf(CTDL_EMERG,
				"cdb_*: cursor still in progress on cdb %d: %s\n",
				i, msg);
			abort();
		}
}

static void check_handles(void *arg)
{
	if (arg != NULL) {
		struct cdbtsd *tsd = (struct cdbtsd *) arg;

		bailIfCursor(tsd->cursors, "in check_handles");

		if (tsd->tid != NULL) {
			lprintf(CTDL_EMERG,
				"cdb_*: transaction still in progress!");
			abort();
		}
	}
}

static void dest_tsd(void *arg)
{
	if (arg != NULL) {
		check_handles(arg);
		free(arg);
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
void cdb_allocate_tsd(void)
{
	struct cdbtsd *tsd;

	if (pthread_getspecific(tsdkey) != NULL)
		return;

	tsd = malloc(sizeof(struct cdbtsd));

	tsd->tid = NULL;

	memset(tsd->cursors, 0, sizeof tsd->cursors);
	pthread_setspecific(tsdkey, tsd);
}

void cdb_free_tsd(void)
{
	dest_tsd(pthread_getspecific(tsdkey));
	pthread_setspecific(tsdkey, NULL);
}

void cdb_check_handles(void)
{
	check_handles(pthread_getspecific(tsdkey));
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
 * Cull the database logs
 */
static void cdb_cull_logs(void)
{
	u_int32_t flags;
	int ret;
	char **file, **list;
	char errmsg[SIZ];

	flags = DB_ARCH_ABS;

	/* Get the list of names. */
	if ((ret = dbenv->log_archive(dbenv, &list, flags)) != 0) {
		lprintf(CTDL_ERR, "cdb_cull_logs: %s\n", db_strerror(ret));
		return;
	}

	/* Print the list of names. */
	if (list != NULL) {
		for (file = list; *file != NULL; ++file) {
			lprintf(CTDL_DEBUG, "Deleting log: %s\n", *file);
			ret = unlink(*file);
			if (ret != 0) {
				snprintf(errmsg, sizeof(errmsg),
					 " ** ERROR **\n \n \n "
					 "Citadel was unable to delete the "
					 "database log file '%s' because of the "
					 "following error:\n \n %s\n \n"
					 " This log file is no longer in use "
					 "and may be safely deleted.\n",
					 *file, strerror(errno));
				aide_message(errmsg, "Database Warning Message");
			}
		}
		free(list);
	}
}

/*
 * Manually initiate log file cull.
 */
void cmd_cull(char *argbuf) {
	if (CtdlAccessCheck(ac_internal)) return;
	cdb_cull_logs();
	cprintf("%d Database log file cull completed.\n", CIT_OK);
}


/*
 * Request a checkpoint of the database.
 */
static void cdb_checkpoint(void)
{
	int ret;
	static time_t last_run = 0L;

	/* Only do a checkpoint once per minute. */
	if ((time(NULL) - last_run) < 60L) {
		return;
	}
	last_run = time(NULL);

	lprintf(CTDL_DEBUG, "-- db checkpoint --\n");
	ret = dbenv->txn_checkpoint(dbenv,
				    MAX_CHECKPOINT_KBYTES,
				    MAX_CHECKPOINT_MINUTES, 0);

	if (ret != 0) {
		lprintf(CTDL_EMERG, "cdb_checkpoint: txn_checkpoint: %s\n",
			db_strerror(ret));
		abort();
	}

	/* After a successful checkpoint, we can cull the unused logs */
	if (config.c_auto_cull) {
		cdb_cull_logs();
	}
}


/*
 * Main loop for the checkpoint thread.
 */
void *checkpoint_thread(void *arg) {
	struct CitContext checkpointCC;

	lprintf(CTDL_DEBUG, "checkpoint_thread() initializing\n");

	memset(&checkpointCC, 0, sizeof(struct CitContext));
	checkpointCC.internal_pgm = 1;
	checkpointCC.cs_pid = 0;
	pthread_setspecific(MyConKey, (void *)&checkpointCC );

	cdb_allocate_tsd();

	while (!time_to_die) {
		cdb_checkpoint();
		sleep(1);
	}

	lprintf(CTDL_DEBUG, "checkpoint_thread() exiting\n");
	pthread_exit(NULL);
}

/*
 * Open the various databases we'll be using.  Any database which
 * does not exist should be created.  Note that we don't need a
 * critical section here, because there aren't any active threads
 * manipulating the database yet.
 */
void open_databases(void)
{
	int ret;
	int i;
	char dbfilename[SIZ];
	u_int32_t flags = 0;
	DIR *dp;
	struct dirent *d;
	char filename[PATH_MAX];

	lprintf(CTDL_DEBUG, "cdb_*: open_databases() starting\n");
	lprintf(CTDL_DEBUG, "Compiled db: %s\n", DB_VERSION_STRING);
	lprintf(CTDL_INFO, "  Linked db: %s\n",
		db_version(NULL, NULL, NULL));
#ifdef HAVE_ZLIB
	lprintf(CTDL_INFO, "Linked zlib: %s\n", zlibVersion());
#endif

	/*
	 * Silently try to create the database subdirectory.  If it's
	 * already there, no problem.
	 */
	mkdir(ctdl_data_dir, 0700);
	chmod(ctdl_data_dir, 0700);
	chown(ctdl_data_dir, CTDLUID, (-1));

	lprintf(CTDL_DEBUG, "cdb_*: Setting up DB environment\n");
	db_env_set_func_yield(sched_yield);
	ret = db_env_create(&dbenv, 0);
	if (ret) {
		lprintf(CTDL_EMERG, "cdb_*: db_env_create: %s\n",
			db_strerror(ret));
		exit(ret);
	}
	dbenv->set_errpfx(dbenv, "citserver");
	dbenv->set_paniccall(dbenv, dbpanic);

	/*
	 * We want to specify the shared memory buffer pool cachesize,
	 * but everything else is the default.
	 */
	ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0);
	if (ret) {
		lprintf(CTDL_EMERG, "cdb_*: set_cachesize: %s\n",
			db_strerror(ret));
		dbenv->close(dbenv, 0);
		exit(ret);
	}

	if ((ret = dbenv->set_lk_detect(dbenv, DB_LOCK_DEFAULT))) {
		lprintf(CTDL_EMERG, "cdb_*: set_lk_detect: %s\n",
			db_strerror(ret));
		dbenv->close(dbenv, 0);
		exit(ret);
	}

	flags =
	    DB_CREATE | DB_RECOVER | DB_INIT_MPOOL | DB_PRIVATE |
	    DB_INIT_TXN | DB_INIT_LOCK | DB_THREAD;
	lprintf(CTDL_DEBUG, "dbenv->open(dbenv, %s, %d, 0)\n", ctdl_data_dir,
		flags);
	ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	if (ret) {
		lprintf(CTDL_DEBUG, "cdb_*: dbenv->open: %s\n",
			db_strerror(ret));
		dbenv->close(dbenv, 0);
		exit(ret);
	}

	lprintf(CTDL_INFO, "cdb_*: Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], dbenv, 0);
		if (ret) {
			lprintf(CTDL_DEBUG, "cdb_*: db_create: %s\n",
				db_strerror(ret));
			exit(ret);
		}


		/* Arbitrary names for our tables -- we reference them by
		 * number, so we don't have string names for them.
		 */
		snprintf(dbfilename, sizeof dbfilename, "cdb.%02x", i);

		ret = dbp[i]->open(dbp[i],
				   NULL,
				   dbfilename,
				   NULL,
				   DB_BTREE,
				   DB_CREATE | DB_AUTO_COMMIT | DB_THREAD,
				   0600);
		if (ret) {
			lprintf(CTDL_EMERG, "cdb_*: db_open[%d]: %s\n", i,
				db_strerror(ret));
			exit(ret);
		}
	}

	if ((ret = pthread_key_create(&tsdkey, dest_tsd))) {
		lprintf(CTDL_EMERG, "cdb_*: pthread_key_create: %s\n",
			strerror(ret));
		exit(1);
	}

	cdb_allocate_tsd();

	/* Now make sure we own all the files, because in a few milliseconds
	 * we're going to drop root privs.
	 */
	dp = opendir(ctdl_data_dir);
	if (dp != NULL) {
		while (d = readdir(dp), d != NULL) {
			if (d->d_name[0] != '.') {
				snprintf(filename, sizeof filename,
					 "%s/%s", ctdl_data_dir, d->d_name);
				chmod(filename, 0600);
				chown(filename, CTDLUID, (-1));
			}
		}
		closedir(dp);
	}

	lprintf(CTDL_DEBUG, "cdb_*: open_databases() finished\n");

	CtdlRegisterProtoHook(cmd_cull, "CULL", "Cull database logs");
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

	if ((ret = dbenv->txn_checkpoint(dbenv, 0, 0, 0))) {
		lprintf(CTDL_EMERG,
			"cdb_*: txn_checkpoint: %s\n", db_strerror(ret));
	}

	for (a = 0; a < MAXCDB; ++a) {
		lprintf(CTDL_INFO, "cdb_*: Closing database %d\n", a);
		ret = dbp[a]->close(dbp[a], 0);
		if (ret) {
			lprintf(CTDL_EMERG,
				"cdb_*: db_close: %s\n", db_strerror(ret));
		}

	}

	/* Close the handle. */
	ret = dbenv->close(dbenv, 0);
	if (ret) {
		lprintf(CTDL_EMERG,
			"cdb_*: DBENV->close: %s\n", db_strerror(ret));
	}
}


/*
 * Compression functions only used if we have zlib
 */
#ifdef HAVE_ZLIB

void cdb_decompress_if_necessary(struct cdbdata *cdb)
{
	static int magic = COMPRESS_MAGIC;
	struct CtdlCompressHeader zheader;
	char *uncompressed_data;
	char *compressed_data;
	uLongf destLen, sourceLen;

	if (cdb == NULL)
		return;
	if (cdb->ptr == NULL)
		return;
	if (memcmp(cdb->ptr, &magic, sizeof(magic)))
		return;

	/* At this point we know we're looking at a compressed item. */
	memcpy(&zheader, cdb->ptr, sizeof(struct CtdlCompressHeader));

	compressed_data = cdb->ptr;
	compressed_data += sizeof(struct CtdlCompressHeader);

	sourceLen = (uLongf) zheader.compressed_len;
	destLen = (uLongf) zheader.uncompressed_len;
	uncompressed_data = malloc(zheader.uncompressed_len);

	if (uncompress((Bytef *) uncompressed_data,
		       (uLongf *) & destLen,
		       (const Bytef *) compressed_data,
		       (uLong) sourceLen) != Z_OK) {
		lprintf(CTDL_EMERG, "uncompress() error\n");
		abort();
	}

	free(cdb->ptr);
	cdb->len = (size_t) destLen;
	cdb->ptr = uncompressed_data;
}

#endif				/* HAVE_ZLIB */


/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * key already exists it should be overwritten.
 */
int cdb_store(int cdb, void *ckey, int ckeylen, void *cdata, int cdatalen)
{

	DBT dkey, ddata;
	DB_TXN *tid;
	int ret;

#ifdef HAVE_ZLIB
	struct CtdlCompressHeader zheader;
	char *compressed_data = NULL;
	int compressing = 0;
	size_t buffer_len;
	uLongf destLen;
#endif

	memset(&dkey, 0, sizeof(DBT));
	memset(&ddata, 0, sizeof(DBT));
	dkey.size = ckeylen;
	dkey.data = ckey;
	ddata.size = cdatalen;
	ddata.data = cdata;

#ifdef HAVE_ZLIB
	/* Only compress Visit records.  Everything else is uncompressed. */
	if (cdb == CDB_VISIT) {
		compressing = 1;
		zheader.magic = COMPRESS_MAGIC;
		zheader.uncompressed_len = cdatalen;
		buffer_len = ((cdatalen * 101) / 100) + 100
		    + sizeof(struct CtdlCompressHeader);
		destLen = (uLongf) buffer_len;
		compressed_data = malloc(buffer_len);
		if (compress2((Bytef *) (compressed_data +
					 sizeof(struct
						CtdlCompressHeader)),
			      &destLen, (Bytef *) cdata, (uLongf) cdatalen,
			      1) != Z_OK) {
			lprintf(CTDL_EMERG, "compress2() error\n");
			abort();
		}
		zheader.compressed_len = (size_t) destLen;
		memcpy(compressed_data, &zheader,
		       sizeof(struct CtdlCompressHeader));
		ddata.size = (size_t) (sizeof(struct CtdlCompressHeader) +
				       zheader.compressed_len);
		ddata.data = compressed_data;
	}
#endif

	if (MYTID != NULL) {
		ret = dbp[cdb]->put(dbp[cdb],	/* db */
				    MYTID,	/* transaction ID */
				    &dkey,	/* key */
				    &ddata,	/* data */
				    0);	/* flags */
		if (ret) {
			lprintf(CTDL_EMERG, "cdb_store(%d): %s\n", cdb,
				db_strerror(ret));
			abort();
		}
#ifdef HAVE_ZLIB
		if (compressing)
			free(compressed_data);
#endif
		return ret;

	} else {
		bailIfCursor(MYCURSORS,
			     "attempt to write during r/o cursor");

	      retry:
		txbegin(&tid);

		if ((ret = dbp[cdb]->put(dbp[cdb],	/* db */
					 tid,	/* transaction ID */
					 &dkey,	/* key */
					 &ddata,	/* data */
					 0))) {	/* flags */
			if (ret == DB_LOCK_DEADLOCK) {
				txabort(tid);
				goto retry;
			} else {
				lprintf(CTDL_EMERG, "cdb_store(%d): %s\n",
					cdb, db_strerror(ret));
				abort();
			}
		} else {
			txcommit(tid);
#ifdef HAVE_ZLIB
			if (compressing)
				free(compressed_data);
#endif
			return ret;
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
		if (ret) {
			lprintf(CTDL_EMERG, "cdb_delete(%d): %s\n", cdb,
				db_strerror(ret));
			if (ret != DB_NOTFOUND)
				abort();
		}
	} else {
		bailIfCursor(MYCURSORS,
			     "attempt to delete during r/o cursor");

	      retry:
		txbegin(&tid);

		if ((ret = dbp[cdb]->del(dbp[cdb], tid, &dkey, 0))
		    && ret != DB_NOTFOUND) {
			if (ret == DB_LOCK_DEADLOCK) {
				txabort(tid);
				goto retry;
			} else {
				lprintf(CTDL_EMERG, "cdb_delete(%d): %s\n",
					cdb, db_strerror(ret));
				abort();
			}
		} else {
			txcommit(tid);
		}
	}
	return ret;
}

static DBC *localcursor(int cdb)
{
	int ret;
	DBC *curs;

	if (MYCURSORS[cdb] == NULL)
		ret = dbp[cdb]->cursor(dbp[cdb], MYTID, &curs, 0);
	else
		ret =
		    MYCURSORS[cdb]->c_dup(MYCURSORS[cdb], &curs,
					  DB_POSITION);

	if (ret) {
		lprintf(CTDL_EMERG, "localcursor: %s\n", db_strerror(ret));
		abort();
	}

	return curs;
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
	dkey.size = keylen;
	dkey.data = key;

	if (MYTID != NULL) {
		memset(&dret, 0, sizeof(DBT));
		dret.flags = DB_DBT_MALLOC;
		ret = dbp[cdb]->get(dbp[cdb], MYTID, &dkey, &dret, 0);
	} else {
		DBC *curs;

		do {
			memset(&dret, 0, sizeof(DBT));
			dret.flags = DB_DBT_MALLOC;

			curs = localcursor(cdb);

			ret = curs->c_get(curs, &dkey, &dret, DB_SET);
			cclose(curs);
		}
		while (ret == DB_LOCK_DEADLOCK);

	}

	if ((ret != 0) && (ret != DB_NOTFOUND)) {
		lprintf(CTDL_EMERG, "cdb_fetch(%d): %s\n", cdb,
			db_strerror(ret));
		abort();
	}

	if (ret != 0)
		return NULL;
	tempcdb = (struct cdbdata *) malloc(sizeof(struct cdbdata));

	if (tempcdb == NULL) {
		lprintf(CTDL_EMERG,
			"cdb_fetch: Cannot allocate memory for tempcdb\n");
		abort();
	}

	tempcdb->len = dret.size;
	tempcdb->ptr = dret.data;
#ifdef HAVE_ZLIB
	cdb_decompress_if_necessary(tempcdb);
#endif
	return (tempcdb);
}


/*
 * Free a cdbdata item.
 *
 * Note that we only free the 'ptr' portion if it is not NULL.  This allows
 * other code to assume ownership of that memory simply by storing the
 * pointer elsewhere and then setting 'ptr' to NULL.  cdb_free() will then
 * avoid freeing it.
 */
void cdb_free(struct cdbdata *cdb)
{
	if (cdb->ptr) {
		free(cdb->ptr);
	}
	free(cdb);
}

void cdb_close_cursor(int cdb)
{
	if (MYCURSORS[cdb] != NULL)
		cclose(MYCURSORS[cdb]);

	MYCURSORS[cdb] = NULL;
}

/* 
 * Prepare for a sequential search of an entire database.
 * (There is guaranteed to be no more than one traversal in
 * progress per thread at any given time.)
 */
void cdb_rewind(int cdb)
{
	int ret = 0;

	if (MYCURSORS[cdb] != NULL) {
		lprintf(CTDL_EMERG,
			"cdb_rewind: must close cursor on database %d before reopening.\n",
			cdb);
		abort();
		/* cclose(MYCURSORS[cdb]); */
	}

	/*
	 * Now initialize the cursor
	 */
	ret = dbp[cdb]->cursor(dbp[cdb], MYTID, &MYCURSORS[cdb], 0);
	if (ret) {
		lprintf(CTDL_EMERG, "cdb_rewind: db_cursor: %s\n",
			db_strerror(ret));
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

	ret = MYCURSORS[cdb]->c_get(MYCURSORS[cdb], &key, &data, DB_NEXT);

	if (ret) {
		if (ret != DB_NOTFOUND) {
			lprintf(CTDL_EMERG, "cdb_next_item(%d): %s\n",
				cdb, db_strerror(ret));
			abort();
		}
		cclose(MYCURSORS[cdb]);
		MYCURSORS[cdb] = NULL;
		return NULL;	/* presumably, end of file */
	}

	cdbret = (struct cdbdata *) malloc(sizeof(struct cdbdata));
	cdbret->len = data.size;
	cdbret->ptr = data.data;
#ifdef HAVE_ZLIB
	cdb_decompress_if_necessary(cdbret);
#endif

	return (cdbret);
}



/*
 * Transaction-based stuff.  I'm writing this as I bake cookies...
 */

void cdb_begin_transaction(void)
{

	bailIfCursor(MYCURSORS,
		     "can't begin transaction during r/o cursor");

	if (MYTID != NULL) {
		lprintf(CTDL_EMERG,
			"cdb_begin_transaction: ERROR: nested transaction\n");
		abort();
	}

	txbegin(&MYTID);
}

void cdb_end_transaction(void)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (MYCURSORS[i] != NULL) {
			lprintf(CTDL_WARNING,
				"cdb_end_transaction: WARNING: cursor %d still open at transaction end\n",
				i);
			cclose(MYCURSORS[i]);
			MYCURSORS[i] = NULL;
		}

	if (MYTID == NULL) {
		lprintf(CTDL_EMERG,
			"cdb_end_transaction: ERROR: txcommit(NULL) !!\n");
		abort();
	} else
		txcommit(MYTID);

	MYTID = NULL;
}

/*
 * Truncate (delete every record)
 */
void cdb_trunc(int cdb)
{
	/* DB_TXN *tid; */
	int ret;
	u_int32_t count;

	if (MYTID != NULL) {
		lprintf(CTDL_EMERG,
			"cdb_trunc must not be called in a transaction.\n");
		abort();
	} else {
		bailIfCursor(MYCURSORS,
			     "attempt to write during r/o cursor");

	      retry:
		/* txbegin(&tid); */

		if ((ret = dbp[cdb]->truncate(dbp[cdb],	/* db */
					      NULL,	/* transaction ID */
					      &count,	/* #rows deleted */
					      0))) {	/* flags */
			if (ret == DB_LOCK_DEADLOCK) {
				/* txabort(tid); */
				goto retry;
			} else {
				lprintf(CTDL_EMERG,
					"cdb_truncate(%d): %s\n", cdb,
					db_strerror(ret));
				abort();
			}
		} else {
			/* txcommit(tid); */
		}
	}
}
