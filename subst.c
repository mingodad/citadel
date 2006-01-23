/*
 * $Id$
 */
/**
 * \defgroup Subst Variable substitution type stuff
 *
 */

/*@{*/

#include "webcit.h"


/**
 * \brief Clear out the list of substitution variables local to this session
 */
void clear_local_substs(void) {
	struct wcsubst *ptr;

	while (WC->vars != NULL) {
		ptr = WC->vars->next;

		if ((WC->vars->wcs_type == WCS_STRING)
		   || (WC->vars->wcs_type == WCS_SERVCMD)) {
			free(WC->vars->wcs_value);
		}

		free(WC->vars);
		WC->vars = ptr;
	}

	WC->vars = NULL;
}


/*
 * \brief Add a substitution variable (local to this session)
 * \param keyname the replacementstring to substitute
 * \param keytype the kind of the key
 * \param format the format string ala printf
 * \param ... the arguments to substitute in the formatstring
 */
void svprintf(char *keyname, int keytype, const char *format,...)
{
	va_list arg_ptr;
	char wbuf[SIZ];
	struct wcsubst *ptr = NULL;
	struct wcsubst *scan;

	/**
	 * First scan through to see if we're doing a replacement of
	 * an existing key
	 */
	for (scan=WC->vars; scan!=NULL; scan=scan->next) {
		if (!strcasecmp(scan->wcs_key, keyname)) {
			ptr = scan;
			free(ptr->wcs_value);
		}
	}

	/** Otherwise allocate a new one */
	if (ptr == NULL) {
		ptr = (struct wcsubst *) malloc(sizeof(struct wcsubst));
		ptr->next = WC->vars;
		safestrncpy(ptr->wcs_key, keyname, sizeof ptr->wcs_key);
		WC->vars = ptr;
	}

	/** Format the string and save it */

	va_start(arg_ptr, format);
	vsnprintf(wbuf, sizeof wbuf, format, arg_ptr);
	va_end(arg_ptr);

	ptr->wcs_type = keytype;
	ptr->wcs_value = strdup(wbuf);
}

/**
 * \brief Add a substitution variable (local to this session) that does a callback
 * \param keyname the keystring to substitute
 * \param fcn_ptr the function callback to give the substitution string
 */
void svcallback(char *keyname, void (*fcn_ptr)() )
{
	struct wcsubst *ptr;

	ptr = (struct wcsubst *) malloc(sizeof(struct wcsubst));
	ptr->next = WC->vars;
	ptr->wcs_type = WCS_FUNCTION;
	strcpy(ptr->wcs_key, keyname);
	ptr->wcs_function = fcn_ptr;
	WC->vars = ptr;
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
void print_value_of(char *keyname) {
	struct wcsubst *ptr;
	void *fcn();

	if (keyname[0] == '=') {
		do_template(&keyname[1]);
	}

	if (!strcasecmp(keyname, "SERV_PID")) {
		wprintf("%d", WC->ctdl_pid);
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
		escputs(WC->wc_fullname);
	}

	else if (!strcasecmp(keyname, "CURRENT_ROOM")) {
		escputs(WC->wc_roomname);
	}

	/** Page-local variables */
	else for (ptr = WC->vars; ptr != NULL; ptr = ptr->next) {
		if (!strcasecmp(ptr->wcs_key, keyname)) {
			if (ptr->wcs_type == WCS_STRING) {
				wprintf("%s", ptr->wcs_value);
			}
			else if (ptr->wcs_type == WCS_SERVCMD) {
				pvo_do_cmd(ptr->wcs_value);
			}
			else if (ptr->wcs_type == WCS_FUNCTION) {
				(*ptr->wcs_function) ();
			}
		}
	}
}



/**
 * \brief Display a variable-substituted template
 * \param templatename template file to load
 */
void do_template(void *templatename) {
	char filename[PATH_MAX];
	FILE *fp;
	char inbuf[1024];
	char outbuf[sizeof inbuf];
	char key[sizeof inbuf];
	int i, pos;

	strcpy(filename, "static/");
	strcat(filename, templatename);
	if (WC->is_wap)
		strcat(filename, ".wml");
	else
		strcat(filename, ".html");
	
	fp = fopen(filename, "r");
	if (fp == NULL) {
		wprintf(_("ERROR: could not open template "));
		wprintf("'%s' - %s<br />\n",
			templatename, strerror(errno));
		return;
	}

	strcpy(inbuf, "");

	while (fgets(inbuf, sizeof inbuf, fp) != NULL) {
		strcpy(outbuf, "");

		while (strlen(inbuf) > 0) {
			pos = (-1);
			for (i=strlen(inbuf); i>=0; --i) {
				if ((inbuf[i]=='<')&&(inbuf[i+1]=='?')) pos = i;
			}
			if (pos < 0) {
				wprintf("%s", inbuf);
				strcpy(inbuf, "");
			}
			else {
				strncpy(outbuf, inbuf, pos);
				outbuf[pos] = 0;
				wprintf("%s", outbuf);
				strcpy(inbuf, &inbuf[pos]);
				pos = 1;
				for (i=strlen(inbuf); i>=0; --i) {
					if (inbuf[i]=='>') pos = i;
				}
				strncpy(key, &inbuf[2], pos-2);
				key[pos-2] = 0;
				print_value_of(key);
				strcpy(inbuf, &inbuf[pos+1]);
			}
		}
	}

	fclose(fp);
}



/*@}*/
