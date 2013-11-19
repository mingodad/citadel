typedef void* (*GetTokenDataFunc)(void);

typedef struct __TokenHandler {
	HashList *Properties;
	GetTokenDataFunc GetToken;
}TokenHandler;

typedef struct __PropertyHandler {
	const char *NameSpace;
	long NameSpaceLen;
	const char *Token;
	long TokenLen;
	const char *Property;
	long PropertyLen;
	long offset;
} PropertyHandler;

void XUnbuffer(void);
void XPutBody(const char *Str, long Len);
void XPutProp(const char *Str, long Len);
#define XPutSProp(STR) XPutProp(ChrPtr(STR), StrLength(STR))
void XPut(const char *Str, long Len);
#define XPUT(CONSTSTR) XPut(CONSTSTR, sizeof(CONSTSTR) -1)

void XPrintf(const char *Format, ...);

#define XCLOSED (1<<0)
void XPrint(const char *Token, long tlen,
	    int Flags,
	    ...);

#define TYPE_STR 1
#define TYPE_OPTSTR 2
#define TYPE_INT 3
#define TYPE_BODYSTR 4
#define TYPE_ARGEND 5
#define XPROPERTY(NAME, VALUE, VLEN) TYPE_STR, NAME, sizeof(NAME)-1, VALUE, VLEN
#define XSPROPERTY(NAME, VALUE) TYPE_STR, NAME, sizeof(NAME)-1, ChrPtr(VALUE), StrLength(VALUE)
#define XOPROPERTY(NAME, VALUE, VLEN) TYPE_OPTSTR, NAME, sizeof(NAME)-1, VALUE, VLEN
#define XCPROPERTY(NAME, VALUE) TYPE_STR, NAME, sizeof(NAME)-1, VALUE, sizeof(VALUE) - 1
#define XIPROPERTY(NAME, LVALUE) TYPE_INT, NAME, SIZEOF(NAME)-1
#define XBODY(VALUE, VLEN) TYPE_BODYSTR, VALUE, VLEN
#define XCFGBODY(WHICH) TYPE_BODYSTR, config.WHICH, configlen.WHICH

void XMPP_RegisterTokenProperty(const char *NS, long NSLen,
				const char *Token, long TLen,
				const char *Property, long PLen,
				GetTokenDataFunc GetToken,
				long offset);
