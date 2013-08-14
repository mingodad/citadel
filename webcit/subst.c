#include "sysdep.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

#define SHOW_ME_VAPPEND_PRINTF

#include "webcit.h"
#include "webserver.h"

extern char *static_dirs[PATH_MAX];  /* Disk representation */

HashList *TemplateCache;
HashList *LocalTemplateCache;

HashList *GlobalNS;
HashList *Iterators;
HashList *Conditionals;
HashList *SortHash;
HashList *Defines;

int DumpTemplateI18NStrings = 0;
int LoadTemplates = 0;
int dbg_backtrace_template_errors = 0;
WCTemplputParams NoCtx;
StrBuf *I18nDump = NULL;

const char EmptyStr[]="";

#define SV_GETTEXT 1
#define SV_CONDITIONAL 2
#define SV_NEG_CONDITIONAL 3
#define SV_CUST_STR_CONDITIONAL 4
#define SV_SUBTEMPL 5
#define SV_PREEVALUATED 6



/*
 * Dynamic content for variable substitution in templates
 */
typedef struct _wcsubst {
	ContextFilter Filter;
	int wcs_type;				/* which type of Substitution are we */
	char wcs_key[32];			/* copy of our hashkey for debugging */
	StrBuf *wcs_value;			/* if we're a string, keep it here */
	long lvalue;				/* type long? keep data here */
	WCHandlerFunc wcs_function;		/* funcion hook ???*/
} wcsubst;


typedef struct _WCTemplate {
	StrBuf *Data;
	StrBuf *FileName;
	int nTokensUsed;
	int TokenSpace;
	StrBuf *MimeType;
	WCTemplateToken **Tokens;
} WCTemplate;

typedef struct _HashHandler {
	ContextFilter Filter;
	WCPreevalFunc PreEvalFunc;
	WCHandlerFunc HandlerFunc;
}HashHandler;

typedef enum _estate {
	eNext,
	eSkipTilEnd
} TemplState;

void *load_template(StrBuf *Target, WCTemplate *NewTemplate);
int EvaluateConditional(StrBuf *Target, int Neg, int state, WCTemplputParams **TPP);



typedef struct _SortStruct {
	StrBuf *Name;
	StrBuf *PrefPrepend;
	CompareFunc Forward;
	CompareFunc Reverse;
	CompareFunc GroupChange;

	CtxType ContextType;
}SortStruct;

HashList *CtxList = NULL;

static CtxType CtxCounter = CTX_NONE;

CtxType CTX_STRBUF = CTX_NONE;
CtxType CTX_STRBUFARR = CTX_NONE;
CtxType CTX_LONGVECTOR = CTX_NONE;

CtxType CTX_ITERATE = CTX_NONE;
CtxType CTX_TAB = CTX_NONE;

void HFreeContextType(void *pCtx)
{
	CtxTypeStruct *FreeStruct = (CtxTypeStruct *) pCtx;
	FreeStrBuf(&FreeStruct->Name);
	free(FreeStruct);
}
void PutContextType(const char *name, long len, CtxType TheCtx)
{
	CtxTypeStruct *NewStruct;

	NewStruct = (CtxTypeStruct*) malloc(sizeof(CtxTypeStruct));
	NewStruct->Name = NewStrBufPlain(name, len);
	NewStruct->Type = TheCtx;

	Put(CtxList, IKEY(NewStruct->Type), NewStruct, HFreeContextType);
}
void RegisterContextType(const char *name, long len, CtxType *TheCtx)
{
	if (*TheCtx != CTX_NONE)
		return;

	*TheCtx = ++CtxCounter;
	PutContextType(name, len, *TheCtx);
}

CtxTypeStruct *GetContextType(CtxType Type)
{
	void *pv = NULL;
	GetHash(CtxList, IKEY(Type), &pv);
	return pv;
}

const char *UnknownContext = "CTX_UNKNOWN";

const char *ContextName(CtxType ContextType)
{
	CtxTypeStruct *pCtx;

	pCtx = GetContextType(ContextType);

	if (pCtx != NULL) 
		return ChrPtr(pCtx->Name);
	else
		return UnknownContext;
}

void StackDynamicContext(WCTemplputParams *Super, 
			 WCTemplputParams *Sub, 
			 void *Context,
			 CtxType ContextType,
			 int nArgs,
			 WCTemplateToken *Tokens, 
			 WCConditionalFunc ExitCtx, 
			 long ExitCTXID)
{
	memset(Sub, 0, sizeof(WCTemplputParams));

	if (Super != NULL) {
		Sub->Sub = Super->Sub;
		Super->Sub = Sub;
	}
	if (Sub->Sub != NULL)
		Sub->Sub->Super = Sub;
	Sub->Super = Super;
	
	Sub->Context = Context;
	Sub->Filter.ContextType = ContextType;
	Sub->nArgs = nArgs;
	Sub->Tokens = Tokens;
	Sub->ExitCtx = ExitCtx;
	Sub->ExitCTXID = ExitCTXID;
}

void UnStackContext(WCTemplputParams *Sub)
{
	if (Sub->Super != NULL)
	{
		Sub->Super->Sub = Sub->Sub;
	}
	if (Sub->Sub != NULL)
	{
		Sub->Sub->Super = Sub->Super;
	}
}
void UnStackDynamicContext(StrBuf *Target, WCTemplputParams **TPP)
{
	WCTemplputParams *TP = *TPP;
	WCTemplputParams *Super = TP->Super;
	TP->ExitCtx(Target, TP);
	*TPP = Super;
}

void *GetContextPayload(WCTemplputParams *TP, CtxType ContextType)
{
	WCTemplputParams *whichTP = TP;

	if (ContextType == CTX_NONE)
		return TP->Context;

	while ((whichTP != NULL) && (whichTP->Filter.ContextType != ContextType))
		whichTP = whichTP->Super;

	return whichTP->Context;	
}

void DestroySortStruct(void *vSort)
{
	SortStruct *Sort = (SortStruct*) vSort;
	FreeStrBuf(&Sort->Name);
	FreeStrBuf(&Sort->PrefPrepend);
	free (Sort);
}


void LogTemplateError (StrBuf *Target, const char *Type, int ErrorPos, WCTemplputParams *TP, const char *Format, ...)
{
	wcsession *WCC;
	StrBuf *Error;
	StrBuf *Info;
        va_list arg_ptr;
	const char *Err = NULL;

	Info = NewStrBuf();
	Error = NewStrBuf();

        va_start(arg_ptr, Format);
	StrBufVAppendPrintf(Error, Format, arg_ptr);
	va_end(arg_ptr);

	switch (ErrorPos) {
	case ERR_NAME: /* the main token name... */ 
		Err = (TP->Tokens!= NULL)? TP->Tokens->pName:"";
		break;
	default:
		Err = ((TP->Tokens!= NULL) && 
		       (TP->Tokens->nParameters > ErrorPos - 1))? 
			TP->Tokens->Params[ErrorPos - 1]->Start : "";
		break;
	}
	if (TP->Tokens != NULL) 
	{
		syslog(LOG_WARNING, "%s [%s]  (in '%s' line %ld); %s; [%s]\n", 
		       Type, 
		       Err, 
		       ChrPtr(TP->Tokens->FileName),
		       TP->Tokens->Line, 
		       ChrPtr(Error), 
		       ChrPtr(TP->Tokens->FlatToken));
	}
	else 
	{
		syslog(LOG_WARNING, "%s: %s;\n", 
		       Type, 
		       ChrPtr(Error));
	}
	WCC = WC;
	if (WCC == NULL) {
		FreeStrBuf(&Info);
		FreeStrBuf(&Error);
		return; 
	}

	if (WCC->WFBuf == NULL) WCC->WFBuf = NewStrBuf();
	if (TP->Tokens != NULL) 
	{
		/* deprecated: 
		StrBufAppendPrintf(                                                          
			Target,                                                              
			"<pre>\n%s [%s] (in '%s' line %ld); %s\n[%s]\n</pre>\n",
			Type, 
			Err, 
			ChrPtr(TP->Tokens->FileName),
			TP->Tokens->Line,
			ChrPtr(Error),
			ChrPtr(TP->Tokens->FlatToken));
		*/
		StrBufPrintf(Info, "%s [%s]  %s; [%s]", 
			     Type, 
			     Err, 
			     ChrPtr(Error), 
			     ChrPtr(TP->Tokens->FlatToken));


		SerializeJson(WCC->WFBuf, WildFireException(SKEY(TP->Tokens->FileName),
							TP->Tokens->Line,
							Info,
							1), 1);
/*
		SerializeJson(Header, WildFireMessage(SKEY(TP->Tokens->FileName),
						      TP->Tokens->Line,
						      Error,
						      eERROR), 1);
*/
		
	}
	else
	{
		/* deprecated.
		StrBufAppendPrintf(                                                          
			Target,                                                              
			"<pre>\n%s: %s\n</pre>\n",
			Type, 
			ChrPtr(Error));
		*/
		StrBufPrintf(Info, "%s [%s]  %s; [%s]", 
			     Type, 
			     Err, 
			     ChrPtr(Error), 
			     ChrPtr(TP->Tokens->FlatToken));
		SerializeJson(WCC->WFBuf, WildFireException(HKEY(__FILE__), __LINE__, Info, 1), 1);
	}
	FreeStrBuf(&Info);
	FreeStrBuf(&Error);
/*
	if (dbg_backtrace_template_errors)
		wc_backtrace(LOG_DEBUG); 
*/
}

void LogError (StrBuf *Target, const char *Type, const char *Format, ...)
{
	wcsession *WCC;
	StrBuf *Error;
	StrBuf *Info;
        va_list arg_ptr;

	Info = NewStrBuf();
	Error = NewStrBuf();

        va_start(arg_ptr, Format);
	StrBufVAppendPrintf(Error, Format, arg_ptr);
	va_end(arg_ptr);

	syslog(LOG_WARNING, "%s", ChrPtr(Error));

	WCC = WC;
	if (WCC->WFBuf == NULL) WCC->WFBuf = NewStrBuf();

	SerializeJson(WCC->WFBuf, WildFireException(Type, strlen(Type),
						    0,
						    Info,
						    1), 1);

	FreeStrBuf(&Info);
	FreeStrBuf(&Error);
/*
	if (dbg_backtrace_template_errors)
		wc_backtrace(LOG_DEBUG); 
*/
}


void RegisterNS(const char *NSName, 
		long len, 
		int nMinArgs, 
		int nMaxArgs, 
		WCHandlerFunc HandlerFunc, 
		WCPreevalFunc PreevalFunc,
		CtxType ContextRequired)
{
	HashHandler *NewHandler;
	
	NewHandler = (HashHandler*) malloc(sizeof(HashHandler));
	memset(NewHandler, 0, sizeof(HashHandler));
	NewHandler->Filter.nMinArgs = nMinArgs;
	NewHandler->Filter.nMaxArgs = nMaxArgs;
	NewHandler->Filter.ContextType = ContextRequired;

	NewHandler->PreEvalFunc = PreevalFunc;
	NewHandler->HandlerFunc = HandlerFunc;	
	Put(GlobalNS, NSName, len, NewHandler, NULL);
}



