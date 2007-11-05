/*
 * $Id$
 */
/**
 * \defgroup StickyNotes Functions which handle "sticky notes"
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"
#include "groupdav.h"
#include "webserver.h"

/**
 * \brief display sticky notes
 * \param msgnum the citadel mesage number
 */
void display_note(long msgnum)
{
	char buf[SIZ];
	char notetext[SIZ];
	char display_notetext[SIZ];
	char eid[128];
	int in_text = 0;
	int i, len;

//	wprintf("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\">\n");
	serv_printf("MSG0 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("%s<br />\n", &buf[4]);
		return;
	}

	strcpy(notetext, "");
	strcpy(eid, "");
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		/** Fill the buffer */
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

	/** Now sanitize the buffer */
	len = strlen(notetext);
	for (i=0; i<len; ++i) {
		if (isspace(notetext[i])) notetext[i] = ' ';
	}

	/** Make it HTML-happy and print it. */
	stresc(display_notetext, SIZ, notetext, 0, 0);
/* Lets try it as a draggable */
	if (!IsEmptyStr(eid)) {
		wprintf ("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\" id=\"note_%s\" alt=\"Note\" ", eid); 
		wprintf ("class=\"notes\">\n");
		wprintf ("<script type=\"text/javascript\">\n");
//		wprintf ("//<![CDATA[\n");
		wprintf ("new Draggable (\"note_%s\", {revert:true})\n", eid);
//		wprintf ("//]]>\n");
		wprintf ("</script>\n");
	}
	else {
		wprintf ("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\" id=\"note_%s\" ", msgnum); 
		wprintf ("class=\"notes\">\n");
		wprintf ("<script type=\"text/javascript\">\n");
//		wprintf ("//<![CDATA[\n");
		wprintf ("new Draggable (\"note_%s\", {revert:true})\n", msgnum);
//		wprintf ("//]]>\n");
		wprintf ("</script>\n");
	}
	
	if (!IsEmptyStr(eid)) {
		wprintf("<span id=\"note%s\">%s</span><br />\n", eid, display_notetext);
	}
	else {
		wprintf("<span id=\"note%ld\">%s</span><br />\n", msgnum, display_notetext);
	}

	/** Offer in-place editing. */
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


/**
 * \brief  This gets called by the Ajax.InPlaceEditor when we save a note.
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
		
				/** Fill the buffer */
				if ( (in_text) && (strlen(notetext) < SIZ-256) ) {
					strcat(notetext, buf);
				}
		
				if ( (!in_text) && (!strcasecmp(buf, "text")) ) {
					in_text = 1;
				}
			}
			/** Now sanitize the buffer */
			len = strlen(notetext);
			for (i=0; i<len; ++i) {
				if (isspace(notetext[i])) notetext[i] = ' ';
			}
		
			/** Make it HTML-happy and print it. */
			stresc(display_notetext, SIZ, notetext, 0, 0);
			wprintf("%s\n", display_notetext);
		}
	}
	else {
		wprintf("%s", _("An error has occurred."));
	}

	end_ajax_response();
}



/*@}*/
