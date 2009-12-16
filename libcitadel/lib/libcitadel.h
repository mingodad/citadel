/*
 * $Id$
 *
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
#define LIBCITADEL_VERSION_NUMBER	766

/*
 * Here's a bunch of stupid magic to make the MIME parser portable.
 */
#ifndef SIZ
#define SIZ	4096
#endif


/* Logging levels - correspond to syslog(3) */
enum LogLevel {
	/* When about to exit the server for an unrecoverable error */
	 CTDL_EMERG,	/* system is unusable */
	/* Manual intervention is required to avoid an abnormal exit */
	 CTDL_ALERT,	/* action must be taken immediately */
	/* The server can continue to run with degraded functionality */
	 CTDL_CRIT,	/* critical conditions */
	/* An error occurs but the server continues to run normally */
	 CTDL_ERR,	/* error conditions */
	/* An abnormal condition was detected; server will continue normally */
	 CTDL_WARNING,	/* warning conditions */
	/* Normal messages (login/out, activity, etc.) */
	 CTDL_NOTICE,	/* normal but significant condition */
	/* Unimportant progress messages, etc. */
	 CTDL_INFO,	/* informational */
	/* Debugging messages */
	 CTDL_DEBUG	/* debug-level messages */
};


/*
 * View definitions.
 * Note that not all views are implemented in all clients.
 */
#define	VIEW_BBS		0	/* Bulletin board view */
#define VIEW_MAILBOX		1	/* Mailbox summary */
#define VIEW_ADDRESSBOOK	2	/* Address book view */
#define VIEW_CALENDAR		3	/* Calendar view */
#define VIEW_TASKS		4	/* Tasks view */
#define VIEW_NOTES		5	/* Notes view */
#define	VIEW_WIKI		6	/* Wiki view */
#define VIEW_CALBRIEF		7	/* Brief Calendar view */
#define VIEW_JOURNAL		8	/* Journal view */


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

void extract_key(char *target, char *source, long sourcelen, char *key, long keylen);

void mime_parser(char *content_start, char *content_end,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			char *cbid,
			void *cbuserdata),
		void (*PreMultiPartCallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			char *cbid,
			void *cbuserdata),
		void (*PostMultiPartCallBack)
			(char *cbname,
			char *cbfilename,
			char *cbpartnum,
			char *cbdisp,
			void *cbcontent,
			char *cbtype,
			char *cbcharset,
			size_t cblength,
			char *cbencoding,
			char *cbid,
			void *cbuserdata),
		void *userdata,
		int dont_decode
);



char *fixed_partnum(char *);
void mime_decode(char *partnum,
		 char *part_start, size_t length,
		 char *content_type, char *charset, char *encoding,
		 char *disposition,
		 char *id,
		 char *name, char *filename,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   char *cbid,
		   void *cbuserdata),
		 void (*PreMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   char *cbid,
		   void *cbuserdata),
		 void (*PostMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   char *cbid,
		   void *cbuserdata),
		  void *userdata,
		  int dont_decode
);
void the_mime_parser(char *partnum,
		     char *content_start, char *content_end,
		     void (*CallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       char *cbid,
		       void *cbuserdata),
		     void (*PreMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       char *cbid,
		       void *cbuserdata),
		     void (*PostMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       char *cbid,
		       void *cbuserdata),
		      void *userdata,
		      int dont_decode
);

typedef struct StrBuf StrBuf;

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

int StrBufSipLine(StrBuf *LineBuf, StrBuf *Buf, const char **Ptr);
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
void StrBufUpCase(StrBuf *Buf);
void StrBufLowerCase(StrBuf *Buf);
void StrBufStripSlashes(StrBuf *Dir, int RemoveTrailingSlash);
void StrBufEUid_unescapize(StrBuf *target, const StrBuf *source);
void StrBufEUid_escapize(StrBuf *target, const StrBuf *source);

