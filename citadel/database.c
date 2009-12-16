/*
 * $Id$
 *
 * This is a data store backend for the Citadel server which uses Berkeley DB.
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*****************************************************************************
       Tunable configuration parameters for the Berkeley DB back end
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


#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "database.h"
#include "msgbase.h"
#include "sysdep_decls.h"
#include "threads.h"
#include "config.h"
#include "control.h"

#include "ctdl_module.h"


static DB *dbp[MAXCDB];		/* One DB handle for each Citadel database */
static DB_ENV *dbenv;		/* The DB environment (global) */


#ifdef HAVE_ZLIB
#include <zlib.h>
#endif


/* Verbose logging callback */
void cdb_verbose_log(const DB_ENV *dbenv, const char *msg)
{
	if (!IsEmptyStr(msg)) {
		CtdlLogPrintf(CTDL_DEBUG, "DB: %s\n", msg);
	}
}


/* Verbose logging callback */
void cdb_verbose_err(const DB_ENV *dbenv, const char *errpfx, const char *msg)
{
	CtdlLogPrintf(CTDL_ALERT, "DB: %s\n", msg);
}


/* just a little helper function */
static void txabort(DB_TXN * tid)
{
	int ret;

	ret = tid->abort(tid);

	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): txn_abort: %s\n", db_strerror(ret));
		abort();
	}
}

/* this one is even more helpful than the last. */
static void txcommit(DB_TXN * tid)
{
	int ret;

	ret = tid->commit(tid, 0);

	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): txn_commit: %s\n", db_strerror(ret));
		abort();
	}
}

/* are you sensing a pattern yet? */
static void txbegin(DB_TXN ** tid)
{
	int ret;

	ret = dbenv->txn_begin(dbenv, NULL, tid, 0);

	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): txn_begin: %s\n", db_strerror(ret));
		abort();
	}
}

static void dbpanic(DB_ENV * env, int errval)
{
	CtdlLogPrintf(CTDL_EMERG, "bdb(): PANIC: %s\n", db_strerror(errval));
}

static void cclose(DBC * cursor)
{
	int ret;

	if ((ret = cursor->c_close(cursor))) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): c_close: %s\n", db_strerror(ret));
		abort();
	}
}

static void bailIfCursor(DBC ** cursors, const char *msg)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (cursors[i] != NULL) {
			CtdlLogPrintf(CTDL_EMERG,
				"bdb(): cursor still in progress on cdb %02x: %s\n", i, msg);
			abort();
		}
}

void check_handles(void *arg)
{
	if (arg != NULL) {
		ThreadTSD *tsd = (ThreadTSD *) arg;

		bailIfCursor(tsd->cursors, "in check_handles");

		if (tsd->tid != NULL) {
			CtdlLogPrintf(CTDL_EMERG, "bdb(): transaction still in progress!");
			abort();
		}
	}
}

