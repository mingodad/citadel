/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include "calendar.h"

CtxType CTX_VCARD = CTX_NONE;

ConstStr VCStr [] = {
	{HKEY("n")}, /* N is name, but only if there's no FN already there */
	{HKEY("fn")}, /* FN (full name) is a true 'display name' field */
	{HKEY("title")},   /* title */
	{HKEY("org")},    /* organization */
	{HKEY("email")},
	{HKEY("tel")},
	{HKEY("tel_tel")},
	{HKEY("tel_work")},
	{HKEY("tel_home")},
	{HKEY("tel_cell")},
	{HKEY("adr")},
	{HKEY("photo")},
	{HKEY("version")},
	{HKEY("rev")},
	{HKEY("label")}
};

typedef enum _eVC{
	VC_n,
	VC_fn,
	VC_title,
	VC_org,
	VC_email,
	VC_tel,
	VC_tel_tel,
	VC_tel_work,
	VC_tel_home,
	VC_tel_cell,
	VC_adr,
	VC_photo,
	VC_version,
	VC_rev,
	VC_label
} eVC;

HashList *VCToEnum = NULL;

/*
 * Record compare function for sorting address book indices
 */
int abcmp(const void *ab1, const void *ab2) {
	return(strcasecmp(
		(((const addrbookent *)ab1)->ab_name),
		(((const addrbookent *)ab2)->ab_name)
	));
}


/*
 * Helper function for do_addrbook_view()
 * Converts a name into a three-letter tab label
 */
void nametab(char *tabbuf, long len, char *name) {
	stresc(tabbuf, len, name, 0, 0);
	tabbuf[0] = toupper(tabbuf[0]);
	tabbuf[1] = tolower(tabbuf[1]);
	tabbuf[2] = tolower(tabbuf[2]);
	tabbuf[3] = 0;
}


/*
 * If it's an old "Firstname Lastname" style record, try to convert it.
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



wc_mime_attachment *load_vcard(message_summary *Msg) 
{
	HashPos  *it;
	StrBuf *FoundCharset = NewStrBuf();
	StrBuf *Error;
	void *vMime;
	const char *Key;
	long len;
	wc_mime_attachment *Mime;
	wc_mime_attachment *VCMime = NULL;

	Msg->MsgBody =  (wc_mime_attachment*) malloc(sizeof(wc_mime_attachment));
	memset(Msg->MsgBody, 0, sizeof(wc_mime_attachment));
	Msg->MsgBody->msgnum = Msg->msgnum;

	load_message(Msg, FoundCharset, &Error);

	FreeStrBuf(&FoundCharset);
	/* look up the vcard... */
	it = GetNewHashPos(Msg->AllAttach, 0);
	while (GetNextHashPos(Msg->AllAttach, it, &len, &Key, &vMime) && 
	       (vMime != NULL)) 
	{
		Mime = (wc_mime_attachment*) vMime;
		if ((strcmp(ChrPtr(Mime->ContentType),
			   "text/x-vcard") == 0) ||
		    (strcmp(ChrPtr(Mime->ContentType),
			    "text/vcard") == 0))
		{
			VCMime = Mime;
			break;
		}
	}
	DeleteHashPos(&it);
	if (VCMime == NULL)
		return NULL;

	if (VCMime->Data == NULL)
		MimeLoadData(VCMime);
	return VCMime;
}

/*
 * fetch the display name off a vCard
 */
void fetch_ab_name(message_summary *Msg, char **namebuf) {
	long len;
	int i;
	wc_mime_attachment *VCMime = NULL;

	if (namebuf == NULL) return;

	VCMime = load_vcard(Msg);
	if (VCMime == NULL)
		return;

	/* Grab the name off the card */
	display_vcard(WC->WBuf, VCMime, 0, 0, namebuf, Msg->msgnum);

	if (*namebuf != NULL) {
		lastfirst_firstlast(*namebuf);
		striplt(*namebuf);
		len = strlen(*namebuf);
		for (i=0; i<len; ++i) {
			if ((*namebuf)[i] != ';') return;
		}
		free (*namebuf);
		(*namebuf) = strdup(_("(no name)"));
	}
	else {
		(*namebuf) = strdup(_("(no name)"));
	}
}



/*
 * Turn a vCard "n" (name) field into something displayable.
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




/*
 * preparse a vcard name
 * display_vcard() calls this after parsing the textual vCard into
 * our 'struct vCard' data object.
 * This gets called instead of display_parsed_vcard() if we are only looking
 * to extract the person's name instead of displaying the card.
 */
