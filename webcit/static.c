/*
 * This is the main transaction loop of the web service.  It maintains a
 * persistent session to the Citadel server, handling HTTP WebCit requests as
 * they arrive and presenting a user interface.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>


#include "webcit.h"
#include "webserver.h"

unsigned char OnePixelGif[37] = {
		0x47,
		0x49,
		0x46,
		0x38,
		0x37,
		0x61,
		0x01,
		0x00,
		0x01,
		0x00,
		0x80,
		0x00,
		0x00,
		0xff,
		0xff,
		0xff,
		0xff,
		0xff,
		0xff,
		0x2c,
		0x00,
		0x00,
		0x00,
		0x00,
		0x01,
		0x00,
		0x01,
		0x00,
		0x00,
		0x02,
		0x02,
		0x44,
		0x01,
		0x00,
		0x3b 
};


HashList *StaticFilemappings[4] = {NULL, NULL, NULL, NULL};
/*
  {
  syslog(9, "Suspicious request. Ignoring.");
  hprintf("HTTP/1.1 404 Security check failed\r\n");
  hprintf("Content-Type: text/plain\r\n\r\n");
  wc_printf("You have sent a malformed or invalid request.\r\n");
  end_burst();
  }
*/


void output_error_pic(const char *ErrMsg1, const char *ErrMsg2)
{
	hprintf("HTTP/1.1 200 %s\r\n", ErrMsg1);
	hprintf("Content-Type: image/gif\r\n");
	hprintf("x-webcit-errormessage: %s\r\n", ErrMsg2);
	begin_burst();
	StrBufPlain(WC->WBuf, (const char *)OnePixelGif, sizeof(OnePixelGif));
	end_burst();
}

/*
 * dump out static pages from disk
 */
void output_static(const char *what)
{
	int fd;
	struct stat statbuf;
	off_t bytes;
	const char *content_type;
	int len;
	const char *Err;

	len = strlen (what);
	content_type = GuessMimeByFilename(what, len);
	fd = open(what, O_RDONLY);
	if (fd <= 0) {
		syslog(9, "output_static('%s') [%s]  -- NOT FOUND --\n", what, ChrPtr(WC->Hdr->this_page));
		if (strstr(content_type, "image/") != NULL)
		{
			output_error_pic("the file you requsted is gone.", strerror(errno));
		}
		else
		{
			hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
			hprintf("Content-Type: text/plain\r\n");
			begin_burst();
			wc_printf("Cannot open %s: %s\r\n", what, strerror(errno));
			end_burst();
		}
	} else {
		if (fstat(fd, &statbuf) == -1) {
			syslog(9, "output_static('%s')  -- FSTAT FAILED --\n", what);
			if (strstr(content_type, "image/") != NULL)
			{
				output_error_pic("Stat failed!", strerror(errno));
			}
			else
			{
				hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
				hprintf("Content-Type: text/plain\r\n");
				begin_burst();
				wc_printf("Cannot fstat %s: %s\n", what, strerror(errno));
				end_burst();
			}
			if (fd > 0) close(fd);
			return;
		}

		bytes = statbuf.st_size;

		if (StrBufReadBLOB(WC->WBuf, &fd, 1, bytes, &Err) < 0)
		{
			if (fd > 0) close(fd);
			syslog(9, "output_static('%s')  -- FREAD FAILED (%s) --\n", what, strerror(errno));
				hprintf("HTTP/1.1 500 internal server error \r\n");
				hprintf("Content-Type: text/plain\r\n");
				end_burst();
				return;
		}


		close(fd);
		http_transmit_thing(content_type, 2);
	}
	if (yesbstr("force_close_session")) {
		end_webcit_session();
	}
}


int LoadStaticDir(const char *DirName, HashList *DirList, const char *RelDir)
{
	char dirname[PATH_MAX];
	char reldir[PATH_MAX];
	StrBuf *FileName = NULL;
	StrBuf *Dir = NULL;
	StrBuf *WebDir = NULL;
	StrBuf *OneWebName = NULL;
	DIR *filedir = NULL;
	struct dirent *d;
	struct dirent *filedir_entry;
	int d_type = 0;
        int d_namelen;
	int istoplevel;
		
	filedir = opendir (DirName);
	if (filedir == NULL) {
		return 0;
	}

	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		closedir(filedir);
		return 0;
	}

	Dir = NewStrBufPlain(DirName, -1);
	WebDir = NewStrBufPlain(RelDir, -1);
	istoplevel = IsEmptyStr(RelDir);
	OneWebName = NewStrBuf();

	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namelen;

#else
		d_namelen = strlen(filedir_entry->d_name);
#endif

#ifdef _DIRENT_HAVE_D_TYPE
		d_type = filedir_entry->d_type;
#else

#ifndef DT_UNKNOWN
#define DT_UNKNOWN     0
#define DT_DIR         4
#define DT_REG         8
#define DT_LNK         10

#define IFTODT(mode)   (((mode) & 0170000) >> 12)
#define DTTOIF(dirtype)        ((dirtype) << 12)
#endif
		d_type = DT_UNKNOWN;
