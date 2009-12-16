
/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "stringbuf_test.h"
#include "../lib/libcitadel.h"


int fromstdin = 0;

static void TestRevalidateStrBuf(StrBuf *Buf)
{
	CU_ASSERT(strlen(ChrPtr(Buf)) == StrLength(Buf));
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

static void TestRFC822Decode(void)
{
	StrBuf *Target;
	StrBuf *Source;
	StrBuf *DefaultCharset;
	StrBuf *FoundCharset;
	
	DefaultCharset = NewStrBufPlain(HKEY("iso-8859-1"));
	FoundCharset = NewStrBuf();
	Source = NewStrBufPlain(HKEY("=?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?="));
	Target = NewStrBuf();

	StrBuf_RFC822_to_Utf8(Target, Source, DefaultCharset, FoundCharset);


	TestRevalidateStrBuf(Target);
	printf("the ugly multi:>%s<\n", ChrPtr(Target));
	FreeStrBuf(&Source);
	FreeStrBuf(&Target);
	FreeStrBuf(&FoundCharset);
	FreeStrBuf(&DefaultCharset);
}


static void TestRFC822DecodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;
	StrBuf *DefaultCharset;
	StrBuf *FoundCharset;
	
	DefaultCharset = NewStrBufPlain(HKEY("iso-8859-1"));
	FoundCharset = NewStrBuf();
	Source = NewStrBuf();

	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrBuf_RFC822_to_Utf8(Target, Source, DefaultCharset, FoundCharset);
		
		TestRevalidateStrBuf(Target);
		printf("the ugly multi:>%s<\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	FreeStrBuf(&Source);
	FreeStrBuf(&FoundCharset);
	FreeStrBuf(&DefaultCharset);
}



static void AddStrBufSimlpeTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufConversions", NULL, NULL);
	if (!fromstdin) {
		pTest = CU_add_test(pGroup, "testRFC822Decode", TestRFC822Decode);
		pTest = CU_add_test(pGroup, "testRFC822Decode1", TestRFC822Decode);
		pTest = CU_add_test(pGroup, "testRFC822Decode2", TestRFC822Decode);
		pTest = CU_add_test(pGroup, "testRFC822Decode3", TestRFC822Decode);
	}
	else
		pTest = CU_add_test(pGroup, "testRFC822Decode3", TestRFC822DecodeStdin);

}


int main(int argc, char* argv[])
{
	int a;

	while ((a = getopt(argc, argv, "i")) != EOF)
		switch (a) {
		case 'i':
			fromstdin = 1;
			
			break;
		}


	setvbuf(stdout, NULL, _IONBF, 0);

	StartLibCitadel(8);
	CU_BOOL Run = CU_FALSE ;
	
	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}
	
	Run = CU_TRUE ;
	AddStrBufSimlpeTests();
	
	if (CU_TRUE == Run) {
		//CU_console_run_tests();
    printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    
    ///CU_automated_run_tests();
	}
	
	CU_cleanup_registry();

	return 0;
}
