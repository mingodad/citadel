/*
 * $Id: webcit.c 7459 2009-05-17 08:34:33Z dothebart $
 *
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

#include "webcit.h"
#include "webserver.h"


HashList *StaticFilemappings[4] = {NULL, NULL, NULL, NULL};


/*
  for ( a = 0; a < 9; ++a)
  {
  extract_token(index[a], ChrPtr(ReqLine), a + 1, '/', sizes[a]);
  if (strstr(index[a], "?")) *strstr(index[a], "?") = 0;
  if (strstr(index[a], "&")) *strstr(index[a], "&") = 0;
  if (strstr(index[a], " ")) *strstr(index[a], " ") = 0;
  if ((index[a][0] == '.') && (index[a][1] == '.'))
  nBackDots++;
  if (index[a][0] == '\0')
  nEmpty++;
  }
*/

/* TODO: staticdata
{


	/** Figure out the action * /
	index[0] = action;
	sizes[0] = sizeof action;
	for (a=1; a<9; a++)
	{
		index[a] = arg[a-1];
		sizes[a] = sizeof arg[a-1];
	}
	nBackDots = 0;
	nEmpty = 0;


	/* Static content can be sent without connecting to Citadel. * /
	is_static = 0;
	for (a=0; a<ndirs && ! is_static; ++a) {
		if (!strcasecmp(action, (char*)static_content_dirs[a])) { /* map web to disk location * /
			is_static = 1;
			n_static = a;
		}
	}
	if (is_static) {
		if (nBackDots < 2)
		{
			snprintf(buf, sizeof buf, "%s/%s/%s/%s/%s/%s/%s/%s",
				 static_dirs[n_static], 
				 index[1], index[2], index[3], index[4], index[5], index[6], index[7]);
			for (a=0; a<8; ++a) {
				if (buf[strlen(buf)-1] == '/') {
					buf[strlen(buf)-1] = 0;
				}
			}
			for (a = 0; a < strlen(buf); ++a) {
				if (isspace(buf[a])) {
					buf[a] = 0;
				}
			}
			output_static(buf);
		}
		else 
		{
			lprintf(9, "Suspicious request. Ignoring.");
			hprintf("HTTP/1.1 404 Security check failed\r\n");
			hprintf("Content-Type: text/plain\r\n\r\n");
			wprintf("You have sent a malformed or invalid request.\r\n");
			end_burst();
		}
		goto SKIP_ALL_THIS_CRAP;	/* Don't try to connect * /
	}
	}*/


/*
 * dump out static pages from disk
 */
void output_static(const char *what)
{
	int fd;
	struct stat statbuf;
	off_t bytes;
	off_t count = 0;
	const char *content_type;
	int len;
	const char *Err;

	fd = open(what, O_RDONLY);
	if (fd <= 0) {
		lprintf(9, "output_static('%s')  -- NOT FOUND --\n", what);
		hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
		hprintf("Content-Type: text/plain\r\n");
		wprintf("Cannot open %s: %s\r\n", what, strerror(errno));
		end_burst();
	} else {
		len = strlen (what);
		content_type = GuessMimeByFilename(what, len);

		if (fstat(fd, &statbuf) == -1) {
			lprintf(9, "output_static('%s')  -- FSTAT FAILED --\n", what);
			hprintf("HTTP/1.1 404 %s\r\n", strerror(errno));
			hprintf("Content-Type: text/plain\r\n");
			wprintf("Cannot fstat %s: %s\n", what, strerror(errno));
			end_burst();
			return;
		}

		count = 0;
		bytes = statbuf.st_size;

		if (StrBufReadBLOB(WC->WBuf, &fd, 1, bytes, &Err) < 0)
		{
			if (fd > 0) close(fd);
			lprintf(9, "output_static('%s')  -- FREAD FAILED (%s) --\n", what, strerror(errno));
				hprintf("HTTP/1.1 500 internal server error \r\n");
				hprintf("Content-Type: text/plain\r\n");
				end_burst();
				return;
		}


		close(fd);
#ifndef TECH_PREVIEW
		lprintf(9, "output_static('%s')  %s\n", what, content_type);
#endif
		http_transmit_thing(content_type, 1);
	}
	if (yesbstr("force_close_session")) {
		end_webcit_session();
	}
}

	/* TODO: integrate this into the static startup logic

	 * While we're at it, gracefully handle requests for the
	 * robots.txt and favicon.ico files.
	 * /
	if ((StrLength(ReqLine) >= 11) &&
	    !strncasecmp(ChrPtr(ReqLine), "/robots.txt", 11)) {
		StrBufPlain(ReqLine, 
			    HKEY("/static/robots.txt"
				 "?force_close_session=yes HTTP/1.1"));
		Hdr.eReqType = eGET;
	}
	else if ((StrLength(ReqLine) >= 11) &&
		 !strncasecmp(ChrPtr(ReqLine), "/favicon.ico", 12)) {
		StrBufPlain(ReqLine, HKEY("/static/favicon.ico"));
		Hdr.eReqType = eGET;
	}

*/