int CheckContext(StrBuf *Target, ContextFilter *Need, WCTemplputParams *TP, const char *ErrType)
{
	WCTemplputParams *TPP = TP;
	
	if ((Need != NULL) &&
	    (Need->ContextType != CTX_NONE) && 
	    (Need->ContextType != TPP->Filter.ContextType)) {

		while ((TPP != NULL) && 
		       (Need->ContextType != TPP->Filter.ContextType))
		{
			TPP = TPP->Super;
		}

		if (TPP != NULL)
			return 1;

                LogTemplateError(
                        Target, ErrType, ERR_NAME, TP,
			"  WARNING: requires Context: [%s], have [%s]!", 
			ContextName(Need->ContextType), 
			ContextName(TP->Filter.ContextType));
		return 0;
	}
/*			
	if (TP->Tokens->nParameters < Need->nMinArgs) {
		LogTemplateError(Target, ErrType, ERR_NAME, TP,
				 "needs at least %ld params, have %ld", 
				 Need->nMinArgs, 
				 TP->Tokens->nParameters);
		return 0;

	}
	else if (TP->Tokens->nParameters > Need->nMaxArgs) {
		LogTemplateError(Target, ErrType, ERR_NAME, TP,
				 "just needs %ld params, you gave %ld",
				 Need->nMaxArgs,
				 TP->Tokens->nParameters); 
		return 0;

	}
*/
	return 1;
}

void FreeToken(WCTemplateToken **Token)
{
	int i; 
	FreeStrBuf(&(*Token)->FlatToken);
	if ((*Token)->HaveParameters) 
		for (i = 0; i < (*Token)->nParameters; i++)
			free((*Token)->Params[i]);
	free(*Token);
	*Token = NULL;
}



void FreeWCTemplate(void *vFreeMe)
{
	int i;
	WCTemplate *FreeMe = (WCTemplate*)vFreeMe;

	if (FreeMe->TokenSpace > 0) {
		for (i = 0; i < FreeMe->nTokensUsed; i ++) {
			FreeToken(&FreeMe->Tokens[i]);
		}
		free(FreeMe->Tokens);
	}
	FreeStrBuf(&FreeMe->FileName);
	FreeStrBuf(&FreeMe->Data);
	FreeStrBuf(&FreeMe->MimeType);
	free(FreeMe);
}

int HaveTemplateTokenString(StrBuf *Target, 
			    WCTemplputParams *TP,
			    int N,
			    const char **Value, 
			    long *len)
{
	if (N >= TP->Tokens->nParameters) {
		return 0;
	}

	switch (TP->Tokens->Params[N]->Type) {
	case TYPE_INTDEFINE:
	case TYPE_STR:
	case TYPE_BSTR:
	case TYPE_PREFSTR:
	case TYPE_ROOMPREFSTR:
	case TYPE_GETTEXT:
	case TYPE_SUBTEMPLATE:
		return 1;
	case TYPE_LONG:
	case TYPE_PREFINT:
	default:
		return 0;
	}
}

void GetTemplateTokenString(StrBuf *Target, 
			    WCTemplputParams *TP,
			    int N,
			    const char **Value, 
			    long *len)
{
	StrBuf *Buf;

	if (N >= TP->Tokens->nParameters) {
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "invalid token %d. this shouldn't have come till here.\n", N);
		*Value = "";
		*len = 0;
		return;
	}

	switch (TP->Tokens->Params[N]->Type) {

	case TYPE_INTDEFINE:
	case TYPE_STR:
		*Value = TP->Tokens->Params[N]->Start;
		*len = TP->Tokens->Params[N]->len;
		break;
	case TYPE_BSTR:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type BSTR, empty lookup string not admitted.", N);
			*len = 0;
			*Value = EmptyStr;
			break;
		}
		Buf = (StrBuf*) SBstr(TKEY(N));
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		break;
	case TYPE_PREFSTR:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type PREFSTR, empty lookup string not admitted.", N);
			*len = 0;
			*Value = EmptyStr;
			break;
		}
		get_PREFERENCE(TKEY(N), &Buf);
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		break;
	case TYPE_ROOMPREFSTR:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type PREFSTR, empty lookup string not admitted.", N);
			*len = 0;
			*Value = EmptyStr;
			break;
		}
		Buf = get_ROOM_PREFS(TKEY(N));
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		break;
	case TYPE_LONG:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "Requesting parameter %d; of type LONG, want string.", N);
		break;
	case TYPE_PREFINT:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "Requesting parameter %d; of type PREFINT, want string.", N);
		break;
	case TYPE_GETTEXT:
		*Value = _(TP->Tokens->Params[N]->Start);
		*len = strlen(*Value);
		break;
	case TYPE_SUBTEMPLATE:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type SUBTEMPLATE, empty lookup string not admitted.", N);
			*len = 0;
			*Value = EmptyStr;
			break;
		}

		Buf = NewStrBuf();
		DoTemplate(TKEY(N), Buf, TP);

		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		/* we can't free it here, so we put it into the subst so its discarded later on. */
		PutRequestLocalMem(Buf, HFreeStrBuf);
		break;

	default:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "unknown param type %d; [%d]", N, TP->Tokens->Params[N]->Type);
		break;
	}
}

long GetTemplateTokenNumber(StrBuf *Target, WCTemplputParams *TP, int N, long dflt)
{
	long Ret;
	if (N >= TP->Tokens->nParameters) {
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "invalid token %d. this shouldn't have come till here.\n", N);
		wc_backtrace(LOG_DEBUG); 
		return 0;
	}

	switch (TP->Tokens->Params[N]->Type) {

	case TYPE_STR:
		return atol(TP->Tokens->Params[N]->Start);
		break;
	case TYPE_BSTR:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type BSTR, empty lookup string not admitted.", N);
			return 0;
		}
		return  LBstr(TKEY(N));
		break;
	case TYPE_PREFSTR:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "requesting a prefstring in param %d want a number", N);
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type PREFSTR, empty lookup string not admitted.", N);
			return 0;
		}
		if (get_PREF_LONG(TKEY(N), &Ret, dflt))
			return Ret;
		return 0;
	case TYPE_ROOMPREFSTR:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "requesting a prefstring in param %d want a number", N);
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type PREFSTR, empty lookup string not admitted.", N);
			return 0;
		}
		if (get_ROOM_PREFS_LONG(TKEY(N), &Ret, dflt))
			return Ret;
		return 0;
	case TYPE_INTDEFINE:
	case TYPE_LONG:
		return TP->Tokens->Params[N]->lvalue;
	case TYPE_PREFINT:
		if (TP->Tokens->Params[N]->len == 0) {
			LogTemplateError(Target, 
					 "TokenParameter", N, TP, 
					 "Requesting parameter %d; of type PREFINT, empty lookup string not admitted.", N);
			return 0;
		}
		if (get_PREF_LONG(TKEY(N), &Ret, dflt))
			return Ret;
		return 0;		
	case TYPE_GETTEXT:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "requesting a I18N string in param %d; want a number", N);
		return 0;
	case TYPE_SUBTEMPLATE:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "requesting a subtemplate in param %d; not supported for numbers", N);
		return 0;
	default:
		LogTemplateError(Target, 
				 "TokenParameter", N, TP, 
				 "unknown param type %d; [%d]", N, TP->Tokens->Params[N]->Type);
		return 0;
	}
}


/*
 * puts string into the template and computes which escape methon we should use
 * Source = the string we should put into the template
 * FormatTypeIndex = where should we look for escape types if?
 */
void StrBufAppendTemplate(StrBuf *Target, 
			  WCTemplputParams *TP,
			  const StrBuf *Source, int FormatTypeIndex)
{
	const char *pFmt = NULL;
	char EscapeAs = ' ';

	if ((FormatTypeIndex < TP->Tokens->nParameters) &&
	    (TP->Tokens->Params[FormatTypeIndex]->Type == TYPE_STR) &&
	    (TP->Tokens->Params[FormatTypeIndex]->len >= 1)) {
		pFmt = TP->Tokens->Params[FormatTypeIndex]->Start;
		EscapeAs = *pFmt;
	}

	switch(EscapeAs)
	{
	case 'H':
		StrEscAppend(Target, Source, NULL, 0, 2);
		break;
	case 'X':
		StrEscAppend(Target, Source, NULL, 0, 0);
		break;
	case 'J':
		StrECMAEscAppend(Target, Source, NULL);
	  break;
	case 'K':
		StrHtmlEcmaEscAppend(Target, Source, NULL, 0, 0);
	  break;
	case 'U':
		StrBufUrlescAppend(Target, Source, NULL);
		break;
	case 'F':
		if (pFmt != NULL) 	pFmt++;
		else			pFmt = "JUSTIFY";
		if (*pFmt == '\0')	pFmt = "JUSTIFY";
		FmOut(Target, pFmt, Source);
		break;
	default:
		StrBufAppendBuf(Target, Source, 0);
	}
}

/*
 * puts string into the template and computes which escape methon we should use
 * Source = the string we should put into the template
 * FormatTypeIndex = where should we look for escape types if?
 */
void StrBufAppendTemplateStr(StrBuf *Target, 
			     WCTemplputParams *TP,
			     const char *Source, int FormatTypeIndex)
{
	const char *pFmt = NULL;
	char EscapeAs = ' ';

	if ((FormatTypeIndex < TP->Tokens->nParameters) &&
	    (TP->Tokens->Params[FormatTypeIndex]->Type == TYPE_STR) &&
	    (TP->Tokens->Params[FormatTypeIndex]->len >= 1)) {
		pFmt = TP->Tokens->Params[FormatTypeIndex]->Start;
		EscapeAs = *pFmt;
	}

	switch(EscapeAs)
	{
	case 'H':
		StrEscAppend(Target, NULL, Source, 0, 2);
		break;
	case 'X':
		StrEscAppend(Target, NULL, Source, 0, 0);
		break;
	case 'J':
		StrECMAEscAppend(Target, NULL, Source);
	  break;
	case 'K':
		StrHtmlEcmaEscAppend(Target, NULL, Source, 0, 0);
	  break;
	case 'U':
		StrBufUrlescAppend(Target, NULL, Source);
		break;
/*
	case 'F':
		if (pFmt != NULL) 	pFmt++;
		else			pFmt = "JUSTIFY";
		if (*pFmt == '\0')	pFmt = "JUSTIFY";
		FmOut(Target, pFmt, Source);
		break;
*/
	default:
		StrBufAppendBufPlain(Target, Source, -1, 0);
	}
}


void PutNewToken(WCTemplate *Template, WCTemplateToken *NewToken)
{
	if (Template->nTokensUsed + 1 >= Template->TokenSpace) {
		if (Template->TokenSpace <= 0) {
			Template->Tokens = (WCTemplateToken**)malloc(
				sizeof(WCTemplateToken*) * 10);
			memset(Template->Tokens, 0, sizeof(WCTemplateToken*) * 10);
			Template->TokenSpace = 10;
		}
		else {
			WCTemplateToken **NewTokens;

			NewTokens= (WCTemplateToken**) malloc(
				sizeof(WCTemplateToken*) * Template->TokenSpace * 2);

			memset(NewTokens, 
			       0, sizeof(WCTemplateToken*) * Template->TokenSpace * 2);

			memcpy(NewTokens, 
			       Template->Tokens, 
			       sizeof(WCTemplateToken*) * Template->nTokensUsed);

			free(Template->Tokens);
			Template->TokenSpace *= 2;
			Template->Tokens = NewTokens;
		}
	}
	Template->Tokens[(Template->nTokensUsed)++] = NewToken;
}

