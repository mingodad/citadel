/*
 * $Id$
 */
/**
 * \defgroup Subst Variable substitution type stuff
 * \ingroup CitadelConfig
 */

/*@{*/

#include "sysdep.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#define SHOW_ME_VAPPEND_PRINTF

#include "webcit.h"
#include "webserver.h"

extern char *static_dirs[PATH_MAX];  /**< Disk representation */

HashList *WirelessTemplateCache;
HashList *WirelessLocalTemplateCache;
HashList *TemplateCache;
HashList *LocalTemplateCache;

HashList *GlobalNS;
HashList *Iterators;
HashList *Conditionals;
HashList *SortHash;

int LoadTemplates = 0;
int dbg_bactrace_template_errors = 0;
WCTemplputParams NoCtx;

#define SV_GETTEXT 1
#define SV_CONDITIONAL 2
#define SV_NEG_CONDITIONAL 3
#define SV_CUST_STR_CONDITIONAL 4
#define SV_SUBTEMPL 5
#define SV_PREEVALUATED 6

typedef struct _WCTemplate {
	StrBuf *Data;
	StrBuf *FileName;
	int nTokensUsed;
	int TokenSpace;
	WCTemplateToken **Tokens;
} WCTemplate;

typedef struct _HashHandler {
	ContextFilter Filter;

	WCHandlerFunc HandlerFunc;
}HashHandler;

void *load_template(StrBuf *filename, StrBuf *Key, HashList *PutThere);
int EvaluateConditional(StrBuf *Target, int Neg, int state, WCTemplputParams *TP);



typedef struct _SortStruct {
	StrBuf *Name;
	StrBuf *PrefPrepend;
	CompareFunc Forward;
	CompareFunc Reverse;
	CompareFunc GroupChange;

	long ContextType;
}SortStruct;

const char *CtxNames[]  = {
	"Context NONE",
	"Context SITECFG",
	"Context SESSION",
	"Context INETCFG",
	"Context VNOTE",
	"Context WHO",
	"Context PREF",
	"Context NODECONF",
	"Context USERLIST",
	"Context MAILSUM",
	"Context MIME_ATACH",
	"Context FILELIST",
	"Context STRBUF",
	"Context LONGVECTOR",
	"Context ROOMS",
	"Context FLOORS",
	"Context ITERATE",
	"Context UNKNOWN"
};



void DestroySortStruct(void *vSort)
{
	SortStruct *Sort = (SortStruct*) vSort;
	FreeStrBuf(&Sort->Name);
	FreeStrBuf(&Sort->PrefPrepend);
	free (Sort);
}

const char *ContextName(int ContextType)
{
	if (ContextType < CTX_UNKNOWN)
		return CtxNames[ContextType];
	else
		return CtxNames[CTX_UNKNOWN];
}

void LogTemplateError (StrBuf *Target, const char *Type, int ErrorPos, WCTemplputParams *TP, const char *Format, ...)
{
	wcsession *WCC;
	StrBuf *Header;
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
	default:
	case ERR_NAME: /* the main token name... */ 
		Err = (TP->Tokens!= NULL)? TP->Tokens->pName:"";
		break;
	case ERR_PARM1:
		Err = (TP->Tokens!= NULL)? TP->Tokens->Params[0]->Start:"";
		break;
	case ERR_PARM2:
		Err = (TP->Tokens!= NULL)? TP->Tokens->Params[1]->Start:"";
		break;
	}
	if (TP->Tokens != NULL) 
	{
		lprintf(1, "%s [%s]  (in '%s' line %ld); %s; [%s]\n", 
			Type, 
			Err, 
			ChrPtr(TP->Tokens->FileName),
			TP->Tokens->Line, 
			ChrPtr(Error), 
			ChrPtr(TP->Tokens->FlatToken));
	}
	else 
	{
		lprintf(1, "%s: %s;\n", 
			Type, 
			ChrPtr(Error));
	}
	if (Target == NULL) 
		return;
	WCC = WC;
	Header = NewStrBuf();
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

		SerializeJson(Header, WildFireException(SKEY(TP->Tokens->FileName),
							TP->Tokens->Line,
							Error,
							1), 1);
/*
		SerializeJson(Header, WildFireMessage(SKEY(TP->Tokens->FileName),
						      TP->Tokens->Line,
						      Error,
						      eERROR), 1);
*/
		WildFireSerializePayload(Header, WCC->HBuf, &WCC->nWildfireHeaders, NULL);
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
		SerializeJson(Header, WildFireException(HKEY(__FILE__), __LINE__, Error, 1), 1);
		WildFireSerializePayload(Header, WCC->HBuf, &WCC->nWildfireHeaders, NULL);
	}
	FreeStrBuf(&Header);
/*
	if (dbg_bactrace_template_errors)
		wc_backtrace(); 
*/
}


void RegisterNS(const char *NSName, 
		long len, 
		int nMinArgs, 
		int nMaxArgs, 
		WCHandlerFunc HandlerFunc, 
		int ContextRequired)
{
	HashHandler *NewHandler;
	
	NewHandler = (HashHandler*) malloc(sizeof(HashHandler));
	NewHandler->Filter.nMinArgs = nMinArgs;
	NewHandler->Filter.nMaxArgs = nMaxArgs;
	NewHandler->Filter.ContextType = ContextRequired;
	NewHandler->Filter.ControlContextType = CTX_NONE;

	NewHandler->HandlerFunc = HandlerFunc;	
	Put(GlobalNS, NSName, len, NewHandler, NULL);
}

void RegisterControlNS(const char *NSName, 
		       long len, 
		       int nMinArgs, 
		       int nMaxArgs, 
		       WCHandlerFunc HandlerFunc, 
		       int ControlContextRequired)
{
	HashHandler *NewHandler;
	
	NewHandler = (HashHandler*) malloc(sizeof(HashHandler));
	NewHandler->Filter.nMinArgs = nMinArgs;
	NewHandler->Filter.nMaxArgs = nMaxArgs;
	NewHandler->Filter.ContextType = CTX_NONE;
	NewHandler->Filter.ControlContextType = ControlContextRequired;
	NewHandler->HandlerFunc = HandlerFunc;	
	Put(GlobalNS, NSName, len, NewHandler, NULL);
}



