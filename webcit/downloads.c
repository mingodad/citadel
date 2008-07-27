/*
 * $Id$
 */
#include "webcit.h"
#include "webserver.h"

typedef struct _FileListStruct {
	char Filename[256];
	int FilenameLen;
	long FileSize;
	char MimeType[64];
	int MimeTypeLen;
	char Comment[512];
	int CommentLen;
	int IsPic;
	int Sequence;
}FileListStruct;


int CompareFilelistByMime(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);

	if (File1->IsPic != File2->IsPic)
		return File1->IsPic > File2->IsPic;
	return strcasecmp(File1->MimeType, File2->MimeType);
}
int CompareFilelistByMimeRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	if (File1->IsPic != File2->IsPic)
		return File1->IsPic < File2->IsPic;
	return strcasecmp(File2->MimeType, File1->MimeType);
}

int CompareFilelistBySize(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	if (File1->FileSize == File2->FileSize)
		return 0;
	return (File1->FileSize > File2->FileSize);
}

int CompareFilelistBySizeRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	if (File1->FileSize == File2->FileSize)
		return 0;
	return (File1->FileSize < File2->FileSize);
}

int CompareFilelistByComment(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return strcasecmp(File1->Comment, File2->Comment);
}
int CompareFilelistByCommentRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return strcasecmp(File2->Comment, File1->Comment);
}

int CompareFilelistBySequence(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return (File2->Sequence >  File1->Sequence);
}

HashList* LoadFileList(int *HavePic)
{
	FileListStruct *Entry;
	HashList *Files;
	int HavePics = 0;
	int Order;
	int sequence = 0;
	char buf[1024];

	serv_puts("RDIR");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		Files = NewHash(1, NULL);
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000"))
		{
			Entry = (FileListStruct*) malloc(sizeof (FileListStruct));

			Entry->FilenameLen = extract_token(
				Entry->Filename, buf, 0, '|', 
				sizeof(Entry->Filename));
			Entry->FileSize = extract_long(buf, 1);
			Entry->MimeTypeLen = extract_token(
				Entry->MimeType, buf, 2, '|', 
				sizeof(Entry->MimeType));
			Entry->CommentLen = extract_token(
				Entry->Comment,  buf, 3, '|', 
				sizeof(Entry->Comment));
			
			Entry->Sequence = sequence++;
			Entry->IsPic = (strstr(Entry->MimeType, "image") != NULL);
			if (!HavePics && Entry->IsPic) {
				HavePics = 1;
				*HavePic = 1;
			}
			Put(Files, Entry->Filename, 
			    Entry->FilenameLen, 
			    Entry, NULL);
		}
		Order = ibstr("SortOrder");
		switch (ibstr("SortBy")){
		case 1: /*NAME*/
			SortByHashKey(Files,Order);
			break;
		case 2: /* SIZE*/
			SortByPayload(Files, (Order)? 
				      CompareFilelistBySize:
				      CompareFilelistBySizeRev);
			break;
		case 3: /*MIME*/
			SortByPayload(Files, (Order)? 
				      CompareFilelistByMime:
				      CompareFilelistByMimeRev);
			break;
		case 4: /*COMM*/
			SortByPayload(Files, (Order)? 
				      CompareFilelistByComment:
				      CompareFilelistByCommentRev);
			break;
		default:
			SortByPayload(Files, CompareFilelistBySequence);
		}
		return Files;
	}
	else
		return NULL;
}

void FilePrintEntry(const char *FileName, void *vFile, int odd)
{
	FileListStruct *File = (FileListStruct*) vFile;

	wprintf("<tr bgcolor=\"#%s\">", (odd ? "DDDDDD" : "FFFFFF"));
	wprintf("<td>"
		"<a href=\"download_file/");
	urlescputs(File->Filename);
	wprintf("\"><img src=\"display_mime_icon?type=%s\" border=0 align=middle>\n", 
		File->MimeType);
	escputs(File->Filename);	wprintf("</a></td>");
	wprintf("<td>%ld</td>", File->FileSize);
	wprintf("<td>");	escputs(File->MimeType);	wprintf("</td>");
	wprintf("<td>");	escputs(File->Comment);	wprintf("</td>");
	wprintf("</tr>\n");
}

