/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  
 *  This library is open source software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stringbuf_test.h"
#include "../lib/libcitadel.h"


static HashList *GetFilledHash(int n, int stepwidth)
{
	HashList* TestHash;
	int i;
	int *val;

	TestHash = NewHash(1, Flathash);

	for (i = 0; i < n;  i+=stepwidth)
	{
		val = (int*) malloc(sizeof(int));
		*val = i;
		Put(TestHash, IKEY(i), val, NULL);
	}
	return TestHash;
}



static void test_iterate_hash(HashList *testh, int forward, int stepwidth)
{
	int i = 0;
	HashPos  *it;
	void *vTest;
	long len = 0;
	const char *Key;
	int dir = 1;

	if (forward == 0)
		dir = -1;
	it = GetNewHashPos(testh, dir * stepwidth);
	while (GetNextHashPos(testh, it, &len, &Key, &vTest) &&
               (vTest != NULL)) {

		printf("i: %d c: %d\n", i, *(int*) vTest);
		i+=stepwidth;
	}
	DeleteHashPos(&it);
}

static void ValidateBackAndForth(HashList *H)
{
	test_iterate_hash(H, 1, 1);
	printf("\n");

	test_iterate_hash(H, 0, 1);
	printf("\n");

	test_iterate_hash(H, 1, 2);
	printf("\n");

	test_iterate_hash(H, 0, 2);
	printf("\n");

	test_iterate_hash(H, 1, 3);
	printf("\n");

	test_iterate_hash(H, 0, 3);
	printf("\n");
}

static void TestHashlistIteratorForward (void)
{
	HashList *H;

	H = GetFilledHash (10, 1);

	ValidateBackAndForth(H);

	DeleteHash(&H);
}


static void TestHashlistAddDelete (void)
{
	HashList *H;
	HashPos *at;
	int *val, i;

	H = GetFilledHash (10, 1);

	at = GetNewHashPos(H, 0);

	printf("Remove first\n");
	DeleteEntryFromHash(H, at);
	DeleteHashPos(&at);

	ValidateBackAndForth(H);

	printf("Insert 15\n");
	i = 15;
	val = (int*) malloc(sizeof(int));
	*val = i;
	Put(H, IKEY(i), val, NULL);

	ValidateBackAndForth(H);

	printf("Remove third\n");
	at = GetNewHashPos(H, 0);
	NextHashPos(H, at);
	NextHashPos(H, at);
	NextHashPos(H, at);
	DeleteEntryFromHash(H, at);

	ValidateBackAndForth(H);
	printf("Insert -15\n");

	i = -15;
	val = (int*) malloc(sizeof(int));
	*val = i;
	Put(H, IKEY(i), val, NULL);

	ValidateBackAndForth(H);

	DeleteHashPos(&at);
	DeleteHash(&H);
}

/*
Some samples from the original...
	CU_ASSERT_EQUAL(10, 10);
	CU_ASSERT_EQUAL(0, -0);
	CU_ASSERT_EQUAL(-12, -12);
	CU_ASSERT_NOT_EQUAL(10, 11);
	CU_ASSERT_NOT_EQUAL(0, -1);
	CU_ASSERT_NOT_EQUAL(-12, -11);
	CU_ASSERT_PTR_EQUAL((void*)0x100, (void*)0x100);
	CU_ASSERT_PTR_NOT_EQUAL((void*)0x100, (void*)0x101);
	CU_ASSERT_PTR_NULL(NULL);
	CU_ASSERT_PTR_NULL(0x0);
	CU_ASSERT_PTR_NOT_NULL((void*)0x23);
	CU_ASSERT_STRING_EQUAL(str1, str2);
	CU_ASSERT_STRING_NOT_EQUAL(str1, str2);
	CU_ASSERT_NSTRING_EQUAL(str1, str2, strlen(str1));
	CU_ASSERT_NSTRING_EQUAL(str1, str1, strlen(str1));
	CU_ASSERT_NSTRING_EQUAL(str1, str1, strlen(str1) + 1);
	CU_ASSERT_NSTRING_NOT_EQUAL(str1, str2, 3);
	CU_ASSERT_NSTRING_NOT_EQUAL(str1, str3, strlen(str1) + 1);
	CU_ASSERT_DOUBLE_EQUAL(10, 10.0001, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(10, 10.0001, -0.0001);
	CU_ASSERT_DOUBLE_EQUAL(-10, -10.0001, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(-10, -10.0001, -0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(10, 10.001, 0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(10, 10.001, -0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(-10, -10.001, 0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(-10, -10.001, -0.0001);
*/



