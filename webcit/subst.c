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
HashList *Contitionals;

#define SV_GETTEXT 1
#define SV_CONDITIONAL 2
#define SV_NEG_CONDITIONAL 3

typedef struct _WCTemplate {
	StrBuf *Data;
	int nTokensUsed;
	int TokenSpace;
	WCTemplateToken **Tokens;
} WCTemplate;

typedef struct _HashHandler {
	int nMinArgs;
	int nMaxArgs;
	WCHandlerFunc HandlerFunc;
}HashHandler;

void RegisterNS(const char *NSName, long len, int nMinArgs, int nMaxArgs, WCHandlerFunc HandlerFunc)
{
	HashHandler *NewHandler;
	
	NewHandler = (HashHandler*) malloc(sizeof(HashHandler));
	NewHandler->nMinArgs = nMinArgs;
	NewHandler->nMaxArgs = nMaxArgs;
	NewHandler->HandlerFunc = HandlerFunc;	
	Put(GlobalNS, NSName, len, NewHandler, NULL);
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
void clear_substs(struct wcsession *wc) {

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
	struct wcsession *WCC = WC;

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
	struct wcsession *WCC = WC;
	
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
	struct wcsession *WCC = WC;
		
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
	struct wcsession *WCC = WC;

	
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
	struct wcsession *WCC = WC;

	
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
void SVCallback(char *keyname, size_t keylen, var_callback_fptr fcn_ptr)
{
	wcsubst *ptr;
	void *vPtr;
	struct wcsession *WCC = WC;

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
inline void SVCALLBACK(char *keyname, var_callback_fptr fcn_ptr)
{
	SVCallback(keyname, strlen(keyname), fcn_ptr);
}



void SVPUTBuf(const char *keyname, int keylen, StrBuf *Buf, int ref)
{
	wcsubst *ptr;
	void *vPtr;
	struct wcsession *WCC = WC;

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
	ptr->wcs_value = Buf;
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

/**
 * \brief Print the value of a variable
 * \param keyname get a key to print
 */
void print_value_of(StrBuf *Target, const char *keyname, size_t keylen) {
	struct wcsession *WCC = WC;
	wcsubst *ptr;
	void *fcn();
	void *vVar;

	/*if (WCC->vars != NULL) PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (keyname[0] == '=') {
		DoTemplate(keyname+1, keylen - 1, NULL, NULL);
	}

//////TODO: if param[1] == "U" -> urlescape
/// X -> escputs
	/** Page-local variables */
	if ((WCC->vars!= NULL) && GetHash(WCC->vars, keyname, keylen, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
			StrBufAppendBuf(Target, ptr->wcs_value, 0);
			break;
		case WCS_SERVCMD:
			pvo_do_cmd(Target, ptr->wcs_value);
			break;
		case WCS_FUNCTION:
			(*ptr->wcs_function) ();
			break;
		case WCS_STRBUF:
		case WCS_STRBUF_REF:
			StrBufAppendBuf(Target, ptr->wcs_value, 0);
			break;
		case WCS_LONG:
			StrBufAppendPrintf(Target, "%ld", ptr->lvalue);
			break;
		default:
			lprintf(1,"WARNING: invalid value in SV-Hash at %s!", keyname);
		}
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

TemplateParam *GetNextParameter(StrBuf *Buf, const char **pCh, const char *pe)
{
	const char *pch = *pCh;
	const char *pchs, *pche;
	TemplateParam *Parm = (TemplateParam *) malloc(sizeof(TemplateParam));
	char quote = '\0';
	
	/* Skip leading whitespaces */
	while ((*pch == ' ' )||
	       (*pch == '\t')||
	       (*pch == '\r')||
	       (*pch == '\n')) pch ++;
	if (*pch == '"')
		quote = '"';
	else if (*pch == '\'')
		quote = '\'';
	if (quote != '\0') {
		pch ++;
		pchs = pch;
		Parm->Type = TYPE_STR;
		while (pch <= pe &&
		       ((*pch != quote) ||
			( (pch > pchs) && (*(pch - 1) == '\\'))
			       )) {
			pch ++;
		}
		pche = pch;
		if (*pch != quote) {
			lprintf(1, "Error evaluating template param [%s]\n", *pCh);
			pch ++;
			free(Parm);
			return NULL;
		}
		else {
			StrBufPeek(Buf, pch, -1, '\0');		
			lprintf(1, "DBG: got param [%s]\n", pchs);
			Parm->Start = pchs;
			Parm->len = pche - pchs;
			pch ++; /* move after trailing quote */
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
			lprintf(1, "Error evaluating template long param [%s]", *pCh);
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
				       const char *pTmplEnd)
{
	const char *pch;
	TemplateParam *Param;
	WCTemplateToken *NewToken = (WCTemplateToken*)malloc(sizeof(WCTemplateToken));

	NewToken->Flags = 0;
	NewToken->pTokenStart = pTmplStart;
	NewToken->TokenStart = pTmplStart - pStart;
	NewToken->TokenEnd =  (pTmplEnd - pStart) - NewToken->TokenStart;
	NewToken->pTokenEnd = pTmplEnd;
	NewToken->NameEnd = NewToken->TokenEnd - 2;
	
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
			while (pch < pTmplEnd - 1) {
				Param = GetNextParameter(Buf, &pch, pTmplEnd - 1);
				if (Param != NULL) {
					NewToken->HaveParameters = 1;
					if (NewToken->nParameters > MAXPARAM) {
						lprintf(1, "Only %ld Tokens supported!\n", MAXPARAM);
						free(Param);
						return NULL;
					}
					NewToken->Params[NewToken->nParameters++] = Param;
				}
				else break;
			}
			if((NewToken->NameEnd == 1) &&
			   (NewToken->HaveParameters == 1))
			   
			{
				if ((NewToken->nParameters == 1) &&
				    (*(NewToken->pName) == '_'))
					NewToken->Flags = SV_GETTEXT;
				else if ((NewToken->nParameters >= 2) &&
					 (*(NewToken->pName) == '?'))
					NewToken->Flags = SV_CONDITIONAL;
				else if ((NewToken->nParameters >=2) &&
					 (*(NewToken->pName) == '!'))
					NewToken->Flags = SV_NEG_CONDITIONAL;
			}
		}
		else pch ++;		
	}
	return NewToken;
}

void FreeToken(WCTemplateToken **Token)
{
	int i; 
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
	FreeStrBuf(&FreeMe->Data);
	free(FreeMe);
}


int EvaluateConditional(WCTemplateToken *Token, void *Context, int Neg, int state)
{
	void *vConditional;
	ConditionalStruct *Cond;

	if ((Token->Params[0]->len == 1) &&
	    (Token->Params[0]->Start[0] == 'X'))
		return (state != 0)?Token->Params[1]->lvalue:0;

	if (!GetHash(Contitionals, 
		 Token->Params[0]->Start,
		 Token->Params[0]->len,
		 &vConditional)) {
		lprintf(1, "Conditional %s Not found!\n", 
			Token->Params[0]->Start);
	}
	    
	Cond = (ConditionalStruct *) vConditional;

	if (Cond == NULL) {
		lprintf(1, "Conditional %s Not found!\n", 
			Token->Params[0]->Start);
		return 0;
	}
	if (Token->nParameters < Cond->nParams) {
		lprintf(1, "Conditional [%s] needs %ld Params!\n", 
			Token->Params[0]->Start,
			Cond->nParams);
		return 0;
	}
	if (Cond->CondF(Token, Context) == Neg)
		return Token->Params[1]->lvalue;
	return 0;
}

int EvaluateToken(StrBuf *Target, WCTemplateToken *Token, void *Context, int state)
{
	void *vVar;
// much output, since pName is not terminated...
//	lprintf(1,"Doing token: %s\n",Token->pName);
	if (Token->Flags == SV_GETTEXT) {
		TmplGettext(Target, Token->nParameters, Token);
	}
	else if (Token->Flags == SV_CONDITIONAL) {
		return EvaluateConditional(Token, Context, 1, state);
	}
	else if (Token->Flags == SV_NEG_CONDITIONAL) {
		return EvaluateConditional(Token, Context, 0, state);
	}
	else if (GetHash(GlobalNS, Token->pName, Token->NameEnd, &vVar)) {
		HashHandler *Handler;
		Handler = (HashHandler*) vVar;
		if ((Token->nParameters < Handler->nMinArgs) || 
		    (Token->nParameters > Handler->nMaxArgs)) {
			lprintf(1, "Handler [%s] doesn't work with %ld params", 
				Token->pName,
				Token->nParameters);
		}
		else {
			Handler->HandlerFunc(Target, 
					     Token->nParameters,
					     Token,
					     Context); /*TODO: subset of that */
		
			
		}
	}
	else {
		print_value_of(Target, Token->pName, Token->NameEnd);
	}
	return 0;
}

void ProcessTemplate(WCTemplate *Tmpl, StrBuf *Target, void *Context)
{
	int done = 0;
	int i, state;
	const char *pData, *pS;
	long len;

	pS = pData = ChrPtr(Tmpl->Data);
	len = StrLength(Tmpl->Data);
	i = 0;
	state = 0;
	while (!done) {
		if (i >= Tmpl->nTokensUsed) {
			StrBufAppendBufPlain(Target, 
					     pData, 
					     len - (pData - pS), 0);
			done = 1;
		}
		else {
			StrBufAppendBufPlain(
				Target, pData, 
				Tmpl->Tokens[i]->pTokenStart - pData, 0);
			state = EvaluateToken(Target, Tmpl->Tokens[i], Context, state);
			while ((state != 0) && (i+1 < Tmpl->nTokensUsed)) {
			/* condition told us to skip till its end condition */
				i++;
				if ((Tmpl->Tokens[i]->Flags == SV_CONDITIONAL) ||
				    (Tmpl->Tokens[i]->Flags == SV_NEG_CONDITIONAL)) {
					if (state == EvaluateConditional(Tmpl->Tokens[i], 
									 Context, 
									 Tmpl->Tokens[i]->Flags,
									 state))
						state = 0;
				}
			}
			pData = Tmpl->Tokens[i++]->pTokenEnd + 1;
			if (i > Tmpl->nTokensUsed)
				done = 1;
		}
	}
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

	pS = pch = ChrPtr(NewTemplate->Data);
	pE = pS + StrLength(NewTemplate->Data);
	while (pch < pE) {
		const char *pts, *pte;
		int InQuotes = 0;
		int InDoubleQuotes = 0;
		pos = (-1);
		for (; pch < pE; pch ++) {
			if ((*pch=='<')&&(*(pch + 1)=='?'))
				break;
		}
		if (pch >= pE)
			continue;
		pts = pch;
		for (; pch < pE - 1; pch ++) {
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
		if (pch + 1 >= pE)
			continue;
		pte = pch;
		PutNewToken(NewTemplate, 
			    NewTemplateSubstitute(NewTemplate->Data, pS, pts, pte));
		pch ++;
	}
	Put(PutThere, ChrPtr(Key), StrLength(Key), NewTemplate, FreeWCTemplate);
	return NewTemplate;
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void DoTemplate(const char *templatename, long len, void *Context, StrBuf *Target) 
{
	HashList *Static;
	HashList *StaticLocal;
	void *vTmpl;
	
	if (Target == NULL)
		Target = WC->WBuf;
	if (WC->is_mobile) {
		Static = WirelessTemplateCache;
		StaticLocal = WirelessLocalTemplateCache;
	}
	else {
		Static = TemplateCache;
		StaticLocal = LocalTemplateCache;
	}

	if (!GetHash(StaticLocal, templatename, len, &vTmpl) &&
	    !GetHash(Static, templatename, len, &vTmpl)) {
		printf ("didn't find %s\n", templatename);
		return;
	}
	if (vTmpl == NULL) 
		return;
	ProcessTemplate(vTmpl, Target, Context);	
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

		IsMobile = (strstr(filedir_entry->d_name, ".m.html")!= NULL);
		PStart = filedir_entry->d_name;
		StrBufPrintf(FileName, "%s/%s", ChrPtr(Dir),  filedir_entry->d_name);
		MinorPtr = strchr(filedir_entry->d_name, '.');
		if (MinorPtr != NULL)
			*MinorPtr = '\0';
		StrBufPlain(Tag, filedir_entry->d_name, MinorPtr - filedir_entry->d_name);


		printf("%s %d %s\n",ChrPtr(FileName), IsMobile, ChrPtr(Tag));
		load_template(FileName, Tag, (IsMobile)?wireless:big);		
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

void tmplput_serv_ip(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrBufAppendPrintf(Target, "%d", WC->ctdl_pid);
}

void tmplput_serv_nodename(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, serv_info.serv_nodename, 0, 0);
}

void tmplput_serv_humannode(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, serv_info.serv_humannode, 0, 0);
}

void tmplput_serv_fqdn(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, serv_info.serv_fqdn, 0, 0);
}

void tmmplput_serv_software(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, serv_info.serv_software, 0, 0);
}

void tmplput_serv_rev_level(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrBufAppendPrintf(Target, "%d.%02d",
			    serv_info.serv_rev_level / 100,
			    serv_info.serv_rev_level % 100);
}

void tmmplput_serv_bbs_city(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, serv_info.serv_bbs_city, 0, 0);
}

void tmplput_current_user(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, WC->wc_fullname, 0, 0);
}

void tmplput_current_room(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrEscAppend(Target, NULL, WC->wc_roomname, 0, 0);
}


typedef struct _HashIterator {
	HashList *StaticList;
	RetrieveHashlistFunc GetHash;
	HashDestructorFunc Destructor;
	SubTemplFunc DoSubTemplate;
} HashIterator;

void tmpl_iterate_subtmpl(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	void *vIt;
	HashIterator *It;
	HashList *List;
	HashPos  *it;
	long len; 
	const char *Key;
	void *vContext;
	StrBuf *SubBuf;
	int oddeven = 0;
	
	if (!GetHash(Iterators, 
		     Tokens->Params[0]->Start,
		     Tokens->Params[0]->len,
		     &vIt))
		return;
	It = (HashIterator*) vIt;
	if (It->StaticList == NULL)
		List = It->GetHash();
	else
		List = It->StaticList;

	SubBuf = NewStrBuf();
	it = GetNewHashPos();
	while (GetNextHashPos(List, it, &len, &Key, &vContext)) {
		svprintf(HKEY("ITERATE:ODDEVEN"), WCS_STRING, "%s", (oddeven)?"odd":"even");
		It->DoSubTemplate(SubBuf, vContext);
		DoTemplate(Tokens->Params[1]->Start,
			   Tokens->Params[1]->len,
			   vContext, SubBuf);
			
		StrBufAppendBuf(Target, SubBuf, 0);
		FlushStrBuf(SubBuf);
		oddeven = ~ oddeven;
	}
	FreeStrBuf(&SubBuf);
	DeleteHashPos(&it);
	It->Destructor(List);
}

int ConditionalVar(WCTemplateToken *Tokens, void *Context)
{
	void *vsubst;
	wcsubst *subst;
	
	if (!GetHash(WC->vars, 
		     Tokens->Params[2]->Start,
		     Tokens->Params[2]->len,
		     &vsubst))
		return 0;
	subst = (wcsubst*) vsubst;
	switch(subst->wcs_type) {
	case WCS_FUNCTION:
		return (subst->wcs_function!=NULL);
	case WCS_SERVCMD:
		lprintf(1, "  -> Server [%s]\n", subst->wcs_value);////todo
		return 1;
	case WCS_STRING:
	case WCS_STRBUF:
	case WCS_STRBUF_REF:
		if (Tokens->nParameters < 4)
			return 1;
		return (strcmp(Tokens->Params[3]->Start, ChrPtr(subst->wcs_value)) == 0);
	case WCS_LONG:
		if (Tokens->nParameters < 4)
			return (subst->lvalue != 0);
		return (subst->lvalue == Tokens->Params[3]->lvalue);
	default:
		lprintf(1,"  WARNING: invalid type: [%ld]!\n", subst->wcs_type);
	}
	return 0;
}

void RegisterITERATOR(const char *Name, long len, 
		      HashList *StaticList, 
		      RetrieveHashlistFunc GetHash, 
		      SubTemplFunc DoSubTempl,
		      HashDestructorFunc Destructor)
{
	HashIterator *It = (HashIterator*)malloc(sizeof(HashIterator));
	It->StaticList = StaticList;
	It->GetHash = GetHash;
	It->DoSubTemplate = DoSubTempl;
	It->Destructor = Destructor;
	Put(Iterators, Name, len, It, NULL);
}

void RegisterConditional(const char *Name, long len, 
			 int nParams,
			 WCConditionalFunc CondF)
{
	ConditionalStruct *Cond = (ConditionalStruct*)malloc(sizeof(ConditionalStruct));
	Cond->nParams = nParams;
	Cond->CondF = CondF;
	Put(Contitionals, Name, len, Cond, NULL);
}

void tmpl_do_boxed(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	if (nArgs == 2) {
		StrBuf *Headline = NewStrBuf();
		DoTemplate(Tokens->Params[1]->Start, 
			   Tokens->Params[1]->len,
			   Context, 
			   Headline);
		SVPutBuf("BOXTITLE", Headline, 0);
	}

	DoTemplate(HKEY("beginbox"), Context, Target);
	DoTemplate(Tokens->Params[0]->Start, 
		   Tokens->Params[0]->len,
		   Context, 
		   Target);
	DoTemplate(HKEY("endbox"), Context, Target);
}

void tmpl_do_tabbed(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context)
{
	StrBuf **TabNames;
	int i, ntabs;

	ntabs = Tokens->nParameters / 2;
	TabNames = (StrBuf **) malloc(ntabs * sizeof(StrBuf*));

	for (i = 0; i < ntabs; i++) {
		TabNames[i] = NewStrBuf();
		DoTemplate(Tokens->Params[i * 2]->Start, 
			   Tokens->Params[0]->len,
			   Context,
			   TabNames[i]);
	}

	StrTabbedDialog(Target, ntabs, TabNames);
	for (i = 0; i < ntabs; i++) {
		StrBeginTab(Target, ntabs, i);

		DoTemplate(Tokens->Params[i * 2 + 1]->Start, 
			   Tokens->Params[i * 2 + 1]->len,
			   Context, 
			   Target);
		StrEndTab(Target, ntabs, i);
	}
}

void 
InitModule_SUBST
(void)
{
	RegisterNamespace("SERV_PID", 0, 0, tmplput_serv_ip);
	RegisterNamespace("SERV_NODENAME", 0, 0, tmplput_serv_nodename);
	RegisterNamespace("SERV_HUMANNODE", 0, 0, tmplput_serv_humannode);
	RegisterNamespace("SERV_FQDN", 0, 0, tmplput_serv_fqdn);
	RegisterNamespace("SERV_SOFTWARE", 0, 0, tmmplput_serv_software);
	RegisterNamespace("SERV_REV_LEVEL", 0, 0, tmplput_serv_rev_level);
	RegisterNamespace("SERV_BBS_CITY", 0, 0, tmmplput_serv_bbs_city);
	RegisterNamespace("CURRENT_USER", 0, 0, tmplput_current_user);
	RegisterNamespace("CURRENT_ROOM", 0, 0, tmplput_current_room);
	RegisterNamespace("ITERATE", 2, 4, tmpl_iterate_subtmpl);
	RegisterNamespace("DOBOXED", 1, 2, tmpl_do_boxed);
	RegisterNamespace("DOTABBED", 2, 100, tmpl_do_tabbed);
	RegisterConditional(HKEY("COND:SUBST"), 3, ConditionalVar);
}

/*@}*/
