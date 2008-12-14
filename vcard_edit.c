/*
 * $Id$
 */

#include "webcit.h"


/**
 * \brief Record compare function for sorting address book indices
 * \param ab1 adressbook one
 * \param ab2 adressbook two
 */
int abcmp(const void *ab1, const void *ab2) {
	return(strcasecmp(
		(((const addrbookent *)ab1)->ab_name),
		(((const addrbookent *)ab2)->ab_name)
	));
}


/**
 * \brief Helper function for do_addrbook_view()
 * Converts a name into a three-letter tab label
 * \param tabbuf the tabbuffer to add name to
 * \param name the name to add to the tabbuffer
 */
void nametab(char *tabbuf, long len, char *name) {
	stresc(tabbuf, len, name, 0, 0);
	tabbuf[0] = toupper(tabbuf[0]);
	tabbuf[1] = tolower(tabbuf[1]);
	tabbuf[2] = tolower(tabbuf[2]);
	tabbuf[3] = 0;
}


/**
 * \brief display the adressbook overview
 * \param msgnum the citadel message number
 * \param alpha what????
 */
void display_addressbook(long msgnum, char alpha) {
	//char buf[SIZ];
	/* char mime_partnum[SIZ]; */
/* 	char mime_filename[SIZ]; */
/* 	char mime_content_type[SIZ]; */
	///char mime_disposition[SIZ];
	//int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	message_summary summ;////TODO: this will leak

	memset(&summ, 0, sizeof(summ));
	///safestrncpy(summ.subj, _("(no subject)"), sizeof summ.subj);
///Load Message headers
//	Msg = 
	if (!IsEmptyStr(vcard_partnum)) {
		vcard_source = load_mimepart(msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/** Display the summary line */
			display_vcard(WC->WBuf, vcard_source, alpha, 0, NULL,msgnum);

			/** If it's my vCard I can edit it */
			if (	(!strcasecmp(WC->wc_roomname, USERCONFIGROOM))
				|| (!strcasecmp(&WC->wc_roomname[11], USERCONFIGROOM))
				|| (WC->wc_view == VIEW_ADDRESSBOOK)
			) {
				wprintf("<a href=\"edit_vcard?"
					"msgnum=%ld&partnum=%s\">",
					msgnum, vcard_partnum);
				wprintf("[%s]</a>", _("edit"));
			}

			free(vcard_source);
		}
	}

}



/**
 * \brief  If it's an old "Firstname Lastname" style record, try to convert it.
 * \param namebuf name to analyze, reverse if nescessary
 */
void lastfirst_firstlast(char *namebuf) {
	char firstname[SIZ];
	char lastname[SIZ];
	int i;

	if (namebuf == NULL) return;
	if (strchr(namebuf, ';') != NULL) return;

	i = num_tokens(namebuf, ' ');
	if (i < 2) return;

	extract_token(lastname, namebuf, i-1, ' ', sizeof lastname);
	remove_token(namebuf, i-1, ' ');
	strcpy(firstname, namebuf);
	sprintf(namebuf, "%s; %s", lastname, firstname);
}

/**
 * \brief fetch what??? name
 * \param msgnum the citadel message number
 * \param namebuf where to put the name in???
 */
void fetch_ab_name(message_summary *Msg, char *namebuf) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char vcard_partnum[SIZ];
	char *vcard_source = NULL;
	int i, len;
	message_summary summ;/// TODO this will lak

	if (namebuf == NULL) return;
	strcpy(namebuf, "");

	memset(&summ, 0, sizeof(summ));
	//////safestrncpy(summ.subj, "(no subject)", sizeof summ.subj);

	sprintf(buf, "MSG0 %ld|0", Msg->msgnum);	/** unfortunately we need the mime info now */
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
			mime_length = extract_int(&buf[5], 5);

			if (  (!strcasecmp(mime_content_type, "text/x-vcard"))
			   || (!strcasecmp(mime_content_type, "text/vcard")) ) {
				strcpy(vcard_partnum, mime_partnum);
			}

		}
	}

	if (!IsEmptyStr(vcard_partnum)) {
		vcard_source = load_mimepart(Msg->msgnum, vcard_partnum);
		if (vcard_source != NULL) {

			/* Grab the name off the card */
			display_vcard(WC->WBuf, vcard_source, 0, 0, namebuf, Msg->msgnum);

			free(vcard_source);
		}
	}

	lastfirst_firstlast(namebuf);
	striplt(namebuf);
	len = strlen(namebuf);
	for (i=0; i<len; ++i) {
		if (namebuf[i] != ';') return;
	}
	strcpy(namebuf, _("(no name)"));
}



