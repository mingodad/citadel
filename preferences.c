/*
 * $Id$
 *
 * Manage user preferences with a little help from the Citadel server.
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
#include "webserver.h"



void load_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;

	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_gets(buf);
	if (buf[0] != '2') return;
	
	serv_puts("MSGS ALL|0|1");
	serv_gets(buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("MSG0 %ld", msgnum);
		serv_gets(buf);
		if (buf[0] == '1') {
			while (serv_gets(buf),
				(strcmp(buf, "text") && strcmp(buf, "000"))) {
			}
			if (!strcmp(buf, "text")) {
				while (serv_gets(buf), strcmp(buf, "000")) {
					if (WC->preferences == NULL) {
						WC->preferences = malloc(SIZ);
						strcpy(WC->preferences, "");
					}
					else {
						WC->preferences = realloc(
							WC->preferences,
							strlen(WC->preferences)
							+SIZ
						);
					}
					strcat(WC->preferences, buf);
					strcat(WC->preferences, "\n");
				}
			}
		}
	}

	/* Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_gets(buf);
}

/*
 * Goto the user's configuration room, creating it if necessary.
 * Returns 0 on success or nonzero upon failure.
 */
int goto_config_room(void) {
	char buf[SIZ];

	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_gets(buf);
	if (buf[0] != '2') { /* try to create the config room if not there */
		serv_printf("CRE8 1|%s|4|0", USERCONFIGROOM);
		serv_gets(buf);
		serv_printf("GOTO %s", USERCONFIGROOM);
		serv_gets(buf);
		if (buf[0] != '2') return(1);
	}
	return(0);
}


void save_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;

	if (goto_config_room() != 0) return;	/* oh well. */
	serv_puts("MSGS ALL|0|1");
	serv_gets(buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_gets(buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("DELE %ld", msgnum);
		serv_gets(buf);
	}

	serv_printf("ENT0 1||0|1|__ WebCit Preferences __|");
	serv_gets(buf);
	if (buf[0] == '4') {
		serv_puts(WC->preferences);
		serv_puts("");
		serv_puts("000");
	}

	/* Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_gets(buf);
}

void get_preference(char *key, char *value) {
	int num_prefs;
	int i;
	char buf[SIZ];
	char thiskey[SIZ];

	strcpy(value, "");

	num_prefs = num_tokens(WC->preferences, '\n');
	for (i=0; i<num_prefs; ++i) {
		extract_token(buf, WC->preferences, i, '\n');
		extract_token(thiskey, buf, 0, '|');
		if (!strcasecmp(thiskey, key)) {
			extract_token(value, buf, 1, '|');
		}
	}
}

void set_preference(char *key, char *value) {
	int num_prefs;
	int i;
	char buf[SIZ];
	char thiskey[SIZ];
	char *newprefs = NULL;

	num_prefs = num_tokens(WC->preferences, '\n');
	for (i=0; i<num_prefs; ++i) {
		extract_token(buf, WC->preferences, i, '\n');
		if (num_tokens(buf, '|') == 2) {
			extract_token(thiskey, buf, 0, '|');
			if (strcasecmp(thiskey, key)) {
				if (newprefs == NULL) newprefs = strdup("");
				newprefs = realloc(newprefs,
					strlen(newprefs) + SIZ );
				strcat(newprefs, buf);
				strcat(newprefs, "\n");
			}
		}
	}


	if (newprefs == NULL) newprefs = strdup("");
	newprefs = realloc(newprefs, strlen(newprefs) + SIZ);
	sprintf(&newprefs[strlen(newprefs)], "%s|%s\n", key, value);

	free(WC->preferences);
	WC->preferences = newprefs;

	save_preferences();
}
