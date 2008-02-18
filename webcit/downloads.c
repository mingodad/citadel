/*
 * $Id$
 */
#include "webcit.h"
#include "webserver.h"

void display_room_directory(void)
{
	char buf[1024];
	char filename[256];
	char filesize[256];
	char mimetype[64];
	char comment[512];
	int bg = 0;
	char title[256];
	int havepics = 0;

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	snprintf(title, sizeof title, _("Files available for download in %s"), WC->wc_roomname);
	escputs(title);
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"downloads_background\"><tr><td>\n");
	wprintf("<tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th></tr>\n",
		_("Filename"),
		_("Size"),
		_("Content"),
		_("Description")
	);

	serv_puts("RDIR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000"))
	{
		extract_token(filename, buf, 0, '|', sizeof filename);
		extract_token(filesize, buf, 1, '|', sizeof filesize);
		extract_token(mimetype, buf, 2, '|', sizeof mimetype);
		extract_token(comment,  buf, 3, '|', sizeof comment);
		bg = 1 - bg;
		wprintf("<tr bgcolor=\"#%s\">", (bg ? "DDDDDD" : "FFFFFF"));
		wprintf("<td>"
			"<a href=\"download_file/");
		urlescputs(filename);
		wprintf("\"><img src=\"display_mime_icon?type=%s\" border=0 align=middle>\n", mimetype);
					escputs(filename);	wprintf("</a></td>");
		wprintf("<td>");	escputs(filesize);	wprintf("</td>");
		wprintf("<td>");	escputs(mimetype);	wprintf("</td>");
		wprintf("<td>");	escputs(comment);	wprintf("</td>");
		wprintf("</tr>\n");
		if (!havepics && (strstr(mimetype, "image") != NULL))
			havepics = 1;
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
	if (havepics)
		wprintf("<div class=\"buttons\"><a href=\"display_pictureview&frame=1\">%s</a></div>", _("Slideshow"));
	wDumpContent(1);
}


void display_pictureview(void)
{
	char buf[1024];
	char filename[256];
	char filesize[256];
	char mimetype[64];
	char comment[512];
	char title[256];
	int n = 0;
		

	if (atol(bstr("frame")) == 1) {

		output_headers(1, 1, 2, 0, 0, 0);
		wprintf("<div id=\"banner\">\n");
		wprintf("<h1>");
		snprintf(title, sizeof title, _("Pictures in %s"), WC->wc_roomname);
		escputs(title);
		wprintf("</h1>");
		wprintf("</div>\n");
		
		wprintf("<div id=\"content\" class=\"service\">\n");

		wprintf("<div class=\"fix_scrollbar_bug\">"
			"<table class=\"downloads_background\"><tr><td>\n");



		wprintf("<script type=\"text/javascript\" language=\"JavaScript\" > \nvar fadeimages=new Array()\n");

		serv_puts("RDIR");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000"))  {
				extract_token(filename, buf, 0, '|', sizeof filename);
				extract_token(filesize, buf, 1, '|', sizeof filesize);
				extract_token(mimetype, buf, 2, '|', sizeof mimetype);
				extract_token(comment,  buf, 3, '|', sizeof comment);
				if (strstr(mimetype, "image") != NULL) {
					wprintf("fadeimages[%d]=[\"download_file/", n);
					escputs(filename);
					wprintf("\", \"\", \"\"]\n");

					/*
							   //mimetype);
					   escputs(filename);	wprintf("</a></td>");
					   wprintf("<td>");	escputs(filesize);	wprintf("</td>");
					   wprintf("<td>");	escputs(mimetype);	wprintf("</td>");
					   wprintf("<td>");	escputs(comment);	wprintf("</td>");
					   wprintf("</tr>\n");
					*/
					n++;
				}
			}
		wprintf("</script>\n");
		wprintf("<tr><td><script type=\"text/javascript\" src=\"static/fadeshow.js\">\n</script>\n");
		wprintf("<script type=\"text/javascript\" >\n");
		wprintf("new fadeshow(fadeimages, 500, 400, 0, 3000, 1, \"R\");\n");
		wprintf("</script></td><th>\n");
		wprintf("</div>\n");
	}
	wDumpContent(1);


}

extern char* static_dirs[];
void display_mime_icon(void)
{
	char diskette[SIZ];

	snprintf (diskette, SIZ, "%s%s", static_dirs[0], "/diskette_24x.gif");
	output_static(diskette);

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
	const char *MimeType;
	char buf[1024];
	size_t bytes_transmitted = 0;
	size_t blocksize;
	struct wcsession *WCC = WC;     /* stack this for faster access (WC is a function) */

	MimeType = GuessMimeType(WCC->upload, WCC->upload_length); 
	serv_printf("UOPN %s|%s|%s", WCC->upload_filename, MimeType, bstr("description"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2')
	{
		strcpy(WCC->ImportantMessage, &buf[4]);
		display_room_directory();
		return;
	}

	while (bytes_transmitted < WCC->upload_length)
	{
		blocksize = 4096;
		if (blocksize > (WCC->upload_length - bytes_transmitted))
		{
			blocksize = (WCC->upload_length - bytes_transmitted);
		}
		serv_printf("WRIT %d", blocksize);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '7')
		{
			blocksize = atoi(&buf[4]);
			serv_write(&WCC->upload[bytes_transmitted], blocksize);
			bytes_transmitted += blocksize;
		}
	}

	serv_puts("UCLS 1");
	serv_getln(buf, sizeof buf);
	strcpy(WCC->ImportantMessage, &buf[4]);
	display_room_directory();
}