/**
 * \brief Turn a vCard "n" (name) field into something displayable.
 * \param name the name field to convert
 */
void vcard_n_prettyize(char *name)
{
	char *original_name;
	int i, j, len;

	original_name = strdup(name);
	len = strlen(original_name);
	for (i=0; i<5; ++i) {
		if (len > 0) {
			if (original_name[len-1] == ' ') {
				original_name[--len] = 0;
			}
			if (original_name[len-1] == ';') {
				original_name[--len] = 0;
			}
		}
	}
	strcpy(name, "");
	j=0;
	for (i=0; i<len; ++i) {
		if (original_name[i] == ';') {
			name[j++] = ',';
			name[j++] = ' ';			
		}
		else {
			name[j++] = original_name[i];
		}
	}
	name[j] = '\0';
	free(original_name);
}




/**
 * \brief preparse a vcard name
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
 * \param v the vcard to retrieve the name from
 * \param storename where to put the name at
 */
void fetchname_parsed_vcard(struct vCard *v, char *storename) {
	char *name;

	strcpy(storename, "");

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		strcpy(storename, name);
		/* vcard_n_prettyize(storename); */
	}

}



/**
 * \brief html print a vcard
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 *
 * Set 'full' to nonzero to display the full card, otherwise it will only
 * show a summary line.
 *
 * This code is a bit ugly, so perhaps an explanation is due: we do this
 * in two passes through the vCard fields.  On the first pass, we process
 * fields we understand, and then render them in a pretty fashion at the
 * end.  Then we make a second pass, outputting all the fields we don't
 * understand in a simple two-column name/value format.
 * \param v the vCard to display
 * \param full display all items of the vcard?
 * \param msgnum Citadel message pointer
 */
