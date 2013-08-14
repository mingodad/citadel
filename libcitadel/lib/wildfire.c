/*@{*/

#include "sysdep.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "libcitadel.h"
#include "libcitadellocal.h"


ConstStr WF_MsgStrs[] = {
	{HKEY("LOG")},
	{HKEY("INFO")},
	{HKEY("WARN")},
	{HKEY("ERROR")},
	{HKEY("TRACE")},
	{HKEY("EXCEPTION")}
};

static JsonValue *WFInfo(const char *Filename, long fnlen,
			 long LineNo, 
			 WF_MessageType Type)
{
	JsonValue *Val;

	Val = NewJsonObject(NULL, 0);
	JsonObjectAppend(Val, 
			 NewJsonPlainString(HKEY("Type"),
					    WF_MsgStrs[Type].Key, 
					    WF_MsgStrs[Type].len));
	JsonObjectAppend(Val, 
			 NewJsonPlainString(HKEY("File"), 
					    Filename, fnlen));
	JsonObjectAppend(Val, 
			 NewJsonNumber(HKEY("Line"), LineNo));
	return Val;
}
			    

JsonValue *WildFireMessage(const char *Filename, long fnlen,
			   long LineNo,
			   StrBuf *Msg, 
			   WF_MessageType Type)
{
	JsonValue *Ret;

	Ret = NewJsonArray(NULL, 0);
	JsonArrayAppend(Ret, WFInfo(Filename, fnlen,
				    LineNo, Type));

	JsonArrayAppend(Ret, 
			NewJsonString(NULL, 0, Msg));
	return Ret;
}

JsonValue *WildFireMessagePlain(const char *Filename, long fnlen,
				long LineNo,
				const char *Message, long len, 
				WF_MessageType Type)
{
	JsonValue *Val;
	Val = NewJsonArray(NULL, 0);

	JsonArrayAppend(Val, WFInfo(Filename, fnlen,
				    LineNo, Type));
	JsonArrayAppend(Val, 
			NewJsonPlainString(NULL, 0, Message, len));
	return Val;
}

void WildFireAddArray(JsonValue *ReportBase, JsonValue *Array, WF_MessageType Type)
{
	JsonValue *Val;
	Val = NewJsonArray(NULL, 0);
	JsonArrayAppend(Val, 
			NewJsonPlainString(NULL, 0, 
					   WF_MsgStrs[Type].Key, 
					   WF_MsgStrs[Type].len));

	JsonArrayAppend(Val, Array);
}

int addr2line_write_pipe[2];
int addr2line_read_pipe[2];
pid_t addr2line_pid;

#ifdef HAVE_BACKTRACE
/* 
 * Start up the addr2line daemon so we can decode function pointers
 */
static void start_addr2line_daemon(const char *binary) 
{
	struct stat filestats;
	int i;
	const char *addr2line = "/usr/bin/addr2line";
	const char minuse[] = "-e";

	printf("Starting addr2line daemon for decoding of backtraces\n");

	if ((stat(addr2line, &filestats)==-1) ||
	    (filestats.st_size==0)){
		printf("didn't find addr2line daemon in %s: %s\n", addr2line, strerror(errno));
		abort();
	}
	if (pipe(addr2line_write_pipe) != 0) {
		printf("Unable to create pipe for addr2line daemon: %s\n", strerror(errno));
		abort();
	}
	if (pipe(addr2line_read_pipe) != 0) {
		printf("Unable to create pipe for addr2line daemon: %s\n", strerror(errno));
		abort();
	}

	addr2line_pid = fork();
	if (addr2line_pid < 0) {
		printf("Unable to fork addr2line daemon: %s\n", strerror(errno));
		abort();
	}
	if (addr2line_pid == 0) {
		dup2(addr2line_write_pipe[0], 0);
		dup2(addr2line_read_pipe[1], 1);
		for (i=2; i<256; ++i) close(i);
		execl(addr2line, addr2line, minuse, binary, NULL);
		printf("Unable to exec addr2line daemon: %s\n", strerror(errno));
		abort();
		exit(errno);
	}
}