int GetNextParameter(StrBuf *Buf, 
		     const char **pCh, 
		     const char *pe, 
		     WCTemplateToken *Tokens, 
		     WCTemplate *pTmpl, 
		     WCTemplputParams *TP, 
		     TemplateParam **pParm)
{
	const char *pch = *pCh;
	const char *pchs, *pche;
	TemplateParam *Parm;
	char quote = '\0';
	int ParamBrace = 0;

	*pParm = Parm = (TemplateParam *) malloc(sizeof(TemplateParam));
	memset(Parm, 0, sizeof(TemplateParam));
	Parm->Type = TYPE_STR;

	/* Skip leading whitespaces */
	while ((*pch == ' ' )||
	       (*pch == '\t')||
	       (*pch == '\r')||
	       (*pch == '\n')) pch ++;

	if (*pch == ':') {
		Parm->Type = TYPE_PREFSTR;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}
	else if (*pch == '.') {
		Parm->Type = TYPE_ROOMPREFSTR;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}
	else if (*pch == ';') {
		Parm->Type = TYPE_PREFINT;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}
	else if (*pch == '#') {
		Parm->Type = TYPE_INTDEFINE;
		pch ++;
	}
	else if (*pch == '_') {
		Parm->Type = TYPE_GETTEXT;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}
	else if (*pch == 'B') {
		Parm->Type = TYPE_BSTR;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}
	else if (*pch == '=') {
		Parm->Type = TYPE_SUBTEMPLATE;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
	}


	if (*pch == '"')
		quote = '"';
	else if (*pch == '\'')
		quote = '\'';
	if (quote != '\0') {
		pch ++;
		pchs = pch;
		while (pch <= pe &&
		       ((*pch != quote) ||
			( (pch > pchs) && (*(pch - 1) == '\\'))
			       )) {
			pch ++;
		}
		pche = pch;
		if (*pch != quote) {
			syslog(LOG_WARNING, "Error (in '%s' line %ld); "
				"evaluating template param [%s] in Token [%s]\n",
				ChrPtr(pTmpl->FileName),
				Tokens->Line,
				ChrPtr(Tokens->FlatToken),
				*pCh);
			pch ++;
			free(Parm);
			*pParm = NULL;
			return 0;
		}
		else {
			StrBufPeek(Buf, pch, -1, '\0');		
			if (LoadTemplates > 1) {			
				syslog(LOG_DEBUG,
					"DBG: got param [%s] "SIZE_T_FMT" "SIZE_T_FMT"\n", 
					pchs, pche - pchs, strlen(pchs)
				);
			}
			Parm->Start = pchs;
			Parm->len = pche - pchs;
			pch ++; /* move after trailing quote */
			if (ParamBrace && (*pch == ')')) {
				pch ++;
			}

		}
	}
	else {
		Parm->Type = TYPE_LONG;
		pchs = pch;
		while ((pch <= pe) &&
		       (isdigit(*pch) ||
			(*pch == '+') ||
			(*pch == '-')))
			pch ++;
		pch ++;
		if (pch - pchs > 1){
			StrBufPeek(Buf, pch, -1, '\0');
			Parm->lvalue = atol(pchs);
			Parm->Start = pchs;
			pch++;
		}
		else {
			Parm->lvalue = 0;
/* TODO whUT?
			syslog(LOG_DEBUG, "Error (in '%s' line %ld); "
				"evaluating long template param [%s] in Token [%s]\n",
				ChrPtr(pTmpl->FileName),
				Tokens->Line,
				ChrPtr(Tokens->FlatToken),
				*pCh);
				*/
			free(Parm);
			*pParm = NULL;
			return 0;
		}
	}
	while ((*pch == ' ' )||
	       (*pch == '\t')||
	       (*pch == '\r')||
	       (*pch == ',' )||
	       (*pch == '\n')) pch ++;

	switch (Parm->Type)
	{
	case TYPE_GETTEXT:
		if (DumpTemplateI18NStrings) {
			StrBufAppendPrintf(I18nDump, "_(\"%s\");\n", Parm->Start);
		}
		break;
	case TYPE_INTDEFINE: {
		void *vPVal;
		
		if (GetHash(Defines, Parm->Start, Parm->len, &vPVal) &&
		    (vPVal != NULL))
		{
			long *PVal;
			PVal = (long*) vPVal;
		
			Parm->lvalue = *PVal;
		}
		else if (strchr(Parm->Start, '|') != NULL)
		{
			const char *Pos;
			StrBuf *pToken;
			StrBuf *Match;

			Parm->MaskBy = eOR;
			pToken = NewStrBufPlain (Parm->Start, Parm->len);
			Match = NewStrBufPlain (NULL, Parm->len);
			Pos = ChrPtr(pToken);
			
			while ((Pos != NULL) && (Pos != StrBufNOTNULL))
			{
				StrBufExtract_NextToken(Match, pToken, &Pos, '|');
				StrBufTrim(Match);
				if (StrLength (Match) > 0)
				{
					if (GetHash(Defines, SKEY(Match), &vPVal) &&
					    (vPVal != NULL))
					{
						long *PVal;
						PVal = (long*) vPVal;
						
						Parm->lvalue |= *PVal;
					}
					else {
						LogTemplateError(NULL, "Define", 
								 Tokens->nParameters,
								 TP,
								 "%s isn't known!!",
								 ChrPtr(Match));

					}
				}
			}
			FreeStrBuf(&pToken);
			FreeStrBuf(&Match);
		}
		else if (strchr(Parm->Start, '&') != NULL)
		{
			const char *Pos;
			StrBuf *pToken;
			StrBuf *Match;

			Parm->MaskBy = eAND;
			pToken = NewStrBufPlain (Parm->Start, Parm->len);
			Match = NewStrBufPlain (NULL, Parm->len);
			Pos = ChrPtr(pToken);
			
			while ((Pos != NULL) && (Pos != StrBufNOTNULL))
			{
				StrBufExtract_NextToken(Match, pToken, &Pos, '&');
				StrBufTrim(Match);
				if (StrLength (Match) > 0)
				{
					if (GetHash(Defines, SKEY(Match), &vPVal) &&
					    (vPVal != NULL))
					{
						long *PVal;
						PVal = (long*) vPVal;
						
						Parm->lvalue |= *PVal;
					}
					else {
						LogTemplateError(NULL, "Define", 
								 Tokens->nParameters,
								 TP,
								 "%s isn't known!!",
								 ChrPtr(Match));

					}
				}
			}
			FreeStrBuf(&Match);
			FreeStrBuf(&pToken);
		}
		else {


			LogTemplateError(NULL, "Define", 
					 Tokens->nParameters,
					 TP,
					 "%s isn't known!!",
					 Parm->Start);
		}}
		break;
	case TYPE_SUBTEMPLATE:{
		void *vTmpl;
		/* well, we don't check the mobile stuff here... */
		if (!GetHash(LocalTemplateCache, Parm->Start, Parm->len, &vTmpl) &&
		    !GetHash(TemplateCache, Parm->Start, Parm->len, &vTmpl)) {
			LogTemplateError(NULL, 
					 "SubTemplate", 
					 Tokens->nParameters,
					 TP,
					 "referenced here doesn't exist");
		}}
		break;
	}
	*pCh = pch;
	return 1;
}

WCTemplateToken *NewTemplateSubstitute(StrBuf *Buf, 
				       const char *pStart, 
				       const char *pTokenStart, 
				       const char *pTokenEnd, 
				       long Line,
				       WCTemplate *pTmpl)
{
	void *vVar;
	const char *pch;
	WCTemplateToken *NewToken;
	WCTemplputParams TP;

	NewToken = (WCTemplateToken*)malloc(sizeof(WCTemplateToken));
	memset(NewToken, 0, sizeof(WCTemplateToken));
	TP.Tokens = NewToken;
	NewToken->FileName = pTmpl->FileName; /* to print meaningfull log messages... */
	NewToken->Flags = 0;
	NewToken->Line = Line + 1;
	NewToken->pTokenStart = pTokenStart;
	NewToken->TokenStart = pTokenStart - pStart;
	NewToken->TokenEnd =  (pTokenEnd - pStart) - NewToken->TokenStart;
	NewToken->pTokenEnd = pTokenEnd;
	NewToken->NameEnd = NewToken->TokenEnd - 2;
	NewToken->PreEval = NULL;
	NewToken->FlatToken = NewStrBufPlain(pTokenStart + 2, pTokenEnd - pTokenStart - 2);
	StrBufShrinkToFit(NewToken->FlatToken, 1);

	StrBufPeek(Buf, pTokenStart, + 1, '\0');
	StrBufPeek(Buf, pTokenEnd, -1, '\0');
	pch = NewToken->pName = pTokenStart + 2;

	NewToken->HaveParameters = 0;;
	NewToken->nParameters = 0;

	while (pch < pTokenEnd - 1) {
		if (*pch == '(') {
			StrBufPeek(Buf, pch, -1, '\0');
			NewToken->NameEnd = pch - NewToken->pName;
			pch ++;
			if (*(pTokenEnd - 1) != ')') {
				LogTemplateError(
					NULL, "Parseerror", ERR_NAME, &TP, 
					"Warning, Non welformed Token; missing right parenthesis");
			}
			while (pch < pTokenEnd - 1) {
				NewToken->nParameters++;
				if (GetNextParameter(Buf, 
						     &pch, 
						     pTokenEnd - 1, 
						     NewToken, 
						     pTmpl, 
						     &TP, 
						     &NewToken->Params[NewToken->nParameters - 1]))
				{
					NewToken->HaveParameters = 1;
					if (NewToken->nParameters >= MAXPARAM) {
						LogTemplateError(
							NULL, "Parseerror", ERR_NAME, &TP,
							"only [%d] Params allowed in Tokens",
							MAXPARAM);

						FreeToken(&NewToken);
						return NULL;
					}
				}
				else break;
			}
			if((NewToken->NameEnd == 1) &&
			   (NewToken->HaveParameters == 1))
			   
			{
				if (*(NewToken->pName) == '_')
					NewToken->Flags = SV_GETTEXT;
				else if (*(NewToken->pName) == '=')
					NewToken->Flags = SV_SUBTEMPL;
				else if (*(NewToken->pName) == '%')
					NewToken->Flags = SV_CUST_STR_CONDITIONAL;
				else if (*(NewToken->pName) == '?')
					NewToken->Flags = SV_CONDITIONAL;
				else if (*(NewToken->pName) == '!')
					NewToken->Flags = SV_NEG_CONDITIONAL;
			}
		}
		else pch ++;		
	}
	
	switch (NewToken->Flags) {
	case 0:
		/* If we're able to find out more about the token, do it now while its fresh. */
		pch = NewToken->pName;
		while (pch <  NewToken->pName + NewToken->NameEnd)
		{
			if (((*pch >= 'A') && (*pch <= 'Z')) || 
			    ((*pch >= '0') && (*pch <= '9')) ||
			    (*pch == ':') || 
			    (*pch == '-') ||
			    (*pch == '_')) 
				pch ++;
			else
			{
				LogTemplateError(
					NULL, "Token Name", ERR_NAME, &TP,
					"contains illegal char: '%c'", 
					*pch);
				pch++;
			}

		}
		if (GetHash(GlobalNS, NewToken->pName, NewToken->NameEnd, &vVar)) {
			HashHandler *Handler;
			Handler = (HashHandler*) vVar;
			if ((NewToken->nParameters < Handler->Filter.nMinArgs) || 
			    (NewToken->nParameters > Handler->Filter.nMaxArgs)) {
				LogTemplateError(
					NULL, "Token", ERR_NAME, &TP,
					"doesn't work with %d params", 
					NewToken->nParameters);

			}
			else {
				NewToken->PreEval = Handler;
				NewToken->Flags = SV_PREEVALUATED;		
				if (Handler->PreEvalFunc != NULL)
					Handler->PreEvalFunc(NewToken);
			}
		} else {
			LogTemplateError(
				NULL, "Token ", ERR_NAME, &TP,
				" isn't known to us.");
		}
		break;
	case SV_GETTEXT:
		if ((NewToken->nParameters < 1) || (NewToken->nParameters > 2)) {
			LogTemplateError(                               
				NULL, "Gettext", ERR_NAME, &TP,
				"requires 1 or 2 parameter, you gave %d params", 
				NewToken->nParameters);
			NewToken->Flags = 0;
			break;
		}
		if (DumpTemplateI18NStrings) {
			StrBufAppendPrintf(I18nDump, "_(\"%s\");\n", NewToken->Params[0]->Start);
		}
		break;
	case SV_SUBTEMPL:
		if (NewToken->nParameters != 1) {
			LogTemplateError(
				NULL, "Subtemplates", ERR_NAME, &TP,
				"require exactly 1 parameter, you gave %d params", 
				NewToken->nParameters);
			break;
		}
		else {
			void *vTmpl;
			/* well, we don't check the mobile stuff here... */
			if (!GetHash(LocalTemplateCache, 
				     NewToken->Params[0]->Start, 
				     NewToken->Params[0]->len, 
				     &vTmpl) &&
			    !GetHash(TemplateCache, 
				     NewToken->Params[0]->Start, 
				     NewToken->Params[0]->len, 
				     &vTmpl)) {
				LogTemplateError(
					NULL, "SubTemplate", ERR_PARM1, &TP,
					"doesn't exist");
			}
		}
		break;
	case SV_CUST_STR_CONDITIONAL:
	case SV_CONDITIONAL:
	case SV_NEG_CONDITIONAL:
		if (NewToken->nParameters <2) {
			LogTemplateError(
				NULL, "Conditional", ERR_PARM1, &TP,
				"require at least 2 parameters, you gave %d params", 
				NewToken->nParameters);
			NewToken->Flags = 0;
			break;
		}
		if (NewToken->Params[1]->lvalue == 0) {
			LogTemplateError(
				NULL, "Conditional", ERR_PARM1, &TP,
				"Conditional ID (Parameter 1) mustn't be 0!");
			NewToken->Flags = 0;
			break;
		}
		if (!GetHash(Conditionals, 
			     NewToken->Params[0]->Start, 
			     NewToken->Params[0]->len, 
			     &vVar) || 
		    (vVar == NULL)) {
			if ((NewToken->Params[0]->len == 1) &&
			    (NewToken->Params[0]->Start[0] == 'X'))
				break;
			LogTemplateError(
				NULL, "Conditional", ERR_PARM1, &TP,
				"Not found!");
/*
			NewToken->Error = NewStrBuf();
			StrBufAppendPrintf(
				NewToken->Error, 
				"<pre>\nConditional [%s] (in '%s' line %ld); Not found!\n[%s]\n</pre>\n", 
				NewToken->Params[0]->Start,
				ChrPtr(pTmpl->FileName),
				NewToken->Line,
				ChrPtr(NewToken->FlatToken));
*/
		}
		else {
			NewToken->PreEval = vVar;
		}
		break;
	}
	return NewToken;
}