void display_parsed_vcard(StrBuf *Target, struct vCard *v, int full, long msgnum) {
	int i, j;
	char buf[SIZ];
	char *name;
	int is_qp = 0;
	int is_b64 = 0;
	char *thisname, *thisvalue;
	char firsttoken[SIZ];
	int pass;

	char fullname[SIZ];
	char title[SIZ];
	char org[SIZ];
	char phone[SIZ];
	char mailto[SIZ];

	strcpy(fullname, "");
	strcpy(phone, "");
	strcpy(mailto, "");
	strcpy(title, "");
	strcpy(org, "");

	if (!full) {
		StrBufAppendPrintf(Target, "<TD>");
		name = vcard_get_prop(v, "fn", 1, 0, 0);
		if (name != NULL) {
			StrEscAppend(Target, NULL, name, 0, 0);
		}
		else if (name = vcard_get_prop(v, "n", 1, 0, 0), name != NULL) {
			strcpy(fullname, name);
			vcard_n_prettyize(fullname);
			StrEscAppend(Target, NULL, fullname, 0, 0);
		}
		else {
			StrBufAppendPrintf(Target, "&nbsp;");
		}
		StrBufAppendPrintf(Target, "</TD>");
		return;
	}

	StrBufAppendPrintf(Target, "<div align=center>"
		"<table bgcolor=#aaaaaa width=50%%>");
	for (pass=1; pass<=2; ++pass) {

		if (v->numprops) for (i=0; i<(v->numprops); ++i) {
			int len;
			thisname = strdup(v->prop[i].name);
			extract_token(firsttoken, thisname, 0, ';', sizeof firsttoken);
	
			for (j=0; j<num_tokens(thisname, ';'); ++j) {
				extract_token(buf, thisname, j, ';', sizeof buf);
				if (!strcasecmp(buf, "encoding=quoted-printable")) {
					is_qp = 1;
					remove_token(thisname, j, ';');
				}
				if (!strcasecmp(buf, "encoding=base64")) {
					is_b64 = 1;
					remove_token(thisname, j, ';');
				}
			}
			
			len = strlen(v->prop[i].value);
			/* if we have some untagged QP, detect it here. */
			if (!is_qp && (strstr(v->prop[i].value, "=?")!=NULL))
				utf8ify_rfc822_string(v->prop[i].value);

			if (is_qp) {
				// %ff can become 6 bytes in utf8 
				thisvalue = malloc(len * 2 + 3); 
				j = CtdlDecodeQuotedPrintable(
					thisvalue, v->prop[i].value,
					len);
				thisvalue[j] = 0;
			}
			else if (is_b64) {
				// ff will become one byte..
				thisvalue = malloc(len + 50);
				CtdlDecodeBase64(
					thisvalue, v->prop[i].value,
					strlen(v->prop[i].value) );
			}
			else {
				thisvalue = strdup(v->prop[i].value);
			}
	
			/** Various fields we may encounter ***/
	
			/** N is name, but only if there's no FN already there */
			if (!strcasecmp(firsttoken, "n")) {
				if (IsEmptyStr(fullname)) {
					strcpy(fullname, thisvalue);
					vcard_n_prettyize(fullname);
				}
			}
	
			/** FN (full name) is a true 'display name' field */
			else if (!strcasecmp(firsttoken, "fn")) {
				strcpy(fullname, thisvalue);
			}

			/** title */
			else if (!strcasecmp(firsttoken, "title")) {
				strcpy(title, thisvalue);
			}
	
			/** organization */
			else if (!strcasecmp(firsttoken, "org")) {
				strcpy(org, thisvalue);
			}
	
			else if (!strcasecmp(firsttoken, "email")) {
				size_t len;
				if (!IsEmptyStr(mailto)) strcat(mailto, "<br />");
				strcat(mailto,
					"<a href=\"display_enter"
					"?force_room=_MAIL_?recp=");

				len = strlen(mailto);
				urlesc(&mailto[len], SIZ - len, "\"");
				len = strlen(mailto);
				urlesc(&mailto[len], SIZ - len,  fullname);
				len = strlen(mailto);
				urlesc(&mailto[len], SIZ - len, "\" <");
				len = strlen(mailto);
				urlesc(&mailto[len], SIZ - len, thisvalue);
				len = strlen(mailto);
				urlesc(&mailto[len], SIZ - len, ">");

				strcat(mailto, "\">");
				len = strlen(mailto);
				stresc(mailto+len, SIZ - len, thisvalue, 1, 1);
				strcat(mailto, "</A>");
			}
			else if (!strcasecmp(firsttoken, "tel")) {
				if (!IsEmptyStr(phone)) strcat(phone, "<br />");
				strcat(phone, thisvalue);
				for (j=0; j<num_tokens(thisname, ';'); ++j) {
					extract_token(buf, thisname, j, ';', sizeof buf);
					if (!strcasecmp(buf, "tel"))
						strcat(phone, "");
					else if (!strcasecmp(buf, "work"))
						strcat(phone, _(" (work)"));
					else if (!strcasecmp(buf, "home"))
						strcat(phone, _(" (home)"));
					else if (!strcasecmp(buf, "cell"))
						strcat(phone, _(" (cell)"));
					else {
						strcat(phone, " (");
						strcat(phone, buf);
						strcat(phone, ")");
					}
				}
			}
			else if (!strcasecmp(firsttoken, "adr")) {
				if (pass == 2) {
					StrBufAppendPrintf(Target, "<TR><TD>");
					StrBufAppendPrintf(Target, _("Address:"));
					StrBufAppendPrintf(Target, "</TD><TD>");
					for (j=0; j<num_tokens(thisvalue, ';'); ++j) {
						extract_token(buf, thisvalue, j, ';', sizeof buf);
						if (!IsEmptyStr(buf)) {
							StrEscAppend(Target, NULL, buf, 0, 0);
							if (j<3) StrBufAppendPrintf(Target, "<br />");
							else StrBufAppendPrintf(Target, " ");
						}
					}
					StrBufAppendPrintf(Target, "</TD></TR>\n");
				}
			}
			/* else if (!strcasecmp(firsttoken, "photo") && full && pass == 2) { 
				// Only output on second pass
				StrBufAppendPrintf(Target, "<tr><td>");
				StrBufAppendPrintf(Target, _("Photo:"));
				StrBufAppendPrintf(Target, "</td><td>");
				StrBufAppendPrintf(Target, "<img src=\"/vcardphoto/%ld/\" alt=\"Contact photo\"/>",msgnum);
				StrBufAppendPrintf(Target, "</td></tr>\n");
			} */
			else if (!strcasecmp(firsttoken, "version")) {
				/* ignore */
			}
			else if (!strcasecmp(firsttoken, "rev")) {
				/* ignore */
			}
			else if (!strcasecmp(firsttoken, "label")) {
				/* ignore */
			}
			else {

				/*** Don't show extra fields.  They're ugly.
				if (pass == 2) {
					StrBufAppendPrintf(Target, "<TR><TD>");
					StrEscAppend(Target, NULL, thisname, 0, 0);
					StrBufAppendPrintf(Target, "</TD><TD>");
					StrEscAppend(Target, NULL, thisvalue, 0, 0);
					StrBufAppendPrintf(Target, "</TD></TR>\n");
				}
				***/
			}
	
			free(thisname);
			free(thisvalue);
		}
	
		if (pass == 1) {
			StrBufAppendPrintf(Target, "<TR BGCOLOR=\"#AAAAAA\">"
			"<TD COLSPAN=2 BGCOLOR=\"#FFFFFF\">"
			"<IMG ALIGN=CENTER src=\"static/viewcontacts_48x.gif\">"
			"<FONT SIZE=+1><B>");
			StrEscAppend(Target, NULL, fullname, 0, 0);
			StrBufAppendPrintf(Target, "</B></FONT>");
			if (!IsEmptyStr(title)) {
				StrBufAppendPrintf(Target, "<div align=right>");
				StrEscAppend(Target, NULL, title, 0, 0);
				StrBufAppendPrintf(Target, "</div>");
			}
			if (!IsEmptyStr(org)) {
				StrBufAppendPrintf(Target, "<div align=right>");
				StrEscAppend(Target, NULL, org, 0, 0);
				StrBufAppendPrintf(Target, "</div>");
			}
			StrBufAppendPrintf(Target, "</TD></TR>\n");
		
			if (!IsEmptyStr(phone)) {
				StrBufAppendPrintf(Target, "<tr><td>");
				StrBufAppendPrintf(Target, _("Telephone:"));
				StrBufAppendPrintf(Target, "</td><td>%s</td></tr>\n", phone);
			}
			if (!IsEmptyStr(mailto)) {
				StrBufAppendPrintf(Target, "<tr><td>");
				StrBufAppendPrintf(Target, _("E-mail:"));
				StrBufAppendPrintf(Target, "</td><td>%s</td></tr>\n", mailto);
			}
		}

	}

	StrBufAppendPrintf(Target, "</table></div>\n");
}