void cdb_check_handles(void)
{
	check_handles(pthread_getspecific(ThreadKey));
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
		CtdlLogPrintf(CTDL_ERR, "cdb_cull_logs: %s\n", db_strerror(ret));
		return;
	}

	/* Print the list of names. */
	if (list != NULL) {
		for (file = list; *file != NULL; ++file) {
			CtdlLogPrintf(CTDL_DEBUG, "Deleting log: %s\n", *file);
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
				CtdlAideMessage(errmsg, "Database Warning Message");
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
 * Request a checkpoint of the database.  Called once per minute by the thread manager.
 */
void cdb_checkpoint(void)
{
	int ret;

	CtdlLogPrintf(CTDL_DEBUG, "-- db checkpoint --\n");
	ret = dbenv->txn_checkpoint(dbenv, MAX_CHECKPOINT_KBYTES, MAX_CHECKPOINT_MINUTES, 0);

	if (ret != 0) {
		CtdlLogPrintf(CTDL_EMERG, "cdb_checkpoint: txn_checkpoint: %s\n", db_strerror(ret));
		abort();
	}

	/* After a successful checkpoint, we can cull the unused logs */
	if (config.c_auto_cull) {
		cdb_cull_logs();
	}
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
	char dbfilename[32];
	u_int32_t flags = 0;
	int dbversion_major, dbversion_minor, dbversion_patch;
	int current_dbversion = 0;

	CtdlLogPrintf(CTDL_DEBUG, "bdb(): open_databases() starting\n");
	CtdlLogPrintf(CTDL_DEBUG, "Compiled db: %s\n", DB_VERSION_STRING);
	CtdlLogPrintf(CTDL_INFO, "  Linked db: %s\n",
		db_version(&dbversion_major, &dbversion_minor, &dbversion_patch));

	current_dbversion = (dbversion_major * 1000000) + (dbversion_minor * 1000) + dbversion_patch;

	CtdlLogPrintf(CTDL_DEBUG, "Calculated dbversion: %d\n", current_dbversion);
	CtdlLogPrintf(CTDL_DEBUG, "  Previous dbversion: %d\n", CitControl.MMdbversion);

	if ( (getenv("SUPPRESS_DBVERSION_CHECK") == NULL)
	   && (CitControl.MMdbversion > current_dbversion) ) {
		CtdlLogPrintf(CTDL_EMERG, "You are attempting to run the Citadel server using a version\n"
					"of Berkeley DB that is older than that which last created or\n"
					"updated the database.  Because this would probably cause data\n"
					"corruption or loss, the server is aborting execution now.\n");
		exit(CTDLEXIT_DB);
	}

	CitControl.MMdbversion = current_dbversion;
	put_control();

#ifdef HAVE_ZLIB
	CtdlLogPrintf(CTDL_INFO, "Linked zlib: %s\n", zlibVersion());
#endif

	/*
	 * Silently try to create the database subdirectory.  If it's
	 * already there, no problem.
	 */
	mkdir(ctdl_data_dir, 0700);
	chmod(ctdl_data_dir, 0700);
	chown(ctdl_data_dir, CTDLUID, (-1));

	CtdlLogPrintf(CTDL_DEBUG, "bdb(): Setting up DB environment\n");
	db_env_set_func_yield(sched_yield);
	ret = db_env_create(&dbenv, 0);
	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): db_env_create: %s\n", db_strerror(ret));
		CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}
	dbenv->set_errpfx(dbenv, "citserver");
	dbenv->set_paniccall(dbenv, dbpanic);
	dbenv->set_errcall(dbenv, cdb_verbose_err);
	dbenv->set_errpfx(dbenv, "ctdl");
#if (DB_VERSION_MAJOR == 4) && (DB_VERSION_MINOR >= 3)
	dbenv->set_msgcall(dbenv, cdb_verbose_log);
#endif
	dbenv->set_verbose(dbenv, DB_VERB_DEADLOCK, 1);
	dbenv->set_verbose(dbenv, DB_VERB_RECOVERY, 1);

	/*
	 * We want to specify the shared memory buffer pool cachesize,
	 * but everything else is the default.
	 */
	ret = dbenv->set_cachesize(dbenv, 0, 64 * 1024, 0);
	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): set_cachesize: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	if ((ret = dbenv->set_lk_detect(dbenv, DB_LOCK_DEFAULT))) {
		CtdlLogPrintf(CTDL_EMERG, "bdb(): set_lk_detect: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	flags = DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_INIT_TXN | DB_INIT_LOCK | DB_THREAD | DB_RECOVER;
	CtdlLogPrintf(CTDL_DEBUG, "dbenv->open(dbenv, %s, %d, 0)\n", ctdl_data_dir, flags);
	ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	if (ret == DB_RUNRECOVERY) {
		CtdlLogPrintf(CTDL_ALERT, "dbenv->open: %s\n", db_strerror(ret));
		CtdlLogPrintf(CTDL_ALERT, "Attempting recovery...\n");
		flags |= DB_RECOVER;
		ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	}
	if (ret == DB_RUNRECOVERY) {
		CtdlLogPrintf(CTDL_ALERT, "dbenv->open: %s\n", db_strerror(ret));
		CtdlLogPrintf(CTDL_ALERT, "Attempting catastrophic recovery...\n");
		flags &= ~DB_RECOVER;
		flags |= DB_RECOVER_FATAL;
		ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	}
	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "dbenv->open: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	CtdlLogPrintf(CTDL_INFO, "Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], dbenv, 0);
		if (ret) {
			CtdlLogPrintf(CTDL_EMERG, "db_create: %s\n", db_strerror(ret));
			CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
			exit(CTDLEXIT_DB);
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
				   0600
		);
		if (ret) {
			CtdlLogPrintf(CTDL_EMERG, "db_open[%02x]: %s\n", i, db_strerror(ret));
			if (ret == ENOMEM) {
				CtdlLogPrintf(CTDL_EMERG, "You may need to tune your database; please read http://www.citadel.org/doku.php/faq:troubleshooting:out_of_lock_entries for more information.\n");
			}
			CtdlLogPrintf(CTDL_EMERG, "exit code %d\n", ret);
			exit(CTDLEXIT_DB);
		}
	}

}


