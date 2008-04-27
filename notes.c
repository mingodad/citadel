/*
 * $Id$
 *
 */

#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"


/*
 * Uncomment this #define in order to get the new vNote-based sticky notes view.
 * We're keeping both versions here so that I can work on the new view without breaking
 * the existing implementation prior to completion.
 */

/* #define NEW_NOTES_VIEW */


#ifndef NEW_NOTES_VIEW
/*
 * display sticky notes
 *
 * msgnum = Message number on the local server of the note to be displayed
 */
void display_note(long msgnum, int unread)
{
	char buf[SIZ];
	char notetext[SIZ];
	char display_notetext[SIZ];
	char eid[128];
	int in_text = 0;
	int i, len;

	serv_printf("MSG0 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("%s<br />\n", &buf[4]);
		return;
	}

	strcpy(notetext, "");
	strcpy(eid, "");
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		/* Fill the buffer */
		if ( (in_text) && (strlen(notetext) < SIZ-256) ) {
			strcat(notetext, buf);
		}

		if ( (!in_text) && (!strncasecmp(buf, "exti=", 5)) ) {
			safestrncpy(eid, &buf[5], sizeof eid);
		}

		if ( (!in_text) && (!strcasecmp(buf, "text")) ) {
			in_text = 1;
		}
	}

	/* Now sanitize the buffer */
	len = strlen(notetext);
	for (i=0; i<len; ++i) {
		if (isspace(notetext[i])) notetext[i] = ' ';
	}

	/* Make it HTML-happy and print it. */
	stresc(display_notetext, SIZ, notetext, 0, 0);

	/* Lets try it as a draggable */
	if (!IsEmptyStr(eid)) {
		wprintf ("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\" id=\"note_%s\" alt=\"Note\" ", eid); 
		wprintf ("class=\"notes\">\n");
		wprintf ("<script type=\"text/javascript\">\n");
		wprintf ("new Draggable (\"note_%s\", {revert:true})\n", eid);
		wprintf ("</script>\n");
	}
	else {
		wprintf ("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\" id=\"note_%ld\" ", msgnum); 
		wprintf ("class=\"notes\">\n");
		wprintf ("<script type=\"text/javascript\">\n");
		wprintf ("new Draggable (\"note_%ld\", {revert:true})\n", msgnum);
		wprintf ("</script>\n");
	}
	
	if (!IsEmptyStr(eid)) {
		wprintf("<span id=\"note%s\">%s</span><br />\n", eid, display_notetext);
	}
	else {
		wprintf("<span id=\"note%ld\">%s</span><br />\n", msgnum, display_notetext);
	}

	/* Offer in-place editing. */
	if (!IsEmptyStr(eid)) {
		wprintf("<script type=\"text/javascript\">"
			"new Ajax.InPlaceEditor('note%s', 'updatenote?nonce=%ld?eid=%s', {rows:5,cols:72});"
			"</script>\n",
			eid,
			WC->nonce,
			eid
		);
	}
}
#endif


/*
 * This gets called by the Ajax.InPlaceEditor when we save a note.
 */
void updatenote(void)
{
	char buf[SIZ];
	char notetext[SIZ];
	char display_notetext[SIZ];
	long msgnum;
	int in_text = 0;
	int i, len;

	serv_printf("ENT0 1||0|0||||||%s", bstr("eid"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		text_to_server(bstr("value"));
		serv_puts("000");
	}

	begin_ajax_response();
	msgnum = locate_message_by_uid(bstr("eid"));
	if (msgnum >= 0L) {
		serv_printf("MSG0 %ld", msgnum);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			strcpy(notetext, "");
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		
				/* Fill the buffer */
				if ( (in_text) && (strlen(notetext) < SIZ-256) ) {
					strcat(notetext, buf);
				}
		
				if ( (!in_text) && (!strcasecmp(buf, "text")) ) {
					in_text = 1;
				}
			}
			/* Now sanitize the buffer */
			len = strlen(notetext);
			for (i=0; i<len; ++i) {
				if (isspace(notetext[i])) notetext[i] = ' ';
			}
		
			/* Make it HTML-happy and print it. */
			stresc(display_notetext, SIZ, notetext, 0, 0);
			wprintf("%s\n", display_notetext);
		}
	}
	else {
		wprintf("%s", _("An error has occurred."));
	}

	end_ajax_response();
}


#ifdef NEW_NOTES_VIEW

/*
 * Display a <div> containing a rendered sticky note.
 */
void display_vnote_div(struct vnote *v, long msgnum) {

	/* begin outer div */

	wprintf("<div id=\"note%ld\" ", msgnum);
	wprintf("class=\"stickynote_outer\" ");
	wprintf("style=\"");
	wprintf("left: %dpx; ", v->pos_left);
	wprintf("top: %dpx; ", v->pos_top);
	wprintf("width: %dpx; ", v->pos_width);
	wprintf("height: %dpx; ", v->pos_height);
	wprintf("background-color: #%02X%02X%02X ", v->color_red, v->color_green, v->color_blue);
	wprintf("\">");

	/* begin title bar */

	wprintf("<div id=\"titlebar%ld\" ", msgnum);
	wprintf("class=\"stickynote_titlebar\" ");
	wprintf("style=\"");
	wprintf("background-color: #%02X%02X%02X ", v->color_red/2, v->color_green/2, v->color_blue/2);
	wprintf("\">");

	wprintf("<table border=0 cellpadding=0 cellspacing=0 valign=middle width=100%%><tr>");
	wprintf("<td>&nbsp;</td>", msgnum);

	wprintf("<td align=right valign=middle "
		// "onclick=\"javascript:$('address_book_popup').style.display='none';\" "
		"><img src=\"static/closewindow.gif\">");
	wprintf("</td></tr></table>");

	wprintf("</div>\n");

	escputs(v->body);

	wprintf("</div>\n");
}




/*
 * display sticky notes
 *
 * msgnum = Message number on the local server of the note to be displayed
 */
void display_note(long msgnum, int unread) {
	char buf[1024];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	int mime_length;
	char relevant_partnum[256];
	char *relevant_source = NULL;

	relevant_partnum[0] = '\0';
	sprintf(buf, "MSG4 %ld", msgnum);	/* we need the mime headers */
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

			if (!strcasecmp(mime_content_type, "text/vnote")) {
				strcpy(relevant_partnum, mime_partnum);
			}
		}
	}

	if (!IsEmptyStr(relevant_partnum)) {
		relevant_source = load_mimepart(msgnum, relevant_partnum);
		if (relevant_source != NULL) {
			struct vnote *v = vnote_new_from_str(relevant_source);
			free(relevant_source);
			display_vnote_div(v, msgnum);
			vnote_free(v);

			/* FIXME remove these debugging messages when finished */
			wprintf("<script type=\"text/javascript\">");
			wprintf("document.write('L: ' + $('note%ld').style.left + '<br>');", msgnum);
			wprintf("document.write('T: ' + $('note%ld').style.top + '<br>');", msgnum);
			wprintf("document.write('W: ' + $('note%ld').style.width + '<br>');", msgnum);
			wprintf("document.write('H: ' + $('note%ld').style.height + '<br>');", msgnum);
			wprintf("</script>");
		}
	}
}

#endif