/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void *prepare_template(StrBuf *filename, StrBuf *Key, HashList *PutThere)
{
	WCTemplate *NewTemplate;

	NewTemplate = (WCTemplate *) malloc(sizeof(WCTemplate));
	memset(NewTemplate, 0, sizeof(WCTemplate));
	NewTemplate->Data = NULL;
	NewTemplate->FileName = NewStrBufDup(filename);
	StrBufShrinkToFit(NewTemplate->FileName, 1);
	NewTemplate->nTokensUsed = 0;
	NewTemplate->TokenSpace = 0;
	NewTemplate->Tokens = NULL;
	NewTemplate->MimeType = NewStrBufPlain(GuessMimeByFilename (SKEY(NewTemplate->FileName)), -1);
	if (strstr(ChrPtr(NewTemplate->MimeType), "text") != NULL) {
		StrBufAppendBufPlain(NewTemplate->MimeType, HKEY("; charset=utf-8"), 0);
	}

	if (strstr(ChrPtr(NewTemplate->MimeType), "text") != NULL) {
		StrBufAppendBufPlain(NewTemplate->MimeType, HKEY("; charset=utf-8"), 0);
	}

	Put(PutThere, ChrPtr(Key), StrLength(Key), NewTemplate, FreeWCTemplate);
	return NewTemplate;
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void *duplicate_template(WCTemplate *OldTemplate)
{
	WCTemplate *NewTemplate;

	NewTemplate = (WCTemplate *) malloc(sizeof(WCTemplate));
	memset(NewTemplate, 0, sizeof(WCTemplate));
	NewTemplate->Data = NULL;
	NewTemplate->FileName = NewStrBufDup(OldTemplate->FileName);
	StrBufShrinkToFit(NewTemplate->FileName, 1);
	NewTemplate->nTokensUsed = 0;
	NewTemplate->TokenSpace = 0;
	NewTemplate->Tokens = NULL;
	NewTemplate->MimeType = NewStrBufDup(OldTemplate->MimeType);
	return NewTemplate;
}


void SanityCheckTemplate(StrBuf *Target, WCTemplate *CheckMe)
{
	int i = 0;
	int j;
	int FoundConditionalEnd;

	for (i = 0; i < CheckMe->nTokensUsed; i++)
	{
		switch(CheckMe->Tokens[i]->Flags)
		{
		case SV_CONDITIONAL:
		case SV_NEG_CONDITIONAL:
			FoundConditionalEnd = 0;
			if ((CheckMe->Tokens[i]->Params[0]->len == 1) && 
			    (CheckMe->Tokens[i]->Params[0]->Start[0] == 'X'))
				break;
			for (j = i + 1; j < CheckMe->nTokensUsed; j++)
			{
				if (((CheckMe->Tokens[j]->Flags == SV_CONDITIONAL) ||
				     (CheckMe->Tokens[j]->Flags == SV_NEG_CONDITIONAL)) && 
				    (CheckMe->Tokens[i]->Params[1]->lvalue == 
				     CheckMe->Tokens[j]->Params[1]->lvalue))
				{
					FoundConditionalEnd = 1;
					break;
				}

			}
			if (!FoundConditionalEnd)
			{
				WCTemplputParams TP;
				memset(&TP, 0, sizeof(WCTemplputParams));
				TP.Tokens = CheckMe->Tokens[i];
				LogTemplateError(
					Target, "Token", ERR_PARM1, &TP,
					"Conditional without Endconditional"
					);
			}
			break;
		default:
			break;
		}
	}
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void *load_template(StrBuf *Target, WCTemplate *NewTemplate)
{
	int fd;
	struct stat statbuf;
	const char *pS, *pE, *pch, *Err;
	long Line;

	fd = open(ChrPtr(NewTemplate->FileName), O_RDONLY);
	if (fd <= 0) {
		syslog(LOG_WARNING, "ERROR: could not open template '%s' - %s\n",
			ChrPtr(NewTemplate->FileName), strerror(errno));
		return NULL;
	}

	if (fstat(fd, &statbuf) == -1) {
		syslog(LOG_WARNING, "ERROR: could not stat template '%s' - %s\n",
			ChrPtr(NewTemplate->FileName), strerror(errno));
		return NULL;
	}

	NewTemplate->Data = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(NewTemplate->Data, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		syslog(LOG_WARNING, "ERROR: reading template '%s' - %s<br>\n",
			ChrPtr(NewTemplate->FileName), strerror(errno));
		return NULL;
	}
	close(fd);

	Line = 0;
	StrBufShrinkToFit(NewTemplate->Data, 1);
	StrBufShrinkToFit(NewTemplate->MimeType, 1);
	pS = pch = ChrPtr(NewTemplate->Data);
	pE = pS + StrLength(NewTemplate->Data);
	while (pch < pE) {
		const char *pts, *pte;
		char InQuotes = '\0';
		void *pv;

		/** Find one <? > */
		for (; pch < pE; pch ++) {
			if ((*pch=='<')&&(*(pch + 1)=='?') &&
			    !((pch == pS) && /* we must ommit a <?xml */
			      (*(pch + 2) == 'x') && 
			      (*(pch + 3) == 'm') && 
			      (*(pch + 4) == 'l')))			     
				break;
			if (*pch=='\n') Line ++;
		}
		if (pch >= pE)
			continue;
		pts = pch;

		/** Found one? parse it. */
		for (; pch <= pE - 1; pch ++) {
			if ((!InQuotes) &&
			    ((*pch == '\'') || (*pch == '"')))
			{
				InQuotes = *pch;
			}
			else if (InQuotes && (InQuotes == *pch))
			{
				InQuotes = '\0';
			}
			else if ((InQuotes) &&
				 (*pch == '\\') &&
				 (*(pch + 1) == InQuotes))
			{
				pch++;
			}
			else if ((!InQuotes) && 
				 (*pch == '>'))
			{
				break;
			}
		}
		if (pch + 1 > pE)
			continue;
		pte = pch;
		pv = NewTemplateSubstitute(NewTemplate->Data, pS, pts, pte, Line, NewTemplate);
		if (pv != NULL) {
			PutNewToken(NewTemplate, pv);
			pch ++;
		}
	}

	SanityCheckTemplate(NULL, NewTemplate);
	return NewTemplate;
}


const char* PrintTemplate(void *vSubst)
{
	WCTemplate *Tmpl = vSubst;

	return ChrPtr(Tmpl->FileName);

}

int LoadTemplateDir(const StrBuf *DirName, HashList *big, const StrBuf *BaseKey)
{
	int Toplevel;
	StrBuf *FileName;
	StrBuf *Key;
	StrBuf *SubKey;
	StrBuf *SubDirectory;
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	struct dirent *d;
	int d_type = 0;
	int d_namelen;
	int d_without_ext;
	
	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		return 0;
	}

	filedir = opendir (ChrPtr(DirName));
	if (filedir == NULL) {
		free(d);
		return 0;
	}

	Toplevel = StrLength(BaseKey) == 0;
	SubDirectory = NewStrBuf();
	SubKey = NewStrBuf();
	FileName = NewStrBufPlain(NULL, PATH_MAX);
	Key = NewStrBuf();
	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
		char *MinorPtr;

#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_type = DT_UNKNOWN;
#endif
		d_without_ext = d_namelen;

		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		if (d_type == DT_UNKNOWN) {
			struct stat s;
			char path[PATH_MAX];
			snprintf(path, PATH_MAX, "%s/%s", 
				 ChrPtr(DirName), filedir_entry->d_name);
			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}
		switch (d_type)
		{
		case DT_DIR:
			/* Skip directories we are not interested in... */
			if (strcmp(filedir_entry->d_name, ".svn") == 0)
				continue;

			FlushStrBuf(SubKey);
			if (!Toplevel) {
				/* If we're not toplevel, the upper dirs count as foo_bar_<local name>*/
				StrBufAppendBuf(SubKey, BaseKey, 0);
				StrBufAppendBufPlain(SubKey, HKEY("_"), 0);
			}
			StrBufAppendBufPlain(SubKey, filedir_entry->d_name, d_namelen, 0);

			FlushStrBuf(SubDirectory);
			StrBufAppendBuf(SubDirectory, DirName, 0);
			if (ChrPtr(SubDirectory)[StrLength(SubDirectory) - 1] != '/')
				StrBufAppendBufPlain(SubDirectory, HKEY("/"), 0);
			StrBufAppendBufPlain(SubDirectory, filedir_entry->d_name, d_namelen, 0);

			LoadTemplateDir(SubDirectory, big, SubKey);

			break;
		case DT_LNK: 
		case DT_REG:


			while ((d_without_ext > 0) && (filedir_entry->d_name[d_without_ext] != '.'))
				d_without_ext --;
			if ((d_without_ext == 0) || (d_namelen < 3))
				continue;
			if (((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~') ||
			    (strcmp(&filedir_entry->d_name[d_without_ext], ".orig") == 0) ||
			    (strcmp(&filedir_entry->d_name[d_without_ext], ".swp") == 0))
				continue; /* Ignore backup files... */
			StrBufPrintf(FileName, "%s/%s", ChrPtr(DirName),  filedir_entry->d_name);
			MinorPtr = strchr(filedir_entry->d_name, '.');
			if (MinorPtr != NULL)
				*MinorPtr = '\0';
			FlushStrBuf(Key);
			if (!Toplevel) {
				/* If we're not toplevel, the upper dirs count as foo_bar_<local name>*/
				StrBufAppendBuf(Key, BaseKey, 0);
				StrBufAppendBufPlain(Key, HKEY("_"), 0);
			}
			StrBufAppendBufPlain(Key, filedir_entry->d_name, MinorPtr - filedir_entry->d_name, 0);

			if (LoadTemplates >= 1)
				syslog(LOG_DEBUG, "%s %s\n", ChrPtr(FileName), ChrPtr(Key));
			prepare_template(FileName, Key, big);
		default:
			break;
		}
	}
	free(d);
	closedir(filedir);
	FreeStrBuf(&FileName);
	FreeStrBuf(&Key);
	FreeStrBuf(&SubDirectory);
	FreeStrBuf(&SubKey);
	return 1;
}

void InitTemplateCache(void)
{
	int i;
	StrBuf *Key;
	StrBuf *Dir;
	HashList *Templates[2];

	Dir = NewStrBuf();
	Key = NewStrBuf();

	/* Primary Template set... */
	StrBufPrintf(Dir, "%s/t", static_dirs[0]);
	LoadTemplateDir(Dir,
			TemplateCache, 
			Key);

	/* User local Template set */
	StrBufPrintf(Dir, "%s/t", static_dirs[1]);
	LoadTemplateDir(Dir,
			LocalTemplateCache, 
			Key);
	
	/* Debug Templates, just to be loaded while debugging. */
	
	StrBufPrintf(Dir, "%s/dbg", static_dirs[0]);
	LoadTemplateDir(Dir,
			TemplateCache, 
			Key);
	Templates[0] = TemplateCache;
	Templates[1] = LocalTemplateCache;


	if (LoadTemplates == 0) 
		for (i=0; i < 2; i++) {
			const char *Key;
			long KLen;
			HashPos *At;
			void *vTemplate;

			At = GetNewHashPos(Templates[i], 0);
			while (GetNextHashPos(Templates[i], 
					      At, 
					      &KLen,
					      &Key, 
					      &vTemplate) && 
			       (vTemplate != NULL))
			{
				load_template(NULL, (WCTemplate *)vTemplate);
			}
			DeleteHashPos(&At);
		}


	FreeStrBuf(&Dir);
	FreeStrBuf(&Key);
}



/*-----------------------------------------------------------------------------
 *                      Filling & processing Templates
 */
/**
 * \brief executes one token
 * \param Target buffer to append to
 * \param Token da to  process.
 * \param Template we're iterating
 * \param Context Contextpoointer to pass in
 * \param state are we in conditional state?
 * \param ContextType what type of information does context giv us?
 */
int EvaluateToken(StrBuf *Target, int state, WCTemplputParams **TPP)
{
	const char *AppendMe;
	long AppendMeLen;
	HashHandler *Handler;
	void *vVar;
	WCTemplputParams *TP = *TPP;
	
/* much output, since pName is not terminated...
	syslog(LOG_DEBUG,"Doing token: %s\n",Token->pName);
*/

	switch (TP->Tokens->Flags) {
	case SV_GETTEXT:
		TmplGettext(Target, TP);
		break;
	case SV_CONDITIONAL: /** Forward conditional evaluation */
		Handler = (HashHandler*) TP->Tokens->PreEval;
		if (!CheckContext(Target, &Handler->Filter, TP, "Conditional")) {
			return 0;
		}
		return EvaluateConditional(Target, 1, state, TPP);
		break;
	case SV_NEG_CONDITIONAL: /** Reverse conditional evaluation */
		Handler = (HashHandler*) TP->Tokens->PreEval;
		if (!CheckContext(Target, &Handler->Filter, TP, "Conditional")) {
			return 0;
		}
		return EvaluateConditional(Target, 0, state, TPP);
		break;
	case SV_CUST_STR_CONDITIONAL: /** Conditional put custom strings from params */
		Handler = (HashHandler*) TP->Tokens->PreEval;
		if (!CheckContext(Target, &Handler->Filter, TP, "Conditional")) {
			return 0;
		}
		if (TP->Tokens->nParameters >= 6) {
			if (EvaluateConditional(Target, 0, state, TPP)) {
				GetTemplateTokenString(Target, TP, 5, &AppendMe, &AppendMeLen);
				StrBufAppendBufPlain(Target, 
						     AppendMe, 
						     AppendMeLen,
						     0);
			}
			else{
				GetTemplateTokenString(Target, TP, 4, &AppendMe, &AppendMeLen);
				StrBufAppendBufPlain(Target, 
						     AppendMe, 
						     AppendMeLen,
						     0);
			}
			if (*TPP != TP)
			{
				UnStackDynamicContext(Target, TPP);
			}
		}
		else  {
			LogTemplateError(
				Target, "Conditional", ERR_NAME, TP,
				"needs at least 6 Params!"); 
		}
		break;
	case SV_SUBTEMPL:
		if (TP->Tokens->nParameters == 1)
			DoTemplate(TKEY(0), Target, TP);
		break;
	case SV_PREEVALUATED:
		Handler = (HashHandler*) TP->Tokens->PreEval;
		if (!CheckContext(Target, &Handler->Filter, TP, "Token")) {
			return 0;
		}
		Handler->HandlerFunc(Target, TP);
		break;		
	default:
		if (GetHash(GlobalNS, TP->Tokens->pName, TP->Tokens->NameEnd, &vVar)) {
			Handler = (HashHandler*) vVar;
			if (!CheckContext(Target, &Handler->Filter, TP, "Token")) {
				return 0;
			}
			else {
				Handler->HandlerFunc(Target, TP);
			}
		}
		else {
			LogTemplateError(
				Target, "Token UNKNOWN", ERR_NAME, TP,
				"You've specified a token that isn't known to webcit.!");
		}
	}
	return 0;
}



const StrBuf *ProcessTemplate(WCTemplate *Tmpl, StrBuf *Target, WCTemplputParams *CallingTP)
{
	WCTemplate *pTmpl = Tmpl;
	int done = 0;
	int i;
	TemplState state;
	const char *pData, *pS;
	long len;
	WCTemplputParams TP;
	WCTemplputParams *TPtr = &TP;

	memset(TPtr, 0, sizeof(WCTemplputParams));

	memcpy(&TP.Filter, &CallingTP->Filter, sizeof(ContextFilter));

	TP.Context = CallingTP->Context;
	TP.Sub = CallingTP->Sub;
	TP.Super = CallingTP->Super;

	if (LoadTemplates != 0) {			
		if (LoadTemplates > 1)
			syslog(LOG_DEBUG, "DBG: ----- loading:  [%s] ------ \n", 
				ChrPtr(Tmpl->FileName));
		pTmpl = duplicate_template(Tmpl);
		if(load_template(Target, pTmpl) == NULL) {
			StrBufAppendPrintf(
				Target, 
				"<pre>\nError loading Template [%s]\n See Logfile for details\n</pre>\n", 
				ChrPtr(Tmpl->FileName));
			FreeWCTemplate(pTmpl);
			return NULL;
		}

	}

	pS = pData = ChrPtr(pTmpl->Data);
	len = StrLength(pTmpl->Data);
	i = 0;
	state = eNext;
	while (!done) {
		if (i >= pTmpl->nTokensUsed) {
			StrBufAppendBufPlain(Target, 
					     pData, 
					     len - (pData - pS), 0);
			done = 1;
		}
		else {
			int TokenRc = 0;

			StrBufAppendBufPlain(
				Target, pData, 
				pTmpl->Tokens[i]->pTokenStart - pData, 0);
			TPtr->Tokens = pTmpl->Tokens[i];
			TPtr->nArgs = pTmpl->Tokens[i]->nParameters;

		        TokenRc = EvaluateToken(Target, TokenRc, &TPtr);
			if (TokenRc > 0)
			{
				state = eSkipTilEnd;
			}
			else if (TokenRc < 0)
			{
				if ((TPtr != &TP) &&
				    (TPtr->ExitCTXID == -TokenRc))
				{
					UnStackDynamicContext(Target, &TPtr);
				}
				TokenRc = 0;
			}

			while ((state != eNext) && (i+1 < pTmpl->nTokensUsed)) {
			/* condition told us to skip till its end condition */
				i++;
				TPtr->Tokens = pTmpl->Tokens[i];
				TPtr->nArgs = pTmpl->Tokens[i]->nParameters;
				if ((pTmpl->Tokens[i]->Flags == SV_CONDITIONAL) ||
				    (pTmpl->Tokens[i]->Flags == SV_NEG_CONDITIONAL))
				{
					int rc;
				        rc = EvaluateConditional(
						Target, 
						pTmpl->Tokens[i]->Flags, 
						TokenRc, 
						&TPtr);
					if (-rc == TokenRc)
					{
						TokenRc = 0;
						state = eNext;
						if ((TPtr != &TP) &&
						    (TPtr->ExitCTXID == - rc))
						{
							UnStackDynamicContext(Target, &TPtr);
						}
					}
				}
			}

			pData = pTmpl->Tokens[i++]->pTokenEnd + 1;
			if (i > pTmpl->nTokensUsed)
				done = 1;
		}
	}
	if (LoadTemplates != 0) {
		FreeWCTemplate(pTmpl);
	}
	return Tmpl->MimeType;

}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 * \returns the mimetype of the template its doing
 */
const StrBuf *DoTemplate(const char *templatename, long len, StrBuf *Target, WCTemplputParams *TP) 
{
	WCTemplputParams LocalTP;
	HashList *Static;
	HashList *StaticLocal;
	void *vTmpl;
	
	if (Target == NULL)
		Target = WC->WBuf;
	if (TP == NULL) {
		memset(&LocalTP, 0, sizeof(WCTemplputParams));
		TP = &LocalTP;
	}

	Static = TemplateCache;
	StaticLocal = LocalTemplateCache;

	if (len == 0)
	{
		syslog(LOG_WARNING, "Can't to load a template with empty name!\n");
		StrBufAppendPrintf(Target, "<pre>\nCan't to load a template with empty name!\n</pre>");
		return NULL;
	}

	if (!GetHash(StaticLocal, templatename, len, &vTmpl) &&
	    !GetHash(Static, templatename, len, &vTmpl)) {
		syslog(LOG_WARNING, "didn't find Template [%s] %ld %ld\n", templatename, len , (long)strlen(templatename));
		StrBufAppendPrintf(Target, "<pre>\ndidn't find Template [%s] %ld %ld\n</pre>", 
				   templatename, len, 
				   (long)strlen(templatename));
#if 0
		dbg_PrintHash(Static, PrintTemplate, NULL);
		PrintHash(Static, VarPrintTransition, PrintTemplate);
#endif
		return NULL;
	}
	if (vTmpl == NULL) 
		return NULL;
	return ProcessTemplate(vTmpl, Target, TP);

}


void tmplput_Comment(StrBuf *Target, WCTemplputParams *TP)
{
	if (LoadTemplates != 0)
	{
		StrBuf *Comment;
		const char *pch;
		long len;

		GetTemplateTokenString(Target, TP, 0, &pch, &len);
		Comment = NewStrBufPlain(pch, len);
		StrBufAppendBufPlain(Target, HKEY("<!--"), 0);
		StrBufAppendTemplate(Target, TP, Comment, 1);
		StrBufAppendBufPlain(Target, HKEY("-->"), 0);
		FreeStrBuf(&Comment);
	}
}

/*-----------------------------------------------------------------------------
 *                      Iterators
 */
typedef struct _HashIterator {
	HashList *StaticList;
	int AdditionalParams;
	CtxType ContextType;
	CtxType XPectContextType;
	int Flags;
	RetrieveHashlistFunc GetHash;
	HashDestructorFunc Destructor;
	SubTemplFunc DoSubTemplate;
} HashIterator;

void RegisterITERATOR(const char *Name, long len, 
		      int AdditionalParams, 
		      HashList *StaticList, 
		      RetrieveHashlistFunc GetHash, 
		      SubTemplFunc DoSubTempl,
		      HashDestructorFunc Destructor,
		      CtxType ContextType, 
		      CtxType XPectContextType, 
		      int Flags)
{
	HashIterator *It;

	It = (HashIterator*)malloc(sizeof(HashIterator));
	memset(It, 0, sizeof(HashIterator));
	It->StaticList = StaticList;
	It->AdditionalParams = AdditionalParams;
	It->GetHash = GetHash;
	It->DoSubTemplate = DoSubTempl;
	It->Destructor = Destructor;
	It->ContextType = ContextType;
	It->XPectContextType = XPectContextType;
	It->Flags = Flags;
	Put(Iterators, Name, len, It, NULL);
}

typedef struct _iteratestruct {
	int GroupChange;
	int oddeven;
	const char *Key;
	long KeyLen;
	int n;
	int LastN;
	}IterateStruct; 

int preeval_iterate(WCTemplateToken *Token)
{
	WCTemplputParams TPP;
	WCTemplputParams *TP;
	void *vTmpl;
	void *vIt;
	HashIterator *It;

	memset(&TPP, 0, sizeof(WCTemplputParams));
	TP = &TPP;
	TP->Tokens = Token;
	if (!GetHash(Iterators, TKEY(0), &vIt)) {
		LogTemplateError(
			NULL, "Iterator", ERR_PARM1, TP,
			"not found");
		return 0;
	}
	if (TP->Tokens->Params[1]->Type != TYPE_SUBTEMPLATE) {
		LogTemplateError(NULL, "Iterator", ERR_PARM1, TP,
				 "Need token with type Subtemplate as param 1, have %s", 
				 TP->Tokens->Params[1]->Start);
	}
	
	/* well, we don't check the mobile stuff here... */
	if (!GetHash(LocalTemplateCache, TKEY(1), &vTmpl) &&
	    !GetHash(TemplateCache, TKEY(1), &vTmpl)) {
		LogTemplateError(NULL, "SubTemplate", ERR_PARM1, TP,
				 "referenced here doesn't exist");
	}
	Token->Preeval2 = vIt;
	It = (HashIterator *) vIt;

	if (TP->Tokens->nParameters < It->AdditionalParams + 2) {
		LogTemplateError(                               
			NULL, "Iterator", ERR_PARM1, TP,
			"doesn't work with %d params", 
			TP->Tokens->nParameters);
	}


	return 1;
}

void tmpl_iterate_subtmpl(StrBuf *Target, WCTemplputParams *TP)
{
	HashIterator *It;
	HashList *List;
	HashPos  *it;
	SortStruct *SortBy = NULL;
	void *vSortBy;
	int DetectGroupChange = 0;
	int nMembersUsed;
	void *vContext;
	void *vLastContext = NULL;
	StrBuf *SubBuf;
	WCTemplputParams IterateTP;
	WCTemplputParams SubTP;
	IterateStruct Status;

	long StartAt = 0;
	long StepWidth = 0;
	long StopAt = -1;

	memset(&Status, 0, sizeof(IterateStruct));
	
	It = (HashIterator*) TP->Tokens->Preeval2;
	if (It == NULL) {
		LogTemplateError(
			Target, "Iterator", ERR_PARM1, TP, "Unknown!");
		return;
	}

	if (TP->Tokens->nParameters < It->AdditionalParams + 2) {
		LogTemplateError(                               
			Target, "Iterator", ERR_PARM1, TP,
			"doesn't work with %d params", 
			TP->Tokens->nParameters - 1);
		return;
	}

	if ((It->XPectContextType != CTX_NONE) &&
	    (It->XPectContextType != TP->Filter.ContextType)) {
		LogTemplateError(
			Target, "Iterator", ERR_PARM1, TP,
			"requires context of type %s, have %s", 
			ContextName(It->XPectContextType), 
			ContextName(TP->Filter.ContextType));
		return ;
		
	}

	if (It->StaticList == NULL)
		List = It->GetHash(Target, TP);
	else
		List = It->StaticList;

	DetectGroupChange = (It->Flags & IT_FLAG_DETECT_GROUPCHANGE) != 0;
	if (DetectGroupChange) {
		const StrBuf *BSort;
		DetectGroupChange = 0;
		if (havebstr("SortBy")) {
			BSort = sbstr("SortBy");
			if (GetHash(SortHash, SKEY(BSort), &vSortBy) &&
			    (vSortBy != NULL)) {
				SortBy = (SortStruct*)vSortBy;
				/* first check whether its intended for us... */
				if ((SortBy->ContextType == It->ContextType)&&
				/** Ok, its us, lets see in which direction we should sort... */
				    (havebstr("SortOrder"))) {
					int SortOrder;
					SortOrder = LBSTR("SortOrder");
					if (SortOrder != 0)
						DetectGroupChange = 1;
				}
			}
		}
	}
	nMembersUsed = GetCount(List);

	StackContext (TP, &IterateTP, &Status, CTX_ITERATE, 0, TP->Tokens);
	{
		SubBuf = NewStrBuf();
	
		if (HAVE_PARAM(2)) {
			StartAt = GetTemplateTokenNumber(Target, TP, 2, 0);
		}
		if (HAVE_PARAM(3)) {
			StepWidth = GetTemplateTokenNumber(Target, TP, 3, 0);
		}
		if (HAVE_PARAM(4)) {
			StopAt = GetTemplateTokenNumber(Target, TP, 4, -1);
		}
		it = GetNewHashPos(List, StepWidth);
		if (StopAt < 0) {
			StopAt = GetCount(List);
		}
		while (GetNextHashPos(List, it, &Status.KeyLen, &Status.Key, &vContext)) {
			if ((Status.n >= StartAt) && (Status.n <= StopAt)) {
				if (DetectGroupChange && Status.n > 0) {
					Status.GroupChange = SortBy->GroupChange(vContext, vLastContext);
				}
				Status.LastN = (Status.n + 1) == nMembersUsed;
				StackContext(&IterateTP, &SubTP, vContext, It->ContextType, 0, NULL);
				{
					if (It->DoSubTemplate != NULL)
						It->DoSubTemplate(SubBuf, &SubTP);
					DoTemplate(TKEY(1), SubBuf, &SubTP);

					StrBufAppendBuf(Target, SubBuf, 0);
					FlushStrBuf(SubBuf);
				}
				UnStackContext(&SubTP);
				Status.oddeven = ! Status.oddeven;
				vLastContext = vContext;
			}
			Status.n++;
		}
	}
	UnStackContext(&IterateTP);
	FreeStrBuf(&SubBuf);
	DeleteHashPos(&it);
	if (It->Destructor != NULL)
		It->Destructor(&List);
}


int conditional_ITERATE_ISGROUPCHANGE(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);
	if (TP->Tokens->nParameters < 3)
		return 	Ctx->GroupChange;

	return TP->Tokens->Params[2]->lvalue == Ctx->GroupChange;
}

void tmplput_ITERATE_ODDEVEN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);
	if (Ctx->oddeven)
		StrBufAppendBufPlain(Target, HKEY("odd"), 0);
	else
		StrBufAppendBufPlain(Target, HKEY("even"), 0);
}