int CheckContext(StrBuf *Target, ContextFilter *Need, WCTemplputParams *TP, const char *ErrType)
{
	if ((Need->ContextType != CTX_NONE) && 
	    (Need->ContextType != TP->Filter.ContextType)) {
                LogTemplateError(
                        Target, ErrType, ERR_PARM1, TP,
			"  WARNING: requires Context: [%s], have [%s]!", 
			ContextName(Need->ContextType), 
			ContextName(TP->Filter.ContextType));
		return 0;
	}

	if ((Need->ControlContextType != CTX_NONE) && 
	    (Need->ControlContextType != TP->Filter.ControlContextType)) {
                LogTemplateError(
                        Target, ErrType, ERR_PARM1, TP,
			"  WARNING: requires Control Context: [%s], have [%s]!", 
			ContextName(Need->ControlContextType), 
			ContextName(TP->Filter.ControlContextType));
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
	free(FreeMe);
}


/**
 * \brief debugging function to print array to log
 */
void VarPrintTransition(void *vVar1, void *vVar2, int odd){}
/**
 * \brief debugging function to print array to log
 */
void VarPrintEntry(const char *Key, void *vSubst, int odd)
{
	wcsubst *ptr;
	lprintf(1,"Subst[%s] : ", Key);
	ptr = (wcsubst*) vSubst;

	switch(ptr->wcs_type) {
	case WCS_STRING:
		lprintf(1, "  -> %s\n", ChrPtr(ptr->wcs_value));
		break;
	case WCS_SERVCMD:
		lprintf(1, "  -> Server [%s]\n", ChrPtr(ptr->wcs_value));
		break;
	case WCS_FUNCTION:
		lprintf(1, "  -> function at [%0xd]\n", ptr->wcs_function);
		break;
	default:
		lprintf(1,"  WARNING: invalid type: [%ld]!\n", ptr->wcs_type);
	}
}



/**
 * \brief Clear out the list of substitution variables local to this session
 */
void clear_substs(wcsession *wc) {

	if (wc->vars != NULL) {
		DeleteHash(&wc->vars);
	}
}

/**
 * \brief Clear out the list of substitution variables local to this session
 */
void clear_local_substs(void) {
	clear_substs (WC);
}

int NeedNewBuf(type)
{
	switch(type) {
	case WCS_STRING:
	case WCS_SERVCMD:
	case WCS_STRBUF:
		return 1;
	case WCS_FUNCTION:
	case WCS_STRBUF_REF:
	case WCS_LONG:
	default:
		return 0;
	}
}

void FlushPayload(wcsubst *ptr, int reusestrbuf, int type)
{
	int NeedNew = NeedNewBuf(type);
	switch(ptr->wcs_type) {
	case WCS_STRING:
	case WCS_SERVCMD:
	case WCS_STRBUF:
		if (reusestrbuf && NeedNew) {
			FlushStrBuf(ptr->wcs_value);
		}
		else {
			
			FreeStrBuf(&ptr->wcs_value);
			ptr->wcs_value = NULL;
		}
		break;
	case WCS_FUNCTION:
		ptr->wcs_function = NULL;
		if (reusestrbuf && NeedNew)
			ptr->wcs_value = NewStrBuf();
		break;
	case WCS_STRBUF_REF:
		ptr->wcs_value = NULL;
		if (reusestrbuf && NeedNew)
			ptr->wcs_value = NewStrBuf();
		break;
	case WCS_LONG:
		ptr->lvalue = 0;
		if (reusestrbuf && NeedNew)
			ptr->wcs_value = NewStrBuf();
		break;
	default:
		if (reusestrbuf && NeedNew)
			ptr->wcs_value = NewStrBuf();
		break;
	}
}


/**
 * \brief destructor; kill one entry.
 */
void deletevar(void *data)
{
	wcsubst *ptr = (wcsubst*)data;
	FlushPayload(ptr, 0, ptr->wcs_type);
	free(ptr);	
}


wcsubst *NewSubstVar(const char *keyname, int keylen, int type)
{
	wcsubst* ptr;
	wcsession *WCC = WC;

	ptr = (wcsubst *) malloc(sizeof(wcsubst));
	memset(ptr, 0, sizeof(wcsubst));

	ptr->wcs_type = type;
	safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
	Put(WCC->vars, keyname, keylen, ptr,  deletevar);

	switch(ptr->wcs_type) {
	case WCS_STRING:
	case WCS_SERVCMD:
		ptr->wcs_value = NewStrBuf();
		break;
	case WCS_STRBUF:
	case WCS_FUNCTION:
	case WCS_STRBUF_REF:
	case WCS_LONG:
	default:
		break;
	}
	return ptr;
}


/**
 * \brief Add a substitution variable (local to this session) (strlen version...)
 * \param keyname the replacementstring to substitute
 * \param keytype the kind of the key
 * \param format the format string ala printf
 * \param ... the arguments to substitute in the formatstring
 */
void SVPRINTF(char *keyname, int keytype, const char *format,...)
{
	va_list arg_ptr;
	void *vPtr;
	wcsubst *ptr = NULL;
	size_t keylen;
	wcsession *WCC = WC;
	
	keylen = strlen(keyname);
	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, keytype, keytype);
		ptr->wcs_type = keytype;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, keytype);
	}

	/** Format the string */
	va_start(arg_ptr, format);
	StrBufVAppendPrintf(ptr->wcs_value, format, arg_ptr);
	va_end(arg_ptr);
}

/**
 * \brief Add a substitution variable (local to this session)
 * \param keyname the replacementstring to substitute
 * \param keytype the kind of the key
 * \param format the format string ala printf
 * \param ... the arguments to substitute in the formatstring
 */
void svprintf(char *keyname, size_t keylen, int keytype, const char *format,...)
{
	va_list arg_ptr;
	void *vPtr;
	wcsubst *ptr = NULL;
	wcsession *WCC = WC;
		
	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, 1, keytype);
		ptr->wcs_type = keytype;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, keytype);
	}

	/** Format the string and save it */
	va_start(arg_ptr, format);
	StrBufVAppendPrintf(ptr->wcs_value, format, arg_ptr);
	va_end(arg_ptr);
}

/**
 * \brief Add a substitution variable (local to this session)
 * \param keyname the replacementstring to substitute
 * \param keytype the kind of the key
 * \param format the format string ala printf
 * \param ... the arguments to substitute in the formatstring
 */
void SVPut(char *keyname, size_t keylen, int keytype, char *Data)
{
	void *vPtr;
	wcsubst *ptr = NULL;
	wcsession *WCC = WC;

	
	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, 1, keytype);
		ptr->wcs_type = keytype;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, keytype);
	}
	StrBufAppendBufPlain(ptr->wcs_value, Data, -1, 0);
}

/**
 * \brief Add a substitution variable (local to this session)
 * \param keyname the replacementstring to substitute
 * \param keytype the kind of the key
 * \param format the format string ala printf
 * \param ... the arguments to substitute in the formatstring
 */
void SVPutLong(char *keyname, size_t keylen, long Data)
{
	void *vPtr;
	wcsubst *ptr = NULL;
	wcsession *WCC = WC;

	
	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, 1, WCS_LONG);
		ptr->wcs_type = WCS_LONG;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, WCS_LONG);
	}
	ptr->lvalue = Data;
}

