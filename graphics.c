#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "webcit.h"
#include "child.h"


void display_graphics_upload(char *description, char *check_cmd, char *uplurl) {
	char buf[256];

	serv_puts(check_cmd);
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
		}

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);
        wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=770000><TR><TD>");
        wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
        wprintf("<B>Set/change %s</B>\n", description);
        wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n");
	wprintf("Please select a file to upload:<BR>\n");
	wprintf("<FORM ENCTYPE=\"multipart/form-data\" ACTION=\"%s\" METHOD=\"POST\">\n", uplurl);
        wprintf("<INPUT TYPE=\"FILE\" NAME=\"filename\" SIZE=\"35\">\n");
	wprintf("<BR>");
        wprintf("<INPUT TYPE=\"SUBMIT\" VALUE=\"Upload\">\n");
        wprintf("<INPUT TYPE=\"RESET\" VALUE=\"Reset Form\">\n");
        wprintf("</FORM>\n");
	wprintf("<A HREF=\"/display_main_menu\">Cancel</A>\n");
	wprintf("</CENTER></BODY></HTML>\n");
	wDumpContent();
	}

void do_graphics_upload(char *upl_cmd) {
	char buf[256];
	int bytes_remaining;
	int pos = 0;
	int thisblock;

	if (upload_length == 0) {
		display_error("You didn't upload a file.\n");
		return;
		}

	serv_puts(upl_cmd);
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
		}

	bytes_remaining = upload_length;
	while (bytes_remaining) {
		thisblock = ((bytes_remaining > 4096) ? 4096 : bytes_remaining);
		serv_printf("WRIT %d", thisblock);
		serv_gets(buf);
		if (buf[0] != '7') {
			display_error(&buf[4]);
			serv_puts("UCLS 0");
			serv_gets(buf);
			return;
			}
		thisblock = extract_int(&buf[4], 0);
		serv_write(&upload[pos], thisblock);
		pos = pos + thisblock;
		bytes_remaining = bytes_remaining - thisblock;
		}

	serv_puts("UCLS 1");
	serv_gets(buf);
	/* FIX display something other than an error if it's ok */
	if (buf[0] != 'x') {
		display_error(&buf[4]);
		return;
		}
	}
