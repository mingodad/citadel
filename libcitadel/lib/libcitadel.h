/*
 * Header file for libcitadel
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


/* protect against double includes */
#ifndef LIBCITADEL_H
#define LIBCITADEL_H


/*
 * since we reference time_t...
 */
#include <time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>

#define LIBCITADEL_VERSION_NUMBER	812

/*
 * Here's a bunch of stupid magic to make the MIME parser portable.
 */
#ifndef SIZ
#define SIZ	4096
#endif


typedef enum AXLevel {
	AxDeleted = 0,
	AxNewU = 1,
	AxProbU = 2,
	AxLocU = 3,
	AxNetU = 4,
	AxPrefU = 5,
	AxAideU = 6
}eUsrAxlvl;

enum RoomNetCfg {
	subpending,
	unsubpending,
	lastsent, /* Server internal use only */
	ignet_push_share,
	listrecp,
	digestrecp,
	pop3client,
	rssclient,
	participate,
	maxRoomNetCfg
};

enum GNET_POP3_PARTS { /* pop3client splits into these columns: */
	GNET_POP3_HOST = 1,
	GNET_POP3_USER = 2,
	GNET_POP3_PASS = 4,
	GNET_POP3_DONT_DELETE_REMOTE = 4,
	GNET_POP3_INTERVAL = 5
};

enum GNET_PUSH_SHARE { /* ignet_push_share splits into these columns: */
	GNET_IGNET_NODE = 1,
	GNET_IGNET_ROOM = 2
};

typedef enum __GPEXWhichPolicy {
	roompolicy,
	floorpolicy,
	sitepolicy,
	mailboxespolicy,
	maxpolicy
}GPEXWhichPolicy;

/*
 * View definitions.
 * Note that not all views are implemented in all clients.
 */
typedef enum _room_views {
	VIEW_BBS		= 0,	/* Bulletin board view */
	VIEW_MAILBOX		= 1,	/* Mailbox summary */
	VIEW_ADDRESSBOOK	= 2,	/* Address book view */
	VIEW_CALENDAR		= 3,	/* Calendar view */
	VIEW_TASKS		= 4,	/* Tasks view */
	VIEW_NOTES		= 5,	/* Notes view */
	VIEW_WIKI		= 6,	/* Wiki view */
	VIEW_CALBRIEF		= 7,	/* Brief Calendar view */
	VIEW_JOURNAL		= 8,	/* Journal view */
	VIEW_DRAFTS		= 9,	/* Drafts view */
	VIEW_BLOG		= 10,	/* Blog view */
	VIEW_QUEUE		= 11,   /* SMTP/... QUEUE rooms */
	VIEW_MAX,
	VIEW_JSON_LIST,
} ROOM_VIEWS;

#define BLOG_EUIDBUF_SIZE	40

#ifndef IsEmptyStr
#define IsEmptyStr(a) ((a)[0] == '\0')
#endif


/*
 * another word to indicate n/a for a pointer if NULL already has a "meaning"
 */
extern const char *StrBufNOTNULL;

/*
 * Misc declarations
 */

char *libcitadel_version_string(void);
int libcitadel_version_number(void);
void StartLibCitadel(size_t basesize);
void ShutDownLibCitadel(void);

/*
 * MIME parser declarations
 */

long extract_key(char *target, char *source, long sourcelen, char *key, long keylen, char KeyEnd);


typedef void (*MimeParserCallBackType)(char *cbname,
				       char *cbfilename,
				       char *cbpartnum,
				       char *cbdisp,
				       void *cbcontent,
				       char *cbtype,
				       char *cbcharset,
				       size_t cblength,
				       char *cbencoding,
				       char *cbid,
				       void *cbuserdata);

void mime_parser(char *content_start, char *content_end,
		 MimeParserCallBackType CallBack,
		 MimeParserCallBackType PreMultiPartCallBack,
		 MimeParserCallBackType PostMultiPartCallBack,
		 void *userdata,
		 int dont_decode);



char *fixed_partnum(char *);
void mime_decode(char *partnum,
		 char *part_start, size_t length,
		 char *content_type, char *charset, char *encoding,
		 char *disposition,
		 char *id,
		 char *name, char *filename,
		 MimeParserCallBackType CallBack,
		 MimeParserCallBackType PreMultiPartCallBack,
		 MimeParserCallBackType PostMultiPartCallBack,
		 void *userdata,
		 int dont_decode);
