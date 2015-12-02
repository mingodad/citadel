/*
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef DATABASE_H
#define DATABASE_H


void open_databases (void);
void close_databases (void);
int cdb_store (int cdb, const void *key, int keylen, void *data, int datalen);
int cdb_delete (int cdb, void *key, int keylen);
struct cdbdata *cdb_fetch (int cdb, const void *key, int keylen);
void cdb_free (struct cdbdata *cdb);
void cdb_rewind (int cdb);
struct cdbdata *cdb_next_item (int cdb);
void cdb_close_cursor(int cdb);
void cdb_begin_transaction(void);
void cdb_end_transaction(void);
void cdb_allocate_tsd(void);
void cdb_free_tsd(void);
void cdb_check_handles(void);
void cdb_trunc(int cdb);
void *checkpoint_thread(void *arg);
void cdb_chmod_data(void);
void cdb_checkpoint(void);
void check_handles(void *arg);
void cdb_cull_logs(void);


/*
 * Database records beginning with this magic number are assumed to
 * be compressed.  In the event that a database record actually begins with
 * this magic number, we *must* compress it whether we want to or not,
 * because the fetch function will try to uncompress it anyway.
 * 
 * (No need to #ifdef this stuff; it compiles ok even if zlib is not present
 * and doesn't declare anything so it won't bloat the code)
 */
#define COMPRESS_MAGIC	0xc0ffeeee

struct CtdlCompressHeader {
	int magic;
	size_t uncompressed_len;
	size_t compressed_len;
};

typedef enum __eCheckType {
	eCheckExist,   /* look up the item, return the timestamp if its there, 0 if not. */
	eCheckUpdate,  /* if it exists, refresh in db timestamp. return the timstamp if its there, 0 if not. */
	eUpdate,       /* insert/update the new value, return the old if its there, 0 if not. */
	eWrite         /* write this to DB, unconditional. */
}eCheckType;

time_t CheckIfAlreadySeen(const char *Facility,
			  StrBuf *guid,
			  time_t now,
			  time_t antiexpire,
			  eCheckType cType,
			  long ccid,
			  long ioid);


#endif /* DATABASE_H */