/**
 * \brief Add a substitution variable (local to this session) that does a callback
 * \param keyname the keystring to substitute
 * \param fcn_ptr the function callback to give the substitution string
 */
void SVCallback(char *keyname, size_t keylen, WCHandlerFunc fcn_ptr)
{
	wcsubst *ptr;
	void *vPtr;
	wcsession *WCC = WC;

	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, 1, WCS_FUNCTION);
		ptr->wcs_type = WCS_FUNCTION;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, WCS_FUNCTION);
	}

	ptr->wcs_function = fcn_ptr;
}
inline void SVCALLBACK(char *keyname, WCHandlerFunc fcn_ptr)
{
	SVCallback(keyname, strlen(keyname), fcn_ptr);
}



void SVPUTBuf(const char *keyname, int keylen, const StrBuf *Buf, int ref)
{
	wcsubst *ptr;
	void *vPtr;
	wcsession *WCC = WC;

	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		FlushPayload(ptr, 0, (ref)?WCS_STRBUF_REF:WCS_STRBUF);
		ptr->wcs_type = (ref)?WCS_STRBUF_REF:WCS_STRBUF;
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = NewSubstVar(keyname, keylen, (ref)?WCS_STRBUF_REF:WCS_STRBUF);
	}
	ptr->wcs_value = (StrBuf*)Buf;
}

/**
 * \brief back end for print_value_of() ... does a server command
 * \param servcmd server command to execute on the citadel server
 */
void pvo_do_cmd(StrBuf *Target, StrBuf *servcmd) {
	char buf[SIZ];
	int len;

	serv_puts(ChrPtr(servcmd));
	len = serv_getln(buf, sizeof buf);

	switch(buf[0]) {
		case '2':
		case '3':
		case '5':
			StrBufAppendPrintf(Target, "%s\n", &buf[4]);
			break;
		case '1':
			_fmout(Target, "CENTER");
			break;
		case '4':
			StrBufAppendPrintf(Target, "%s\n", &buf[4]);
			serv_puts("000");
			break;
	}
}

void GetTemplateTokenString(WCTemplputParams *TP,
			    int N,
			    const char **Value, 
			    long *len)
{
	StrBuf *Buf;
	WCTemplputParams SubTP;

	if (TP->Tokens->nParameters < N) {
		lprintf(1, "invalid token. this shouldn't have come till here.\n");
		wc_backtrace(); 
		*Value = "";
		*len = 0;
		return;
	}

	switch (TP->Tokens->Params[N]->Type) {

	case TYPE_STR:
		*Value = TP->Tokens->Params[N]->Start;
		*len = TP->Tokens->Params[N]->len;
		break;
	case TYPE_BSTR:
		Buf = (StrBuf*) SBstr(TKEY(N));
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		break;
	case TYPE_PREFSTR:
		get_PREFERENCE(TKEY(N), &Buf);
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		break;
	case TYPE_LONG:
	case TYPE_PREFINT:
		break; /* todo: string to text? */
	case TYPE_GETTEXT:
		*Value = _(TP->Tokens->Params[N]->Start);
		*len = strlen(*Value);
		break;
	case TYPE_SUBTEMPLATE:
		memset(&SubTP, 0, sizeof(WCTemplputParams *));
		SubTP.Context = TP->Context;
		SubTP.Filter.ContextType = TP->Filter.ContextType;
		Buf = NewStrBuf();
		DoTemplate(TKEY(N), Buf, &SubTP);
		*Value = ChrPtr(Buf);
		*len = StrLength(Buf);
		/* we can't free it here, so we put it into the subst so its discarded later on. */
		SVPUTBuf(TKEY(N), Buf, 0);
		break;

	default:
		break;
/*/todo log error */
	}
}



/**
 * \brief Print the value of a variable
 * \param keyname get a key to print
 */
void print_value_of(StrBuf *Target, WCTemplputParams *TP)
 
{
	wcsession *WCC = WC;
	wcsubst *ptr;
	void *vVar;

	/*if (WCC->vars != NULL) PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	/* TODO: depricated! */
	if (TP->Tokens->pName[0] == '=') {
		DoTemplate(TP->Tokens->pName+1, TP->Tokens->NameEnd - 1, NULL, &NoCtx);
	}
/*/////TODO: if param[1] == "U" -> urlescape
/// X -> escputs */
	/** Page-local variables */
	if ((WCC->vars!= NULL) && GetHash(WCC->vars, TP->Tokens->pName, TP->Tokens->NameEnd, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
			StrBufAppendBuf(Target, ptr->wcs_value, 0);
			break;
		case WCS_SERVCMD:
			pvo_do_cmd(Target, ptr->wcs_value);
			break;
		case WCS_FUNCTION:
			(*ptr->wcs_function) (Target, TP);
			break;
		case WCS_STRBUF:
		case WCS_STRBUF_REF:
			StrBufAppendBuf(Target, ptr->wcs_value, 0);
			break;
		case WCS_LONG:
			StrBufAppendPrintf(Target, "%ld", ptr->lvalue);
			break;
		default:
			LogTemplateError(
                                Target, "Subst", ERR_NAME, TP,
				"WARNING: invalid value in SV-Hash at %s!", TP->Tokens->pName);
		}
	}
	else {
		LogTemplateError(
			Target, "Token", ERR_NAME, TP,
			"didn't find Handler \"%s\"", TP->Tokens->pName);
		wc_backtrace();
	}
}

int CompareSubstToToken(TemplateParam *ParamToCompare, TemplateParam *ParamToLookup)
{
	wcsession *WCC = WC;
	wcsubst *ptr;
	void *vVar;

	if ((WCC->vars!= NULL) && GetHash(WCC->vars, ParamToLookup->Start, 
					  ParamToLookup->len, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
		case WCS_STRBUF:
		case WCS_STRBUF_REF:
			if (ParamToCompare->Type == TYPE_STR)
				return ((ParamToCompare->len == StrLength(ptr->wcs_value)) &&
					(strcmp(ParamToCompare->Start, ChrPtr(ptr->wcs_value)) == 0));
			else
				return ParamToCompare->lvalue == StrTol(ptr->wcs_value);
			break;
		case WCS_SERVCMD:
			return 1; 
			break;
		case WCS_FUNCTION:
			return 1;
		case WCS_LONG:
			if (ParamToCompare->Type == TYPE_STR)
				return 0;
			else 
				return ParamToCompare->lvalue == ptr->lvalue;
			break;
		default:
			lprintf(1,"WARNING: invalid value in SV-Hash at %s!\n", 
				ParamToLookup->Start);
		}
	}
	return 0;
}

