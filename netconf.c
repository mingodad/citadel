#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

struct sharelist {
	struct sharelist *next;
	char shname[256];
};


void display_edit_node(void)
{
	char buf[256];
	char node[256];
	char sroom[256];

	strcpy(node, bstr("node"));

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Edit share list for ");
	escputs(node);
	wprintf("</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n");
	wprintf("<A HREF=\"/display_share&node=");
	urlescputs(node);
	wprintf("\">Add a shared room</A><BR>\n");

	wprintf("<A HREF=\"/display_netconf");
	wprintf("\">Return to network configuration screen</A><BR>\n");

	serv_printf("NSET roomlist|%s", node);
	serv_gets(buf);
	if (buf[0] == '1') {
		wprintf("<TABLE border=0>\n");
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(sroom, buf, 0);
			wprintf("<TR><TD><FONT SIZE=+1>");
			escputs(sroom);
			wprintf("</FONT></TD>");
			wprintf("<TD><A HREF=\"/display_confirm_unshare&sroom=");
			urlescputs(sroom);
			wprintf("&node=");
			urlescputs(node);
			wprintf("\">(UnShare)</A></TD>");
			wprintf("</TR>\n");
		}
		wprintf("</TABLE></CENTER>\n");
	}
	wDumpContent(1);
}



void display_netconf(void)
{
	char buf[256];
	char node[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Network configuration</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("<A HREF=\"/display_add_node\">");
	wprintf("Add a new node</A><BR>\n");
	wprintf("</CENTER>");

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Currently configured nodes</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");
	serv_puts("NSET nodelist");
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


void display_confirm_unshare(void)
{
	char node[256];
	char sroom[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Confirm unshare</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	strcpy(node, bstr("node"));
	strcpy(sroom, bstr("sroom"));
	wprintf("<CENTER>Are you sure you want to unshare <FONT SIZE=+1>");
	escputs(sroom);
	wprintf("</FONT>?<BR>\n");
	wprintf("<A HREF=\"/unshare&node=");
	urlescputs(node);
	wprintf("&sroom=");
	urlescputs(sroom);
	wprintf("\">Yes</A>&nbsp;&nbsp;&nbsp;");
	wprintf("<A HREF=\"/display_edit_node&node=");
	urlescputs(node);
	wprintf("\">No</A><BR>\n");
	wDumpContent(1);
}


void display_confirm_delete_node(void)
{
	char node[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Confirm delete</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	strcpy(node, bstr("node"));
	wprintf("<CENTER>Are you sure you want to delete <FONT SIZE=+1>");
	escputs(node);
	wprintf("</FONT>?<BR>\n");
	wprintf("<A HREF=\"/delete_node&node=");
	urlescputs(node);
	wprintf("\">Yes</A>&nbsp;&nbsp;&nbsp;");
	wprintf("<A HREF=\"/display_netconf\">No</A><BR>\n");
	wDumpContent(1);
}


void delete_node(void)
{
	char node[256];
	char buf[256];

	strcpy(node, bstr("node"));
	sprintf(buf, "NSET deletenode|%s", node);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1, "bottom");
		server_to_text();
		wprintf("<A HREF=\"/display_netconf\">Back to menu</A>\n");
		wDumpContent(1);
	} else {
		display_error(&buf[4]);
	}
}


void unshare(void)
{
	char node[256];
	char sroom[256];
	char buf[256];

	strcpy(node, bstr("node"));
	strcpy(sroom, bstr("sroom"));
	sprintf(buf, "NSET unshare|%s|%s", node, sroom);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1, "bottom");
		server_to_text();
		wprintf("<A HREF=\"/display_netconf\">Back to menu</A>\n");
		wDumpContent(1);
	} else {
		display_error(&buf[4]);
	}
}



void display_add_node(void)
{

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Add a new node</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/add_node\">\n");

	wprintf("Enter name of new node: ");
	wprintf("<INPUT TYPE=\"text\" NAME=\"node\" MAXLENGTH=\"64\"><BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Add\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);
}



void add_node(void)
{
	char node[256];
	char buf[256];
	char sc[256];

	strcpy(node, bstr("node"));
	strcpy(sc, bstr("sc"));

	if (!strcmp(sc, "Add")) {
		sprintf(buf, "NSET addnode|%s", node);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '1') {
			printf("HTTP/1.0 200 OK\n");
			output_headers(1, "bottom");
			server_to_text();
			wprintf("<A HREF=\"/display_netconf\">Back to menu</A>\n");
			wDumpContent(1);
		} else {
			display_error(&buf[4]);
		}
	}
}



void display_share(void)
{
	char buf[256];
	char node[256];
	char sroom[256];
	struct sharelist *shlist = NULL;
	struct sharelist *shptr;
	int already_shared;

	strcpy(node, bstr("node"));

	printf("HTTP/1.0 200 OK\n");
	output_headers(1, "bottom");
	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Add a shared room</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>");
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/share\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"node\" VALUE=\"");
	urlescputs(node);
	wprintf("\">\n");

	sprintf(buf, "NSET roomlist|%s", node);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			shptr = (struct sharelist *)
			    malloc(sizeof(struct sharelist));
			shptr->next = shlist;
			extract(shptr->shname, buf, 0);
			shlist = shptr;
		}
	}
	wprintf("<SELECT NAME=\"sroom\" SIZE=5 WIDTH=30>\n");
	serv_puts("LKRA");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(sroom, buf, 0);
			already_shared = 0;
			for (shptr = shlist; shptr != NULL; shptr = shptr->next) {
				if (!strcasecmp(sroom, shptr->shname))
					already_shared = 1;
			}

			if (already_shared == 0) {
				wprintf("<OPTION>");
				escputs(sroom);
				wprintf("\n");
			}
		}
	}
	wprintf("</SELECT>\n");
	wprintf("<BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Share\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);

	/* free the list */
	while (shlist != NULL) {
		shptr = shlist->next;
		free(shlist);
		shlist = shptr;
	}

}



void share(void)
{
	char node[256];
	char buf[256];
	char sc[256];
	char sroom[256];

	strcpy(node, bstr("node"));
	strcpy(sc, bstr("sc"));
	strcpy(sroom, bstr("sroom"));

	if (!strcmp(sc, "Share")) {
		sprintf(buf, "NSET share|%s|%s", node, sroom);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '1') {
			printf("HTTP/1.0 200 OK\n");
			output_headers(1, "bottom");
			server_to_text();
			wprintf("<A HREF=\"/display_netconf\">Back to menu</A>\n");
			wDumpContent(1);
		} else {
			display_error(&buf[4]);
		}

	}
}