/* Make sure we own all the files, because in a few milliseconds
 * we're going to drop root privs.
 */
void cdb_chmod_data(void) {
	DIR *dp;
	struct dirent *d;
	char filename[PATH_MAX];

	dp = opendir(ctdl_data_dir);
	if (dp != NULL) {
		while (d = readdir(dp), d != NULL) {
			if (d->d_name[0] != '.') {
				snprintf(filename, sizeof filename,
					 "%s/%s", ctdl_data_dir, d->d_name);
				CtdlLogPrintf(9, "chmod(%s, 0600) returned %d\n",
					filename, chmod(filename, 0600)
				);
				CtdlLogPrintf(9, "chown(%s, CTDLUID, -1) returned %d\n",
					filename, chown(filename, CTDLUID, (-1))
				);
			}
		}
		closedir(dp);
	}

	CtdlLogPrintf(CTDL_DEBUG, "open_databases() finished\n");
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

	ctdl_thread_internal_free_tsd();
	
	if ((ret = dbenv->txn_checkpoint(dbenv, 0, 0, 0))) {
		CtdlLogPrintf(CTDL_EMERG,
			"txn_checkpoint: %s\n", db_strerror(ret));
	}

	/* print some statistics... */
#ifdef DB_STAT_ALL
	dbenv->lock_stat_print(dbenv, DB_STAT_ALL);
#endif

	/* close the tables */
	for (a = 0; a < MAXCDB; ++a) {
		CtdlLogPrintf(CTDL_INFO, "Closing database %02x\n", a);
		ret = dbp[a]->close(dbp[a], 0);
		if (ret) {
			CtdlLogPrintf(CTDL_EMERG, "db_close: %s\n", db_strerror(ret));
		}

	}

	/* Close the handle. */
	ret = dbenv->close(dbenv, 0);
	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "DBENV->close: %s\n", db_strerror(ret));
	}
}


/*
 * Compression functions only used if we have zlib
 */
void cdb_decompress_if_necessary(struct cdbdata *cdb)
{
	static int magic = COMPRESS_MAGIC;

	if (cdb == NULL)
		return;
	if (cdb->ptr == NULL)
		return;
	if (memcmp(cdb->ptr, &magic, sizeof(magic)))
		return;

#ifdef HAVE_ZLIB
	/* At this point we know we're looking at a compressed item. */

	struct CtdlCompressHeader zheader;
	char *uncompressed_data;
	char *compressed_data;
	uLongf destLen, sourceLen;

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
		CtdlLogPrintf(CTDL_EMERG, "uncompress() error\n");
		abort();
	}

	free(cdb->ptr);
	cdb->len = (size_t) destLen;
	cdb->ptr = uncompressed_data;
#else				/* HAVE_ZLIB */
	CtdlLogPrintf(CTDL_EMERG, "Database contains compressed data, but this citserver was built without compression support.\n");
	abort();
#endif				/* HAVE_ZLIB */
}



/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * key already exists it should be overwritten.
 */
int cdb_store(int cdb, void *ckey, int ckeylen, void *cdata, int cdatalen)
{

	DBT dkey, ddata;
	DB_TXN *tid;
	int ret = 0;

#ifdef HAVE_ZLIB
	struct CtdlCompressHeader zheader;
	char *compressed_data = NULL;
	int compressing = 0;
	size_t buffer_len = 0;
	uLongf destLen = 0;
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
		if (compress2((Bytef *) (compressed_data + sizeof(struct CtdlCompressHeader)),
			&destLen, (Bytef *) cdata, (uLongf) cdatalen, 1) != Z_OK)
		{
			CtdlLogPrintf(CTDL_EMERG, "compress2() error\n");
			abort();
		}
		zheader.compressed_len = (size_t) destLen;
		memcpy(compressed_data, &zheader, sizeof(struct CtdlCompressHeader));
		ddata.size = (size_t) (sizeof(struct CtdlCompressHeader) + zheader.compressed_len);
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
			CtdlLogPrintf(CTDL_EMERG, "cdb_store(%d): %s\n", cdb, db_strerror(ret));
			abort();
		}
#ifdef HAVE_ZLIB
		if (compressing)
			free(compressed_data);