int mime_decode_now (char *part_start, 
		     size_t length,
		     char *encoding,
		     char **decoded,
		     size_t *bytes_decoded);
void the_mime_parser(char *partnum,
		     char *content_start, char *content_end,
		     MimeParserCallBackType CallBack,
		     MimeParserCallBackType PreMultiPartCallBack,
		     MimeParserCallBackType PostMultiPartCallBack,
		     void *userdata,
		     int dont_decode);

typedef struct StrBuf StrBuf;

#define strof(a) #a
#define CStrOf(a) #a, sizeof(#a) - 1
typedef struct _ConstStr {
	const char *Key;
	long len;
}ConstStr;
#define CKEY(a) (a).Key, (a).len

StrBuf* NewStrBuf(void);
StrBuf* NewStrBufDup(const StrBuf *CopyMe);
StrBuf* NewStrBufPlain(const char* ptr, int nChars);
long StrBufShrinkToFit(StrBuf *Buf, int Force);
void ReAdjustEmptyBuf(StrBuf *Buf, long ThreshHold, long NewSize);
void NewStrBufDupAppendFlush(StrBuf **CreateRelpaceMe, StrBuf *CopyFlushMe, const char *NoMe, int KeepOriginal);

int StrBufPlain(StrBuf *Buf, const char* ptr, int nChars);
StrBuf* _NewConstStrBuf(const char* StringConstant, size_t SizeOfStrConstant);
#define NewConstStrBuf(a) _NewConstStrBuf(a, sizeof(a))
void FreeStrBuf (StrBuf **FreeMe);
char *SmashStrBuf (StrBuf **SmashMe);
void HFreeStrBuf (void *VFreeMe);
int FlushStrBuf(StrBuf *buf);
int FLUSHStrBuf(StrBuf *buf); /* expensive but doesn't leave content behind for others to find in case of errors */

const char *ChrPtr(const StrBuf *Str);
int StrLength(const StrBuf *Str);
#define SKEY(a) ChrPtr(a), StrLength(a)
long StrBufPeek(StrBuf *Buf, const char* ptr, long nThChar, char PeekValue);
long StrBufPook(StrBuf *Buf, const char* ptr, long nThChar, long nChars, char PookValue);

int StrBufTCP_read_line(StrBuf *buf, int *fd, int append, const char **Error);
int StrBufReadBLOB(StrBuf *Buf, int *fd, int append, long nBytes, const char **Error);
#define NNN_TERM 1
#define O_TERM 0
int StrBufReadBLOBBuffered(StrBuf *Buf, 
			   StrBuf *IOBuf, 
			   const char **BufPos,
			   int *fd, 
			   int append, 
			   long nBytes, 
			   int check, 
			   const char **Error);
int StrBufTCP_read_buffered_line(StrBuf *Line, 
				 StrBuf *buf, 
				 int *fd, 
				 int timeout, 
				 int selectresolution, 
				 const char **Error);
int StrBufTCP_read_buffered_line_fast(StrBuf *Line, 
				      StrBuf *buf, 
				      const char **Pos,
				      int *fd, 
				      int timeout, 
				      int selectresolution, 
				      const char **Error);

typedef enum _eReadState {
	eReadFail,
	eReadSuccess,
	eMustReadMore, 
	eBufferNotEmpty
} eReadState;

typedef struct _file_buffer {
	StrBuf *Buf;
	const char *ReadWritePointer;
	int fd;
	int LineCompleted;
	int nBlobBytesWanted;
} IOBuffer;


typedef struct __fd_iobuffer {
	IOBuffer *IOB;
	int OtherFD;
	int SplicePipe[2];
	int PipeSize;
	long TotalSendSize;
	long TotalSentAlready;
	long TotalReadAlready;
	long ChunkSize;
	long ChunkSendRemain;
	StrBuf *ChunkBuffer; /* just used if we don't have sendfile */
} FDIOBuffer;


void FDIOBufferInit(FDIOBuffer *FDB, IOBuffer *IO, int FD, long TotalSendSize);
void FDIOBufferDelete(FDIOBuffer *FDB);
int FileSendChunked(FDIOBuffer *FDB, const char **Err);
int FileRecvChunked(FDIOBuffer *FDB, const char **Err);
int FileMoveChunked(FDIOBuffer *FDB, const char **Err);
eReadState WriteIOBAlreadyRead(FDIOBuffer *FDB, const char **Error);

