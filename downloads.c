/*
 * $Id$
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

	wprintf("</table>\n");

	/** Now offer the ability to upload files... */
	if (WC->room_flags & QR_UPLOAD)
	{
		wprintf("<hr>");
		wprintf("<form "
			"enctype=\"multipart/form-data\" "
			"method=\"POST\" "
			"accept-charset=\"UTF-8\" "
			"action=\"upload_file\" "
			"name=\"upload_file_form\""
			">\n"
		);
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);

		wprintf(_("Upload a file:"));
		wprintf("&nbsp;<input NAME=\"filename\" SIZE=16 TYPE=\"file\">&nbsp;\n");
		wprintf(_("Description:"));
		wprintf("&nbsp;<input type=\"text\" name=\"description\" maxlength=\"64\" size=\"64\">&nbsp;");
		wprintf("<input type=\"submit\" name=\"attach_button\" value=\"%s\">\n", _("Upload"));

		wprintf("</form>\n");
	}

	wprintf("</div>\n");
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
	
	safestrncpy(buf, filename, sizeof buf);
	unescape_input(buf);
	serv_printf("OPEN %s", buf);
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



void upload_file(void)
{
	char buf[1024];
	size_t bytes_transmitted = 0;
	size_t blocksize;

	serv_printf("UOPN %s|%s", WC->upload_filename, bstr("description"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2')
	{
		strcpy(WC->ImportantMessage, &buf[4]);
		display_room_directory();
		return;
	}

	while (bytes_transmitted < WC->upload_length)
	{
		blocksize = 4096;
		if (blocksize > (WC->upload_length - bytes_transmitted))
		{
			blocksize = (WC->upload_length - bytes_transmitted);
		}
		serv_printf("WRIT %d", blocksize);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '7')
		{
			blocksize = atoi(&buf[4]);
			serv_write(&WC->upload[bytes_transmitted], blocksize);
			bytes_transmitted += blocksize;
		}
	}

	serv_puts("UCLS 1");
	serv_getln(buf, sizeof buf);
	strcpy(WC->ImportantMessage, &buf[4]);
	display_room_directory();
}
