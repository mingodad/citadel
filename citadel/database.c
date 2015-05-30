/*
 * This is a data store backend for the Citadel server which uses Berkeley DB.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <stdio.h>
#include <dirent.h>
#include <zlib.h>

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

#include "ctdl_module.h"
#include "control.h"
#include "citserver.h"
#include "config.h"


static DB *dbp[MAXCDB];		/* One DB handle for each Citadel database */
static DB_ENV *dbenv;		/* The DB environment (global) */


void cdb_abort(void) {
	syslog(LOG_DEBUG,
		"citserver is stopping in order to prevent data loss. uid=%d gid=%d euid=%d egid=%d",
		getuid(),
		getgid(),
		geteuid(),
		getegid()
	);
	cit_backtrace();
	exit(CTDLEXIT_DB);
}


/* Verbose logging callback */
void cdb_verbose_log(const DB_ENV *dbenv, const char *msg)
{
	if (!IsEmptyStr(msg)) {
		syslog(LOG_DEBUG, "DB: %s", msg);
		cit_backtrace();
	}
}


/* Verbose logging callback */
void cdb_verbose_err(const DB_ENV *dbenv, const char *errpfx, const char *msg)
{
	int *FOO = NULL;
	syslog(LOG_ALERT, "DB: %s", msg);
	cit_backtrace();
	*FOO = 1;
}


/* just a little helper function */
static void txabort(DB_TXN * tid)
{
	int ret;

	ret = tid->abort(tid);

	if (ret) {
		syslog(LOG_EMERG, "bdb(): txn_abort: %s", db_strerror(ret));
		cdb_abort();
	}
}

/* this one is even more helpful than the last. */
static void txcommit(DB_TXN * tid)
{
	int ret;

	ret = tid->commit(tid, 0);

	if (ret) {
		syslog(LOG_EMERG, "bdb(): txn_commit: %s", db_strerror(ret));
		cdb_abort();
	}
}

/* are you sensing a pattern yet? */
static void txbegin(DB_TXN ** tid)
{
	int ret;

	ret = dbenv->txn_begin(dbenv, NULL, tid, 0);

	if (ret) {
		syslog(LOG_EMERG, "bdb(): txn_begin: %s", db_strerror(ret));
		cdb_abort();
	}
}

static void dbpanic(DB_ENV * env, int errval)
{
	syslog(LOG_EMERG, "bdb(): PANIC: %s", db_strerror(errval));
	cit_backtrace();
}

static void cclose(DBC * cursor)
{
	int ret;

	if ((ret = cursor->c_close(cursor))) {
		syslog(LOG_EMERG, "bdb(): c_close: %s", db_strerror(ret));
		cdb_abort();
	}
}

static void bailIfCursor(DBC ** cursors, const char *msg)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (cursors[i] != NULL) {
			syslog(LOG_EMERG, "bdb(): cursor still in progress on cdb %02x: %s", i, msg);
			cdb_abort();
		}
}


void cdb_check_handles(void)
{
	bailIfCursor(TSD->cursors, "in check_handles");

	if (TSD->tid != NULL) {
		syslog(LOG_EMERG, "bdb(): transaction still in progress!");
		cdb_abort();
	}
}


/*
 * Cull the database logs
 */
