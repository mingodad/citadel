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

#include "webcit.h"
#include "webserver.h"

HashList *WirelessTemplateCache;
HashList *WirelessLocalTemplateCache;
HashList *TemplateCache;
HashList *LocalTemplateCache;

typedef struct _TemplateToken {
	const char *pTokenStart;
	size_t TokenStart;
	size_t TokenEnd;
	const char *pTokenEnd;

	const char *pName;
	size_t NameEnd;

	int HaveParameters;
	int nParameters;
	size_t ParamStart [10];
	size_t ParamEnd [10];
} WCTemplateToken;

typedef struct _WCTemplate {
	StrBuf *Data;
	int nTokensUsed;
	int TokenSpace;
	WCTemplateToken **Tokens;
} WCTemplate;

HashList *GlobalNS;

typedef struct _HashHandler {
	int foo;
}HashHandler;

typedef int (*HandlerFunc)(int nArgs, va_list vaarg);
void RegisterNS(const char *NSName, int nMinArgs, int nMaxArgs, HandlerFunc Handler)
{

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
		lprintf(1, "  -> %s\n", ptr->wcs_value);
		break;
	case WCS_SERVCMD:
		lprintf(1, "  -> Server [%s]\n", ptr->wcs_value);
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

/**
 * \brief destructor; kill one entry.
 */
void deletevar(void *data)
{
	wcsubst *ptr = (wcsubst*)data;
//		if ((wc->vars->wcs_type == WCS_STRING)
//		   || (wc->vars->wcs_type == WCS_SERVCMD)) {
	if (ptr->wcs_type != WCS_FUNCTION)
		free(ptr->wcs_value);
	free(ptr);	
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
	char wbuf[SIZ];
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
		if (ptr->wcs_value != NULL)
			free(ptr->wcs_value);
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = (wcsubst *) malloc(sizeof(wcsubst));
		safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
		Put(WCC->vars, keyname, keylen, ptr,  deletevar);
	}

	/** Format the string and save it */

	va_start(arg_ptr, format);
	vsnprintf(wbuf, sizeof wbuf, format, arg_ptr);
	va_end(arg_ptr);

	ptr->wcs_function = NULL;
	ptr->wcs_type = keytype;
	ptr->wcs_value = strdup(wbuf);
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
	char wbuf[SIZ];
	void *vPtr;
	wcsubst *ptr = NULL;
	struct wcsession *WCC = WC;
	size_t len;
	
	/**
	 * First look if we're doing a replacement of
	 * an existing key
	 */
	/*PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (GetHash(WCC->vars, keyname, keylen, &vPtr)) {
		ptr = (wcsubst*)vPtr;
		if (ptr->wcs_value != NULL)
			free(ptr->wcs_value);
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = (wcsubst *) malloc(sizeof(wcsubst));
		safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
		Put(WCC->vars, keyname, keylen, ptr,  deletevar);
	}

	/** Format the string and save it */

	va_start(arg_ptr, format);
	len = vsnprintf(wbuf, sizeof wbuf, format, arg_ptr);
	va_end(arg_ptr);

	ptr->wcs_value = (char*) malloc(len + 1);
	memcpy(ptr->wcs_value, wbuf, len + 1);
	ptr->wcs_function = NULL;
	ptr->wcs_type = keytype;
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
		if (ptr->wcs_value != NULL)
			free(ptr->wcs_value);
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = (wcsubst *) malloc(sizeof(wcsubst));
		safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
		Put(WCC->vars, keyname, keylen, ptr,  deletevar);
	}

	ptr->wcs_function = NULL;
	ptr->wcs_type = keytype;
	ptr->wcs_value = strdup(Data);
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
		if (ptr->wcs_value != NULL)
			free(ptr->wcs_value);
	}
	else 	/** Otherwise allocate a new one */
	{
		ptr = (wcsubst *) malloc(sizeof(wcsubst));
		safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
		Put(WCC->vars, keyname, keylen, ptr,  deletevar);
	}

	ptr->wcs_value = NULL;
	ptr->wcs_type = WCS_FUNCTION;
	ptr->wcs_function = fcn_ptr;
}
inline void SVCALLBACK(char *keyname, var_callback_fptr fcn_ptr)
{
	SVCallback(keyname, strlen(keyname), fcn_ptr);
}