void fetchname_parsed_vcard(struct vCard *v, char **storename) {
	char *name;
	char *prop;
	char buf[SIZ];
	int j, n, len;
	int is_qp = 0;
	int is_b64 = 0;

	*storename = NULL;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		len = strlen(name);
		prop = vcard_get_prop(v, "n", 1, 0, 1);
		n = num_tokens(prop, ';');

		for (j=0; j<n; ++j) {
			extract_token(buf, prop, j, ';', sizeof buf);
			if (!strcasecmp(buf, "encoding=quoted-printable")) {
				is_qp = 1;
			}
			if (!strcasecmp(buf, "encoding=base64")) {
				is_b64 = 1;
			}
		}
		if (is_qp) {
			/* %ff can become 6 bytes in utf8  */
			*storename = malloc(len * 2 + 3); 
			j = CtdlDecodeQuotedPrintable(
				*storename, name,
				len);
			(*storename)[j] = 0;
		}
		else if (is_b64) {
			/* ff will become one byte.. */
			*storename = malloc(len + 50);
			CtdlDecodeBase64(
				*storename, name,
				len);
		}
		else {
			size_t len;

			len = strlen (name);
			
			*storename = malloc(len + 3); /* \0 + eventualy missing ', '*/
			memcpy(*storename, name, len + 1);
		}
		/* vcard_n_prettyize(storename); */
	}

}



/*
 * html print a vcard
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
 * v		the vCard to display
 * full		display all items of the vcard?
 * msgnum	Citadel message pointer
 */
