/* 
 * Server functions which handle file transfers and room directories.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "config.h"
#include "file_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "msgbase.h"
#include "citserver.h"
#include "threads.h"
#include "ctdl_module.h"
#include "user_ops.h"



/*
 * Server command to delete a file from a room's directory
 */
void cmd_delf(char *filename)
{
	char pathname[64];
	int a;

	if (CtdlAccessCheck(ac_room_aide))
		return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}
	for (a = 0; !IsEmptyStr(&filename[a]); ++a) {
		if ( (filename[a] == '/') || (filename[a] == '\\') ) {
			filename[a] = '_';
		}
	}
	snprintf(pathname, sizeof pathname,
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, filename);
	a = unlink(pathname);
	if (a == 0) {
		cprintf("%d File '%s' deleted.\n", CIT_OK, pathname);
	}
	else {
		cprintf("%d File '%s' not found.\n",
			ERROR + FILE_NOT_FOUND, pathname);
	}
}




/*
 * move a file from one room directory to another
 */
void cmd_movf(char *cmdbuf)
{
	char filename[PATH_MAX];
	char pathname[PATH_MAX];
	char newpath[PATH_MAX];
	char newroom[ROOMNAMELEN];
	char buf[PATH_MAX];
	int a;
	struct ctdlroom qrbuf;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);
	extract_token(newroom, cmdbuf, 1, '|', sizeof newroom);

	if (CtdlAccessCheck(ac_room_aide)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	for (a = 0; !IsEmptyStr(&filename[a]); ++a) {
		if ( (filename[a] == '/') || (filename[a] == '\\') ) {
			filename[a] = '_';
		}
	}
	snprintf(pathname, sizeof pathname, "./files/%s/%s",
		 CC->room.QRdirname, filename);
	if (access(pathname, 0) != 0) {
		cprintf("%d File '%s' not found.\n",
			ERROR + FILE_NOT_FOUND, pathname);
		return;
	}

	if (CtdlGetRoom(&qrbuf, newroom) != 0) {
		cprintf("%d '%s' does not exist.\n", ERROR + ROOM_NOT_FOUND, newroom);
		return;
	}
	if ((qrbuf.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d '%s' is not a directory room.\n",
			ERROR + NOT_HERE, qrbuf.QRname);
		return;
	}
	snprintf(newpath, sizeof newpath, "./files/%s/%s", qrbuf.QRdirname,
		 filename);
	if (link(pathname, newpath) != 0) {
		cprintf("%d Couldn't move file: %s\n", ERROR + INTERNAL_ERROR,
			strerror(errno));
		return;
	}
	unlink(pathname);

	/* this is a crude method of copying the file description */
	snprintf(buf, sizeof buf,
		 "cat ./files/%s/filedir |grep \"%s\" >>./files/%s/filedir",
		 CC->room.QRdirname, filename, qrbuf.QRdirname);
	system(buf);
	cprintf("%d File '%s' has been moved.\n", CIT_OK, filename);
}


/*
 * This code is common to all commands which open a file for downloading,
 * regardless of whether it's a file from the directory, an image, a network
 * spool file, a MIME attachment, etc.
 * It examines the file and displays the OK result code and some information
 * about the file.  NOTE: this stuff is Unix dependent.
 */
void OpenCmdResult(char *filename, const char *mime_type)
{
	struct stat statbuf;
	time_t modtime;
	long filesize;

	fstat(fileno(CC->download_fp), &statbuf);
	CC->download_fp_total = statbuf.st_size;
	filesize = (long) statbuf.st_size;
	modtime = (time_t) statbuf.st_mtime;

	cprintf("%d %ld|%ld|%s|%s\n",
		CIT_OK, filesize, (long)modtime, filename, mime_type);
}


/*
 * open a file for downloading
 */
