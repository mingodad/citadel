/*
 * Edits a users push email settings 
 * Author: Mathew McBride <matt@mcbridematt.dhs.org>
 */
#include "webcit.h"

void display_pushemail(void) 
{
	int Done = 0;
	StrBuf *Buf;
	int is_none = 0;
	int is_pager = 0;
	int is_funambol = 0;
	char mobnum[20];

	/* Find any existing settings*/
	Buf = NewStrBuf();
	if (goto_config_room(Buf) == 0) {
		int msgnum = 0;
		serv_puts("MSGS ALL|0|1");
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 8) {
			serv_puts("subj|__ Push email settings __");
			serv_puts("000");
			while (!Done &&
			       StrBuf_ServGetln(Buf)) {
				if ( (StrLength(Buf)==3) && 
				     !strcmp(ChrPtr(Buf), "000")) {
					Done = 1;
					break;
				}
				msgnum = StrTol(Buf);
			}
		}
		if (msgnum > 0L) {
		serv_printf("MSG0 %d", msgnum);
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 1) {
			int i =0;
			Done = 0;
			while (!Done &&
			       StrBuf_ServGetln(Buf)) {
				if (( (StrLength(Buf)==3) && 
				      !strcmp(ChrPtr(Buf), "000"))||
				    ((StrLength(Buf)==4) && 
				     !strcmp(ChrPtr(Buf), "text")))
				{
					Done = 1;
					break;
				}
			}
			if (!strcmp(ChrPtr(Buf), "text")) {
				Done = 0;
				while (!Done &&
				       StrBuf_ServGetln(Buf)) {
					if ( (StrLength(Buf)==3) && 
					     !strcmp(ChrPtr(Buf), "000")) {
						Done = 1;
						break;
					}
					if (strncasecmp(ChrPtr(Buf), "none", 4) == 0) {
						is_none = 1;
					} else if (strncasecmp(ChrPtr(Buf), "textmessage", 11) == 0) {
						is_pager = 1;
						i++;
					} else if (strncasecmp(ChrPtr(Buf), "funambol", 8) == 0) {
						is_funambol = 1;
					} else if (i == 1) {
						strncpy(mobnum, ChrPtr(Buf), 20);
						i++;
					}
				}	
			}
		}
		}
		/* TODO: do in a saner fashion. */
		svput("PUSH_NONE", WCS_STRING, " "); /* defaults */
		svput("PUSH_TEXT", WCS_STRING, " ");
		svput("PUSH_FNBL", WCS_STRING, " ");
		svput("SMSNUM", WCS_STRING, " ");
		if (is_none) {
			svput("PUSH_NONE", WCS_STRING, "checked=\"checked\"");
		} else if (is_pager) {
			svput("PUSH_TEXT", WCS_STRING, "checked=\"checked\"");
			svprintf(HKEY("SMSNUM"), WCS_STRING, "value=\"%s\"", mobnum);
		} else if (is_funambol) {
			svput("PUSH_FNBL", WCS_STRING, "checked=\"checked\"");
		}
		serv_printf("GOTO %s", ChrPtr(WC->wc_roomname));
		StrBuf_ServGetln(Buf);
		GetServerStatus(Buf, NULL);
	}
	output_headers(1, 1, 2, 0, 0, 0);
	do_template("pushemail", NULL);
/*do_template("endbox"); */
	wDumpContent(1);
	FreeStrBuf(&Buf);
}

void save_pushemail(void) 
{
	int Done = 0;
	StrBuf *Buf;
	char buf[SIZ];
	int msgnum = 0;
	char *pushsetting = bstr("pushsetting");
	char *sms = NULL;

	if (strncasecmp(pushsetting, "textmessage", 11) == 0) {
		sms = bstr("user_sms_number");
	}
	Buf = NewStrBuf();
	if (goto_config_room(Buf) != 0) {
		FreeStrBuf(&Buf);
		return;	/* oh well. */
	}
	serv_puts("MSGS ALL|0|1");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 8) {
		serv_puts("subj|__ Push email settings __");
		serv_puts("000");
	} else {
		printf("Junk in save_pushemail buffer!: %s\n", buf);
		return;
	}

	while (!Done &&
	       StrBuf_ServGetln(Buf)) {
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			Done = 1;
			break;
		}
		msgnum = StrTol(Buf);
	}

	if (msgnum > 0L) {
		serv_printf("DELE %d", msgnum);
		StrBuf_ServGetln(Buf);
		GetServerStatus(Buf, NULL);
	}

	serv_printf("ENT0 1||0|1|__ Push email settings __|");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 4) {
		serv_puts(pushsetting);
		if (sms != NULL) {
		serv_puts(sms);
		} 
		serv_puts("");
		serv_puts("000");
	}

	/** Go back to the room we're supposed to be in */
	serv_printf("GOTO %s", ChrPtr(WC->wc_roomname));
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, NULL);
	http_redirect("display_pushemail");
}

void 
InitModule_PUSHMAIL
(void)
{
	WebcitAddUrlHandler(HKEY("display_pushemail"), display_pushemail, 0);
	WebcitAddUrlHandler(HKEY("save_pushemail"), save_pushemail, 0);
}
