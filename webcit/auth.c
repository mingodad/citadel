/*
 * auth.c
 *
 * This file contains code which relates to authentication of users to Citadel.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "webcit.h"

void display_login_page() {
	
	printf("HTTP/1.0 200 OK\n");
	output_headers();

	wprintf("<HTML><HEAD><TITLE>Please log in</TITLE></HEAD><BODY>\n");
	wprintf("<TABLE border=0><TR><TD>\n");
	wprintf("<IMG SRC=\"/static/velma.gif\">\n");	
	wprintf("</TD><TD>");
	wprintf("<H1>&quot;Velma&quot;</H1><H2>(next generation WebCit)</H2>");
	wprintf("Please log in...<BR>\n");


	wprintf("<FORM ACTION=\"/login\" METHOD=\"POST\">\n");
	wprintf("<TABLE border><TR><TD>");
	wprintf("User Name:</TD><TD><INPUT TYPE=\"text\" NAME=\"name\" MAXLENGTH=\"25\"   >\n");
	wprintf("</TD></TR><TD>");
	wprintf("Password:</TD><TD><INPUT TYPE=\"password\" NAME=\"pass\" MAXLENGTH=\"20\">");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Login\">\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"New User\">\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Exit\">\n");
	wprintf("</FORM>\n");

	wprintf("</TD></TR></TABLE>\n");
	wprintf("</BODY></HTML>\n");

	wDumpContent();
	}




/*
 * This function needs to get called whenever a PASS or NEWU succeeds
 */
void become_logged_in(char *user, char *pass, char *serv_response) {

	logged_in = 1;
	strcpy(wc_username, user);
	strcpy(wc_password, pass);
	
	}


void do_login() {
	char buf[256];

	if (!strcasecmp(bstr("action"), "Login")) {
		serv_printf("USER %s", bstr("name"));
		serv_gets(buf);
		if (buf[0]=='3') {
			serv_printf("PASS %s", bstr("pass"));
			serv_gets(buf);
			if (buf[0]=='2') {
				become_logged_in(bstr("name"), bstr("pass"), buf);
				}
			}
		}

	if (logged_in) {
		output_static("frameset.html");
		}
	else {
		printf("HTTP/1.0 200 OK\n");
		output_headers();
		wprintf("<HTML><HEAD><TITLE>Nope</TITLE></HEAD><BODY>\n");
		wprintf("Your password was not accepted.\n");
		wprintf("<HR><A HREF=\"/\">Try again</A>\n");
		wprintf("</BODY></HTML>\n");
		wDumpContent();
		}

	}
