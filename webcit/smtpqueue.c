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

	serv_printf("MSG2 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

			wprintf("<tr><td>");
			wprintf(_("Message ID"));
			wprintf("</td><td>");
			wprintf(_("Date/time submitted"));
			wprintf("</td><td>");
			wprintf(_("Last attempt"));
			wprintf("</td><td>");
			wprintf(_("Sender"));
			wprintf("</td><td>");
			wprintf(_("Recipients"));
			wprintf("</td></tr>\n");

	}

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

			wprintf("<table border=1 width=100%%>\n");
			wprintf("<tr><td>");
			wprintf(_("Message ID"));
			wprintf("</td><td>");
			wprintf(_("Date/time submitted"));
			wprintf("</td><td>");
			wprintf(_("Last attempt"));
			wprintf("</td><td>");
			wprintf(_("Sender"));
			wprintf("</td><td>");
			wprintf(_("Recipients"));
			wprintf("</td></tr>\n");

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