void cmd_open(char *cmdbuf)
{
	char filename[256];
	char pathname[PATH_MAX];
	int a;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	for (a = 0; !IsEmptyStr(&filename[a]); ++a) {
		if ( (filename[a] == '/') || (filename[a] == '\\') ) {
			filename[a] = '_';
		}
	}

	snprintf(pathname, sizeof pathname,
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, filename);
	CC->download_fp = fopen(pathname, "r");

	if (CC->download_fp == NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, pathname, strerror(errno));
		return;
	}

	OpenCmdResult(filename, "application/octet-stream");
}

/*
 * open an image file
 */
void cmd_oimg(char *cmdbuf)
{
	char filename[256];
	char pathname[PATH_MAX];
	char MimeTestBuf[32];
	struct ctdluser usbuf;
	char which_user[USERNAME_SIZE];
	int which_floor;
	int a;
	int rv;

	extract_token(filename, cmdbuf, 0, '|', sizeof filename);

	if (IsEmptyStr(filename)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	if (!strcasecmp(filename, "_userpic_")) {
		extract_token(which_user, cmdbuf, 1, '|', sizeof which_user);
		if (CtdlGetUser(&usbuf, which_user) != 0) {
			cprintf("%d No such user.\n",
				ERROR + NO_SUCH_USER);
			return;
		}
		snprintf(pathname, sizeof pathname, 
				 "%s/%ld",
				 ctdl_usrpic_dir,
				 usbuf.usernum);
	} else if (!strcasecmp(filename, "_floorpic_")) {
		which_floor = extract_int(cmdbuf, 1);
		snprintf(pathname, sizeof pathname,
				 "%s/floor.%d",
				 ctdl_image_dir, which_floor);
	} else if (!strcasecmp(filename, "_roompic_")) {
		assoc_file_name(pathname, sizeof pathname, &CC->room, ctdl_image_dir);
	} else {
		for (a = 0; !IsEmptyStr(&filename[a]); ++a) {
			filename[a] = tolower(filename[a]);
			if ( (filename[a] == '/') || (filename[a] == '\\') ) {
				filename[a] = '_';
			}
		}
		snprintf(pathname, sizeof pathname,
				 "%s/%s",
				 ctdl_image_dir,
				 filename);
	}

	CC->download_fp = fopen(pathname, "rb");
	if (CC->download_fp == NULL) {
		strcat(pathname, ".gif");
		CC->download_fp = fopen(pathname, "rb");
	}
	if (CC->download_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + FILE_NOT_FOUND, pathname, strerror(errno));
		return;
	}
	rv = fread(&MimeTestBuf[0], 1, 32, CC->download_fp);
	if (rv == -1) {
		cprintf("%d Cannot access %s: %s\n",
			ERROR + FILE_NOT_FOUND, pathname, strerror(errno));
		return;
	}

	rewind (CC->download_fp);
	OpenCmdResult(pathname, GuessMimeType(&MimeTestBuf[0], 32));
}

/*
 * open a file for uploading
 */
void cmd_uopn(char *cmdbuf)
{
	int a;

	extract_token(CC->upl_file, cmdbuf, 0, '|', sizeof CC->upl_file);
	extract_token(CC->upl_mimetype, cmdbuf, 1, '|', sizeof CC->upl_mimetype);
	extract_token(CC->upl_comment, cmdbuf, 2, '|', sizeof CC->upl_comment);

	if (CtdlAccessCheck(ac_logged_in)) return;

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d No directory in this room.\n",
			ERROR + NOT_HERE);
		return;
	}

	if (IsEmptyStr(CC->upl_file)) {
		cprintf("%d You must specify a file name.\n",
			ERROR + FILE_NOT_FOUND);
		return;
	}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have a upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	for (a = 0; !IsEmptyStr(&CC->upl_file[a]); ++a) {
		if ( (CC->upl_file[a] == '/') || (CC->upl_file[a] == '\\') ) {
			CC->upl_file[a] = '_';
		}
	}
	snprintf(CC->upl_path, sizeof CC->upl_path, 
			 "%s/%s/%s",
			 ctdl_file_dir,
			 CC->room.QRdirname, CC->upl_file);
	snprintf(CC->upl_filedir, sizeof CC->upl_filedir,
			 "%s/%s/filedir", 
			 ctdl_file_dir,
			 CC->room.QRdirname);

	CC->upload_fp = fopen(CC->upl_path, "r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR + ALREADY_EXISTS, CC->upl_path);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}
	cprintf("%d Ok\n", CIT_OK);
}