static int addr2lineBacktrace(StrBuf *Function, 
			      StrBuf *FileName, 
			      StrBuf *Pointer, 
			      StrBuf *Buf,
			      unsigned int *FunctionLine)

{
	const char *err;
	const char *pch, *pche;

	write(addr2line_write_pipe[1], SKEY(Pointer));
	if (StrBufTCP_read_line(Buf, &addr2line_read_pipe[0], 0, &err) <= 0)
	{
		StrBufAppendBufPlain(Buf, err, -1, 0);
		return 0;
	}
	pch = ChrPtr(Buf);
	pche = strchr(pch, ':');
	FlushStrBuf(FileName);
	StrBufAppendBufPlain(FileName, pch, pche - pch, 0);
	if (pche != NULL)
	{
		pche++;
		*FunctionLine = atoi(pche);
	}
	else 
		*FunctionLine = 0;
	return 1;
}

static int ParseBacktrace(char *Line, 
			  StrBuf *Function, 
			  StrBuf *FileName, 
			  unsigned int *FunctionLine)
{
	char *pch, *pche;

	pch = Line;
	pche = strchr(pch, '(');
	if (pche == NULL) return 0;
	StrBufAppendBufPlain(FileName, pch, pche - pch, 0);
	pch = pche + 1;
	pche = strchr(pch, '+');
	if (pche == NULL) return 0;
	StrBufAppendBufPlain(Function, pch, pche - pch, 0);
	pch = pche + 1;
	pche = strchr(pch, ')');
	if (pche == NULL) return 0;
	*pche = '\0';
	sscanf(pch, "%x", FunctionLine);
	StrBufAppendBufPlain(Function, pche + 1, -1, 0);
	return 1;
}
#endif
long BaseFrames = 0;
StrBuf *FullBinaryName = NULL;

void WildFireShutdown(void)
{
	close(addr2line_write_pipe[0]);
	close(addr2line_read_pipe[0]);

	FreeStrBuf(&FullBinaryName);
}

void WildFireInitBacktrace(const char *argvNull, int AddBaseFrameSkip)
{

#ifdef HAVE_BACKTRACE
	void *stack_frames[100];
	size_t size;
	long i;
	char **strings;
	StrBuf *FileName;
	StrBuf *Function;
	StrBuf *Pointer;
	StrBuf *Buf;
	unsigned int FunctionLine;
	struct stat filestats;

	FileName = NewStrBuf();
	Function = NewStrBuf();
	Pointer = NewStrBuf();
	Buf = NewStrBuf();

	BaseFrames = size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	BaseFrames --;
	BaseFrames += AddBaseFrameSkip;
	strings = backtrace_symbols(stack_frames, size);
	for (i = 1; i < size; i++) {
		if (strings != NULL){
			ParseBacktrace(strings[i], Function, 
				       FileName, 
				       &FunctionLine);
			FullBinaryName = NewStrBufDup(FileName);
			size = i;
		}
		else {
			char path[256];
			getcwd(path, sizeof(path));
			FullBinaryName = NewStrBufPlain(path, -1);
			StrBufAppendBufPlain(FullBinaryName, HKEY("/"), 0);
			StrBufAppendBufPlain(FullBinaryName, argvNull, -1, 0);
			i = size;
		 }
	}
	if ((stat(ChrPtr(FullBinaryName), &filestats)==-1) ||
	    (filestats.st_size==0)){
		FlushStrBuf(FullBinaryName);
		StrBufAppendBufPlain(FullBinaryName, argvNull, -1, 0);
		if ((stat(ChrPtr(FullBinaryName), &filestats)==-1) ||
		    (filestats.st_size==0)){
			FlushStrBuf(FullBinaryName);
			fprintf(stderr, "unable to open my binary for addr2line checking, verbose backtraces won't work.\n");
		}
	}
	free(strings);
	FreeStrBuf(&FileName);
	FreeStrBuf(&Function);
	FreeStrBuf(&Pointer);
	FreeStrBuf(&Buf);
	if (StrLength(FullBinaryName) > 0)
		start_addr2line_daemon(ChrPtr(FullBinaryName));
#endif


}


