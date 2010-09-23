/* 
 * $Id$
 *
 * Display the outbound SMTP queue
 */

#include "webcit.h"

/*
 * display one message in the queue
 */
void display_queue_msg(long msgnum)
{
	char buf[1024];
	char keyword[32];
	int in_body = 0;
	int is_delivery_list = 0;
	time_t submitted = 0;
	time_t attempted = 0;
	time_t last_attempt = 0;
	int number_of_attempts = 0;
	char sender[256];
	char recipients[65536];
	int recipients_len = 0;
	char thisrecp[256];
	char thisdsn[256];
	char thismsg[512];
	int thismsg_len;
	long msgid = 0;
	int len;

	strcpy(sender, "");
	strcpy(recipients, "");
	recipients_len = 0;

	serv_printf("MSG2 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		if (!IsEmptyStr(buf)) {
			len = strlen(buf);
			if (buf[len - 1] == 13) {
				buf[len - 1] = 0;
			}
		}

		if ( (IsEmptyStr(buf)) && (in_body == 0) ) {
			in_body = 1;
		}

		if ( (!in_body)
		   && (!strncasecmp(buf, "Content-type: application/x-citadel-delivery-list", 49))
		) {
			is_delivery_list = 1;
		}

		if ( (in_body) && (!is_delivery_list) ) {
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				/* Not a delivery list; flush and return quietly. */
			}
			return;
		}

		if ( (in_body) && (is_delivery_list) ) {
			extract_token(keyword, buf, 0, '|', sizeof keyword);

			if (!strcasecmp(keyword, "msgid")) {
				msgid = extract_long(buf, 1);
			}

			if (!strcasecmp(keyword, "submitted")) {
				submitted = extract_long(buf, 1);
			}

			if (!strcasecmp(keyword, "attempted")) {
				attempted = extract_long(buf, 1);
				++number_of_attempts;
				if (attempted > last_attempt) {
					last_attempt = attempted;
				}
			}

			if (!strcasecmp(keyword, "bounceto")) {
				char *atsign;
				extract_token(sender, buf, 1, '|', sizeof sender);

				/* Strip off local hostname if it's our own */
				atsign = strchr(sender, '@');
				if (atsign != NULL) {
					++atsign;
					if (!strcasecmp(atsign, ChrPtr(WC->serv_info->serv_nodename))) {
						--atsign;
						*atsign = 0;
					}
				}
			}

			if (!strcasecmp(keyword, "remote")) {
				thismsg[0] = 0;

				extract_token(thisrecp, buf, 1, '|', sizeof thisrecp);
				extract_token(thisdsn, buf, 3, '|', sizeof thisdsn);

				if (!IsEmptyStr(thisrecp)) {
					stresc(thismsg, sizeof thismsg, thisrecp, 1, 1);
					if (!IsEmptyStr(thisdsn)) {
						strcat(thismsg, "<br />&nbsp;&nbsp;<i>");
						stresc(&thismsg[strlen(thismsg)], sizeof thismsg,
							thisdsn, 1, 1);
						strcat(thismsg, "</i>");
					}
					thismsg_len = strlen(thismsg);

					if ((recipients_len + thismsg_len + 100) < sizeof recipients) {
						if (!IsEmptyStr(recipients)) {
							strcpy(&recipients[recipients_len], "<br />");
							recipients_len += 6;
						}
						strcpy(&recipients[recipients_len], thismsg);
						recipients_len += thismsg_len;
					}
				}

			}

		}

	}

	wc_printf("<tr><td>");
	wc_printf("%ld<br />", msgnum);
	wc_printf(" <a href=\"javascript:DeleteSMTPqueueMsg(%ld,%ld);\">%s</a>", 
		msgnum, msgid, _("(Delete)")
	);

	wc_printf("</td><td>");
	if (submitted > 0) {
		webcit_fmt_date(buf, 1024, submitted, 1);
		wc_printf("%s", buf);
	}
	else {
		wc_printf("&nbsp;");
	}

	wc_printf("</td><td>");
	if (last_attempt > 0) {
		webcit_fmt_date(buf, 1024, last_attempt, 1);
		wc_printf("%s", buf);
	}
	else {
		wc_printf("&nbsp;");
	}

	wc_printf("</td><td>");
	escputs(sender);

	wc_printf("</td><td>");
	wc_printf("%s", recipients);
	wc_printf("</td></tr>\n");

}