#endif
		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		if (d_type == DT_UNKNOWN) {
			struct stat s;
			char path[PATH_MAX];
			snprintf(path, PATH_MAX, "%s/%s", 
				DirName, filedir_entry->d_name);
			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}

		switch (d_type)
		{
		case DT_DIR:
			/* Skip directories we are not interested in... */
			if ((strcmp(filedir_entry->d_name, ".svn") == 0) ||
			    (strcmp(filedir_entry->d_name, "t") == 0))
				break;
			snprintf(dirname, PATH_MAX, "%s/%s/", 
				 DirName, filedir_entry->d_name);
			if (istoplevel)
				snprintf(reldir, PATH_MAX, "%s/", 
					 filedir_entry->d_name);
			else
				snprintf(reldir, PATH_MAX, "%s/%s/", 
					 RelDir, filedir_entry->d_name);
			StripSlashes(dirname, 1);
			StripSlashes(reldir, 1);
			LoadStaticDir(dirname, DirList, reldir); 				 
			break;
		case DT_LNK: /* TODO: check whether its a file or a directory */
		case DT_REG:
			FileName = NewStrBufDup(Dir);
			if (ChrPtr(FileName) [ StrLength(FileName) - 1] != '/')
				StrBufAppendBufPlain(FileName, "/", 1, 0);
			StrBufAppendBufPlain(FileName, filedir_entry->d_name, d_namelen, 0);

			FlushStrBuf(OneWebName);
			StrBufAppendBuf(OneWebName, WebDir, 0);
			if ((StrLength(OneWebName) != 0) && 
			    (ChrPtr(OneWebName) [ StrLength(OneWebName) - 1] != '/'))
				StrBufAppendBufPlain(OneWebName, "/", 1, 0);
			StrBufAppendBufPlain(OneWebName, filedir_entry->d_name, d_namelen, 0);

			Put(DirList, SKEY(OneWebName), FileName, HFreeStrBuf);
			/* syslog(9, "[%s | %s]\n", ChrPtr(OneWebName), ChrPtr(FileName)); */
			break;
		default:
			break;
		}


	}
	free(d);
	closedir(filedir);
	FreeStrBuf(&Dir);
	FreeStrBuf(&WebDir);
	FreeStrBuf(&OneWebName);
	return 1;
}


void output_flat_static(void)
{
	wcsession *WCC = WC;
	void *vFile;
	StrBuf *File;

	if (WCC->Hdr->HR.Handler == NULL)
		return;
	if (GetHash(StaticFilemappings[0], SKEY(WCC->Hdr->HR.Handler->Name), &vFile) &&
	    (vFile != NULL))
	{
		File = (StrBuf*) vFile;
		output_static(ChrPtr(File));
	}
}

void output_static_safe(HashList *DirList)
{
	wcsession *WCC = WC;
	void *vFile;
	StrBuf *File;
	const char *MimeType;

	if (GetHash(DirList, SKEY(WCC->Hdr->HR.ReqLine), &vFile) &&
	    (vFile != NULL))
	{
		File = (StrBuf*) vFile;
		output_static(ChrPtr(File));
	}
	else {
		syslog(1, "output_static_safe() file %s not found. \n", 
			ChrPtr(WCC->Hdr->HR.ReqLine));
		MimeType =  GuessMimeByFilename(SKEY(WCC->Hdr->HR.ReqLine));
		if (strstr(MimeType, "image/") != NULL)
		{
			output_error_pic("the file you requested isn't known to our cache", "maybe reload webcit?");
		}
		else
		{		    
			do_404();
		}
	}
}
void output_static_0(void)
{
	output_static_safe(StaticFilemappings[0]);
}
void output_static_1(void)
{
	output_static_safe(StaticFilemappings[1]);
}
void output_static_2(void)
{
	output_static_safe(StaticFilemappings[2]);
}
void output_static_3(void)
{
	output_static_safe(StaticFilemappings[3]);
}


/*
 * robots.txt
 */
void robots_txt(void) {
	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/plain\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n",
		PACKAGE_STRING);
	begin_burst();

	wc_printf("User-agent: *\r\n"
		"Disallow: /printmsg\r\n"
		"Disallow: /msgheaders\r\n"
		"Disallow: /groupdav\r\n"
		"Disallow: /do_template\r\n"
		"Disallow: /static\r\n"
		"Disallow: /display_page\r\n"
		"Disallow: /readnew\r\n"
		"Disallow: /display_enter\r\n"
		"Disallow: /skip\r\n"
		"Disallow: /ungoto\r\n"
		"Sitemap: %s/sitemap.xml\r\n"
		"\r\n"
		,
		ChrPtr(site_prefix)
	);

	wDumpContent(0);
}


void 
ServerStartModule_STATIC
(void)
{
	StaticFilemappings[0] = NewHash(1, NULL);
	StaticFilemappings[1] = NewHash(1, NULL);
	StaticFilemappings[2] = NewHash(1, NULL);
	StaticFilemappings[3] = NewHash(1, NULL);
}
void 
ServerShutdownModule_STATIC
(void)
{
	DeleteHash(&StaticFilemappings[0]);
	DeleteHash(&StaticFilemappings[1]);
	DeleteHash(&StaticFilemappings[2]);
	DeleteHash(&StaticFilemappings[3]);
}


void 
InitModule_STATIC
(void)
{
	LoadStaticDir(static_dirs[0], StaticFilemappings[0], "");
	LoadStaticDir(static_dirs[1], StaticFilemappings[1], "");
	LoadStaticDir(static_dirs[2], StaticFilemappings[2], "");
	LoadStaticDir(static_dirs[3], StaticFilemappings[3], "");

	WebcitAddUrlHandler(HKEY("robots.txt"), "", 0, robots_txt, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("favicon.ico"), "", 0, output_flat_static, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("static"), "", 0, output_static_0, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("static.local"), "", 0, output_static_1, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("tinymce"), "", 0, output_static_2, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
	WebcitAddUrlHandler(HKEY("tiny_mce"), "", 0, output_static_2, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC|LOGCHATTY);
}
