
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
#define TYPE_SUBTEMPLATE 7
#define TYPE_INTDEFINE 8
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

#define CTX TP->Context
#define CCTX TP->ControlContext

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
#define CTX_ITERATE 16
#define CTX_ICAL 17
#define CTX_DAVNS 18

#define CTX_UNKNOWN 19


/**
 * ContextFilter resembles our RTTI information. With this structure
 * we can make shure a tmplput function can live with the environment
 * we call it in.
 * if not, we will log/print an error and refuse to call it.
 */
typedef struct _contexts {
	int ContextType;                /* do we require a User Context ? */
	int ControlContextType;         /* are we inside of a control structure? */
	int nMinArgs;                   /* How many arguments do we need at least? */
	int nMaxArgs;                   /* up to how many arguments can we handle? */
} ContextFilter;


/* Forward declarations... */
typedef struct WCTemplateToken WCTemplateToken;
typedef struct WCTemplputParams WCTemplputParams;

/* this is the signature of a tmplput function */
typedef void (*WCHandlerFunc)(StrBuf *Target, WCTemplputParams *TP);

/* if you want to pre-evaluate parts of your token, or do additional syntax, use this. */ 
typedef int (*WCPreevalFunc)(WCTemplateToken *Token);

/* make a template token a lookup key: */
#define TKEY(a) TP->Tokens->Params[a]->Start, TP->Tokens->Params[a]->len

/**
 * this is the signature of a conditional function 
 * Note: Target is just passed in for error messages; don't write onto it in regular cases.
 */
typedef int (*WCConditionalFunc)(StrBuf *Target, WCTemplputParams *TP);


typedef struct _TemplateParam {
        /* are we a string or a number? */
	int Type;
	/* string data: */
	const char *Start;
	long len;
	/* if we're a number: */
	long lvalue;
} TemplateParam;


/**
 * Representation of a token; everything thats inbetween <? and >
 */ 
struct WCTemplateToken {
        /* Reference to the filename we're in to print error messages; not to be freed */
	const StrBuf *FileName; 
	/* Raw copy of our original token; for error printing */
	StrBuf *FlatToken;
	/* Which line did the template parser pick us up in? For error printing */
	long Line;

	/* our position in the template cache buffer */
	const char *pTokenStart;
	/* our token length */
	size_t TokenStart;
	size_t TokenEnd;
	/* point after us */
	const char *pTokenEnd;
	/* just our token name: */
	const char *pName;
	size_t NameEnd;

	/* stuff the pre-evaluater finds out: */
	int Flags;
	/* pointer to our runntime evaluator; so we can cache this and save hash-lookups */
	void *PreEval;
	void *Preeval2;

	/* if we have parameters here we go: */
	/* do we have parameters or not? */
	int HaveParameters;
	/* How many of them? */
	int nParameters;
	/* the parameters */
	TemplateParam *Params[MAXPARAM];
};



struct WCTemplputParams {
	ContextFilter Filter;
	void *Context;
	void *ControlContext;
	int nArgs;
	WCTemplateToken *Tokens;
};



typedef struct _ConditionalStruct {
	ContextFilter Filter;
	const char *PlainName;
	WCConditionalFunc CondF;
} ConditionalStruct;


typedef void (*SubTemplFunc)(StrBuf *TemplBuffer, WCTemplputParams *TP);
typedef HashList *(*RetrieveHashlistFunc)(StrBuf *Target, WCTemplputParams *TP);
typedef void (*HashDestructorFunc) (HashList **KillMe);


extern WCTemplputParams NoCtx;

#define HAVE_PARAM(a) (TP->Tokens->nParameters > a)


#define ERR_NAME 0
#define ERR_PARM1 1
#define ERR_PARM2 2
/**
 * @Brief log an error while evaluating a token; print it to the actual template 
 * @param Target your Target Buffer to print the error message next to the log
 * @param Type What sort of thing are we talking about? Tokens? Conditionals?
 * @param TP grab our set of default information here
 * @param Format for the custom error message
 */ 
void LogTemplateError (StrBuf *Target, 
		       const char *Type, 
		       int ErrorPos, 
		       WCTemplputParams *TP, 
		       const char *Format, ...)__attribute__((__format__(__printf__,5,6)));


/**
 * @Brief log an error while in global context; print it to Wildfire / Target
 * @param Target your Target Buffer to print the error message next to the log
 * @param Type What sort of thing are we talking about? Tokens? Conditionals?
 * @param Format for the custom error message
 */ 
void LogError (StrBuf *Target, const char *Type, const char *Format, ...);

/**
 * @Brief get the actual value of a token parameter
 * in your tmplputs or conditionals use this function to access parameters that can also be 
 * retrieved from dynamic facilities:
 *  _ -> Gettext; retrieve this token from the i18n facilities
 *  : -> lookup a setting of that name
 *  B -> bstr; an URL-Parameter
 *  = -> subtemplate; parse a template by this name, and treat its content as this tokens value 
 * 
 * @param N which token do you want to lookup?
 * @param Value reference to the string of the token; don't free me.
 * @param len the length of Value
 */
void GetTemplateTokenString(StrBuf *Target, 
			    WCTemplputParams *TP,
			    int N,
			    const char **Value, 
			    long *len);