/*
 * open an image file for uploading
 */
void cmd_uimg(char *cmdbuf)
{
	int is_this_for_real;
	char basenm[256];
	int which_floor;
	int a;

	if (num_parms(cmdbuf) < 2) {
		cprintf("%d Usage error.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	is_this_for_real = extract_int(cmdbuf, 0);
	extract_token(CC->upl_mimetype, cmdbuf, 1, '|', sizeof CC->upl_mimetype);
	extract_token(basenm, cmdbuf, 2, '|', sizeof basenm);
	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	strcpy(CC->upl_path, "");

	for (a = 0; !IsEmptyStr(&basenm[a]); ++a) {
		basenm[a] = tolower(basenm[a]);
		if ( (basenm[a] == '/') || (basenm[a] == '\\') ) {
			basenm[a] = '_';
		}
	}

	if (CC->user.axlevel >= AxAideU) {
		snprintf(CC->upl_path, sizeof CC->upl_path, 
				 "%s/%s",
				 ctdl_image_dir,
				 basenm);
	}

	if (!strcasecmp(basenm, "_userpic_")) {
		snprintf(CC->upl_path, sizeof CC->upl_path,
				 "%s/%ld.gif",
				 ctdl_usrpic_dir,
				 CC->user.usernum);
	}

	if ((!strcasecmp(basenm, "_floorpic_"))
	    && (CC->user.axlevel >= AxAideU)) {
		which_floor = extract_int(cmdbuf, 2);
		snprintf(CC->upl_path, sizeof CC->upl_path,
				 "%s/floor.%d.gif",
				 ctdl_image_dir,
				 which_floor);
	}

	if ((!strcasecmp(basenm, "_roompic_")) && (is_room_aide())) {
		assoc_file_name(CC->upl_path, sizeof CC->upl_path, &CC->room, ctdl_image_dir);
	}

	if (IsEmptyStr(CC->upl_path)) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if (is_this_for_real == 0) {
		cprintf("%d Ok to send image\n", CIT_OK);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "wb");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}
	cprintf("%d Ok\n", CIT_OK);
	CC->upload_type = UPL_IMAGE;
}


/*
 * close the download file
 */
void cmd_clos(char *cmdbuf)
{
	char buf[256];

	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",
			ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	fclose(CC->download_fp);
	CC->download_fp = NULL;

	if (CC->dl_is_net == 1) {
		CC->dl_is_net = 0;
		snprintf(buf, sizeof buf, 
				 "%s/%s",
				 ctdl_netout_dir,
				 CC->net_node);
		unlink(buf);
	}

	cprintf("%d Ok\n", CIT_OK);
}


/*
 * abort an upload
 */
void abort_upl(CitContext *who)
{
	if (who->upload_fp != NULL) {
		fclose(who->upload_fp);
		who->upload_fp = NULL;
		unlink(CC->upl_path);
	}
}



/*
 * close the upload file
 */
void cmd_ucls(char *cmd)
{
	FILE *fp;
	char upload_notice[512];
	static int seq = 0;

	if (CC->upload_fp == NULL) {
		cprintf("%d You don't have an upload file open.\n", ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	fclose(CC->upload_fp);
	CC->upload_fp = NULL;

	if ((!strcasecmp(cmd, "1")) && (CC->upload_type != UPL_FILE)) {
		cprintf("%d Upload completed.\n", CIT_OK);

		if (CC->upload_type == UPL_NET) {
			char final_filename[PATH_MAX];
		        snprintf(final_filename, sizeof final_filename,
				"%s/%s.%04lx.%04x",
				ctdl_netin_dir,
				CC->net_node,
				(long)getpid(),
				++seq
			);

			if (link(CC->upl_path, final_filename) == 0) {
				unlink(CC->upl_path);
			}
			else {
				syslog(LOG_ALERT, "Cannot link %s to %s: %s\n",
					CC->upl_path, final_filename, strerror(errno)
				);
			}

			/* FIXME ... here we need to trigger a network run */
		}

		CC->upload_type = UPL_FILE;
		return;
	}

	if (!strcasecmp(cmd, "1")) {
		cprintf("%d File '%s' saved.\n", CIT_OK, CC->upl_path);
		fp = fopen(CC->upl_filedir, "a");
		if (fp == NULL) {
			fp = fopen(CC->upl_filedir, "w");
		}
		if (fp != NULL) {
			fprintf(fp, "%s %s %s\n", CC->upl_file,
				CC->upl_mimetype,
				CC->upl_comment);
			fclose(fp);
		}

		/* put together an upload notice */
		snprintf(upload_notice, sizeof upload_notice,
			"NEW UPLOAD: '%s'\n %s\n%s\n",
			 CC->upl_file, 
			 CC->upl_comment, 
			 CC->upl_mimetype);
		quickie_message(CC->curr_user, NULL, NULL, CC->room.QRname,
				upload_notice, 0, NULL);
	} else {
		abort_upl(CC);
		cprintf("%d File '%s' aborted.\n", CIT_OK, CC->upl_path);
	}
}


/*
 * read from the download file
 */
void cmd_read(char *cmdbuf)
{
	long start_pos;
	size_t bytes;
	char buf[SIZ];
	int rc;

	/* The client will transmit its requested offset and byte count */
	start_pos = extract_long(cmdbuf, 0);
	bytes = extract_int(cmdbuf, 1);
	if ((start_pos < 0) || (bytes <= 0)) {
		cprintf("%d you have to specify a value > 0.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (CC->download_fp == NULL) {
		cprintf("%d You don't have a download file open.\n",
			ERROR + RESOURCE_NOT_OPEN);
		return;
	}

	/* If necessary, reduce the byte count to the size of our buffer */
	if (bytes > sizeof(buf)) {
		bytes = sizeof(buf);
	}

	rc = fseek(CC->download_fp, start_pos, 0);
	if (rc < 0) {
		cprintf("%d your file is smaller then %ld.\n", ERROR + ILLEGAL_VALUE, start_pos);
		syslog(LOG_ALERT, "your file %s is smaller then %ld. [%s]\n", 
		       CC->upl_path, 
		       start_pos,
		       strerror(errno));

		return;
	}
	bytes = fread(buf, 1, bytes, CC->download_fp);
	if (bytes > 0) {
		/* Tell the client the actual byte count and transmit it */
		cprintf("%d %d\n", BINARY_FOLLOWS, (int)bytes);
		client_write(buf, bytes);
	}
	else {
		cprintf("%d %s\n", ERROR, strerror(errno));
	}
}


/*
 * write to the upload file
 */
void cmd_writ(char *cmdbuf)
{
	int bytes;
	char *buf;
	int rv;

	unbuffer_output();

	bytes = extract_int(cmdbuf, 0);

	if (CC->upload_fp == NULL) {
		cprintf("%d You don't have an upload file open.\n", ERROR + RESOURCE_NOT_OPEN);
		return;
	}
	if (bytes <= 0) {
		cprintf("%d you have to specify a value > 0.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (bytes > 100000) {
		bytes = 100000;
	}

	cprintf("%d %d\n", SEND_BINARY, bytes);
	buf = malloc(bytes + 1);
	client_read(buf, bytes);
	rv = fwrite(buf, bytes, 1, CC->upload_fp);
	if (rv == -1) {
		syslog(LOG_EMERG, "Couldn't write: %s\n",
		       strerror(errno));
	}
	free(buf);
}




/*
 * cmd_ndop() - open a network spool file for downloading
 */
void cmd_ndop(char *cmdbuf)
{
	char pathname[256];
	struct stat statbuf;

	if (IsEmptyStr(CC->net_node)) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR + NOT_LOGGED_IN);
		return;
	}

	if (CC->download_fp != NULL) {
		cprintf("%d You already have a download file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	snprintf(pathname, sizeof pathname, 
			 "%s/%s",
			 ctdl_netout_dir,
			 CC->net_node);

	/* first open the file in append mode in order to create a
	 * zero-length file if it doesn't already exist 
	 */
	CC->download_fp = fopen(pathname, "a");
	if (CC->download_fp != NULL)
		fclose(CC->download_fp);

	/* now open it */
	CC->download_fp = fopen(pathname, "r");
	if (CC->download_fp == NULL) {
		cprintf("%d cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, pathname, strerror(errno));
		return;
	}


	/* set this flag so other routines know that the download file
	 * currently open is a network spool file 
	 */
	CC->dl_is_net = 1;

	stat(pathname, &statbuf);
	CC->download_fp_total = statbuf.st_size;
	cprintf("%d %ld\n", CIT_OK, (long)statbuf.st_size);
}

/*
 * cmd_nuop() - open a network spool file for uploading
 */
void cmd_nuop(char *cmdbuf)
{
	static int seq = 1;

	if (IsEmptyStr(CC->net_node)) {
		cprintf("%d Not authenticated as a network node.\n",
			ERROR + NOT_LOGGED_IN);
		return;
	}

	if (CC->upload_fp != NULL) {
		cprintf("%d You already have an upload file open.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}

	snprintf(CC->upl_path, sizeof CC->upl_path,
			 "%s/%s.%04lx.%04x",
			 ctdl_nettmp_dir,
			 CC->net_node, 
			 (long)getpid(), 
			 ++seq);

	CC->upload_fp = fopen(CC->upl_path, "r");
	if (CC->upload_fp != NULL) {
		fclose(CC->upload_fp);
		CC->upload_fp = NULL;
		cprintf("%d '%s' already exists\n",
			ERROR + ALREADY_EXISTS, CC->upl_path);
		return;
	}

	CC->upload_fp = fopen(CC->upl_path, "w");
	if (CC->upload_fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
			ERROR + INTERNAL_ERROR, CC->upl_path, strerror(errno));
		return;
	}

	CC->upload_type = UPL_NET;
	cprintf("%d Ok\n", CIT_OK);
}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(file_ops)
{
	if (!threading) {

		CtdlRegisterProtoHook(cmd_delf, "DELF", "Delete a file");
		CtdlRegisterProtoHook(cmd_movf, "MOVF", "Move a file");
		CtdlRegisterProtoHook(cmd_open, "OPEN", "Open a download file transfer");
		CtdlRegisterProtoHook(cmd_clos, "CLOS", "Close a download file transfer");
		CtdlRegisterProtoHook(cmd_uopn, "UOPN", "Open an upload file transfer");
		CtdlRegisterProtoHook(cmd_ucls, "UCLS", "Close an upload file transfer");
		CtdlRegisterProtoHook(cmd_read, "READ", "File transfer read operation");
		CtdlRegisterProtoHook(cmd_writ, "WRIT", "File transfer write operation");
		CtdlRegisterProtoHook(cmd_ndop, "NDOP", "Open a network spool file for download");
		CtdlRegisterProtoHook(cmd_nuop, "NUOP", "Open a network spool file for upload");
		CtdlRegisterProtoHook(cmd_oimg, "OIMG", "Open an image file for download");
		CtdlRegisterProtoHook(cmd_uimg, "UIMG", "Upload an image file");
	}
        /* return our Subversion id for the Log */
	return "file_ops";
}
