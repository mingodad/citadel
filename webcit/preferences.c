/*
 * $Id$
 *
 * Manage user preferences with a little help from the Citadel server.
 *
 */

#include "webcit.h"
#include "webserver.h"



void load_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;

	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return;
	
	serv_puts("MSGS ALL|0|1");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("MSG0 %ld", msgnum);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			while (serv_getln(buf, sizeof buf),
				(strcmp(buf, "text") && strcmp(buf, "000"))) {
			}
			if (!strcmp(buf, "text")) {
				while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
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
	serv_getln(buf, sizeof buf);
}

/*
 * Goto the user's configuration room, creating it if necessary.
 * Returns 0 on success or nonzero upon failure.
 */
int goto_config_room(void) {
	char buf[SIZ];

	serv_printf("GOTO %s", USERCONFIGROOM);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') { /* try to create the config room if not there */
		serv_printf("CRE8 1|%s|4|0", USERCONFIGROOM);
		serv_getln(buf, sizeof buf);
		serv_printf("GOTO %s", USERCONFIGROOM);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') return(1);
	}
	return(0);
}


void save_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;

	if (goto_config_room() != 0) return;	/* oh well. */
	serv_puts("MSGS ALL|0|1");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '8') {
		serv_puts("subj|__ WebCit Preferences __");
		serv_puts("000");
	}
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		msgnum = atol(buf);
	}

	if (msgnum > 0L) {
		serv_printf("DELE %ld", msgnum);
		serv_getln(buf, sizeof buf);
	}

	serv_printf("ENT0 1||0|1|__ WebCit Preferences __|");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		serv_puts(WC->preferences);
		serv_puts("");
		serv_puts("000");
	}

	/* Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_getln(buf, sizeof buf);
}

void get_preference(char *key, char *value, size_t value_len) {
	int num_prefs;
	int i;
	char buf[SIZ];
	char thiskey[SIZ];

	strcpy(value, "");

	num_prefs = num_tokens(WC->preferences, '\n');
	for (i=0; i<num_prefs; ++i) {
		extract_token(buf, WC->preferences, i, '\n', sizeof buf);
		extract_token(thiskey, buf, 0, '|', sizeof thiskey);
		if (!strcasecmp(thiskey, key)) {
			extract_token(value, buf, 1, '|', value_len);
		}
	}
}

void set_preference(char *key, char *value, int save_to_server) {
	int num_prefs;
	int i;
	char buf[SIZ];
	char thiskey[SIZ];
	char *newprefs = NULL;

	num_prefs = num_tokens(WC->preferences, '\n');
	for (i=0; i<num_prefs; ++i) {
		extract_token(buf, WC->preferences, i, '\n', sizeof buf);
		if (num_tokens(buf, '|') == 2) {
			extract_token(thiskey, buf, 0, '|', sizeof thiskey);
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

	if (save_to_server) save_preferences();
}




/* 
 * display form for changing your preferences and settings
 */
void display_preferences(void)
{
	output_headers(1, 1, 2, 0, 0, 0, 0);
	char buf[256];

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<IMG SRC=\"/static/advanpage2_48x.gif\" ALT=\" \" ALIGN=MIDDLE>");
	wprintf("<SPAN CLASS=\"titlebar\">&nbsp;Preferences and settings");
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	/* begin form */
	wprintf("<center>\n"
		"<form name=\"prefform\" action=\"set_preferences\" "
		"method=\"post\">\n"
		"<table border=0 cellspacing=5 cellpadding=5>\n");


	get_preference("roomlistview", buf, sizeof buf);
	wprintf("<tr><td>Room list view</td><td>");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"folders\"");
	if (!strcasecmp(buf, "folders")) wprintf(" checked");
	wprintf(">Tree (folders) view<br></input>\n");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"rooms\"");
	if (!strcasecmp(buf, "rooms")) wprintf(" checked");
	wprintf(">Table (rooms) view<br></input>\n");

	wprintf("</td></tr>\n");


	get_preference("calhourformat", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "12");
	wprintf("<tr><td>Calendar hour format</td><td>");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"12\"");
	if (!strcasecmp(buf, "12")) wprintf(" checked");
	wprintf(">12 hour (am/pm)<br></input>\n");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"24\"");
	if (!strcasecmp(buf, "24")) wprintf(" checked");
	wprintf(">24 hour<br></input>\n");

	wprintf("</td></tr>\n");

	wprintf("</table>\n"
		"<input type=\"submit\" name=\"action\" value=\"Change\">"
		"&nbsp;"
		"<INPUT type=\"submit\" name=\"action\" value=\"Cancel\">\n");

	wprintf("</form></center>\n");

	/* end form */


	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/*
 * Commit new preferences and settings
 */
void set_preferences(void)
{
	if (strcmp(bstr("action"), "Change")) {
		safestrncpy(WC->ImportantMessage, 
			"Cancelled.  No settings were changed.",
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	/* Set the last argument to 1 only for the final setting, so
	 * we don't send the prefs file to the server repeatedly
	 */
	set_preference("roomlistview", bstr("roomlistview"), 0);
	set_preference("calhourformat", bstr("calhourformat"), 1);

	display_main_menu();
}
