/*
 * $Id$
 *
 */

#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"

int pastel_palette[9][3] = {
	{ 0x80, 0x80, 0x80 },
	{ 0xff, 0x80, 0x80 },
	{ 0x80, 0x80, 0xff },
	{ 0xff, 0xff, 0x80 },
	{ 0x80, 0xff, 0x80 },
	{ 0xff, 0x80, 0xff },
	{ 0x80, 0xff, 0xff },
	{ 0xff, 0x80, 0x80 },
	{ 0x80, 0x80, 0x80 }
};


/*
 * Display a <div> containing a rendered sticky note.
 */
void display_vnote_div(struct vnote *v) {
	int i;


	wprintf("<div id=\"note-%s\" ", v->uid);	// begin outer div
	wprintf("class=\"stickynote_outer\" ");
	wprintf("style=\"");
	wprintf("left: %dpx; ", v->pos_left);
	wprintf("top: %dpx; ", v->pos_top);
	wprintf("width: %dpx; ", v->pos_width);
	wprintf("height: %dpx; ", v->pos_height);
	wprintf("background-color: #%02X%02X%02X ", v->color_red, v->color_green, v->color_blue);
	wprintf("\">");





	wprintf("<div id=\"titlebar-%s\" ", v->uid);	// begin title bar div
	wprintf("class=\"stickynote_titlebar\" ");
	wprintf("onMouseDown=\"NotesDragMouseDown(event,'%s')\" ", v->uid);
	wprintf("style=\"");
	wprintf("background-color: #%02X%02X%02X ", v->color_red/2, v->color_green/2, v->color_blue/2);
	wprintf("\">");

	wprintf("<table border=0 cellpadding=0 cellspacing=0 valign=middle width=100%%><tr>");

	wprintf("<td align=left valign=middle>");
	wprintf("<img onclick=\"NotesClickPalette(event,'%s')\" ", v->uid);
	wprintf("src=\"static/8paint16.gif\">");

	wprintf("</td>");

	wprintf("<td></td>");	// nothing in the title bar, it's just for dragging

	wprintf("<td align=right valign=middle>");
	wprintf("<img onclick=\"DeleteStickyNote(event,'%s','%s')\" ", v->uid, _("Delete this note?") );
	wprintf("src=\"static/closewindow.gif\">");
	wprintf("</td></tr></table>");

	wprintf("</div>\n");				// end title bar div





	wprintf("<div id=\"notebody-%s\" ", v->uid);	// begin body div
	wprintf("class=\"stickynote_body\"");
	wprintf(">");
	escputs(v->body);
	wprintf("</div>\n");				// end body div

        StrBufAppendPrintf(WC->trailing_javascript,
		" new Ajax.InPlaceEditor('notebody-%s', 'ajax_update_note?note_uid=%s', "
		"{rows:%d,cols:%d,onEnterHover:false,onLeaveHover:false,"
		"okText:'%s',cancelText:'%s',clickToEditText:'%s'});",
		v->uid,
		v->uid,
		(v->pos_height / 16) - 5,
		(v->pos_width / 9) - 1,
		_("Save"),
		_("Cancel"),
		_("Click on any note to edit it.")
	);

	wprintf("<div id=\"resize-%s\" ", v->uid);	// begin resize handle div
	wprintf("class=\"stickynote_resize\" ");
	wprintf("onMouseDown=\"NotesResizeMouseDown(event,'%s')\"", v->uid);
	wprintf("> </div>");				// end resize handle div




	/* embed color selector - it doesn't have to be inside the title bar html because
	 * it's a separate div placed by css
	 */
	wprintf("<div id=\"palette-%s\" ", v->uid);	// begin stickynote_palette div
	wprintf("class=\"stickynote_palette\">");

	wprintf("<table border=0 cellpadding=0 cellspacing=0>");
	for (i=0; i<9; ++i) {
		if ((i%3)==0) wprintf("<tr>");
		wprintf("<td ");
		wprintf("onClick=\"NotesClickColor(event,'%s',%d,%d,%d,'#%02x%02x%02x','#%02x%02x%02x')\" ",
			v->uid,
			pastel_palette[i][0],		// color values to pass to ajax call
			pastel_palette[i][1],
			pastel_palette[i][2],
			pastel_palette[i][0],		// new color of note
			pastel_palette[i][1],
			pastel_palette[i][2],
			pastel_palette[i][0] / 2,	// new color of title bar
			pastel_palette[i][1] / 2,
			pastel_palette[i][2] / 2
		);
		wprintf("bgcolor=\"#%02x%02x%02x\"> </td>",
			pastel_palette[i][0],
			pastel_palette[i][1],
			pastel_palette[i][2]
		);
		if (((i+1)%3)==0) wprintf("</tr>");
	}
	wprintf("</table>");

	wprintf("</div>");				// end stickynote_palette div





	wprintf("</div>\n");				// end outer div
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
				vnote_from_body->color_red = pastel_palette[3][0];
				vnote_from_body->color_green = pastel_palette[3][1];
				vnote_from_body->color_blue = pastel_palette[3][2];
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
 * Serialize a vnote and write it to the server
 */
void write_vnote_to_server(struct vnote *v) 
{
	char buf[1024];
	char *pch;

	serv_puts("ENT0 1|||4");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		serv_puts("Content-type: text/vnote");
		serv_puts("");
		pch = vnote_serialize(v);
		serv_puts(pch);
		free(pch);
		serv_puts("000");
	}
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

	// lprintf(9, "Note UID = %s\n", bstr("note_uid"));
	serv_printf("EUID %s", bstr("note_uid"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		begin_ajax_response();
		wprintf("Cannot find message containing vNote with the requested uid!");
		end_ajax_response();
		return;
	}
	msgnum = atol(&buf[4]);
	// lprintf(9, "Note msg = %ld\n", msgnum);

	// Was this request a delete operation?  If so, nuke it...
	if (havebstr("deletenote")) {
		if (!strcasecmp(bstr("deletenote"), "yes")) {
			serv_printf("DELE %d", msgnum);
			serv_getln(buf, sizeof buf);
			begin_ajax_response();
			wprintf("%s", buf);
			end_ajax_response();
			return;
		}
	}

	// If we get to this point it's an update, not a delete
	v = vnote_new_from_msg(msgnum);
	if (!v) {
		begin_ajax_response();
		wprintf("Cannot locate a vNote within message %d\n", msgnum);
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
        if (havebstr("red")) {
		v->color_red = atoi(bstr("red"));
	}
        if (havebstr("green")) {
		v->color_green = atoi(bstr("green"));
	}
        if (havebstr("blue")) {
		v->color_blue = atoi(bstr("blue"));
	}
        if (havebstr("value")) {	// I would have preferred 'body' but InPlaceEditor hardcodes 'value'
		if (v->body) free(v->body);
		v->body = strdup(bstr("value"));
	}

	/* Serialize it and save it to the message base.  Server will delete the old one. */
	write_vnote_to_server(v);

	begin_ajax_response();
	if (v->body) {
		escputs(v->body);
	}
	end_ajax_response();

	vnote_free(v);
}




/*
 * display sticky notes
 *
 * msgnum = Message number on the local server of the note to be displayed
 */
void display_note(long msgnum, int unread) {
	struct vnote *v;

	v = vnote_new_from_msg(msgnum);
	if (v) {
//		display_vnote_div(v);
		DoTemplate(HKEY("vnoteitem"),
			   WC->WBuf, v, CTX_VNOTE);
			

		/* uncomment these lines to see ugly debugging info 
		StrBufAppendPrintf(WC->trailing_javascript,
			"document.write('L: ' + $('note-%s').style.left + '; ');", v->uid);
		StrBufAppendPrintf(WC->trailing_javascript,
			"document.write('T: ' + $('note-%s').style.top + '; ');", v->uid);
		StrBufAppendPrintf(WC->trailing_javascript,
			"document.write('W: ' + $('note-%s').style.width + '; ');", v->uid);
		StrBufAppendPrintf(WC->trailing_javascript,
			"document.write('H: ' + $('note-%s').style.height + '<br>');", v->uid);
		*/

		vnote_free(v);
	}
}



/*
 * Create a new note
 */
void add_new_note(void) {
	struct vnote *v;

	v = vnote_new();
	if (v) {
		v->uid = malloc(128);
		generate_uuid(v->uid);
		v->color_red = pastel_palette[3][0];
		v->color_green = pastel_palette[3][1];
		v->color_blue = pastel_palette[3][2];
		v->body = strdup(_("Click on any note to edit it."));
		write_vnote_to_server(v);
		vnote_free(v);
	}
	
	readloop("readfwd");
}


void tmpl_vcard_put_posleft(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", v->pos_left);
}

void tmpl_vcard_put_postop(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", v->pos_top);
}

void tmpl_vcard_put_poswidth(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", v->pos_width);
}

void tmpl_vcard_put_posheight(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", v->pos_height);
}

void tmpl_vcard_put_posheight2(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", (v->pos_height / 16) - 5);
}

void tmpl_vcard_put_width2(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%d", (v->pos_width / 9) - 1);
}

void tmpl_vcard_put_color(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%02X%02X%02X", v->color_red, v->color_green, v->color_blue);
}

void tmpl_vcard_put_bgcolor(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendPrintf(Target, "%02X%02X%02X", v->color_red/2, v->color_green/2, v->color_blue/2);
}

void tmpl_vcard_put_message(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrEscAppend(Target, NULL, v->body, 0, 0); ///TODO?
}

void tmpl_vcard_put_uid(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType)
{
	struct vnote *v = (struct vnote *) Context;
	StrBufAppendBufPlain(Target, v->uid, -1, 0);
}

void 
InitModule_NOTES
(void)
{
	WebcitAddUrlHandler(HKEY("add_new_note"), add_new_note, 0);
	WebcitAddUrlHandler(HKEY("ajax_update_note"), ajax_update_note, 0);

	RegisterNamespace("VNOTE:POS:LEFT", 0, 0, tmpl_vcard_put_posleft, CTX_VNOTE);
	RegisterNamespace("VNOTE:POS:TOP", 0, 0, tmpl_vcard_put_postop, CTX_VNOTE);
	RegisterNamespace("VNOTE:POS:WIDTH", 0, 0, tmpl_vcard_put_poswidth, CTX_VNOTE);
	RegisterNamespace("VNOTE:POS:HEIGHT", 0, 0, tmpl_vcard_put_posheight, CTX_VNOTE);
	RegisterNamespace("VNOTE:POS:HEIGHT2", 0, 0, tmpl_vcard_put_posheight2, CTX_VNOTE);
	RegisterNamespace("VNOTE:POS:WIDTH2", 0, 0, tmpl_vcard_put_width2, CTX_VNOTE);
	RegisterNamespace("VNOTE:COLOR", 0, 0, tmpl_vcard_put_color, CTX_VNOTE);
	RegisterNamespace("VNOTE:BGCOLOR", 0, 0,tmpl_vcard_put_bgcolor, CTX_VNOTE);
	RegisterNamespace("VNOTE:MSG", 0, 1, tmpl_vcard_put_message, CTX_VNOTE);
	RegisterNamespace("VNOTE:UID", 0, 0, tmpl_vcard_put_uid, CTX_VNOTE);
}