/**
 * \brief  Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner.
 * Or, if "storename" is non-NULL, just store the person's name in that
 * buffer instead of displaying the card at all.
 * \param vcard_source the buffer containing the vcard text
 * \param alpha what???
 * \param full should we usse all lines?
 * \param storename where to store???
 * \param msgnum Citadel message pointer
 */
void display_vcard(StrBuf *Target, const char *vcard_source, char alpha, int full, char *storename, 
	long msgnum) {
	struct vCard *v;
	char *name;
	char buf[SIZ];
	char this_alpha = 0;

	v = vcard_load((char*)vcard_source); ///TODO

	if (v == NULL) return;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		utf8ify_rfc822_string(name);
		strcpy(buf, name);
		this_alpha = buf[0];
	}

	if (storename != NULL) {
		fetchname_parsed_vcard(v, storename);
	}
	else if (	(alpha == 0)
			|| ((isalpha(alpha)) && (tolower(alpha) == tolower(this_alpha)) )
			|| ((!isalpha(alpha)) && (!isalpha(this_alpha)))
		) {
		display_parsed_vcard(Target, v, full,msgnum);
	}

	vcard_free(v);
}



/**
 * \brief Render the address book using info we gathered during the scan
 * \param addrbook the addressbook to render
 * \param num_ab the number of the addressbook
 */
