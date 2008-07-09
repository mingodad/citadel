#include <stdint.h>
#include <stdlib.h>
#include <string.h>
//dbg
#include <stdio.h>
#include "libcitadel.h"
#include "lookup3.h"

typedef struct Payload Payload;

struct Payload {
	/**
	 * \brief Hash Payload storage Structure; filled in linear.
	 */
	void *Data; /**< the Data belonging to this storage */
	DeleteHashDataFunc Destructor; /**< if we want to destroy Data, do it with this function. */
};

struct HashKey {
        /**
	 * \brief Hash key element; sorted by key
	 */
	long Key;         /**< Numeric Hashkey comperator for hash sorting */
	long Position;    /**< Pointer to a Payload struct in the Payload Aray */
	char *HashKey;    /**< the Plaintext Hashkey */
	long HKLen;       /**< length of the Plaintext Hashkey */
	Payload *PL;      /**< pointer to our payload for sorting */
};

struct HashList {
	/**
	 * \brief Hash structure; holds arrays of Hashkey and Payload. 
	 */
	Payload **Members;     /**< Our Payload members. This fills up linear */
	HashKey **LookupTable; /**< Hash Lookup table. Elements point to members, and are sorted by their hashvalue */
	char **MyKeys;         /**< this keeps the members for a call of GetHashKeys */
	HashFunc Algorithm;    /**< should we use an alternating algorithm to calc the hash values? */
	long nMembersUsed;     /**< how many pointers inside of the array are used? */
	long MemberSize;       /**< how big is Members and LookupTable? */
	long tainted;          /**< if 0, we're hashed, else s.b. else sorted us in his own way. */
	long uniq;             /**< are the keys going to be uniq? */
};

struct HashPos {
	/**
	 * \brief Anonymous Hash Iterator Object. used for traversing the whole array from outside 
	 */
	long Position;
};


/**
 * \brief Iterate over the hash and call PrintEntry. 
 * \param Hash your Hashlist structure
 * \param Trans is called so you could for example print 'A:' if the next entries are like that.
 *        Must be aware to receive NULL in both pointers.
 * \param PrintEntry print entry one by one
 * \returns the number of items printed
 */
int PrintHash(HashList *Hash, TransitionFunc Trans, PrintHashDataFunc PrintEntry)
{
	int i;
	void *Previous;
	void *Next;
	const char* KeyStr;

	if (Hash == NULL)
		return 0;

	for (i=0; i < Hash->nMembersUsed; i++) {
		if (i==0) {
			Previous = NULL;
		}
		else {
			if (Hash->LookupTable[i - 1] == NULL)
				Previous = NULL;
			else
				Previous = Hash->Members[Hash->LookupTable[i-1]->Position]->Data;
		}
		if (Hash->LookupTable[i] == NULL) {
			KeyStr = "";
			Next = NULL;
		}
		else {
			Next = Hash->Members[Hash->LookupTable[i]->Position]->Data;
			KeyStr = Hash->LookupTable[i]->HashKey;
		}

		Trans(Previous, Next, i % 2);
		PrintEntry(KeyStr, Next, i % 2);
	}
	return i;
}


/**
 * \brief verify the contents of a hash list; here for debugging purposes.
 * \param Hash your Hashlist structure
 * \param First Functionpointer to allow you to print your payload
 * \param Second Functionpointer to allow you to print your payload
 * \returns 0
 */
int dbg_PrintHash(HashList *Hash, PrintHashContent First, PrintHashContent Second)
{
	const char *foo;
	const char *bar;
	const char *bla = "";
	long key;
	long i;

	if (Hash == NULL)
		return 0;

	if (Hash->MyKeys != NULL)
		free (Hash->MyKeys);

	Hash->MyKeys = (char**) malloc(sizeof(char*) * Hash->nMembersUsed);
#ifdef DEBUG
	printf("----------------------------------\n");
#endif
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
			if (First != NULL)
				bar = First(Hash->Members[Hash->LookupTable[i]->Position]->Data);
			else 
				bar = "";
			if (Second != NULL)
				bla = Second(Hash->Members[Hash->LookupTable[i]->Position]->Data);
			else
				bla = "";
		}
#ifdef DEBUG
		printf (" ---- Hashkey[%ld][%ld]: '%s' Value: '%s' ; %s\n", i, key, foo, bar, bla);
