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

		if (WC->vars->wcs_type == WCS_STRING) {
			free(WC->vars->wcs_value);
		}

		free(WC->vars);
		WC->vars = ptr;
	}
}


/*
 * Add a substitution variable (local to this session)
 */
void svprintf(char *keyname, const char *format,...)
{
	va_list arg_ptr;
	char wbuf[1024];
	struct wcsubst *ptr;

	va_start(arg_ptr, format);
	vsprintf(wbuf, format, arg_ptr);
	va_end(arg_ptr);

	ptr = (struct wcsubst *) malloc(sizeof(struct wcsubst));
	ptr->next = WC->vars;
	ptr->wcs_type = WCS_STRING;
	strcpy(ptr->wcs_key, keyname);
	ptr->wcs_value = malloc(strlen(wbuf)+1);
	strcpy(ptr->wcs_value, wbuf);
	WC->vars = ptr;
}



/*
 * Print the value of a variable
 */
void print_value_of(char *keyname) {
	struct wcsubst *ptr;

	for (ptr = WC->vars; ptr != NULL; ptr = ptr->next) {
		if (!strcasecmp(ptr->wcs_key, keyname)) {
			if (ptr->wcs_type == WCS_STRING) {
				wprintf("%s", ptr->wcs_value);
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
	int i, j, pos;
	int olen;

	strcpy(filename, "static/");
	strcat(filename, templatename);
	
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
		olen = 0;

		for (i=0; i<strlen(inbuf); ++i) {
			if (strncmp(&inbuf[i], "<?", 2)) {
				outbuf[olen] = inbuf[i];
				outbuf[++olen] = 0;
			}
			else {
				pos = (-1);
				for (j=strlen(inbuf); j>=i;  --j)  {
					if (inbuf[j]=='>') pos = j;
				}
				if (pos > 0) {
					strncpy(key, &inbuf[i+2], pos-i-2);
					print_value_of(key);
					olen = strlen(outbuf);
					i = pos;
				}
				else {
					i = i + 2;
				}
			}
		}
		wprintf("%s", outbuf);
	}

	fclose(fp);
}
