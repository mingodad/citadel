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
 * Fetch a message from the server and extract a vNote from it
 */
struct vnote *vnote_new_from_msg(long msgnum,int unread) 
{
	StrBuf *Buf;
	StrBuf *Data = NULL;
	const char *bptr;
	int Done = 0;
	char uid_from_headers[256];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	int mime_length;
	char relevant_partnum[256];
	int phase = 0;				/* 0 = citadel headers, 1 = mime headers, 2 = body */
	char msg4_content_type[256] = "";
	char msg4_content_encoding[256] = "";
	int msg4_content_length = 0;
	struct vnote *vnote_from_body = NULL;
	int vnote_inline = 0;			/* 1 = MSG4 gave us a text/x-vnote top level */

	relevant_partnum[0] = '\0';
	serv_printf("MSG4 %ld", msgnum);	/* we need the mime headers */
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		FreeStrBuf (&Buf);
		return NULL;
	}
	while ((StrBuf_ServGetln(Buf)>=0) && !Done) {
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			Done = 1;
			break;
		}
		bptr = ChrPtr(Buf);
		switch (phase) {
		case 0:
			if (!strncasecmp(bptr, "exti=", 5)) {
				safestrncpy(uid_from_headers, &(ChrPtr(Buf)[5]), sizeof uid_from_headers);
			}
			else if (!strncasecmp(bptr, "part=", 5)) {
				extract_token(mime_filename, &bptr[5], 1, '|', sizeof mime_filename);
				extract_token(mime_partnum, &bptr[5], 2, '|', sizeof mime_partnum);
				extract_token(mime_disposition, &bptr[5], 3, '|', sizeof mime_disposition);
				extract_token(mime_content_type, &bptr[5], 4, '|', sizeof mime_content_type);
				mime_length = extract_int(&bptr[5], 5);

				if (!strcasecmp(mime_content_type, "text/vnote")) {
					strcpy(relevant_partnum, mime_partnum);
				}
			}
			else if ((phase == 0) && (!strncasecmp(bptr, "text", 4))) {
				phase = 1;
			}
		break;
		case 1:
			if (!IsEmptyStr(bptr)) {
				if (!strncasecmp(bptr, "Content-type: ", 14)) {
					safestrncpy(msg4_content_type, &bptr[14], sizeof msg4_content_type);
					striplt(msg4_content_type);
				}
				else if (!strncasecmp(bptr, "Content-transfer-encoding: ", 27)) {
					safestrncpy(msg4_content_encoding, &bptr[27], sizeof msg4_content_encoding);
					striplt(msg4_content_type);
				}
				else if ((!strncasecmp(bptr, "Content-length: ", 16))) {
					msg4_content_length = atoi(&bptr[16]);
				}
				break;
			}
			else {
				phase++;
				if ((msg4_content_length > 0)
				    && ( !strcasecmp(msg4_content_encoding, "7bit"))
				    && (!strcasecmp(msg4_content_type, "text/vnote"))
				) { 
					vnote_inline = 1;
				}
			}
		case 2:
			if (vnote_inline) {
				Data = NewStrBufPlain(NULL, msg4_content_length * 2);
				if (msg4_content_length > 0) {
					StrBuf_ServGetBLOBBuffered(Data, msg4_content_length);
					phase ++;
				}
				else {
					StrBufAppendBuf(Data, Buf, 0);
					StrBufAppendBufPlain(Data, "\r\n", 1, 0);
				}
			}
		case 3:
			if (vnote_inline) {
				StrBufAppendBuf(Data, Buf, 0);
			}
		}
	}
	FreeStrBuf(&Buf);

	/* If MSG4 didn't give us the part we wanted, but we know that we can find it
	 * as one of the other MIME parts, attempt to load it now.
	 */
	if ((!vnote_inline) && (!IsEmptyStr(relevant_partnum))) {
		Data = load_mimepart(msgnum, relevant_partnum);
	}

	if (StrLength(Data) > 0) {
		if (IsEmptyStr(uid_from_headers)) {
			/* Convert an old-style note to a vNote */
			vnote_from_body = vnote_new();
			vnote_from_body->uid = strdup(uid_from_headers);
			vnote_from_body->color_red = pastel_palette[3][0];
			vnote_from_body->color_green = pastel_palette[3][1];
			vnote_from_body->color_blue = pastel_palette[3][2];
			vnote_from_body->body = malloc(StrLength(Data) + 1);
			vnote_from_body->body[0] = 0;
			memcpy(vnote_from_body->body, ChrPtr(Data), StrLength(Data) + 1);
			FreeStrBuf(&Data);
			return vnote_from_body;
		}
		else {
			char *Buf = SmashStrBuf(&Data);
			
			struct vnote *v = vnote_new_from_str(Buf);
			free(Buf);
			return(v);
		}
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
	char boundary[256];
	static int seq = 0;

	snprintf(boundary, sizeof boundary, "Citadel--Multipart--%s--%04x--%04x",
		ChrPtr(WC->serv_info->serv_fqdn),
		getpid(),
		++seq
	);

	serv_puts("ENT0 1|||4");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		/* Remember, serv_printf() appends an extra newline */
		serv_printf("Content-type: multipart/alternative; "
			"boundary=\"%s\"\n", boundary);
		serv_printf("This is a multipart message in MIME format.\n");
		serv_printf("--%s", boundary);
	
		serv_puts("Content-type: text/plain; charset=utf-8");
		serv_puts("Content-Transfer-Encoding: 7bit");
		serv_puts("");
		serv_puts(v->body);
		serv_puts("");
	
		serv_printf("--%s", boundary);
		serv_puts("Content-type: text/vnote");
		serv_puts("Content-Transfer-Encoding: 7bit");
		serv_puts("");
		pch = vnote_serialize(v);
		serv_puts(pch);
		free(pch);
		serv_printf("--%s--", boundary);
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

	serv_printf("EUID %s", bstr("note_uid"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		begin_ajax_response();
		wprintf("Cannot find message containing vNote with the requested uid!");
		end_ajax_response();
		return;
	}
	msgnum = atol(&buf[4]);
	
	/* Was this request a delete operation?  If so, nuke it... */
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

	/* If we get to this point it's an update, not a delete */
	v = vnote_new_from_msg(msgnum, 0);
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
        if (havebstr("value")) {	/* I would have preferred 'body' but InPlaceEditor hardcodes 'value' */
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
////TODO: falscher hook
int notes_LoadMsgFromServer(SharedMessageStatus *Stat, 
			    void **ViewSpecific, 
			    message_summary* Msg, 
			    int is_new, 
			    int i)
{
	struct vnote *v;
	WCTemplputParams TP;

	memset(&TP, 0, sizeof(WCTemplputParams));
	TP.Filter.ContextType = CTX_VNOTE;
	v = vnote_new_from_msg(Msg->msgnum, is_new);
	if (v) {
		TP.Context = v;
		DoTemplate(HKEY("vnoteitem"),
			   WC->WBuf, &TP);
			

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
	return 0;
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
	
	readloop(readfwd);
}


void tmpl_vcard_put_posleft(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", v->pos_left);
}

void tmpl_vcard_put_postop(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", v->pos_top);
}

void tmpl_vcard_put_poswidth(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", v->pos_width);
}

void tmpl_vcard_put_posheight(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", v->pos_height);
}

void tmpl_vcard_put_posheight2(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", (v->pos_height / 16) - 5);
}

void tmpl_vcard_put_width2(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%d", (v->pos_width / 9) - 1);
}

void tmpl_vcard_put_color(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%02X%02X%02X", v->color_red, v->color_green, v->color_blue);
}

void tmpl_vcard_put_bgcolor(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendPrintf(Target, "%02X%02X%02X", v->color_red/2, v->color_green/2, v->color_blue/2);
}

void tmpl_vcard_put_message(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrEscAppend(Target, NULL, v->body, 0, 0); ///TODO?
}

void tmpl_vcard_put_uid(StrBuf *Target, WCTemplputParams *TP)
{
	struct vnote *v = (struct vnote *) CTX;
	StrBufAppendBufPlain(Target, v->uid, -1, 0);
}




int notes_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				 void **ViewSpecific, 
				 long oper, 
				 char *cmd, 
				 long len)
{
	strcpy(cmd, "MSGS ALL");
	Stat->maxmsgs = 32767;
	wprintf("<div id=\"new_notes_here\"></div>\n");
	return 200;

}

int notes_Cleanup(void **ViewSpecific)
{
	end_burst();
	return 0;
}


void 
InitModule_NOTES
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_NOTES,
		notes_GetParamsGetServerCall,
		NULL,
		notes_LoadMsgFromServer,
		NULL,
		notes_Cleanup);

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