#endif
	}
#ifdef DEBUG
	printf("----------------------------------\n");
#endif
	return 0;
}


/**
 * \brief instanciate a new hashlist
 * \returns the newly allocated list. 
 */
HashList *NewHash(int Uniq, HashFunc F)
{
	HashList *NewList;
	NewList = malloc (sizeof(HashList));
	memset(NewList, 0, sizeof(HashList));

	NewList->Members = malloc(sizeof(Payload*) * 100);
	memset(NewList->Members, 0, sizeof(Payload*) * 100);

	NewList->LookupTable = malloc(sizeof(HashKey*) * 100);
	memset(NewList->LookupTable, 0, sizeof(HashKey*) * 100);

	NewList->MemberSize = 100;
	NewList->tainted = 0;
        NewList->uniq = Uniq;
	NewList->Algorithm = F;

	return NewList;
}

int GetCount(HashList *Hash)
{
	if(Hash==NULL) return 0;
	return Hash->nMembersUsed;
}


/**
 * \brief private destructor for one hash element.
 * \param Data an element to free using the user provided destructor, or just plain free
 */
static void DeleteHashPayload (Payload *Data)
{
	/** do we have a destructor for our payload? */
	if (Data->Destructor)
		Data->Destructor(Data->Data);
	else
		free(Data->Data);
}

/**
 * \brief destroy a hashlist and all of its members
 * \param Hash Hash to destroy. Is NULL'ed so you are shure its done.
 */
void DeleteHash(HashList **Hash)
{
	int i;
	HashList *FreeMe;

	FreeMe = *Hash;
	if (FreeMe == NULL)
		return;
	for (i=0; i < FreeMe->nMembersUsed; i++)
	{
		/** get rid of our payload */
		if (FreeMe->Members[i] != NULL)
		{
			DeleteHashPayload(FreeMe->Members[i]);
			free(FreeMe->Members[i]);
		}
		/** delete our hashing data */
		if (FreeMe->LookupTable[i] != NULL)
		{
			free(FreeMe->LookupTable[i]->HashKey);
			free(FreeMe->LookupTable[i]);
		}
	}
	/** now, free our arrays... */
	free(FreeMe->LookupTable);
	free(FreeMe->Members);
	/** did s.b. want an array of our keys? free them. */
	if (FreeMe->MyKeys != NULL)
		free(FreeMe->MyKeys);
	/** buye bye cruel world. */	
	free (FreeMe);
	*Hash = NULL;
}

/**
 * \brief Private function to increase the hash size.
 * \param Hash the Hasharray to increase
 */
static void IncreaseHashSize(HashList *Hash)
{
	/* Ok, Our space is used up. Double the available space. */
	Payload **NewPayloadArea;
	HashKey **NewTable;
	
	if (Hash == NULL)
		return ;

	/** double our payload area */
	NewPayloadArea = (Payload**) malloc(sizeof(Payload*) * Hash->MemberSize * 2);
	memset(&NewPayloadArea[Hash->MemberSize], 0, sizeof(Payload*) * Hash->MemberSize);
	memcpy(NewPayloadArea, Hash->Members, sizeof(Payload*) * Hash->MemberSize);
	free(Hash->Members);
	Hash->Members = NewPayloadArea;
	
	/** double our hashtable area */
	NewTable = malloc(sizeof(HashKey*) * Hash->MemberSize * 2);
	memset(&NewTable[Hash->MemberSize], 0, sizeof(HashKey*) * Hash->MemberSize);
	memcpy(NewTable, Hash->LookupTable, sizeof(HashKey*) * Hash->MemberSize);
	free(Hash->LookupTable);
	Hash->LookupTable = NewTable;
	
	Hash->MemberSize *= 2;
}


/**
 * \brief private function to add a new item to / replace an existing in -  the hashlist
 * if the hash list is full, its re-alloced with double size.
 * \parame Hash our hashlist to manipulate
 * \param HashPos where should we insert / replace?
 * \param HashKeyStr the Hash-String
 * \param HKLen length of HashKeyStr
 * \param Data your Payload to add
 * \param Destructor Functionpointer to free Data. if NULL, default free() is used.
 */
