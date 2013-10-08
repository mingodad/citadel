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
#include <stdarg.h>

    
#ifndef CUNIT_AUTOMATED_H_SEEN
#define CUNIT_AUTOMATED_H_SEEN

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/TestDB.h>


CU_EXPORT void         CU_automated_run_tests(void);
CU_EXPORT CU_ErrorCode CU_list_tests_to_file(void);
CU_EXPORT void         CU_set_output_filename(const char* szFilenameRoot);


#endif  /*  CUNIT_AUTOMATED_H_SEEN  */

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "../lib/libcitadel.h"

static void TestStripAllBut(void) {
	int i;
	long l;

	char *teststrings[] = {
		"Nmm <fghjk> tyutyu",
		"Sdd <> ghhjjkk",
		"<>",
		"",
		"Sdd <",
		"Df Ghjkl>",
		">< bggt",
		"FSDFSDFSDFSDF<><><>bggt",
		"FSDFSDFSDF<><eoeo><>bggt",
		"Abc<QWER>",
		"><",
		"Zxcv<dy<<lkj>>>"
	};

	char *strippedstrings[] = {
		"fghjk",
		"",
		"",
		"",
		"Sdd <",
		"Df Ghjkl>",
		">< bggt",
		"FSDFSDFSDFSDF<><><>bggt",
		"FSDFSDFSDF<><eoeo><>bggt",
		"QWER",
		"><",
		"lkj"
	};

	long strippedlens[] = {
		5,
		0,
		0,
		0,
		5,
		9,
		7,
		23,
		24,
		4,
		2,
		3
	};

	char foo[128];
	StrBuf *Test = NewStrBuf();;

	for (i=0; i<(sizeof(teststrings) / sizeof (char *)); ++i) {
		strcpy(foo, teststrings[i]);
		l = stripallbut(foo, '<', '>');

		CU_ASSERT_STRING_EQUAL(foo, strippedstrings[i]);
		CU_ASSERT_EQUAL(strlen(foo), strippedlens[i]);


		StrBufPlain(Test, teststrings[i], -1);
		StrBufStripAllBut(Test, '<', '>');

		CU_ASSERT_STRING_EQUAL(ChrPtr(Test), strippedstrings[i]);
		CU_ASSERT_EQUAL(StrLength(Test), strippedlens[i]);

		printf("[%s] -> [%s][%s][%s]\n", teststrings[i], foo, ChrPtr(Test), strippedstrings[i]);
	}
}



static void AddStringToolsTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringTools", NULL, NULL);
	pTest = CU_add_test(pGroup, "testStripAllBut", TestStripAllBut);
}

int main(int argc, char* argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);

	StartLibCitadel(8);

	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
		return 1;
	}
	AddStringToolsTests();
	
	printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    
		
	CU_cleanup_registry();
	
	return 0;
}
