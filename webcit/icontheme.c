/*
 * Displays and customizes the iconbar.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

HashList *AvailableThemes = NULL;

const StrBuf *DefaultTheme = NULL;
void LoadIconthemeSettings(StrBuf *icontheme, long lvalue)
{
	wcsession *WCC = WC;
	void *vTheme;
	const StrBuf *theme;

	if (GetHash(AvailableThemes, SKEY(icontheme), &vTheme))
		theme = (StrBuf*)vTheme;
	else
		theme = DefaultTheme;

	if (WCC->IconTheme != NULL) 
		StrBufPlain(WCC->IconTheme, SKEY(theme));
	else
		WCC->IconTheme = NewStrBufDup(theme);
}


void tmplput_icontheme(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	if ( (WCC != NULL)     &&
	     (WCC->IconTheme != NULL))
	{
		 StrBufAppendTemplate(Target, TP, WCC->IconTheme, 0);
	}
	else
	{
		 StrBufAppendTemplate(Target, TP, DefaultTheme, 0);
	}
}


int LoadThemeDir(const char *DirName)
{
	StrBuf *Dir = NULL;
	DIR *filedir = NULL;
	struct dirent *d;
	struct dirent *filedir_entry;
	int d_type = 0;
        int d_namelen;
		
	filedir = opendir (DirName);
	if (filedir == NULL) {
		return 0;
	}

	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 1);
	if (d == NULL) {
		return 0;
	}

	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMELEN
		d_namelen = filedir_entry->d_namelen;
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
		d_namelen = strlen(filedir_entry->d_name);
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
			if (stat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}

		switch (d_type)
		{
		case DT_LNK: /* TODO: check whether its a file or a directory */
		case DT_DIR:
			/* Skip directories we are not interested in... */
			if ((strcmp(filedir_entry->d_name, ".svn") == 0) ||
			    (strcmp(filedir_entry->d_name, "t") == 0))
				break;
			
			Dir = NewStrBufPlain (filedir_entry->d_name, d_namelen);
			if (DefaultTheme == NULL)
				DefaultTheme = Dir;
			Put(AvailableThemes, SKEY(Dir), Dir, HFreeStrBuf);
			break;
		case DT_REG:
		default:
			break;
		}


	}
	free(d);
	closedir(filedir);

	return 1;
}

HashList *GetValidThemeHash(StrBuf *Target, WCTemplputParams *TP)
{
	return AvailableThemes;
}
void 
ServerStartModule_ICONTHEME
(void)
{
	AvailableThemes = NewHash(1, NULL);
}
void 
InitModule_ICONTHEME
(void)
{
	StrBuf *Themes = NewStrBufPlain(static_dirs[0], -1);

	StrBufAppendBufPlain(Themes, HKEY("/"), 0);
	StrBufAppendBufPlain(Themes, HKEY("webcit_icons"), 0);
	LoadThemeDir(ChrPtr(Themes));
	FreeStrBuf(&Themes);

	RegisterPreference("icontheme", _("Icon Theme"), PRF_STRING, LoadIconthemeSettings);
	RegisterNamespace("ICONTHEME", 0, 0, tmplput_icontheme, NULL, CTX_NONE);

	RegisterIterator("PREF:VALID:THEME", 0, NULL, 
			 GetValidThemeHash, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
}

void 
ServerShutdownModule_ICONTHEME
(void)
{
	DeleteHash(&AvailableThemes);
}

void 
SessionDestroyModule_ICONTHEME
(wcsession *sess)
{
	FreeStrBuf(&sess->IconTheme);
}

