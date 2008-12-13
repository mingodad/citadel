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
				extract_token(sender, buf, 1, '|', sizeof sender);

				/* Strip off local hostname if it's our own */
				char *atsign;
				atsign = strchr(sender, '@');
				if (atsign != NULL) {
					++atsign;
					if (!strcasecmp(atsign, serv_info.serv_nodename)) {
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

	wprintf("<tr><td>");
	wprintf("%ld<br />", msgnum);
	wprintf(" <a href=\"javascript:DeleteQueueMsg(%ld,%ld);\">%s</a>", 
		msgnum, msgid, _("(Delete)")
	);

	wprintf("</td><td>");
	if (submitted > 0) {
		webcit_fmt_date(buf, submitted, 1);
		wprintf("%s", buf);
	}
	else {
		wprintf("&nbsp;");
	}

	wprintf("</td><td>");
	if (last_attempt > 0) {
		webcit_fmt_date(buf, last_attempt, 1);
		wprintf("%s", buf);
	}
	else {
		wprintf("&nbsp;");
	}

	wprintf("</td><td>");
	escputs(sender);

	wprintf("</td><td>");
	wprintf("%s", recipients);
	wprintf("</td></tr>\n");

}


void display_smtpqueue_inner_div(void) {
	message_summary *Msg;
	wcsession *WCC = WC;
	int i;
	int num_msgs;

	/* Check to see if we can go to the __CitadelSMTPspoolout__ room.
	 * If not, we don't have access to the queue.
	 */
	gotoroom("__CitadelSMTPspoolout__");
	if (!strcasecmp(WCC->wc_roomname, "__CitadelSMTPspoolout__")) {

		num_msgs = load_msg_ptrs("MSGS ALL", 0);
		if (num_msgs > 0) {
                        wprintf("<table class=\"mailbox_summary\" rules=rows "
                        	"cellpadding=2 style=\"width:100%%;\">"
			);

			wprintf("<tr><td><b><i>");
			wprintf(_("Message ID"));
			wprintf("</i></b></td><td><b><i>");
			wprintf(_("Date/time submitted"));
			wprintf("</i></b></td><td><b><i>");
			wprintf(_("Last attempt"));
			wprintf("</i></b></td><td><b><i>");
			wprintf(_("Sender"));
			wprintf("</i></b></td><td><b><i>");
			wprintf(_("Recipients"));
			wprintf("</i></b></td></tr>\n");

			for (i=0; i<num_msgs; ++i) {
				Msg = GetMessagePtrAt(i, WCC->summ);

				display_queue_msg((Msg==NULL)? 0 : Msg->msgnum);
			}

			wprintf("</table>");

		}
		else {
			wprintf("<br /><br /><div align=\"center\">");
			wprintf(_("The queue is empty."));
			wprintf("</div><br /><br />");
		}
	}
	else {
		wprintf("<br /><br /><div align=\"center\">");
		wprintf(_("You do not have permission to view this resource."));
		wprintf("</div><br /><br />");
	}

}

/**
 * \brief display the outbound SMTP queue
 */
void display_smtpqueue(void)
{
	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<script type=\"text/javascript\">				\n"
		"function RefreshQueueDisplay() {				\n"
		"	new Ajax.Updater('smtpqueue_inner_div', 		\n"
		"	'display_smtpqueue_inner_div', { method: 'get',		\n"
		"		parameters: Math.random() } );			\n"
		"}								\n"
		"								\n"
		"function DeleteQueueMsg(msgnum1, msgnum2) {					\n"
 		"	new Ajax.Request(							\n"
		"		'ajax_servcmd', {						\n"
		"			method: 'post',						\n"
		"			parameters: 'g_cmd=DELE ' + msgnum1 + ',' + msgnum2,	\n"
		"			onComplete: RefreshQueueDisplay()			\n"
		"		}								\n"
		"	);									\n"
		"}										\n"
		"								\n"
		"</script>							\n"
	);

	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("View the outbound SMTP queue"));
	wprintf("</h1>\n");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"smtpqueue_background\">"
		"<tr><td valign=top>\n");

	wprintf("<div id=\"smtpqueue_inner_div\">");

	display_smtpqueue_inner_div();

	wprintf("</div>"
		"<div align=\"center\">"
		"<a href=\"javascript:RefreshQueueDisplay();\">%s</a>"
		"</div>"
		"</td></tr></table></div>\n", _("Refresh this page")
	);
	wDumpContent(1);

}

void 
InitModule_SMTP_QUEUE
(void)
{
	WebcitAddUrlHandler(HKEY("display_smtpqueue"), display_smtpqueue, 0);
	WebcitAddUrlHandler(HKEY("display_smtpqueue_inner_div"), display_smtpqueue_inner_div, 0);
}

/*@}*/
