#include "webcit_test.h"
#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/TestDB.h>


#include "webcit.h"
#include "webserver.h"
#include "modules_init.h"
#include <stdio.h>


wcsession *TestSessionList = NULL;


ParsedHttpHdrs Hdr;
wcsession *TheSession;

extern StrBuf *Username;
extern StrBuf *Passvoid;


extern int ReadHttpSubject(ParsedHttpHdrs *Hdr, StrBuf *Line, StrBuf *Buf);
extern wcsession *CreateSession(int Lockable, wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex);
extern void groupdav_main(void);



void SetUpContext(void)
{
        memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
        Hdr.HR.eReqType = eGET;
        http_new_modules(&Hdr); 


	Hdr.http_sock = 1; /* STDOUT */
/* Context loop */
	Hdr.HR.dav_depth = 32767; /* TODO: find a general way to have non-0 defaults */
	TheSession = CreateSession(1, &TestSessionList, &Hdr, NULL);
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->Hdr = &Hdr;
	Hdr.HTTPHeaders = NewHash(1, NULL);
	session_attach_modules(TheSession);
}

int SetUpConnection(void)
{
	StrBuf *Response;
	if (!GetConnected ())
	{
		Response = NewStrBuf();
		become_logged_in(Username, Passvoid, Response);
		return 1;
		
	}
	else {
		CU_FAIL("Establishing session failed!");
		return 0;
	}
}

void SetHttpURL(ParsedHttpHdrs *Hdr, const char *Title, long tlen, StrBuf *Buf)
{
	StrBuf *Line = NewStrBufPlain (Title, tlen);
	FreeStrBuf(&Line);
}

/* from context_loop.c: */
extern void DestroyHttpHeaderHandler(void *V);
extern int ReadHttpSubject(ParsedHttpHdrs *Hdr, StrBuf *Line, StrBuf *Buf);
void SetUpRequest(const char *UrlPath)
{
	OneHttpHeader *pHdr;
	wcsession *WCC = WC;
	StrBuf *Buf;
	StrBuf *Line, *HeaderName;

	HeaderName = NewStrBuf();
	Buf = NewStrBuf();
	Line = NewStrBuf();
	StrBufPrintf(Line, "GET %s HTTP/1.0\r\n", UrlPath);

	WCC->Hdr->HTTPHeaders = NewHash(1, NULL);
	pHdr = (OneHttpHeader*) malloc(sizeof(OneHttpHeader));
	memset(pHdr, 0, sizeof(OneHttpHeader));
	pHdr->Val = Line;
	Put(Hdr.HTTPHeaders, HKEY("GET /"), pHdr, DestroyHttpHeaderHandler);
	syslog(9, "%s\n", ChrPtr(Line));

	if (ReadHttpSubject(&Hdr, Line, HeaderName))
		CU_FAIL("Failed to parse Request line / me is bogus!");

	FreeStrBuf(&Buf);
}



void TearDownRequest(void)
{
/* End Context loop */
	http_detach_modules(&Hdr);
}

void TearDownContext(void)
{
	http_destroy_modules(&Hdr);
/* End Session Loop */
	session_detach_modules(TheSession);
	session_destroy_modules(&TheSession);

/* End Session loop */
/* now shut it down clean. */
//	shutdown_sessions();
	do_housekeeping();
}

void test_worker_entry(StrBuf *UrlPath)
{


}


void SetGroupdavHeaders(int DavDepth)
{
	Hdr.HR.dav_depth = DavDepth;
}

void FlushHeaders(void)
{

}

void test_groupdav_directorycommands(void)
{
	SetUpContext();
	if (SetUpConnection())
	{
		SetUpRequest("/");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();


		SetUpRequest("/groupdav");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();


		SetUpRequest("/groupdav/My%20Folders");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/Calendar");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/Calendar");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/Calendar/");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/groupdav/My%20Folders/Calendar/");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();


		SetUpRequest("/");
		SetGroupdavHeaders(0);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();

		SetUpRequest("/");
		SetGroupdavHeaders(1);
		groupdav_main();
		FlushHeaders();
		TearDownRequest();
	}


	TearDownContext();
}

extern void httplang_to_locale(StrBuf *LocaleString, wcsession *sess);

static void test_gettext(const char *str, long len)
{
	StrBuf *Test = NewStrBufPlain(str, len);

	SetUpContext();
	httplang_to_locale(Test, TheSession);
	TearDownContext();

	FreeStrBuf(&Test);
}

void test_gettext_headerevaluation_Opera(void)
{
	test_gettext(HKEY("sq;q=1.0,de;q=0.9,as;q=0.8,ar;q=0.7,bn;q=0.6,zh-cn;q=0.5,kn;q=0.4,ch;q=0.3,fo;q=0.2,gn;q=0.1,ce;q=0.1,ie;q=0.1"));
}

void test_gettext_headerevaluation_firefox1(void)
{
	test_gettext(HKEY("de-de,en-us;q=0.7,en;q=0.3"));
}

void test_gettext_headerevaluation_firefox2(void)
{
	test_gettext(HKEY("de,en-ph;q=0.8,en-us;q=0.5,de-at;q=0.3"));
}

void test_gettext_headerevaluation_firefox3(void)
{
	test_gettext(HKEY("de,en-us;q=0.9,it;q=0.9,de-de;q=0.8,en-ph;q=0.7,de-at;q=0.7,zh-cn;q=0.6,cy;q=0.5,ar-om;q=0.5,en-tt;q=0.4,xh;q=0.3,nl-be;q=0.3,cs;q=0.2,sv;q=0.1,tk;q=0.1"));
}

void test_gettext_headerevaluation_ie7(void)
{
// ie7????
// User-Agent: Mozilla/4.0 (compatible; MSIE 7.0; Windows NT 6.0; FunWebProducts; FBSMTWB; GTB6.3; SLCC1; .NET CLR 2.0.50727; Media Center PC 5.0; .NET CLR 3.5.30729; .NET CLR 3.0.30618; Seekmo 10.3.86.0)

	test_gettext(HKEY("en-us,x-ns1MvoLpRxbNhu,x-ns2F0f0NnyPOPN"));
}

static void AddTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;
/*
	pGroup = CU_add_suite("TestLocaleEvaluator", NULL, NULL);
	pTest = CU_add_test(pGroup, "Test ie7", test_gettext_headerevaluation_ie7);
	pTest = CU_add_test(pGroup, "Test Opera", test_gettext_headerevaluation_Opera);
	pTest = CU_add_test(pGroup, "Test firefox1", test_gettext_headerevaluation_firefox1);
	pTest = CU_add_test(pGroup, "Test firefox2", test_gettext_headerevaluation_firefox2);
	pTest = CU_add_test(pGroup, "Test firefox3", test_gettext_headerevaluation_firefox3);
*/
	pGroup = CU_add_suite("TestUrlPatterns", NULL, NULL);
	pTest = CU_add_test(pGroup, "Test", test_groupdav_directorycommands);


}







void run_tests(void)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}

	AddTests();

	printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
	CU_cleanup_registry();

}
