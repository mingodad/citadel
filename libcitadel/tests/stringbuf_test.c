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

#include "stringbuf_test.h"
#define SHOW_ME_VAPPEND_PRINTF
#include "../lib/libcitadel.h"

int Quiet = 0;
/*
 * Stolen from wc_printf; we need to test that other printf too... 
 */
static void TEST_StrBufAppendPrintf(StrBuf *WBuf, const char *format,...)
{
	va_list arg_ptr;

	if (WBuf == NULL)
		return;

	va_start(arg_ptr, format);
	StrBufVAppendPrintf(WBuf, format, arg_ptr);
	va_end(arg_ptr);
}

static void TestRevalidateStrBuf(StrBuf *Buf)
{
	CU_ASSERT(strlen(ChrPtr(Buf)) == StrLength(Buf));
}


static void TestCreateBuf(void)
{
	StrBuf *Buf;
	StrBuf *Buf2;
	long len;
	long i;

	Buf = NewStrBuf();
	CU_ASSERT(Buf != NULL);
	FreeStrBuf(&Buf);

	Buf = NewStrBufPlain(ChrPtr(NULL), StrLength(NULL));
	CU_ASSERT(Buf != NULL);
	FreeStrBuf(&Buf);

	/* make it alloc a bigger buffer... */
	Buf = NewStrBufPlain(NULL, SIZ);
	CU_ASSERT(Buf != NULL);
	FreeStrBuf(&Buf);


	Buf = NewStrBufDup(NULL);
	CU_ASSERT(Buf != NULL);
	StrBufPlain(Buf, "abc", -1);
	TestRevalidateStrBuf(Buf);
	StrBufPlain(Buf, HKEY("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"));
	TestRevalidateStrBuf(Buf);
	FreeStrBuf(&Buf);

	FlushStrBuf(NULL);
	FLUSHStrBuf(NULL);

	CU_ASSERT(Buf == NULL);
	Buf = NewStrBufPlain(HKEY("ABC"));
	TestRevalidateStrBuf(Buf);
	CU_ASSERT(StrLength(Buf) == 3);
	CU_ASSERT_NSTRING_EQUAL("ABC", ChrPtr(Buf), 3);

	len = StrLength(Buf);
	for (i=0; i< 500; i ++)
	{
		StrBufAppendBufPlain(Buf, HKEY("ABC"), 0);
		len += 3;
		CU_ASSERT(StrLength(Buf) == len);
	}	
	StrBufShrinkToFit(Buf, 1);
	FlushStrBuf(Buf);
	CU_ASSERT(StrLength(Buf) == 0);
	ReAdjustEmptyBuf(Buf, 1, 1);
	TestRevalidateStrBuf(Buf);
	FreeStrBuf(&Buf);
	CU_ASSERT(Buf == NULL);
	
	Buf = NewStrBufPlain(HKEY("ABC"));
	TestRevalidateStrBuf(Buf);
	len = StrLength(Buf);
	for (i=0; i< 500; i ++)
	{
		StrBufAppendPrintf(Buf, "%s", "ABC");
		len += 3;
		CU_ASSERT(StrLength(Buf) == len);		
	}
	TestRevalidateStrBuf(Buf);
	StrBufShrinkToFit(Buf, 1);
	TestRevalidateStrBuf(Buf);
	FreeStrBuf(&Buf);


	Buf = NewStrBufPlain(HKEY("ABC"));
	Buf2 = NewStrBufPlain(HKEY("------"));
	TestRevalidateStrBuf(Buf);
	len = StrLength(Buf);
	for (i=0; i< 50; i ++)
	{
		StrBufPrintf(Buf, "%s", ChrPtr(Buf2));
		CU_ASSERT(StrLength(Buf) == StrLength(Buf2));		

		StrBufAppendBufPlain(Buf2, HKEY("ABCDEFG"), 0);
	}
	TestRevalidateStrBuf(Buf);
	StrBufShrinkToFit(Buf, 1);
	TestRevalidateStrBuf(Buf);
	FreeStrBuf(&Buf);
	FreeStrBuf(&Buf2);	


	Buf = NewStrBufPlain(HKEY("ABC"));
	TestRevalidateStrBuf(Buf);
	len = StrLength(Buf);
	for (i=0; i< 500; i ++)
	{
		TEST_StrBufAppendPrintf(Buf, "%s", "ABC");
		len += 3;
		CU_ASSERT(StrLength(Buf) == len);		
	}
	TestRevalidateStrBuf(Buf);
	StrBufShrinkToFit(Buf, 1);
	TestRevalidateStrBuf(Buf);


	Buf2 = NewStrBufDup(Buf);
	CU_ASSERT(StrLength(Buf) == StrLength(Buf2));		
	
	CU_ASSERT_NSTRING_EQUAL(ChrPtr(Buf2), ChrPtr(Buf), StrLength(Buf2));
	
	CU_ASSERT(StrBufIsNumber(Buf) == 0);

	FlushStrBuf(Buf2);
	CU_ASSERT(StrLength(Buf2) == 0);

	FLUSHStrBuf(Buf);
	CU_ASSERT(StrLength(Buf) == 0);

	HFreeStrBuf(NULL);
	HFreeStrBuf(Buf2);
	CU_ASSERT(Buf2 != NULL);

	FreeStrBuf(&Buf);
	CU_ASSERT(Buf == NULL);
	
}




