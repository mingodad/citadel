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
static void TestRevalidateStrBuf(StrBuf *Buf)
{
	CU_ASSERT(strlen(ChrPtr(Buf)) == StrLength(Buf));
}




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



static void AddStrBufSimlpeTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufConversions", NULL, NULL);
	if (!parse_email) {
		if (!fromstdin) {
			pTest = CU_add_test(pGroup, "testRFC822Decode", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode1", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode2", TestRFC822Decode);
			pTest = CU_add_test(pGroup, "testRFC822Decode3", TestRFC822Decode);
		}
		else
			pTest = CU_add_test(pGroup, "testRFC822DecodeSTDIN", TestRFC822DecodeStdin);
	}
	else {
		if (!fromstdin) {
			pTest = CU_add_test(pGroup, "TestParseEmailSTDIN", TestEncodeEmail);
		}
		else
			pTest = CU_add_test(pGroup, "TestParseEmailSTDIN", TestEncodeEmailSTDIN);
	}

}


int main(int argc, char* argv[])
{
	int a;

	while ((a = getopt(argc, argv, "@i")) != EOF)
		switch (a) {
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
