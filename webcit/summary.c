/*
 * Displays the "Summary Page"
 *
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
#include "calendar.h"

extern int calendar_summary_view(void);

/*
 * Display today's date in a friendly format
 */
void output_date(void) {
	struct tm tm;
	time_t now;
	char buf[128];

	time(&now);
	localtime_r(&now, &tm);

	wc_strftime(buf, 32, "%A, %x", &tm);
	wc_printf("%s", buf);
}

void tmplput_output_date(StrBuf *Target, WCTemplputParams *TP)
{
	struct tm tm;
	time_t now;
	char buf[128];
	size_t n;

	time(&now);
	localtime_r(&now, &tm);

	n = wc_strftime(buf, 32, "%A, %x", &tm);
	StrBufAppendBufPlain(Target, buf, n, 0);
}


/*
 * New messages section
 */
void new_messages_section(void) {
	char buf[SIZ];
	char room[SIZ];
	int i;
	int number_of_rooms_to_check;
	char *rooms_to_check = "Mail|Lobby";


	number_of_rooms_to_check = num_tokens(rooms_to_check, '|');
	if (number_of_rooms_to_check == 0) return;

	wc_printf("<table border=\"0\" width=\"100%%\">\n");
	for (i=0; i<number_of_rooms_to_check; ++i) {
		extract_token(room, rooms_to_check, i, '|', sizeof room);

		serv_printf("GOTO %s", room);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			extract_token(room, &buf[4], 0, '|', sizeof room);
			wc_printf("<tr><td><a href=\"dotgoto?room=");
			urlescputs(room);
			wc_printf("\">");
			escputs(room);
			wc_printf("</a></td><td>%d/%d</td></tr>\n",
				extract_int(&buf[4], 1),
				extract_int(&buf[4], 2)
			);
		}
	}
	wc_printf("</table>\n");

}


/*
 * Task list section
 */
void tasks_section(void) {
	int num_msgs = 0;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wcsession *WCC = WC;
	StrBuf *Buf;
	SharedMessageStatus Stat;

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);

	Buf = NewStrBufPlain(HKEY("_TASKS_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);

	if (WCC->CurRoom.view != VIEW_TASKS) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", NULL, &Stat, NULL);
	}

	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			tasks_LoadMsgFromServer(NULL, NULL, Msg, 0, 0);
		}
		DeleteHashPos(&at);
	}

	if (calendar_summary_view() < 1) {
		wc_printf("<i>");
		wc_printf(_("(None)"));
		wc_printf("</i><br>\n");
	}
}


/*
 * Calendar section
 */
void calendar_section(void) {
	char cmd[SIZ];
	char filter[SIZ];
	int num_msgs = 0;
	HashPos *at;
	const char *HashKey;
	long HKLen;
	void *vMsg;
	message_summary *Msg;
	wcsession *WCC = WC;
	StrBuf *Buf;
	void *v = NULL;
	SharedMessageStatus Stat;

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	Stat.maxload = 10000;
	Stat.lowest_found = (-1);
	Stat.highest_found = (-1);
	
	Buf = NewStrBufPlain(HKEY("_CALENDAR_"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if ( (WC->CurRoom.view != VIEW_CALENDAR) && (WC->CurRoom.view != VIEW_CALBRIEF) ) {
		num_msgs = 0;
	}
	else {
		num_msgs = load_msg_ptrs("MSGS ALL", NULL, &Stat, NULL);
	}
	calendar_GetParamsGetServerCall(&Stat, 
					&v,
					readnew, 
					cmd, 
					sizeof(cmd),
					filter,
					sizeof(filter));


	if (num_msgs > 0) {
		at = GetNewHashPos(WCC->summ, 0);
		while (GetNextHashPos(WCC->summ, at, &HKLen, &HashKey, &vMsg)) {
			Msg = (message_summary*) vMsg;		
			calendar_LoadMsgFromServer(NULL, &v, Msg, 0, 0);
		}
		DeleteHashPos(&at);
	}
	if (calendar_summary_view() < 1) {
		wc_printf("<i>");
		wc_printf(_("(Nothing)"));
		wc_printf("</i><br>\n");
	}
	__calendar_Cleanup(&v);
}

void tmplput_new_messages_section(StrBuf *Target, WCTemplputParams *TP) {
	new_messages_section();
}
void tmplput_tasks_section(StrBuf *Target, WCTemplputParams *TP) {
	tasks_section();
}
void tmplput_calendar_section(StrBuf *Target, WCTemplputParams *TP) {
	calendar_section();
}

void 
InitModule_SUMMARY
(void)
{
	RegisterNamespace("TIME:NOW", 0, 0, tmplput_output_date, NULL, CTX_NONE);
	RegisterNamespace("SUMMARY:NEWMESSAGES_SELECTION", 0, 0, tmplput_new_messages_section, NULL, CTX_NONE);
	RegisterNamespace("SUMMARY:TASKSSECTION", 0, 0, tmplput_tasks_section, NULL, CTX_NONE);
	RegisterNamespace("SUMMARY:CALENDAR_SECTION", 0, 0, tmplput_calendar_section, NULL, CTX_NONE);

	WebcitAddUrlHandler(HKEY("new_messages_html"), "", 0, new_messages_section, AJAX);
	WebcitAddUrlHandler(HKEY("tasks_inner_html"), "", 0, tasks_section, AJAX);
	WebcitAddUrlHandler(HKEY("calendar_inner_html"), "", 0, calendar_section, AJAX);

}