static void TestBufNumbers(void)
{
	StrBuf *Buf;
	StrBuf *Buf2;
	StrBuf *Buf3;
	char *ch;
	int i;

	Buf2 = NewStrBuf();
	Buf3 = NewStrBufPlain(HKEY("abcd"));
	Buf = NewStrBufPlain(HKEY("123456"));
	CU_ASSERT(StrBufIsNumber(Buf) == 1);
	CU_ASSERT(StrBufIsNumber(NULL) == 0);
	CU_ASSERT(StrBufIsNumber(Buf2) == 0);
	CU_ASSERT(StrBufIsNumber(Buf3) == 0);

	CU_ASSERT(StrTol(Buf) == 123456);
	CU_ASSERT(StrTol(NULL) == 0);
	CU_ASSERT(StrTol(Buf2) == 0);

	CU_ASSERT(StrToi(Buf) == 123456);
	CU_ASSERT(StrToi(NULL) == 0);
	CU_ASSERT(StrToi(Buf2) == 0);
	ch = SmashStrBuf(NULL);
	CU_ASSERT(ch == NULL);
	i = StrLength(Buf);
	ch = SmashStrBuf(&Buf);
	CU_ASSERT(strlen(ch) == i);
	free(ch);
	FreeStrBuf(&Buf2);
	FreeStrBuf(&Buf3);
}

static void TestStrBufPeek(void)
{
	StrBuf *Buf;
	const char *pch;

	Buf = NewStrBufPlain(HKEY("0123456"));
	pch = ChrPtr(Buf);

	CU_ASSERT(StrBufPeek(NULL, pch + 4, -1, 'A') == -1);

	CU_ASSERT(StrBufPeek(Buf, pch + 4, -1, 'A') == 4);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123A56");

	CU_ASSERT(StrBufPeek(Buf, pch - 1, -1, 'A') == -1);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123A56");

	CU_ASSERT(StrBufPeek(Buf, pch + 10, -1, 'A') == -1);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123A56");

	CU_ASSERT(StrBufPeek(Buf, NULL, -1, 'A') == -1);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123A56");

	CU_ASSERT(StrBufPeek(Buf, NULL, 10, 'A') == -1);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123A56");

	CU_ASSERT(StrBufPeek(Buf, NULL, 5, 'A') == 5);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "0123AA6");
	FreeStrBuf(&Buf);
}