void tmplput_ITERATE_KEY(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);

	StrBufAppendBufPlain(Target, Ctx->Key, Ctx->KeyLen, 0);
}


void tmplput_ITERATE_LASTN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);
	StrBufAppendPrintf(Target, "%d", Ctx->n);
}

int conditional_ITERATE_FIRSTN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);
	return Ctx->n == 0;
}

int conditional_ITERATE_LASTN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CTX(CTX_ITERATE);
	return Ctx->LastN;
}



/*-----------------------------------------------------------------------------
 *                      Conditionals
 */
int EvaluateConditional(StrBuf *Target, int Neg, int state, WCTemplputParams **TPP)
{
	ConditionalStruct *Cond;
	int rc = 0;
	int res;
	WCTemplputParams *TP = *TPP;

	if ((TP->Tokens->Params[0]->len == 1) &&
	    (TP->Tokens->Params[0]->Start[0] == 'X'))
	{
		return - (TP->Tokens->Params[1]->lvalue);
	}
	    
	Cond = (ConditionalStruct *) TP->Tokens->PreEval;
	if (Cond == NULL) {
		LogTemplateError(
			Target, "Conditional", ERR_PARM1, TP,
			"unknown!");
		return 0;
	}

	if (!CheckContext(Target, &Cond->Filter, TP, "Conditional")) {
		return 0;
	}

	res = Cond->CondF(Target, TP);
	if (res == Neg)
		rc = TP->Tokens->Params[1]->lvalue;

	if (LoadTemplates > 5) 
		syslog(LOG_DEBUG, "<%s> : %d %d==%d\n", 
			ChrPtr(TP->Tokens->FlatToken), 
			rc, res, Neg);

	if (TP->Sub != NULL)
	{
		*TPP = TP->Sub;
	}
	return rc;
}