void do_addrbook_view(addrbookent *addrbook, int num_ab) {
	int i = 0;
	int displayed = 0;
	int bg = 0;
	static int NAMESPERPAGE = 60;
	int num_pages = 0;
	int tabfirst = 0;
	char tabfirst_label[64];
	int tablast = 0;
	char tablast_label[64];
	char this_tablabel[64];
	int page = 0;
	char **tablabels;

	if (num_ab == 0) {
		wprintf("<br /><br /><br /><div align=\"center\"><i>");
		wprintf(_("This address book is empty."));
		wprintf("</i></div>\n");
		return;
	}

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(addrbookent), abcmp);
	}

	num_pages = (num_ab / NAMESPERPAGE) + 1;

	tablabels = malloc(num_pages * sizeof (char *));
	if (tablabels == NULL) {
		wprintf("<br /><br /><br /><div align=\"center\"><i>");
		wprintf(_("An internal error has occurred."));
		wprintf("</i></div>\n");
		return;
	}

	for (i=0; i<num_pages; ++i) {
		tabfirst = i * NAMESPERPAGE;
		tablast = tabfirst + NAMESPERPAGE - 1;
		if (tablast > (num_ab - 1)) tablast = (num_ab - 1);
		nametab(tabfirst_label, 64, addrbook[tabfirst].ab_name);
		nametab(tablast_label, 64, addrbook[tablast].ab_name);
		sprintf(this_tablabel, "%s&nbsp;-&nbsp;%s", tabfirst_label, tablast_label);
		tablabels[i] = strdup(this_tablabel);
	}

	tabbed_dialog(num_pages, tablabels);
	page = (-1);

	for (i=0; i<num_ab; ++i) {

		if ((i / NAMESPERPAGE) != page) {	/* New tab */
			page = (i / NAMESPERPAGE);
			if (page > 0) {
				wprintf("</tr></table>\n");
				end_tab(page-1, num_pages);
			}
			begin_tab(page, num_pages);
			wprintf("<table border=0 cellspacing=0 cellpadding=3 width=100%%>\n");
			displayed = 0;
		}

		if ((displayed % 4) == 0) {
			if (displayed > 0) {
				wprintf("</tr>\n");
			}
			bg = 1 - bg;
			wprintf("<tr bgcolor=\"#%s\">",
				(bg ? "DDDDDD" : "FFFFFF")
			);
		}
	
		wprintf("<td>");

		wprintf("<a href=\"readfwd?startmsg=%ld?is_singlecard=1",
			addrbook[i].ab_msgnum);
		wprintf("?maxmsgs=1?is_summary=0?alpha=%s\">", bstr("alpha"));
		vcard_n_prettyize(addrbook[i].ab_name);
		escputs(addrbook[i].ab_name);
		wprintf("</a></td>\n");
		++displayed;
	}

	/* Placeholders for empty columns at end */
	if ((num_ab % 4) != 0) {
		for (i=0; i<(4-(num_ab % 4)); ++i) {
			wprintf("<td>&nbsp;</td>");
		}
	}

	wprintf("</tr></table>\n");
	end_tab((num_pages-1), num_pages);

	begin_tab(num_pages, num_pages);
	/* FIXME there ought to be something here */
	end_tab(num_pages, num_pages);

	for (i=0; i<num_pages; ++i) {
		free(tablabels[i]);
	}
	free(tablabels);
}