static void InsertHashItem(HashList *Hash, 
			   long HashPos, 
			   long HashBinKey, 
			   const char *HashKeyStr, 
			   long HKLen, 
			   void *Data,
			   DeleteHashDataFunc Destructor)
{
	Payload *NewPayloadItem;
	HashKey *NewHashKey;

	if (Hash == NULL)
		return;

	if (Hash->nMembersUsed >= Hash->MemberSize)
		IncreaseHashSize (Hash);

	/** Arrange the payload */
	NewPayloadItem = (Payload*) malloc (sizeof(Payload));
	NewPayloadItem->Data = Data;
	NewPayloadItem->Destructor = Destructor;
	/** Arrange the hashkey */
	NewHashKey = (HashKey*) malloc (sizeof(HashKey));
	NewHashKey->HashKey = (char *) malloc (HKLen + 1);
	NewHashKey->HKLen = HKLen;
	memcpy (NewHashKey->HashKey, HashKeyStr, HKLen + 1);
	NewHashKey->Key = HashBinKey;
	NewHashKey->PL = NewPayloadItem;
	/** our payload is queued at the end... */
	NewHashKey->Position = Hash->nMembersUsed;
	/** but if we should be sorted into a specific place... */
	if ((Hash->nMembersUsed != 0) && 
	    (HashPos != Hash->nMembersUsed) ) {
		long ItemsAfter;

		ItemsAfter = Hash->nMembersUsed - HashPos;
		/** make space were we can fill us in */
		if (ItemsAfter > 0)
		{
			memmove(&Hash->LookupTable[HashPos + 1],
				&Hash->LookupTable[HashPos],
				ItemsAfter * sizeof(HashKey*));
		} 
	}
	
	Hash->Members[Hash->nMembersUsed] = NewPayloadItem;
	Hash->LookupTable[HashPos] = NewHashKey;
	Hash->nMembersUsed++;
}

/**
 * \brief if the user has tainted the hash, but wants to insert / search items by their key
 *  we need to search linear through the array. You have been warned that this will take more time!
 * \param Hash Our Hash to manipulate
 * \param HashBinKey the Hash-Number to lookup. 
 * \returns the position (most closely) matching HashBinKey (-> Caller needs to compare! )
 */
static long FindInTaintedHash(HashList *Hash, long HashBinKey)
{
	long SearchPos;

	if (Hash == NULL)
		return 0;

	for (SearchPos = 0; SearchPos < Hash->nMembersUsed; SearchPos ++) {
		if (Hash->LookupTable[SearchPos]->Key == HashBinKey){
			return SearchPos;
		}
	}
	return SearchPos;
}

/**
 * \brief Private function to lookup the Item / the closest position to put it in
 * \param Hash Our Hash to manipulate
 * \param HashBinKey the Hash-Number to lookup. 
 * \returns the position (most closely) matching HashBinKey (-> Caller needs to compare! )
 */