void StrBufReplaceChars(StrBuf *buf, char search, char replace);

int CompressBuffer(StrBuf *Buf);
void StrBufConvert(StrBuf *ConvertBuf, StrBuf *TmpBuf, void *pic);
void ctdl_iconv_open(const char *tocode, const char *fromcode, void *pic);
void StrBuf_RFC822_to_Utf8(StrBuf *Target, const StrBuf *DecodeMe, const StrBuf* DefaultCharset, StrBuf *FoundCharset);
int StrBufDecodeBase64(StrBuf *Buf);
int StrBufDecodeHex(StrBuf *Buf);
int StrBufRFC2047encode(StrBuf **target, const StrBuf *source);
int StrBufSanitizeAscii(StrBuf *Buf, const char Mute);
#define LB			(1)		/* Internal escape chars */
#define RB			(2)
#define QU			(3)
void StrBufUrlescAppend(StrBuf *OutBuf, const StrBuf *In, const char *PlainIn);
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
#define IsEmptyStr(a) ((a)[0] == '\0')
#define num_parms(source)		num_tokens(source,(char)'|')
int stripout(char *str, char leftboundary, char rightboundary);
void stripallbut(char *str, char leftboundary, char rightboundary);
char *myfgets(char *s, int size, FILE *stream);
void urlesc(char *outbuf, size_t oblen, char *strbuf);
char *CtdlTempFileName(char *prefix1, int prefix2);
FILE *CtdlTempFile(void);
void generate_uuid(char *buf);
char *bmstrcasestr(char *text, char *pattern);
char *bmstrcasestr_len(char *text, size_t textlen, char *pattern, size_t patlen);
void CtdlMakeTempFileName(char *name, int len);
char *rfc2047encode(char *line, long length);
int is_msg_in_mset(const char *mset, long msgnum);
int pattern2(char *search, char *patn);
void stripltlen(char *, int *);
char *html_to_ascii(char *inputmsg, int msglen, int screenwidth, int do_citaformat);
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
typedef int (*HashFunc)(const char *Str, long Len);
typedef void (*TransitionFunc) (void *Item1, void *Item2, int Odd);
typedef void (*PrintHashDataFunc) (const char *Key, void *Item, int Odd);

int Flathash(const char *str, long len);
#define IKEY(a) (const char*) &a, sizeof(a)

HashList *NewHash(int Uniq, HashFunc F);
void DeleteHash(HashList **Hash);
void HDeleteHash(void *vHash);
int GetHash(HashList *Hash, const char *HKey, long HKLen, void **Data);
void Put(HashList *Hash, const char *HKey, long HKLen, void *Data, DeleteHashDataFunc DeleteIt);
int GetKey(HashList *Hash, char *HKey, long HKLen, void **Data);
int GetHashKeys(HashList *Hash, char ***List);
int dbg_PrintHash(HashList *Hash, PrintHashContent first, PrintHashContent Second);
int PrintHash(HashList *Hash, TransitionFunc Trans, PrintHashDataFunc PrintEntry);
HashPos *GetNewHashPos(HashList *Hash, int StepWidth);
int GetHashPosCounter(HashList *Hash, HashPos *At);
void DeleteHashPos(HashPos **DelMe);
int GetNextHashPos(HashList *Hash, HashPos *At, long *HKLen, const char **HashKey, void **Data);
int GetHashAt(HashList *Hash,long At, long *HKLen, const char **HashKey, void **Data);
void SortByHashKey(HashList *Hash, int Order);
void SortByHashKeyStr(HashList *Hash);
int GetCount(HashList *Hash);
const void *GetSearchPayload(const void *HashVoid);
void SortByPayload(HashList *Hash, CompareFunc SortBy);
void generic_free_handler(void *ptr);
void reference_free_handler(void *ptr);
int HashLittle(const void *key, size_t length);


void convert_spaces_to_underscores(char *str);

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


#endif	// LIBCITADEL_H