/**
 * \brief back end for print_value_of() ... does a server command
 * \param servcmd server command to execute on the citadel server
 */
void pvo_do_cmd(char *servcmd) {
	char buf[SIZ];

	serv_puts(servcmd);
	serv_getln(buf, sizeof buf);

	switch(buf[0]) {
		case '2':
		case '3':
		case '5':
			wprintf("%s\n", &buf[4]);
			break;
		case '1':
			fmout("CENTER");
			break;
		case '4':
			wprintf("%s\n", &buf[4]);
			serv_puts("000");
			break;
	}
}

/**
 * \brief Print the value of a variable
 * \param keyname get a key to print
 */
void print_value_of(const char *keyname, size_t keylen) {
	struct wcsession *WCC = WC;
	wcsubst *ptr;
	void *fcn();
	void *vVar;

	/*if (WCC->vars != NULL) PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (keyname[0] == '=') {
		DoTemplate(keyname+1, keylen - 1);
	}
	/** Page-local variables */
	if ((WCC->vars!= NULL) && GetHash(WCC->vars, keyname, keylen, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
			wprintf("%s", (const char*)ptr->wcs_value);
			break;
		case WCS_SERVCMD:
			pvo_do_cmd(ptr->wcs_value);
			break;
		case WCS_FUNCTION:
			(*ptr->wcs_function) ();
			break;
		default:
			lprintf(1,"WARNING: invalid value in SV-Hash at %s!", keyname);
		}
	}
	else {
		char ch;
		ch = keyname[keylen];
		((char*) keyname)[keylen] = '\0'; ////TODO
		if (!strcasecmp(keyname, "SERV_PID")) {
			wprintf("%d", WCC->ctdl_pid);
		}

		else if (!strcasecmp(keyname, "SERV_NODENAME")) {
			escputs(serv_info.serv_nodename);
		}

		else if (!strcasecmp(keyname, "SERV_HUMANNODE")) {
			escputs(serv_info.serv_humannode);
		}

		else if (!strcasecmp(keyname, "SERV_FQDN")) {
			escputs(serv_info.serv_fqdn);
		}

		else if (!strcasecmp(keyname, "SERV_SOFTWARE")) {
			escputs(serv_info.serv_software);
		}

		else if (!strcasecmp(keyname, "SERV_REV_LEVEL")) {
			wprintf("%d.%02d",
				serv_info.serv_rev_level / 100,
				serv_info.serv_rev_level % 100
				);
		}

		else if (!strcasecmp(keyname, "SERV_BBS_CITY")) {
			escputs(serv_info.serv_bbs_city);
		}

		else if (!strcasecmp(keyname, "CURRENT_USER")) {
			escputs(WCC->wc_fullname);
		}

		else if (!strcasecmp(keyname, "CURRENT_ROOM")) {
			escputs(WCC->wc_roomname);
		}
		((char*) keyname)[keylen] = ch;//// TODO
		
	}

}

extern char *static_dirs[PATH_MAX];  /**< Disk representation */


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
			       sizeof(WCTemplateToken) * Template->nTokensUsed);
			free(Template->Tokens);
			Template->TokenSpace *= 2;
			Template->Tokens = NewTokens;
		}
		

	}
	Template->Tokens[(Template->nTokensUsed)++] = NewToken;
}

WCTemplateToken *NewTemlpateSubstitute(const char *pStart, const char *pTmplStart, const char *pTmplEnd)
{
	WCTemplateToken *NewToken = (WCTemplateToken*)malloc(sizeof(WCTemplateToken));

	NewToken->pTokenStart = pTmplStart;
	NewToken->TokenStart = pTmplStart - pStart;
	NewToken->TokenEnd =  (pTmplEnd - pStart) - NewToken->TokenStart;
	NewToken->pTokenEnd = pTmplEnd;
	
	NewToken->pName = pTmplStart + 2;
	NewToken->NameEnd = NewToken->TokenEnd - 2;
	NewToken->HaveParameters = 0;;
	NewToken->nParameters = 0;
	NewToken->ParamStart[0] = 0;
	NewToken->ParamEnd[0] = 0;
	return NewToken;
}