static long FindInHash(HashList *Hash, long HashBinKey)
{
	long SearchPos;
	long StepWidth;

	if (Hash == NULL)
		return 0;

	if (Hash->tainted)
		return FindInTaintedHash(Hash, HashBinKey);

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

/**
 * \brief private abstract wrapper around the hashing algorithm
 * \param HKey the hash string
 * \param HKLen length of HKey
 * \returns the calculated hash value
 */
inline static long CalcHashKey (HashList *Hash, const char *HKey, long HKLen)
{
	if (Hash == NULL)
		return 0;

	if (Hash->Algorithm == NULL)
		return hashlittle(HKey, HKLen, 9283457);
	else
		return Hash->Algorithm(HKey, HKLen);
}


/**
 * \brief Add a new / Replace an existing item in the Hash
 * \param HashList the list to manipulate
 * \param HKey the hash-string to store Data under
 * \param HKeyLen Length of HKey
 * \param Data the payload you want to associate with HKey
 * \param DeleteIt if not free() should be used to delete Data set to NULL, else DeleteIt is used.
 */
void Put(HashList *Hash, const char *HKey, long HKLen, void *Data, DeleteHashDataFunc DeleteIt)
{
	long HashBinKey;
	long HashAt;

	if (Hash == NULL)
		return;

	/** first, find out were we could fit in... */
	HashBinKey = CalcHashKey(Hash, HKey, HKLen);
	HashAt = FindInHash(Hash, HashBinKey);

	if (HashAt >= Hash->MemberSize)
		IncreaseHashSize (Hash);

	/** oh, we're brand new... */
	if (Hash->LookupTable[HashAt] == NULL) {
		InsertHashItem(Hash, HashAt, HashBinKey, HKey, HKLen, Data, DeleteIt);
	}/** Insert After? */
	else if (Hash->LookupTable[HashAt]->Key > HashBinKey) {
		InsertHashItem(Hash, HashAt, HashBinKey, HKey, HKLen, Data, DeleteIt);
	}/** Insert before? */
	else if (Hash->LookupTable[HashAt]->Key < HashBinKey) {
		InsertHashItem(Hash, HashAt + 1, HashBinKey, HKey, HKLen, Data, DeleteIt);
	}
	else { /** Ok, we have a colision. replace it. */
		if (Hash->uniq) {
			long PayloadPos;
			
			PayloadPos = Hash->LookupTable[HashAt]->Position;
			DeleteHashPayload(Hash->Members[PayloadPos]);
			Hash->Members[PayloadPos]->Data = Data;
			Hash->Members[PayloadPos]->Destructor = DeleteIt;
		}
		else {
			InsertHashItem(Hash, HashAt + 1, HashBinKey, HKey, HKLen, Data, DeleteIt);
		}
	}
}

/**
 * \brief Lookup the Data associated with HKey
 * \param Hash the Hashlist to search in
 * \param HKey the hashkey to look up
 * \param HKLen length of HKey
 * \param Data returns the Data associated with HKey
 * \returns 0 if not found, 1 if.
 */
int GetHash(HashList *Hash, const char *HKey, long HKLen, void **Data)
{
	long HashBinKey;
	long HashAt;

	if (Hash == NULL)
		return 0;

	if (HKLen <= 0) {
		*Data = NULL;
		return  0;
	}
	/** first, find out were we could be... */
	HashBinKey = CalcHashKey(Hash, HKey, HKLen);
	HashAt = FindInHash(Hash, HashBinKey);
	if ((HashAt < 0) || /**< Not found at the lower edge? */
	    (HashAt >= Hash->nMembersUsed) || /**< Not found at the upper edge? */
	    (Hash->LookupTable[HashAt]->Key != HashBinKey)) { /**< somewhere inbetween but no match? */
		*Data = NULL;
		return 0;
	}
	else { /** GOTCHA! */
		long MemberPosition;

		MemberPosition = Hash->LookupTable[HashAt]->Position;
		*Data = Hash->Members[MemberPosition]->Data;
		return 1;
	}
}

/* TODO? */
int GetKey(HashList *Hash, char *HKey, long HKLen, void **Payload)
{
	return 0;
}

/**
 * \brief get the Keys present in this hash, simila to array_keys() in PHP
 *  Attention: List remains to Hash! don't modify or free it!
 * \param Hash Your Hashlist to extract the keys from
 * \param List returns the list of hashkeys stored in Hash
 */
int GetHashKeys(HashList *Hash, char ***List)
{
	long i;
	if (Hash == NULL)
		return 0;
	if (Hash->MyKeys != NULL)
		free (Hash->MyKeys);

	Hash->MyKeys = (char**) malloc(sizeof(char*) * Hash->nMembersUsed);
	for (i=0; i < Hash->nMembersUsed; i++) {
	
		Hash->MyKeys[i] = Hash->LookupTable[i]->HashKey;
	}
	*List = (char**)Hash->MyKeys;
	return Hash->nMembersUsed;
}

/**
 * \brief creates a hash-linear iterator object
 * \returns the hash iterator
 */
HashPos *GetNewHashPos(void)
{
	HashPos *Ret;
	
	Ret = (HashPos*)malloc(sizeof(HashPos));
	Ret->Position = 0;
	return Ret;
}

/**
 * \brief frees a linear hash iterator
 */
void DeleteHashPos(HashPos **DelMe)
{
	if (*DelMe != NULL)
	{
		free(*DelMe);
		*DelMe = NULL;
	}
}


/**
 * \brief Get the data located where HashPos Iterator points at, and Move HashPos one forward
 * \param Hash your Hashlist to follow
 * \param HKLen returns Length of Hashkey Returned
 * \param HashKey returns the Hashkey corrosponding to HashPos
 * \param Data returns the Data found at HashPos
 * \returns whether the item was found or not.
 */
int GetNextHashPos(HashList *Hash, HashPos *At, long *HKLen, char **HashKey, void **Data)
{
	long PayloadPos;

	if ((Hash == NULL) || (Hash->nMembersUsed <= At->Position))
		return 0;
	*HKLen = Hash->LookupTable[At->Position]->HKLen;
	*HashKey = Hash->LookupTable[At->Position]->HashKey;
	PayloadPos = Hash->LookupTable[At->Position]->Position;
	*Data = Hash->Members[PayloadPos]->Data;
	At->Position++;
	return 1;
}

/**
 * \brief sorting function for sorting the Hash alphabeticaly by their strings
 * \param Key1 first item
 * \param Key2 second item
 */
static int SortByKeys(const void *Key1, const void* Key2)
{
	HashKey *HKey1, *HKey2;
	HKey1 = *(HashKey**) Key1;
	HKey2 = *(HashKey**) Key2;

	return strcasecmp(HKey1->HashKey, HKey2->HashKey);
}

/**
 * \brief sorting function for sorting the Hash alphabeticaly reverse by their strings
 * \param Key1 first item
 * \param Key2 second item
 */
static int SortByKeysRev(const void *Key1, const void* Key2)
{
	HashKey *HKey1, *HKey2;
	HKey1 = *(HashKey**) Key1;
	HKey2 = *(HashKey**) Key2;

	return strcasecmp(HKey2->HashKey, HKey1->HashKey);
}

/**
 * \brief sorting function to regain hash-sequence and revert tainted status
 * \param Key1 first item
 * \param Key2 second item
 */
static int SortByHashKeys(const void *Key1, const void* Key2)
{
	HashKey *HKey1, *HKey2;
	HKey1 = *(HashKey**) Key1;
	HKey2 = *(HashKey**) Key2;

	return HKey1->Key > HKey2->Key;
}


/**
 * \brief sort the hash alphabeticaly by their keys.
 * Caution: This taints the hashlist, so accessing it later 
 * will be significantly slower! You can un-taint it by SortByHashKeyStr
 * \param Hash the list to sort
 * \param Order 0/1 Forward/Backward
 */
void SortByHashKey(HashList *Hash, int Order)
{
	if (Hash->nMembersUsed < 2)
		return;
	qsort(Hash->LookupTable, Hash->nMembersUsed, sizeof(HashKey*), 
	      (Order)?SortByKeys:SortByKeysRev);
	Hash->tainted = 1;
}

/**
 * \brief sort the hash by their keys (so it regains untainted state).
 * this will result in the sequence the hashing allgorithm produces it by default.
 * \param Hash the list to sort
 */
void SortByHashKeyStr(HashList *Hash)
{
	Hash->tainted = 0;
	if (Hash->nMembersUsed < 2)
		return;
	qsort(Hash->LookupTable, Hash->nMembersUsed, sizeof(HashKey*), SortByHashKeys);
}


/**
 * \brief gives user sort routines access to the hash payload
 * \param Searchentry to retrieve Data to
 * \returns Data belonging to HashVoid
 */
const void *GetSearchPayload(const void *HashVoid)
{
	return (*(HashKey**)HashVoid)->PL->Data;
}

/**
 * \brief sort the hash by your sort function. see the following sample.
 * this will result in the sequence the hashing allgorithm produces it by default.
 * \param Hash the list to sort
 * \param SortBy Sortfunction; see below how to implement this
 */
void SortByPayload(HashList *Hash, CompareFunc SortBy)
{
	if (Hash->nMembersUsed < 2)
		return;
	qsort(Hash->LookupTable, Hash->nMembersUsed, sizeof(HashKey*), SortBy);
	Hash->tainted = 1;
}




/**
 * given you've put char * into your hash as a payload, a sort function might
 * look like this:
 * int SortByChar(const void* First, const void* Second)
 * {
 *      char *a, *b;
 *      a = (char*) GetSearchPayload(First);
 *      b = (char*) GetSearchPayload(Second);
 *      return strcmp (a, b);
 * }
 */


/*
 * Generic function to free a pointer.  This can be used as a callback with the
 * hash table, even on systems where free() is defined as a macro or has had other
 * horrible things done to it.
 */
void generic_free_handler(void *ptr) {
	free(ptr);
}



