/* 
 * netconf.c
 *
 * Functions which handle network and sharing configuration.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"



void edit_node(void) {
	char buf[SIZ];
	char node[SIZ];
	char cnode[SIZ];
	FILE *fp;

	if (!strcmp(bstr("sc"), "OK")) {
		strcpy(node, bstr("node") );
		fp = tmpfile();
		if (fp != NULL) {
			serv_puts("CONF getsys|application/x-citadel-ignet-config");
			serv_gets(buf);
			if (buf[0] == '1') {
				while (serv_gets(buf), strcmp(buf, "000")) {
					extract(cnode, buf, 0);
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
			serv_gets(buf);
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
	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Add new node</SPAN>");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/edit_node\">\n");
	wprintf("<CENTER><TABLE border=0>\n");
	wprintf("<TR><TD>Node name</TD>");
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"node\" MAXLENGTH=\"16\"></TD></TR>\n");
	wprintf("<TR><TD>Shared secret</TD>");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"secret\" MAXLENGTH=\"16\"></TD></TR>\n");
	wprintf("<TR><TD>Host or IP</TD>");
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"host\" MAXLENGTH=\"64\"></TD></TR>\n");
	wprintf("<TR><TD>Port</TD>");
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"port\" MAXLENGTH=\"8\"></TD></TR>\n");
	wprintf("</TABLE><br />");
       	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
	wprintf("&nbsp;");
       	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
	wprintf("</CENTER></FORM>\n");

	wDumpContent(1);
}

void display_edit_node(void)
{
	char buf[SIZ];
	char node[SIZ];
	char cnode[SIZ];
	char csecret[SIZ];
	char chost[SIZ];
	char cport[SIZ];

	strcpy(node, bstr("node"));

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Edit node configuration for ");
	escputs(node);
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	serv_puts("CONF getsys|application/x-citadel-ignet-config");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(cnode, buf, 0);
			extract(csecret, buf, 1);
			extract(chost, buf, 2);
			extract(cport, buf, 3);

			if (!strcasecmp(node, cnode)) {
				wprintf("<FORM METHOD=\"POST\" ACTION=\"/edit_node\">\n");
				wprintf("<CENTER><TABLE border=0>\n");
				wprintf("<TR><TD>Node name</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"node\" MAXLENGTH=\"16\" VALUE=\"%s\"></TD></TR>\n", cnode);
				wprintf("<TR><TD>Shared secret</TD>");
				wprintf("<TD><INPUT TYPE=\"password\" NAME=\"secret\" MAXLENGTH=\"16\" VALUE=\"%s\"></TD></TR>\n", csecret);
				wprintf("<TR><TD>Host or IP</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"host\" MAXLENGTH=\"64\" VALUE=\"%s\"></TD></TR>\n", chost);
				wprintf("<TR><TD>Port</TD>");
				wprintf("<TD><INPUT TYPE=\"text\" NAME=\"port\" MAXLENGTH=\"8\" VALUE=\"%s\"></TD></TR>\n", cport);
				wprintf("</TABLE><br />");
        			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
				wprintf("&nbsp;");
        			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
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

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Network configuration</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<CENTER>");
	wprintf("<A HREF=\"/display_add_node\">");
	wprintf("Add a new node</A><br />\n");
	wprintf("</CENTER>");

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Currently configured nodes</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	serv_puts("CONF getsys|application/x-citadel-ignet-config");
	serv_gets(buf);
	if (buf[0] == '1') {
		wprintf("<CENTER><TABLE border=0>\n");
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(node, buf, 0);
			wprintf("<TR><TD><FONT SIZE=+1>");
			escputs(node);
			wprintf("</FONT></TD>");
			wprintf("<TD><A HREF=\"/display_edit_node&node=");
			urlescputs(node);
			wprintf("\">(Edit)</A></TD>");
			wprintf("<TD><A HREF=\"/display_confirm_delete_node&node=");
			urlescputs(node);
			wprintf("\">(Delete)</A></TD>");
			wprintf("</TR>\n");
		}
		wprintf("</TABLE></CENTER>\n");
	}
	wDumpContent(1);
}


void display_confirm_delete_node(void)
{
	char node[SIZ];

	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Confirm delete</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	strcpy(node, bstr("node"));
	wprintf("<CENTER>Are you sure you want to delete <FONT SIZE=+1>");
	escputs(node);
	wprintf("</FONT>?<br />\n");
	wprintf("<A HREF=\"/delete_node&node=");
	urlescputs(node);
	wprintf("\">Yes</A>&nbsp;&nbsp;&nbsp;");
	wprintf("<A HREF=\"/display_netconf\">No</A><br />\n");
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
		serv_gets(buf);
		if (buf[0] == '1') {
			while (serv_gets(buf), strcmp(buf, "000")) {
				extract(cnode, buf, 0);
				if (strcasecmp(node, cnode)) {
					fprintf(fp, "%s\n", buf);
				}
			}
		}
		rewind(fp);

		serv_puts("CONF putsys|application/x-citadel-ignet-config");
		serv_gets(buf);
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
	char sc[SIZ];

	strcpy(node, bstr("node"));
	strcpy(sc, bstr("sc"));

	if (!strcmp(sc, "Add")) {
		sprintf(buf, "NSET addnode|%s", node);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '1') {
			output_headers(1, 1, 0, 0, 0, 0, 0);
			server_to_text();
			wprintf("<A HREF=\"/display_netconf\">Back to menu</A>\n");
			wDumpContent(1);
		} else {
			strcpy(WC->ImportantMessage, &buf[4]);
			display_netconf();
		}
	}
}