void FreeWCTemplate(void *vFreeMe)
{
	int i;
	WCTemplate *FreeMe = (WCTemplate*)vFreeMe;

	if (FreeMe->TokenSpace > 0) {
		for (i = 0; i < FreeMe->nTokensUsed; i ++) {
			free(FreeMe->Tokens[i]);
		}
		free(FreeMe->Tokens);
	}
	free(FreeMe);
}

void EvaluateToken(StrBuf *Target, WCTemplateToken *Token)
{


	
	print_value_of(Token->pName, Token->NameEnd);
}


void ProcessTemplate(WCTemplate *Tmpl, StrBuf *Target)
{
	int done = 0;
	int i;
	const char *pData, *pS;
	long len;

	pS = pData = ChrPtr(Tmpl->Data);
	len = StrLength(Tmpl->Data);
	i = 0;
	while (!done) {
		if (i >= Tmpl->nTokensUsed) {
			StrBufAppendBufPlain(Target, pData, len, 0);
			done = 1;
		}
		else {
			StrBufAppendBufPlain(
				Target, pData, 
				Tmpl->Tokens[i]->pTokenStart - pData, 0);
			EvaluateToken(Target, Tmpl->Tokens[i]);
			pData = Tmpl->Tokens[i++]->pTokenEnd + 1;
		}
	}

}



/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void *load_template(const char *templatename, long len) {
	HashList *pCache;
	HashList *Static;
	HashList *StaticLocal;
	StrBuf *flat_filename;
	char filename[PATH_MAX];
	int fd;
	struct stat statbuf;
	const char *pS, *pE, *pch, *Err;
	int pos;
	struct stat mystat;
	WCTemplate *NewTemplate;

	flat_filename = NewStrBufPlain(templatename, len);
	if (WC->is_mobile) {
		Static = WirelessTemplateCache;
		StaticLocal = WirelessLocalTemplateCache;
		StrBufAppendBufPlain(flat_filename, HKEY(".m.html"), 0);
	}
	else {
		Static = TemplateCache;
		StaticLocal = LocalTemplateCache;
		StrBufAppendBufPlain(flat_filename, HKEY(".html"), 0);
	}
	
	strcpy(filename, static_dirs[1]);
	strcat(filename, ChrPtr(flat_filename));
	pCache = StaticLocal;
	if (stat(filename, &mystat) == -1)
	{
		pCache = Static;
		strcpy(filename, static_dirs[0]);
		strcat(filename, ChrPtr(flat_filename));
	}

	fd = open(filename, O_RDONLY);
	if (fd <= 0) {
		wprintf(_("ERROR: could not open template "));
		wprintf("'%s' - %s<br />\n",
			(const char*)templatename, strerror(errno));
		return NULL;
	}

	if (fstat(fd, &statbuf) == -1) {
		wprintf(_("ERROR: could not stat template "));
		wprintf("'%s' - %s<br />\n",
			(const char*)templatename, strerror(errno));
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
		wprintf(_("ERROR: reading template "));
		wprintf("'%s' - %s<br />\n",
			(const char*)templatename, strerror(errno));
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
		PutNewToken(NewTemplate, NewTemlpateSubstitute(pS, pts, pte));
		pch ++;
	}
	Put(pCache, filename, strlen(filename), NewTemplate, FreeWCTemplate);
	return NewTemplate;
}

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void DoTemplate(const char *templatename, long len) 
{
	HashList *Static;
	HashList *StaticLocal;
	void *vTmpl;

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
		vTmpl = load_template(templatename, len); 
		//////TODO: lock this!
	}
	if (vTmpl == NULL) 
		return;
	ProcessTemplate(vTmpl, WC->WBuf);
	
}

void 
InitModule_SUBST
(void)
{

}

/*@}*/
