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
#include <stdio.h>
#include <unistd.h>

#include "stringbuf_test.h"
#include "../lib/libcitadel.h"


int fromstdin = 0;
int parse_email = 0;
int parse_html = 0;
int OutputEscape = 0;
int OutputEscapeAs = 0;

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


	DefaultCharset = NewStrBufPlain(HKEY("iso-8859-1"));
	FoundCharset = NewStrBuf();
	Source = NewStrBufPlain(HKEY("\"w.goesgens\" <w.goesgens@aoeuaoeuaoeu.org>, =?ISO-8859-15?Q?Walter_?= =?ISO-8859-15?Q?G=F6aoeus?= <aoeuaoeu@aoe.de>, =?ISO-8859-15?Q?aoeuaoeuh?= =?ISO-8859-15?Q?_G=F6aoeus?= <aoeuoeuaoeu@oeu.de>, aoeuao aoeuaoeu <aoeuaoeuaoeaoe@aoe.de"));
	Target = NewStrBufPlain(NULL, 256);

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


static void TestHTMLEscEncodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;

	Source = NewStrBuf();

	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrEscAppend(Target, Source, NULL, 0, 2);
		
		TestRevalidateStrBuf(Target);
		printf("%s\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	FreeStrBuf(&Source);
}

static void TestEscEncodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;

	Source = NewStrBuf();

	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrEscAppend(Target, Source, NULL, 0, 0);
		
		TestRevalidateStrBuf(Target);
		printf("%s\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	FreeStrBuf(&Source);
}


static void TestECMAEscEncodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;

	Source = NewStrBuf();

	printf("[");
	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrECMAEscAppend(Target, Source, NULL);
		
		TestRevalidateStrBuf(Target);
		printf("\"%s\",\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	printf("]\n");
	FreeStrBuf(&Source);
}

static void TestHtmlEcmaEscEncodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;

	Source = NewStrBuf();

	printf("[");
	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrHtmlEcmaEscAppend(Target, Source, NULL, 0, 2);
		
		TestRevalidateStrBuf(Target);
		printf("\"%s\",\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	printf("]");
	FreeStrBuf(&Source);
}

static void TestUrlescEncodeStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;

	Source = NewStrBuf();

	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		Target = NewStrBuf();
		
		StrBufUrlescAppend(Target, Source, NULL);
		
		TestRevalidateStrBuf(Target);
		printf("%s\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	FreeStrBuf(&Source);
}


static void TestEncodeEmail(void)
{
	StrBuf *Target;
	StrBuf *Source;
	StrBuf *UserName = NewStrBuf();
	StrBuf *EmailAddress = NewStrBuf();
	StrBuf *EncBuf = NewStrBuf();
	
	Source = NewStrBuf();
 
// 	Source = NewStrBufPlain(HKEY("Art Cancro <ajc@uncensored.citadel.org>, Art Cancro <ajc@uncensored.citadel.org>"));

	Source = NewStrBufPlain(HKEY("\"Alexandra Weiz, Restless GmbH\" <alexandra.weiz@boblbee.de>, \"NetIN\" <editor@netin.co.il>, \" יריב ברקאי, מולטימדי\" <info@immembed.com>"));	
	Target = StrBufSanitizeEmailRecipientVector(
		Source,
		UserName, 
		EmailAddress,
		EncBuf
		);		
	
	TestRevalidateStrBuf(Target);
	printf("the source:>%s<\n", ChrPtr(Source));
	printf("the target:>%s<\n", ChrPtr(Target));
	FreeStrBuf(&Target);
	FreeStrBuf(&UserName);
	FreeStrBuf(&EmailAddress);
	FreeStrBuf(&EncBuf);

	FreeStrBuf(&Source);
}

