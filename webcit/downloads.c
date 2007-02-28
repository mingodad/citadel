/*
 * $Id: downloads.c 4849 2007-01-08 20:05:56Z ajc $
 */
#include "webcit.h"

void display_room_directory(void)
{
	char buf[1024];
	char filename[256];
	char filesize[256];
	char comment[512];
	int bg = 0;
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<table class=\"downloads_banner\"><tr><td>"
		"<span class=\"titlebar\">");
	snprintf(title, sizeof title, _("Files available for download in %s"), WC->wc_roomname);
	escputs(title);
	wprintf("</span>"
		"</td></tr></table>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"downloads_background\"><tr><td>\n");
	wprintf("<tr><th>%s</th><th>%s</th><th>%s</th></tr>\n",
			_("Filename"),
			_("Size"),
			_("Description")
	);

	serv_puts("RDIR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000"))
	{
		extract_token(filename, buf, 0, '|', sizeof filename);
		extract_token(filesize, buf, 1, '|', sizeof filesize);
		extract_token(comment, buf, 2, '|', sizeof comment);
		bg = 1 - bg;
		wprintf("<tr bgcolor=\"#%s\">", (bg ? "DDDDDD" : "FFFFFF"));
		wprintf("<td>"
			"<a href=\"download_file/");
		urlescputs(filename);
		wprintf("\"><img src=\"static/diskette_24x.gif\" border=0 align=middle>\n");
					escputs(filename);	wprintf("</a></td>");
		wprintf("<td>");	escputs(filesize);	wprintf("</td>");
		wprintf("<td>");	escputs(comment);	wprintf("</td>");
		wprintf("</tr>\n");
	}

	wprintf("</table></div>\n");
	wDumpContent(1);
}


void download_file(char *filename)
{
	char buf[256];
	off_t bytes;
	char content_type[256];
	char *content = NULL;

	/* Setting to nonzero forces a MIME type of application/octet-stream */
	int force_download = 1;
	
	serv_printf("OPEN %s", filename);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);
		content = malloc(bytes + 2);
		if (force_download) {
			strcpy(content_type, "application/octet-stream");
		}
		else {
			extract_token(content_type, &buf[4], 3, '|', sizeof content_type);
		}
		output_headers(0, 0, 0, 0, 0, 0);
		read_server_binary(content, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(content, bytes, content_type, 0);
		free(content);
	} else {
		wprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf(_("An error occurred while retrieving this file: %s\n"), &buf[4]);
	}

}

