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

#include "webcit.h"
#include "webserver.h"

extern char *static_dirs[PATH_MAX];  /**< Disk representation */

HashList *WirelessTemplateCache;
HashList *WirelessLocalTemplateCache;
HashList *TemplateCache;
HashList *LocalTemplateCache;

HashList *GlobalNS;

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

typedef void (*WCHandlerFunc)(int nArgs, WCTemplateToken **Tokens); /*TODO: subset of that */
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
	void *vVar;
// much output, since pName is not terminated...
//	lprintf(1,"Doing token: %s\n",Token->pName);
	if (GetHash(GlobalNS, Token->pName, Token->NameEnd, &vVar)) {
		HashHandler *Handler;
		Handler = (HashHandler*) vVar;
		if ((Token->nParameters < Handler->nMinArgs) || 
		    (Token->nParameters < Handler->nMaxArgs)) {
			lprintf(1, "Handler [%s] doesn't work with %ld params", 
				Token->pName,
				Token->nParameters);
		}
		else {
			Handler->HandlerFunc(Token->nParameters,
					     &Token); /*TODO: subset of that */
		
			
		}
	}
	else {
		print_value_of(Token->pName, Token->NameEnd);
	}
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
		PutNewToken(NewTemplate, NewTemlpateSubstitute(pS, pts, pte));
		pch ++;
	}
	Put(PutThere, ChrPtr(Key), StrLength(Key), NewTemplate, FreeWCTemplate);
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
		printf ("didn't find %s\n", templatename);
		return;
	}
	if (vTmpl == NULL) 
		return;
	ProcessTemplate(vTmpl, WC->WBuf);	
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

void tmplput_serv_ip(int nArgs, WCTemplateToken **Tokens)
{
	wprintf("%d", WC->ctdl_pid);
}

void tmplput_serv_nodename(int nArgs, WCTemplateToken **Tokens)
{
	escputs(serv_info.serv_nodename);
}

void tmplput_serv_humannode(int nArgs, WCTemplateToken **Tokens)
{
	escputs(serv_info.serv_humannode);
}

void tmplput_serv_fqdn(int nArgs, WCTemplateToken **Tokens)
{
	escputs(serv_info.serv_fqdn);
}

void tmmplput_serv_software(int nArgs, WCTemplateToken **Tokens)
{
	escputs(serv_info.serv_software);
}

void tmplput_serv_rev_level(int nArgs, WCTemplateToken **Tokens)
{
	wprintf("%d.%02d",
		serv_info.serv_rev_level / 100,
		serv_info.serv_rev_level % 100);
}

void tmmplput_serv_bbs_city(int nArgs, WCTemplateToken **Tokens)
{
	escputs(serv_info.serv_bbs_city);
}

void tmplput_current_user(int nArgs, WCTemplateToken **Tokens)
{
	escputs(WC->wc_fullname);
}

void tmplput_current_room(int nArgs, WCTemplateToken **Tokens)
{
	escputs(WC->wc_roomname);
}

void 
InitModule_SUBST
(void)
{
	RegisterNS(HKEY("SERV_PID"), 0, 0, tmplput_serv_ip);
	RegisterNS(HKEY("SERV_NODENAME"), 0, 0, tmplput_serv_nodename);
	RegisterNS(HKEY("SERV_HUMANNODE"), 0, 0, tmplput_serv_humannode);
	RegisterNS(HKEY("SERV_FQDN"), 0, 0, tmplput_serv_fqdn);
	RegisterNS(HKEY("SERV_SOFTWARE"), 0, 0, tmmplput_serv_software);
	RegisterNS(HKEY("SERV_REV_LEVEL"), 0, 0, tmplput_serv_rev_level);
	RegisterNS(HKEY("SERV_BBS_CITY"), 0, 0, tmmplput_serv_bbs_city);
	RegisterNS(HKEY("CURRENT_USER"), 0, 0, tmplput_current_user);
	RegisterNS(HKEY("CURRENT_ROOM"), 0, 0, tmplput_current_room);
}

/*@}*/
