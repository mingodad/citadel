
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

void display_graphics_upload(char *description, char *check_cmd, char *uplurl)
{
	char buf[SIZ];

	serv_puts(check_cmd);
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
	}
	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Set/change %s</B>\n", description);
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>\n");

	wprintf("<FORM ENCTYPE=\"multipart/form-data\" ACTION=\"%s&which_room=%s\" METHOD=\"POST\">\n", uplurl, bstr("which_room"));

	wprintf("You can upload any image directly from your computer,\n");
	wprintf("as long as it is in GIF format (JPEG, PNG, etc. won't\n");
	wprintf("work).<BR><BR>\n");

	wprintf("Please select a file to upload:<BR>\n");
	wprintf("<INPUT TYPE=\"FILE\" NAME=\"filename\" SIZE=\"35\">\n");
	wprintf("<BR>");
	wprintf("<INPUT TYPE=\"SUBMIT\" VALUE=\"Upload\">\n");
	wprintf("<INPUT TYPE=\"RESET\" VALUE=\"Reset Form\">\n");
	wprintf("</FORM>\n");
	wprintf("<A HREF=\"/display_main_menu\">Cancel</A>\n");
	wprintf("</CENTER>\n");
	wDumpContent(1);
}

void do_graphics_upload(char *upl_cmd)
{
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;

	if (WC->upload_length == 0) {
		display_error("You didn't upload a file.\n");
		return;
	}
	serv_puts(upl_cmd);
	serv_gets(buf);
	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
	}
	bytes_remaining = WC->upload_length;
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
		serv_write(&WC->upload[pos], thisblock);
		pos = pos + thisblock;
		bytes_remaining = bytes_remaining - thisblock;
	}

	serv_puts("UCLS 1");
	serv_gets(buf);
	if (buf[0] != 'x') {
		display_success(&buf[4]);
		return;
	}
}
