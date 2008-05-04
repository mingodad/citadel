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





/*
 * Display a <div> containing a rendered sticky note.
 */
void display_vnote_div(struct vnote *v) {

	/* begin outer div */

	wprintf("<div id=\"note-%s\" ", v->uid);
	wprintf("class=\"stickynote_outer\" ");
	wprintf("style=\"");
	wprintf("left: %dpx; ", v->pos_left);
	wprintf("top: %dpx; ", v->pos_top);
	wprintf("width: %dpx; ", v->pos_width);
	wprintf("height: %dpx; ", v->pos_height);
	wprintf("background-color: #%02X%02X%02X ", v->color_red, v->color_green, v->color_blue);
	wprintf("\">");

	/* begin title bar */

	wprintf("<div id=\"titlebar-%s\" ", v->uid);
	wprintf("class=\"stickynote_titlebar\" ");
	wprintf("onMouseDown=\"NotesDragMouseDown(event,'%s')\" ", v->uid);
	wprintf("style=\"");
	wprintf("background-color: #%02X%02X%02X ", v->color_red/2, v->color_green/2, v->color_blue/2);
	wprintf("\">");

	wprintf("<table border=0 cellpadding=0 cellspacing=0 valign=middle width=100%%><tr>");
	wprintf("<td></td>");	// nothing in the title bar, it's just for dragging

	wprintf("<td align=right valign=middle "
		// "onclick=\"javascript:$('address_book_popup').style.display='none';\" "
		"><img src=\"static/closewindow.gif\">");
	wprintf("</td></tr></table>");

	wprintf("</div>\n");

	/* begin note body */

	wprintf("<div id=\"notebody-%s\" ", v->uid);
	wprintf("class=\"stickynote_body\"");
	wprintf(">");
	escputs(v->body);
	wprintf("</div>\n");

	wprintf("<script type=\"text/javascript\">");
	wprintf(" new Ajax.InPlaceEditor('notebody-%s', 'ajax_update_note?note_uid=%s', "
		"{rows:%d,cols:%d,highlightcolor:'#%02X%02X%02X',highlightendcolor:'#%02X%02X%02X',"
		"okText:'%s',cancelText:'%s',clickToEditText:'%s'});",
		v->uid,
		v->uid,
		(v->pos_height / 16) - 5,
		(v->pos_width / 9) - 1,
		v->color_red, v->color_green, v->color_blue,
		v->color_red, v->color_green, v->color_blue,
		_("Save"),
		_("Cancel"),
		_("Click on any note to edit it.")
	);
	wprintf("</script>\n");

	/* begin resize handle */

	wprintf("<div id=\"resize-%s\" ", v->uid);
	wprintf("class=\"stickynote_resize\" ");
	wprintf("onMouseDown=\"NotesResizeMouseDown(event,'%s')\"", v->uid);
	wprintf(">");

	wprintf("<img src=\"static/resizecorner.png\">");

	wprintf("</div>\n");

	/* end of note */

	wprintf("</div>\n");
}



/*
 * Fetch a message from the server and extract a vNote from it
 */
struct vnote *vnote_new_from_msg(long msgnum) {
	char buf[1024];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	int mime_length;
	char relevant_partnum[256];
	char *relevant_source = NULL;
	char uid_from_headers[256];
	int in_body = 0;
	int body_line_len = 0;
	int body_len = 0;
	struct vnote *vnote_from_body = NULL;