static void TestBufStringManipulation(void)
{
	long len, i = 0;
	StrBuf *dest = NewStrBuf ();
	StrBuf *Buf = NewStrBufPlain(HKEY("1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"));

	StrBufSub(dest, Buf, -5, i);
	len = StrLength(Buf);
	for (i = 0; i < len + 10; i++)
	{
		StrBufSub(dest, Buf, 5, i);
		if (i + 5 < len)
		{
			CU_ASSERT(StrLength(dest) == i);
		}
		else
		{
			CU_ASSERT(StrLength(dest) == len - 5);
		}
	}
	FreeStrBuf(&dest);
	dest = NewStrBuf ();
	StrBufSub(dest, Buf, -5, 200);

	StrBufCutLeft(Buf, 5);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"67890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890");
	CU_ASSERT(StrLength(Buf) == 95);

	StrBufCutRight(Buf, 5);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 90);

	StrBufCutAt(Buf, 80, NULL);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"67890123456789012345678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 80);

	StrBufCutAt(Buf, -1, ChrPtr(Buf) + 70);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"6789012345678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 70);


	StrBufCutAt(Buf, 0, ChrPtr(Buf) + 60);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 60);

	StrBufCutAt(Buf, 0, ChrPtr(Buf) + 70);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 60);

	StrBufCutAt(Buf, 70, NULL);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"678901234567890123456789012345678901234567890123456789012345");
	CU_ASSERT(StrLength(Buf) == 60);


	StrBufCutLeft(Buf, 70);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"");
	CU_ASSERT(StrLength(Buf) == 0);

	StrBufPlain(Buf, HKEY("678901234567890123456789012345678901234567890123456789012345"));
	StrBufCutRight(Buf, 70);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"");
	CU_ASSERT(StrLength(Buf) == 0);

	FreeStrBuf(&dest);
	FreeStrBuf(&Buf);

	Buf = NewStrBufPlain(HKEY(" \tabc\t "));
	StrBufTrim(Buf);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf),"abc");
	CU_ASSERT(StrLength(Buf) == 3);

	StrBufUpCase(NULL);
	FlushStrBuf(Buf);
	StrBufUpCase(Buf);
	StrBufPlain(Buf, HKEY("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"));
	StrBufUpCase(Buf);

	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");


	StrBufLowerCase(NULL);
	FlushStrBuf(Buf);
	StrBufLowerCase(Buf);
	StrBufPlain(Buf, HKEY("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"));
	StrBufLowerCase(Buf);

	CU_ASSERT_STRING_EQUAL(ChrPtr(Buf), "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789");


	FreeStrBuf(&Buf);

}

static void NextTokenizerIterateBuf(StrBuf *Buf, int NTokens)
{
	long FoundTokens;
	const char *pCh = NULL;
	StrBuf *Buf2;
	long CountTokens = 0;
	long HaveNextToken = 0;
	long HaveNextTokenF = 0;

	TestRevalidateStrBuf(Buf);
	FoundTokens = StrBufNum_tokens(Buf, ',');
	if (!Quiet) 
		printf("\n\nTemplate: >%s< %d, %ld\n", 
		       ChrPtr(Buf), 
		       NTokens, 
		       FoundTokens);

	CU_ASSERT(FoundTokens == NTokens);

	Buf2 = NewStrBuf();
	while (HaveNextToken = StrBufHaveNextToken(Buf, &pCh),
	       HaveNextTokenF = StrBufExtract_NextToken(Buf2, Buf, &pCh, ','),
	       (HaveNextTokenF>= 0))
	{
		CountTokens++;
		
		if (!Quiet) printf("Token: >%s< >%s< %ld:%ld\n", 
				   ChrPtr(Buf2), 
				   ((pCh != NULL) && (pCh != StrBufNOTNULL))? pCh : "N/A", 
				   HaveNextToken, 
				   HaveNextTokenF);
		TestRevalidateStrBuf(Buf2);

		CU_ASSERT(HaveNextToken == (HaveNextTokenF >= 0));
		
		CU_ASSERT(CountTokens <= NTokens);
	} 
	CU_ASSERT(HaveNextToken == (HaveNextTokenF >= 0));
	FreeStrBuf(&Buf2);
}



static void TestNextTokenizer_EndWithEmpty(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("abc,abc, 1, ,,"));
	NextTokenizerIterateBuf(Buf, 6);
	FreeStrBuf(&Buf);
}

static void TestNextTokenizer_StartWithEmpty(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY(",cde,abc, 1, ,,bbb"));
	NextTokenizerIterateBuf(Buf, 7);
	FreeStrBuf(&Buf);
}

static void TestNextTokenizer_Empty(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY(""));
	NextTokenizerIterateBuf(Buf, 0);
	FreeStrBuf(&Buf);
}

