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
	wprintf("Please log in...\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</BODY></HTML>\n");

	wDumpContent();
	}
