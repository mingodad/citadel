
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
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	output_headers(1, 1, 0, 0, 0, 0, 0);

	svprintf("BOXTITLE", WCS_STRING, "Set/change your photo");
	do_template("beginbox");

	wprintf("<CENTER>\n");

	wprintf("<FORM ENCTYPE=\"multipart/form-data\" ACTION=\"%s\" "
		"METHOD=\"POST\" NAME=\"graphicsupload\">\n", uplurl);

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"which_room\" VALUE=\"");
	urlescputs(bstr("which_room"));
	wprintf("\">\n");

	wprintf("You can upload any image directly from your computer,\n");
	wprintf("as long as it is in GIF format (JPEG, PNG, etc. won't\n");
	wprintf("work).<br /><br />\n");

	wprintf("Please select a file to upload:<br /><br />\n");
	wprintf("<INPUT TYPE=\"FILE\" NAME=\"filename\" SIZE=\"35\">\n");
	wprintf("<br /><br />");
	wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" VALUE=\"Upload\">\n");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"RESET\" VALUE=\"Reset Form\">\n");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" VALUE=\"Cancel\">\n");
	wprintf("</FORM>\n");
	wprintf("</CENTER>\n");
	do_template("endbox");
	wDumpContent(1);
}

void do_graphics_upload(char *upl_cmd)
{
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;

	if (!strcasecmp(bstr("sc"), "Cancel")) {
		strcpy(WC->ImportantMessage,
			"Graphics upload cancelled.");
		display_main_menu();
		return;
	}

	if (WC->upload_length == 0) {
		strcpy(WC->ImportantMessage,
			"You didn't upload a file.");
		display_main_menu();
		return;
	}
	serv_puts(upl_cmd);
	serv_gets(buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	bytes_remaining = WC->upload_length;
	while (bytes_remaining) {
		thisblock = ((bytes_remaining > 4096) ? 4096 : bytes_remaining);
		serv_printf("WRIT %d", thisblock);
		serv_gets(buf);
		if (buf[0] != '7') {
			strcpy(WC->ImportantMessage, &buf[4]);
			serv_puts("UCLS 0");
			serv_gets(buf);
			display_main_menu();
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