static void TestNextTokenizer_TwoEmpty(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY(","));
	NextTokenizerIterateBuf(Buf, 2);
	FreeStrBuf(&Buf);
}

static void TestNextTokenizer_One(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("one"));
	NextTokenizerIterateBuf(Buf, 1);
	FreeStrBuf(&Buf);
}

static void TestNextTokenizer_Sequence(void)
{
	StrBuf *Buf;
        char *teststring = "40:24524,24662,24673,27869:27935,28393,28426,31247:31258,31731,31749,31761,31778,31782,31801:31803,31813,31904,31915,33708,33935,34619,34672,34720:34723,34766,34835,37594,38854,39235,39942,40030,40142,40520,40815,40907,41201,41578,41781,41954,42292,43110,43565,43801,43998,44180,44241,44295,44401,44561,44635,44798,44861,44946,45022,45137:45148,45166,45179,45707,47114,47141:47157,47194,47314,47349,47386,47489,47496,47534:47543,54460,54601,54637:54652";
        Buf = NewStrBufPlain(teststring, -1);
	NextTokenizerIterateBuf(Buf, 67);
	FreeStrBuf(&Buf);
}



static void NextLineterateBuf(StrBuf *Buf, int NLines)
{
	int n = 0;
	const char *pCh = NULL;
	StrBuf *OneLine;
	StrBuf *ConcatenatedLines;
	long CountTokens = 0;
	
	TestRevalidateStrBuf(Buf);
			     
	OneLine = NewStrBuf();
	ConcatenatedLines = NewStrBuf();

	if (!Quiet) printf("\n");

	if (StrLength(Buf) > 0) 
		do 
		{
			n = StrBufSipLine(OneLine, Buf, &pCh);
			
			CountTokens++;
			
			if (!Quiet) printf("Line: >%s< >%s<\n", 
					   ChrPtr(OneLine), 
					   ((pCh != NULL) && (pCh != StrBufNOTNULL))? pCh : "N/A");
			TestRevalidateStrBuf(OneLine);
			CU_ASSERT(CountTokens <= NLines);
			StrBufAppendBuf(ConcatenatedLines, OneLine, 0);
			
			if ((pCh == StrBufNOTNULL) && 
			    (*(ChrPtr(Buf) + StrLength(Buf) - 1) != '\n'))
			{
			}
			else 
				StrBufAppendBufPlain(ConcatenatedLines, HKEY("\n"), 0);
			
		} 
		while ((pCh != StrBufNOTNULL) &&
		       (pCh != NULL));
	

	if (!Quiet) printf("\n\nTemplate: >%s<\n", ChrPtr(Buf));
	if (!Quiet) printf("\n\nAfter: >%s<\n", ChrPtr(ConcatenatedLines));
	CU_ASSERT_NSTRING_EQUAL(ChrPtr(ConcatenatedLines), 
				ChrPtr(Buf), 
				StrLength(Buf));

	FreeStrBuf(&OneLine);
	FreeStrBuf(&ConcatenatedLines);
}


static void TestNextLine_Empty(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY(""));
	NextLineterateBuf(Buf, 0);
	FreeStrBuf(&Buf);
}


static void TestNextLine_OneLine(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("abc\n"));
	NextLineterateBuf(Buf, 1);
	FreeStrBuf(&Buf);
}


static void TestNextLine_TwoLinesMissingCR(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("abc\ncde"));
	NextLineterateBuf(Buf, 2);
	FreeStrBuf(&Buf);
}


static void TestNextLine_twolines(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("abc\ncde\n"));
	NextLineterateBuf(Buf, 2);
	FreeStrBuf(&Buf);
}

static void TestNextLine_LongLine(void)
{
	StrBuf *Buf;

	Buf = NewStrBufPlain(HKEY("abcde\n1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\n"));
	NextLineterateBuf(Buf, 2);
	FreeStrBuf(&Buf);
}


