/*
 * $Id$
 *
 * Manage user preferences with a little help from the Citadel server.
 *
 */

#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"

inline const char *PrintPref(void *Prefstr)
{
	return Prefstr;
}

/*
 * display preferences dialog
 */
void load_preferences(void) {
	char buf[SIZ];
	long msgnum = 0L;
	char key[SIZ], value[SIZ];
	
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
					extract_token(key, buf, 0, '|', sizeof key);
					extract_token(value, buf, 1, '|', sizeof value);
					if (!IsEmptyStr(key))
						Put(WC->hash_prefs, key, strlen(key), strdup(value), free);
				}
			}
		}
	}

	/* Go back to the room we're supposed to be in */
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
		long len;
		HashPos *HashPos;
		HashList *Hash;
		char *Value;
		char *Key;
		
		Hash = WC->hash_prefs;
		PrintHash(Hash, PrintPref, NULL);
		HashPos = GetNewHashPos();
		while (GetNextHashPos(Hash, HashPos, &len, &Key, (void**)&Value)!=0)
		{
			serv_printf("%s|%s", Key, Value);
		}
		serv_puts("");
		serv_puts("000");
		DeleteHashPos(&HashPos);
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
	void *hash_value = NULL;
	
	strcpy(value, "");
	PrintHash(WC->hash_prefs, PrintPref, NULL);
	if (GetHash(WC->hash_prefs, key, strlen(key), &hash_value) == 0)
		return;
	
	if(hash_value)
		safestrncpy(value, hash_value, value_len);
}

/**
 * \brief	Write a key into the webcit preferences database for this user
 *
 * \params	key		key whichs value is to be modified
 * \param	value		value to set
 * \param	save_to_server	1 = flush all data to the server, 0 = cache it for now
 */
void set_preference(char *key, char *value, int save_to_server) {
	
	Put(WC->hash_prefs, key, strlen(key), strdup(value), free);
	
	if (save_to_server) save_preferences();
}




/** 
 * \brief display form for changing your preferences and settings
 */
