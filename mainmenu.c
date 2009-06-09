/*
 * $Id$
 */

#include "webcit.h"

/*
 * The Main Menu
 */
void display_main_menu(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("display_main_menu"), NULL, &NoCtx);
	end_burst();
}


/*
 * System administration menu
 */
void display_aide_menu(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("display_aide_menu"), NULL, &NoCtx);
	end_burst();
}



/*
 * Display the screen to enter a generic server command
 */
void display_generic(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Enter a server command"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"mainmenu_background\"><tr><td>\n");

	wprintf("<center>");
	wprintf(_("This screen allows you to enter Citadel server commands which are "
		"not supported by WebCit.  If you do not know what that means, "
		"then this screen will not be of much use to you."));
	wprintf("<br />\n");

	wprintf("<form method=\"post\" action=\"do_generic\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf(_("Enter command:"));
	wprintf("<br /><input type=\"text\" name=\"g_cmd\" size=80 maxlength=\"250\"><br />\n");

	wprintf(_("Command input (if requesting SEND_LISTING transfer mode):"));
	wprintf("<br /><textarea name=\"g_input\" rows=10 cols=80 width=80></textarea><br />\n");

	wprintf("<font size=-2>");
	wprintf(_("Detected host header is %s://%s"), (is_https ? "https" : "http"), ChrPtr(WC->Hdr->HR.http_host));
	wprintf("</font>\n");
	wprintf("<input type=\"submit\" name=\"sc_button\" value=\"%s\">", _("Send command"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\"><br />\n", _("Cancel"));

	wprintf("</form></center>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/*
 * Interactive window to perform generic Citadel server commands.
 */
void do_generic(void)
{

	wcsession *WCC = WC;
	int Done = 0;
	StrBuf *Buf;
	char *junk;
	size_t len;

	if (!havebstr("sc_button")) {
		display_main_menu();
		return;
	}

	output_headers(1, 1, 0, 0, 0, 0);
	Buf = NewStrBuf();
	serv_puts(bstr("g_cmd"));
	StrBuf_ServGetln(Buf);
	svput("BOXTITLE", WCS_STRING, _("Server command results"));
	do_template("beginboxx", NULL);

	wprintf("<table border=0><tr><td>Command:</td><td><tt>");
	StrEscAppend(WCC->WBuf, sbstr("g_cmd"), NULL, 0, 0);
	wprintf("</tt></td></tr><tr><td>Result:</td><td><tt>");
	StrEscAppend(WCC->WBuf, Buf, NULL, 0, 0);
	StrBufAppendBufPlain(WCC->WBuf, HKEY("<br>\n"), 0);
	wprintf("</tt></td></tr></table><br />\n");
	
	switch (GetServerStatus(Buf, NULL)) {
	case 8:
		serv_puts("\n\n000");
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			StrBufAppendBufPlain(WCC->WBuf, HKEY("\000"), 0);
			break;
		}
	case 1:
		while (!Done) {
			StrBuf_ServGetln(Buf);
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) {
				Done = 1;
			}
			StrEscAppend(WCC->WBuf, Buf, NULL, 0, 0);
			StrBufAppendBufPlain(WCC->WBuf, HKEY("<br>\n"), 0);
		}
		break;
	case 4:
		text_to_server(bstr("g_input"));
		serv_puts("000");
		break;
	case 6:
		len = atol(&ChrPtr(Buf)[4]);
		StrBuf_ServGetBLOBBuffered(Buf, len);
		break;
	case 7:
		len = atol(&ChrPtr(Buf)[4]);
		junk = malloc(len);
		memset(junk, 0, len);
		serv_write(junk, len);
		free(junk);
	}
	
	wprintf("<hr />");
	wprintf("<a href=\"display_generic\">Enter another command</a><br />\n");
	wprintf("<a href=\"display_advanced\">Return to menu</a>\n");
	do_template("endbox", NULL);
	FreeStrBuf(&Buf);
	wDumpContent(1);
}


/*
 * Display the menubar.  
 *
 * Set 'as_single_page' to display HTML headers and footers -- otherwise it's assumed
 * that the menubar is being embedded in another page.
 */
void display_menubar(int as_single_page) {

	if (as_single_page) {
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("<html>\n"
			"<head>\n"
			"<title>MenuBar</title>\n"
			"<style type=\"text/css\">\n"
			"body	{ text-decoration: none; }\n"
			"</style>\n"
			"</head>\n");
		do_template("background", NULL);
	}

	do_template("menubar", NULL);

	if (as_single_page) {
		wDumpContent(2);
	}


}


/*
 * Display the wait / input dialog while restarting the server.
 */
void display_shutdown(void)
{
	char buf[SIZ];
	char *when;
	
	when=bstr("when");
	if (strcmp(when, "now") == 0){
		serv_printf("DOWN 1");
		serv_getln(buf, sizeof buf);
		if (atol(buf) == 500)
		{ /* upsie. maybe the server is not running as daemon? */
			
			safestrncpy(WC->ImportantMessage,
				    &buf[4],
				    sizeof WC->ImportantMessage);
		}
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(HKEY("display_serverrestart"), NULL, &NoCtx);
		end_burst();
		lingering_close(WC->Hdr->http_sock);
		sleeeeeeeeeep(10);
		serv_printf("NOOP");
		serv_printf("NOOP");
	}
	else if (strcmp(when, "page") == 0) {
		char *message;
	       
		message = bstr("message");
		if ((message == NULL) || (IsEmptyStr(message)))
		{
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("display_serverrestartpage"), NULL, &NoCtx);
			end_burst();
		}
		else
		{
			serv_printf("SEXP broadcast|%s", message);
			serv_getln(buf, sizeof buf); /* TODO: should we care? */
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("display_serverrestartpagedo"), NULL, &NoCtx);
			end_burst();			
		}
	}
	else if (!strcmp(when, "idle")) {
		serv_printf("SCDN 3");
		serv_getln(buf, sizeof buf);

		if (atol(buf) == 500)
		{ /* upsie. maybe the server is not running as daemon? */
			safestrncpy(WC->ImportantMessage,
				    &buf[4],
				    sizeof WC->ImportantMessage);
		}
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(HKEY("display_aide_menu"), NULL, &NoCtx);
		end_burst();			
	}
}

void _display_menubar(void) { display_menubar(0); }

void 
InitModule_MAINMENU
(void)
{
	WebcitAddUrlHandler(HKEY("display_aide_menu"), display_aide_menu, 0);
	WebcitAddUrlHandler(HKEY("server_shutdown"), display_shutdown, 0);
	WebcitAddUrlHandler(HKEY("display_main_menu"), display_main_menu, 0);
	WebcitAddUrlHandler(HKEY("display_generic"), display_generic, 0);
	WebcitAddUrlHandler(HKEY("do_generic"), do_generic, 0);
	WebcitAddUrlHandler(HKEY("display_menubar"), _display_menubar, 0);
}
