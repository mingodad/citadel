/*
 * auth.c
 *
 * This file contains code which relates to authentication of users to Citadel.
 *
 * $Id$
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include "webcit.h"
#include "child.h"



/*
 * Display the login screen
 */
void display_login(void) {
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers();

	wprintf("<HTML><BODY>\n");
	wprintf("<CENTER><TABLE border=0><TR><TD>\n");

	/* FIX replace with the correct image */
	wprintf("<IMG SRC=\"/static/velma.gif\">");
	wprintf("</TD><TD><CENTER>\n");

	serv_puts("MESG hello");
	serv_gets(buf);
	if (buf[0]=='1') fmout(NULL);

	wprintf("</CENTER></TD></TR></TABLE></CENTER>\n");

	wprintf("<HR>\n");
	/* FIX add instructions here */
	wprintf("<CENTER><FORM ACTION=\"/login\" METHOD=\"POST\">\n");
	wprintf("<TABLE border><TR>\n");
	wprintf("<TD>User Name:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"text\" NAME=\"name\" MAXLENGTH=\"25\">\n");
	wprintf("</TD></TR><TR>\n");
	wprintf("<TD>Password:</TD>\n");
	wprintf("<TD><INPUT TYPE=\"password\" NAME=\"pass\" MAXLENGTH=\"20\"></TD>\n");
	wprintf("</TR></TABLE>\n");
	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Login\">\n");
        wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"New User\">\n");
        wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Exit\">\n");
        wprintf("</FORM></CENTER>\n");
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}




/*
 * This function needs to get called whenever a PASS or NEWU succeeds
 */
void become_logged_in(char *user, char *pass, char *serv_response) {
	logged_in = 1;
	extract(wc_username, &serv_response[4], 0);
	strcpy(wc_password, pass);
	axlevel = extract_int(&serv_response[4], 1);
	if (axlevel >=6) is_aide = 1;
	}


void do_login(void) {
	char buf[256];

	fprintf(stderr, "do_login() called\n");
	if (!strcasecmp(bstr("action"), "Login")) {
		serv_printf("USER %s", bstr("name"));
		serv_gets(buf);
		if (buf[0]=='3') {
			serv_printf("PASS %s", bstr("pass"));
			serv_gets(buf);
			if (buf[0]=='2') {
				become_logged_in(bstr("name"),
					bstr("pass"), buf);
				}
			}
		}

	fprintf(stderr, "logged_in==%d\n", logged_in);

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

void do_welcome(void) {
	printf("HTTP/1.0 200 OK\n");
	output_headers();
	wprintf("<HTML><BODY>\n");
	wprintf("<CENTER><H1>");
	escputs(wc_username);
	wprintf("</H1>\n");
	/* other stuff here */
	wprintf("</BODY></HTML>\n");
	wDumpContent();
	}


void do_logout(void) {
	char buf[256];

	strcpy(wc_username, "");
	strcpy(wc_password, "");
	strcpy(wc_roomname, "");
	strcpy(wc_host, "");
	strcpy(wc_port, "");

	printf("HTTP/1.0 200 OK\n");
	output_headers();
	printf("X-WebCit-Session: close\n");
	
	wprintf("<HTML><HEAD><TITLE>Goodbye</TITLE></HEAD><BODY><CENTER>\n");

	serv_puts("MESG goodbye");
	serv_gets(buf);

	if (buf[0]=='1') fmout(NULL);
	else wprintf("Goodbye\n");

	wprintf("</CENTER></BODY></HTML>\n");
	wDumpContent();
	serv_puts("QUIT");
	exit(0);
	}
