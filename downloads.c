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
void tmplput_FILE_NAME(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *F = (FileListStruct*) CTX;
	StrBufAppendTemplate(Target, TP, F->Filename, 0);
}
void tmplput_FILE_SIZE(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *F = (FileListStruct*) CTX;
	StrBufAppendPrintf(Target, "%ld", F->FileSize);
}
void tmplput_FILEMIMETYPE(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *F = (FileListStruct*) CTX;
	StrBufAppendTemplate(Target, TP, F->MimeType, 0);
}
void tmplput_FILE_COMMENT(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *F = (FileListStruct*) CTX;
	StrBufAppendTemplate(Target, TP, F->Comment, 0);
}

/* -------------------------------------------------------------------------------- */

int Conditional_FILE_ISPIC(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *F = (FileListStruct*) CTX;
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
HashList* LoadFileList(StrBuf *Target, WCTemplputParams *TP)
{
	FileListStruct *Entry;
	StrBuf *Buf;
	HashList *Files;
	int Done = 0;
	int sequence = 0;
	char buf[1024];
	CompareFunc SortIt;
	int HavePic;
	WCTemplputParams SubTP;

	memset(&TP, 0, sizeof(WCTemplputParams));
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
	SubTP.Filter.ContextType = CTX_FILELIST;
	SortIt = RetrieveSort(&SubTP, NULL, 0, HKEY("fileunsorted"), 0);
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
	wcsession *WCC = WC;
	StrBuf *Buf;
	off_t bytes;
	StrBuf *ContentType = NewStrBufPlain(HKEY("application/octet-stream"));

	/* Setting to nonzero forces a MIME type of application/octet-stream */
	int force_download = 1;
	
	Buf = NewStrBuf();
	StrBufExtract_token(Buf, WCC->Hdr->HR.ReqLine, 0, '/');
	StrBufUnescape(Buf, 1);
	serv_printf("OPEN %s", ChrPtr(Buf));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufCutLeft(Buf, 4);
		bytes = StrBufExtract_long(Buf, 0, '|');
		if (!force_download) {
			StrBufExtract_token(ContentType, Buf, 3, '|');
		}
		read_server_binary(WCC->WBuf, bytes, Buf);
		serv_puts("CLOS");
		StrBuf_ServGetln(Buf);
		http_transmit_thing(ChrPtr(ContentType), 0);
	} else {
		StrBufCutLeft(Buf, 4);
		hprintf("HTTP/1.1 404 %s\n", ChrPtr(Buf));
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf(_("An error occurred while retrieving this file: %s\n"), 
			ChrPtr(Buf));
		end_burst();
	}
	FreeStrBuf(&ContentType);
	FreeStrBuf(&Buf);
}



void delete_file(void)
{
	const StrBuf *MimeType;
	StrBuf *Buf;
	char buf[256];
	
	safestrncpy(buf, bstr("file"), sizeof buf);
	unescape_input(buf);
	serv_printf("DELF %s", buf);
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, NULL);
	StrBufCutLeft(Buf, 4);
	strcpy(WC->ImportantMessage, ChrPtr(Buf));
	MimeType = DoTemplate(HKEY("files"), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(MimeType), 0);
	FreeStrBuf(&Buf);
}



void upload_file(void)
{
	const StrBuf *RetMimeType;
	const char *MimeType;
	char buf[1024];
	long bytes_transmitted = 0;
	long blocksize;
	wcsession *WCC = WC;     /* stack this for faster access (WC is a function) */

	MimeType = GuessMimeType(ChrPtr(WCC->upload), WCC->upload_length); 
	serv_printf("UOPN %s|%s|%s", WCC->upload_filename, MimeType, bstr("description"));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2')
	{
		strcpy(WCC->ImportantMessage, &buf[4]);
		RetMimeType = DoTemplate(HKEY("files"), NULL, &NoCtx);
		http_transmit_thing(ChrPtr(RetMimeType), 0);
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
			serv_write(&ChrPtr(WCC->upload)[bytes_transmitted], blocksize);
			bytes_transmitted += blocksize;
		}
	}

	serv_puts("UCLS 1");
	serv_getln(buf, sizeof buf);
	strcpy(WCC->ImportantMessage, &buf[4]);
	RetMimeType = DoTemplate(HKEY("files"), NULL, &NoCtx);
	http_transmit_thing(ChrPtr(RetMimeType), 0);
}



/*
 * When the browser requests an image file from the Citadel server,
 * this function is called to transmit it.
 */
void output_image(void)
{
	StrBuf *Buf;
	wcsession *WCC = WC;
	off_t bytes;
	const char *MimeType;
	
	Buf = NewStrBuf();
	serv_printf("OIMG %s|%s", bstr("name"), bstr("parm"));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufCutLeft(Buf, 4);
		bytes = StrBufExtract_long(Buf, 0, '|');

		/** Read it from the server */
		
		if (read_server_binary(WCC->WBuf, bytes, Buf) > 0) {
			serv_puts("CLOS");
			StrBuf_ServGetln(Buf);
		
			MimeType = GuessMimeType (ChrPtr(WCC->WBuf), StrLength(WCC->WBuf));
			/** Write it to the browser */
			if (!IsEmptyStr(MimeType))
			{
				http_transmit_thing(MimeType, 0);
				FreeStrBuf(&Buf);
				return;
			}
		}
		/* hm... unknown mimetype? fallback to blank gif */
	} 

	
	/*
	 * Instead of an ugly 404, send a 1x1 transparent GIF
	 * when there's no such image on the server.
	 */
	StrBufPrintf (Buf, "%s%s", static_dirs[0], "/blank.gif");
	output_static(ChrPtr(Buf));
	FreeStrBuf(&Buf);
}

void 
InitModule_DOWNLOAD
(void)
{

	RegisterIterator("ROOM:FILES", 0, NULL, LoadFileList,
			 NULL, DeleteHash, CTX_FILELIST, CTX_NONE, 
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

	RegisterNamespace("FILE:NAME", 0, 2, tmplput_FILE_NAME, NULL, CTX_FILELIST);
	RegisterNamespace("FILE:SIZE", 0, 1, tmplput_FILE_SIZE, NULL, CTX_FILELIST);
	RegisterNamespace("FILE:MIMETYPE", 0, 2, tmplput_FILEMIMETYPE, NULL, CTX_FILELIST);
	RegisterNamespace("FILE:COMMENT", 0, 2, tmplput_FILE_COMMENT, NULL, CTX_FILELIST);

	RegisterConditional(HKEY("COND:FILE:ISPIC"), 0, Conditional_FILE_ISPIC, CTX_FILELIST);

	WebcitAddUrlHandler(HKEY("image"), output_image, ANONYMOUS);
	WebcitAddUrlHandler(HKEY("display_mime_icon"), display_mime_icon , ANONYMOUS);
	WebcitAddUrlHandler(HKEY("download_file"), download_file, NEED_URL);
	WebcitAddUrlHandler(HKEY("delete_file"), delete_file, NEED_URL);
	WebcitAddUrlHandler(HKEY("upload_file"), upload_file, 0);
}