	relevant_partnum[0] = 0;
	uid_from_headers[0] = 0;
	sprintf(buf, "MSG4 %ld", msgnum);	/* we need the mime headers */
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return NULL;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "exti=", 5)) {
			safestrncpy(uid_from_headers, &buf[5], sizeof uid_from_headers);
		}
		else if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
			mime_length = extract_int(&buf[5], 5);

			if (!strcasecmp(mime_content_type, "text/vnote")) {
				strcpy(relevant_partnum, mime_partnum);
			}
		}
		else if ((in_body) && (IsEmptyStr(relevant_partnum)) && (!IsEmptyStr(uid_from_headers))) {
			// Convert an old-style note to a vNote
			if (!vnote_from_body) {
				vnote_from_body = vnote_new();
				vnote_from_body->uid = strdup(uid_from_headers);
				vnote_from_body->body = malloc(32768);
				vnote_from_body->body[0] = 0;
				body_len = 0;
			}
			body_line_len = strlen(buf);
			if ((body_len + body_line_len + 10) < 32768) {
				strcpy(&vnote_from_body->body[body_len++], " ");
				strcpy(&vnote_from_body->body[body_len], buf);
				body_len += body_line_len;
			}
		}
		else if (IsEmptyStr(buf)) {
			in_body = 1;
		}
	}

	if (!IsEmptyStr(relevant_partnum)) {
		relevant_source = load_mimepart(msgnum, relevant_partnum);
		if (relevant_source != NULL) {
			struct vnote *v = vnote_new_from_str(relevant_source);
			free(relevant_source);
			return(v);
		}
	}

	if (vnote_from_body) {
		return(vnote_from_body);
	}
	return NULL;
}


/*
 * Background ajax call to receive updates from the browser when a note is moved, resized, or updated.
 */
void ajax_update_note(void) {

	char buf[1024];
	int msgnum;
	struct vnote *v = NULL;

        if (!havebstr("note_uid")) {
		begin_ajax_response();
		wprintf("Received ajax_update_note() request without a note UID.");
		end_ajax_response();
		return;
	}

	lprintf(9, "Note UID = %s\n", bstr("note_uid"));
	serv_printf("EUID %s", bstr("note_uid"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		begin_ajax_response();
		wprintf("Cannot find message containing vNote with the requested uid!");
		end_ajax_response();
		return;
	}
	msgnum = atol(&buf[4]);
	v = vnote_new_from_msg(msgnum);
	if (!v) {
		begin_ajax_response();
		wprintf("Cannot locate a vNote within message %ld\n", msgnum);
		end_ajax_response();
		return;
	}

	/* Make any requested changes */
        if (havebstr("top")) {
		v->pos_top = atoi(bstr("top"));
	}
        if (havebstr("left")) {
		v->pos_left = atoi(bstr("left"));
	}
        if (havebstr("height")) {
		v->pos_height = atoi(bstr("height"));
	}
        if (havebstr("width")) {
		v->pos_width = atoi(bstr("width"));
	}
        if (havebstr("value")) {	// I would have preferred 'body' but InPlaceEditor hardcodes 'value'
		if (v->body) free(v->body);
		v->body = strdup(bstr("value"));
	}

	/* Serialize it and save it to the message base.  Server will delete the old one. */
	serv_puts("ENT0 1|||4");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		serv_puts("Content-type: text/vnote");
		serv_puts("");
		serv_puts(vnote_serialize(v));
		serv_puts("000");
	}

	begin_ajax_response();
	if (v->body) {
		escputs(v->body);
	}
	end_ajax_response();

	vnote_free(v);
}






#ifdef NEW_NOTES_VIEW

/*
 * display sticky notes
 *
 * msgnum = Message number on the local server of the note to be displayed
 */
void display_note(long msgnum, int unread) {
	struct vnote *v;

	v = vnote_new_from_msg(msgnum);
	if (v) {
		display_vnote_div(v);

		/* uncomment these lines to see ugly debugging info 
		wprintf("<script type=\"text/javascript\">");
		wprintf("document.write('L: ' + $('note-%s').style.left + '; ');", v->uid);
		wprintf("document.write('T: ' + $('note-%s').style.top + '; ');", v->uid);
		wprintf("document.write('W: ' + $('note-%s').style.width + '; ');", v->uid);
		wprintf("document.write('H: ' + $('note-%s').style.height + '<br>');", v->uid);
		wprintf("</script>");
		*/

		vnote_free(v);
	}
}

#endif