static void TestEncodeEmailSTDIN(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;
	StrBuf *UserName = NewStrBuf();
	StrBuf *EmailAddress = NewStrBuf();
	StrBuf *EncBuf = NewStrBuf();
	
	Source = NewStrBuf();

	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		printf("the source:>%s<\n", ChrPtr(Source));
		Target = StrBufSanitizeEmailRecipientVector(
			Source,
			UserName, 
			EmailAddress,
			EncBuf
			);
		
		TestRevalidateStrBuf(Target);
		printf("the target:>%s<\n", ChrPtr(Target));
		FreeStrBuf(&Target);
	}
	FreeStrBuf(&UserName);
	FreeStrBuf(&EmailAddress);
	FreeStrBuf(&EncBuf);

	FreeStrBuf(&Source);
}

static void StrBufRFC2047encodeMessageStdin(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Target;
	StrBuf *Source;
	StrBuf *Src;

	Source = NewStrBuf();
	Src = NewStrBuf();

	printf("[");
	while (fdin == 0) {

		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		StrBufAppendBuf(Src, Source, 0);
		StrBufAppendBufPlain(Src, HKEY("\n"), 0);
				
	}

	Target = StrBufRFC2047encodeMessage(Src);

	printf("Target: \n%s\n", ChrPtr(Target));
	FreeStrBuf(&Source);
	FreeStrBuf(&Src);
	FreeStrBuf(&Target);
}

static void TestHTML2ASCII_line(void)
{
	int fdin = 0;// STDIN
	const char *Err;
	StrBuf *Source;
	char *Target;

	Source = NewStrBuf();

	while (fdin == 0) {
		
		StrBufTCP_read_line(Source, &fdin, 0, &Err);
		printf("the source:>%s<\n", ChrPtr(Source));
		Target = html_to_ascii(ChrPtr(Source), StrLength(Source), 80, 0);
		
		printf("the target:>%s<\n", Target);
		FlushStrBuf(Source);
		free(Target);
	}

	FreeStrBuf(&Source);
}


static void AddStrBufSimlpeTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufConversions", NULL, NULL);
	if (parse_email) {
		if (!fromstdin) {
			pTest = CU_add_test(pGroup, "TestParseEmailSTDIN", TestEncodeEmail);
		}
		else
			pTest = CU_add_test(pGroup, "TestParseEmailSTDIN", TestEncodeEmailSTDIN);
	}
	else if (parse_html) {
			pTest = CU_add_test(pGroup, "TestParseHTMLSTDIN", TestHTML2ASCII_line);
	}
	else {
		if (!fromstdin) {
			pTest = CU_add_test(pGroup, "testRFC822Decode", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode1", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode2", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode3", TestRFC822Decode);
		}
		else
		{
			if (!OutputEscape)
				pTest = CU_add_test(pGroup, "testRFC822DecodeSTDIN", TestRFC822DecodeStdin);
			else switch(OutputEscapeAs)
			     {
			     case 'H':
				     pTest = CU_add_test(pGroup, "TestHTMLEscEncodeStdin", TestHTMLEscEncodeStdin);
				     break;
			     case 'X':
				     pTest = CU_add_test(pGroup, "TestEscEncodeStdin", TestEscEncodeStdin);
				     break;
			     case 'J':
				     pTest = CU_add_test(pGroup, "TestECMAEscEncodeStdin", TestECMAEscEncodeStdin);
				     break;
			     case 'K':
				     pTest = CU_add_test(pGroup, "TestHtmlEcmaEscEncodeStdin", TestHtmlEcmaEscEncodeStdin);
				     break;
			     case 'U':
				     pTest = CU_add_test(pGroup, "TestUrlescEncodeStdin", TestUrlescEncodeStdin);
				     break;
			     case 'R':
				     pTest = CU_add_test(pGroup, "StrBufRFC2047encodeMessageStdin", StrBufRFC2047encodeMessageStdin);
				     break;
			     default:
				     printf("%c not supported!\n", OutputEscapeAs);
				     CU_ASSERT(1);
			     }
		}
	}

}


int main(int argc, char* argv[])
{
	int a;

	while ((a = getopt(argc, argv, "@iho:")) != EOF)
		switch (a) {
		case 'o':
			if (optarg)
			{
				OutputEscape = 1;
				OutputEscapeAs = *optarg;
			}
			break;
		case 'h':
			parse_html = 1;
			break;
		case '@':
			parse_email = 1;
			break;
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
