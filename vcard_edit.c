/*
 * $Id$
 *
 * Handles editing of vCard objects.
 *
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
#include "vcard.h"


/* Edit the vCard component of a MIME message.  Supply the message number
 * and MIME part number to fetch.  Or, specify -1 for the message number
 * to start with a blank card.
 */
void do_edit_vcard(long msgnum, char *partnum, char *return_to) {
	char buf[SIZ];
	char *serialized_vcard = NULL;
	size_t total_len = 0;
	struct vCard *v;
	int i;
	char *key, *value;
	char whatuser[SIZ];

	char lastname[SIZ];
	char firstname[SIZ];
	char middlename[SIZ];
	char prefix[SIZ];
	char suffix[SIZ];
	char pobox[SIZ];
	char extadr[SIZ];
	char street[SIZ];
	char city[SIZ];
	char state[SIZ];
	char zipcode[SIZ];
	char country[SIZ];
	char hometel[SIZ];
	char worktel[SIZ];
	char primary_inetemail[SIZ];
	char other_inetemail[SIZ];
	char extrafields[SIZ];
	char title[SIZ];
	char org[SIZ];

	lastname[0] = 0;
	firstname[0] = 0;
	middlename[0] = 0;
	prefix[0] = 0;
	suffix[0] = 0;
	pobox[0] = 0;
	extadr[0] = 0;
	street[0] = 0;
	city[0] = 0;
	state[0] = 0;
	zipcode[0] = 0;
	country[0] = 0;
	hometel[0] = 0;
	worktel[0] = 0;
	primary_inetemail[0] = 0;
	other_inetemail[0] = 0;
	title[0] = 0;
	org[0] = 0;
	extrafields[0] = 0;

	strcpy(whatuser, "");

	if (msgnum >= 0) {
		sprintf(buf, "MSG0 %ld|1", msgnum);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '1') {
			convenience_page("770000", "Error", &buf[4]);
			return;
		}
		while (serv_gets(buf), strcmp(buf, "000")) {
			if (!strncasecmp(buf, "from=", 5)) {
				strcpy(whatuser, &buf[5]);
			}
			else if (!strncasecmp(buf, "node=", 5)) {
				strcat(whatuser, " @ ");
				strcat(whatuser, &buf[5]);
			}
		}
	
		sprintf(buf, "OPNA %ld|%s", msgnum, partnum);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] != '2') {
			convenience_page("770000", "Error", &buf[4]);
			return;
		}
	
		total_len = atoi(&buf[4]);
		serialized_vcard = malloc(total_len + 2);
	
		read_server_binary(serialized_vcard, total_len);
	
		serv_puts("CLOS");
		serv_gets(buf);
		serialized_vcard[total_len] = 0;
	
		v = vcard_load(serialized_vcard);
		free(serialized_vcard);
	
		/* Populate the variables for our form */
		i = 0;
		while (key = vcard_get_prop(v, "", 0, i, 1), key != NULL) {
			value = vcard_get_prop(v, "", 0, i++, 0);
	
			if (!strcasecmp(key, "n")) {
				extract_token(lastname, value, 0, ';', sizeof lastname);
				extract_token(firstname, value, 1, ';', sizeof firstname);
				extract_token(middlename, value, 2, ';', sizeof middlename);
				extract_token(prefix, value, 3, ';', sizeof prefix);
				extract_token(suffix, value, 4, ';', sizeof suffix);
			}

			else if (!strcasecmp(key, "title")) {
				strcpy(title, value);
			}
	
			else if (!strcasecmp(key, "org")) {
				strcpy(org, value);
			}
	
			else if (!strcasecmp(key, "adr")) {
				extract_token(pobox, value, 0, ';', sizeof pobox);
				extract_token(extadr, value, 1, ';', sizeof extadr);
				extract_token(street, value, 2, ';', sizeof street);
				extract_token(city, value, 3, ';', sizeof city);
				extract_token(state, value, 4, ';', sizeof state);
				extract_token(zipcode, value, 5, ';', sizeof zipcode);
				extract_token(country, value, 6, ';', sizeof country);
			}
	
			else if (!strcasecmp(key, "tel;home")) {
				extract_token(hometel, value, 0, ';', sizeof hometel);
			}
	
			else if (!strcasecmp(key, "tel;work")) {
				extract_token(worktel, value, 0, ';', sizeof worktel);
			}
	
			else if (!strcasecmp(key, "email;internet")) {
				if (primary_inetemail[0] == 0) {
					strcpy(primary_inetemail, value);
				}
				else {
					if (other_inetemail[0] != 0) {
						strcat(other_inetemail, "\n");
					}
					strcat(other_inetemail, value);
				}
			}
	
			else {
				strcat(extrafields, key);
				strcat(extrafields, ":");
				strcat(extrafields, value);
				strcat(extrafields, "\n");
			}
	
		}
	
		vcard_free(v);
	}

	/* Display the form */
	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">"
		"<img src=\"/static/vcard.gif\">"
		"Edit contact information"
		"</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/submit_vcard\">\n");
	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf("<TABLE border=0><TR>"
		"<TD>Prefix</TD>"
		"<TD>First</TD>"
		"<TD>Middle</TD>"
		"<TD>Last</TD>"
		"<TD>Suffix</TD></TR>\n");
	wprintf("<TR><TD><INPUT TYPE=\"text\" NAME=\"prefix\" "
		"VALUE=\"%s\" MAXLENGTH=\"5\" SIZE=\"5\"></TD>",
		prefix);
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"firstname\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD>",
		firstname);
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"middlename\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD>",
		middlename);
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"lastname\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD>",
		lastname);
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"suffix\" "
		"VALUE=\"%s\" MAXLENGTH=\"10\" SIZE=\"10\"></TD></TR></TABLE>\n",
		suffix);

	wprintf("<table border=0 width=100%% bgcolor=\"#dddddd\">");
	wprintf("<tr><td>");

	wprintf("Title:<br>"
		"<INPUT TYPE=\"text\" NAME=\"title\" "
		"VALUE=\"%s\" MAXLENGTH=\"40\"><br><br>\n",
		title
	);

	wprintf("Organization:<br>"
		"<INPUT TYPE=\"text\" NAME=\"org\" "
		"VALUE=\"%s\" MAXLENGTH=\"40\"><br><br>\n",
		org
	);

	wprintf("</td><td>");

	wprintf("<table border=0>");
	wprintf("<tr><td>PO box:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"pobox\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></td></tr>\n",
		pobox);
	wprintf("<tr><td>Address:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"extadr\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></td></tr>\n",
		extadr);
	wprintf("<tr><td> </td><td>"
		"<INPUT TYPE=\"text\" NAME=\"street\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></td></tr>\n",
		street);
	wprintf("<tr><td>City:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"city\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></td></tr>\n",
		city);
	wprintf("<tr><td>State:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"state\" "
		"VALUE=\"%s\" MAXLENGTH=\"2\"></td></tr>\n",
		state);
	wprintf("<tr><td>ZIP code:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"zipcode\" "
		"VALUE=\"%s\" MAXLENGTH=\"10\"></td></tr>\n",
		zipcode);
	wprintf("<tr><td>Country:</td><td>"
		"<INPUT TYPE=\"text\" NAME=\"country\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\" WIDTH=\"5\"></td></tr>\n",
		country);
	wprintf("</table>\n");

	wprintf("</table>\n");

	wprintf("<TABLE BORDER=0><TR><TD>Home telephone:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"hometel\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD>\n",
		hometel);
	wprintf("<TD>Work telephone:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"worktel\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR></TABLE>\n",
		worktel);

	wprintf("<table border=0 width=100%% bgcolor=\"#dddddd\">");
	wprintf("<tr><td>");

	wprintf("<TABLE border=0><TR>"
		"<TD VALIGN=TOP>Primary Internet e-mail address<br />"
		"<INPUT TYPE=\"text\" NAME=\"primary_inetemail\" "
		"SIZE=40 MAXLENGTH=40 VALUE=\"");
	escputs(primary_inetemail);
	wprintf("\"><br />"
		"</TD><TD VALIGN=TOP>"
		"Internet e-mail aliases<br />"
		"<TEXTAREA NAME=\"other_inetemail\" ROWS=5 COLS=40 WIDTH=40>");
	escputs(other_inetemail);
	wprintf("</TEXTAREA></TD></TR></TABLE>\n");

	wprintf("</td></tr></table>\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"extrafields\" VALUE=\"");
	escputs(extrafields);
	wprintf("\">\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"return_to\" VALUE=\"");
	urlescputs(return_to);
	wprintf("\">\n");

	wprintf("<CENTER>\n"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">"
		"&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">"
		"</CENTER></FORM>\n"
	);
	
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}



