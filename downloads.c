/*
 * $Id$
 */
#include "webcit.h"
#include "webserver.h"

extern char* static_dirs[];

typedef struct _FileListStruct {
	StrBuf *Filename;
	long FileSize;
	StrBuf *MimeType;
	StrBuf *Comment;
	int IsPic;
	int Sequence;
} FileListStruct;

void FreeFiles(void *vFile)
{
	FileListStruct *F = (FileListStruct*) vFile;
	FreeStrBuf(&F->Filename);
	FreeStrBuf(&F->MimeType);
	FreeStrBuf(&F->Comment);
	free(F);
}

/* -------------------------------------------------------------------------------- */
void tmplput_FILE_NAME(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *F = (FileListStruct*) Context;
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, F->Filename, 0);
}
void tmplput_FILE_SIZE(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *F = (FileListStruct*) Context;
	StrBufAppendPrintf(Target, "%ld", F->FileSize);
}
void tmplput_FILEMIMETYPE(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *F = (FileListStruct*) Context;
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, F->MimeType, 0);
}
void tmplput_FILE_COMMENT(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *F = (FileListStruct*) Context;
	StrBufAppendTemplate(Target, nArgs, Tokens, Context, ContextType, F->Comment, 0);
}

/* -------------------------------------------------------------------------------- */

int Conditional_FILE_ISPIC(WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *F = (FileListStruct*) Context;
	return F->IsPic;
}

/* -------------------------------------------------------------------------------- */
int CompareFilelistByMime(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);

	if (File1->IsPic != File2->IsPic)
		return File1->IsPic > File2->IsPic;
	return strcasecmp(ChrPtr(File1->MimeType), ChrPtr(File2->MimeType));
}
int CompareFilelistByMimeRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	if (File1->IsPic != File2->IsPic)
		return File1->IsPic < File2->IsPic;
	return strcasecmp(ChrPtr(File2->MimeType), ChrPtr(File1->MimeType));
}
int GroupchangeFilelistByMime(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) vFile1;
	FileListStruct *File2 = (FileListStruct*) vFile2;

	if (File1->IsPic != File2->IsPic)
		return File1->IsPic > File2->IsPic;
	return strcasecmp(ChrPtr(File1->MimeType), ChrPtr(File2->MimeType)) != 0;
}


int CompareFilelistByName(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);

	if (File1->IsPic != File2->IsPic)
		return File1->IsPic > File2->IsPic;
	return strcasecmp(ChrPtr(File1->Filename), ChrPtr(File2->Filename));
}
int CompareFilelistByNameRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	if (File1->IsPic != File2->IsPic)
		return File1->IsPic < File2->IsPic;
	return strcasecmp(ChrPtr(File2->Filename), ChrPtr(File1->Filename));
}
int GroupchangeFilelistByName(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) vFile1;
	FileListStruct *File2 = (FileListStruct*) vFile2;

	return ChrPtr(File1->Filename)[0] != ChrPtr(File2->Filename)[0];
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
int GroupchangeFilelistBySize(const void *vFile1, const void *vFile2)
{
	return 0;
}


int CompareFilelistByComment(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return strcasecmp(ChrPtr(File1->Comment), ChrPtr(File2->Comment));
}
int CompareFilelistByCommentRev(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return strcasecmp(ChrPtr(File2->Comment), ChrPtr(File1->Comment));
}
int GroupchangeFilelistByComment(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) vFile1;
	FileListStruct *File2 = (FileListStruct*) vFile2;
	return ChrPtr(File1->Comment)[9] != ChrPtr(File2->Comment)[0];
}


int CompareFilelistBySequence(const void *vFile1, const void *vFile2)
{
	FileListStruct *File1 = (FileListStruct*) GetSearchPayload(vFile1);
	FileListStruct *File2 = (FileListStruct*) GetSearchPayload(vFile2);
	return (File2->Sequence >  File1->Sequence);
}
int GroupchangeFilelistBySequence(const void *vFile1, const void *vFile2)
{
	return 0;
}

