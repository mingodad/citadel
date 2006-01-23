/*
 * $Id$
 */
/**
 * \defgroup ManagePrefs Manage user preferences with a little help from the Citadel server.
 *
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/**
 * \brief display preferences dialog
 */
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

	/** Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_getln(buf, sizeof buf);
}

/**
 * \brief Goto the user's configuration room, creating it if necessary.
 * \return 0 on success or nonzero upon failure.
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

/**
 * \brief save the modifications
 */
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

	/** Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_getln(buf, sizeof buf);
}

/**
 * \brief query the actual setting of key in the citadel database
 * \param key config key to query
 * \param value value to the key to get
 * \param value_len length of the value string
 */
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

/**
 * \brief Write a key into the citadel settings database
 * \param key key whichs value is to be modified
 * \param value value to set
 * \param save_to_server really write it????
 */
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




/** 
 * \brief display form for changing your preferences and settings
 */
void display_preferences(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	char ebuf[300];
	char buf[256];
	char calhourformat[16];
	int i;

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<img src=\"static/advanpage2_48x.gif\" ALT=\" \" ALIGN=MIDDLE>");
	wprintf("<SPAN CLASS=\"titlebar\">&nbsp;");
	wprintf(_("Preferences and settings"));
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	/** begin form */
	wprintf("<center>\n"
		"<form name=\"prefform\" action=\"set_preferences\" "
		"method=\"post\">\n"
		"<table border=0 cellspacing=5 cellpadding=5>\n");

	/**
	 * Room list view
	 */
	get_preference("roomlistview", buf, sizeof buf);
	wprintf("<tr><td>");
	wprintf(_("Room list view"));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"folders\"");
	if (!strcasecmp(buf, "folders")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Tree (folders) view"));
	wprintf("<br></input>\n");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"rooms\"");
	if (!strcasecmp(buf, "rooms")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Table (rooms) view"));
	wprintf("<br></input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Calendar hour format
	 */
	get_preference("calhourformat", calhourformat, sizeof calhourformat);
	if (calhourformat[0] == 0) strcpy(calhourformat, "12");
	wprintf("<tr><td>");
	wprintf(_("Calendar hour format"));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"12\"");
	if (!strcasecmp(calhourformat, "12")) wprintf(" checked");
	wprintf(">");
	wprintf(_("12 hour (am/pm)"));
	wprintf("<br></input>\n");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"24\"");
	if (!strcasecmp(calhourformat, "24")) wprintf(" checked");
	wprintf(">");
	wprintf(_("24 hour"));
	wprintf("<br></input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day start time
	 */
	get_preference("daystart", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "8");
	wprintf("<tr><td>");
	wprintf(_("Calendar day view begins at:"));
	wprintf("</td><td>");

	wprintf("<SELECT NAME=\"daystart\" SIZE=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (!strcasecmp(calhourformat, "24")) {
			wprintf("<OPTION %s VALUE=\"%d\">%d:00</OPTION>\n",
				((atoi(buf) == i) ? "SELECTED" : ""),
				i, i
			);
		}
		else {
			wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
				((atoi(buf) == i) ? "SELECTED" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</SELECT>\n");
	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day end time
	 */
	get_preference("dayend", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "17");
	wprintf("<tr><td>");
	wprintf(_("Calendar day view ends at:"));
	wprintf("</td><td>");

	wprintf("<SELECT NAME=\"dayend\" SIZE=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (!strcasecmp(calhourformat, "24")) {
			wprintf("<OPTION %s VALUE=\"%d\">%d:00</OPTION>\n",
				((atoi(buf) == i) ? "SELECTED" : ""),
				i, i
			);
		}
		else {
			wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
				((atoi(buf) == i) ? "SELECTED" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</SELECT>\n");
	wprintf("</td></tr>\n");

	/**
	 * Signature
	 */
	get_preference("use_sig", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "no");
	wprintf("<tr><td>");
	wprintf(_("Attach signature to email messages?"));
	wprintf("</td><td>");

	wprintf("	<script type=\"text/javascript\">					"
		"	function show_or_hide_sigbox() {					"
		"		if ( $F('yes_sig') ) {						"
		"			$('signature_box').style.display = 'inline';		"
		"		}								"
		"		else {								"
		"			$('signature_box').style.display = 'none';		"
		"		}								"
		"	}									"
		"	</script>								"
	);

	wprintf("<input type=\"radio\" id=\"no_sig\" name=\"use_sig\" VALUE=\"no\"");
	if (!strcasecmp(buf, "no")) wprintf(" checked");
	wprintf(" onChange=\"show_or_hide_sigbox();\" >");
	wprintf(_("No signature"));
	wprintf("<br></input>\n");

	wprintf("<input type=\"radio\" id=\"yes_sig\" name=\"use_sig\" VALUE=\"yes\"");
	if (!strcasecmp(buf, "yes")) wprintf(" checked");
	wprintf(" onChange=\"show_or_hide_sigbox();\" >");
	wprintf(_("Use this signature:"));
	wprintf("<div id=\"signature_box\">"
		"<br><textarea name=\"signature\" cols=\"40\" rows=\"5\">"
	);
	get_preference("signature", ebuf, sizeof ebuf);
	euid_unescapize(buf, ebuf);
	escputs(buf);
	wprintf("</textarea>"
		"</div>"
	);

	wprintf("<br></input>\n");

	wprintf("</td></tr>\n");

	wprintf("	<script type=\"text/javascript\">	"
		"	show_or_hide_sigbox();			"
		"	</script>				"
	);


	wprintf("</table>\n"
		"<input type=\"submit\" name=\"change_button\" value=\"%s\">"
		"&nbsp;"
		"<INPUT type=\"submit\" name=\"cancel_button\" value=\"%s\">\n",
		_("Change"),
		_("Cancel")
	);

	wprintf("</form></center>\n");

	/** end form */


	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/**
 * \brief Commit new preferences and settings
 */
void set_preferences(void)
{
	char ebuf[300];

	if (strlen(bstr("change_button")) == 0) {
		safestrncpy(WC->ImportantMessage, 
			_("Cancelled.  No settings were changed."),
			sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}

	/**
	 * Set the last argument to 1 only for the final setting, so
	 * we don't send the prefs file to the server repeatedly
	 */
	set_preference("roomlistview", bstr("roomlistview"), 0);
	set_preference("calhourformat", bstr("calhourformat"), 0);
	set_preference("use_sig", bstr("use_sig"), 0);
	set_preference("daystart", bstr("daystart"), 0);
	set_preference("dayend", bstr("dayend"), 0);

	euid_escapize(ebuf, bstr("signature"));
	set_preference("signature", ebuf, 1);

	display_main_menu();
}


/*@}*/