void display_parsed_vcard(StrBuf *Target, struct vCard *v, int full, wc_mime_attachment *Mime)
{
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
		StrBufAppendPrintf(Target, "<td>");
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
		StrBufAppendPrintf(Target, "</td>");
		return;
	}

	StrBufAppendPrintf(Target, "<div align=\"center\">"
		"<table bgcolor=\"#aaaaaa\" width=\"50%%\">");
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
				utf8ify_rfc822_string(&v->prop[i].value);

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
	
			/* Various fields we may encounter ***/
	
			/* N is name, but only if there's no FN already there */
			if (!strcasecmp(firsttoken, "n")) {
				if (IsEmptyStr(fullname)) {
					strcpy(fullname, thisvalue);
					vcard_n_prettyize(fullname);
				}
			}
	
			/* FN (full name) is a true 'display name' field */
			else if (!strcasecmp(firsttoken, "fn")) {
				strcpy(fullname, thisvalue);
			}

			/* title */
			else if (!strcasecmp(firsttoken, "title")) {
				strcpy(title, thisvalue);
			}
	
			/* organization */
			else if (!strcasecmp(firsttoken, "org")) {
				strcpy(org, thisvalue);
			}
	
			else if (!strcasecmp(firsttoken, "email")) {
				size_t len;
				if (!IsEmptyStr(mailto)) strcat(mailto, "<br>");
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
				if (!IsEmptyStr(phone)) strcat(phone, "<br>");
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
					StrBufAppendPrintf(Target, "<tr><td>");
					StrBufAppendPrintf(Target, _("Address:"));
					StrBufAppendPrintf(Target, "</td><td>");
					for (j=0; j<num_tokens(thisvalue, ';'); ++j) {
						extract_token(buf, thisvalue, j, ';', sizeof buf);
						if (!IsEmptyStr(buf)) {
							StrEscAppend(Target, NULL, buf, 0, 0);
							if (j<3) StrBufAppendPrintf(Target, "<br>");
							else StrBufAppendPrintf(Target, " ");
						}
					}
					StrBufAppendPrintf(Target, "</td></tr>\n");
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
			StrBufAppendPrintf(Target, "<tr bgcolor=\"#aaaaaa\">"
      "<td colspan=2 bgcolor=\"#ffffff\">"
      "<img align=\"center\" src=\"static/webcit_icons/essen/32x32/contact.png\">"
      "<font size=\"+1\"><b>");
			StrEscAppend(Target, NULL, fullname, 0, 0);
			StrBufAppendPrintf(Target, "</b></font>");
			if (!IsEmptyStr(title)) {
				StrBufAppendPrintf(Target, "<div align=\"right>\"");
				StrEscAppend(Target, NULL, title, 0, 0);
				StrBufAppendPrintf(Target, "</div>");
			}
			if (!IsEmptyStr(org)) {
				StrBufAppendPrintf(Target, "<div align=\"right\">");
				StrEscAppend(Target, NULL, org, 0, 0);
				StrBufAppendPrintf(Target, "</div>");
			}
			StrBufAppendPrintf(Target, "</td></tr>\n");
		
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

/*
 * html print a vcard
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
 * v		the vCard to display
 * full		display all items of the vcard?
 * msgnum	Citadel message pointer
 */
void parse_vcard(StrBuf *Target, struct vCard *v, HashList *VC, int full, wc_mime_attachment *Mime)
{
	StrBuf *Val = NULL;
	StrBuf *Swap = NULL;
	int i, j;
	char buf[SIZ];
	int is_qp = 0;
	int is_b64 = 0;
	StrBuf *thisname = NULL;
	char firsttoken[SIZ];
	void *V;

	Swap = NewStrBuf ();
	thisname = NewStrBuf();
	for (i=0; i<(v->numprops); ++i) {
		is_qp = 0;
		is_b64 = 0;
		StrBufPlain(thisname, v->prop[i].name, -1);
		StrBufLowerCase(thisname);
		
		/*len = */extract_token(firsttoken, ChrPtr(thisname), 0, ';', sizeof firsttoken);
		
		for (j=0; j<num_tokens(ChrPtr(thisname), ';'); ++j) {
			extract_token(buf, ChrPtr(thisname), j, ';', sizeof buf);
			if (!strcasecmp(buf, "encoding=quoted-printable")) {
				is_qp = 1;
/*				remove_token(thisname, j, ';');*/
			}
			if (!strcasecmp(buf, "encoding=base64")) {
				is_b64 = 1;
/*				remove_token(thisname, j, ';');*/
			}
		}
		
		/* copy over the payload into a StrBuf */
		Val = NewStrBufPlain(v->prop[i].value, -1);
			
		/* if we have some untagged QP, detect it here. */
		if (is_qp || (strstr(v->prop[i].value, "=?")!=NULL)){
			StrBuf *b;
			StrBuf_RFC822_to_Utf8(Swap, Val, NULL, NULL); /* default charset, current charset */
			b = Val;
			Val = Swap; 
			Swap = b;
			FlushStrBuf(Swap);
		}
		else if (is_b64) {
			StrBufDecodeBase64(Val);

		}
		syslog(LOG_DEBUG, "%s [%s][%s]",
			firsttoken,
			ChrPtr(Val),
			v->prop[i].value);
		if (GetHash(VCToEnum, firsttoken, strlen(firsttoken), &V))
		{
			eVC evc = (eVC) V;
			Put(VC, IKEY(evc), Val, HFreeStrBuf);
			syslog(LOG_DEBUG, "[%ul]\n", evc);
			Val = NULL;
		}
		else
			syslog(LOG_DEBUG, "[]\n");
/*
TODO: check for layer II
		else 
		{
			long max = num_tokens(thisname, ';');
			firsttoken[len] = '_';

			for (j = 0; j < max; j++) {
//			firsttoken[len]

				extract_token(buf, thisname, j, ';', sizeof (buf));
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

		}
*/
	
		FreeStrBuf(&Val);
		free(thisname);
		thisname = NULL;
	}

}

void tmplput_VCARD_ITEM(StrBuf *Target, WCTemplputParams *TP)
{
	HashList *VC = CTX(CTX_VCARD);
	eVC evc;
	void *vStr;

	evc = GetTemplateTokenNumber(Target, TP, 0, -1);
	if (evc != -1)
	{
		if (GetHash(VC, IKEY(evc), &vStr))
		{
			StrBufAppendTemplate(Target, TP,
					     (StrBuf*) vStr,
					     1);
		}
	}
	
}

void new_vcard (StrBuf *Target, struct vCard *v, int full, wc_mime_attachment *Mime)
{
	HashList *VC;
	WCTemplputParams SubTP;

        memset(&SubTP, 0, sizeof(WCTemplputParams));    


	VC = NewHash(0, Flathash);
	parse_vcard(Target, v, VC, full, Mime);

	SubTP.Filter.ContextType = CTX_VCARD;
	SubTP.Context = VC;

	DoTemplate(HKEY("test_vcard"), Target, &SubTP);
	DeleteHash(&VC);
}



/*
 * Display a textual vCard
 * (Converts to a vCard object and then calls the actual display function)
 * Set 'full' to nonzero to display the whole card instead of a one-liner.
 * Or, if "storename" is non-NULL, just store the person's name in that
 * buffer instead of displaying the card at all.
 *
 * vcard_source	the buffer containing the vcard text
 * alpha	Display only if name begins with this letter of the alphabet
 * full		Display the full vCard (otherwise just the display name)
 * storename	If not NULL, also store the display name here
 * msgnum	Citadel message pointer
 */
void display_vcard(StrBuf *Target, 
		   wc_mime_attachment *Mime, 
		   char alpha, 
		   int full, 
		   char **storename, 
		   long msgnum) 
{
	struct vCard *v;
	char *name;
	StrBuf *Buf;
	StrBuf *Buf2;
	char this_alpha = 0;

	v = VCardLoad(Mime->Data);

	if (v == NULL) return;

	name = vcard_get_prop(v, "n", 1, 0, 0);
	if (name != NULL) {
		Buf = NewStrBufPlain(name, -1);
		Buf2 = NewStrBufPlain(NULL, StrLength(Buf));
		StrBuf_RFC822_to_Utf8(Buf2, Buf, WC->DefaultCharset, NULL);
		this_alpha = ChrPtr(Buf)[0];
		FreeStrBuf(&Buf);
		FreeStrBuf(&Buf2);
	}

	if (storename != NULL) {
		fetchname_parsed_vcard(v, storename);
	}
	else if ((alpha == 0) || 
		 ((isalpha(alpha)) && (tolower(alpha) == tolower(this_alpha))) || 
		 ((!isalpha(alpha)) && (!isalpha(this_alpha)))
		) 
	{
#ifdef XXX_XXX
		new_vcard (Target, v, full, Mime);
#else
		display_parsed_vcard(Target, v, full, Mime);
#endif		
	}

	vcard_free(v);
}



/*
 * Render the address book using info we gathered during the scan
 *
 * addrbook	the addressbook to render
 * num_ab	the number of the addressbook
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
		wc_printf("<br><br><br><div align=\"center\"><i>");
		wc_printf(_("This address book is empty."));
		wc_printf("</i></div>\n");
		return;
	}

	if (num_ab > 1) {
		qsort(addrbook, num_ab, sizeof(addrbookent), abcmp);
	}

	num_pages = (num_ab / NAMESPERPAGE) + 1;

	tablabels = malloc(num_pages * sizeof (char *));
	if (tablabels == NULL) {
		wc_printf("<br><br><br><div align=\"center\"><i>");
		wc_printf(_("An internal error has occurred."));
		wc_printf("</i></div>\n");
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
				wc_printf("</tr></table>\n");
				end_tab(page-1, num_pages);
			}
			begin_tab(page, num_pages);
			wc_printf("<table border=\"0\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%%\">\n");
			displayed = 0;
		}

		if ((displayed % 4) == 0) {
			if (displayed > 0) {
				wc_printf("</tr>\n");
			}
			bg = 1 - bg;
			wc_printf("<tr bgcolor=\"#%s\">",
				(bg ? "dddddd" : "ffffff")
			);
		}
	
		wc_printf("<td>");

		wc_printf("<a href=\"readfwd?startmsg=%ld?is_singlecard=1",
			addrbook[i].ab_msgnum);
		wc_printf("?maxmsgs=1?is_summary=0?alpha=%s\">", bstr("alpha"));
		vcard_n_prettyize(addrbook[i].ab_name);
		escputs(addrbook[i].ab_name);
		wc_printf("</a></td>\n");
		++displayed;
	}

	/* Placeholders for empty columns at end */
	if ((num_ab % 4) != 0) {
		for (i=0; i<(4-(num_ab % 4)); ++i) {
			wc_printf("<td>&nbsp;</td>");
		}
	}

	wc_printf("</tr></table>\n");
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
void do_edit_vcard(long msgnum, char *partnum, 
		   message_summary *VCMsg,
		   wc_mime_attachment *VCAtt,
		   const char *return_to, 
		   const char *force_room) {
	wcsession *WCC = WC;
	message_summary *Msg = NULL;
	wc_mime_attachment *VCMime = NULL;
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

	if ((msgnum >= 0) || 
	    ((VCMsg != NULL) && (VCAtt != NULL)))
	{
		if ((VCMsg == NULL) && (VCAtt == NULL)) {

			Msg = (message_summary *) malloc(sizeof(message_summary));
			memset(Msg, 0, sizeof(message_summary));
			Msg->msgnum = msgnum;
			VCMime = load_vcard(Msg);
			if (VCMime == NULL) {
				convenience_page("770000", _("Error"), "");///TODO: important message
				DestroyMessageSummary(Msg);
				return;
			}
		
			v = VCardLoad(VCMime->Data);
		}
		else {
			v = VCardLoad(VCAtt->Data);
		}
	
		/* Populate the variables for our form */
		i = 0;
		while (key = vcard_get_prop(v, "", 0, i, 1), key != NULL) {
			char prp[256];	/* property name */
			char prm[256];	/* parameters */

			value = vcard_get_prop(v, "", 0, i++, 0);


			extract_token(prp, key, 0, ';', sizeof prp);
			safestrncpy(prm, key, sizeof prm);
			remove_token(prm, 0, ';');

			if (!strcasecmp(prp, "n")) {
				extract_token(lastname, value, 0, ';', sizeof lastname);
				extract_token(firstname, value, 1, ';', sizeof firstname);
				extract_token(middlename, value, 2, ';', sizeof middlename);
				extract_token(prefix, value, 3, ';', sizeof prefix);
				extract_token(suffix, value, 4, ';', sizeof suffix);
			}

			else if (!strcasecmp(prp, "fn")) {
				safestrncpy(fullname, value, sizeof fullname);
			}

			else if (!strcasecmp(prp, "title")) {
				safestrncpy(title, value, sizeof title);
			}
	
			else if (!strcasecmp(prp, "org")) {
				safestrncpy(org, value, sizeof org);
			}
	
			else if (!strcasecmp(prp, "adr")) {
				extract_token(pobox, value, 0, ';', sizeof pobox);
				extract_token(extadr, value, 1, ';', sizeof extadr);
				extract_token(street, value, 2, ';', sizeof street);
				extract_token(city, value, 3, ';', sizeof city);
				extract_token(state, value, 4, ';', sizeof state);
				extract_token(zipcode, value, 5, ';', sizeof zipcode);
				extract_token(country, value, 6, ';', sizeof country);
			}

			else if (!strcasecmp(prp, "tel")) {

				if (bmstrcasestr(prm, "home")) {
					extract_token(hometel, value, 0, ';', sizeof hometel);
				}
				else if (bmstrcasestr(prm, "work")) {
					extract_token(worktel, value, 0, ';', sizeof worktel);
				}
				else if (bmstrcasestr(prm, "fax")) {
					extract_token(faxtel, value, 0, ';', sizeof faxtel);
				}
				else if (bmstrcasestr(prm, "cell")) {
					extract_token(mobiletel, value, 0, ';', sizeof mobiletel);
				}
				else {	/* Missing or unknown type; put it in the home phone */
					extract_token(hometel, value, 0, ';', sizeof hometel);
				}
			}
	
			else if ( (!strcasecmp(prp, "email")) && (bmstrcasestr(prm, "internet")) ) {
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

			/* Unrecognized properties are preserved here so we don't discard them
			 * just because the vCard was edited with WebCit.
			 */
			else {
				strcat(extrafields, key);
				strcat(extrafields, ":");
				strcat(extrafields, value);
				strcat(extrafields, "\n");
			}
	
		}
	
		vcard_free(v);
	}

	/* Display the form */
	output_headers(1, 1, 1, 0, 0, 0);

	do_template("box_begin_1");
	StrBufAppendBufPlain(WC->WBuf, _("Edit contact information"), -1, 0);
	do_template("box_begin_2");

	wc_printf("<form method=\"POST\" action=\"submit_vcard\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	if (force_room != NULL) {
		wc_printf("<input type=\"hidden\" name=\"force_room\" value=\"");
		escputs(force_room);
		wc_printf("\">\n");
	}
	else
	{
		wc_printf("<input type=\"hidden\" name=\"go\" value=\"");
		StrEscAppend(WCC->WBuf, WCC->CurRoom.name, NULL, 0, 0);
		wc_printf("\">\n");
	}

	wc_printf("<table class=\"vcard_edit_background\"><tr><td>\n");

	wc_printf("<table border=\"0\"><tr>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td>"
		"<td>%s</td></tr>\n",
		_("Prefix"), _("First Name"), _("Middle Name"), _("Last Name"), _("Suffix")
	);
	wc_printf("<tr><td><input type=\"text\" name=\"prefix\" "
		"value=\"%s\" maxlength=\"5\" size=\"5\"></td>",
		prefix);
	wc_printf("<td><input type=\"text\" name=\"firstname\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		firstname);
	wc_printf("<td><input type=\"text\" name=\"middlename\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		middlename);
	wc_printf("<td><input type=\"text\" name=\"lastname\" "
		"value=\"%s\" maxlength=\"29\"></td>",
		lastname);
	wc_printf("<td><input type=\"text\" name=\"suffix\" "
		"value=\"%s\" maxlength=\"10\" size=\"10\"></td></tr></table>\n",
		suffix);

	wc_printf("<table  class=\"vcard_edit_background_alt\">");
	wc_printf("<tr><td>");

	wc_printf(_("Display name:"));
	wc_printf("<br>"
		"<input type=\"text\" name=\"fullname\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		fullname
	);

	wc_printf(_("Title:"));
	wc_printf("<br>"
		"<input type=\"text\" name=\"title\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		title
	);

	wc_printf(_("Organization:"));
	wc_printf("<br>"
		"<input type=\"text\" name=\"org\" "
		"value=\"%s\" maxlength=\"40\"><br><br>\n",
		org
	);

	wc_printf("</td><td>");

	wc_printf("<table border=\"0\">");
	wc_printf("<tr><td>");
	wc_printf(_("PO box:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"pobox\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		pobox);
	wc_printf("<tr><td>");
	wc_printf(_("Address:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"extadr\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		extadr);
	wc_printf("<tr><td> </td><td>"
		"<input type=\"text\" name=\"street\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		street);
	wc_printf("<tr><td>");
	wc_printf(_("City:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"city\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		city);
	wc_printf("<tr><td>");
	wc_printf(_("State:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"state\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		state);
	wc_printf("<tr><td>");
	wc_printf(_("ZIP code:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"zipcode\" "
		"value=\"%s\" maxlength=\"10\"></td></tr>\n",
		zipcode);
	wc_printf("<tr><td>");
	wc_printf(_("Country:"));
	wc_printf("</td><td>"
		"<input type=\"text\" name=\"country\" "
		"value=\"%s\" maxlength=\"29\" width=\"5\"></td></tr>\n",
		country);
	wc_printf("</table>\n");

	wc_printf("</table>\n");

	wc_printf("<table border=0><tr><td>");
	wc_printf(_("Home telephone:"));
	wc_printf("</td>"
		"<td><input type=\"text\" name=\"hometel\" "
		"value=\"%s\" maxlength=\"29\"></td>\n",
		hometel);
	wc_printf("<td>");
	wc_printf(_("Work telephone:"));
	wc_printf("</td>"
		"<td><input type=\"text\" name=\"worktel\" "
		"value=\"%s\" maxlength=\"29\"></td></tr>\n",
		worktel);
	wc_printf("<tr><td>");
	wc_printf(_("Mobile telephone:"));
	wc_printf("</td>"
		"<td><input type=\"text\" name=\"mobiletel\" "
		"value=\"%s\" maxlength=\"29\"></td>\n",
		mobiletel);
	wc_printf("<td>");
	wc_printf(_("Fax number:"));
	wc_printf("</td>"
		"<td><input type=\"text\" name=\"faxtel\" "
		"value=\"%s\" maxlength=\"29\"></td></tr></table>\n",
		faxtel);

	wc_printf("<table class=\"vcard_edit_background_alt\">");
	wc_printf("<tr><td>");

	wc_printf("<table border=0><TR>"
		"<td valign=top>");
	wc_printf(_("Primary Internet e-mail address"));
	wc_printf("<br>"
		"<input type=\"text\" name=\"primary_inetemail\" "
		"size=40 maxlength=60 value=\"");
	escputs(primary_inetemail);
	wc_printf("\"><br>"
		"</td><td valign=top>");
	wc_printf(_("Internet e-mail aliases"));
	wc_printf("<br>"
		"<textarea name=\"other_inetemail\" rows=5 cols=40 width=40>");
	escputs(other_inetemail);
	wc_printf("</textarea></td></tr></table>\n");

	wc_printf("</td></tr></table>\n");

	wc_printf("<input type=\"hidden\" name=\"extrafields\" value=\"");
	escputs(extrafields);
	wc_printf("\">\n");

	wc_printf("<input type=\"hidden\" name=\"return_to\" value=\"");
	escputs(return_to);
	wc_printf("\">\n");

	wc_printf("<div class=\"buttons\">\n"
		"<input type=\"submit\" name=\"ok_button\" value=\"%s\">"
		"&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">"
		"</div></form>\n",
		_("Save changes"),
		_("Cancel")
	);
	
	wc_printf("</td></tr></table>\n");
	do_template("box_end");
	wDumpContent(1);
	if (Msg != NULL) {
		DestroyMessageSummary(Msg);
	}
}


/*
 *  commit the edits to the citadel server
 */
void edit_vcard(void) {
	long msgnum;
	char *partnum;

	msgnum = lbstr("msgnum");
	partnum = bstr("partnum");
	do_edit_vcard(msgnum, partnum, NULL, NULL, "", NULL);
}



/*
 *  parse edited vcard from the browser
 */
void submit_vcard(void) {
	struct vCard *v;
	char *serialized_vcard;
	char buf[SIZ];
	StrBuf *Buf;
	const StrBuf *ForceRoom;
	int i;

	if (!havebstr("ok_button")) { 
		readloop(readnew, eUseDefault);
		return;
	}

	if (havebstr("force_room")) {
		ForceRoom = sbstr("force_room");
		if (gotoroom(ForceRoom) != 200) {
			AppendImportantMessage(_("Unable to enter the room to save your message"), -1);
			AppendImportantMessage(HKEY(": "));
			AppendImportantMessage(SKEY(ForceRoom));
			AppendImportantMessage(HKEY("; "));
			AppendImportantMessage(_("Aborting."), -1);

			if (!strcmp(bstr("return_to"), "select_user_to_edit")) {
				select_user_to_edit(NULL);
			}
			else if (!strcmp(bstr("return_to"), "do_welcome")) {
				do_welcome();
			}
			else if (!IsEmptyStr(bstr("return_to"))) {
				http_redirect(bstr("return_to"));
			}
			else {
				readloop(readnew, eUseDefault);
			}
			return;
		}
	}

	Buf = NewStrBuf();
	serv_write(HKEY("ENT0 1|||4\n"));
	if (!StrBuf_ServGetln(Buf) && (GetServerStatus(Buf, NULL) != 4))
	{
		edit_vcard();
		return;
	}

	/* Make a vCard structure out of the data supplied in the form */
	StrBufPrintf(Buf, "begin:vcard\r\n%s\r\nend:vcard\r\n",
		     bstr("extrafields")
	);
	v = VCardLoad(Buf);	/* Start with the extra fields */
	if (v == NULL) {
		AppendImportantMessage(_("An error has occurred."), -1);
		edit_vcard();
		FreeStrBuf(&Buf);
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
		AppendImportantMessage(_("An error has occurred."), -1);
		edit_vcard();
		FreeStrBuf(&Buf);
		return;
	}

	serv_write(HKEY("Content-type: text/x-vcard; charset=UTF-8\n"));
	serv_write(HKEY("\n"));
	serv_printf("%s\r\n", serialized_vcard);
	serv_write(HKEY("000\n"));
	free(serialized_vcard);

	if (!strcmp(bstr("return_to"), "select_user_to_edit")) {
		select_user_to_edit(NULL);
	}
	else if (!strcmp(bstr("return_to"), "do_welcome")) {
		do_welcome();
	}
	else if (!IsEmptyStr(bstr("return_to"))) {
		http_redirect(bstr("return_to"));
	}
	else {
		readloop(readnew, eUseDefault);
	}
	FreeStrBuf(&Buf);
}



/*
 * Extract an embedded photo from a vCard for display on the client
 */
void display_vcard_photo_img(void)
{
	long msgnum = 0L;
	StrBuf *vcard;
	struct vCard *v;
	char *photosrc;
	const char *contentType;
	wcsession *WCC = WC;

	msgnum = StrBufExtract_long(WCC->Hdr->HR.ReqLine, 0, '/');
	
	vcard = load_mimepart(msgnum,"1");
	v = VCardLoad(vcard);
	
	photosrc = vcard_get_prop(v, "PHOTO", 1,0,0);
	FlushStrBuf(WCC->WBuf);
	StrBufAppendBufPlain(WCC->WBuf, photosrc, -1, 0);
	if (StrBufDecodeBase64(WCC->WBuf) <= 0) {
		FlushStrBuf(WCC->WBuf);
		
		hprintf("HTTP/1.1 500 %s\n","Unable to get photo");
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		begin_burst();
		wc_printf(_("Could Not decode vcard photo\n"));
		end_burst();
		return;
	}
	contentType = GuessMimeType(ChrPtr(WCC->WBuf), StrLength(WCC->WBuf));
	http_transmit_thing(contentType, 0);
	free(v);
	free(photosrc);
}

typedef struct _vcardview_struct {
	long is_singlecard;
	addrbookent *addrbook;
	long num_ab;

} vcardview_struct;

int vcard_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				 void **ViewSpecific, 
				 long oper, 
				 char *cmd, 
				 long len,
				 char *filter,
				 long flen)
{
	vcardview_struct *VS;

	VS = (vcardview_struct*) malloc (sizeof(vcardview_struct));
	memset(VS, 0, sizeof(vcardview_struct));
	*ViewSpecific = (void*)VS;

	VS->is_singlecard = ibstr("is_singlecard");
	if (VS->is_singlecard != 1) {
		if (oper == do_search) {
			snprintf(cmd, len, "MSGS SEARCH|%s", bstr("query"));
		}
		else {
			strcpy(cmd, "MSGS ALL");
		}
		Stat->maxmsgs = 9999999;
	}
	return 200;
}

int vcard_LoadMsgFromServer(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    message_summary* Msg, 
			    int is_new, 
			    int i)
{
	vcardview_struct *VS;
	char *ab_name;

	VS = (vcardview_struct*) *ViewSpecific;

	ab_name = NULL;
	fetch_ab_name(Msg, &ab_name);
	if (ab_name == NULL) 
		return 0;
	++VS->num_ab;
	VS->addrbook = realloc(VS->addrbook,
			       (sizeof(addrbookent) * VS->num_ab) );
	safestrncpy(VS->addrbook[VS->num_ab-1].ab_name, ab_name,
		    sizeof(VS->addrbook[VS->num_ab-1].ab_name));
	VS->addrbook[VS->num_ab-1].ab_msgnum = Msg->msgnum;
	free(ab_name);
	return 0;
}


int vcard_RenderView_or_Tail(SharedMessageStatus *Stat, void **ViewSpecific, long oper)
{
	const StrBuf *Mime;
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	if (VS->is_singlecard)
		read_message(WC->WBuf, HKEY("view_message"), lbstr("startmsg"), NULL, &Mime);
	else
		do_addrbook_view(VS->addrbook, VS->num_ab);	/* Render the address book */
	return 0;
}

int vcard_Cleanup(void **ViewSpecific)
{
	vcardview_struct *VS;

	VS = (vcardview_struct*) *ViewSpecific;
	wDumpContent(1);
	if ((VS != NULL) && 
	    (VS->addrbook != NULL))
		free(VS->addrbook);
	if (VS != NULL) 
		free(VS);
	return 0;
}

void 
ServerStartModule_VCARD
(void)
{
	VCToEnum = NewHash(0, NULL);

}

void 
ServerShutdownModule_VCARD
(void)
{
	DeleteHash(&VCToEnum);
}

void 
InitModule_VCARD
(void)
{
	RegisterCTX(CTX_VCARD);
	RegisterReadLoopHandlerset(
		VIEW_ADDRESSBOOK,
		vcard_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL, 
		vcard_LoadMsgFromServer,
		vcard_RenderView_or_Tail,
		vcard_Cleanup);
	WebcitAddUrlHandler(HKEY("edit_vcard"), "", 0, edit_vcard, 0);
	WebcitAddUrlHandler(HKEY("submit_vcard"), "", 0, submit_vcard, 0);
	WebcitAddUrlHandler(HKEY("vcardphoto"), "", 0, display_vcard_photo_img, NEED_URL);

	Put(VCToEnum, HKEY("n"), (void*)VC_n, reference_free_handler);
	Put(VCToEnum, HKEY("fn"), (void*)VC_fn, reference_free_handler);
	Put(VCToEnum, HKEY("title"), (void*)VC_title, reference_free_handler);
	Put(VCToEnum, HKEY("org"), (void*)VC_org, reference_free_handler);
	Put(VCToEnum, HKEY("email"), (void*)VC_email, reference_free_handler);
	Put(VCToEnum, HKEY("tel"), (void*)VC_tel, reference_free_handler);
	Put(VCToEnum, HKEY("tel_tel"), (void*)VC_tel_tel, reference_free_handler);
	Put(VCToEnum, HKEY("tel_work"), (void*)VC_tel_work, reference_free_handler);
	Put(VCToEnum, HKEY("tel_home"), (void*)VC_tel_home, reference_free_handler);
	Put(VCToEnum, HKEY("tel_cell"), (void*)VC_tel_cell, reference_free_handler);
	Put(VCToEnum, HKEY("adr"), (void*)VC_adr, reference_free_handler);
	Put(VCToEnum, HKEY("photo"), (void*)VC_photo, reference_free_handler);
	Put(VCToEnum, HKEY("version"), (void*)VC_version, reference_free_handler);
	Put(VCToEnum, HKEY("rev"), (void*)VC_rev, reference_free_handler);
	Put(VCToEnum, HKEY("label"), (void*)VC_label, reference_free_handler);


	RegisterNamespace("VC", 1, 2, tmplput_VCARD_ITEM, NULL, CTX_VCARD);

	REGISTERTokenParamDefine(VC_n);
	REGISTERTokenParamDefine(VC_fn);
	REGISTERTokenParamDefine(VC_title);
	REGISTERTokenParamDefine(VC_org);
	REGISTERTokenParamDefine(VC_email);
	REGISTERTokenParamDefine(VC_tel);
	REGISTERTokenParamDefine(VC_tel_tel);
	REGISTERTokenParamDefine(VC_tel_work);
	REGISTERTokenParamDefine(VC_tel_home);
	REGISTERTokenParamDefine(VC_tel_cell);
	REGISTERTokenParamDefine(VC_adr);
	REGISTERTokenParamDefine(VC_photo);
	REGISTERTokenParamDefine(VC_version);
	REGISTERTokenParamDefine(VC_rev);
	REGISTERTokenParamDefine(VC_label);

}

