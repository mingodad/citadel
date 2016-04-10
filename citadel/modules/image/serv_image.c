/*
 * Copyright (c) 1987-2016 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ctdl_module.h"
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


/*
 * Import function called by import_old_userpic_files() for a single user
 */
void import_one_userpic_file(char *username, long usernum, char *path)
{
	syslog(LOG_DEBUG, "Import legacy userpic for %s, usernum=%ld, filename=%s", username, usernum, path);

	FILE *fp = fopen(path, "r");
	if (!fp) return;

	fseek(fp, 0, SEEK_END);
	long data_length = ftell(fp);

	if (data_length >= 1) {
		rewind(fp);
		char *unencoded_data = malloc(data_length);
		if (unencoded_data) {
			fread(unencoded_data, data_length, 1, fp);
			char *encoded_data = malloc((data_length * 2) + 100);
			if (encoded_data) {
				sprintf(encoded_data, "Content-type: image/gif\nContent-transfer-encoding: base64\n\n");
				CtdlEncodeBase64(&encoded_data[strlen(encoded_data)], unencoded_data, data_length, 1);

				char userconfigroomname[ROOMNAMELEN];
				struct ctdluser usbuf;

				if (CtdlGetUser(&usbuf, username) == 0) {	// no need to lock it , we are still initializing
					long old_msgnum = usbuf.msgnum_pic;
					CtdlMailboxName(userconfigroomname, sizeof userconfigroomname, &usbuf, USERCONFIGROOM);
					long new_msgnum = quickie_message("Citadel", NULL, NULL, userconfigroomname, encoded_data, FMT_RFC822, "Photo imported from file");
					syslog(LOG_DEBUG, "Message %ld is now the profile for %s", new_msgnum, username);
					usbuf.msgnum_pic = new_msgnum;
					CtdlPutUser(&usbuf);
					unlink(path);				// delete the old file , it's in the database now
					if (old_msgnum > 0) {
						syslog(LOG_DEBUG, "Deleting old message %ld from %s", old_msgnum, userconfigroomname);
						CtdlDeleteMessages(userconfigroomname, &old_msgnum, 1, "");
					}
				}
				free(encoded_data);
			}
			free(unencoded_data);
		}
	}
	fclose(fp);
}


/*
 * Look for old-format "userpic" files and import them into the message base
 */
void import_old_userpic_files(void)
{
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	struct dirent *d;
	size_t d_namelen;
	struct ctdluser usbuf;
	long usernum = 0;
	int d_type = 0;
	struct stat s;
	char path[PATH_MAX];


	syslog(LOG_DEBUG, "Importing old style userpic files into the message base");
	d = (struct dirent *)malloc(offsetof(struct dirent, d_name) + PATH_MAX + 2);
	if (d == NULL) {
		return;
	}

	filedir = opendir (ctdl_usrpic_dir);
	if (filedir == NULL) {
		free(d);
		return;
	}
	while ((readdir_r(filedir, d, &filedir_entry) == 0) &&
	       (filedir_entry != NULL))
	{
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namlen;

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
		if ((d_namelen == 1) && 
		    (filedir_entry->d_name[0] == '.'))
			continue;

		if ((d_namelen == 2) && 
		    (filedir_entry->d_name[0] == '.') &&
		    (filedir_entry->d_name[1] == '.'))
			continue;

		snprintf(path, PATH_MAX, "%s/%s", ctdl_usrpic_dir, filedir_entry->d_name);
		if (d_type == DT_UNKNOWN) {
			if (lstat(path, &s) == 0) {
				d_type = IFTODT(s.st_mode);
			}
		}
		switch (d_type)
		{
		case DT_DIR:
			break;
		case DT_LNK:
		case DT_REG:
			usernum = atol(filedir_entry->d_name);
			if (CtdlGetUserByNumber(&usbuf, usernum) == 0) {
				import_one_userpic_file(usbuf.fullname, usernum, path);
			}
		}
	}
	free(d);
	closedir(filedir);
	rmdir(ctdl_usrpic_dir);
}



CTDL_MODULE_INIT(image)
{
	if (!threading)
	{
		import_old_userpic_files();
	}
	/* return our module name for the log */
        return "image";
}