void cdb_cull_logs(void)
{
	u_int32_t flags;
	int ret;
	char **file, **list;
	char errmsg[SIZ];

	flags = DB_ARCH_ABS;

	/* Get the list of names. */
	if ((ret = dbenv->log_archive(dbenv, &list, flags)) != 0) {
		syslog(LOG_ERR, "cdb_cull_logs: %s", db_strerror(ret));
		return;
	}

	/* Print the list of names. */
	if (list != NULL) {
		for (file = list; *file != NULL; ++file) {
			syslog(LOG_DEBUG, "Deleting log: %s", *file);
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
 * Request a checkpoint of the database.  Called once per minute by the thread manager.
 */
void cdb_checkpoint(void)
{
	int ret;

	syslog(LOG_DEBUG, "-- db checkpoint --");
	ret = dbenv->txn_checkpoint(dbenv, MAX_CHECKPOINT_KBYTES, MAX_CHECKPOINT_MINUTES, 0);

	if (ret != 0) {
		syslog(LOG_EMERG, "cdb_checkpoint: txn_checkpoint: %s", db_strerror(ret));
		cdb_abort();
	}

	/* After a successful checkpoint, we can cull the unused logs */
	if (CtdlGetConfigInt("c_auto_cull")) {
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

	syslog(LOG_DEBUG, "bdb(): open_databases() starting");
	syslog(LOG_DEBUG, "Compiled db: %s", DB_VERSION_STRING);
	syslog(LOG_INFO, "  Linked db: %s", db_version(&dbversion_major, &dbversion_minor, &dbversion_patch));
	syslog(LOG_INFO, "Linked zlib: %s\n", zlibVersion());

	/*
	 * Silently try to create the database subdirectory.  If it's
	 * already there, no problem.
	 */
	if ((mkdir(ctdl_data_dir, 0700) != 0) && (errno != EEXIST)){
		syslog(LOG_EMERG, 
			      "unable to create database directory [%s]: %s", 
			      ctdl_data_dir, strerror(errno));
	}
	if (chmod(ctdl_data_dir, 0700) != 0){
		syslog(LOG_EMERG, 
			      "unable to set database directory accessrights [%s]: %s", 
			      ctdl_data_dir, strerror(errno));
	}
	if (chown(ctdl_data_dir, CTDLUID, (-1)) != 0){
		syslog(LOG_EMERG, 
			      "unable to set the owner for [%s]: %s", 
			      ctdl_data_dir, strerror(errno));
	}
	syslog(LOG_DEBUG, "bdb(): Setting up DB environment\n");
	/* db_env_set_func_yield((int (*)(u_long,  u_long))sched_yield); */
	ret = db_env_create(&dbenv, 0);
	if (ret) {
		syslog(LOG_EMERG, "bdb(): db_env_create: %s\n", db_strerror(ret));
		syslog(LOG_EMERG, "exit code %d\n", ret);
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
		syslog(LOG_EMERG, "bdb(): set_cachesize: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		syslog(LOG_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	if ((ret = dbenv->set_lk_detect(dbenv, DB_LOCK_DEFAULT))) {
		syslog(LOG_EMERG, "bdb(): set_lk_detect: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		syslog(LOG_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	flags = DB_CREATE | DB_INIT_MPOOL | DB_PRIVATE | DB_INIT_TXN | DB_INIT_LOCK | DB_THREAD | DB_RECOVER;
	syslog(LOG_DEBUG, "dbenv->open(dbenv, %s, %d, 0)\n", ctdl_data_dir, flags);
	ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	if (ret == DB_RUNRECOVERY) {
		syslog(LOG_ALERT, "dbenv->open: %s\n", db_strerror(ret));
		syslog(LOG_ALERT, "Attempting recovery...\n");
		flags |= DB_RECOVER;
		ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	}
	if (ret == DB_RUNRECOVERY) {
		syslog(LOG_ALERT, "dbenv->open: %s\n", db_strerror(ret));
		syslog(LOG_ALERT, "Attempting catastrophic recovery...\n");
		flags &= ~DB_RECOVER;
		flags |= DB_RECOVER_FATAL;
		ret = dbenv->open(dbenv, ctdl_data_dir, flags, 0);
	}
	if (ret) {
		syslog(LOG_EMERG, "dbenv->open: %s\n", db_strerror(ret));
		dbenv->close(dbenv, 0);
		syslog(LOG_EMERG, "exit code %d\n", ret);
		exit(CTDLEXIT_DB);
	}

	syslog(LOG_INFO, "Starting up DB\n");

	for (i = 0; i < MAXCDB; ++i) {

		/* Create a database handle */
		ret = db_create(&dbp[i], dbenv, 0);
		if (ret) {
			syslog(LOG_EMERG, "db_create: %s\n", db_strerror(ret));
			syslog(LOG_EMERG, "exit code %d\n", ret);
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
			syslog(LOG_EMERG, "db_open[%02x]: %s\n", i, db_strerror(ret));
			if (ret == ENOMEM) {
				syslog(LOG_EMERG, "You may need to tune your database; please read http://www.citadel.org/doku.php?id=faq:troubleshooting:out_of_lock_entries for more information.");
			}
			syslog(LOG_EMERG, "exit code %d\n", ret);
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
				syslog(LOG_DEBUG, "chmod(%s, 0600) returned %d\n",
					filename, chmod(filename, 0600)
				);
				syslog(LOG_DEBUG, "chown(%s, CTDLUID, -1) returned %d\n",
					filename, chown(filename, CTDLUID, (-1))
				);
			}
		}
		closedir(dp);
	}

	syslog(LOG_DEBUG, "open_databases() finished\n");
}


/*
 * Close all of the db database files we've opened.  This can be done
 * in a loop, since it's just a bunch of closes.
 */
void close_databases(void)
{
	int a;
	int ret;

	if ((ret = dbenv->txn_checkpoint(dbenv, 0, 0, 0))) {
		syslog(LOG_EMERG,
			"txn_checkpoint: %s\n", db_strerror(ret));
	}

	/* print some statistics... */
#ifdef DB_STAT_ALL
	dbenv->lock_stat_print(dbenv, DB_STAT_ALL);
#endif

	/* close the tables */
	for (a = 0; a < MAXCDB; ++a) {
		syslog(LOG_INFO, "Closing database %02x\n", a);
		ret = dbp[a]->close(dbp[a], 0);
		if (ret) {
			syslog(LOG_EMERG, "db_close: %s\n", db_strerror(ret));
		}

	}

	/* Close the handle. */
	ret = dbenv->close(dbenv, 0);
	if (ret) {
		syslog(LOG_EMERG, "DBENV->close: %s\n", db_strerror(ret));
	}
}


/*
 * Decompress a database item if it was compressed on disk
 */
void cdb_decompress_if_necessary(struct cdbdata *cdb)
{
	static int magic = COMPRESS_MAGIC;

	if ((cdb == NULL) || 
	    (cdb->ptr == NULL) || 
	    (cdb->len < sizeof(magic)) ||
	    (memcmp(cdb->ptr, &magic, sizeof(magic))))
	    return;

	/* At this point we know we're looking at a compressed item. */

	struct CtdlCompressHeader zheader;
	char *uncompressed_data;
	char *compressed_data;
	uLongf destLen, sourceLen;
	size_t cplen;

	memset(&zheader, 0, sizeof(struct CtdlCompressHeader));
	cplen = sizeof(struct CtdlCompressHeader);
	if (sizeof(struct CtdlCompressHeader) > cdb->len)
		cplen = cdb->len;
	memcpy(&zheader, cdb->ptr, cplen);

	compressed_data = cdb->ptr;
	compressed_data += sizeof(struct CtdlCompressHeader);

	sourceLen = (uLongf) zheader.compressed_len;
	destLen = (uLongf) zheader.uncompressed_len;
	uncompressed_data = malloc(zheader.uncompressed_len);

	if (uncompress((Bytef *) uncompressed_data,
		       (uLongf *) & destLen,
		       (const Bytef *) compressed_data,
		       (uLong) sourceLen) != Z_OK) {
		syslog(LOG_EMERG, "uncompress() error\n");
		cdb_abort();
	}

	free(cdb->ptr);
	cdb->len = (size_t) destLen;
	cdb->ptr = uncompressed_data;
}



/*
 * Store a piece of data.  Returns 0 if the operation was successful.  If a
 * key already exists it should be overwritten.
 */
int cdb_store(int cdb, const void *ckey, int ckeylen, void *cdata, int cdatalen)
{

	DBT dkey, ddata;
	DB_TXN *tid;
	int ret = 0;

	struct CtdlCompressHeader zheader;
	char *compressed_data = NULL;
	int compressing = 0;
	size_t buffer_len = 0;
	uLongf destLen = 0;

	memset(&dkey, 0, sizeof(DBT));
	memset(&ddata, 0, sizeof(DBT));
	dkey.size = ckeylen;
	/* no, we don't care for this error. */
	dkey.data = ckey;

	ddata.size = cdatalen;
	ddata.data = cdata;

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
			syslog(LOG_EMERG, "compress2() error\n");
			cdb_abort();
		}
		zheader.compressed_len = (size_t) destLen;
		memcpy(compressed_data, &zheader, sizeof(struct CtdlCompressHeader));
		ddata.size = (size_t) (sizeof(struct CtdlCompressHeader) + zheader.compressed_len);
		ddata.data = compressed_data;
	}

	if (TSD->tid != NULL) {
		ret = dbp[cdb]->put(dbp[cdb],	/* db */
				    TSD->tid,	/* transaction ID */
				    &dkey,	/* key */
				    &ddata,	/* data */
				    0);	/* flags */
		if (ret) {
			syslog(LOG_EMERG, "cdb_store(%d): %s", cdb, db_strerror(ret));
			cdb_abort();
		}
		if (compressing)
			free(compressed_data);
		return ret;

	} else {
		bailIfCursor(TSD->cursors, "attempt to write during r/o cursor");

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
				syslog(LOG_EMERG, "cdb_store(%d): %s", cdb, db_strerror(ret));
				cdb_abort();
			}
		} else {
			txcommit(tid);
			if (compressing) {
				free(compressed_data);
			}
			return ret;
		}
	}
	return ret;
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

	if (TSD->tid != NULL) {
		ret = dbp[cdb]->del(dbp[cdb], TSD->tid, &dkey, 0);
		if (ret) {
			syslog(LOG_EMERG, "cdb_delete(%d): %s\n", cdb, db_strerror(ret));
			if (ret != DB_NOTFOUND) {
				cdb_abort();
			}
		}
	} else {
		bailIfCursor(TSD->cursors, "attempt to delete during r/o cursor");

	      retry:
		txbegin(&tid);

		if ((ret = dbp[cdb]->del(dbp[cdb], tid, &dkey, 0))
		    && ret != DB_NOTFOUND) {
			if (ret == DB_LOCK_DEADLOCK) {
				txabort(tid);
				goto retry;
			} else {
				syslog(LOG_EMERG, "cdb_delete(%d): %s\n",
					cdb, db_strerror(ret));
				cdb_abort();
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

	if (TSD->cursors[cdb] == NULL)
		ret = dbp[cdb]->cursor(dbp[cdb], TSD->tid, &curs, 0);
	else
		ret = TSD->cursors[cdb]->c_dup(TSD->cursors[cdb], &curs, DB_POSITION);

	if (ret) {
		syslog(LOG_EMERG, "localcursor: %s\n", db_strerror(ret));
		cdb_abort();
	}

	return curs;
}


/*
 * Fetch a piece of data.  If not found, returns NULL.  Otherwise, it returns
 * a struct cdbdata which it is the caller's responsibility to free later on
 * using the cdb_free() routine.
 */
struct cdbdata *cdb_fetch(int cdb, const void *key, int keylen)
{
	struct cdbdata *tempcdb;
	DBT dkey, dret;
	int ret;

	memset(&dkey, 0, sizeof(DBT));
	dkey.size = keylen;
	/* no we don't care about this error. */
	dkey.data = key;

	if (TSD->tid != NULL) {
		memset(&dret, 0, sizeof(DBT));
		dret.flags = DB_DBT_MALLOC;
		ret = dbp[cdb]->get(dbp[cdb], TSD->tid, &dkey, &dret, 0);
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
		syslog(LOG_EMERG, "cdb_fetch(%d): %s\n", cdb, db_strerror(ret));
		cdb_abort();
	}

	if (ret != 0)
		return NULL;
	tempcdb = (struct cdbdata *) malloc(sizeof(struct cdbdata));

	if (tempcdb == NULL) {
		syslog(LOG_EMERG, "cdb_fetch: Cannot allocate memory for tempcdb\n");
		cdb_abort();
		return NULL; /* make it easier for static analysis... */
	}
	else
	{
		tempcdb->len = dret.size;
		tempcdb->ptr = dret.data;
		cdb_decompress_if_necessary(tempcdb);
		return (tempcdb);
	}
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
	if (TSD->cursors[cdb] != NULL) {
		cclose(TSD->cursors[cdb]);
	}

	TSD->cursors[cdb] = NULL;
}

/* 
 * Prepare for a sequential search of an entire database.
 * (There is guaranteed to be no more than one traversal in
 * progress per thread at any given time.)
 */
void cdb_rewind(int cdb)
{
	int ret = 0;

	if (TSD->cursors[cdb] != NULL) {
		syslog(LOG_EMERG,
		       "cdb_rewind: must close cursor on database %d before reopening.\n", cdb);
		cdb_abort();
		/* cclose(TSD->cursors[cdb]); */
	}

	/*
	 * Now initialize the cursor
	 */
	ret = dbp[cdb]->cursor(dbp[cdb], TSD->tid, &TSD->cursors[cdb], 0);
	if (ret) {
		syslog(LOG_EMERG, "cdb_rewind: db_cursor: %s\n", db_strerror(ret));
		cdb_abort();
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

	ret = TSD->cursors[cdb]->c_get(TSD->cursors[cdb], &key, &data, DB_NEXT);

	if (ret) {
		if (ret != DB_NOTFOUND) {
			syslog(LOG_EMERG, "cdb_next_item(%d): %s\n", cdb, db_strerror(ret));
			cdb_abort();
		}
		cdb_close_cursor(cdb);
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

	bailIfCursor(TSD->cursors, "can't begin transaction during r/o cursor");

	if (TSD->tid != NULL) {
		syslog(LOG_EMERG, "cdb_begin_transaction: ERROR: nested transaction\n");
		cdb_abort();
	}

	txbegin(&TSD->tid);
}

void cdb_end_transaction(void)
{
	int i;

	for (i = 0; i < MAXCDB; i++)
		if (TSD->cursors[i] != NULL) {
			syslog(LOG_WARNING,
				"cdb_end_transaction: WARNING: cursor %d still open at transaction end\n",
				i);
			cclose(TSD->cursors[i]);
			TSD->cursors[i] = NULL;
		}

	if (TSD->tid == NULL) {
		syslog(LOG_EMERG,
			"cdb_end_transaction: ERROR: txcommit(NULL) !!\n");
		cdb_abort();
	} else {
		txcommit(TSD->tid);
	}

	TSD->tid = NULL;
}

/*
 * Truncate (delete every record)
 */
void cdb_trunc(int cdb)
{
	/* DB_TXN *tid; */
	int ret;
	u_int32_t count;

	if (TSD->tid != NULL) {
		syslog(LOG_EMERG, "cdb_trunc must not be called in a transaction.");
		cdb_abort();
	} else {
		bailIfCursor(TSD->cursors, "attempt to write during r/o cursor");

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
				syslog(LOG_EMERG, "cdb_truncate(%d): %s\n", cdb, db_strerror(ret));
				if (ret == ENOMEM) {
					syslog(LOG_EMERG, "You may need to tune your database; please read http://www.citadel.org/doku.php?id=faq:troubleshooting:out_of_lock_entries for more information.");
				}
				exit(CTDLEXIT_DB);
			}
		} else {
			/* txcommit(tid); */
		}
	}
}

int SeentDebugEnabled = 0;

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (SeentDebugEnabled != 0))
#define SEENM_syslog(LEVEL, FORMAT)					\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "%s[%ld]CC[%ld] SEEN[%s][%d] " FORMAT,	\
			     IOSTR, ioid, ccid, Facility, cType)

#define SEEN_syslog(LEVEL, FORMAT, ...)					\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "%s[%ld]CC[%ld] SEEN[%s][%d] " FORMAT,	\
			     IOSTR, ioid, ccid, Facility, cType,	\
			     __VA_ARGS__)

time_t CheckIfAlreadySeen(const char *Facility,
			  StrBuf *guid,
			  time_t now,
			  time_t antiexpire,
			  eCheckType cType,
			  long ccid,
			  long ioid)
{
	time_t InDBTimeStamp = 0;
	struct UseTable ut;
	struct cdbdata *cdbut;

	if (cType != eWrite)
	{
		SEEN_syslog(LOG_DEBUG, "Loading [%s]", ChrPtr(guid));
		cdbut = cdb_fetch(CDB_USETABLE, SKEY(guid));
		if ((cdbut != NULL) && (cdbut->ptr != NULL)) {
			memcpy(&ut, cdbut->ptr,
			       ((cdbut->len > sizeof(struct UseTable)) ?
				sizeof(struct UseTable) : cdbut->len));
			InDBTimeStamp = now - ut.ut_timestamp;

			if (InDBTimeStamp < antiexpire)
			{
				SEEN_syslog(LOG_DEBUG, "Found - Not expired %ld < %ld", InDBTimeStamp, antiexpire);
				cdb_free(cdbut);
				return InDBTimeStamp;
			}
			else
			{
				SEEN_syslog(LOG_DEBUG, "Found - Expired. %ld >= %ld", InDBTimeStamp, antiexpire);
				cdb_free(cdbut);
			}
		}
		else
		{
			if (cdbut) cdb_free(cdbut);

			SEENM_syslog(LOG_DEBUG, "not Found");
		}

		if (cType == eCheckExist)
			return InDBTimeStamp;
	}

	memcpy(ut.ut_msgid, SKEY(guid));
	ut.ut_timestamp = now;

	SEENM_syslog(LOG_DEBUG, "Saving new Timestamp");
	/* rewrite the record anyway, to update the timestamp */
	cdb_store(CDB_USETABLE,
		  SKEY(guid),
		  &ut, sizeof(struct UseTable) );

	SEENM_syslog(LOG_DEBUG, "Done Saving");
	return InDBTimeStamp;
}


void cmd_rsen(char *argbuf) {
	char Token[SIZ];
	long TLen;
	char Time[SIZ];

	struct UseTable ut;
	struct cdbdata *cdbut;
	
	if (CtdlAccessCheck(ac_aide)) return;

	TLen = extract_token(Token, argbuf, 1, '|', sizeof Token);
	if (strncmp(argbuf, "GET", 3) == 0) {
		cdbut = cdb_fetch(CDB_USETABLE, Token, TLen);
		if (cdbut != NULL) {
			memcpy(&ut, cdbut->ptr,
			       ((cdbut->len > sizeof(struct UseTable)) ?
				sizeof(struct UseTable) : cdbut->len));
			
			cprintf("%d %ld\n", CIT_OK, ut.ut_timestamp);
		}
		else {
			cprintf("%d not found\n", ERROR + NOT_HERE);
		}

	}
	else if (strncmp(argbuf, "SET", 3) == 0) {
		memcpy(ut.ut_msgid, Token, TLen);
		extract_token(Time, argbuf, 2, '|', sizeof Time);
		ut.ut_timestamp = atol(Time);
		cdb_store(CDB_USETABLE,
			  Token, TLen,
			  &ut, sizeof(struct UseTable) );
		cprintf("%d token updated\n", CIT_OK);
	}
	else if (strncmp(argbuf, "DEL", 3) == 0) {
		if (cdb_delete(CDB_USETABLE, Token, TLen))
			cprintf("%d not found\n", ERROR + NOT_HERE);
		else
			cprintf("%d deleted.\n", CIT_OK);

	}
	else {
		cprintf("%d Usage: [GET|SET|DEL]|Token|timestamp\n", ERROR);
	}

}
void LogDebugEnableSeenEnable(const int n)
{
	SeentDebugEnabled = n;
}

CTDL_MODULE_INIT(database)
{
	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("SeenDebug"), LogDebugEnableSeenEnable, &SeentDebugEnabled);
		CtdlRegisterProtoHook(cmd_rsen, "RSEN", "manipulate Aggregators seen database");
	}

	/* return our module id for the log */
 	return "database";
}
