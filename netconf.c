/* 
 * $Id$
 *
 * Functions which handle network and sharing configuration.
 *
 */

#include "webcit.h"

void edit_node(void) {
	char buf[SIZ];
	char node[SIZ];
	char cnode[SIZ];
	FILE *fp;

	if (strlen(bstr("ok_button")) > 0) {
		strcpy(node, bstr("node") );
		fp = tmpfile();
		if (fp != NULL) {
			serv_puts("CONF getsys|application/x-citadel-ignet-config");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '1') {
				while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
					extract_token(cnode, buf, 0, '|', sizeof cnode);
					if (strcasecmp(node, cnode)) {
						fprintf(fp, "%s\n", buf);
					}
				}
			fprintf(fp, "%s|%s|%s|%s\n", 
				bstr("node"),
				bstr("secret"),
				bstr("host"),
				bstr("port") );
			}
			rewind(fp);

			serv_puts("CONF putsys|application/x-citadel-ignet-config");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '4') {
				while (fgets(buf, sizeof buf, fp) != NULL) {
					buf[strlen(buf)-1] = 0;
					serv_puts(buf);
				}
				serv_puts("000");
			}
			fclose(fp);
		}
	}

	display_netconf();
}



void display_add_node(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Add a new node"));
	wprintf("</SPAN>");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<FORM METHOD=\"POST\" action=\"/edit_node\">\n");
	wprintf("<CENTER><TABLE border=0>\n");
	wprintf("<TR><TD>%s</TD>", _("Node name"));
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"node\" MAXLENGTH=\"16\"></TD></TR>\n");
	wprintf("<TR><TD>%s</TD>", _("Shared secret"));
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"secret\" MAXLENGTH=\"16\"></TD></TR>\n");
	wprintf("<TR><TD>%s</TD>", _("Host or IP address"));
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"host\" MAXLENGTH=\"64\"></TD></TR>\n");
	wprintf("<TR><TD>%s</TD>", _("Port number"));
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"port\" MAXLENGTH=\"8\"></TD></TR>\n");
	wprintf("</TABLE><br />");
       	wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Add node"));
	wprintf("&nbsp;");
       	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
	wprintf("</CENTER></FORM>\n");

	wDumpContent(1);
}

void display_edit_node(void)
{
	char buf[512];
	char node[256];
	char cnode[256];
	char csecret[256];
	char chost[256];
	char cport[256];

	strcpy(node, bstr("node"));

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Edit node configuration for "));
	escputs(node);
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	serv_puts("CONF getsys|application/x-citadel-ignet-config");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(cnode, buf, 0, '|', sizeof cnode);
			extract_token(csecret, buf, 1, '|', sizeof csecret);
			extract_token(chost, buf, 2, '|', sizeof chost);
			extract_token(cport, buf, 3, '|', sizeof cport);

			if (!strcasecmp(node, cnode)) {
				wprintf("<FORM METHOD=\"POST\" action=\"/edit_node\">\n");
				wprintf("<CENTER><TABLE border=0>\n");
				wprintf("<TR><TD>");
				wprintf(_("Node name"));
				wprintf("</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"node\" MAXLENGTH=\"16\" VALUE=\"%s\"></TD></TR>\n", cnode);
				wprintf("<TR><TD>");
				wprintf(_("Shared secret"));
				wprintf("</TD>");
				wprintf("<TD><INPUT TYPE=\"password\" NAME=\"secret\" MAXLENGTH=\"16\" VALUE=\"%s\"></TD></TR>\n", csecret);
				wprintf("<TR><TD>");
				wprintf(_("Host or IP address"));
				wprintf("</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"host\" MAXLENGTH=\"64\" VALUE=\"%s\"></TD></TR>\n", chost);
				wprintf("<TR><TD>");
				wprintf(_("Port number"));
				wprintf("</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"port\" MAXLENGTH=\"8\" VALUE=\"%s\"></TD></TR>\n", cport);
				wprintf("</TABLE><br />");
        			wprintf("<INPUT TYPE=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">",
					_("Save changes"));
				wprintf("&nbsp;");
        			wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">",
					_("Cancel"));
				wprintf("</CENTER></FORM>\n");
			}

		}
	}

	else {		/* command error getting configuration */
		wprintf("%s<br />\n", &buf[4]);
	}

	wDumpContent(1);
}



void display_netconf(void)
{
	char buf[SIZ];
	char node[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Network configuration"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<CENTER>");
	wprintf("<a href=\"/display_add_node\">");
	wprintf(_("Add a new node"));
	wprintf("</A><br />\n");
	wprintf("</CENTER>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Currently configured nodes"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	serv_puts("CONF getsys|application/x-citadel-ignet-config");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		wprintf("<CENTER><TABLE border=0>\n");
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(node, buf, 0, '|', sizeof node);
			wprintf("<TR><TD><FONT SIZE=+1>");
			escputs(node);
			wprintf("</FONT></TD>");
			wprintf("<TD><a href=\"/display_edit_node&node=");
			urlescputs(node);
			wprintf("\">");
			wprintf(_("(Edit)"));
			wprintf("</A></TD>");
			wprintf("<TD><a href=\"/display_confirm_delete_node&node=");
			urlescputs(node);
			wprintf("\">");
			wprintf(_("(Delete)"));
			wprintf("</A></TD>");
			wprintf("</TR>\n");
		}
		wprintf("</TABLE></CENTER>\n");
	}
	wDumpContent(1);
}


void display_confirm_delete_node(void)
{
	char node[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Confirm delete"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	strcpy(node, bstr("node"));
	wprintf("<CENTER>");
	wprintf(_("Are you sure you want to delete "));
	wprintf("<FONT SIZE=+1>");
	escputs(node);
	wprintf("</FONT>?<br />\n");
	wprintf("<a href=\"/delete_node&node=");
	urlescputs(node);
	wprintf("\">");
	wprintf(_("Yes"));
	wprintf("</A>&nbsp;&nbsp;&nbsp;");
	wprintf("<a href=\"/display_netconf\">");
	wprintf(_("No"));
	wprintf("</A><br />\n");
	wDumpContent(1);
}


void delete_node(void)
{
	char buf[SIZ];
	char node[SIZ];
	char cnode[SIZ];
	FILE *fp;

	strcpy(node, bstr("node") );
	fp = tmpfile();
	if (fp != NULL) {
		serv_puts("CONF getsys|application/x-citadel-ignet-config");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cnode, buf, 0, '|', sizeof cnode);
				if (strcasecmp(node, cnode)) {
					fprintf(fp, "%s\n", buf);
				}
			}
		}
		rewind(fp);

		serv_puts("CONF putsys|application/x-citadel-ignet-config");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			while (fgets(buf, sizeof buf, fp) != NULL) {
				buf[strlen(buf)-1] = 0;
				serv_puts(buf);
			}
			serv_puts("000");
		}
		fclose(fp);
	}

	display_netconf();
}


void add_node(void)
{
	char node[SIZ];
	char buf[SIZ];

	strcpy(node, bstr("node"));

	if (strlen(bstr("add_button")) > 0)  {
		sprintf(buf, "NSET addnode|%s", node);
		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			output_headers(1, 1, 0, 0, 0, 0);
			server_to_text();
			wprintf("<a href=\"/display_netconf\">");
			wprintf(_("Back to menu"));
			wprintf("</A>\n");
			wDumpContent(1);
		} else {
			strcpy(WC->ImportantMessage, &buf[4]);
			display_netconf();
		}
	}
}


