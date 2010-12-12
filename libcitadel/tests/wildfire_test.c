#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <fcntl.h>

#include <unistd.h>
#include <stddef.h>


#include "../lib/libcitadel.h"

static void CreateWildfireSampleMessage(StrBuf *OutBuf)
{
	JsonValue *Error;
		
	StrBuf *Buf;
	StrBuf *Header;
	StrBuf *Json;
	int n = 1;

	Header = NewStrBuf();
	Json = NewStrBuf();

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__,  HKEY("Warn message"), eWARN);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Error message"), eERROR);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);
	FlushStrBuf(Json);
	FlushStrBuf(Header);


	Buf = NewStrBufPlain(HKEY("test error message"));
	Error = WildFireException(HKEY(__FILE__), __LINE__, Buf, 1);
	SerializeJson(Json, Error, 1);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(OutBuf, Header, 0);

	FreeStrBuf(&Buf);
	FreeStrBuf(&Json);
	FreeStrBuf(&Header);

}

int main(int argc, char* argv[])
{
	StrBuf *WFBuf;
	StrBuf *OutBuf;
	StrBuf *Info;
	int nWildfireHeaders = 0;

	StartLibCitadel(8);
	printf("%s == %d?\n", libcitadel_version_string(), libcitadel_version_number());
       
	
	WildFireInitBacktrace(argv[0], 0);

	WFBuf = NewStrBuf();
	OutBuf = NewStrBuf();
	Info = NewStrBufPlain(HKEY("this is just a test message"));
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	SerializeJson(WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	
	WildFireSerializePayload(WFBuf, OutBuf, &nWildfireHeaders, NULL);

	CreateWildfireSampleMessage(OutBuf);
	printf("%s\n\n", ChrPtr(OutBuf));

	FreeStrBuf(&WFBuf);
	FreeStrBuf(&OutBuf);
	FreeStrBuf(&Info);
	ShutDownLibCitadel();
	return 0;
}
