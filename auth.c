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


void do_login() {
	char buf[256];
	char actual_username[256];

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

void do_welcome() {
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
