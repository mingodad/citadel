/* $Id$ */
void defrag_databases (void);
void open_databases (void);
void close_databases (void);
int cdb_store (int cdb, void *key, int keylen, void *data, int datalen);
int cdb_delete (int cdb, void *key, int keylen);
struct cdbdata *cdb_fetch (int cdb, void *key, int keylen);
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