JsonValue *WildFireException(const char *Filename, long FileLen,
			     long LineNo,
			     StrBuf *Message,
			     int StackOffset)
{
	JsonValue *ExcClass;
	JsonValue *Val;
	Val = NewJsonArray(NULL, 0);

	JsonArrayAppend(Val, WFInfo(Filename, FileLen,
				    LineNo, eEXCEPTION));

	ExcClass = NewJsonObject(WF_MsgStrs[eTRACE].Key, 
				 WF_MsgStrs[eTRACE].len);
	
	JsonArrayAppend(Val, ExcClass);
	JsonObjectAppend(ExcClass, 
			 NewJsonPlainString(HKEY("Class"), 
					    HKEY("Exception")));
	JsonObjectAppend(ExcClass, 
			 NewJsonString(HKEY("Message"), Message));
	JsonObjectAppend(ExcClass, 
			 NewJsonPlainString(HKEY("File"), 
					    Filename, FileLen));
/*
	JsonObjectAppend(ExcClass, 
			 NewJsonPlainString(HKEY("Type"), 
					    HKEY("throw")));
*/
	JsonObjectAppend(ExcClass, 
			 NewJsonNumber(HKEY("Line"), LineNo));

#ifdef HAVE_BACKTRACE
	{
		void *stack_frames[100];
		size_t size;
		long i;
		char **strings;
		JsonValue *Trace;
		JsonValue *Frame;
		StrBuf *FileName;
		StrBuf *Function;
		StrBuf *Pointer;
		StrBuf *Buf;
		unsigned int FunctionLine;

		Trace = NewJsonArray(HKEY("Trace"));
		JsonObjectAppend(ExcClass, Trace);
		FileName = NewStrBuf();
		Function = NewStrBuf();
		Pointer = NewStrBuf();
		Buf = NewStrBuf();

		size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
		strings = backtrace_symbols(stack_frames, size);
		for (i = StackOffset + 1; i < size; i++) {
			if (strings != NULL){
				ParseBacktrace(strings[i], Function, 
					       FileName,
					       &FunctionLine);
				
			}
			StrBufPrintf(Pointer, "%p\n", stack_frames[i]);
			
			addr2lineBacktrace(Function, 
					   FileName, 
					   Pointer, 
					   Buf, 
					   &FunctionLine);

			Frame = NewJsonObject(NULL, 0);
			JsonArrayAppend(Trace, Frame);
			JsonObjectAppend(Frame, 
					 NewJsonString(HKEY("function"), Function));
			JsonObjectAppend(Frame, 
					 NewJsonString(HKEY("file"), FileName));
			JsonObjectAppend(Frame, 
					 NewJsonNumber(HKEY("line"), FunctionLine));
			JsonObjectAppend(Frame, 
					 NewJsonArray(HKEY("args")));/* not supportet... */

			FunctionLine = 0;
			FlushStrBuf(FileName);
			FlushStrBuf(Function);
			FlushStrBuf(Pointer);
		}
		free(strings);
		FreeStrBuf(&FileName);
		FreeStrBuf(&Function);
		FreeStrBuf(&Pointer);
		FreeStrBuf(&Buf);
	}
#endif
	return Val;
}