/*
 * Edit the vCard component of a MIME message.  
 * Supply the message number
 * and MIME part number to fetch.  Or, specify -1 for the message number
 * to start with a blank card.
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
	
			else if ( (!strcasecmp(key, "adr")) || (!strncasecmp(key, "adr;", 4)) ) {
				extract_token(pobox, value, 0, ';', sizeof pobox);
				extract_token(extadr, value, 1, ';', sizeof extadr);
				extract_token(street, value, 2, ';', sizeof street);
				extract_token(city, value, 3, ';', sizeof city);
				extract_token(state, value, 4, ';', sizeof state);
				extract_token(zipcode, value, 5, ';', sizeof zipcode);
				extract_token(country, value, 6, ';', sizeof country);
			}
	
			else if ( (!strcasecmp(key, "tel;home")) || (!strcasecmp(key, "tel;type=home")) ) {
				extract_token(hometel, value, 0, ';', sizeof hometel);
			}
	
			else if ( (!strcasecmp(key, "tel;work")) || (!strcasecmp(key, "tel;type=work")) ) {
				extract_token(worktel, value, 0, ';', sizeof worktel);
			}
	
			else if ( (!strcasecmp(key, "tel;fax")) || (!strcasecmp(key, "tel;type=fax")) ) {
				extract_token(faxtel, value, 0, ';', sizeof faxtel);
			}
	
			else if ( (!strcasecmp(key, "tel;cell")) || (!strcasecmp(key, "tel;type=cell")) ) {
				extract_token(mobiletel, value, 0, ';', sizeof mobiletel);
			}
	
			else if ( (!strcasecmp(key, "email;internet"))
			     || (!strcasecmp(key, "email;type=internet")) ) {
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
	do_template("beginboxx", NULL);

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
 *  commit the edits to the citadel server
 */
void edit_vcard(void) {
	long msgnum;
	char *partnum;

	msgnum = lbstr("msgnum");
	partnum = bstr("partnum");
	do_edit_vcard(msgnum, partnum, "", NULL);
}



/**
 *  parse edited vcard from the browser
 */
void submit_vcard(void) {
	struct vCard *v;
	char *serialized_vcard;
	char buf[SIZ];
	int i;

	if (!havebstr("ok_button")) { 
		readloop(readnew);
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
		readloop(readnew);
	}
}



/*
 * Extract an embedded photo from a vCard for display on the client
 */
void display_vcard_photo_img(void)
{
	long msgnum = 0L;
	char *vcard;
	struct vCard *v;
	char *photosrc;
	const char *contentType;
	wcsession *WCC = WC;

	msgnum = StrTol(WCC->UrlFragment2);
	
	vcard = load_mimepart(msgnum,"1");
	v = vcard_load(vcard);
	
	photosrc = vcard_get_prop(v, "PHOTO", 1,0,0);
	FlushStrBuf(WCC->WBuf);
	StrBufAppendBufPlain(WCC->WBuf, photosrc, -1, 0);
	if (StrBufDecodeBase64(WCC->WBuf) <= 0) {
		FlushStrBuf(WCC->WBuf);
		
		hprintf("HTTP/1.1 500 %s\n","Unable to get photo");
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf(_("Could Not decode vcard photo\n"));
		end_burst();
		return;
	}
	contentType = GuessMimeType(ChrPtr(WCC->WBuf), StrLength(WCC->WBuf));
	http_transmit_thing(contentType, 0);
	free(v);
	free(photosrc);
}



void 
InitModule_VCARD
(void)
{
	WebcitAddUrlHandler(HKEY("edit_vcard"), edit_vcard, 0);
	WebcitAddUrlHandler(HKEY("submit_vcard"), submit_vcard, 0);
	WebcitAddUrlHandler(HKEY("vcardphoto"), display_vcard_photo_img, NEED_URL);
}