long StrBuf_read_one_chunk_callback (int fd, short event, IOBuffer *FB);
int StrBuf_write_one_chunk_callback(int fd, short event, IOBuffer *FB);

eReadState StrBufChunkSipLine(StrBuf *LineBuf, IOBuffer *FB);
eReadState StrBufCheckBuffer(IOBuffer *FB);
long IOBufferStrLength(IOBuffer *FB);

int StrBufSipLine(StrBuf *LineBuf, const StrBuf *Buf, const char **Ptr);
int StrBufReplaceToken(StrBuf *Buf, long where, long HowLong, const char *Repl, long ReplLen);
int StrBufExtract_tokenFromStr(StrBuf *dest, const char *Source, long SourceLen, int parmnum, char separator);
int StrBufExtract_token(StrBuf *dest, const StrBuf *Source, int parmnum, char separator);
int StrBufSub(StrBuf *dest, const StrBuf *Source, unsigned long Offset, size_t nChars);

unsigned long StrBufExtract_unsigned_long(const StrBuf* Source, int parmnum, char separator);
long StrBufExtract_long(const StrBuf* Source, int parmnum, char separator);
int StrBufExtract_int(const StrBuf* Source, int parmnum, char separator);
int StrBufNum_tokens(const StrBuf *source, char tok);
int StrBufRemove_token(StrBuf *Source, int parmnum, char separator);

int StrBufHaveNextToken(const StrBuf *Source, const char **pStart);
int StrBufExtract_NextToken(StrBuf *dest, const StrBuf *Source, const char **pStart, char separator);
int StrBufSkip_NTokenS(const StrBuf *Source, const char **pStart, char separator, int nTokens);
unsigned long StrBufExtractNext_unsigned_long(const StrBuf* Source, const char **pStart, char separator);
long StrBufExtractNext_long(const StrBuf* Source, const char **pStart, char separator);
int StrBufExtractNext_int(const StrBuf* Source, const char **pStart, char separator);


void StrBufAppendBufPlain(StrBuf *Buf, const char *AppendBuf, long AppendSize, unsigned long Offset);
void StrBufAppendBuf(StrBuf *Buf, const StrBuf *AppendBuf, unsigned long Offset);
size_t CurlFillStrBuf_callback(void *ptr, size_t size, size_t nmemb, void *stream);
void StrBufAppendPrintf(StrBuf *Buf, const char *format, ...);
#ifdef SHOW_ME_VAPPEND_PRINTF
/* so owe don't create an include depndency, this is just visible on demand. */
void StrBufVAppendPrintf(StrBuf *Buf, const char *format, va_list ap);
#endif
void StrBufPrintf(StrBuf *Buf, const char *format, ...) __attribute__((__format__(__printf__,2,3)));
void StrBufCutLeft(StrBuf *Buf, int nChars);
void StrBufCutRight(StrBuf *Buf, int nChars);
void StrBufCutAt(StrBuf *Buf, int AfternChars, const char *At);
void StrBufTrim(StrBuf *Buf);
void StrBufSpaceToBlank(StrBuf *Buf);
void StrBufStripAllBut(StrBuf *Buf, char leftboundary, char rightboundary);
void StrBufUpCase(StrBuf *Buf);
void StrBufLowerCase(StrBuf *Buf);
void StrBufStripSlashes(StrBuf *Dir, int RemoveTrailingSlash);
void StrBufEUid_unescapize(StrBuf *target, const StrBuf *source);
void StrBufEUid_escapize(StrBuf *target, const StrBuf *source);

void StrBufToUnixLF(StrBuf *buf);
void StrBufReplaceChars(StrBuf *buf, char search, char replace);

int CompressBuffer(StrBuf *Buf);
void StrBufConvert(StrBuf *ConvertBuf, StrBuf *TmpBuf, void *pic);
void ctdl_iconv_open(const char *tocode, const char *fromcode, void *pic);
void StrBuf_RFC822_2_Utf8(StrBuf *Target, 
			  const StrBuf *DecodeMe, 
			  const StrBuf* DefaultCharset, 
			  StrBuf *FoundCharset, 
			  StrBuf *ConvertBuf, 
			  StrBuf *ConvertBuf2);
