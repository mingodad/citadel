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