void RegisterContextConditional(const char *Name, long len, 
				int nParams,
				WCConditionalFunc CondF, 
				WCConditionalFunc ExitCtxCond,
				int ContextRequired)
{
	ConditionalStruct *Cond;

	Cond = (ConditionalStruct*)malloc(sizeof(ConditionalStruct));
	memset(Cond, 0, sizeof(ConditionalStruct));
	Cond->PlainName = Name;
	Cond->Filter.nMaxArgs = nParams;
	Cond->Filter.nMinArgs = nParams;
	Cond->CondF = CondF;
	Cond->CondExitCtx = ExitCtxCond;
	Cond->Filter.ContextType = ContextRequired;
	Put(Conditionals, Name, len, Cond, NULL);
}

void RegisterTokenParamDefine(const char *Name, long len, 
			      long Value)
{
	long *PVal;

	PVal = (long*)malloc(sizeof(long));
	*PVal = Value;
	Put(Defines, Name, len, PVal, NULL);
}

long GetTokenDefine(const char *Name, long len, 
		    long DefValue)
{
	void *vPVal;

	if (GetHash(Defines, Name, len, &vPVal) &&
	     (vPVal != NULL))
	 {
		 return *(long*) vPVal;
	 }
	 else
	 {
		 return DefValue;
	 }
}

void tmplput_DefStr(StrBuf *Target, WCTemplputParams *TP)
{
	const char *Str;
	long len;
	GetTemplateTokenString(Target, TP, 2, &Str, &len);
	
	StrBufAppendBufPlain(Target, Str, len, 0);
}

void tmplput_DefVal(StrBuf *Target, WCTemplputParams *TP)
{
	int val;

	val = GetTemplateTokenNumber(Target, TP, 0, 0);
	StrBufAppendPrintf(Target, "%d", val);
}

HashList *Defines;

/*-----------------------------------------------------------------------------
 *                      Context Strings
 */
void tmplput_ContextString(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, (StrBuf*)CTX(CTX_STRBUF), 0);
}
int ConditionalContextStr(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *TokenText = (StrBuf*) CTX((CTX_STRBUF));
	const char *CompareToken;
	long len;

	GetTemplateTokenString(Target, TP, 2, &CompareToken, &len);
	return strcmp(ChrPtr(TokenText), CompareToken) == 0;
}