/* deprecated old version: */
void StrBuf_RFC822_to_Utf8(StrBuf *Target, const StrBuf *DecodeMe, const StrBuf* DefaultCharset, StrBuf *FoundCharset);

int StrBufDecodeBase64(StrBuf *Buf);
int StrBufDecodeHex(StrBuf *Buf);
int StrBufRFC2047encode(StrBuf **target, const StrBuf *source);
StrBuf *StrBufSanitizeEmailRecipientVector(const StrBuf *Recp, 
					   StrBuf *UserName, 
					   StrBuf *EmailAddress,
					   StrBuf *EncBuf);
int StrBufSanitizeAscii(StrBuf *Buf, const char Mute);
#define LB			(1)		/* Internal escape chars */
#define RB			(2)
#define QU			(3)
void StrBufUrlescAppend(StrBuf *OutBuf, const StrBuf *In, const char *PlainIn);
void StrBufUrlescUPAppend(StrBuf *OutBuf, const StrBuf *In, const char *PlainIn);
void StrBufHexescAppend(StrBuf *OutBuf, const StrBuf *In, const char *PlainIn);
void StrBufHexEscAppend(StrBuf *OutBuf, const StrBuf *In, const unsigned char *PlainIn, long PlainInLen);
long StrEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn, int nbsp, int nolinebreaks);
long StrECMAEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn);
long StrHtmlEcmaEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn, int nbsp, int nolinebreaks);
void StrMsgEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn);
void StrIcalEscAppend(StrBuf *Target, const StrBuf *Source, const char *PlainIn);

long StrTol(const StrBuf *Buf);
int StrToi(const StrBuf *Buf);
int StrBufIsNumber(const StrBuf *Buf);
long StrBuf_Utf8StrLen(StrBuf *Buf);
long StrBuf_Utf8StrCut(StrBuf *Buf, int maxlen);

const char *GuessMimeType(const char *data, size_t dlen);
const char* GuessMimeByFilename(const char *what, size_t len);

/* Run once at Programstart */
int LoadIconDir(const char *DirName);

/* Select the icon for a given MIME type */
const char *GetIconFilename(char *MimeType, size_t len);


/* URL parsing & connection data */
typedef struct ParsedURL ParsedURL;
struct ParsedURL {
	int Priority;
	StrBuf *URL;
	StrBuf *UrlWithoutCred;
	StrBuf *CurlCreds;
	unsigned Port;
	const char *Host;
	const char *User;
	const char *Pass;
	const char *LocalPart;
	const char *PlainUrl;
	int IsIP;
	int IPv6;
	int af;
	struct hostent *HEnt;
	struct sockaddr_in6 Addr;
	ParsedURL *Next;
};

void FreeURL(ParsedURL** Url);
int ParseURL(ParsedURL **Url, StrBuf *UrlStr, unsigned short DefaultPort);
void CurlPrepareURL(ParsedURL *Url);

/* tools */


int safestrncpy(char *dest, const char *src, size_t n);
int num_tokens (const char *source, char tok);
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen);
long grab_token(char **dest, const char *source, int parmnum, char separator);
int extract_int (const char *source, int parmnum);
long extract_long (const char *source, int parmnum);
unsigned long extract_unsigned_long(const char *source, int parmnum);
void CtdlInitBase64Table(void);
size_t CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen, int linebreaks);
int CtdlDecodeBase64(char *dest, const char *source, size_t length);
unsigned int decode_hex(char *Source);
int CtdlDecodeQuotedPrintable(char *decoded, char *encoded, int sourcelen);
void StripSlashes(char *Dir, int TrailingSlash);
size_t striplt(char *);
int haschar(const char *st, int ch);
void remove_token(char *source, int parmnum, char separator);
void fmt_date(char *buf, size_t n, time_t thetime, int seconds);
int is_msg_in_sequence_set(const char *mset, long msgnum);
char *memreadline(char *start, char *buf, int maxlen);
char *memreadlinelen(char *start, char *buf, int maxlen, int *retlen);
const char *cmemreadline(const char *start, char *buf, int maxlen);
const char *cmemreadlinelen(const char *start, char *buf, int maxlen, int *retlen);
#define IsEmptyStr(a) ((a)[0] == '\0')
#define num_parms(source)		num_tokens(source,(char)'|')
int stripout(char *str, char leftboundary, char rightboundary);
long stripallbut(char *str, char leftboundary, char rightboundary);
char *myfgets(char *s, int size, FILE *stream);
void urlesc(char *outbuf, size_t oblen, char *strbuf);
char *CtdlTempFileName(char *prefix1, int prefix2);
FILE *CtdlTempFile(void);
void generate_uuid(char *buf);
char *bmstrcasestr(char *text, const char *pattern);
char *bmstrcasestr_len(char *text, size_t textlen, const char *pattern, size_t patlen);
const char *cbmstrcasestr(const char *text, const char *pattern);
const char *cbmstrcasestr_len(const char *text, size_t textlen, const char *pattern, size_t patlen);
void CtdlMakeTempFileName(char *name, int len);
char *rfc2047encode(char *line, long length);
int is_msg_in_mset(const char *mset, long msgnum);
int pattern2(char *search, char *patn);
void stripltlen(char *, int *);
char *html_to_ascii(const char *inputmsg, int msglen, int screenwidth, int do_citaformat);
void LoadEntityList(char *FileName);



