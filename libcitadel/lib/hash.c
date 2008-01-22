#include <stdint.h>
#include <stdlib.h>
#include <string.h>
//dbg
#include <stdio.h>
#include "libcitadel.h"
#include "lookup3.h"

typedef struct Payload Payload;

struct Payload {
	void *Data;
	DeleteHashDataFunc Destructor;
};

struct HashKey {
	long Key;
	long Position;
	char *HashKey;
	long HKLen;
};

struct HashList {
	Payload **Members;
	HashKey **LookupTable;
	char **MyKeys;
	long nMembersUsed;
	long MemberSize;
};

struct HashPos {
	long Position;
};

int PrintHash(HashList *Hash)
{
	char *foo;
	char *bar;
	long key;
	long i;
	if (Hash->MyKeys != NULL)
		free (Hash->MyKeys);

	Hash->MyKeys = (char**) malloc(sizeof(char*) * Hash->nMembersUsed);
	printf("----------------------------------\n");
	for (i=0; i < Hash->nMembersUsed; i++) {
		
		if (Hash->LookupTable[i] == NULL)
		{
			foo = "";
			bar = "";
			key = 0;
		}
		else 
		{
			key = Hash->LookupTable[i]->Key;
			foo = Hash->LookupTable[i]->HashKey;
			bar = (char*) Hash->Members[Hash->LookupTable[i]->Position]->Data;
		}
		printf (" ---- Hashkey[%ld][%ld]: '%s' Value: '%s' \n", i, key, foo, bar);
	}
	printf("----------------------------------\n");
	return 0;
}



HashList *NewHash(void) 
{
	HashList *NewList;
	NewList = malloc (sizeof(HashList));
	memset(NewList, 0, sizeof(HashList));

	NewList->Members = malloc(sizeof(Payload*) * 100);
	memset(NewList->Members, 0, sizeof(Payload*) * 100);

	NewList->LookupTable = malloc(sizeof(HashKey*) * 100);
	memset(NewList->LookupTable, 0, sizeof(HashKey*) * 100);

	NewList->MemberSize = 100;

	return NewList;
}


static void DeleteHashPayload (Payload *Data)
{
	if (Data->Destructor)
		Data->Destructor(Data->Data);
	else
		free(Data->Data);
}
void DeleteHash(HashList **Hash)
{
	int i;
	HashList *FreeMe;

	FreeMe = *Hash;
	if (FreeMe == NULL)
		return;
	for (i=0; i < FreeMe->nMembersUsed; i++)
	{
		if (FreeMe->Members[i] != NULL)
		{
			DeleteHashPayload(FreeMe->Members[i]);
			free(FreeMe->Members[i]);
		}
		if (FreeMe->LookupTable[i] != NULL)
		{
			free(FreeMe->LookupTable[i]->HashKey);
			free(FreeMe->LookupTable[i]);
		}
	}
	
	free(FreeMe->LookupTable);
	free(FreeMe->Members);
	if (FreeMe->MyKeys != NULL)
		free(FreeMe->MyKeys);
		
	free (FreeMe);
	*Hash = NULL;
}

static void InsertHashItem(HashList *Hash, 
			   long HashPos, 
			   long HashBinKey, 
			   char *HashKeyStr, 
			   long HKLen, 
			   void *Data,
			   DeleteHashDataFunc Destructor)
{
	Payload *NewPayloadItem;
	HashKey *NewHashKey;

	if (Hash->nMembersUsed >= Hash->MemberSize)
	{
		/* Ok, Our space is used up. Double the available space. */
		Payload **NewPayloadArea;
		HashKey **NewTable;

		NewPayloadArea = (Payload**) malloc(sizeof(Payload*) * Hash->MemberSize * 2);
		memset(&NewPayloadArea[Hash->MemberSize], 0, sizeof(Payload*) * Hash->MemberSize);
		memcpy(NewPayloadArea, Hash->Members, sizeof(Payload*) * Hash->MemberSize);

		NewTable = malloc(sizeof(HashKey*) * Hash->MemberSize * 2);
		memset(&NewTable[Hash->MemberSize], 0, sizeof(HashKey*) * Hash->MemberSize);
		memcpy(NewTable, &Hash->LookupTable, sizeof(HashKey*) * Hash->MemberSize);

		Hash->MemberSize *= 2;
	}
	
	NewPayloadItem = (Payload*) malloc (sizeof(Payload));
	NewPayloadItem->Data = Data;
	NewPayloadItem->Destructor = Destructor;

	NewHashKey = (HashKey*) malloc (sizeof(HashKey));
	NewHashKey->HashKey = (char *) malloc (HKLen + 1);
	NewHashKey->HKLen = HKLen;
	memcpy (NewHashKey->HashKey, HashKeyStr, HKLen + 1);
	NewHashKey->Key = HashBinKey;
	NewHashKey->Position = Hash->nMembersUsed;

	if ((Hash->nMembersUsed != 0) && 
	    (HashPos != Hash->nMembersUsed) ) {
		long InsertAt;
		long ItemsAfter;

		ItemsAfter = Hash->nMembersUsed - HashPos;
		InsertAt = HashPos;

		if (ItemsAfter > 0)
		{
			memmove(&Hash->LookupTable[InsertAt + 1],
				&Hash->LookupTable[InsertAt],
				ItemsAfter * sizeof(HashKey*));
		} 
	}
		
	Hash->Members[Hash->nMembersUsed] = NewPayloadItem;
	Hash->LookupTable[HashPos] = NewHashKey;
	Hash->nMembersUsed++;
}