int CompareSubstToStrBuf(StrBuf *Compare, TemplateParam *ParamToLookup)
{
	wcsession *WCC = WC;
	wcsubst *ptr;
	void *vVar;

	if ((WCC->vars!= NULL) && GetHash(WCC->vars, ParamToLookup->Start, 
					  ParamToLookup->len, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
		case WCS_STRBUF:
		case WCS_STRBUF_REF:
			return ((StrLength(Compare) == StrLength(ptr->wcs_value)) &&
				(strcmp(ChrPtr(Compare), ChrPtr(ptr->wcs_value)) == 0));
		case WCS_SERVCMD:
			return 1; 
			break;
		case WCS_FUNCTION:
			return 1;
		case WCS_LONG:
			return StrTol(Compare) == ptr->lvalue;
		default:
			lprintf(1,"WARNING: invalid value in SV-Hash at %s!\n", 
				ParamToLookup->Start);
		}
	}
	return 0;
}



/**
 * \brief puts string into the template and computes which escape methon we should use
 * \param Source the string we should put into the template
 * \param FormatTypeIndex where should we look for escape types if?
 */
void StrBufAppendTemplate(StrBuf *Target, 
			  WCTemplputParams *TP,
			  const StrBuf *Source, int FormatTypeIndex)
{
        wcsession *WCC;
	StrBuf *Buf;
	char EscapeAs = ' ';

	if ((FormatTypeIndex < TP->Tokens->nParameters) &&
	    (TP->Tokens->Params[FormatTypeIndex]->Type == TYPE_STR) &&
	    (TP->Tokens->Params[FormatTypeIndex]->len == 1)) {
		EscapeAs = *TP->Tokens->Params[FormatTypeIndex]->Start;
	}

	switch(EscapeAs)
	{
	case 'H':
		WCC = WC;
		Buf = NewStrBufPlain(NULL, StrLength(Source));
		StrBuf_RFC822_to_Utf8(Buf, 
				      Source, 
				      (WCC!=NULL)? WCC->DefaultCharset : NULL, 
				      NULL);
		StrEscAppend(Target, Buf, NULL, 0, 0);
		FreeStrBuf(&Buf);
		break;
	case 'X':
		StrEscAppend(Target, Source, NULL, 0, 0);
		break;
	case 'J':
	  StrECMAEscAppend(Target, Source, NULL);
	  break;
	default:
		StrBufAppendBuf(Target, Source, 0);
	}
}


void PutNewToken(WCTemplate *Template, WCTemplateToken *NewToken)
{
	if (Template->nTokensUsed + 1 >= Template->TokenSpace) {
		if (Template->TokenSpace <= 0) {
			Template->Tokens = (WCTemplateToken**)malloc(
				sizeof(WCTemplateToken*) * 10);
			Template->TokenSpace = 10;
		}
		else {
			WCTemplateToken **NewTokens;
			NewTokens= (WCTemplateToken**)malloc(
				sizeof(WCTemplateToken*) * 
				Template->TokenSpace * 2);
			memcpy(NewTokens, Template->Tokens, 
			       sizeof(WCTemplateToken*) * Template->nTokensUsed);
			free(Template->Tokens);
			Template->TokenSpace *= 2;
			Template->Tokens = NewTokens;
		}
	}
	Template->Tokens[(Template->nTokensUsed)++] = NewToken;
}

TemplateParam *GetNextParameter(StrBuf *Buf, const char **pCh, const char *pe, WCTemplateToken *Tokens, WCTemplate *pTmpl)
{
	const char *pch = *pCh;
	const char *pchs, *pche;
	TemplateParam *Parm = (TemplateParam *) malloc(sizeof(TemplateParam));
	char quote = '\0';
	int ParamBrace = 0;

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
	else if (*pch == ';') {
		Parm->Type = TYPE_PREFINT;
		pch ++;
		if (*pch == '(') {
			pch ++;
			ParamBrace = 1;
		}
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
			lprintf(1, "Error (in '%s' line %ld); "
				"evaluating template param [%s] in Token [%s]\n",
				ChrPtr(pTmpl->FileName),
				Tokens->Line,
				ChrPtr(Tokens->FlatToken),
				*pCh);
			pch ++;
			free(Parm);
			return NULL;
		}
		else {
			StrBufPeek(Buf, pch, -1, '\0');		
			if (LoadTemplates > 1) {			
				lprintf(1, "DBG: got param [%s] %ld %ld\n", 
					pchs, pche - pchs, strlen(pchs));
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
			lprintf(1, "Error (in '%s' line %ld); "
				"evaluating long template param [%s] in Token [%s]\n",
				ChrPtr(pTmpl->FileName),
				Tokens->Line,
				ChrPtr(Tokens->FlatToken),
				*pCh);
				*/
			free(Parm);
			return NULL;
		}
	}
	while ((*pch == ' ' )||
	       (*pch == '\t')||
	       (*pch == '\r')||
	       (*pch == ',' )||
	       (*pch == '\n')) pch ++;

	*pCh = pch;
	return Parm;
}