void edit_vcard(void) {
	long msgnum;
	char *partnum;

	msgnum = atol(bstr("msgnum"));
	partnum = bstr("partnum");
	do_edit_vcard(msgnum, partnum, "");
}




void submit_vcard(void) {
	char buf[SIZ];
	int i;

	if (strcmp(bstr("sc"), "OK")) { 
		readloop("readnew");
		return;
	}

	sprintf(buf, "ENT0 1|||4||");
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '4') {
		edit_vcard();
		return;
	}

	serv_puts("Content-type: text/x-vcard");
	serv_puts("");
	serv_puts("begin:vcard");
	serv_printf("n:%s;%s;%s;%s;%s",
		bstr("lastname"),
		bstr("firstname"),
		bstr("middlename"),
		bstr("prefix"),
		bstr("suffix") );
	serv_printf("title:%s", bstr("title") );
	serv_printf("org:%s", bstr("org") );
	serv_printf("adr:%s;%s;%s;%s;%s;%s;%s",
		bstr("pobox"),
		bstr("extadr"),
		bstr("street"),
		bstr("city"),
		bstr("state"),
		bstr("zipcode"),
		bstr("country") );
	serv_printf("tel;home:%s", bstr("hometel") );
	serv_printf("tel;work:%s", bstr("worktel") );

	serv_printf("email;internet:%s\n", bstr("primary_inetemail"));	
	for (i=0; i<num_tokens(bstr("other_inetemail"), '\n'); ++i) {
		extract_token(buf, bstr("other_inetemail"), i, '\n', sizeof buf);
		if (strlen(buf) > 0) {
			serv_printf("email;internet:%s", buf);
		}
	}

	serv_printf("%s", bstr("extrafields") );
	serv_puts("end:vcard");
	serv_puts("000");

	if (!strcmp(bstr("return_to"), "/select_user_to_edit")) {
		select_user_to_edit(NULL, NULL);
	}
	else if (!strcmp(bstr("return_to"), "/do_welcome")) {
		do_welcome();
	}
	else {
		readloop("readnew");
	}
}
