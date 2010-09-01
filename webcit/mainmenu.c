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
	DoTemplate(HKEY("aide_display_menu"), NULL, &NoCtx);
	end_burst();
}


/*
 * Interactive window to perform generic Citadel server commands.
 */
void do_generic(void)
{
        WCTemplputParams SubTP;
	int Done = 0;
	StrBuf *Buf;
	StrBuf *LineBuf;
	char *junk;
	size_t len;

	if (!havebstr("sc_button")) {
		display_main_menu();
		return;
	}

        memset(&SubTP, 0, sizeof(WCTemplputParams));
	Buf = NewStrBuf();
	serv_puts(bstr("g_cmd"));
	StrBuf_ServGetln(Buf);
	
	switch (GetServerStatus(Buf, NULL)) {
	case 8:
		serv_puts("\n\n000");
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			StrBufAppendBufPlain(Buf, HKEY("\000"), 0);
			break;
		}
	case 1:
		LineBuf = NewStrBuf();
		StrBufAppendBufPlain(Buf, HKEY("\n"), 0);
		while (!Done) {
			StrBuf_ServGetln(LineBuf);
			if ( (StrLength(LineBuf)==3) && 
			     !strcmp(ChrPtr(LineBuf), "000")) {
				Done = 1;
			}
			StrBufAppendBuf(Buf, LineBuf, 0);
			StrBufAppendBufPlain(Buf, HKEY("\n"), 0);
		}
		FreeStrBuf(&LineBuf);
		break;
	case 2:
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
		break;
	}
	
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);

        SubTP.Filter.ContextType = CTX_STRBUF;
        SubTP.Context = Buf;

        DoTemplate(HKEY("aide_display_generic_result"), NULL, &SubTP);

        wDumpContent(1);

	FreeStrBuf(&Buf);
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
		DoTemplate(HKEY("aide_display_serverrestart"), NULL, &NoCtx);
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
			DoTemplate(HKEY("aide_display_serverrestart_page"), NULL, &NoCtx);
			end_burst();
		}
		else
		{
			serv_printf("SEXP broadcast|%s", message);
			serv_getln(buf, sizeof buf); /* TODO: should we care? */
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("aide_display_serverrestart_page"), NULL, &NoCtx);
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
		DoTemplate(HKEY("aide_display_menu"), NULL, &NoCtx);
		end_burst();			
	}
}

void 
InitModule_MAINMENU
(void)
{
	WebcitAddUrlHandler(HKEY("display_aide_menu"), "", 0, display_aide_menu, 0);
	WebcitAddUrlHandler(HKEY("server_shutdown"), "", 0, display_shutdown, 0);
	WebcitAddUrlHandler(HKEY("display_main_menu"), "", 0, display_main_menu, 0);
	WebcitAddUrlHandler(HKEY("do_generic"), "", 0, do_generic, 0);
}