#endif
		return ret;

	} else {
		bailIfCursor(MYCURSORS, "attempt to write during r/o cursor");

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
				CtdlLogPrintf(CTDL_EMERG, "cdb_store(%d): %s\n",
					cdb, db_strerror(ret));
				abort();
			}
		} else {
			txcommit(tid);
#ifdef HAVE_ZLIB
			if (compressing) {
				free(compressed_data);
			}
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
			CtdlLogPrintf(CTDL_EMERG, "cdb_delete(%d): %s\n", cdb, db_strerror(ret));
			if (ret != DB_NOTFOUND) {
				abort();
			}
		}
	} else {
		bailIfCursor(MYCURSORS, "attempt to delete during r/o cursor");

	      retry:
		txbegin(&tid);

		if ((ret = dbp[cdb]->del(dbp[cdb], tid, &dkey, 0))
		    && ret != DB_NOTFOUND) {
			if (ret == DB_LOCK_DEADLOCK) {
				txabort(tid);
				goto retry;
			} else {
				CtdlLogPrintf(CTDL_EMERG, "cdb_delete(%d): %s\n",
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
		CtdlLogPrintf(CTDL_EMERG, "localcursor: %s\n", db_strerror(ret));
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
		CtdlLogPrintf(CTDL_EMERG, "cdb_fetch(%d): %s\n", cdb, db_strerror(ret));
		abort();
	}

	if (ret != 0)
		return NULL;
	tempcdb = (struct cdbdata *) malloc(sizeof(struct cdbdata));

	if (tempcdb == NULL) {
		CtdlLogPrintf(CTDL_EMERG, "cdb_fetch: Cannot allocate memory for tempcdb\n");
		abort();
	}

	tempcdb->len = dret.size;
	tempcdb->ptr = dret.data;
	cdb_decompress_if_necessary(tempcdb);
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
	if (MYCURSORS[cdb] != NULL) {
		cclose(MYCURSORS[cdb]);
	}

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
		CtdlLogPrintf(CTDL_EMERG,
			"cdb_rewind: must close cursor on database %d before reopening.\n", cdb);
		abort();
		/* cclose(MYCURSORS[cdb]); */
	}

	/*
	 * Now initialize the cursor
	 */
	ret = dbp[cdb]->cursor(dbp[cdb], MYTID, &MYCURSORS[cdb], 0);
	if (ret) {
		CtdlLogPrintf(CTDL_EMERG, "cdb_rewind: db_cursor: %s\n", db_strerror(ret));
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
			CtdlLogPrintf(CTDL_EMERG, "cdb_next_item(%d): %s\n", cdb, db_strerror(ret));
			abort();
		}
		cclose(MYCURSORS[cdb]);
		MYCURSORS[cdb] = NULL;
		return NULL;	/* presumably, end of file */
	}

	cdbret = (struct cdbdata *) malloc(sizeof(struct cdbdata));
	cdbret->len = data.size;
	cdbret->ptr = data.data;
	cdb_decompress_if_necessary(cdbret);

	return (cdbret);
}



/*
 * Transaction-based stuff.  I'm writing this as I bake cookies...
 */

void cdb_begin_transaction(void)
{

	bailIfCursor(MYCURSORS, "can't begin transaction during r/o cursor");

	if (MYTID != NULL) {
		CtdlLogPrintf(CTDL_EMERG, "cdb_begin_transaction: ERROR: nested transaction\n");
		abort();
	}

	txbegin(&MYTID);
}

void cdb_end_transaction(void)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (MYCURSORS[i] != NULL) {
			CtdlLogPrintf(CTDL_WARNING,
				"cdb_end_transaction: WARNING: cursor %d still open at transaction end\n",
				i);
			cclose(MYCURSORS[i]);
			MYCURSORS[i] = NULL;
		}

	if (MYTID == NULL) {
		CtdlLogPrintf(CTDL_EMERG,
			"cdb_end_transaction: ERROR: txcommit(NULL) !!\n");
		abort();
	} else {
		txcommit(MYTID);
	}

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
		CtdlLogPrintf(CTDL_EMERG,
			"cdb_trunc must not be called in a transaction.\n");
		abort();
	} else {
		bailIfCursor(MYCURSORS, "attempt to write during r/o cursor");

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
				CtdlLogPrintf(CTDL_EMERG, "cdb_truncate(%d): %s\n", cdb, db_strerror(ret));
				if (ret == ENOMEM) {
					CtdlLogPrintf(CTDL_EMERG, "You may need to tune your database; please read http://www.citadel.org/doku.php/faq:troubleshooting:out_of_lock_entries for more information.\n");
				}
				abort();
			}
		} else {
			/* txcommit(tid); */
		}
	}
}