int LoadStaticDir(const char *DirName, HashList *DirList, const char *RelDir)
{
	char dirname[PATH_MAX];
	char reldir[PATH_MAX];
	StrBuf *FileName;
	StrBuf *Tag;
	StrBuf *Dir;
	StrBuf *WebDir;
	StrBuf *OneWebName;
	DIR *filedir = NULL;
	struct dirent d;
	struct dirent *filedir_entry;
	int d_namelen;
	int d_without_ext;
		
	Dir = NewStrBufPlain(DirName, -1);
	WebDir = NewStrBufPlain(RelDir, -1);
	OneWebName = NewStrBuf();
	filedir = opendir (DirName);
	if (filedir == NULL) {
		FreeStrBuf(&Dir);
		return 0;
	}

	FileName = NewStrBuf();
	Tag = NewStrBuf();
	while ((readdir_r(filedir, &d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
		char *PStart;
#ifdef _DIRENT_HAVE_D_NAMELEN
		d_namelen = filedir_entry->d_namelen;
#else
		d_namelen = strlen(filedir_entry->d_name);
#endif
		d_without_ext = d_namelen;

		if ((d_namelen > 1) && filedir_entry->d_name[d_namelen - 1] == '~')
			continue; /* Ignore backup files... */

		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		switch (filedir_entry->d_type)
		{
		case DT_DIR:
			/* Skip directories we are not interested in... */
			if ((strcmp(filedir_entry->d_name, ".svn") == 0) ||
			    (strcmp(filedir_entry->d_name, "t") == 0))
				break;
			snprintf(dirname, PATH_MAX, "%s/%s", 
				 DirName, filedir_entry->d_name);
			snprintf(reldir, PATH_MAX, "%s/%s", 
				 RelDir, filedir_entry->d_name);
			LoadStaticDir(dirname, DirList, reldir); 				 
			break;
		case DT_LNK: /* TODO: check whether its a file or a directory */
		case DT_REG:
			PStart = filedir_entry->d_name;
			FileName = NewStrBufDup(Dir);
			if (ChrPtr(FileName) [ StrLength(FileName) - 1] != '/')
				StrBufAppendBufPlain(FileName, "/", 1, 0);
			StrBufAppendBufPlain(FileName, filedir_entry->d_name, d_namelen, 0);

			FlushStrBuf(OneWebName);
			StrBufAppendBuf(OneWebName, WebDir, 0);
			if ((StrLength(OneWebName) != 0) && 
			    (ChrPtr(OneWebName) [ StrLength(OneWebName)] != '/'))
				StrBufAppendBufPlain(OneWebName, "/", 1, 0);
			StrBufAppendBufPlain(OneWebName, filedir_entry->d_name, d_namelen, 0);

			Put(DirList, SKEY(OneWebName), FileName, HFreeStrBuf);
			printf("[%s | %s]  \n", ChrPtr(OneWebName), ChrPtr(FileName));
			break;
		default:
			break;
		}


	}
	closedir(filedir);
	FreeStrBuf(&FileName);
	FreeStrBuf(&Tag);
	FreeStrBuf(&Dir);
	FreeStrBuf(&OneWebName);
	return 1;
}


void output_static_safe(HashList *DirList)
{
	wcsession *WCC = WC;
	void *vFile;
	StrBuf *File;

	if (GetHash(DirList, SKEY(WCC->Hdr->ReqLine), &vFile) &&
	    (vFile != NULL))
	{
		File = (StrBuf*) vFile;
		output_static(ChrPtr(vFile));
	}
	else {
///TODO: detect image & output blank image
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

	WebcitAddUrlHandler(HKEY("robots.txt"), output_static_0, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("favicon.ico"), output_static_0, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("static"), output_static_0, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("static.local"), output_static_1, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);
	WebcitAddUrlHandler(HKEY("tinymce"), output_static_2, ANONYMOUS|COOKIEUNNEEDED|ISSTATIC);


}
