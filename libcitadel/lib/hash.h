
typedef struct HashList HashList;

typedef struct HashKey HashKey;

typedef void (*DeleteHashDataFunc)(void * Data);

int GetHash(HashList *Hash, char *HKey, void **Payload);

void Put(HashList *Hash, char *HKey, long HKLen, void *Payload, DeleteHashDataFunc DeleteIt);

int GetKey(HashList *Hash, char *HKey, long HKLen, void **Payload);

int GetHashKeys(HashList *Hash, char **List);
