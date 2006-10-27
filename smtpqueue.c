/* 
 * $Id: $
 */
/**
 * \defgroup SMTPqueue Display the outbound SMTP queue
 * \ingroup CitadelConfig
 */
/*@{*/
#include "webcit.h"

/**
 * \brief display one message in the queue
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
	char thisrecp[256];
	char thisdsn[256];

	strcpy(sender, "");
	strcpy(recipients, "");

	serv_printf("MSG2 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		if (strlen(buf) > 0) {
			if (buf[strlen(buf)-1] == 13) {
				buf[strlen(buf)-1] = 0;
			}
		}

		if ( (strlen(buf) == 0) && (in_body == 0) ) {
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
			}

			if (!strcasecmp(keyword, "remote")) {
				extract_token(thisrecp, buf, 1, '|', sizeof thisrecp);
				extract_token(thisdsn, buf, 3, '|', sizeof thisdsn);

				if (strlen(recipients) + strlen(thisrecp) + strlen(thisdsn) + 100
				   < sizeof recipients) {
					if (strlen(recipients) > 0) {
						strcat(recipients, "<br />");
					}
					stresc(&recipients[strlen(recipients)], thisrecp, 1, 1);
					strcat(recipients, "<br />&nbsp;&nbsp;<i>");
					stresc(&recipients[strlen(recipients)], thisdsn, 1, 1);
					strcat(recipients, "</i>");
				}

			}

		}

	}

	wprintf("<tr><td>");
	wprintf("%ld", msgnum);

	wprintf("</td><td>");
	if (submitted > 0) {
		fmt_date(buf, submitted, 1);
		wprintf("%s", buf);
	}
	else {
		wprintf("&nbsp;");
	}

	wprintf("</td><td>");
	if (last_attempt > 0) {
		fmt_date(buf, last_attempt, 1);
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


/**
 * \brief display the outbound SMTP queue
 */
void display_smtpqueue(void)
{
	int i;
	int num_msgs;

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("View the outbound SMTP queue"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#FFFFFF\">"
		"<tr><td valign=top>\n");


	/* Check to see if we can go to the __CitadelSMTPspoolout__ room.
	 * If not, we don't have access to the queue.
	 */
	gotoroom("__CitadelSMTPspoolout__");
	if (!strcasecmp(WC->wc_roomname, "__CitadelSMTPspoolout__")) {

		num_msgs = load_msg_ptrs("MSGS ALL", 0);
		if (num_msgs > 0) {
                        wprintf("<table class=\"mailbox_summary\" rules=rows "
                        	"cellpadding=2 style=\"width:100%%;-moz-user-select:none;\">"
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
				display_queue_msg(WC->msgarr[i]);
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

	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

}




/*@}*/
