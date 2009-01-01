
extern HashList *Conditionals;
extern HashList *GlobalNS;
extern HashList *Iterators;
extern HashList *WirelessTemplateCache;
extern HashList *WirelessLocalTemplateCache;
extern HashList *TemplateCache;
extern HashList *LocalTemplateCache;


#define TYPE_STR   1
#define TYPE_LONG  2
#define TYPE_PREFSTR 3
#define TYPE_PREFINT 4
#define TYPE_GETTEXT 5
#define TYPE_BSTR 6
#define MAXPARAM  20


/*
 * \brief Values for wcs_type
 */
enum {
	WCS_STRING,       /* its a string */
	WCS_FUNCTION,     /* its a function callback */
	WCS_SERVCMD,      /* its a command to send to the citadel server */
	WCS_STRBUF,       /* its a strbuf we own */
	WCS_STRBUF_REF,   /* its a strbuf we mustn't free */
	WCS_LONG          /* its an integer */
};



typedef struct _TemplateParam {
	const char *Start;
	int Type;
	long len;
	long lvalue;
} TemplateParam;

/* make a template token a lookup key: */
#define TKEY(a) Tokens->Params[a]->Start, Tokens->Params[a]->len
typedef struct _TemplateToken {
	const StrBuf *FileName; /* Reference to print error messages; not to be freed */
	StrBuf *FlatToken;
	long Line;
	const char *pTokenStart;
	size_t TokenStart;
	size_t TokenEnd;
	const char *pTokenEnd;
	int Flags;
	void *PreEval;

	const char *pName;
	size_t NameEnd;

	int HaveParameters;
	int nParameters;
	TemplateParam *Params[MAXPARAM];
} WCTemplateToken;

typedef void (*WCHandlerFunc)();


/*
 * \brief Dynamic content for variable substitution in templates
 */
typedef struct _wcsubst {
	int wcs_type;			    /* which type of Substitution are we */
	char wcs_key[32];		    /* copy of our hashkey for debugging */
	StrBuf *wcs_value;		    /* if we're a string, keep it here */
	long lvalue;                        /* type long? keep data here */
	int ContextRequired;                /* do we require a context type? */
	WCHandlerFunc wcs_function; /* funcion hook ???*/
} wcsubst;

#define CTX_NONE 0
#define CTX_SITECFG 1
#define CTX_SESSION 2
#define CTX_INETCFG 3
#define CTX_VNOTE 4
#define CTX_WHO 5
#define CTX_PREF 6
#define CTX_NODECONF 7
#define CTX_USERLIST 8
#define CTX_MAILSUM 9
#define CTX_MIME_ATACH 10
#define CTX_FILELIST 11
#define CTX_STRBUF 12
#define CTX_LONGVECTOR 13
#define CTX_ROOMS 14
#define CTX_FLOORS 15

void RegisterNS(const char *NSName, long len, 
		int nMinArgs, 
		int nMaxArgs, 
		WCHandlerFunc HandlerFunc,
		int ContextRequired);
#define RegisterNamespace(a, b, c, d, e) RegisterNS(a, sizeof(a)-1, b, c, d, e)

typedef int (*WCConditionalFunc)(WCTemplateToken *Token, void *Context, int ContextType);
typedef struct _ConditionalStruct {
	const char *PlainName;
	int nParams;
	int ContextRequired;
	WCConditionalFunc CondF;
} ConditionalStruct;
void RegisterConditional(const char *Name, long len, 
			 int nParams,
			 WCConditionalFunc CondF, 
			 int ContextRequired);



typedef void (*SubTemplFunc)(StrBuf *TemplBuffer, void *Context, WCTemplateToken *Token);
typedef HashList *(*RetrieveHashlistFunc)(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType);
typedef void (*HashDestructorFunc) (HashList **KillMe);
void RegisterITERATOR(const char *Name, long len, /* Our identifier */
		      int AdditionalParams,       /* doe we use more parameters? */
		      HashList *StaticList,       /* pointer to webcit lifetime hashlists */
		      RetrieveHashlistFunc GetHash, /* else retrieve the hashlist by calling this function */
		      SubTemplFunc DoSubTempl,       /* call this function on each iteration for svput & friends */
		      HashDestructorFunc Destructor, /* use this function to shut down the hash; NULL if its a reference */
		      int ContextType,               /* which context do we provide to the subtemplate? */
		      int XPectContextType,          /* which context do we expct to be called in? */
		      int Flags);

#define IT_NOFLAG 0
#define IT_FLAG_DETECT_GROUPCHANGE (1<<0)

#define RegisterIterator(a, b, c, d, e, f, g, h, i) RegisterITERATOR(a, sizeof(a)-1, b, c, d, e, f, g, h, i)

void GetTemplateTokenString(WCTemplateToken *Tokens,
			    int N, 
			    const char **Value, 
			    long *len);


void SVPut(char *keyname, size_t keylen, int keytype, char *Data);
#define svput(a, b, c) SVPut(a, sizeof(a) - 1, b, c)
void SVPutLong(char *keyname, size_t keylen, long Data);
#define svputlong(a, b) SVPutLong(a, sizeof(a) - 1, b)
void svprintf(char *keyname, size_t keylen, int keytype, const char *format,...) __attribute__((__format__(__printf__,4,5)));
void SVPRINTF(char *keyname, int keytype, const char *format,...) __attribute__((__format__(__printf__,3,4)));
void SVCALLBACK(char *keyname, WCHandlerFunc fcn_ptr);
void SVCallback(char *keyname, size_t keylen,  WCHandlerFunc fcn_ptr);
#define svcallback(a, b) SVCallback(a, sizeof(a) - 1, b)

void SVPUTBuf(const char *keyname, int keylen, const StrBuf *Buf, int ref);
#define SVPutBuf(a, b, c); SVPUTBuf(a, sizeof(a) - 1, b, c)

void DoTemplate(const char *templatename, long len, StrBuf *Target, void *Context, int ContextType);
#define do_template(a, b) DoTemplate(a, sizeof(a) -1, NULL, b, 0);
void url_do_template(void);

int CompareSubstToToken(TemplateParam *ParamToCompare, TemplateParam *ParamToLookup);
int CompareSubstToStrBuf(StrBuf *Compare, TemplateParam *ParamToLookup);

void StrBufAppendTemplate(StrBuf *Target, 
			  int nArgs, 
			  WCTemplateToken *Tokens,
			  void *Context, int ContextType,
			  const StrBuf *Source, int FormatTypeIndex);
CompareFunc RetrieveSort(long ContextType, const char *OtherPrefix, 
			 const char *Default, long ldefault, long DefaultDirection);
void RegisterSortFunc(const char *name, long len, 
		      const char *prepend, long preplen,
		      CompareFunc Forward, 
		      CompareFunc Reverse, 
		      CompareFunc GroupChange, 
		      long ContextType);

void dbg_print_longvector(long *LongVector);