static void TestStrBufRemove_token_NotThere(void)
{
//	StrBuf *Test = NewStrBufPlain(HKEY(" 127.0.0.1"));
	StrBuf *Test = NewStrBufPlain(HKEY(" 10.122.44.30, 10.122.44.30"));
	StrBufRemove_token(Test, 0, ',');
	TestRevalidateStrBuf(Test);
	FreeStrBuf(&Test);
}


static void TestStrBufUrlescAppend(void)
{
	const char *expect = "%20%2B%23%26%3B%60%27%7C%2A%3F%2D%7E%3C%3E%5E%28%29%5B%5D%7B%7D%2F%24%22%5C";
	StrBuf *In = NewStrBufPlain(HKEY( " +#&;`'|*?-~<>^()[]{}/$\"\\"));
	StrBuf *Out = NewStrBuf();

	StrBufUrlescAppend (Out, In, NULL);
	if (!Quiet) printf ("%s<\n%s<\n%s\n", ChrPtr(In), ChrPtr(Out), expect);
	CU_ASSERT_STRING_EQUAL(ChrPtr(Out), expect);
	FreeStrBuf(&In);
	FreeStrBuf(&Out);
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





static void AddStrBufSimpleTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufSimpleAppenders", NULL, NULL);
	pTest = CU_add_test(pGroup, "testCreateBuf", TestCreateBuf);
	pTest = CU_add_test(pGroup, "TestBufNumbers", TestBufNumbers);
	pTest = CU_add_test(pGroup, "TestStrBufPeek", TestStrBufPeek);
	pTest = CU_add_test(pGroup, "TestBufStringManipulation", TestBufStringManipulation);


	pGroup = CU_add_suite("TestStringTokenizer", NULL, NULL);
	pTest = CU_add_test(pGroup, "testNextTokenizer_EndWithEmpty", TestNextTokenizer_EndWithEmpty);
	pTest = CU_add_test(pGroup, "testNextTokenizer_StartWithEmpty", TestNextTokenizer_StartWithEmpty);
	pTest = CU_add_test(pGroup, "testNextTokenizer_StartWithEmpty", TestNextTokenizer_StartWithEmpty);
	pTest = CU_add_test(pGroup, "testNextTokenizer_Empty", TestNextTokenizer_Empty);
	pTest = CU_add_test(pGroup, "testNextTokenizer_TwoEmpty", TestNextTokenizer_TwoEmpty);
	pTest = CU_add_test(pGroup, "testNextTokenizer_One", TestNextTokenizer_One);
	pTest = CU_add_test(pGroup, "testNextTokenizer_Sequence", TestNextTokenizer_Sequence);


	pGroup = CU_add_suite("TestStrBufSipLine", NULL, NULL);
	pTest = CU_add_test(pGroup, "TestNextLine_Empty", TestNextLine_Empty);
	pTest = CU_add_test(pGroup, "TestNextLine_OneLine", TestNextLine_OneLine);
	pTest = CU_add_test(pGroup, "TestNextLine_TwoLinesMissingCR", TestNextLine_TwoLinesMissingCR);
	pTest = CU_add_test(pGroup, "TestNextLine_twolines", TestNextLine_twolines);
 	pTest = CU_add_test(pGroup, "TestNextLine_LongLine", TestNextLine_LongLine);
	
	pGroup = CU_add_suite("TestStrBufRemove_token", NULL, NULL);
	pTest = CU_add_test(pGroup, "TestStrBufRemove_token_NotThere", TestStrBufRemove_token_NotThere);

	pGroup = CU_add_suite("TestStrBuf_escapers", NULL, NULL);
	pTest = CU_add_test(pGroup, "TestStrBufUrlescAppend", TestStrBufUrlescAppend);
}


int main(int argc, char* argv[])
{
	///int i;
	setvbuf(stdout, NULL, _IONBF, 0);

	StartLibCitadel(8);
	CU_BOOL Run = CU_FALSE ;

	if (argc > 0)
		Quiet = 1; // todo: -q ;-)
//	for (i=0; i< 100000; i++) {
	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
//	}
	
	Run = CU_TRUE ;
	AddStrBufSimpleTests();
	
	if (CU_TRUE == Run) {
		//CU_console_run_tests();
		printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    
    ///CU_automated_run_tests();
	}
	
	CU_cleanup_registry();
	}
	return 0;
}