/**
 * @Brief get the actual integer value of a token parameter
 * in your tmplputs or conditionals use this function to access parameters that can also be 
 * retrieved from dynamic facilities:
 *  _ -> Gettext; retrieve this token from the i18n facilities
 *  : -> lookup a setting of that name
 *  B -> bstr; an URL-Parameter
 *  = -> subtemplate; parse a template by this name, and treat its content as this tokens value 
 * 
 * @param N which token do you want to lookup?
 * @param dflt default value to be retrieved if not found in preferences
 * \returns the long value
 */
long GetTemplateTokenNumber(StrBuf *Target, 
			    WCTemplputParams *TP, 
			    int N, long dflt);

/**
 * @Brief put a token value into the template
 * use this function to append your strings into a Template. 
 * it can escape your string according to the token at FormattypeIndex:
 *  H: de-QP and utf8-ify
 *  X: escapize for HTML
 *  J: JSON Escapize
 * @param Target the destination buffer
 * @param TP the template token information
 * @param Source string to append
 * @param FormatTypeIndex which parameter contains the escaping functionality?
 *        if this token doesn't have as much parameters, plain append is done.
 */
void StrBufAppendTemplate(StrBuf *Target, 
			  WCTemplputParams *TP,
			  const StrBuf *Source, 
			  int FormatTypeIndex);


#define RegisterNamespace(a, b, c, d, e, f) RegisterNS(a, sizeof(a)-1, b, c, d, e, f)
/**
 * @Brief register a template token handler
 * call this function in your InitModule_MODULENAME which will be called at the server start
 * @param nMinArgs how much parameters does your token require at least?
 * @param nMaxArgs how many parameters does your token accept?
 * @param HandlerFunc your callback when the template is rendered and your token is there 
 * @param PreEvalFunc is called when the template is parsed; you can do additional 
 *        syntax checks here or pre-evaluate stuff for better performance
 * @param ContextRequired if your token requires a specific context, else say CTX_NONE here.
 */
void RegisterNS(const char *NSName, long len, 
		int nMinArgs, 
		int nMaxArgs, 
		WCHandlerFunc HandlerFunc,
		WCPreevalFunc PreEvalFunc,
		int ContextRequired);

/**
 * @Brief register a conditional token <pair> handler
 * call this function in your InitModule_MODULENAME which will be called at the server start
 * conditionals can be ? or ! with a pair or % similar to an implicit if
 * @param Name whats the name of your conditional? should start with COND:
 * @param len the token length so we don't have to measure it.
 * @param nParams how many parameters does your conditional need on top of the default conditional parameters
 * @param CondF your Callback to be called when the template is evaluated at runtime; return 0 or 1 to us please.
 * @param ContextRequired if your token requires a specific context, else say CTX_NONE here.
 */
void RegisterConditional(const char *Name, long len, 
			 int nParams,
			 WCConditionalFunc CondF, 
			 int ContextRequired);

/**
 * @brief register a string that will represent a long value
 * this will allow to resolve <?...(#"Name")> to Value; that way 
 * plain strings can be used an lexed in templates without having the 
 * lookup overhead at runtime.
 * @param Name The name of the define
 * @param len length of Name
 * @param Value the value to associate with Name
 */
void RegisterTokenParamDefine(const char *Name, long len, 
			      long Value);

#define IT_NOFLAG 0
#define IT_FLAG_DETECT_GROUPCHANGE (1<<0)
#define RegisterIterator(a, b, c, d, e, f, g, h, i) RegisterITERATOR(a, sizeof(a)-1, b, c, d, e, f, g, h, i)
void RegisterITERATOR(const char *Name, long len, /* Our identifier */
		      int AdditionalParams,       /* doe we use more parameters? */
		      HashList *StaticList,       /* pointer to webcit lifetime hashlists */
		      RetrieveHashlistFunc GetHash, /* else retrieve the hashlist by calling this function */
		      SubTemplFunc DoSubTempl,       /* call this function on each iteration for svput & friends */
		      HashDestructorFunc Destructor, /* use this function to shut down the hash; NULL if its a reference */
		      int ContextType,               /* which context do we provide to the subtemplate? */
		      int XPectContextType,          /* which context do we expct to be called in? */
		      int Flags);






CompareFunc RetrieveSort(WCTemplputParams *TP, 
			 const char *OtherPrefix, long OtherPrefixLen,  
			 const char *Default, long ldefault, 
			 long DefaultDirection);
void RegisterSortFunc(const char *name, long len, 
		      const char *prepend, long preplen,
		      CompareFunc Forward, 
		      CompareFunc Reverse, 
		      CompareFunc GroupChange, 
		      long ContextType);




void dbg_print_longvector(long *LongVector);





#define do_template(a, b) DoTemplate(a, sizeof(a) -1, NULL, &NoCtx);
const StrBuf *DoTemplate(const char *templatename, long len, StrBuf *Target, WCTemplputParams *TP);
void url_do_template(void);





int CompareSubstToToken(TemplateParam *ParamToCompare, TemplateParam *ParamToLookup);
int CompareSubstToStrBuf(StrBuf *Compare, TemplateParam *ParamToLookup);

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