/*
 * Edits a users push email settings 
 * Author: Mathew McBride <matt@mcbridematt.dhs.org>
 */
#include "webcit.h"

void display_pushemail(void) 
{
	folder Room;
	int Done = 0;
	StrBuf *Buf;
	long vector[8] = {8, 0, 0, 1, 2, 3, 4, 5};
	WCTemplputParams SubTP;
	char mobnum[20];

	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_LONGVECTOR;
	SubTP.Context = &vector;
	vector[0] = 16;

	/* Find any existing settings*/
	Buf = NewStrBuf();
	memset(&Room, 0, sizeof(folder));
	if (goto_config_room(Buf, &Room) == 0) {
		int msgnum = 0;
		serv_puts("MSGS ALL|0|1");
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) == 8) {
			serv_puts("subj|__ Push email settings __");
			serv_puts("000");
			while (!Done &&
			       StrBuf_ServGetln(Buf) >= 0) {
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
			       StrBuf_ServGetln(Buf) >= 0) {
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
				       StrBuf_ServGetln(Buf) >= 0) {
					if ( (StrLength(Buf)==3) && 
					     !strcmp(ChrPtr(Buf), "000")) {
						Done = 1;
						break;
					}
					if (strncasecmp(ChrPtr(Buf), "none", 4) == 0) {
						vector[1] = 0;
					} else if (strncasecmp(ChrPtr(Buf), "textmessage", 11) == 0) {
						vector[1] = 1;
						i++;
					} else if (strncasecmp(ChrPtr(Buf), "funambol", 8) == 0) {
						vector[1] = 2;
					} else if (strncasecmp(ChrPtr(Buf), "httpmessage", 12) == 0) {
						vector[1] = 3;
					} else if (i == 1) {
						strncpy(mobnum, ChrPtr(Buf), 20);
						i++;
					}
				}	
			}
		}
		}
		serv_printf("GOTO %s", ChrPtr(WC->CurRoom.name));
		StrBuf_ServGetln(Buf);
		GetServerStatus(Buf, NULL);
	}
	FlushFolder(&Room);
	output_headers(1, 1, 1, 0, 0, 0);
	DoTemplate(HKEY("prefs_pushemail"), NULL, &SubTP);
	wDumpContent(1);
	FreeStrBuf(&Buf);
}

void save_pushemail(void) 
{
	folder Room;
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
	memset(&Room, 0, sizeof(folder));
	if (goto_config_room(Buf, &Room) != 0) {
		FreeStrBuf(&Buf);
		FlushFolder(&Room);
		return;	/* oh well. */
	}
	FlushFolder(&Room);

	serv_puts("MSGS ALL|0|1");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 8) {
		serv_puts("subj|__ Push email settings __");
		serv_puts("000");
	} else {
		printf("Junk in save_pushemail buffer!: %s\n", buf);
		FreeStrBuf(&Buf);
		return;
	}

	while (!Done &&
	       StrBuf_ServGetln(Buf) >= 0) {
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
	serv_printf("GOTO %s", ChrPtr(WC->CurRoom.name));
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, NULL);
	http_redirect("display_pushemail");
	FreeStrBuf(&Buf);
}

void 
InitModule_PUSHMAIL
(void)
{
	WebcitAddUrlHandler(HKEY("display_pushemail"), "", 0, display_pushemail, 0);
	WebcitAddUrlHandler(HKEY("save_pushemail"), "", 0, save_pushemail, 0);
}