void display_preferences(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	char ebuf[300];
	char buf[256];
	int i;
	int time_format;
	time_t tt;
	struct tm tm;
	char daylabel[32];
	
	time_format = get_time_format_cached ();

        wprintf("<div class=\"box\">\n");
        wprintf("<div class=\"boxlabel\">");
        wprintf(_("Preferences and settings"));
        wprintf("</div>");

        wprintf("<div class=\"boxcontent\">");

	/** begin form */
	wprintf("<form name=\"prefform\" action=\"set_preferences\" "
		"method=\"post\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);

	/** begin table */
        wprintf("<table class=\"altern\">\n");

	/**
	 * Room list view
	 */
	get_preference("roomlistview", buf, sizeof buf);
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Room list view"));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"folders\"");
	if (!strcasecmp(buf, "folders")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Tree (folders) view"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"roomlistview\" VALUE=\"rooms\"");
	if (!strcasecmp(buf, "rooms")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Table (rooms) view"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Time hour format
	 */

	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Time format"));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"12\"");
	if (time_format == WC_TIMEFORMAT_AMPM) 
		wprintf(" checked");
	wprintf(">");
	wprintf(_("12 hour (am/pm)"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"calhourformat\" VALUE=\"24\"");
	if (time_format == WC_TIMEFORMAT_24)
		wprintf(" checked");
	wprintf(">");
	wprintf(_("24 hour"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day start time
	 */
	get_preference("daystart", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "8");
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Calendar day view begins at:"));
	wprintf("</td><td>");

	wprintf("<select name=\"daystart\" size=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("<option %s value=\"%d\">%d:00</option>\n",
				((atoi(buf) == i) ? "selected" : ""),
				i, i
			);
		}
		else {
			wprintf("<option %s value=\"%d\">%s</option>\n",
				((atoi(buf) == i) ? "selected" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Calendar day view -- day end time
	 */
	get_preference("dayend", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "17");
	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Calendar day view ends at:"));
	wprintf("</td><td>");

	wprintf("<select name=\"dayend\" size=\"1\">\n");
	for (i=0; i<=23; ++i) {

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("<option %s value=\"%d\">%d:00</option>\n",
				((atoi(buf) == i) ? "selected" : ""),
				i, i
			);
		}
		else {
			wprintf("<option %s value=\"%d\">%s</option>\n",
				((atoi(buf) == i) ? "selected" : ""),
				i, hourname[i]
			);
		}

	}
	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Day of week to begin calendar month view
	 */
	get_preference("weekstart", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "17");
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Week starts on:"));
	wprintf("</td><td>");

	wprintf("<select name=\"weekstart\" size=\"1\">\n");

	for (i=0; i<=1; ++i) {
                tt = time(NULL);
                localtime_r(&tt, &tm);
		tm.tm_wday = i;
                wc_strftime(daylabel, sizeof daylabel, "%A", &tm);

		wprintf("<option %s value=\"%d\">%s</option>\n",
			((atoi(buf) == i) ? "selected" : ""),
			i, daylabel
		);
	}

	wprintf("</select>\n");
	wprintf("</td></tr>\n");

	/**
	 * Signature
	 */
	get_preference("use_sig", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "no");
	wprintf("<tr class=\"odd\"><td>");
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
	wprintf("</input>&nbsp,&nbsp;&nbsp;\n");

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

	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	wprintf("	<script type=\"text/javascript\">	"
		"	show_or_hide_sigbox();			"
		"	</script>				"
	);

	/** Character set to assume is in use for improperly encoded headers */
	get_preference("default_header_charset", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "UTF-8");
	wprintf("<tr class=\"even\"><td>");
	wprintf(_("Default character set for email headers:"));
	wprintf("</td><td>");
	wprintf("<input type=\"text\" NAME=\"default_header_charset\" MAXLENGTH=\"32\" VALUE=\"");
	escputs(buf);
	wprintf("\">");
	wprintf("</td></tr>");

	/**
	 * Show empty floors?
	 */

	get_preference("emptyfloors", buf, sizeof buf);
	if (buf[0] == 0) strcpy(buf, "no");
	wprintf("<tr class=\"odd\"><td>");
	wprintf(_("Show empty floors"));
	wprintf("</td><td>");

	wprintf("<input type=\"radio\" name=\"emptyfloors\" VALUE=\"yes\"");
	if (!strcasecmp(buf, "yes")) wprintf(" checked");
	wprintf(">");
	wprintf(_("Yes"));
	wprintf("</input>&nbsp;&nbsp;&nbsp;");

	wprintf("<input type=\"radio\" name=\"emptyfloors\" VALUE=\"no\"");
	if (!strcasecmp(buf, "no")) wprintf(" checked");
	wprintf(">");
	wprintf(_("No"));
	wprintf("</input>\n");

	wprintf("</td></tr>\n");

	/** end table */
	wprintf("</table>\n");

	/** submit buttons */
	wprintf("<div class=\"buttons\"> ");
	wprintf("<input type=\"submit\" name=\"change_button\" value=\"%s\">"
		"&nbsp;"
		"<INPUT type=\"submit\" name=\"cancel_button\" value=\"%s\">\n",
		_("Change"),
		_("Cancel")
	);
	wprintf("</div>\n");

	/** end form */
	wprintf("</form>\n");
	wprintf("</div>\n");
	wDumpContent(1);
}

/**
 * \brief Commit new preferences and settings
 */
void set_preferences(void)
{
	char *fmt;
	char ebuf[300];
	int *time_format_cache;
	
	time_format_cache = &(WC->time_format_cache);

	if (IsEmptyStr(bstr("change_button"))) {
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
	fmt = bstr("calhourformat");
	set_preference("calhourformat", fmt, 0);
	if (!strcasecmp(fmt, "24")) 
		*time_format_cache = WC_TIMEFORMAT_24;
	else
		*time_format_cache = WC_TIMEFORMAT_AMPM;

	set_preference("weekstart", bstr("weekstart"), 0);
	set_preference("use_sig", bstr("use_sig"), 0);
	set_preference("daystart", bstr("daystart"), 0);
	set_preference("dayend", bstr("dayend"), 0);
	set_preference("default_header_charset", bstr("default_header_charset"), 0);
	set_preference("emptyfloors", bstr("emptyfloors"), 0);

	euid_escapize(ebuf, bstr("signature"));
	set_preference("signature", ebuf, 1);

	display_main_menu();
}


/*@}*/
