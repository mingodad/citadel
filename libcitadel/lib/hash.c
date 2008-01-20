#include "hash.h"


typedef struct HashList {
	void *Members;
	long nMembersUsed;
	long MemberSize;
	
};

typedef struct Payload {
	void *Data;
	char *HashKey;
	DeleteHashDataFunc Destructor;
};

typedef struct HashKey {
	long Key;
	long Position;
};


int GetHash(HashList *Hash, char *HKey, void **Payload)
{
}

void Put(HashList *Hash, char *HKey, long HKLen, void *Payload, DeleteHashDataFunc DeleteIt)
{
}

int GetKey(HashList *Hash, char *HKey, long HKLen, void **Payload)
{
}

int GetHashKeys(HashList *Hash, char **List)
{
}
