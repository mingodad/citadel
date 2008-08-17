/*
 * $Id$
 */
/**
 * \defgroup vCardEdit Handles on-screen editing of vCard objects.
 * \ingroup VCards
 */
/*@{*/
#include "webcit.h"

/**
 * \brief Edit the vCard component of a MIME message.  
 * Supply the message number
 * and MIME part number to fetch.  Or, specify -1 for the message number
 * to start with a blank card.
 * \param msgnum number of the item on the citadel server
 * \param partnum what???
 * \param return_to where to go back in the browser after edit ????
 */
void do_edit_vcard(long msgnum, char *partnum, char *return_to, char *force_room) {
	char buf[SIZ];
	char *serialized_vcard = NULL;
	size_t total_len = 0;
	struct vCard *v;
	int i;
	char *key, *value;
	char whatuser[256];

	char lastname[256];
	char firstname[256];
	char middlename[256];
	char prefix[256];
	char suffix[256];
	char pobox[256];
	char extadr[256];
	char street[256];
	char city[256];
	char state[256];
	char zipcode[256];
	char country[256];
	char hometel[256];
	char worktel[256];
	char faxtel[256];
	char mobiletel[256];
	char primary_inetemail[256];
	char other_inetemail[SIZ];
	char extrafields[SIZ];
	char fullname[256];
	char title[256];
	char org[256];

	lastname[0] = 0;
	firstname[0] = 0;
	middlename[0] = 0;
	prefix[0] = 0;
	suffix[0] = 0;
	pobox[0] = 0;
	extadr[0] = 0;
	street[0] = 0;
	city[0] = 0;
	state[0] = 0;
	zipcode[0] = 0;
	country[0] = 0;
	hometel[0] = 0;
	worktel[0] = 0;
	faxtel[0] = 0;
	mobiletel[0] = 0;
	primary_inetemail[0] = 0;
	other_inetemail[0] = 0;
	title[0] = 0;
	org[0] = 0;
	extrafields[0] = 0;
	fullname[0] = 0;

	safestrncpy(whatuser, "", sizeof whatuser);

	if (msgnum >= 0) {
		sprintf(buf, "MSG0 %ld|1", msgnum);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '1') {
			convenience_page("770000", _("Error"), &buf[4]);
			return;
		}
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (!strncasecmp(buf, "from=", 5)) {
				safestrncpy(whatuser, &buf[5], sizeof whatuser);
			}
			else if (!strncasecmp(buf, "node=", 5)) {
				strcat(whatuser, " @ ");
				strcat(whatuser, &buf[5]);
			}
		}
	
		sprintf(buf, "DLAT %ld|%s", msgnum, partnum);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '6') {
			convenience_page("770000", "Error", &buf[4]);
			return;
		}
	
		total_len = atoi(&buf[4]);
		serialized_vcard = malloc(total_len + 2);

		serv_read(serialized_vcard, total_len);
		serialized_vcard[total_len] = 0;
	
		v = vcard_load(serialized_vcard);
		free(serialized_vcard);
	
		/* Populate the variables for our form */
		i = 0;
		while (key = vcard_get_prop(v, "", 0, i, 1), key != NULL) {
			value = vcard_get_prop(v, "", 0, i++, 0);
	
			if (!strcasecmp(key, "n")) {
				extract_token(lastname, value, 0, ';', sizeof lastname);
				extract_token(firstname, value, 1, ';', sizeof firstname);
				extract_token(middlename, value, 2, ';', sizeof middlename);
				extract_token(prefix, value, 3, ';', sizeof prefix);
				extract_token(suffix, value, 4, ';', sizeof suffix);
			}

			else if (!strcasecmp(key, "fn")) {
				safestrncpy(fullname, value, sizeof fullname);
			}

			else if (!strcasecmp(key, "title")) {
				safestrncpy(title, value, sizeof title);
			}
	
			else if (!strcasecmp(key, "org")) {
				safestrncpy(org, value, sizeof org);
			}
	
			else if (!strcasecmp(key, "adr")) {
				extract_token(pobox, value, 0, ';', sizeof pobox);
				extract_token(extadr, value, 1, ';', sizeof extadr);
				extract_token(street, value, 2, ';', sizeof street);
				extract_token(city, value, 3, ';', sizeof city);
				extract_token(state, value, 4, ';', sizeof state);
				extract_token(zipcode, value, 5, ';', sizeof zipcode);
				extract_token(country, value, 6, ';', sizeof country);
			}
	
			else if (!strcasecmp(key, "tel;home")) {
				extract_token(hometel, value, 0, ';', sizeof hometel);
			}
	
			else if (!strcasecmp(key, "tel;work")) {
				extract_token(worktel, value, 0, ';', sizeof worktel);
			}
	
			else if (!strcasecmp(key, "tel;fax")) {
				extract_token(faxtel, value, 0, ';', sizeof faxtel);
			}
	
			else if (!strcasecmp(key, "tel;cell")) {
				extract_token(mobiletel, value, 0, ';', sizeof mobiletel);
			}
	
			else if (!strcasecmp(key, "email;internet")) {
				if (primary_inetemail[0] == 0) {
					safestrncpy(primary_inetemail, value, sizeof primary_inetemail);
				}
				else {
					if (other_inetemail[0] != 0) {
						strcat(other_inetemail, "\n");
					}
					strcat(other_inetemail, value);
				}
			}
	
			else {
				strcat(extrafields, key);
				strcat(extrafields, ":");
				strcat(extrafields, value);
				strcat(extrafields, "\n");
			}
	
		}
	
		vcard_free(v);
	}

	/** Display the form */
	output_headers(1, 1, 1, 0, 0, 0);

	svput("BOXTITLE", WCS_STRING, _("Edit contact information"));
	do_template("beginbox", NULL);

	wprintf("<form method=\"POST\" action=\"submit_vcard\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	if (force_room != NULL) {
		wprintf("<input type=\"hidden\" name=\"force_room\" value=\"");
		escputs(force_room);
		wprintf("\">\n");
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"vcard_edit_background\"><tr><td>\n");

	wprintf("<table border=0><tr>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td></tr>\n",
		_("Prefix"), _("First"), _("Middle"), _("Last"), _("Suffix")
	);
	wprintf("<tr><td><input type=\"text\" name=\"prefix\" "
		"value=\"%s\" maxlength=\"5\" size=\"5\"></td>",
		prefix);
	wprintf("<td><input type=\"text\" name=\"firstname\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		firstname);
	wprintf("<td><input type=\"text\" name=\"middlename\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		middlename);
	wprintf("<td><input type=\"text\" name=\"lastname\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		lastname);
	wprintf("<td><input type=\"text\" name=\"suffix\" "
		"value=\"%s\" maxlength=\"10\" size=\"10\"></td></tr></table>\n",
		suffix);

	wprintf("<table  class=\"vcard_edit_background_alt\">");
	wprintf("<tr><td>");

	wprintf(_("Display name:"));
	wprintf("<br>"
		"<input type=\"text\" name=\"fullname\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		fullname
	);

	wprintf(_("Title:"));
	wprintf("<br>"
		"<input type=\"text\" name=\"title\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		title
	);

	wprintf(_("Organization:"));
	wprintf("<br>"
		"<input type=\"text\" name=\"org\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		org
	);

	wprintf("</td><td>");

	wprintf("<table border=0>");
	wprintf("<tr><td>");
	wprintf(_("PO box:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"pobox\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		pobox);
	wprintf("<tr><td>");
	wprintf(_("Address:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"extadr\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		extadr);
	wprintf("<tr><td> </td><td>"
		"<input type=\"text\" name=\"street\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		street);
	wprintf("<tr><td>");
	wprintf(_("City:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"city\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		city);
	wprintf("<tr><td>");
	wprintf(_("State:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"state\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		state);
	wprintf("<tr><td>");
	wprintf(_("ZIP code:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"zipcode\" "
		"value=\"%s\" maxlength=\"10\"></td></tr>\n",
		zipcode);
	wprintf("<tr><td>");
	wprintf(_("Country:"));
	wprintf("</td><td>"
		"<input type=\"text\" name=\"country\" "
		"value=\"%s\" maxlength=\"29\" width=\"5\"></td></tr>\n",
		country);
	wprintf("</table>\n");

	wprintf("</table>\n");

	wprintf("<table border=0><tr><td>");
	wprintf(_("Home telephone:"));
	wprintf("</td>"
		"<td><input type=\"text\" name=\"hometel\" "
		"value=\"%s\" maxlength=\"29\"></td>\n",
		hometel);
	wprintf("<td>");
	wprintf(_("Work telephone:"));
	wprintf("</td>"
		"<td><input type=\"text\" name=\"worktel\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		worktel);
	wprintf("<tr><td>");
	wprintf(_("Mobile telephone:"));
	wprintf("</td>"
		"<td><input type=\"text\" name=\"mobiletel\" "
		"value=\"%s\" maxlength=\"29\"></td>\n",
		mobiletel);
	wprintf("<td>");
	wprintf(_("Fax number:"));
	wprintf("</td>"
		"<td><input type=\"text\" name=\"faxtel\" "
		"value=\"%s\" maxlength=\"29\"></td></tr></table>\n",
		faxtel);

	wprintf("<table class=\"vcard_edit_background_alt\">");
	wprintf("<tr><td>");

	wprintf("<table border=0><TR>"
		"<td valign=top>");
	wprintf(_("Primary Internet e-mail address"));
	wprintf("<br />"
		"<input type=\"text\" name=\"primary_inetemail\" "
		"size=40 maxlength=60 value=\"");
	escputs(primary_inetemail);
	wprintf("\"><br />"
		"</td><td valign=top>");
	wprintf(_("Internet e-mail aliases"));
	wprintf("<br />"
		"<textarea name=\"other_inetemail\" rows=5 cols=40 width=40>");
	escputs(other_inetemail);
	wprintf("</textarea></td></tr></table>\n");

	wprintf("</td></tr></table>\n");

	wprintf("<input type=\"hidden\" name=\"extrafields\" value=\"");
	escputs(extrafields);
	wprintf("\">\n");

	wprintf("<input type=\"hidden\" name=\"return_to\" value=\"");
	urlescputs(return_to);
	wprintf("\">\n");

	wprintf("<div class=\"buttons\">\n"
		"<input type=\"submit\" name=\"ok_button\" value=\"%s\">"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">"
		"</div></form>\n",
		_("Save changes"),
		_("Cancel")
	);
	
	wprintf("</td></tr></table>\n");
	do_template("endbox", NULL);
	wDumpContent(1);
}


/**
 * \brief commit the edits to the citadel server
 */
void edit_vcard(void) {
	long msgnum;
	char *partnum;

	msgnum = lbstr("msgnum");
	partnum = bstr("partnum");
	do_edit_vcard(msgnum, partnum, "", NULL);
}



/**
 * \brief parse edited vcard from the browser
 */
void submit_vcard(void) {
	struct vCard *v;
	char *serialized_vcard;
	char buf[SIZ];
	int i;

	if (!havebstr("ok_button")) { 
		readloop("readnew");
		return;
	}

	if (havebstr("force_room")) {
		gotoroom(bstr("force_room"));
	}

	sprintf(buf, "ENT0 1|||4||");
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		edit_vcard();
		return;
	}

	/** Make a vCard structure out of the data supplied in the form */

	snprintf(buf, sizeof buf, "begin:vcard\r\n%s\r\nend:vcard\r\n",
		bstr("extrafields")
	);
	v = vcard_load(buf);	/** Start with the extra fields */
	if (v == NULL) {
		safestrncpy(WC->ImportantMessage,
			_("An error has occurred."),
			sizeof WC->ImportantMessage
		);
		edit_vcard();
		return;
	}

	snprintf(buf, sizeof buf, "%s;%s;%s;%s;%s",
		bstr("lastname"),
		bstr("firstname"),
		bstr("middlename"),
		bstr("prefix"),
		bstr("suffix") );
	vcard_add_prop(v, "n", buf);
	
	vcard_add_prop(v, "title", bstr("title"));
	vcard_add_prop(v, "fn", bstr("fullname"));
	vcard_add_prop(v, "org", bstr("org"));

	snprintf(buf, sizeof buf, "%s;%s;%s;%s;%s;%s;%s",
		bstr("pobox"),
		bstr("extadr"),
		bstr("street"),
		bstr("city"),
		bstr("state"),
		bstr("zipcode"),
		bstr("country") );
	vcard_add_prop(v, "adr", buf);

	vcard_add_prop(v, "tel;home", bstr("hometel"));
	vcard_add_prop(v, "tel;work", bstr("worktel"));
	vcard_add_prop(v, "tel;fax", bstr("faxtel"));
	vcard_add_prop(v, "tel;cell", bstr("mobiletel"));
	vcard_add_prop(v, "email;internet", bstr("primary_inetemail"));

	for (i=0; i<num_tokens(bstr("other_inetemail"), '\n'); ++i) {
		extract_token(buf, bstr("other_inetemail"), i, '\n', sizeof buf);
		if (!IsEmptyStr(buf)) {
			vcard_add_prop(v, "email;internet", buf);
		}
	}

	serialized_vcard = vcard_serialize(v);
	vcard_free(v);
	if (serialized_vcard == NULL) {
		safestrncpy(WC->ImportantMessage,
			_("An error has occurred."),
			sizeof WC->ImportantMessage
		);
		edit_vcard();
		return;
	}

	serv_puts("Content-type: text/x-vcard; charset=UTF-8");
	serv_puts("");
	serv_printf("%s\r\n", serialized_vcard);
	serv_puts("000");
	free(serialized_vcard);

	if (!strcmp(bstr("return_to"), "select_user_to_edit")) {
		select_user_to_edit(NULL, NULL);
	}
	else if (!strcmp(bstr("return_to"), "do_welcome")) {
		do_welcome();
	}
	else {
		readloop("readnew");
	}
}



void 
InitModule_VCARD
(void)
{
	WebcitAddUrlHandler(HKEY("edit_vcard"), edit_vcard, 0);
	WebcitAddUrlHandler(HKEY("submit_vcard"), submit_vcard, 0);
}

/*@}*/
