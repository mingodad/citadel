struct cdbdata *cdb_fetch(int cdb, void *key, int keylen);
int cdb_store(int cdb, void *key, int keylen, void *data, int datalen);
int cdb_delete(int cdb, void *key, int keylen);