void display_smtpqueue_inner_div(void) {
	message_summary *Msg = NULL;
	wcsession *WCC = WC;
	int i;
	int num_msgs;
	StrBuf *Buf;
	SharedMessageStatus Stat;

	memset(&Stat, 0, sizeof(SharedMessageStatus));
	/* Check to see if we can go to the __CitadelSMTPspoolout__ room.
	 * If not, we don't have access to the queue.
	 */
	Buf = NewStrBufPlain(HKEY("__CitadelSMTPspoolout__"));
	gotoroom(Buf);
	FreeStrBuf(&Buf);
	if (!strcasecmp(ChrPtr(WCC->CurRoom.name), "__CitadelSMTPspoolout__")) {

		Stat.maxload = 10000;
		Stat.lowest_found = (-1);
		Stat.highest_found = (-1);
		num_msgs = load_msg_ptrs("MSGS ALL", &Stat, NULL);
		if (num_msgs > 0) {
                        wc_printf("<table class=\"mailbox_summary\" rules=rows "
                        	"cellpadding=2 style=\"width:100%%;\">"
			);

			wc_printf("<tr><td><b><i>");
			wc_printf(_("Message ID"));
			wc_printf("</i></b></td><td><b><i>");
			wc_printf(_("Date/time submitted"));
			wc_printf("</i></b></td><td><b><i>");
			wc_printf(_("Last attempt"));
			wc_printf("</i></b></td><td><b><i>");
			wc_printf(_("Sender"));
			wc_printf("</i></b></td><td><b><i>");
			wc_printf(_("Recipients"));
			wc_printf("</i></b></td></tr>\n");

			for (i=0; (i < num_msgs) && (i < Stat.maxload); ++i) {
				Msg = GetMessagePtrAt(i, WCC->summ);
				if (Msg != NULL) {
					display_queue_msg(Msg->msgnum);
				}
			}

			wc_printf("</table>");

		}
		else {
			wc_printf("<br /><br /><div align=\"center\">");
			wc_printf(_("The queue is empty."));
			wc_printf("</div><br /><br />");
		}
	}
	else {
		wc_printf("<br /><br /><div align=\"center\">");
		wc_printf(_("You do not have permission to view this resource."));
		wc_printf("</div><br /><br />");
	}
	output_headers(0, 0, 0, 0, 0, 0);
	end_burst();
}

/*
 * display the outbound SMTP queue
 */
void display_smtpqueue(void)
{
	output_headers(1, 1, 2, 0, 0, 0);

	wc_printf("<div id=\"banner\">\n");
	wc_printf("<h1>");
	wc_printf(_("View the outbound SMTP queue"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"smtpqueue_background\">"
		"<tr><td valign=top>\n");

	wc_printf("<div id=\"smtpqueue_inner_div\">"
		"<div align=\"center\"><img src=\"static/throbber.gif\"></div>"
		"</div>"
		"<div align=\"center\">"
		"<a href=\"javascript:RefreshSMTPqueueDisplay();\">%s</a>"
		"</div>"
		"</td></tr></table></div>\n", _("Refresh this page")
	);

	StrBufAppendPrintf(WC->trailing_javascript, "RefreshSMTPqueueDisplay();\n");

	wDumpContent(1);

}

void 
InitModule_SMTP_QUEUE
(void)
{
	WebcitAddUrlHandler(HKEY("display_smtpqueue"), "", 0, display_smtpqueue, 0);
	WebcitAddUrlHandler(HKEY("display_smtpqueue_inner_div"), "", 0, display_smtpqueue_inner_div, 0);
}