const char *MSetStrings[] = {
	"11:63",
	"65",
	"1,65,77",
	"1:65,77,80:*",
	NULL
};

#define InMSet 0
#define NotInMSet 1
const long MessageNumbers[5][2][10] = {
/* First MSet */
	{
		{11, 59, 63, 0, 0, 0, 0, 0, 0, 0}, /* In */
		{2, 10, 64, 65, 0, 0, 0, 0, 0, 0} /* NotIn */
	},
	{
		{65, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* In */
		{1, 64, 66, 0, 0, 0, 0, 0, 0, 0} /* NotIn */
	},

	{
		{1, 65, 77, 0, 0, 0, 0, 0, 0, 0}, /* In */
		{2, 64, 66, 76, 78, 0, 0, 0, 0, 0} /* NotIn */
	},
	{
		{1, 2, 30, 64, 65, 77, 80, 81, 222222, 0}, /* In */
		{66, 76, 78, 79, 0, 0, 0, 0, 0, 0} /* NotIn */
	},
	{
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* In */
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0} /* NotIn */
	}
};



static void TestMSetHashlist (void)
{
	int nTest = 0;
	int j;
	StrBuf *MSetStr;
	StrBuf *Assert;
	MSet *OneMSet;
	

	MSetStr = NewStrBuf();
	Assert = NewStrBuf();
	while (MSetStrings[nTest] != NULL)
	{
		StrBufPlain(MSetStr, MSetStrings[nTest], -1);
		ParseMSet(&OneMSet, MSetStr);

//#ifdef VERBOSE_TEST
		printf("---%s---\n", ChrPtr(MSetStr));
		{
			const char *HashKey;
			long HKLen;
			HashList *ScanMe = (HashList*) OneMSet;
			HashPos *at;
			void *vMsg;
			long *end;

			at = GetNewHashPos(ScanMe, 0);
			while (GetNextHashPos(ScanMe, at, &HKLen, &HashKey, &vMsg)) {
				/* Are you a new message, or an old message? */
				end = (long*) vMsg;
				printf("[%ld][%ld]\n", *(long*)HashKey, *end);
			}
			DeleteHashPos(&at);
		}
//#endif

		j = 0;
		while (MessageNumbers[nTest][InMSet][j] != 0)
		{
			if (!IsInMSetList(OneMSet, MessageNumbers[nTest][InMSet][j]))
			{
				StrBufPrintf(Assert, "InFail: %s <-> %ld\n", 
					     ChrPtr(MSetStr), 
					     MessageNumbers[nTest][InMSet][j]);
				CU_FAIL(ChrPtr(Assert));
				printf("%s", ChrPtr(Assert));
			}
			else
			{
				StrBufPrintf(Assert, "InPass: %s <-> %ld\n", 
					     ChrPtr(MSetStr), 
					     MessageNumbers[nTest][InMSet][j]);
				CU_PASS(ChrPtr(Assert));
			}
			j++;	
		}
		j = 0;
		while (MessageNumbers[nTest][NotInMSet][j] != 0)
		{
			if (IsInMSetList(OneMSet, MessageNumbers[nTest][NotInMSet][j]))
			{
				StrBufPrintf(Assert, "NOT-InFail: %s <-> %ld\n", 
					     ChrPtr(MSetStr), 
					     MessageNumbers[nTest][NotInMSet][j]);
				CU_FAIL(ChrPtr(Assert));
				printf("%s", ChrPtr(Assert));
			}
			else
			{
				StrBufPrintf(Assert, "NOT-InPass: %s <-> %ld\n", 
					     ChrPtr(MSetStr), 
					     MessageNumbers[nTest][InMSet][j]);
				CU_PASS(ChrPtr(Assert));
			}
			j++;
		}
		

		DeleteMSet(&OneMSet);
		nTest++;
		
	}
	FreeStrBuf(&MSetStr);
	FreeStrBuf(&Assert);
}



static void AddHashlistTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufSimpleAppenders", NULL, NULL);
	pTest = CU_add_test(pGroup, "TestHashListIteratorForward", TestHashlistIteratorForward);
	pTest = CU_add_test(pGroup, "TestHashlistAddDelete", TestHashlistAddDelete);
	pTest = CU_add_test(pGroup, "TestMSetHashlist", TestMSetHashlist);
}


int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	StartLibCitadel(8);
	CU_BOOL Run = CU_FALSE ;
	
	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}
	
	Run = CU_TRUE ;
	AddHashlistTests();
	
	if (CU_TRUE == Run) {
		//CU_console_run_tests();
    printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    
    ///CU_automated_run_tests();
	}
	
	CU_cleanup_registry();

	return 0;
}
