/*
 * $Id$
 *
 * Variable substitution type stuff
 *
 */



#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

struct wcsubst *global_subst = NULL;


/*
 * Clear out the list of substitution variables local to this session
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
}


/*
 * Add a substitution variable (local to this session)
 */
void svprintf(char *keyname, int keytype, const char *format,...)
{
	va_list arg_ptr;
	char wbuf[1024];
	struct wcsubst *ptr = NULL;
	struct wcsubst *scan;

	va_start(arg_ptr, format);
	vsprintf(wbuf, format, arg_ptr);
	va_end(arg_ptr);

	/* First scan through to see if we're doing a replacement of
	 * an existing key
	 */
	for (scan=WC->vars; scan!=NULL; scan=scan->next) {
		if (!strcasecmp(scan->wcs_key, keyname)) {
			ptr = scan;
			free(ptr->wcs_value);
		}
	}

	/* Otherwise allocate a new one */
	if (ptr == NULL) {
		ptr = (struct wcsubst *) malloc(sizeof(struct wcsubst));
		ptr->next = WC->vars;
	}

	ptr->wcs_type = keytype;
	strcpy(ptr->wcs_key, keyname);
	ptr->wcs_value = malloc(strlen(wbuf)+1);
	strcpy(ptr->wcs_value, wbuf);
	WC->vars = ptr;
}

/*
 * Add a substitution variable (local to this session) that does a callback
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



/*
 * back end for print_value_of() ... does a server command
 */
void pvo_do_cmd(char *servcmd) {
	char buf[SIZ];

	serv_puts(servcmd);
	serv_gets(buf);

	switch(buf[0]) {
		case '2':
		case '3':
		case '5':
			wprintf("%s\n", &buf[4]);
			break;
		case '1':
			fmout(NULL);
			break;
		case '4':
			wprintf("%s\n", &buf[4]);
			serv_puts("000");
			break;
	}
}



/*
 * Print the value of a variable
 */
void print_value_of(char *keyname) {
	struct wcsubst *ptr;
	void *fcn();

	if (keyname[0] == '=') {
		do_template(&keyname[1]);
	}

	if (!strcasecmp(keyname, "SERV_PID")) {
		wprintf("%d", serv_info.serv_pid);
	}

	else if (!strcasecmp(keyname, "SERV_NODENAME")) {
		wprintf("%s", serv_info.serv_nodename);
	}

	else if (!strcasecmp(keyname, "SERV_HUMANNODE")) {
		wprintf("%s", serv_info.serv_humannode);
	}

	else if (!strcasecmp(keyname, "SERV_FQDN")) {
		wprintf("%s", serv_info.serv_fqdn);
	}

	else if (!strcasecmp(keyname, "SERV_SOFTWARE")) {
		wprintf("%s", serv_info.serv_software);
	}

	else if (!strcasecmp(keyname, "SERV_REV_LEVEL")) {
		wprintf("%d.%02d",
			serv_info.serv_rev_level / 100,
			serv_info.serv_rev_level % 100
		);
	}

	else if (!strcasecmp(keyname, "SERV_BBS_CITY")) {
		wprintf("%s", serv_info.serv_bbs_city);
	}

	/* Page-local variables */
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



/*
 * Display a variable-substituted template
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
		wprintf("<BLINK>ERROR</BLINK> - could not open template ");
		wprintf("'%s' - %s<BR>\n",
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