void tmplput_ContextStringArray(StrBuf *Target, WCTemplputParams *TP)
{
	HashList *Arr = (HashList*) CTX(CTX_STRBUFARR);
	void *pV;
	int val;

	val = GetTemplateTokenNumber(Target, TP, 0, 0);
	if (GetHash(Arr, IKEY(val), &pV) && 
	    (pV != NULL)) {
		StrBufAppendTemplate(Target, TP, (StrBuf*)pV, 1);
	}
}
int ConditionalContextStrinArray(StrBuf *Target, WCTemplputParams *TP)
{
	HashList *Arr = (HashList*) CTX(CTX_STRBUFARR);
	void *pV;
	int val;
	const char *CompareToken;
	long len;

	GetTemplateTokenString(Target, TP, 2, &CompareToken, &len);
	val = GetTemplateTokenNumber(Target, TP, 0, 0);
	if (GetHash(Arr, IKEY(val), &pV) && 
	    (pV != NULL)) {
		return strcmp(ChrPtr((StrBuf*)pV), CompareToken) == 0;
	}
	else
		return 0;
}

/*-----------------------------------------------------------------------------
 *                      Boxed-API
 */

void tmpl_do_boxed(StrBuf *Target, WCTemplputParams *TP)
{
	WCTemplputParams SubTP;

	StrBuf *Headline = NULL;
	if (TP->Tokens->nParameters == 2) {
		if (TP->Tokens->Params[1]->Type == TYPE_STR) {
			Headline = NewStrBuf();
			DoTemplate(TKEY(1), Headline, TP);
		}
		else {
			const char *Ch;
			long len;
			GetTemplateTokenString(Target, 
					       TP, 
					       1,
					       &Ch,
					       &len);
			Headline = NewStrBufPlain(Ch, len);
		}
	}
	/* else TODO error? logging? */

	StackContext (TP, &SubTP, Headline, CTX_STRBUF, 0, NULL);
	{
		DoTemplate(HKEY("box_begin"), Target, &SubTP);
	}
	UnStackContext(&SubTP);
	DoTemplate(TKEY(0), Target, TP);
	DoTemplate(HKEY("box_end"), Target, TP);
	FreeStrBuf(&Headline);
}

/*-----------------------------------------------------------------------------
 *                      Tabbed-API
 */

typedef struct _tab_struct {
	long CurrentTab;
	StrBuf *TabTitle;
} tab_struct;

int preeval_do_tabbed(WCTemplateToken *Token)
{
	WCTemplputParams TPP;
	WCTemplputParams *TP;
	const char *Ch;
	long len;
	int i, nTabs;

	memset(&TPP, 0, sizeof(WCTemplputParams));
	TP = &TPP;
	TP->Tokens = Token;
	nTabs = TP->Tokens->nParameters / 2 - 1;
	if (TP->Tokens->nParameters % 2 != 0)
	{
		LogTemplateError(NULL, "TabbedApi", ERR_PARM1, TP,
				 "need even number of arguments");
		return 0;

	}
	else for (i = 0; i < nTabs; i++) {
		if (!HaveTemplateTokenString(NULL, 
					     TP, 
					     i * 2,
					     &Ch,
					     &len) || 
		    (TP->Tokens->Params[i * 2]->len == 0))
		{
			LogTemplateError(NULL, "TabbedApi", ERR_PARM1, TP,
					 "Tab-Subject %d needs to be able to produce a string, have %s", 
					 i, TP->Tokens->Params[i * 2]->Start);
			return 0;
		}
		if (!HaveTemplateTokenString(NULL, 
					     TP, 
					     i * 2 + 1,
					     &Ch,
					     &len) || 
		    (TP->Tokens->Params[i * 2 + 1]->len == 0))
		{
			LogTemplateError(NULL, "TabbedApi", ERR_PARM1, TP,
					 "Tab-Content %d needs to be able to produce a string, have %s", 
					 i, TP->Tokens->Params[i * 2 + 1]->Start);
			return 0;
		}
	}

	if (!HaveTemplateTokenString(NULL, 
				     TP, 
				     i * 2 + 1,
				     &Ch,
				     &len) || 
	    (TP->Tokens->Params[i * 2 + 1]->len == 0))
	{
		LogTemplateError(NULL, "TabbedApi", ERR_PARM1, TP,
				 "Tab-Content %d needs to be able to produce a string, have %s", 
				 i, TP->Tokens->Params[i * 2 + 1]->Start);
		return 0;
	}
	return 1;
}


void tmpl_do_tabbed(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf **TabNames;
	int i, ntabs, nTabs;
	tab_struct TS;
	WCTemplputParams SubTP;

	memset(&TS, 0, sizeof(tab_struct));

	nTabs = ntabs = TP->Tokens->nParameters / 2;
	TabNames = (StrBuf **) malloc(ntabs * sizeof(StrBuf*));
	memset(TabNames, 0, ntabs * sizeof(StrBuf*));

	for (i = 0; i < ntabs; i++) {
		if ((TP->Tokens->Params[i * 2]->Type == TYPE_STR) &&
		    (TP->Tokens->Params[i * 2]->len > 0)) {
			TabNames[i] = NewStrBuf();
			DoTemplate(TKEY(i * 2), TabNames[i], TP);
		}
		else if (TP->Tokens->Params[i * 2]->Type == TYPE_GETTEXT) {
			const char *Ch;
			long len;
			GetTemplateTokenString(Target, 
					       TP, 
					       i * 2,
					       &Ch,
					       &len);
			TabNames[i] = NewStrBufPlain(Ch, -1);
		}
		else { 
			/** A Tab without subject? we can't count that, add it as silent */
			nTabs --;
		}
	}
	StackContext (TP, &SubTP, &TS, CTX_TAB, 0, NULL);
	{
		StrTabbedDialog(Target, nTabs, TabNames);
		for (i = 0; i < ntabs; i++) {
			memset(&TS, 0, sizeof(tab_struct));
			TS.CurrentTab = i;
			TS.TabTitle = TabNames[i];
			StrBeginTab(Target, i, nTabs, TabNames);
			DoTemplate(TKEY(i * 2 + 1), Target, &SubTP);
			StrEndTab(Target, i, nTabs);
		}
		for (i = 0; i < ntabs; i++) 
			FreeStrBuf(&TabNames[i]);
		free(TabNames);
	}
	UnStackContext(&SubTP);
}

void tmplput_TAB_N(StrBuf *Target, WCTemplputParams *TP)
{
	tab_struct *Ctx = CTX(CTX_TAB);

	StrBufAppendPrintf(Target, "%d", Ctx->CurrentTab);
}

void tmplput_TAB_TITLE(StrBuf *Target, WCTemplputParams *TP)
{
	tab_struct *Ctx = CTX(CTX_TAB);
	StrBufAppendTemplate(Target, TP, Ctx->TabTitle, 0);
}

/*-----------------------------------------------------------------------------
 *                      Sorting-API
 */


void RegisterSortFunc(const char *name, long len, 
		      const char *prepend, long preplen,
		      CompareFunc Forward, 
		      CompareFunc Reverse, 
		      CompareFunc GroupChange, 
		      CtxType ContextType)
{
	SortStruct *NewSort;

	NewSort = (SortStruct*) malloc(sizeof(SortStruct));
	memset(NewSort, 0, sizeof(SortStruct));
	NewSort->Name = NewStrBufPlain(name, len);
	if (prepend != NULL)
		NewSort->PrefPrepend = NewStrBufPlain(prepend, preplen);
	else
		NewSort->PrefPrepend = NULL;
	NewSort->Forward = Forward;
	NewSort->Reverse = Reverse;
	NewSort->GroupChange = GroupChange;
	NewSort->ContextType = ContextType;
	if (ContextType == CTX_NONE) {
		syslog(LOG_WARNING, "sorting requires a context. CTX_NONE won't make it.\n");
		exit(1);
	}
		
	Put(SortHash, name, len, NewSort, DestroySortStruct);
}

CompareFunc RetrieveSort(WCTemplputParams *TP, 
			 const char *OtherPrefix, long OtherPrefixLen,
			 const char *Default, long ldefault, long DefaultDirection)
{
	const StrBuf *BSort = NULL;
	SortStruct *SortBy;
	void *vSortBy;
	long SortOrder = -1;
	
	if (havebstr("SortBy")) {
		BSort = sbstr("SortBy");
		if (OtherPrefix == NULL) {
			set_room_pref("sort", NewStrBufDup(BSort), 0);
		}
		else {
			set_X_PREFS(HKEY("sort"), OtherPrefix, OtherPrefixLen, NewStrBufDup(BSort), 0);
		}
	}
	else { /** Try to fallback to our remembered values... */
		if (OtherPrefix == NULL) {
			BSort = get_room_pref("sort");
		}
		else {
			BSort = get_X_PREFS(HKEY("sort"), OtherPrefix, OtherPrefixLen);
		}
		if (BSort != NULL)
			putbstr("SortBy", NewStrBufDup(BSort));
		else {
			StrBuf *Buf;

			BSort = Buf = NewStrBufPlain(Default, ldefault);
			putbstr("SortBy", Buf);
		}
	}

	if (!GetHash(SortHash, SKEY(BSort), &vSortBy) || 
	    (vSortBy == NULL)) {
		if (!GetHash(SortHash, Default, ldefault, &vSortBy) || 
		    (vSortBy == NULL)) {
			LogTemplateError(
				NULL, "Sorting", ERR_PARM1, TP,
				"Illegal default sort: [%s]", Default);
			wc_backtrace(LOG_WARNING);
		}
	}
	SortBy = (SortStruct*)vSortBy;

	if (SortBy->ContextType != TP->Filter.ContextType)
		return NULL;

	/** Ok, its us, lets see in which direction we should sort... */
	if (havebstr("SortOrder")) {
		SortOrder = LBSTR("SortOrder");
	}
	else { /** Try to fallback to our remembered values... */
		StrBuf *Buf = NULL;
		if (SortBy->PrefPrepend == NULL) {
			Buf = get_room_pref("SortOrder");
			SortOrder = StrTol(Buf);
		}
		else {
			BSort = get_X_PREFS(HKEY("SortOrder"), OtherPrefix, OtherPrefixLen);
		}

		if (Buf == NULL)
			SortOrder = DefaultDirection;

		Buf = NewStrBufPlain(NULL, 64);
		StrBufPrintf(Buf, "%ld", SortOrder);
		putbstr("SortOrder", Buf);
	}
	switch (SortOrder) {
	default:
	case 0:
		return NULL;
	case 1:
		return SortBy->Forward;
	case 2:
		return SortBy->Reverse;
	}
}


enum {
	eNO_SUCH_SORT, 
	eNOT_SPECIFIED,
	eINVALID_PARAM,
	eFOUND
};

ConstStr SortIcons[] = {
	{HKEY("static/webcit_icons/sort_none.gif")},
	{HKEY("static/webcit_icons/up_pointer.gif")},
	{HKEY("static/webcit_icons/down_pointer.gif")},
};

ConstStr SortNextOrder[] = {
	{HKEY("1")},
	{HKEY("2")},
	{HKEY("0")},
};