void WildFireSerializePayload(StrBuf *JsonBuffer, StrBuf *OutBuf, int *MsgCount, AddHeaderFunc AddHdr)
{
	int n = *MsgCount;
	StrBuf *Buf;
	StrBuf *HeaderName;
	StrBuf *N; 
	const char Concatenate[] = "\\";
	const char empty[] = "";
	const char *Cat;
	StrBuf *Header;

	if (OutBuf == NULL)
		Header = NewStrBuf();
	if (*MsgCount == 0) {
		if (OutBuf != NULL) {
			StrBufAppendBufPlain(OutBuf, 
					     HKEY( 
						     "X-Wf-Protocol-1" 
						     ": "
						     "http://meta.wildfirehq.org/Protocol/JsonStream/0.2\r\n"), 0);
			StrBufAppendBufPlain(OutBuf, 
					     HKEY( 
						     "X-Wf-1-Plugin-1" 
						     ": " 
						     "http://meta.firephp.org/Wildfire/Plugin/FirePHP/Library-FirePHPCore/0.2.0\r\n"), 0);
			StrBufAppendBufPlain(OutBuf, 
					     HKEY(
						     "X-Wf-1-Structure-1"
						     ": "
						     "http://meta.firephp.org/Wildfire/Structure/FirePHP/FirebugConsole/0.1\r\n"), 0);
		}
		else {
			AddHdr("X-Wf-Protocol-1", 
			       "http://meta.wildfirehq.org/Protocol/JsonStream/0.2");
			AddHdr("X-Wf-1-Plugin-1",
			       "http://meta.firephp.org/Wildfire/Plugin/FirePHP/Library-FirePHPCore/0.2.0");
			AddHdr("X-Wf-1-Structure-1",
			       "http://meta.firephp.org/Wildfire/Structure/FirePHP/FirebugConsole/0.1");
		}
	}

	N = NewStrBuf();
	StrBufPrintf(N, "%d", StrLength(JsonBuffer));
	Buf = NewStrBufPlain(NULL, 1024);
	HeaderName = NewStrBuf();

	while (StrLength(JsonBuffer) > 0) {
		FlushStrBuf(Buf);
		StrBufPrintf(HeaderName, "X-Wf-"WF_MAJOR"-"WF_STRUCTINDEX"-"WF_SUB"-%d", n);
		if (StrLength(JsonBuffer) > 800) {
			StrBufAppendBufPlain(Buf, ChrPtr(JsonBuffer), 800, 0);
			StrBufCutLeft(JsonBuffer, 800);
			Cat = Concatenate;
		}
		else {
			StrBufAppendBuf(Buf, JsonBuffer, 0);
			FlushStrBuf(JsonBuffer);
			Cat = empty;
		}
		if (OutBuf != NULL) {
			StrBufAppendPrintf(OutBuf, 
					   "%s: %s|%s|%s\r\n", 
					   ChrPtr(HeaderName), 
					   ChrPtr(N),
					   ChrPtr(Buf), 
					   Cat);
		}
		else {
			StrBufAppendPrintf(Header, 
					   "%s|%s|%s", 
					   ChrPtr(N),
					   ChrPtr(Buf), 
					   Cat);
			AddHdr(ChrPtr(HeaderName), ChrPtr(Header));
			
		}

		FlushStrBuf(N);
		n++;
	}
	*MsgCount = n;
	if (OutBuf == NULL) {
		FreeStrBuf(&Header);
	}
	FreeStrBuf(&N);
	FreeStrBuf(&Buf);
	FreeStrBuf(&HeaderName);
}






/* this is how we do it...
void CreateWildfireSampleMessage(void)
{
	JsonValue *Error;
		
	StrBuf *Buf;
	StrBuf *Header;
	StrBuf *Json;
	int n = 1;

	Header = NewStrBuf();
	Json = NewStrBuf();

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__,  HKEY("Warn message"), eWARN);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Error message"), eERROR);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);
	FlushStrBuf(Json);
	FlushStrBuf(Header);

	Error = WildFireMessagePlain(HKEY(__FILE__), __LINE__, HKEY("Info message"), eINFO);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);
	FlushStrBuf(Json);
	FlushStrBuf(Header);


	Buf = NewStrBufPlain(HKEY("test error message"));
	Error = WildFireException(Buf, HKEY(__FILE__), __LINE__, 1);
	SerializeJson(Json, Error);
	WildFireSerializePayload(Json, Header, &n, NULL);
	StrBufAppendBuf(WC->HBuf, Header, 0);
	DeleteJSONValue(Error);

	FlushStrBuf(Json);
	FlushStrBuf(Header);

}

*/