/* vCard stuff */

#define CTDL_VCARD_MAGIC	0xa1f9

/* This data structure represents a vCard object currently in memory. */
struct vCard {
	int magic;
	int numprops;
	struct vCardProp {
		char *name;
		char *value;
	} *prop;
};


struct vCard *vcard_new(void);
void vcard_add_prop(struct vCard *v, char *propname, char *propvalue);
struct vCard *vcard_load(char *vtext);
struct vCard *VCardLoad(StrBuf *vbtext);

void vcard_free(struct vCard *);
void vcard_set_prop(struct vCard *v, char *name, char *value, int append);
char *vcard_get_prop(struct vCard *v, char *propname, int is_partial,
			int instance, int return_propname);
char *vcard_serialize(struct vCard *);
void vcard_fn_to_n(char *vname, char *n, size_t vname_size);
void remove_charset_attribute(char *strbuf);
long StrBufUnescape(StrBuf *Buf, int StripBlanks);

/*
 * Hash list implementation for Citadel
 */
#define HKEY(a) a, (sizeof(a) - 1)

typedef struct HashList HashList;

typedef struct HashKey HashKey;

typedef struct HashPos HashPos;

typedef void (*DeleteHashDataFunc)(void * Data);
typedef const char *(*PrintHashContent)(void * Data);
typedef int (*CompareFunc)(const void* Item1, const void*Item2);
typedef long (*HashFunc)(const char *Str, long Len);
typedef void (*TransitionFunc) (void *Item1, void *Item2, int Odd);
typedef void (*PrintHashDataFunc) (const char *Key, void *Item, int Odd);

long Flathash(const char *str, long len);
long lFlathash(const char *str, long len);
#define IKEY(a) (const char*) &a, sizeof(a)
#define LKEY(a) (const char*) &a, sizeof(a)

int TestValidateHash(HashList *TestHash);

HashList *NewHash(int Uniq, HashFunc F);
void DeleteHash(HashList **Hash);
void DeleteHashContent(HashList **Hash);
void HDeleteHash(void *vHash);
int GetHash(HashList *Hash, const char *HKey, long HKLen, void **Data);
void Put(HashList *Hash, const char *HKey, long HKLen, void *Data, DeleteHashDataFunc DeleteIt);
int GetKey(HashList *Hash, char *HKey, long HKLen, void **Data);
int GetHashKeys(HashList *Hash, char ***List);
int dbg_PrintHash(HashList *Hash, PrintHashContent first, PrintHashContent Second);
int PrintHash(HashList *Hash, TransitionFunc Trans, PrintHashDataFunc PrintEntry);
HashPos *GetNewHashPos(HashList *Hash, int StepWidth);
int GetHashPosFromKey(HashList *Hash, const char *HKey, long HKLen, HashPos *At);
int DeleteEntryFromHash(HashList *Hash, HashPos *At);
int GetHashPosCounter(HashList *Hash, HashPos *At);
void DeleteHashPos(HashPos **DelMe);
int NextHashPos(HashList *Hash, HashPos *At);
int GetHashPos(HashList *Hash, HashPos *At, long *HKLen, const char **HashKey, void **Data);
int GetNextHashPos(HashList *Hash, HashPos *At, long *HKLen, const char **HashKey, void **Data);
int GetHashAt(HashList *Hash,long At, long *HKLen, const char **HashKey, void **Data);
void SortByHashKey(HashList *Hash, int Order);
void SortByHashKeyStr(HashList *Hash);
int GetCount(HashList *Hash);
const void *GetSearchPayload(const void *HashVoid);
void SortByPayload(HashList *Hash, CompareFunc SortBy);
void reference_free_handler(void *ptr);
int HashLittle(const void *key, size_t length);