WCTemplateToken *NewTemplateSubstitute(StrBuf *Buf, 
				       const char *pStart, 
				       const char *pTmplStart, 
				       const char *pTmplEnd, 
				       long Line,
				       WCTemplate *pTmpl)
{
	void *vVar;
	const char *pch;
	TemplateParam *Param;
	WCTemplateToken *NewToken = (WCTemplateToken*)malloc(sizeof(WCTemplateToken));
	WCTemplputParams TP;

	TP.Tokens = NewToken;
	NewToken->FileName = pTmpl->FileName; /* to print meaningfull log messages... */
	NewToken->Flags = 0;
	NewToken->Line = Line + 1;
	NewToken->pTokenStart = pTmplStart;
	NewToken->TokenStart = pTmplStart - pStart;
	NewToken->TokenEnd =  (pTmplEnd - pStart) - NewToken->TokenStart;
	NewToken->pTokenEnd = pTmplEnd;
	NewToken->NameEnd = NewToken->TokenEnd - 2;
	NewToken->PreEval = NULL;
	NewToken->FlatToken = NewStrBufPlain(pTmplStart + 2, pTmplEnd - pTmplStart - 2);
	
	StrBufPeek(Buf, pTmplStart, + 1, '\0');
	StrBufPeek(Buf, pTmplEnd, -1, '\0');
	pch = NewToken->pName = pTmplStart + 2;

	NewToken->HaveParameters = 0;;
	NewToken->nParameters = 0;

	while (pch < pTmplEnd - 1) {
		if (*pch == '(') {
			StrBufPeek(Buf, pch, -1, '\0');
			NewToken->NameEnd = pch - NewToken->pName;
			pch ++;
			if (*(pTmplEnd - 1) != ')') {
				LogTemplateError(
					NULL, "Parseerror", ERR_NAME, &TP, 
					"Warning, Non welformed Token; missing right parenthesis");
			}
			while (pch < pTmplEnd - 1) {
				Param = GetNextParameter(Buf, &pch, pTmplEnd - 1, NewToken, pTmpl);
				if (Param != NULL) {
					NewToken->HaveParameters = 1;
					if (NewToken->nParameters > MAXPARAM) {
						LogTemplateError(
							NULL, "Parseerror", ERR_NAME, &TP,
							"only [%d] Params allowed in Tokens",
							MAXPARAM);

						free(Param);
						FreeToken(&NewToken);
						return NULL;
					}
					NewToken->Params[NewToken->nParameters++] = Param;
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
			}
		}
		break;
	case SV_GETTEXT:
		if (NewToken->nParameters !=1) {
			LogTemplateError(                               
				NULL, "Gettext", ERR_NAME, &TP,
				"requires exactly 1 parameter, you gave %d params", 
				NewToken->nParameters);
			NewToken->Flags = 0;
			break;
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
		break;
	case SV_CUST_STR_CONDITIONAL:
	case SV_CONDITIONAL:
	case SV_NEG_CONDITIONAL:
		if (NewToken->nParameters <2) {
			LogTemplateError(
				NULL, "Conditional", ERR_NAME, &TP,
				"require at least 2 parameters, you gave %d params", 
				NewToken->nParameters);
			NewToken->Flags = 0;
			break;
		}
		if (NewToken->Params[1]->lvalue == 0) {
			LogTemplateError(
				NULL, "Conditional", ERR_NAME, &TP,
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
				NULL, "Conditional", ERR_NAME, &TP,
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
	NewTemplate->Data = NULL;
	NewTemplate->FileName = NewStrBufDup(filename);
	NewTemplate->nTokensUsed = 0;
	NewTemplate->TokenSpace = 0;
	NewTemplate->Tokens = NULL;

	Put(PutThere, ChrPtr(Key), StrLength(Key), NewTemplate, FreeWCTemplate);
	return NewTemplate;
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void *load_template(StrBuf *filename, StrBuf *Key, HashList *PutThere)
{
	int fd;
	struct stat statbuf;
	const char *pS, *pE, *pch, *Err;
	long Line;
	int pos;
	WCTemplate *NewTemplate;

	fd = open(ChrPtr(filename), O_RDONLY);
	if (fd <= 0) {
		lprintf(1, "ERROR: could not open template '%s' - %s\n",
			ChrPtr(filename), strerror(errno));
		return NULL;
	}

	if (fstat(fd, &statbuf) == -1) {
		lprintf(1, "ERROR: could not stat template '%s' - %s\n",
			ChrPtr(filename), strerror(errno));
		return NULL;
	}

	NewTemplate = (WCTemplate *) malloc(sizeof(WCTemplate));
	NewTemplate->Data = NewStrBufPlain(NULL, statbuf.st_size);
	NewTemplate->FileName = NewStrBufDup(filename);
	NewTemplate->nTokensUsed = 0;
	NewTemplate->TokenSpace = 0;
	NewTemplate->Tokens = NULL;
	if (StrBufReadBLOB(NewTemplate->Data, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		FreeWCTemplate(NewTemplate);
		lprintf(1, "ERROR: reading template '%s' - %s<br />\n",
			ChrPtr(filename), strerror(errno));
		return NULL;
	}
	close(fd);

	Line = 0;
	pS = pch = ChrPtr(NewTemplate->Data);
	pE = pS + StrLength(NewTemplate->Data);
	while (pch < pE) {
		const char *pts, *pte;
		int InQuotes = 0;
		int InDoubleQuotes = 0;

		/** Find one <? > */
		pos = (-1);
		for (; pch < pE; pch ++) {
			if ((*pch=='<')&&(*(pch + 1)=='?'))
				break;
			if (*pch=='\n') Line ++;
		}
		if (pch >= pE)
			continue;
		pts = pch;

		/** Found one? parse it. */
		for (; pch <= pE - 1; pch ++) {
			if (*pch == '"')
				InDoubleQuotes = ! InDoubleQuotes;
			else if (*pch == '\'')
				InQuotes = ! InQuotes;
			else if ((!InQuotes  && !InDoubleQuotes) &&
				 ((*pch!='\\')&&(*(pch + 1)=='>'))) {
				pch ++;
				break;
			}
		}
		if (pch + 1 > pE)
			continue;
		pte = pch;
		PutNewToken(NewTemplate, 
			    NewTemplateSubstitute(NewTemplate->Data, pS, pts, pte, Line, NewTemplate));
		pch ++;
	}
	if (LoadTemplates == 0)
		Put(PutThere, ChrPtr(Key), StrLength(Key), NewTemplate, FreeWCTemplate);
	return NewTemplate;
}


const char* PrintTemplate(void *vSubst)
{
	WCTemplate *Tmpl = vSubst;

	return ChrPtr(Tmpl->FileName);

}

int LoadTemplateDir(const char *DirName, HashList *wireless, HashList *big)
{
	StrBuf *FileName;
	StrBuf *Tag;
	StrBuf *Dir;
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	int d_namelen;
	int d_without_ext;
	int IsMobile;
	
	Dir = NewStrBuf();
	StrBufPrintf(Dir, "%s/t", DirName);
	filedir = opendir (ChrPtr(Dir));
	if (filedir == NULL) {
		FreeStrBuf(&Dir);
		return 0;
	}

	FileName = NewStrBuf();
	Tag = NewStrBuf();
	while ((filedir_entry = readdir(filedir)))
	{
		char *MinorPtr;
		char *PStart;
#ifdef _DIRENT_HAVE_D_NAMELEN
		d_namelen = filedir_entry->d_namelen;
#else
		d_namelen = strlen(filedir_entry->d_name);
#endif
		d_without_ext = d_namelen;
		while ((d_without_ext > 0) && (filedir_entry->d_name[d_without_ext] != '.'))
			d_without_ext --;
		if ((d_without_ext == 0) || (d_namelen < 3))
			continue;
		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		IsMobile = (strstr(filedir_entry->d_name, ".m.html")!= NULL);
		PStart = filedir_entry->d_name;
		StrBufPrintf(FileName, "%s/%s", ChrPtr(Dir),  filedir_entry->d_name);
		MinorPtr = strchr(filedir_entry->d_name, '.');
		if (MinorPtr != NULL)
			*MinorPtr = '\0';
		StrBufPlain(Tag, filedir_entry->d_name, MinorPtr - filedir_entry->d_name);

		if (LoadTemplates > 1)
			lprintf(1, "%s %d %s\n",ChrPtr(FileName), IsMobile, ChrPtr(Tag));
		if (LoadTemplates == 0)
			load_template(FileName, Tag, (IsMobile)?wireless:big);
		else
			prepare_template(FileName, Tag, (IsMobile)?wireless:big);
	}
	closedir(filedir);
	FreeStrBuf(&FileName);
	FreeStrBuf(&Tag);
	FreeStrBuf(&Dir);
	return 1;
}

void InitTemplateCache(void)
{
	LoadTemplateDir(static_dirs[0],
			WirelessTemplateCache,
			TemplateCache);
	LoadTemplateDir(static_dirs[1],
			WirelessLocalTemplateCache,
			LocalTemplateCache);
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
int EvaluateToken(StrBuf *Target, int state, WCTemplputParams *TP)
{
	const char *AppendMe;
	long AppendMeLen;
	HashHandler *Handler;
	void *vVar;
	
/* much output, since pName is not terminated...
	lprintf(1,"Doing token: %s\n",Token->pName);
*/

	switch (TP->Tokens->Flags) {
	case SV_GETTEXT:
		TmplGettext(Target, TP);
		break;
	case SV_CONDITIONAL: /** Forward conditional evaluation */
		return EvaluateConditional(Target, 1, state, TP);
		break;
	case SV_NEG_CONDITIONAL: /** Reverse conditional evaluation */
		return EvaluateConditional(Target, 0, state, TP);
		break;
	case SV_CUST_STR_CONDITIONAL: /** Conditional put custom strings from params */
		if (TP->Tokens->nParameters >= 6) {
			if (EvaluateConditional(Target, 0, state, TP)) {
				GetTemplateTokenString(TP, 5, &AppendMe, &AppendMeLen);
				StrBufAppendBufPlain(Target, 
						     AppendMe, 
						     AppendMeLen,
						     0);
			}
			else{
				GetTemplateTokenString(TP, 4, &AppendMe, &AppendMeLen);
				StrBufAppendBufPlain(Target, 
						     AppendMe, 
						     AppendMeLen,
						     0);
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
			return -1;
		}
		Handler->HandlerFunc(Target, TP);
		break;		
	default:
		if (GetHash(GlobalNS, TP->Tokens->pName, TP->Tokens->NameEnd, &vVar)) {
			Handler = (HashHandler*) vVar;
			if (!CheckContext(Target, &Handler->Filter, TP, "Token")) {
				return -1;
			}
			else {
				Handler->HandlerFunc(Target, TP);
			}
		}
		else {
			print_value_of(Target, TP);
		}
	}
	return 0;
}



void ProcessTemplate(WCTemplate *Tmpl, StrBuf *Target, WCTemplputParams *CallingTP)
{
	WCTemplate *pTmpl = Tmpl;
	int done = 0;
	int i, state;
	const char *pData, *pS;
	long len;
	WCTemplputParams TP;

	memcpy(&TP.Filter, &CallingTP->Filter, sizeof(ContextFilter));

	TP.Context = CallingTP->Context;
	TP.ControlContext = CallingTP->ControlContext;

	if (LoadTemplates != 0) {			
		if (LoadTemplates > 1)
			lprintf(1, "DBG: ----- loading:  [%s] ------ \n", 
				ChrPtr(Tmpl->FileName));

		pTmpl = load_template(Tmpl->FileName, NULL, NULL);
		if(pTmpl == NULL) {
			StrBufAppendPrintf(
				Target, 
				"<pre>\nError loading Template [%s]\n See Logfile for details\n</pre>\n", 
				ChrPtr(Tmpl->FileName));
			return;
		}

	}

	pS = pData = ChrPtr(pTmpl->Data);
	len = StrLength(pTmpl->Data);
	i = 0;
	state = 0;
	while (!done) {
		if (i >= pTmpl->nTokensUsed) {
			StrBufAppendBufPlain(Target, 
					     pData, 
					     len - (pData - pS), 0);
			done = 1;
		}
		else {
			StrBufAppendBufPlain(
				Target, pData, 
				pTmpl->Tokens[i]->pTokenStart - pData, 0);
			TP.Tokens = pTmpl->Tokens[i];
			TP.nArgs = pTmpl->Tokens[i]->nParameters;
			state = EvaluateToken(Target, state, &TP);

			while ((state != 0) && (i+1 < pTmpl->nTokensUsed)) {
			/* condition told us to skip till its end condition */
				i++;
				TP.Tokens = pTmpl->Tokens[i];
				TP.nArgs = pTmpl->Tokens[i]->nParameters;
				if ((pTmpl->Tokens[i]->Flags == SV_CONDITIONAL) ||
				    (pTmpl->Tokens[i]->Flags == SV_NEG_CONDITIONAL)) {
					if (state == EvaluateConditional(
						    Target, 
						    pTmpl->Tokens[i]->Flags, 
						    state, 
						    &TP))
						state = 0;
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
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void DoTemplate(const char *templatename, long len, StrBuf *Target, WCTemplputParams *TP) 
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

	if (WC->is_mobile) {
		Static = WirelessTemplateCache;
		StaticLocal = WirelessLocalTemplateCache;
	}
	else {
		Static = TemplateCache;
		StaticLocal = LocalTemplateCache;
	}

	if (len == 0)
	{
		lprintf (1, "Can't to load a template with empty name!\n");
		StrBufAppendPrintf(Target, "<pre>\nCan't to load a template with empty name!\n</pre>");
		return;
	}

	if (!GetHash(StaticLocal, templatename, len, &vTmpl) &&
	    !GetHash(Static, templatename, len, &vTmpl)) {
		lprintf (1, "didn't find Template [%s] %ld %ld\n", templatename, len , (long)strlen(templatename));
		StrBufAppendPrintf(Target, "<pre>\ndidn't find Template [%s] %ld %ld\n</pre>", 
				   templatename, len, 
				   (long)strlen(templatename));
#if 0
		dbg_PrintHash(Static, PrintTemplate, NULL);
		PrintHash(Static, VarPrintTransition, PrintTemplate);
#endif
		return;
	}
	if (vTmpl == NULL) 
		return;
	ProcessTemplate(vTmpl, Target, TP);
}

/*-----------------------------------------------------------------------------
 *                      Iterators
 */
typedef struct _HashIterator {
	HashList *StaticList;
	int AdditionalParams;
	int ContextType;
	int XPectContextType;
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
		      int ContextType, 
		      int XPectContextType, 
		      int Flags)
{
	HashIterator *It = (HashIterator*)malloc(sizeof(HashIterator));
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

/* typedef struct _iteratestruct {
	int GroupChange;
	int oddeven;
	const char *Key;
	long KeyLen;
	int n;
	int LastN;
	}IterateStruct; */

void tmpl_iterate_subtmpl(StrBuf *Target, WCTemplputParams *TP)
{
	void *vIt;
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
	WCTemplputParams SubTP;
	IterateStruct Status;

	memset(&Status, 0, sizeof(IterateStruct));
	memcpy (&SubTP, &TP, sizeof(WCTemplputParams));
	
	if (!GetHash(Iterators, TKEY(0), &vIt)) {
		LogTemplateError(
			Target, "Iterator", ERR_PARM1, TP, "Unknown!");
		return;
	}

	It = (HashIterator*) vIt;

	if (TP->Tokens->nParameters < It->AdditionalParams + 2) {
		LogTemplateError(                               
			Target, "Iterator", ERR_PARM1, TP,
			"doesn't work with %d params", 
			TP->Tokens->nParameters);
		return;
	}

	if ((It->XPectContextType != CTX_NONE) &&
	    (It->XPectContextType != TP->Filter.ContextType)) {
		LogTemplateError(
			Target, "Iterator", ERR_PARM1, TP,
			"requires context of type %d, have %d", 
			It->XPectContextType, 
			TP->Filter.ContextType);
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
				/** Ok, its us, lets see in which direction we should sort... */
				if (havebstr("SortOrder")) {
					int SortOrder;
					SortOrder = LBSTR("SortOrder");
					if (SortOrder != 0)
						DetectGroupChange = 1;
				}
			}
		}
	}
	nMembersUsed = GetCount(List);
	SubBuf = NewStrBuf();
	SubTP.Filter.ContextType = It->ContextType;
	SubTP.Filter.ControlContextType = CTX_ITERATE;
	SubTP.ControlContext = &Status;
	it = GetNewHashPos(List, 0);
	while (GetNextHashPos(List, it, &Status.KeyLen, &Status.Key, &vContext)) {
		if (DetectGroupChange && Status.n > 0) {
			Status.GroupChange = (SortBy->GroupChange(vContext, vLastContext))? 1:0;
		}
		Status.LastN = ++Status.LastN == nMembersUsed;
		SubTP.Context = vContext;
		if (It->DoSubTemplate != NULL)
			It->DoSubTemplate(SubBuf, &SubTP);
		DoTemplate(TKEY(1), SubBuf, &SubTP);
			
		StrBufAppendBuf(Target, SubBuf, 0);
		FlushStrBuf(SubBuf);
		Status.oddeven = ! Status.oddeven;
		vLastContext = vContext;
	}
	FreeStrBuf(&SubBuf);
	DeleteHashPos(&it);
	if (It->Destructor != NULL)
		It->Destructor(&List);
}


int conditional_ITERATE_ISGROUPCHANGE(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CCTX;
	return Ctx->GroupChange;
}

void tmplput_ITERATE_ODDEVEN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CCTX;
	if (Ctx->oddeven)
		StrBufAppendBufPlain(Target, HKEY("odd"), 0);
	else
		StrBufAppendBufPlain(Target, HKEY("even"), 0);
}


void tmplput_ITERATE_KEY(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CCTX;

	StrBufAppendBufPlain(Target, Ctx->Key, Ctx->KeyLen, 0);
}


void tmplput_ITERATE_LASTN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CCTX;
	StrBufAppendPrintf(Target, "%d", Ctx->n);
}

int conditional_ITERATE_LASTN(StrBuf *Target, WCTemplputParams *TP)
{
	IterateStruct *Ctx = CCTX;
	return Ctx->LastN;
}



/*-----------------------------------------------------------------------------
 *                      Conditionals
 */
int EvaluateConditional(StrBuf *Target, int Neg, int state, WCTemplputParams *TP)
{
	ConditionalStruct *Cond;

	if ((TP->Tokens->Params[0]->len == 1) &&
	    (TP->Tokens->Params[0]->Start[0] == 'X'))
		return (state != 0)?TP->Tokens->Params[1]->lvalue:0;
	    
	Cond = (ConditionalStruct *) TP->Tokens->PreEval;
	if (Cond == NULL) {
		LogTemplateError(
			Target, "Conditional", ERR_PARM1, TP,
			"unknown!");
		return 1;
	}

	if (!CheckContext(Target, &Cond->Filter, TP, "Conditional")) {
		return 0;
	}

	if (Cond->CondF(Target, TP) == Neg)
		return TP->Tokens->Params[1]->lvalue;
	return 0;
}

int ConditionalVar(StrBuf *Target, WCTemplputParams *TP)
{
	void *vsubst;
	wcsubst *subst;
	
	if (!GetHash(WC->vars, TKEY(2), &vsubst))
		return 0;
	subst = (wcsubst*) vsubst;
	
	switch(subst->wcs_type) {
	case WCS_FUNCTION:
		return (subst->wcs_function!=NULL);
	case WCS_SERVCMD:
		lprintf(1, "  -> Server [%s]\n", subst->wcs_value);/* TODO */
		return 1;
	case WCS_STRING:
	case WCS_STRBUF:
	case WCS_STRBUF_REF:
		if (TP->Tokens->nParameters < 4)
			return 1;
		return (strcmp(TP->Tokens->Params[3]->Start, ChrPtr(subst->wcs_value)) == 0);
	case WCS_LONG:
		if (TP->Tokens->nParameters < 4)
			return (subst->lvalue != 0);
		return (subst->lvalue == TP->Tokens->Params[3]->lvalue);
	default:
		lprintf(1,"  WARNING: invalid type: [%ld]!\n", subst->wcs_type);
		return -1;
	}
	return 0;
}

void RegisterConditional(const char *Name, long len, 
			 int nParams,
			 WCConditionalFunc CondF, 
			 int ContextRequired)
{
	ConditionalStruct *Cond = (ConditionalStruct*)malloc(sizeof(ConditionalStruct));
	Cond->PlainName = Name;
	Cond->Filter.nMaxArgs = nParams;
	Cond->Filter.nMinArgs = nParams;
	Cond->CondF = CondF;
	Cond->Filter.ContextType = ContextRequired;
	Cond->Filter.ControlContextType = CTX_NONE;
	Put(Conditionals, Name, len, Cond, NULL);
}

void RegisterControlConditional(const char *Name, long len, 
				int nParams,
				WCConditionalFunc CondF, 
				int ControlContextRequired)
{
	ConditionalStruct *Cond = (ConditionalStruct*)malloc(sizeof(ConditionalStruct));
	Cond->PlainName = Name;
	Cond->Filter.nMaxArgs = nParams;
	Cond->Filter.nMinArgs = nParams;
	Cond->CondF = CondF;
	Cond->Filter.ContextType = CTX_NONE;
	Cond->Filter.ControlContextType = ControlContextRequired;
	Put(Conditionals, Name, len, Cond, NULL);
}

/*-----------------------------------------------------------------------------
 *                      Context Strings
 */
void tmplput_ContextString(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, (StrBuf*)CTX, 0);
}
int ConditionalContextStr(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *TokenText = (StrBuf*) CTX;
	const char *CompareToken;
	long len;

	GetTemplateTokenString(TP, 2, &CompareToken, &len);
	return strcmp(ChrPtr(TokenText), CompareToken) == 0;
}

/*-----------------------------------------------------------------------------
 *                      Boxed-API
 */

void tmpl_do_boxed(StrBuf *Target, WCTemplputParams *TP)
{
	WCTemplputParams SubTP;

	StrBuf *Headline;
	if (TP->Tokens->nParameters == 2) {
		if (TP->Tokens->Params[1]->Type == TYPE_STR) {
			Headline = NewStrBuf();
			DoTemplate(TKEY(1), Headline, TP);
		}
		else {
			const char *Ch;
			long len;
			GetTemplateTokenString(TP, 
					       1,
					       &Ch,
					       &len);
			Headline = NewStrBufPlain(Ch, len);
		}
	}
       memcpy (&SubTP, TP, sizeof(WCTemplputParams));
	SubTP.Context = Headline;
	SubTP.Filter.ContextType = CTX_STRBUF;
	DoTemplate(HKEY("beginbox"), Target, &SubTP);
	DoTemplate(TKEY(0), Target, TP);
	DoTemplate(HKEY("endbox"), Target, TP);
	FreeStrBuf(&Headline);
}

/*-----------------------------------------------------------------------------
 *                      Tabbed-API
 */

void tmpl_do_tabbed(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf **TabNames;
	int i, ntabs, nTabs;

	nTabs = ntabs = TP->Tokens->nParameters / 2;
	TabNames = (StrBuf **) malloc(ntabs * sizeof(StrBuf*));

	for (i = 0; i < ntabs; i++) {
		if ((TP->Tokens->Params[i * 2]->Type == TYPE_STR) &&
		    (TP->Tokens->Params[i * 2]->len > 0)) {
			TabNames[i] = NewStrBuf();
			DoTemplate(TKEY(i * 2), TabNames[i], TP);
		}
		else if (TP->Tokens->Params[i * 2]->Type == TYPE_GETTEXT) {
			const char *Ch;
			long len;
			GetTemplateTokenString(TP, 
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

	StrTabbedDialog(Target, nTabs, TabNames);
	for (i = 0; i < ntabs; i++) {
		StrBeginTab(Target, i, nTabs);
		DoTemplate(TKEY(i * 2 + 1), Target, TP);
		StrEndTab(Target, i, nTabs);
	}
}


/*-----------------------------------------------------------------------------
 *                      Sorting-API
 */


void RegisterSortFunc(const char *name, long len, 
		      const char *prepend, long preplen,
		      CompareFunc Forward, 
		      CompareFunc Reverse, 
		      CompareFunc GroupChange, 
		      long ContextType)
{
	SortStruct *NewSort = (SortStruct*) malloc(sizeof(SortStruct));
	NewSort->Name = NewStrBufPlain(name, len);
	if (prepend != NULL)
		NewSort->PrefPrepend = NewStrBufPlain(prepend, preplen);
	else
		NewSort->PrefPrepend = NULL;
	NewSort->Forward = Forward;
	NewSort->Reverse = Reverse;
	NewSort->GroupChange = GroupChange;
	NewSort->ContextType = ContextType;
	Put(SortHash, name, len, NewSort, DestroySortStruct);
}

CompareFunc RetrieveSort(WCTemplputParams *TP, 
			 const char *OtherPrefix, long OtherPrefixLen,
			 const char *Default, long ldefault, long DefaultDirection)
{
	int isdefault = 0;
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
		isdefault = 1;
		if (!GetHash(SortHash, Default, ldefault, &vSortBy) || 
		    (vSortBy == NULL)) {
			LogTemplateError(
				NULL, "Sorting", ERR_PARM1, TP,
				"Illegal default sort: [%s]", Default);
			wc_backtrace();
		}
	}
	SortBy = (SortStruct*)vSortBy;

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
	{HKEY("static/sort_none.gif")},
	{HKEY("static/up_pointer.gif")},
	{HKEY("static/down_pointer.gif")},
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
	long *LongVector = (long*) CTX;

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
	lprintf(1, ChrPtr(Buf));
	FreeStrBuf(&Buf);
}

int ConditionalLongVector(StrBuf *Target, WCTemplputParams *TP)
{
	long *LongVector = (long*) CTX;

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

void 
InitModule_SUBST
(void)
{
	memset(&NoCtx, 0, sizeof(WCTemplputParams));
	RegisterNamespace("SORT:ICON", 1, 2, tmplput_SORT_ICON, CTX_NONE);
	RegisterNamespace("SORT:ORDER", 1, 2, tmplput_SORT_ORDER, CTX_NONE);
	RegisterNamespace("SORT:NEXT", 1, 2, tmplput_SORT_NEXT, CTX_NONE);
	RegisterNamespace("CONTEXTSTR", 0, 1, tmplput_ContextString, CTX_STRBUF);
	RegisterNamespace("ITERATE", 2, 100, tmpl_iterate_subtmpl, CTX_NONE);
	RegisterNamespace("DOBOXED", 1, 2, tmpl_do_boxed, CTX_NONE);
	RegisterNamespace("DOTABBED", 2, 100, tmpl_do_tabbed, CTX_NONE);
	RegisterNamespace("LONGVECTOR", 1, 1, tmplput_long_vector, CTX_LONGVECTOR);
	RegisterConditional(HKEY("COND:SUBST"), 3, ConditionalVar, CTX_NONE);
	RegisterConditional(HKEY("COND:CONTEXTSTR"), 3, ConditionalContextStr, CTX_STRBUF);
	RegisterConditional(HKEY("COND:LONGVECTOR"), 4, ConditionalLongVector, CTX_LONGVECTOR);

	RegisterControlConditional(HKEY("COND:ITERATE:ISGROUPCHANGE"), 2, 
				   conditional_ITERATE_ISGROUPCHANGE, 
				   CTX_ITERATE);
	RegisterControlConditional(HKEY("COND:ITERATE:LASTN"), 2, 
				   conditional_ITERATE_LASTN, 
				   CTX_ITERATE);
	RegisterControlNS(HKEY("ITERATE:ODDEVEN"), 0, 0, tmplput_ITERATE_ODDEVEN, CTX_ITERATE);
	RegisterControlNS(HKEY("ITERATE:KEY"), 0, 0, tmplput_ITERATE_KEY, CTX_ITERATE);
	RegisterControlNS(HKEY("ITERATE:N"), 0, 0, tmplput_ITERATE_LASTN, CTX_ITERATE);
}

/*@}*/