static long FindInHash(HashList *Hash, long HashBinKey)
{
	long SearchPos;
	long StepWidth;

	SearchPos = Hash->nMembersUsed / 2;
	StepWidth = SearchPos / 2;
	while ((SearchPos > 0) && 
	       (SearchPos < Hash->nMembersUsed)) 
	{
		/** Did we find it? */
		if (Hash->LookupTable[SearchPos]->Key == HashBinKey){
			return SearchPos;
		}
		/** are we Aproximating in big steps? */
		if (StepWidth > 1){
			if (Hash->LookupTable[SearchPos]->Key > HashBinKey)
				SearchPos -= StepWidth;
			else
				SearchPos += StepWidth;
			StepWidth /= 2;			
		}
		else { /** We are right next to our target, within 4 positions */
			if (Hash->LookupTable[SearchPos]->Key > HashBinKey) {
				if ((SearchPos > 0) && 
				    (Hash->LookupTable[SearchPos - 1]->Key < HashBinKey))
					return SearchPos;
				SearchPos --;
			}
			else {
				if ((SearchPos + 1 < Hash->nMembersUsed) && 
				    (Hash->LookupTable[SearchPos + 1]->Key > HashBinKey))
					return SearchPos;
				SearchPos ++;
			}
			StepWidth--;
		}
	}
	return SearchPos;
}



inline static long CalcHashKey (char *HKey, long HKLen)
{
	return hashlittle(HKey, HKLen, 9283457);
}


void Put(HashList *Hash, char *HKey, long HKLen, void *Data, DeleteHashDataFunc DeleteIt)
{
	long HashBinKey;
	long HashAt;

	
	HashBinKey = CalcHashKey(HKey, HKLen);
	HashAt = FindInHash(Hash, HashBinKey);

	if (Hash->LookupTable[HashAt] == NULL){
		InsertHashItem(Hash, HashAt, HashBinKey, HKey, HKLen, Data, DeleteIt);
	}
	else if (Hash->LookupTable[HashAt]->Key > HashBinKey) {
		InsertHashItem(Hash, HashAt, HashBinKey, HKey, HKLen, Data, DeleteIt);
	}
	else if (Hash->LookupTable[HashAt]->Key < HashBinKey) {
		InsertHashItem(Hash, HashAt + 1, HashBinKey, HKey, HKLen, Data, DeleteIt);		
	}
	else { /* Ok, we have a colision. replace it. */
		long PayloadPos;

		PayloadPos = Hash->LookupTable[HashAt]->Position;
		DeleteHashPayload(Hash->Members[PayloadPos]);
		Hash->Members[PayloadPos]->Data = Data;
		Hash->Members[PayloadPos]->Destructor = DeleteIt;
	}
}

int GetHash(HashList *Hash, char *HKey, long HKLen, void **Data)
{
	long HashBinKey;
	long HashAt;

	HashBinKey = CalcHashKey(HKey, HKLen);
	HashAt = FindInHash(Hash, HashBinKey);
	if ((HashAt < 0) || (HashAt >= Hash->nMembersUsed)) {
		*Data = NULL;
		return 0;
	}
	else {
		long MemberPosition;

		MemberPosition = Hash->LookupTable[HashAt]->Position;
		*Data = Hash->Members[MemberPosition]->Data;
		return 1;
	}
}

int GetKey(HashList *Hash, char *HKey, long HKLen, void **Payload)
{
	return 0;
}

int GetHashKeys(HashList *Hash, const char ***List)
{
	long i;
	if (Hash->MyKeys != NULL)
		free (Hash->MyKeys);

	Hash->MyKeys = (char**) malloc(sizeof(char*) * Hash->nMembersUsed);
	for (i=0; i < Hash->nMembersUsed; i++) {
	
		Hash->MyKeys[i] = Hash->LookupTable[i]->HashKey;
	}
	*List = Hash->MyKeys;
	return Hash->nMembersUsed;
}

HashPos *GetNewHashPos(void)
{
	HashPos *Ret;
	
	Ret = (HashPos*)malloc(sizeof(HashPos));
	Ret->Position = 0;
	return Ret;
}

void DeleteHashPos(HashPos **DelMe)
{
	free(*DelMe);
	*DelMe = NULL;
}

int GetNextHashPos(HashList *Hash, HashPos *At, long *HKLen, char **HashKey, void **Data)
{
	long PayloadPos;

	if (Hash->nMembersUsed <= At->Position)
		return 0;
	*HKLen = Hash->LookupTable[At->Position]->HKLen;
	*HashKey = Hash->LookupTable[At->Position]->HashKey;
	PayloadPos = Hash->LookupTable[At->Position]->Position;
	*Data = Hash->Members[PayloadPos]->Data;
	At->Position++;
	return 1;
}