typedef struct MSet MSet;
int ParseMSet(MSet **MsetList, StrBuf *MSetStr);
int IsInMSetList(MSet *MSetList, long MsgNo);
void DeleteMSet(MSet **FreeMe);

void convert_spaces_to_underscores(char *str);
int CheckEncode(const char *pch, long len, const char *pche);

/*
 * Convert 4 bytes char into an Integer.
 * usefull for easy inexpensive hashing 
 * of for char strings.
 */
#define CHAR4TO_INT(a) ((int) (a[0] | (a[1]<<8) | (a[2]<<16) | (a[3]<<24)))

/* vNote implementation */

#define CTDL_VNOTE_MAGIC	0xa1fa

struct vnote {
	int magic;
	char *uid;
	char *summary;
	char *body;
	int pos_left;
	int pos_top;
	int pos_width;
	int pos_height;
	int color_red;
	int color_green;
	int color_blue;
};



struct vnote *vnote_new(void);
struct vnote *vnote_new_from_str(char *s);
void vnote_free(struct vnote *v);
char *vnote_serialize(struct vnote *v);
void vnote_serialize_output_field(char *append_to, char *field, char *label);




/*
 * Create JSON style structures in C plus serialize them to one string
 */

typedef struct JsonValue JsonValue;


void DeleteJSONValue(void *vJsonValue);

JsonValue *NewJsonObject(const char *Key, long keylen);

JsonValue *NewJsonArray(const char *Key, long keylen);

JsonValue *NewJsonNumber(const char *Key, long keylen, long Number);

JsonValue *NewJsonBigNumber(const char *Key, long keylen, double Number);

JsonValue *NewJsonString(const char *Key, long keylen, StrBuf *CopyMe);

JsonValue *NewJsonPlainString(const char *Key, long keylen, const char *CopyMe, long len);

JsonValue *NewJsonNull(const char *Key, long keylen);

JsonValue *NewJsonBool(const char *Key, long keylen, int value);

void JsonArrayAppend(JsonValue *Array, JsonValue *Val);

void JsonObjectAppend(JsonValue *Array, JsonValue *Val);

void SerializeJson(StrBuf *Target, JsonValue *Val, int FreeVal);



/*
 * Citadels Wildfire implementation, see 
 * http://www.firephp.org/Wiki/Reference/Protocol
 * and http://wildfirehq.org/ for details
 */
typedef void (*AddHeaderFunc)(const char *HdrName, const char *HdrValue);

typedef enum _WF_MessageType {
	eLOG, 
	eINFO,
	eWARN,
	eERROR,
	eTRACE,
	eEXCEPTION
} WF_MessageType;

JsonValue *WildFireException(const char *Filename, long FileLen,
			     long LineNo,
			     StrBuf *Message,
			     int StackOffset);

void WildFireAddArray(JsonValue *ReportBase, JsonValue *Array, WF_MessageType Type);

JsonValue *WildFireMessagePlain(const char *Filename, long fnlen,
				   long LineNo,
				   const char *Message, long len, 
				   WF_MessageType Type);

JsonValue *WildFireMessage(const char *Filename, long fnlen,
			   long lineno,
			   StrBuf *Msg, 
			   WF_MessageType Type);

void WildFireInitBacktrace(const char *argvNull, int AddBaseFrameSkip);

void WildFireSerializePayload(StrBuf *JsonBuffer, StrBuf *OutBuf, int *MsgCount, AddHeaderFunc AddHdr);

#define WF_MAJOR "1"
#define WF_STRUCTINDEX "1"
#define WF_SUB "1"

extern ConstStr RoomNetCfgStrs[maxRoomNetCfg];

/* Convenience function to "perform a function and cut a log record if result != 0" */
#define CtdlLogResult(x) if (x) syslog(LOG_CRIT, "%s:%d %s", __FILE__, __LINE__, strerror(errno))

/* a nice consistent place to define how we turn a message id into a thread id hash */
#define ThreadIdHash(Buf) abs(HashLittle(ChrPtr(Buf), StrLength(Buf)))


#endif	// LIBCITADEL_H