int GetSortMetric(WCTemplputParams *TP, SortStruct **Next, SortStruct **Param, long *SortOrder, int N)
{
	int bSortError = eNOT_SPECIFIED;
	const StrBuf *BSort;
	void *vSort;
	
	*SortOrder = 0;
	*Next = NULL;
	if (!GetHash(SortHash, TKEY(0), &vSort) || 
	    (vSort == NULL))
		return eNO_SUCH_SORT;
	*Param = (SortStruct*) vSort;
	

	if (havebstr("SortBy")) {
		BSort = sbstr("SortBy");
		bSortError = eINVALID_PARAM;
		if ((*Param)->PrefPrepend == NULL) {
			set_room_pref("sort", NewStrBufDup(BSort), 0);
		}
		else {
			set_X_PREFS(HKEY("sort"), TKEY(N), NewStrBufDup(BSort), 0);
		}
	}
	else { /** Try to fallback to our remembered values... */
		if ((*Param)->PrefPrepend == NULL) {
			BSort = get_room_pref("sort");
		}
		else {
			BSort = get_X_PREFS(HKEY("sort"), TKEY(N));
		}
	}

	if (!GetHash(SortHash, SKEY(BSort), &vSort) || 
	    (vSort == NULL))
		return bSortError;

	*Next = (SortStruct*) vSort;

	/** Ok, its us, lets see in which direction we should sort... */
	if (havebstr("SortOrder")) {
		*SortOrder = LBSTR("SortOrder");
	}
	else { /** Try to fallback to our remembered values... */
		if ((*Param)->PrefPrepend == NULL) {
			*SortOrder = StrTol(get_room_pref("SortOrder"));
		}
		else {
			*SortOrder = StrTol(get_X_PREFS(HKEY("SortOrder"), TKEY(N)));
		}
	}
	if (*SortOrder > 2)
		*SortOrder = 0;

	return eFOUND;
}


void tmplput_SORT_ICON(StrBuf *Target, WCTemplputParams *TP)
{
	long SortOrder;
	SortStruct *Next;
	SortStruct *Param;
	const ConstStr *SortIcon;

	switch (GetSortMetric(TP, &Next, &Param, &SortOrder, 2)){
	case eNO_SUCH_SORT:
                LogTemplateError(
                        Target, "Sorter", ERR_PARM1, TP,
			" Sorter [%s] unknown!", 
			TP->Tokens->Params[0]->Start);
		break;		
	case eINVALID_PARAM:
                LogTemplateError(NULL, "Sorter", ERR_PARM1, TP,
				 " Sorter specified by BSTR 'SortBy' [%s] unknown!", 
				 bstr("SortBy"));
	case eNOT_SPECIFIED:
	case eFOUND:
		if (Next == Param) {
			SortIcon = &SortIcons[SortOrder];
		}
		else { /** Not Us... */
			SortIcon = &SortIcons[0];
		}
		StrBufAppendBufPlain(Target, SortIcon->Key, SortIcon->len, 0);
	}
}

void tmplput_SORT_NEXT(StrBuf *Target, WCTemplputParams *TP)
{
	long SortOrder;
	SortStruct *Next;
	SortStruct *Param;

	switch (GetSortMetric(TP, &Next, &Param, &SortOrder, 2)){
	case eNO_SUCH_SORT:
                LogTemplateError(
                        Target, "Sorter", ERR_PARM1, TP,                                  
			" Sorter [%s] unknown!", 
			TP->Tokens->Params[0]->Start);
		break;		
	case eINVALID_PARAM:
                LogTemplateError(
                        NULL, "Sorter", ERR_PARM1, TP,
			" Sorter specified by BSTR 'SortBy' [%s] unknown!", 
			bstr("SortBy"));
	case eNOT_SPECIFIED:
	case eFOUND:
		StrBufAppendBuf(Target, Param->Name, 0);
		
	}
}

void tmplput_SORT_ORDER(StrBuf *Target, WCTemplputParams *TP)
{
	long SortOrder;
	const ConstStr *SortOrderStr;
	SortStruct *Next;
	SortStruct *Param;

	switch (GetSortMetric(TP, &Next, &Param, &SortOrder, 2)){
	case eNO_SUCH_SORT:
                LogTemplateError(
                        Target, "Sorter", ERR_PARM1, TP,
                        " Sorter [%s] unknown!",
                        TP->Tokens->Params[0]->Start);
		break;		
	case eINVALID_PARAM:
                LogTemplateError(
                        NULL, "Sorter", ERR_PARM1, TP,
                        " Sorter specified by BSTR 'SortBy' [%s] unknown!",
                        bstr("SortBy"));
	case eNOT_SPECIFIED:
	case eFOUND:
		if (Next == Param) {
			SortOrderStr = &SortNextOrder[SortOrder];
		}
		else { /** Not Us... */
			SortOrderStr = &SortNextOrder[0];
		}
		StrBufAppendBufPlain(Target, SortOrderStr->Key, SortOrderStr->len, 0);
	}
}


void tmplput_long_vector(StrBuf *Target, WCTemplputParams *TP)
{
	long *LongVector = (long*) CTX(CTX_LONGVECTOR);

	if ((TP->Tokens->Params[0]->Type == TYPE_LONG) && 
	    (TP->Tokens->Params[0]->lvalue <= LongVector[0]))
	{
		StrBufAppendPrintf(Target, "%ld", LongVector[TP->Tokens->Params[0]->lvalue]);
	}
	else
	{
		if (TP->Tokens->Params[0]->Type != TYPE_LONG) {
			LogTemplateError(
				Target, "Longvector", ERR_NAME, TP,
				"needs a numerical Parameter!");
		}
		else {
			LogTemplateError(
				Target, "LongVector", ERR_PARM1, TP,
				"doesn't have %ld Parameters, its just the size of %ld!", 
				TP->Tokens->Params[0]->lvalue,
				LongVector[0]);
		}
	}
}

void dbg_print_longvector(long *LongVector)
{
	StrBuf *Buf = NewStrBufPlain(HKEY("Longvector: ["));
	int nItems = LongVector[0];
	int i;

	for (i = 0; i < nItems; i++) {
		if (i + 1 < nItems)
			StrBufAppendPrintf(Buf, "%d: %ld | ", i, LongVector[i]);
		else
			StrBufAppendPrintf(Buf, "%d: %ld]\n", i, LongVector[i]);

	}
	syslog(LOG_DEBUG, "%s", ChrPtr(Buf));
	FreeStrBuf(&Buf);
}

int ConditionalLongVector(StrBuf *Target, WCTemplputParams *TP)
{
	long *LongVector = (long*) CTX(CTX_LONGVECTOR);

	if ((TP->Tokens->Params[2]->Type == TYPE_LONG) && 
	    (TP->Tokens->Params[2]->lvalue <= LongVector[0])&&
	    (TP->Tokens->Params[3]->Type == TYPE_LONG) && 
	    (TP->Tokens->Params[3]->lvalue <= LongVector[0]))
	{
		return LongVector[TP->Tokens->Params[2]->lvalue] == 
			LongVector[TP->Tokens->Params[3]->lvalue];
	}
	else
	{
		if ((TP->Tokens->Params[2]->Type == TYPE_LONG) ||
		    (TP->Tokens->Params[2]->Type == TYPE_LONG)) {
			LogTemplateError(
				Target, "ConditionalLongvector", ERR_PARM1, TP,
				"needs two long Parameter!");
		}
		else {
			LogTemplateError(
				Target, "Longvector", ERR_PARM1, TP,
				"doesn't have %ld / %ld Parameters, its just the size of %ld!",
				TP->Tokens->Params[2]->lvalue,
				TP->Tokens->Params[3]->lvalue,
				LongVector[0]);
		}
	}
	return 0;
}


void tmplput_CURRENT_FILE(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, TP->Tokens->FileName, 0);
}

void 
InitModule_SUBST
(void)
{
	RegisterCTX(CTX_TAB);
	RegisterCTX(CTX_ITERATE);

	memset(&NoCtx, 0, sizeof(WCTemplputParams));
	RegisterNamespace("--", 0, 2, tmplput_Comment, NULL, CTX_NONE);
	RegisterNamespace("SORT:ICON", 1, 2, tmplput_SORT_ICON, NULL, CTX_NONE);
	RegisterNamespace("SORT:ORDER", 1, 2, tmplput_SORT_ORDER, NULL, CTX_NONE);
	RegisterNamespace("SORT:NEXT", 1, 2, tmplput_SORT_NEXT, NULL, CTX_NONE);
	RegisterNamespace("CONTEXTSTR", 0, 1, tmplput_ContextString, NULL, CTX_STRBUF);
	RegisterNamespace("CONTEXTSTRARR", 1, 2, tmplput_ContextStringArray, NULL, CTX_STRBUFARR);
	RegisterNamespace("ITERATE", 2, 100, tmpl_iterate_subtmpl, preeval_iterate, CTX_NONE);
	RegisterNamespace("DOBOXED", 1, 2, tmpl_do_boxed, NULL, CTX_NONE);
	RegisterNamespace("DOTABBED", 2, 100, tmpl_do_tabbed, preeval_do_tabbed, CTX_NONE);
	RegisterNamespace("TAB:N", 0, 0, tmplput_TAB_N, NULL, CTX_TAB);
	RegisterNamespace("TAB:SUBJECT", 0, 1, tmplput_TAB_TITLE, NULL, CTX_TAB);


	RegisterNamespace("LONGVECTOR", 1, 1, tmplput_long_vector, NULL, CTX_LONGVECTOR);


	RegisterConditional("COND:CONTEXTSTR", 3, ConditionalContextStr, CTX_STRBUF);
	RegisterConditional("COND:CONTEXTSTRARR", 4, ConditionalContextStrinArray, CTX_STRBUFARR);
	RegisterConditional("COND:LONGVECTOR", 4, ConditionalLongVector, CTX_LONGVECTOR);


	RegisterConditional("COND:ITERATE:ISGROUPCHANGE", 2, 
			    conditional_ITERATE_ISGROUPCHANGE, 
			    CTX_ITERATE);
	RegisterConditional("COND:ITERATE:LASTN", 2, 
			    conditional_ITERATE_LASTN, 
			    CTX_ITERATE);
	RegisterConditional("COND:ITERATE:FIRSTN", 2, 
			    conditional_ITERATE_FIRSTN, 
			    CTX_ITERATE);

	RegisterNamespace("ITERATE:ODDEVEN", 0, 0, tmplput_ITERATE_ODDEVEN, NULL, CTX_ITERATE);
	RegisterNamespace("ITERATE:KEY", 0, 0, tmplput_ITERATE_KEY, NULL, CTX_ITERATE);
	RegisterNamespace("ITERATE:N", 0, 0, tmplput_ITERATE_LASTN, NULL, CTX_ITERATE);
	RegisterNamespace("CURRENTFILE", 0, 1, tmplput_CURRENT_FILE, NULL, CTX_NONE);
	RegisterNamespace("DEF:STR", 1, 1, tmplput_DefStr, NULL, CTX_NONE);
	RegisterNamespace("DEF:VAL", 1, 1, tmplput_DefVal, NULL, CTX_NONE);




}

void
ServerStartModule_SUBST
(void)
{
	LocalTemplateCache = NewHash(1, NULL);
	TemplateCache = NewHash(1, NULL);

	GlobalNS = NewHash(1, NULL);
	Iterators = NewHash(1, NULL);
	Conditionals = NewHash(1, NULL);
	SortHash = NewHash(1, NULL);
	Defines = NewHash(1, NULL);
	CtxList = NewHash(1, NULL);
	
	PutContextType(HKEY("CTX_NONE"), 0);

	RegisterCTX(CTX_STRBUF);
	RegisterCTX(CTX_STRBUFARR);
	RegisterCTX(CTX_LONGVECTOR);
}

void
FinalizeModule_SUBST
(void)
{

}

void 
ServerShutdownModule_SUBST
(void)
{
	DeleteHash(&TemplateCache);
	DeleteHash(&LocalTemplateCache);

	DeleteHash(&GlobalNS);
	DeleteHash(&Iterators);
	DeleteHash(&Conditionals);
	DeleteHash(&SortHash);
	DeleteHash(&Defines);
	DeleteHash(&CtxList);
}


void
SessionNewModule_SUBST
(wcsession *sess)
{

}

void
SessionAttachModule_SUBST
(wcsession *sess)
{
}

void
SessionDetachModule_SUBST
(wcsession *sess)
{
	FreeStrBuf(&sess->WFBuf);
}

void 
SessionDestroyModule_SUBST  
(wcsession *sess)
{

}