void FilePrintTransition(void *vFile1, void *vFile2, int odd)
{
	FileListStruct *File1 = (FileListStruct*) vFile1;
	FileListStruct *File2 = (FileListStruct*) vFile2;
	char StartChar[2] = "\0\0";
	char *SectionName;

	switch (ibstr("SortBy")){
	case 1: /*NAME*/
		if ((File2 != NULL) && 
		    ((File1 == NULL) ||
		     (File1->Filename[0] != File2->Filename[0]))) {
			StartChar[0] = File2->Filename[0];
			SectionName = StartChar;
		}
		else return;
		break;
	case 2: /* SIZE*/
		return;
	case 3: /*MIME*/
		return; /*TODO*/
		break;
	case 4: /*COMM*/
		return;
	default:
		return;
	}

	wprintf("<tr bgcolor=\"#%s\"><th colspan = 4>%s</th></tr>", 
		(odd ? "DDDDDD" : "FFFFFF"),  SectionName);
}


void display_room_directory(void)
{
	char title[256];
	int havepics = 0;
	HashList *Files;
	int Order;
	int SortRow;
	int i;

	long SortDirections[5] = {2,2,2,2,2};
	const char* SortIcons[3] = {
		"static/up_pointer.gif",
		"static/down_pointer.gif",
		"static/sort_none.gif"};
	char *RowNames[5] = {"",
			    _("Filename"),
			    _("Size"),
			    _("Content"),
			    _("Description")};

	Files = LoadFileList (&havepics);
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


	Order = ibstr("SortOrder");
	if (!havebstr("SortOrder") || Order > 2)
		Order = 2; /* <- Unsorted... */
	SortRow = ibstr("SortBy");

	SortDirections[SortRow] = Order;

	wprintf("<tr>\n");
	for (i = 1; i < 5; i++) {
		switch (SortDirections[i]) {
		default:
		case 0:
			Order = 2;
			break;
		case 1:
			Order = 0;
			break;
		case 2:
			Order = 1;
			break;
		}			

		wprintf("  <th>%s \n"
			"      <a href=\"display_room_directory?SortOrder=%d&SortBy=%d\"> \n"
			"       <img src=\"%s\" border=\"0\"></a>\n"
			"  </th>\n",
			RowNames[i],
			Order, i,
			SortIcons[SortDirections[i]]
			);

	}
	wprintf("</tr>\n");
//<th>%s</th><th>%s</th><th>%s</th><th>%s</th></tr>\n",
	//);

	if (Files != NULL) {
		PrintHash(Files, FilePrintTransition, FilePrintEntry);
		DeleteHash(&Files);
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
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

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
		

	if (lbstr("frame") == 1) {

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
	char FileBuf[SIZ];
	const char *FileName;
	char *MimeType;
	size_t tlen;

	MimeType = xbstr("type", &tlen);
	FileName = GetIconFilename(MimeType, tlen);

	if (FileName == NULL)
		snprintf (FileBuf, SIZ, "%s%s", static_dirs[0], "/diskette_24x.gif");
	else
		snprintf (FileBuf, SIZ, "%s%s", static_dirs[3], FileName);
	output_static(FileBuf);

}

void download_file(void)
{
	StrBuf *Buf;
	char buf[256];
	off_t bytes;
	char content_type[256];
	char *content = NULL;

	/* Setting to nonzero forces a MIME type of application/octet-stream */
	int force_download = 1;
	
	safestrncpy(buf, ChrPtr(WC->UrlFragment1), sizeof buf);
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
		Buf = read_server_binary(bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(Buf, content_type, 0);
		free(content);
	} else {
		hprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf(_("An error occurred while retrieving this file: %s\n"), &buf[4]);
		end_burst();
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

void 
InitModule_DOWNLOAD
(void)
{
	WebcitAddUrlHandler(HKEY("display_room_directory"), display_room_directory, 0);
	WebcitAddUrlHandler(HKEY("display_pictureview"), display_pictureview, 0);
	WebcitAddUrlHandler(HKEY("download_file"), download_file, NEED_URL);
	WebcitAddUrlHandler(HKEY("upload_file"), upload_file, 0);
}
