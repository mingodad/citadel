/*
 * vcard_edit.c
 *
 * Handles editing of vCard objects.
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
#include "vcard.h"



void do_edit_vcard(long msgnum, char *partnum, char *return_to) {
	char buf[SIZ];
	char *serialized_vcard = NULL;
	size_t total_len = 0;
	size_t bytes = 0;
	size_t thisblock = 0;
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
	char inetemail[SIZ];
	char extrafields[SIZ];

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
	inetemail[0] = 0;
	extrafields[0] = 0;

	output_headers(3);

	strcpy(whatuser, "");
	sprintf(buf, "MSG0 %ld|1", msgnum);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') {
		wDumpContent(1);
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

	total_len = atoi(&buf[4]);


	sprintf(buf, "OPNA %ld|%s", msgnum, partnum);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		wDumpContent(1);
		return;
	}

	total_len = atoi(&buf[4]);
	serialized_vcard = malloc(total_len + 1);
	while (bytes < total_len) {
		thisblock = 4000;
		if ((total_len - bytes) < thisblock) thisblock = total_len - bytes;
		sprintf(buf, "READ %d|%d", bytes, thisblock);
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '6') {
			thisblock = atoi(&buf[4]);
			serv_read(&serialized_vcard[bytes], thisblock);
			bytes += thisblock;
		}
		else {
			wprintf("Error: %s<BR>\n", &buf[4]);
		}
	}

	serv_puts("CLOS");
	serv_gets(buf);
	serialized_vcard[total_len + 1] = 0;

	v = vcard_load(serialized_vcard);
	free(serialized_vcard);

	/* Populate the variables for our form */
	i = 0;
	while (key = vcard_get_prop(v, "", 0, i, 1), key != NULL) {
		value = vcard_get_prop(v, "", 0, i++, 0);

		if (!strcasecmp(key, "n")) {
			extract_token(lastname, value, 0, ';');
			extract_token(firstname, value, 1, ';');
			extract_token(middlename, value, 2, ';');
			extract_token(prefix, value, 3, ';');
			extract_token(suffix, value, 4, ';');
		}

		else if (!strcasecmp(key, "adr")) {
			extract_token(pobox, value, 0, ';');
			extract_token(extadr, value, 1, ';');
			extract_token(street, value, 2, ';');
			extract_token(city, value, 3, ';');
			extract_token(state, value, 4, ';');
			extract_token(zipcode, value, 5, ';');
			extract_token(country, value, 6, ';');
		}

		else if (!strcasecmp(key, "tel;home")) {
			extract_token(hometel, value, 0, ';');
		}

		else if (!strcasecmp(key, "tel;work")) {
			extract_token(worktel, value, 0, ';');
		}

		else if (!strcasecmp(key, "email;internet")) {
			if (inetemail[0] != 0) {
				strcat(inetemail, "\n");
			}
			strcat(inetemail, value);
		}

		else {
			strcat(extrafields, key);
			strcat(extrafields, ":");
			strcat(extrafields, value);
			strcat(extrafields, "\n");
		}

	}
	
	vcard_free(v);

	/* Display the form */
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/submit_vcard\">\n");
	wprintf("<H2><IMG VALIGN=CENTER SRC=\"/static/vcard.gif\">"
		"Contact information for ");
	escputs(whatuser);
	wprintf("</H2>\n");

	wprintf("<TABLE border=0><TR>"
		"<TD>Prefix</TD>"
		"<TD>First</TD>"
		"<TD>Middle</TD>"
		"<TD>Last</TD>"
		"<TD>Suffix</TD></TR>\n");
	wprintf("<TR><TD><INPUT TYPE=\"text\" NAME=\"prefix\" "
		"VALUE=\"%s\" MAXLENGTH=\"5\"></TD>",
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
		"VALUE=\"%s\" MAXLENGTH=\"10\"></TD></TR></TABLE>\n",
		suffix);

	wprintf("<TABLE border=0><TR><TD>PO box (optional):</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"pobox\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR>\n",
		pobox);
	wprintf("<TR><TD>Address line 1:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"extadr\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR>\n",
		extadr);
	wprintf("<TR><TD>Address line 2:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"street\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR>\n",
		street);
	wprintf("<TR><TD>City:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"city\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\">\n",
		city);
	wprintf(" State: "
		"<INPUT TYPE=\"text\" NAME=\"state\" "
		"VALUE=\"%s\" MAXLENGTH=\"2\">\n",
		state);
	wprintf(" ZIP code: "
		"<INPUT TYPE=\"text\" NAME=\"zipcode\" "
		"VALUE=\"%s\" MAXLENGTH=\"10\"></TD></TR>\n",
		zipcode);
	wprintf("<TR><TD>Country:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"country\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR></TABLE>\n",
		country);

	wprintf("<TABLE BORDER=0><TR><TD>Home telephone:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"hometel\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR>\n",
		hometel);
	wprintf("<TR><TD>Work telephone:</TD>"
		"<TD><INPUT TYPE=\"text\" NAME=\"worktel\" "
		"VALUE=\"%s\" MAXLENGTH=\"29\"></TD></TR></TABLE>\n",
		worktel);

	wprintf("<TABLE border=0><TR><TD>Internet e-mail addresses:<BR>"
		"<FONT size=-2>For addresses in the Citadel directory, "
		"the topmost address will be used in outgoing mail."
		"</FONT></TD><TD>"
		"<TEXTAREA NAME=\"inetemail\" ROWS=5 COLS=40 WIDTH=40>");
	escputs(inetemail);
	wprintf("</TEXTAREA></TD></TR></TABLE><BR>\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"extrafields\" VALUE=\"");
	escputs(extrafields);
	wprintf("\">\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"return_to\" VALUE=\"");
	urlescputs(return_to);
	wprintf("\">\n");

	wprintf("<CENTER>\n");
                wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"OK\">");
                wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
                wprintf("</CENTER></FORM>\n");

	 
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
	
	for (i=0; i<num_tokens(bstr("inetemail"), '\n'); ++i) {
		extract_token(buf, bstr("inetemail"), i, '\n');
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
	else {
		readloop("readnew");
	}
}