/* -------------------------------------------------------------------------------- */
HashList* LoadFileList(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
{
	FileListStruct *Entry;
	StrBuf *Buf;
	HashList *Files;
	int Done = 0;
	int sequence = 0;
	char buf[1024];
	CompareFunc SortIt;
	int HavePic;

	serv_puts("RDIR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return NULL;

	Buf = NewStrBuf();	       
	Files = NewHash(1, NULL);
	while (!Done && (StrBuf_ServGetln(Buf)>=0)) {
		if ( (StrLength(Buf)==3) && 
		    !strcmp(ChrPtr(Buf), "000")) 
		{
			Done = 1;
			continue;
		}

		Entry = (FileListStruct*) malloc(sizeof (FileListStruct));
		Entry->Filename = NewStrBufPlain(NULL, StrLength(Buf));
		Entry->MimeType = NewStrBufPlain(NULL, StrLength(Buf));
		Entry->Comment = NewStrBufPlain(NULL, StrLength(Buf));

		Entry->Sequence = sequence++;

		StrBufExtract_token(Entry->Filename, Buf, 0, '|');
		Entry->FileSize = StrBufExtract_long(Buf, 1, '|');
		StrBufExtract_token(Entry->MimeType, Buf, 2, '|');
		StrBufExtract_token(Entry->Comment, Buf, 3, '|');



		Entry->IsPic = (strstr(ChrPtr(Entry->MimeType), "image") != NULL);
		if (Entry->IsPic) {
			HavePic = 1;
		}
		Put(Files, SKEY(Entry->Filename), Entry, FreeFiles);
	}
	SortIt = RetrieveSort(CTX_FILELIST, NULL, HKEY("fileunsorted"), 0);
	if (SortIt != NULL)
		SortByPayload(Files, SortIt);
	else 
		SortByPayload(Files, CompareFilelistBySequence);
	FreeStrBuf(&Buf);
	svputlong("FILE:HAVEPICS", HavePic);
	return Files;
}

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
	char buf[256];
	off_t bytes;
	char content_type[256];
	char *content = NULL;

	/* Setting to nonzero forces a MIME type of application/octet-stream */
	int force_download = 1;
	
	safestrncpy(buf, ChrPtr(WC->UrlFragment2), sizeof buf);
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
		read_server_binary(WC->WBuf, bytes);
		serv_puts("CLOS");
		serv_getln(buf, sizeof buf);
		http_transmit_thing(content_type, 0);
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
	long bytes_transmitted = 0;
	long blocksize;
	wcsession *WCC = WC;     /* stack this for faster access (WC is a function) */

	MimeType = GuessMimeType(WCC->upload, WCC->upload_length); 
	serv_printf("UOPN %s|%s|%s", WCC->upload_filename, MimeType, bstr("description"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2')
	{
		strcpy(WCC->ImportantMessage, &buf[4]);
		do_template("files", NULL);
		return;
	}

	while (bytes_transmitted < WCC->upload_length)
	{
		blocksize = 4096;
		if (blocksize > (WCC->upload_length - bytes_transmitted))
		{
			blocksize = (WCC->upload_length - bytes_transmitted);
		}
		serv_printf("WRIT %ld", blocksize);
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
	do_template("files", CTX_NONE);
}



/*
 * When the browser requests an image file from the Citadel server,
 * this function is called to transmit it.
 */
void output_image(void)
{
	char blank_gif[SIZ];
	wcsession *WCC = WC;
	char buf[SIZ];
	off_t bytes;
	const char *MimeType;
	
	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		bytes = extract_long(&buf[4], 0);

		/** Read it from the server */
		
		if (read_server_binary(WCC->WBuf, bytes) > 0) {
			serv_puts("CLOS");
			serv_getln(buf, sizeof buf);
		
			MimeType = GuessMimeType (ChrPtr(WCC->WBuf), StrLength(WCC->WBuf));
			/** Write it to the browser */
			if (!IsEmptyStr(MimeType))
			{
				http_transmit_thing(MimeType, 0);
				return;
			}
		}
		/* hm... unknown mimetype? fallback to blank gif */
	} 

	
	/*
	 * Instead of an ugly 404, send a 1x1 transparent GIF
	 * when there's no such image on the server.
	 */
	snprintf (blank_gif, SIZ, "%s%s", static_dirs[0], "/blank.gif");
	output_static(blank_gif);
}

void 
InitModule_DOWNLOAD
(void)
{

	RegisterIterator("ROOM:FILES", 0, NULL, LoadFileList,
			 NULL,NULL, CTX_FILELIST, CTX_NONE, 
			 IT_FLAG_DETECT_GROUPCHANGE);

	RegisterSortFunc(HKEY("filemime"),
			 NULL, 0,
			 CompareFilelistByMime,
			 CompareFilelistByMimeRev,
			 GroupchangeFilelistByMime,
			 CTX_FILELIST);
	RegisterSortFunc(HKEY("filename"),
			 NULL, 0,
			 CompareFilelistByName,
			 CompareFilelistByNameRev,
			 GroupchangeFilelistByName,
			 CTX_FILELIST);
	RegisterSortFunc(HKEY("filesize"),
			 NULL, 0,
			 CompareFilelistBySize,
			 CompareFilelistBySizeRev,
			 GroupchangeFilelistBySize,
			 CTX_FILELIST);
	RegisterSortFunc(HKEY("filesubject"),
			 NULL, 0,
			 CompareFilelistByComment,
			 CompareFilelistByCommentRev,
			 GroupchangeFilelistByComment,
			 CTX_FILELIST);
	RegisterSortFunc(HKEY("fileunsorted"),
			 NULL, 0,
			 CompareFilelistBySequence,
			 CompareFilelistBySequence,
			 GroupchangeFilelistBySequence,
			 CTX_FILELIST);

	RegisterNamespace("FILE:NAME", 0, 2, tmplput_FILE_NAME, CTX_FILELIST);
	RegisterNamespace("FILE:SIZE", 0, 1, tmplput_FILE_SIZE, CTX_FILELIST);
	RegisterNamespace("FILE:MIMETYPE", 0, 2, tmplput_FILEMIMETYPE, CTX_FILELIST);
	RegisterNamespace("FILE:COMMENT", 0, 2, tmplput_FILE_COMMENT, CTX_FILELIST);

	RegisterConditional(HKEY("COND:FILE:ISPIC"), 0, Conditional_FILE_ISPIC, CTX_FILELIST);

	WebcitAddUrlHandler(HKEY("image"), output_image, 0);
	WebcitAddUrlHandler(HKEY("display_mime_icon"), display_mime_icon , 0);
	WebcitAddUrlHandler(HKEY("download_file"), download_file, NEED_URL);
	WebcitAddUrlHandler(HKEY("upload_file"), upload_file, 0);
}
