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
void print_value_of(char *keyname, size_t keylen) {
	struct wcsession *WCC = WC;
	wcsubst *ptr;
	void *fcn();
	void *vVar;

	/*if (WCC->vars != NULL) PrintHash(WCC->vars, VarPrintTransition, VarPrintEntry);*/
	if (keyname[0] == '=') {
		do_template(&keyname[1]);
	}
	/** Page-local variables */
	if ((WCC->vars!= NULL) && GetHash(WCC->vars, keyname, keylen, &vVar)) {
		ptr = (wcsubst*) vVar;
		switch(ptr->wcs_type) {
		case WCS_STRING:
			wprintf("%s", ptr->wcs_value);
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
	else if (!strcasecmp(keyname, "SERV_PID")) {
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

}

extern char *static_dirs[PATH_MAX];  /**< Disk representation */

/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void do_template(void *templatename) {
	char flat_filename[PATH_MAX];
	char filename[PATH_MAX];
	FILE *fp;
	char inbuf[1024];
	char outbuf[sizeof inbuf];
	char key[sizeof inbuf];
	int i, pos;
	struct stat mystat;

	strcpy(flat_filename, templatename);
	if (WC->is_wap)
		strcat(flat_filename, ".wml");
	else
		strcat(flat_filename, ".html");
	
	strcpy(filename, static_dirs[1]);
	strcat(filename, flat_filename);
	if (stat(filename, &mystat) == -1)
	{
		strcpy(filename, static_dirs[0]);
		strcat(filename, flat_filename);
	}

	fp = fopen(filename, "r");
	if (fp == NULL) {
		wprintf(_("ERROR: could not open template "));
		wprintf("'%s' - %s<br />\n",
			templatename, strerror(errno));
		return;
	}

	strcpy(inbuf, "");

	while (fgets(inbuf, sizeof inbuf, fp) != NULL) {
		int len;

		strcpy(outbuf, "");
		len = strlen(inbuf);
		while (len > 0) {
			pos = (-1);
			for (i=len; i>=0; --i) {
				if ((inbuf[i]=='<')&&(inbuf[i+1]=='?')) pos = i;
			}
			if (pos < 0) {
				wprintf("%s", inbuf);
				strcpy(inbuf, "");
				len = 0;
			}
			else {
				strncpy(outbuf, inbuf, pos);
				outbuf[pos] = 0;
				wprintf("%s", outbuf);
				memmove(inbuf, &inbuf[pos], len - pos +1);
				len -= pos;
				pos = 1;
				for (i=len; i>=0; --i) {
					if (inbuf[i]=='>') pos = i;
				}
				strncpy(key, &inbuf[2], pos-2);
				key[pos-2] = 0;
				print_value_of(key, pos-2);
				pos++;
				memmove(inbuf, &inbuf[pos], len - pos + 1);
				len -= pos;
			}
		}
	}

	fclose(fp);
}



/*@}*/
